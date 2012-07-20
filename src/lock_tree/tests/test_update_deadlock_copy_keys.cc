/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// T(A) gets R(L)
// T(B) gets R(L)
// T(A) tries W(L), gets blocked
// T(B) tries W(L), gets deadlock
// T(B) releases locks
// T(A) granted W(L)
// T(A) releases locks

#include "test.h"

int main(int argc, const char *argv[]) {
    int r;

    uint32_t max_locks = 2;
    uint64_t max_lock_memory = 4096;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            if (verbose > 0) verbose--;
            continue;
        }
        if (strcmp(argv[i], "--max_locks") == 0 && i+1 < argc) {
            max_locks = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--max_lock_memory") == 0 && i+1 < argc) {
            max_lock_memory = atoi(argv[++i]);
            continue;
        }        
        assert(0);
    }

    // setup
    toku_ltm *ltm = NULL;
    r = toku_ltm_create(&ltm, max_locks, max_lock_memory, dbpanic);
    assert(r == 0 && ltm);

    toku_lock_tree *lt = NULL;
    r = toku_ltm_get_lt(ltm, &lt, (DICTIONARY_ID){1}, NULL, dbcmp, NULL, NULL, NULL);
    assert(r == 0 && lt);

    const TXNID txn_a = 1;
    DBT key_l; dbt_init(&key_l, "L", 1);
    toku_lock_request a_r_l; toku_lock_request_init(&a_r_l, txn_a, &key_l, &key_l, LOCK_REQUEST_READ);
    r = toku_lock_request_start(&a_r_l, lt, false); assert(r == 0); 
    assert(a_r_l.state == LOCK_REQUEST_COMPLETE && a_r_l.complete_r == 0);
    toku_lock_request_destroy(&a_r_l);

    const TXNID txn_b = 2;
    toku_lock_request b_r_l; toku_lock_request_init(&b_r_l, txn_b, &key_l, &key_l, LOCK_REQUEST_READ);
    r = toku_lock_request_start(&b_r_l, lt, true); assert(r == 0); 
    assert(b_r_l.state == LOCK_REQUEST_COMPLETE && b_r_l.complete_r == 0);
    toku_lock_request_destroy(&b_r_l);

    toku_lock_request a_w_l; toku_lock_request_init(&a_w_l, txn_a, &key_l, &key_l, LOCK_REQUEST_WRITE);
    r = toku_lock_request_start(&a_w_l, lt, true); assert(r == DB_LOCK_NOTGRANTED); 
    assert(a_w_l.state == LOCK_REQUEST_PENDING);

    toku_lock_request b_w_l; toku_lock_request_init(&b_w_l, txn_b, &key_l, &key_l, LOCK_REQUEST_WRITE);
    r = toku_lock_request_start(&b_w_l, lt, true); assert(r == DB_LOCK_DEADLOCK); 
    assert(b_w_l.state == LOCK_REQUEST_COMPLETE && b_w_l.complete_r == DB_LOCK_DEADLOCK);

    r = toku_lt_unlock_txn(lt, txn_b); assert(r == 0);
    toku_lock_request_destroy(&b_w_l);

    assert(a_w_l.state == LOCK_REQUEST_COMPLETE && a_w_l.complete_r == 0);
    toku_lock_request_destroy(&a_w_l);

    r = toku_lt_unlock_txn(lt, txn_a);  assert(r == 0);

    // shutdown 
    toku_lt_remove_db_ref(lt);
    r = toku_ltm_close(ltm); assert(r == 0);

    return 0;
}