/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Armed with EMACS.
 * Time-stamp: <2010-06-02 09:02:27 macan>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __MDS_H__
#define __MDS_H__

#include "hvfs.h"
#include "prof.h"
#include "txg.h"
#include "tx.h"
#include "dh.h"
#include "cbht.h"
#include "mds_api.h"
#include "lib.h"
#include "async.h"
#include "mds_config.h"
#include "bitmapc.h"

/* FIXME: we should implement a ARC/DULO cache */
struct itb_cache 
{
    struct list_head lru;
    atomic_t csize;             /* current cache size */
    xlock_t lock;
};

struct mds_conf 
{
    /* section for dynamic configuration */
    char dcaddr[MDS_DCONF_MAX_NAME_LEN];
    int dcfd, dcepfd;
    pthread_t dcpt;

    /* section for file name */
    char *profiling_file;
    char *conf_file;
    char *log_file;

    /* section for file fd */
    FILE *pf_file, *cf_file, *lf_file;

    /* # of threads */
    /* NOTE: # of profiling thread is always ONE */
    int commit_threads;         /* # of commit threads */
    int service_threads;        /* # of service threads, pass this value to
                                     lnet serve-in threads' pool
                                     initialization */
    int async_threads;          /* # of async threads */
    int spool_threads;          /* # of service threads */
    int max_async_unlink;       /* max # of async unlink in one unlink wave */

    /* misc configs */
    u64 memlimit;               /* ITB mem limit */
    int txc_hash_size;          /* TXC hash table size */
    int bc_hash_size;           /* BC hash table size */
    int txc_ftx;                /* TXC init free TXs */
    int cbht_bucket_depth;      /* CBHT bucket depth */
    int itb_cache;
    int async_unlink;           /* enable/disable async unlink */
    int ring_vid_max;           /* max # of vid in the ring(AUTO) */
    int itb_depth_default;      /* default value of itb depth */
    int async_update_N;         /* default # of processing request */
    int mp_to;                  /* timeout of modify pause */
    int txg_buf_len;            /* length of the txg buffer */
    int bc_roof;                /* upper limmit of bitmap cache entries */
    int txg_ddht_size;          /* TXG dir delta hash table size */
    s8 itbid_check;             /* should we do ITBID check? */
    u8 cbht_slow_down;          /* set to 1 to eliminate the eh->lock
                                 * conflicts */
    u8 cbht_congestion;         /* set to 1 to slow down the incoming
                                 * request handling */
    u8 prof_plot;               /* do we dump profilings for gnuplot? */

    /* intervals */
    int profiling_thread_interval;
    int txg_interval;
    int unlink_interval;
    int bitmap_cache_interval;

    /* conf */
#define HVFS_MDS_CHRECHK        0x01 /* recheck CH ring in fe dispatch */

#define HVFS_MDS_ITB_RWLOCK     0x02 /* use pthread rwlock as index lock */
#define HVFS_MDS_ITB_MUTEX      0x04 /* use pthread mutex as index lock */

#define HVFS_MDS_MEMONLY        0x08 /* memory only service */
#define HVFS_MDS_MEMLIMIT       0x10 /* limit the ITB memory usage */

#define HVFS_MDS_LIMITED        0x20 /* for test/mds/itbsplit use only */
    u64 option;
};

struct hvfs_mds_object
{
    u64 site_id;                /* this site */
    struct xnet_context *xc;    /* the xnet context */

    struct mem_ops *mops;         /* memory management operations */
    struct eh cbht;               /* memory hash table */
    struct dh dh;                 /* directory hash table */

#define CH_RING_NUM     2
#define CH_RING_MDS     0
#define CH_RING_MDSL    1
    struct chring *chring[CH_RING_NUM];
    struct mds_prof prof;
    struct mds_conf conf;
#define TXG_NUM         2
#define TXG_OPEN        0
#define TXG_WB          1
    struct hvfs_txg *txg[TXG_NUM];
    struct hvfs_txc txc;
    struct itb_cache ic;
    struct bitmap_cache bc;
#define HMO_STATE_INIT          0x00
#define HMO_STATE_LAUNCH        0x01
#define HMO_STATE_RUNNING       0x02
#define HMO_STATE_PAUSE         0x03
#define HMO_STATE_RDONLY        0x04
    u64 state;

