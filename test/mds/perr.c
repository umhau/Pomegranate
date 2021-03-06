/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Armed with EMACS.
 * Time-stamp: <2012-08-10 10:35:17 macan>
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
#include "xtable.h"
#include "tx.h"
#include "xnet.h"
#include "mds.h"
#include "lib.h"

int main(int argc, char *argv[])
{
    int err = 0, i;

    if (argc <= 1) {
        hvfs_info(mds, "Usage: %s [errno] [errno] ...\n",
                  argv[0]);
        return 0;
    }
    
    for (i = 1; i < argc; i++) {
        err = atoi(argv[i]);
        if (err < 0)
            err = -err;
        hvfs_info(mds, "errno %d : %s\n", err, strerror(err));
    }

    return 0;
}
