/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Armed with EMACS.
 * Time-stamp: <2010-03-14 21:02:25 macan>
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

#include "hvfs.h"
#include "xnet.h"
#include "mdsl.h"

/* append_buf_create()
 *
 * @state: fde state w/ OPEN/READ/WRITE
 */
int append_buf_create(struct fdhash_entry *fde, char *name, int state)
{
    size_t buf_len;
    int err = 0;
    
    if (state == FDE_FREE)
        return 0;
    
    if (fde->type == MDSL_STORAGE_ITB) {
        buf_len = hmo.conf.itb_file_chunk;
    } else if (fde->type == MDSL_STORAGE_DATA) {
        buf_len = hmo.conf.data_file_chunk;
    } else {
        buf_len = MDSL_STORAGE_DEFAULT_CHUNK;
    }

    xlock_lock(&fde->lock);
    if (fde->state == FDE_FREE) {
        /* ok, we should open it */
        fde->fd = open(name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (fde->fd < 0) {
            hvfs_err(mdsl, "open file '%s' failed\n", name);
            xlock_unlock(&fde->lock);
            return -EINVAL;
        }
        hvfs_info(mdsl, "open file %s w/ fd %d\n", name, fde->fd);
        fde->state = FDE_OPEN;
    }
    if (state == FDE_WRITE && fde->state == FDE_OPEN) {
        /* get the file end offset */
        fde->abuf.file_offset = lseek(fde->fd, 0, SEEK_END);
        if (fde->abuf.file_offset < 0) {
            hvfs_err(mdsl, "lseek to end of file %s failed w/ %d\n", 
                     name, errno);
            err = -errno;
            goto out_close;
        }
        /* fallocate the file chunk */
        if (posix_fallocate(fde->fd, fde->abuf.file_offset, buf_len) < 0) {
            hvfs_err(mdsl, "fallocate file %s failed w/ %d\n",
                     name, errno);
            err = -errno;
            goto out_close;
        }
        /* we create the append buf now */
        fde->abuf.addr = mmap(NULL, buf_len, PROT_WRITE | PROT_READ, 
                              MAP_SHARED, fde->fd, fde->abuf.file_offset);
        if (fde->abuf.addr == MAP_FAILED) {
            hvfs_err(mdsl, "mmap file %s in region [%ld,%ld] failed w/ %d\n",
                     name, fde->abuf.file_offset, 
                     fde->abuf.file_offset + buf_len, errno);
            err = -errno;
            goto out_close;
        }
        fde->abuf.len = buf_len;
        fde->state = FDE_WRITE;
    }

    xlock_unlock(&fde->lock);

    return err;
out_close:
    xlock_unlock(&fde->lock);
    /* close the file */
    close(fde->fd);
    fde->state = FDE_FREE;

    return err;
}

void append_buf_destroy(struct fdhash_entry *fde)
{
    int err;
    
    /* munmap the region */
    if (fde->state == FDE_WRITE) {
        err = munmap(fde->abuf.addr, fde->abuf.len);
        if (err) {
            hvfs_err(mdsl, "munmap fd %d failed w/ %d\n", 
                     fde->fd, err);
        }
        fde->state = FDE_OPEN;
    }
}

int mdsl_storage_init(void)
{
    int i;
    
    if (!hmo.conf.storage_fdhash_size) {
        hmo.conf.storage_fdhash_size = MDSL_STORAGE_FDHASH_SIZE;
    }

    hmo.storage.fdhash = xmalloc(hmo.conf.storage_fdhash_size *
                                 sizeof(struct regular_hash));
    if (!hmo.storage.fdhash) {
        hvfs_err(mdsl, "alloc fd hash table failed.\n");
        return -ENOMEM;
    }
    /* init the hash table */
    for (i = 0; i < hmo.conf.storage_fdhash_size; i++) {
        INIT_HLIST_HEAD(&(hmo.storage.fdhash + i)->h);
        xlock_init(&(hmo.storage.fdhash + i)->lock);
    }

    return 0;
}

void mdsl_storage_destroy(void)
{
    struct fdhash_entry *fde;
    struct hlist_node *pos, *n;
    time_t begin, current;
    int i, notdone, force_close = 0;

    begin = time(NULL);
    do {
        current = time(NULL);
        if (current - begin > 30) {
            hvfs_err(mdsl, "30 seconds passed, we will close all pending "
                     "fils forcely.\n");
            force_close = 1;
        }
        notdone = 0;
        for (i = 0; i < hmo.conf.storage_fdhash_size; i++) {
            xlock_lock(&(hmo.storage.fdhash + i)->lock);
            hlist_for_each_entry_safe(fde, pos, n, 
                                      &(hmo.storage.fdhash + i)->h, list) {
                if (atomic_read(&fde->ref) == 0 || force_close) {
                    hvfs_debug(mdsl, "Final close fd %d.\n", fde->fd);
                    if (fde->type == MDSL_STORAGE_ITB) {
                        append_buf_destroy(fde);
                    }
                    close(fde->fd);
                    hlist_del(&fde->list);
                    xfree(fde);
                } else {
                    notdone = 1;
                }
            }
            xlock_unlock(&(hmo.storage.fdhash + i)->lock);
        }
    } while (notdone);
}


static inline
struct fdhash_entry *mdsl_storage_fd_lookup(u64 duuid, int ftype, u64 arg)
{
    struct fdhash_entry *fde;
    struct hlist_node *pos;
    int idx;
    
    idx = hvfs_hash_fdht(duuid, ftype);
    xlock_lock(&(hmo.storage.fdhash + idx)->lock);
    hlist_for_each_entry(fde, pos, &(hmo.storage.fdhash + idx)->h, list) {
        if (duuid == fde->uuid && ftype == fde->type && arg == fde->arg) {
            atomic_inc(&fde->ref);
            xlock_unlock(&(hmo.storage.fdhash + idx)->lock);
            return fde;
        }
    }
    xlock_unlock(&(hmo.storage.fdhash + idx)->lock);

    return ERR_PTR(-EINVAL);
}

struct fdhash_entry *mdsl_storage_fd_insert(struct fdhash_entry *new)
{
    struct fdhash_entry *fde;
    struct hlist_node *pos;
    int idx, found = 0;

    idx = hvfs_hash_fdht(new->uuid, new->type);
    xlock_lock(&(hmo.storage.fdhash + idx)->lock);
    hlist_for_each_entry(fde, pos, &(hmo.storage.fdhash + idx)->h, list) {
        if (new->uuid == fde->uuid && new->type == fde->type &&
            new->arg == fde->arg) {
            atomic_inc(&fde->ref);
            found = 1;
            break;
        }
    }
    if (!found) {
        hlist_add_head(&new->list, &(hmo.storage.fdhash + idx)->h);
    }
    xlock_unlock(&(hmo.storage.fdhash + idx)->lock);

    if (found)
        return fde;
    else
        return new;
}

void mdsl_storage_fd_remove(struct fdhash_entry *new)
{
    int idx;

    idx = hvfs_hash_fdht(new->uuid, new->type);
    xlock_lock(&(hmo.storage.fdhash + idx)->lock);
    hlist_del(&new->list);
    xlock_unlock(&(hmo.storage.fdhash + idx)->lock);
}

struct fdhash_entry *mdsl_storage_fd_lookup_create(u64 duuid, int fdtype, u64 arg)
{
    struct fdhash_entry *fde;
    char path[HVFS_MAX_NAME_LEN] = {0, };
    int err = 0;
    
    fde = mdsl_storage_fd_lookup(duuid, fdtype, arg);
    if (!IS_ERR(fde))
        return fde;

    /* Step 1: create a new fdhash_entry */
    fde = xzalloc(sizeof(*fde));
    if (!fde) {
        hvfs_err(mdsl, "xzalloc struct fdhash_entry failed.\n");
        return ERR_PTR(-ENOMEM);
    } else {
        struct fdhash_entry *inserted;
        
        /* init it */
        INIT_HLIST_NODE(&fde->list);
        xlock_init(&fde->lock);
        atomic_set(&fde->ref, 1);
        fde->uuid = duuid;
        fde->arg = arg;
        fde->type = fdtype;
        fde->state = FDE_FREE;
        /* insert into the fdhash table */
        inserted = mdsl_storage_fd_insert(fde);
        if (inserted != fde) {
            hvfs_warning(mdsl, "someone insert this fde before us.\n");
            xfree(fde);
            fde = inserted;
        }
    }

    /* Step 2: we should open the file now */
    /* NOTE:
     *
     * 1. itb/data file should be written with self buffering through the mem
     *    window or not
     *
     * 2. itb/data file should be read through the mem window or direct read.
     *
     * 3. md/range file should be read/written with mem window
     */

    /* NOTE2: you should consider the concurrent access here!
     */

    switch (fdtype) {
    case MDSL_STORAGE_MD:
        sprintf(path, "%s/%ld/md", HVFS_MDSL_HOME, duuid);
        break;
    case MDSL_STORAGE_ITB:
        sprintf(path, "%s/%ld/itb-%ld", HVFS_MDSL_HOME, duuid, arg);
        err = append_buf_create(fde, path, FDE_WRITE);
        if (err) {
            hvfs_err(mdsl, "append buf create failed w/ %d\n", err);
            goto out_clean;
        }
        break;
    case MDSL_STORAGE_RANGE:
        sprintf(path, "%s/%ld/range-%ld", HVFS_MDSL_HOME, duuid, arg);
        break;
    case MDSL_STORAGE_DATA:
        sprintf(path, "%s/%ld/data-%ld", HVFS_MDSL_HOME, duuid, arg);
        break;
    case MDSL_STORAGE_DIRECTW:
        sprintf(path, "%s/%ld/directw", HVFS_MDSL_HOME, duuid);
        break;
    case MDSL_STORAGE_LOG:
        sprintf(path, "%s/log", HVFS_MDSL_HOME);
        break;
    case MDSL_STORAGE_SPLIT_LOG:
        sprintf(path, "%s/split_log", HVFS_MDSL_HOME);
        break;
    case MDSL_STORAGE_TXG:
        sprintf(path, "%s/txg", HVFS_MDSL_HOME);
        break;
    case MDSL_STORAGE_TMP_TXG:
        sprintf(path, "%s/tmp_txg", HVFS_MDSL_HOME);
        break;
    default:
        hvfs_err(mdsl, "Invalid file type provided, check your codes.\n");
        err = -EINVAL;
        goto out_clean;
    }

    return fde;
out_clean:
    /* we should release the fde on error */
    ASSERT(atomic_read(&fde->ref) == 1, mdsl);
    mdsl_storage_fd_remove(fde);
    xfree(fde);
    return ERR_PTR(err);
}