    u64 ring_site;

    struct list_head async_unlink;

    /* the following region is used for threads */
    time_t unlink_ts;
    time_t mp_ts;               /* begin time of modify pause */

    sem_t timer_sem;            /* for timer thread wakeup */
    sem_t commit_sem;           /* for commit thread wakeup */
    sem_t unlink_sem;           /* for unlink thread wakeup */
    sem_t async_sem;            /* for async thread wakeup */
    sem_t modify_pause_sem;     /* for pausing the modifing request
                                 * handling */
    
    pthread_t timer_thread;
    pthread_t *commit_thread;   /* array of commit threads */
    pthread_t *async_thread;    /* array of async threads */
    pthread_t unlink_thread;
    pthread_t *spool_thread;    /* array of service threads */

    u8 timer_thread_stop;       /* running flag for timer thread */
    u8 commit_thread_stop;      /* running flag for commit thread */
    u8 async_thread_stop;       /* running flag for async thread */
    u8 dconf_thread_stop;       /* running flag for dconf thread */
    u8 unlink_thread_stop;      /* running flag for unlink thread */
    u8 spool_thread_stop;       /* running flag for service thread */

    u8 spool_modify_pause;      /* pause the modification */

    /* callback functions */
    void (*cb_exit)(void *);
};

extern struct hvfs_mds_info hmi;
extern struct hvfs_mds_object hmo;
extern u32 hvfs_mds_tracing_flags;

struct dconf_req
{
#define DCONF_ECHO_CONF         0
#define DCONF_SET_TXG_INTV      1
#define DCONF_SET_PROF_INTV     2
#define DCONF_SET_UNLINK_INTV   3
    u64 cmd;
    u64 arg0;
};

/* this is the mds forward request header, we should save the route list
 * here. */
struct mds_fwd
{
    int len;
    u64 route[0];
};

/* APIs */
/* for mds.c */
void mds_pre_init(void);
int mds_init(int bdepth);
int mds_verify(void);
void mds_destroy(void);
void mds_reset_itimer(void);

/* for fe.c */
#define MAX_LOOP_FWD    (0x1000)
int mds_do_forward(struct xnet_msg *msg, u64 site_id);
int mds_fe_dispatch(struct xnet_msg *msg);

/* for dispatch.c */
int mds_client_dispatch(struct xnet_msg *msg);
int mds_mds_dispatch(struct xnet_msg *msg);
int mds_mdsl_dispatch(struct xnet_msg *msg);
int mds_ring_dispatch(struct xnet_msg *msg);
int mds_root_dispatch(struct xnet_msg *msg);

/* for cbht.c */
struct bucket *cbht_bucket_alloc(int);
int cbht_bucket_init(struct eh *, struct segment *);
void cbht_copy_dir(struct segment *, u64, u64, struct eh *);
int cbht_enlarge_dir(struct eh *, u32);
int cbht_update_dir(struct eh *, struct bucket *);
int cbht_bucket_split(struct eh *, struct bucket *, u64, u32);
int mds_cbht_init(struct eh *, int);
void mds_cbht_destroy(struct eh *);
int mds_cbht_insert(struct eh *, struct itb *);
int mds_cbht_del(struct eh *, struct itb *);
struct bucket *mds_cbht_search_dir(u64, u32 *);
int mds_cbht_search(struct hvfs_index *, struct hvfs_md_reply *, 
                    struct hvfs_txg *, struct hvfs_txg **);
void cbht_print_dir(struct eh *);
void mds_cbht_search_dump_itb(struct hvfs_index *);
int mds_cbht_insert_bbrlocked(struct eh *, struct itb *, 
                              struct bucket **, 
                              struct bucket_entry **,
                              struct itb **);
int mds_cbht_exist_check(struct eh *, u64, u64);
#define HVFS_MDS_OP_EVICT       0 /* evict the ITB to MDSL if it is clean */
#define HVFS_MDS_OP_CLEAN       1 /* empty ITB to clean to MDSL */
#define HVFS_MDS_MAX_OPS        3
void mds_cbht_scan(struct eh *, int);

