/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*======
This file is part of PerconaFT.


Copyright (c) 2006, 2015, Percona and/or its affiliates. All rights reserved.

    PerconaFT is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2,
    as published by the Free Software Foundation.

    PerconaFT is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PerconaFT.  If not, see <http://www.gnu.org/licenses/>.

----------------------------------------

    PerconaFT is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License, version 3,
    as published by the Free Software Foundation.

    PerconaFT is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with PerconaFT.  If not, see <http://www.gnu.org/licenses/>.
======= */

#ident "Copyright (c) 2006, 2015, Percona and/or its affiliates. All rights reserved."
#include <math.h>
#include <stdio.h>
#include "cachetable/checkpoint.h"
#include "test.h"
#include "affine_test.h"

int
test_main(int argc, const char *argv[]) {
    //set up the node size and basement size for benchmarking
    double nodeMB = parse_args_for_nodesize (argc, argv); 
    //if user supply 4, it means the nodesize = 4MB
    nodesize = nodeMB * (1 << 20);
    size_t B = nodesize /(keysize*8+valsize);
    basementsize = nodesize/fanout;
    calculate_numrows();
    calculate_randomnumbers();
    printf("Benchmarking fractal tree based on nodesize = %zu bytes(%lfMBs) \n \t key: %zu bytes (%zu KBs); value: %zu bytes (%zu KBs) \n\t B = %zu, epsilon=0.5, fanout = %d\n Preparing 16 GBs data...\n", nodesize, nodeMB, keysize*8, (keysize*8)/1024, valsize, valsize/1024, B, fanout);

    //set up the data file
    const char *n = "/mnt/db/affine_benchmark_data";
    int r;
    FT_HANDLE t;
    CACHETABLE ct;
    unlink(n);
    toku_cachetable_create(&ct, 1024*1024*1024, ZERO_LSN, nullptr);
    toku_ft_set_direct_io (true);
    r = toku_open_ft_handle(n, 1, &t, nodesize, basementsize, TOKU_NO_COMPRESSION, ct, null_txn, uint64_dbt_cmp); assert(r==0);
    //set up the fanout so the B^e =  fanout.
    toku_ft_handle_set_fanout(t, fanout);
    uint64_t * array = (uint64_t *)toku_malloc(sizeof(uint64_t) * numrows);
    if (array == NULL) {
       fprintf(stderr, "Allocate memory failed\n");
       return -1;
    }
    randomize(array);
    char val[valsize];
    ZERO_ARRAY(val);
    uint64_t key[keysize]; //key is 16k
    ZERO_ARRAY(key);
    DBT k, v;
    dbt_init(&k, key, keysize*8);
    dbt_init(&v, val, valsize);
    for (size_t i=0; i< random_numbers; i++) {
    	key[0] = toku_htod64(array[i]);
	if (! i%(random_numbers/100)) 
	    fprintf(stdout, "%zu%% complete\n", 100*i/random_numbers);
	toku_ft_insert(t, &k, &v, null_txn);
    }
    CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
    r = toku_checkpoint(cp, NULL, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);
    assert_zero(r); 
    toku_free(array);
    r = toku_close_ft_handle_nolsn(t, 0); assert(r==0);
    toku_cachetable_close(&ct);
    return 0;
}
