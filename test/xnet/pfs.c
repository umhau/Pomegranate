/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Armed with EMACS.
 * Time-stamp: <2011-06-20 20:40:02 macan>
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

#include "pfs.h"
#include "xnet.h"
#include "store.h"
#include "branch.h"

void pfs_cb_branch_destroy(void *arg)
{
    branch_destroy();
}

/* Please use environment variables to pass HVFS specific values
 */
int main(int argc, char *argv[])
{
    char *hargv[20], *value, *dstore = NULL;
    int noatime = -1, nodiratime = -1, ttl = -1, noxattr = -1;
    int hargc;
    int err = 0;

    value = getenv("noatime");
    if (value) {
        noatime = atoi(value);
    }
    value = getenv("nodiratime");
    if (value) {
        nodiratime = atoi(value);
    }
    value = getenv("ttl");
    if (value) {
        ttl = atoi(value);
    }
    value = getenv("dstore");
    if (value) {
        dstore = strdup(value);
    }
    value = getenv("noxattr");
    if (value) {
        noxattr = atoi(value);
    }

    if (noatime >= 0 || nodiratime >= 0 || ttl >= 0 || noxattr >= 0 
        || dstore) {
        /* reset minor value to default value */
        if (noatime < 0)
            noatime = 1;
        if (nodiratime < 0)
            nodiratime = 1;
        if (ttl < 0)
            ttl = 5;
        if (noxattr < 0)
            noxattr = 1;
        pfs_fuse_mgr.inited = 1;
        pfs_fuse_mgr.sync_write = 0;
        pfs_fuse_mgr.use_config = 0;
        pfs_fuse_mgr.noatime = (noatime > 0 ? 1 : 0);
        pfs_fuse_mgr.nodiratime = (nodiratime > 0 ? 1 : 0);
        pfs_fuse_mgr.use_dstore = (dstore ? 1 : 0);
        pfs_fuse_mgr.ttl = ttl;
        pfs_fuse_mgr.noxattr = (noxattr > 0 ? 1 : 0);
    }

    /* init the dstore */
    if (dstore) {
        hvfs_datastore_init();
        err = hvfs_datastore_adding(dstore);
        if (err) {
            hvfs_err(xnet, "Parsing dstore config file failed w/ %d\n",
                     err);
            return EINVAL;
        }
    }

    /* should we reset xattrs? */
    if (pfs_fuse_mgr.noxattr)
        __pfs_reset_xattr();

    /* reconstruct the HVFS arguments */
    /* setup client's self id */
    hargv[0] = "pfs.ut";
    value = getenv("id");
    if (value) {
        hargv[1] = "-d";
        hargv[2] = strdup(value);
    } else {
        hvfs_err(xnet, "Please set client ID through env: id=xxx!\n");
        return EINVAL;
    }
    /* setup root server's ip address */
    value = getenv("root");
    if (value) {
        hargv[3] = "-r";
        hargv[4] = strdup(value);
    } else {
        hvfs_err(xnet, "Please set root IP through env: root=IP!\n");
        return EINVAL;
    }
    /* setup fsid */
    value = getenv("fsid");
    if (value) {
        hargv[5] = "-f";
        hargv[6] = strdup(value);
    } else {
        hargv[5] = "-f";
        hargv[6] = "0";
    }
    /* setup client type */
    {
        hargv[7] = "-y";
        hargv[8] = "client";
    }
    /* setup loop flag */
    {
        hargv[9] = "-l";
    }
    /* setup self port */
    value = getenv("port");
    if (value) {
        hargv[10] = "-p";
        hargv[11] = strdup(value);
        hargc = 12;
    } else {
        hargc = 10;
    }
    /* set page size of internal page cache */
    value = getenv("ps");
    if (value) {
        size_t ps = atol(value);

        g_pagesize = getpagesize();
        if (ps > g_pagesize) {
            g_pagesize = PAGE_ROUNDUP(ps, g_pagesize);
        } else
            g_pagesize = 0;
    }
    
    err = __core_main(hargc, hargv);
    if (err) {
        hvfs_err(xnet, "__core_main() failed w/ '%s'\n",
                 strerror(err > 0 ? err : -err));
        return err;
    }

    /* warn on non-atomic rename and imcompatible w/ MDSL_RADICAL_DEL */
    hvfs_info(xnet, "This PFS client only implements a %sNon-ATOMIC%s "
              "rename.\n",
              HVFS_COLOR_RED, HVFS_COLOR_END);
#ifdef MDSL_RADICAL_DEL
    if (1) {
#else
    if (hmo.conf.option & HVFS_MDSL_RADICAL_DEL) {
#endif
        hvfs_info(xnet, "This Non-ATOMIC rename %sCONFLICTS%s with macro "
                  "MDSL_RADICAL_DEL and option HVFS_MDSL_RADICAL_DEL.\n",
                  HVFS_COLOR_RED, HVFS_COLOR_END);
        hvfs_info(xnet, "You can disable macro MDSL_RADICAL_DEL in "
                  "makefile.inc. Otherwise, rename may lead to file "
                  "content losing.\n");
    }

    /* init branch subsystem */
    hvfs_info(xnet, "Enable branch feeder mode.\n");

    err = branch_init(0, 0, 0, NULL);
    if (err) {
        hvfs_err(xnet, "branch_init() failed w/ '%s'\n",
                 strerror(err > 0 ? err : -err));
        goto out;
    }

    hmo.branch_dispatch = branch_dispatch_split;
    hmo.cb_branch_destroy = pfs_cb_branch_destroy;
    hmo.cb_latency = pfs_cb_latency;
    
#if FUSE_USE_VERSION >= 26
    err = fuse_main(argc, argv, &pfs_ops, NULL);
#else
    err = fuse_main(argc, argv, &pfs_ops);
#endif
    if (err) {
        hvfs_err(xnet, "fuse_main() failed w/ %s\n",
                 strerror(err > 0 ? err : -err));
        goto out;
    }
out:
    __core_exit();
    if (dstore) {
        hvfs_datastore_exit();
    }

    return err;
}