/* for itb.c */
struct itb *mds_read_itb(u64, u64, u64);
void ite_update(struct hvfs_index *, struct ite *);
struct itb *get_free_itb_fast();
struct itb *get_free_itb(struct hvfs_txg *);
void itb_reinit(struct itb *);
void itb_idx_bmp_reinit(struct itb *);
void itb_free(struct itb *);
struct itb *itb_dirty(struct itb *, struct hvfs_txg *, struct itb_lock *,
                      struct hvfs_txg **);
int itb_search(struct hvfs_index *, struct itb *, void *, struct hvfs_txg *,
               struct itb **, struct hvfs_txg **);
int itb_readdir(struct hvfs_index *, struct itb *, struct hvfs_md_reply *);
int itb_cache_init(struct itb_cache *, int);
int itb_cache_destroy(struct itb_cache *);
void itb_dump(struct itb *);
void async_unlink(time_t t);
int unlink_thread_init(void);
void unlink_thread_destroy(void);
void async_unlink_ite(struct itb *, int *);
void itb_del_ite(struct itb *, struct ite *, u64, u64);
int __itb_add_ite_blob(struct itb *, struct ite *);
static inline void itb_get(struct itb *i)
{
    atomic_inc(&i->h.ref);
}
static inline void itb_put(struct itb *i)
{
    if (atomic_dec_return(&i->h.ref) == 0)
        itb_free(i);
}

/* for tx.c */
struct hvfs_tx *mds_alloc_tx(u16, struct xnet_msg *);
void mds_free_tx(struct hvfs_tx *);
void mds_pre_free_tx(int);
void mds_get_tx(struct hvfs_tx *);
void mds_put_tx(struct hvfs_tx *);
int mds_init_txc(struct hvfs_txc *, int, int);
int mds_destroy_txc(struct hvfs_txc *);
int mds_txc_add(struct hvfs_txc *, struct hvfs_tx *);
struct hvfs_tx *mds_txc_search(struct hvfs_txc *, u64, u64);
int mds_txc_evict(struct hvfs_txc *, struct hvfs_tx *);
void mds_tx_done(struct hvfs_tx *);
void mds_tx_reply(struct hvfs_tx *);
void mds_tx_commit(struct hvfs_tx *);
int mds_init_tx(u64);
void mds_destroy_tx(void);
void mds_tx_chg2forget(struct hvfs_tx *);

/* for txg.c: DRAFT */
void txg_add_itb(struct hvfs_txg *, struct itb *);
int txg_switch(struct hvfs_mds_info *, struct hvfs_mds_object *);
void txg_free(struct hvfs_txg *);
static inline void txg_get(struct hvfs_txg *t)
{
    atomic64_inc(&t->tx_pending);
}

static inline void txg_put(struct hvfs_txg *t)
{
    atomic64_dec(&t->tx_pending);
    if ((t->state != TXG_STATE_OPEN) && (atomic64_read(&t->tx_pending) == 0)) {
        /* signal the waiter, if exists */
        mcond_signal(&t->cond);
    }
}
int txg_init(u64);
void txg_changer(time_t);
int commit_tp_init(void);
void commit_tp_destroy(void);
int mds_add_bitmap_delta(struct hvfs_txg *, u64, u64, u64, u64);

#define TXG_ADD_SDATA(name1, name2, name3, cta, msg) do {               \
        struct name1 *pos;                                              \
        list_for_each_entry(pos, &cta->wbt->name2, list) {              \
            xnet_msg_add_sdata(msg, pos->buf, pos->asize * sizeof(struct name3)); \
        }                                                               \
    } while (0)

int txg_add_update_ddelta(struct hvfs_txg *, u64, s32, u32);
int txg_ddht_compact(struct hvfs_txg *);
int txg_rddb_add(struct hvfs_txg *, struct dir_delta_au *, u32);

/* for prof.c */
void dump_profiling(time_t);

/* for conf.c */
int dconf_init(void);
void dconf_destroy(void);

/* for dh.c */
int mds_dh_init(struct dh *, int);
void mds_dh_destroy(struct dh *);
struct dhe *mds_dh_load(struct dh *, u64);
struct dhe *mds_dh_insert(struct dh *, struct hvfs_index *);
struct dhe *mds_dh_search(struct dh *, u64);
int mds_dh_remove(struct dh *, u64);
u64 mds_get_itbid(struct dhe *, u64);
int mds_dh_bitmap_update(struct dh *, u64, u64, u8);
void mds_dh_bitmap_dump(struct dh *, u64);

/* for c2m.c, client 2 mds APIs */
void mds_statfs(struct hvfs_tx *);
void mds_lookup(struct hvfs_tx *);
void mds_create(struct hvfs_tx *);
void mds_release(struct hvfs_tx *);
void mds_update(struct hvfs_tx *);
void mds_linkadd(struct hvfs_tx *);
void mds_unlink(struct hvfs_tx *);
void mds_symlink(struct hvfs_tx *);
void mds_lb(struct hvfs_tx *);
void mds_dump_itb(struct hvfs_tx *);
void mds_c2m_ldh(struct hvfs_tx *);

/* for m2m.c, mds 2 mds APIs */
void mds_ldh(struct xnet_msg *msg);
void mds_ausplit(struct xnet_msg *msg);
void mds_forward(struct xnet_msg *msg);
void mds_aubitmap(struct xnet_msg *msg);
void mds_aubitmap_r(struct xnet_msg *msg);
void mds_m2m_lb(struct xnet_msg *msg);
void mds_audirdelta(struct xnet_msg *msg);
void mds_audirdelta_r(struct xnet_msg *msg);

/* for async.c */
int async_tp_init(void);
void async_tp_destroy(void);
void async_update_checking(time_t);
void au_handle_split_sync(void);
void async_aubitmap_cleanup(u64, u64);
void async_audirdelta_cleanup(u64, u64);

/* please do not move the follow section */
/* SECTION BEGIN */
#include "itb.h"
/* SECTION END */

/* APIs */
/* for spool.c */
int mds_spool_create(void);
void mds_spool_destroy(void);
int mds_spool_dispatch(struct xnet_msg *);
int mds_spool_modify_pause(struct xnet_msg *);
void mds_spool_mp_check(time_t);

/* APIs */
/* __txg_busy_loop_detector()
 *
 * NOTE: I observe the busy loop situation in the test cast this day, but it
 * is very hard to reproduce it, so I put this loop detector() here in the
 * following test cases to catch it.
 */
#define BLD_COUNT       0
#define BLD_RESET       1
static inline 
void __txg_busy_loop_detector(struct hvfs_txg *t, int bld)
{
    static int i = 0;
    if (bld == BLD_COUNT) {
        i++;
    } else if (bld == BLD_RESET) {
        i = 0;
    }
    if (i == 100000000) {
        HVFS_VV("TXG %p %ld state %x\n", t, t->txg, t->state);
        HVFS_BUG();
    }
}

static inline
struct hvfs_txg *mds_get_open_txg(struct hvfs_mds_object *hmo)
{
    struct hvfs_txg *t;

retry:
    /* get the txg first */
    t = hmo->txg[TXG_OPEN];     /* atomic read */
    txg_get(t);
    /* checking the txg state */
    if (t->state != TXG_STATE_OPEN) {
        /* oh, txg switched, for correctness, retry myself */
        txg_put(t);
        __txg_busy_loop_detector(t, BLD_COUNT);
        goto retry;
    }

    __txg_busy_loop_detector(t, BLD_RESET);
    return t;
}

static inline
struct hvfs_txg *mds_get_wb_txg(struct hvfs_mds_object *hmo)
{
    return hmo->txg[TXG_WB];
}

int itb_split_local(struct itb *, int, struct itb_lock *, struct hvfs_txg *);

/* bitmapc.c */
int mds_bitmap_cache_init(void);
void mds_bitmap_cache_destroy(void);

/* ddc.c */
int txg_ddc_update_cbht(struct dir_delta_au *);
int txg_ddc_send_request(struct dir_delta_au *);
int txg_ddc_send_reply(struct hvfs_dir_delta *);

u64 mds_select_ring(struct hvfs_mds_object *);
void mds_set_ring(u64);

#endif
