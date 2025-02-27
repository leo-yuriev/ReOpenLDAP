/* $ReOpenLDAP$ */
/* Copyright 2011-2018 ReOpenLDAP AUTHORS: please see AUTHORS file.
 * All rights reserved.
 *
 * This file is part of ReOpenLDAP.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */

#include "reldap.h"

#include <stdio.h>
#include <ac/string.h>
#include <ac/errno.h>

#define AVL_INTERNAL
#include "back-mdb.h"
#include "idl.h"

#ifdef MDB_TOOL_IDL_CACHING
static int mdb_tool_idl_flush(BackendDB *be, MDBX_txn *txn);

#define IDBLOCK 1024

typedef struct mdb_tool_idl_cache_entry {
  struct mdb_tool_idl_cache_entry *next;
  ID ids[IDBLOCK];
} mdb_tool_idl_cache_entry;

typedef struct mdb_tool_idl_cache {
  struct berval kstr;
  mdb_tool_idl_cache_entry *head, *tail;
  ID first, last;
  int count;
  short offset;
  short flags;
} mdb_tool_idl_cache;
#define WAS_FOUND 0x01
#define WAS_RANGE 0x02

#define MDB_TOOL_IDL_FLUSH(be, txn) mdb_tool_idl_flush(be, txn)
#else
#define MDB_TOOL_IDL_FLUSH(be, txn)
#endif /* MDB_TOOL_IDL_CACHING */

MDBX_txn *mdb_tool_txn = NULL;

static MDBX_cursor *cursor = NULL, *idcursor = NULL;
static MDBX_cursor *mcp = NULL, *mcd = NULL;
static MDBX_val key, data;
static ID previd = NOID;

typedef struct dn_id {
  ID id;
  struct berval dn;
} dn_id;

#define HOLE_SIZE 4096
static dn_id hbuf[HOLE_SIZE], *holes = hbuf;
static unsigned nhmax = HOLE_SIZE;
static unsigned nholes;

static struct berval *tool_base;
static int tool_scope;
static Filter *tool_filter;
static Entry *tool_next_entry;

static ID mdb_tool_ix_id;
static MDBX_txn *mdb_tool_ix_txn;
static int mdb_tool_index_tcount, mdb_tool_threads;
static IndexRec *mdb_tool_index_rec;
static AttrIxInfo **mdb_tool_axinfo;
static ldap_pvt_thread_mutex_t mdb_tool_index_mutex;
static ldap_pvt_thread_cond_t mdb_tool_index_cond_main;
static ldap_pvt_thread_cond_t mdb_tool_index_cond_work;

#ifdef MDB_TOOL_IDL_CACHING
static BackendDB *mdb_tool_ix_be;
static struct mdb_info *mdb_tool_info;
static void *mdb_tool_index_task(void *ctx, void *ptr);
#endif /* MDB_TOOL_IDL_CACHING */

static int mdb_writes, mdb_writes_per_commit;

/* Number of ops per commit in Quick mode.
 * Batching speeds writes overall, but too large a
 * batch will fail with MDB_TXN_FULL.
 */
#ifndef MDB_WRITES_PER_COMMIT
#define MDB_WRITES_PER_COMMIT 500
#endif

static int mdb_tool_entry_get_int(BackendDB *be, ID id, Entry **ep);

int mdb_tool_entry_open(BackendDB *be, int mode) {
  /* In Quick mode, commit once per 500 entries */
  mdb_writes = 0;
  if (slapMode & SLAP_TOOL_QUICK)
    mdb_writes_per_commit = MDB_WRITES_PER_COMMIT;
  else
    mdb_writes_per_commit = 1;

#ifdef MDB_TOOL_IDL_CACHING /* threaded indexing has no performance advantage                                          \
                             */
  /* Set up for threaded slapindex */
  if ((slapMode & (SLAP_TOOL_QUICK | SLAP_TOOL_READONLY)) == SLAP_TOOL_QUICK) {
    if (!mdb_tool_info) {
      struct mdb_info *mdb = (struct mdb_info *)be->be_private;
      ldap_pvt_thread_mutex_init(&mdb_tool_index_mutex);
      ldap_pvt_thread_cond_init(&mdb_tool_index_cond_main);
      ldap_pvt_thread_cond_init(&mdb_tool_index_cond_work);
      if (mdb->mi_nattrs) {
        int i;
        mdb_tool_threads = slap_tool_thread_max - 1;
        if (mdb_tool_threads > 1) {
          mdb_tool_index_rec = ch_calloc(mdb->mi_nattrs, sizeof(IndexRec));
          mdb_tool_axinfo = ch_calloc(mdb_tool_threads, sizeof(AttrIxInfo *) + sizeof(AttrIxInfo));
          mdb_tool_axinfo[0] = (AttrIxInfo *)(mdb_tool_axinfo + mdb_tool_threads);
          for (i = 1; i < mdb_tool_threads; i++)
            mdb_tool_axinfo[i] = mdb_tool_axinfo[i - 1] + 1;
          mdb_tool_index_tcount = mdb_tool_threads - 1;
          mdb_tool_ix_be = be;
          for (i = 1; i < mdb_tool_threads; i++) {
            int *ptr = ch_malloc(sizeof(int));
            *ptr = i;
            ldap_pvt_thread_pool_submit(&connection_pool, mdb_tool_index_task, ptr);
          }
          mdb_tool_info = mdb;
        }
      }
    }
  }
#endif

  return 0;
}

static int mdb_tool_terminate_txn(BackendDB *be, MDBX_txn *txn, int abort) {
  struct mdb_info *mdb = (struct mdb_info *)be->be_private;

  if (idcursor) {
    mdbx_cursor_close(idcursor);
    idcursor = NULL;
  }
  if (cursor) {
    mdbx_cursor_close(cursor);
    cursor = NULL;
  }
  if (mcd) {
    mdbx_cursor_close(mcd);
    mcd = NULL;
  }
  if (mcp) {
    mdbx_cursor_close(mcp);
    mcp = NULL;
  }

  if (mdb) {
    for (int i = 0; i < mdb->mi_nattrs; i++) {
      if (mdb->mi_attrs[i]->ai_cursor) {
        mdbx_cursor_close(mdb->mi_attrs[i]->ai_cursor);
        mdb->mi_attrs[i]->ai_cursor = NULL;
      }
    }
  }

  int rc;
  if (abort)
    rc = mdbx_txn_abort(txn);
  else {
    rc = mdbx_txn_commit(txn);
    if (rc)
      mdbx_txn_abort(txn);
  }
  if (txn == mdb_tool_txn)
    mdb_tool_txn = NULL;

  return rc;
}

#define mdb_tool_txn_commit(be, txn) mdb_tool_terminate_txn(be, txn, 0)

#define mdb_tool_txn_abort(be, txn) (void)mdb_tool_terminate_txn(be, txn, 1)

int mdb_tool_entry_close(BackendDB *be) {
#ifdef MDB_TOOL_IDL_CACHING
  if (mdb_tool_info) {
    int i;
    set_shutdown(SHUT_WR);
    ldap_pvt_thread_mutex_lock(&mdb_tool_index_mutex);

    /* There might still be some threads starting */
    while (mdb_tool_index_tcount > 0) {
      ldap_pvt_thread_cond_wait(&mdb_tool_index_cond_main, &mdb_tool_index_mutex);
    }

    mdb_tool_index_tcount = mdb_tool_threads - 1;
    ldap_pvt_thread_cond_broadcast(&mdb_tool_index_cond_work);

    /* Make sure all threads are stopped */
    while (mdb_tool_index_tcount > 0) {
      ldap_pvt_thread_cond_wait(&mdb_tool_index_cond_main, &mdb_tool_index_mutex);
    }
    ldap_pvt_thread_mutex_unlock(&mdb_tool_index_mutex);

    mdb_tool_info = NULL;
    set_shutdown(SHUT_RD);
    ch_free(mdb_tool_index_rec);
    mdb_tool_index_tcount = mdb_tool_threads - 1;
    if (mdb_tool_txn)
      MDB_TOOL_IDL_FLUSH(be, mdb_tool_txn);
    for (i = 0; i < mdb_tool_threads; i++) {
      mdb_tool_idl_cache *ic;
      mdb_tool_idl_cache_entry *ice;
      while ((ic = mdb_tool_axinfo[i]->ai_clist)) {
        mdb_tool_axinfo[i]->ai_clist = ic->head;
        free(ic);
      }
      while ((ice = mdb_tool_axinfo[i]->ai_flist)) {
        mdb_tool_axinfo[i]->ai_flist = ice->next;
        free(ice);
      }
    }
  }
#endif

  if (idcursor) {
    mdbx_cursor_close(idcursor);
    idcursor = NULL;
  }
  if (cursor) {
    mdbx_cursor_close(cursor);
    cursor = NULL;
  }
  if (mcd) {
    mdbx_cursor_close(mcd);
    mcd = NULL;
  }
  if (mcp) {
    mdbx_cursor_close(mcp);
    mcp = NULL;
  }
  {
    struct mdb_info *mdb = be->be_private;
    if (mdb) {
      int i;
      for (i = 0; i < mdb->mi_nattrs; i++) {
        mdbx_cursor_close(mdb->mi_attrs[i]->ai_cursor);
        mdb->mi_attrs[i]->ai_cursor = NULL;
      }
    }
  }
  if (mdb_tool_txn) {
    MDB_TOOL_IDL_FLUSH(be, mdb_tool_txn);
    int rc = mdb_tool_txn_commit(be, mdb_tool_txn);
    if (rc) {
      Debug(LDAP_DEBUG_ANY,
            LDAP_XSTRING(mdb_tool_entry_close) ": database %s: "
                                               "txn_commit failed: %s (%d)\n",
            be->be_suffix[0].bv_val, mdbx_strerror(rc), rc);
      return -1;
    }
  }

  if (nholes) {
    unsigned i;
    fprintf(stderr, "Error, entries missing!\n");
    for (i = 0; i < nholes; i++) {
      fprintf(stderr, "  entry %zd: %s\n", holes[i].id, holes[i].dn.bv_val);
    }
    nholes = 0;
    return -1;
  }

  return 0;
}

ID mdb_tool_entry_first_x(BackendDB *be, struct berval *base, int scope, Filter *f) {
  tool_base = base;
  tool_scope = scope;
  tool_filter = f;

  return mdb_tool_entry_next(be);
}

ID mdb_tool_entry_next(BackendDB *be) {
  int rc;
  ID id;
  struct mdb_info *mdb;

  assert(be != NULL);
  assert(slapMode & SLAP_TOOL_MODE);

  mdb = (struct mdb_info *)be->be_private;
  assert(mdb != NULL);

  if (!mdb_tool_txn) {
    rc = mdbx_txn_begin(mdb->mi_dbenv, NULL, MDBX_RDONLY, &mdb_tool_txn);
    if (rc)
      return NOID;
    rc = mdbx_cursor_open(mdb_tool_txn, mdb->mi_id2entry, &cursor);
    if (rc) {
      mdb_tool_txn_abort(be, mdb_tool_txn);
      return NOID;
    }
  }

next:;
  rc = mdbx_cursor_get(cursor, &key, &data, MDBX_NEXT);
  if (rc) {
    return NOID;
  }

  previd = *(ID *)key.iov_base;
  id = previd;

  if (!data.iov_len)
    goto next;

  if (tool_filter || tool_base) {
    static Operation op = {0};
    static Opheader ohdr = {0};

    op.o_hdr = &ohdr;
    op.o_bd = be;
    op.o_tmpmemctx = NULL;
    op.o_tmpmfuncs = &ch_mfuncs;

    if (tool_next_entry) {
      mdb_entry_release(&op, tool_next_entry, 0);
      tool_next_entry = NULL;
    }

    rc = mdb_tool_entry_get_int(be, id, &tool_next_entry);
    if (rc == LDAP_NO_SUCH_OBJECT) {
      goto next;
    }

    assert(tool_next_entry != NULL);

    if (tool_filter && test_filter(NULL, tool_next_entry, tool_filter) != LDAP_COMPARE_TRUE) {
      mdb_entry_release(&op, tool_next_entry, 0);
      tool_next_entry = NULL;
      goto next;
    }
  }

  return id;
}

ID mdb_tool_dn2id_get(Backend *be, struct berval *dn) {
  struct mdb_info *mdb;
  Operation op = {0};
  Opheader ohdr = {0};
  ID id;
  int rc;

  if (BER_BVISEMPTY(dn))
    return 0;

  mdb = (struct mdb_info *)be->be_private;

  if (!mdb_tool_txn) {
    rc = mdbx_txn_begin(mdb->mi_dbenv, NULL, (slapMode & SLAP_TOOL_READONLY) != 0 ? MDBX_RDONLY : 0, &mdb_tool_txn);
    if (rc)
      return NOID;
  }

  op.o_hdr = &ohdr;
  op.o_bd = be;
  op.o_tmpmemctx = NULL;
  op.o_tmpmfuncs = &ch_mfuncs;

  rc = mdb_dn2id(&op, mdb_tool_txn, NULL, dn, &id, NULL, NULL, NULL);
  if (rc == MDBX_NOTFOUND)
    return NOID;

  return id;
}

static int mdb_tool_entry_get_int(BackendDB *be, ID id, Entry **ep) {
  Operation op = {0};
  Opheader ohdr = {0};

  Entry *e = NULL;
  struct berval dn = BER_BVNULL, ndn = BER_BVNULL;
  int rc;

  assert(be != NULL);
  assert(slapMode & SLAP_TOOL_MODE);

  if ((tool_filter || tool_base) && id == previd && tool_next_entry != NULL) {
    *ep = tool_next_entry;
    tool_next_entry = NULL;
    return LDAP_SUCCESS;
  }

  if (id != previd) {
    key.iov_len = sizeof(ID);
    key.iov_base = &id;
    rc = mdbx_cursor_get(cursor, &key, &data, MDBX_SET);
    if (rc) {
      rc = LDAP_OTHER;
      goto done;
    }
  }
  if (!data.iov_len) {
    rc = LDAP_NO_SUCH_OBJECT;
    goto done;
  }

  op.o_hdr = &ohdr;
  op.o_bd = be;
  op.o_tmpmemctx = NULL;
  op.o_tmpmfuncs = &ch_mfuncs;
  if (slapMode & SLAP_TOOL_READONLY) {
    rc = mdb_id2name(&op, mdb_tool_txn, &idcursor, id, &dn, &ndn);
    if (rc) {
      rc = LDAP_OTHER;
      goto done;
    }
    if (tool_base != NULL) {
      if (!dnIsSuffixScope(&ndn, tool_base, tool_scope)) {
        ch_free(dn.bv_val);
        ch_free(ndn.bv_val);
        rc = LDAP_NO_SUCH_OBJECT;
        goto done;
      }
    }
  }
  rc = mdb_entry_decode(&op, mdb_tool_txn, &data, id, &e);
  e->e_id = id;
  if (!BER_BVISNULL(&dn)) {
    e->e_name = dn;
    e->e_nname = ndn;
  } else {
    e->e_name.bv_val = NULL;
    e->e_nname.bv_val = NULL;
  }

done:
  if (e != NULL) {
    *ep = e;
  }

  return rc;
}

Entry *mdb_tool_entry_get(BackendDB *be, ID id) {
  Entry *e = NULL;
  int rc;

  if (!mdb_tool_txn) {
    struct mdb_info *mdb = (struct mdb_info *)be->be_private;
    rc = mdbx_txn_begin(mdb->mi_dbenv, NULL, (slapMode & SLAP_TOOL_READONLY) ? MDBX_RDONLY : 0, &mdb_tool_txn);
    if (rc)
      return NULL;
  }
  if (!cursor) {
    struct mdb_info *mdb = (struct mdb_info *)be->be_private;
    rc = mdbx_cursor_open(mdb_tool_txn, mdb->mi_id2entry, &cursor);
    if (rc) {
      mdb_tool_txn_abort(be, mdb_tool_txn);
      return NULL;
    }
  }
  (void)mdb_tool_entry_get_int(be, id, &e);
  return e;
}

static int mdb_tool_next_id(Operation *op, MDBX_txn *tid, Entry *e, struct berval *text, int hole) {
  struct berval dn = e->e_name;
  struct berval ndn = e->e_nname;
  struct berval pdn, npdn, nmatched;
  ID id, pid = 0;
  int rc;

  if (ndn.bv_len == 0) {
    e->e_id = 0;
    return 0;
  }

  rc = mdb_dn2id(op, tid, mcp, &ndn, &id, NULL, NULL, &nmatched);
  if (rc == MDBX_NOTFOUND) {
    if (!be_issuffix(op->o_bd, &ndn)) {
      ID eid = e->e_id;
      dnParent(&ndn, &npdn);
      if (nmatched.bv_len != npdn.bv_len) {
        dnParent(&dn, &pdn);
        e->e_name = pdn;
        e->e_nname = npdn;
        rc = mdb_tool_next_id(op, tid, e, text, 1);
        e->e_name = dn;
        e->e_nname = ndn;
        if (rc) {
          return rc;
        }
        /* If parent didn't exist, it was created just now
         * and its ID is now in e->e_id. Make sure the current
         * entry gets added under the new parent ID.
         */
        if (eid != e->e_id) {
          pid = e->e_id;
        }
      } else {
        pid = id;
      }
    }
    rc = mdb_next_id(op->o_bd, idcursor, &e->e_id);
    if (rc) {
      snprintf(text->bv_val, text->bv_len, "next_id failed: %s (%d)", mdbx_strerror(rc), rc);
      Debug(LDAP_DEBUG_ANY, "=> mdb_tool_next_id: %s\n", text->bv_val);
      return rc;
    }
    rc = mdb_dn2id_add(op, mcp, mcd, pid, 1, 1, e);
    if (rc) {
      snprintf(text->bv_val, text->bv_len, "dn2id_add failed: %s (%d)", mdbx_strerror(rc), rc);
      Debug(LDAP_DEBUG_ANY, "=> mdb_tool_next_id: %s\n", text->bv_val);
    } else if (hole) {
      MDBX_val key, data;
      if (nholes == nhmax - 1) {
        if (holes == hbuf) {
          holes = ch_malloc(nhmax * sizeof(dn_id) * 2);
          memcpy(holes, hbuf, sizeof(hbuf));
        } else {
          holes = ch_realloc(holes, nhmax * sizeof(dn_id) * 2);
        }
        nhmax *= 2;
      }
      ber_dupbv(&holes[nholes].dn, &ndn);
      holes[nholes++].id = e->e_id;
      key.iov_len = sizeof(ID);
      key.iov_base = &e->e_id;
      data.iov_len = 0;
      data.iov_base = NULL;
      rc = mdbx_cursor_put(idcursor, &key, &data, MDBX_NOOVERWRITE);
      if (rc == MDBX_KEYEXIST)
        rc = 0;
      if (rc) {
        snprintf(text->bv_val, text->bv_len, "dummy id2entry add failed: %s (%d)", mdbx_strerror(rc), rc);
        Debug(LDAP_DEBUG_ANY, "=> mdb_tool_next_id: %s\n", text->bv_val);
      }
    }
  } else if (!hole) {
    unsigned i, j;

    e->e_id = id;

    for (i = 0; i < nholes; i++) {
      if (holes[i].id == e->e_id) {
        free(holes[i].dn.bv_val);
        for (j = i; j < nholes; j++)
          holes[j] = holes[j + 1];
        holes[j].id = 0;
        nholes--;
        break;
      } else if (holes[i].id > e->e_id) {
        break;
      }
    }
  }
  return rc;
}

static int mdb_tool_index_add(Operation *op, MDBX_txn *txn, Entry *e) {
  struct mdb_info *mdb = (struct mdb_info *)op->o_bd->be_private;

  if (!mdb->mi_nattrs)
    return 0;

  if (mdb_tool_threads > 1) {
    IndexRec *ir;
    int i, rc;
    Attribute *a;

    ir = mdb_tool_index_rec;
    for (i = 0; i < mdb->mi_nattrs; i++)
      ir[i].ir_attrs = NULL;

    for (a = e->e_attrs; a != NULL; a = a->a_next) {
      rc = mdb_index_recset(mdb, a, a->a_desc->ad_type, &a->a_desc->ad_tags, ir);
      if (rc)
        return rc;
    }
    for (i = 0; i < mdb->mi_nattrs; i++) {
      if (!ir[i].ir_ai)
        break;
      rc = mdbx_cursor_open(txn, ir[i].ir_ai->ai_dbi, &ir[i].ir_ai->ai_cursor);
      if (rc)
        return rc;
    }
    mdb_tool_ix_id = e->e_id;
    mdb_tool_ix_txn = txn;
    ldap_pvt_thread_mutex_lock(&mdb_tool_index_mutex);
    /* Wait for all threads to be ready */
    while (mdb_tool_index_tcount) {
      ldap_pvt_thread_cond_wait(&mdb_tool_index_cond_main, &mdb_tool_index_mutex);
    }

    for (i = 1; i < mdb_tool_threads; i++)
      mdb_tool_index_rec[i].ir_i = LDAP_BUSY;
    mdb_tool_index_tcount = mdb_tool_threads - 1;
    ldap_pvt_thread_cond_broadcast(&mdb_tool_index_cond_work);
    ldap_pvt_thread_mutex_unlock(&mdb_tool_index_mutex);

    return mdb_index_recrun(op, txn, mdb, ir, e->e_id, 0);
  } else {
    return mdb_index_entry_add(op, txn, e);
  }
}

static int mdb_tool_index_finish() {
  int i, rc = LDAP_SUCCESS;
  ldap_pvt_thread_mutex_lock(&mdb_tool_index_mutex);
  for (i = 1; i < mdb_tool_threads; i++) {
    if (mdb_tool_index_rec[i].ir_i == LDAP_BUSY) {
      ldap_pvt_thread_cond_wait(&mdb_tool_index_cond_main, &mdb_tool_index_mutex);
      i--;
      continue;
    }
    if (mdb_tool_index_rec[i].ir_i) {
      rc = mdb_tool_index_rec[i].ir_i;
      break;
    }
  }
  ldap_pvt_thread_mutex_unlock(&mdb_tool_index_mutex);
  return rc;
}

ID mdb_tool_entry_put(BackendDB *be, Entry *e, struct berval *text) {
  int rc;
  struct mdb_info *mdb;
  Operation op = {0};
  Opheader ohdr = {0};

  assert(be != NULL);
  assert(slapMode & SLAP_TOOL_MODE);

  assert(text != NULL);
  assert(text->bv_val != NULL);
  assert(text->bv_val[0] == '\0'); /* overconservative? */

  Debug(LDAP_DEBUG_TRACE, "=> " LDAP_XSTRING(mdb_tool_entry_put) "( %ld, \"%s\" )\n", (long)e->e_id, e->e_dn);

  mdb = (struct mdb_info *)be->be_private;

  if (!mdb_tool_txn) {
    rc = mdbx_txn_begin(mdb->mi_dbenv, NULL, 0, &mdb_tool_txn);
    if (rc != 0) {
      snprintf(text->bv_val, text->bv_len, "txn_begin failed: %s (%d)", mdbx_strerror(rc), rc);
      Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_put) ": %s\n", text->bv_val);
      return NOID;
    }
    rc = mdbx_cursor_open(mdb_tool_txn, mdb->mi_id2entry, &idcursor);
    if (rc != 0) {
      snprintf(text->bv_val, text->bv_len, "cursor_open failed: %s (%d)", mdbx_strerror(rc), rc);
      Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_put) ": %s\n", text->bv_val);
      return NOID;
    }
    if (!mdb_read_nextid(mdb)) {
      ID dummy;
      mdb_next_id(be, idcursor, &dummy);
    }
    rc = mdbx_cursor_open(mdb_tool_txn, mdb->mi_dn2id, &mcp);
    if (rc != 0) {
      snprintf(text->bv_val, text->bv_len, "cursor_open failed: %s (%d)", mdbx_strerror(rc), rc);
      Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_put) ": %s\n", text->bv_val);
      return NOID;
    }
    rc = mdbx_cursor_open(mdb_tool_txn, mdb->mi_dn2id, &mcd);
    if (rc != 0) {
      snprintf(text->bv_val, text->bv_len, "cursor_open failed: %s (%d)", mdbx_strerror(rc), rc);
      Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_put) ": %s\n", text->bv_val);
      return NOID;
    }
  }

  op.o_hdr = &ohdr;
  op.o_bd = be;
  op.o_tmpmemctx = NULL;
  op.o_tmpmfuncs = &ch_mfuncs;

  /* add dn2id indices */
  rc = mdb_tool_next_id(&op, mdb_tool_txn, e, text, 0);
  if (rc != 0) {
    goto done;
  }

  if (mdb_tool_threads > 1) {
    LDAP_SLIST_INSERT_HEAD(&op.o_extra, &mdb_tool_axinfo[0]->ai_oe, oe_next);
  }
  rc = mdb_tool_index_add(&op, mdb_tool_txn, e);
  if (rc != 0) {
    snprintf(text->bv_val, text->bv_len, "index_entry_add failed: err=%d", rc);
    Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_put) ": %s\n", text->bv_val);
    goto done;
  }

  /* id2entry index */
  rc = mdb_id2entry_add(&op, mdb_tool_txn, idcursor, e);
  if (rc != 0) {
    snprintf(text->bv_val, text->bv_len, "id2entry_add failed: err=%d", rc);
    Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_put) ": %s\n", text->bv_val);
    goto done;
  }

  if (mdb->mi_nattrs && mdb_tool_threads > 1)
    rc = mdb_tool_index_finish();

done:
  if (rc == 0) {
    mdb_writes++;
    if (mdb_writes >= mdb_writes_per_commit) {
      MDB_TOOL_IDL_FLUSH(be, mdb_tool_txn);
      rc = mdb_tool_txn_commit(be, mdb_tool_txn);
      mdb_writes = 0;
      if (rc != 0) {
        mdb->mi_numads = 0;
        snprintf(text->bv_val, text->bv_len, "txn_commit failed: %s (%d)", mdbx_strerror(rc), rc);
        Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_put) ": %s\n", text->bv_val);
        e->e_id = NOID;
      }
    }

  } else {
    mdb_tool_txn_abort(be, mdb_tool_txn);
    mdb_writes = 0;
    snprintf(text->bv_val, text->bv_len, "txn_aborted! %s (%d)",
             rc == LDAP_OTHER ? "Internal error" : mdbx_strerror(rc), rc);
    Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_put) ": %s\n", text->bv_val);
    e->e_id = NOID;
  }

  return e->e_id;
}

static int mdb_dn2id_upgrade(BackendDB *be);

int mdb_tool_entry_reindex(BackendDB *be, ID id, AttributeDescription **adv) {
  struct mdb_info *mi = (struct mdb_info *)be->be_private;
  int rc;
  Entry *e;
  Operation op = {0};
  Opheader ohdr = {0};

  Debug(LDAP_DEBUG_ARGS, "=> " LDAP_XSTRING(mdb_tool_entry_reindex) "( %ld )\n", (long)id);
  assert(tool_base == NULL);
  assert(tool_filter == NULL);

  /* Special: do a dn2id upgrade */
  if (adv && adv[0] == slap_schema.si_ad_entryDN) {
    /* short-circuit tool_entry_next() */
    mdbx_cursor_get(cursor, &key, &data, MDBX_LAST);
    return mdb_dn2id_upgrade(be);
  }

  /* No indexes configured, nothing to do. Could return an
   * error here to shortcut things.
   */
  if (!mi->mi_attrs) {
    return 0;
  }

  /* Check for explicit list of attrs to index */
  if (adv) {
    int i, j, n;

    if (mi->mi_attrs[0]->ai_desc != adv[0]) {
      /* count */
      for (n = 0; adv[n]; n++)
        ;

      /* insertion sort */
      for (i = 0; i < n; i++) {
        AttributeDescription *ad = adv[i];
        for (j = i - 1; j >= 0; j--) {
          if (SLAP_PTRCMP(adv[j], ad) <= 0)
            break;
          adv[j + 1] = adv[j];
        }
        adv[j + 1] = ad;
      }
    }

    for (i = 0; adv[i]; i++) {
      if (mi->mi_attrs[i]->ai_desc != adv[i]) {
        for (j = i + 1; j < mi->mi_nattrs; j++) {
          if (mi->mi_attrs[j]->ai_desc == adv[i]) {
            AttrInfo *ai = mi->mi_attrs[i];
            mi->mi_attrs[i] = mi->mi_attrs[j];
            mi->mi_attrs[j] = ai;
            break;
          }
        }
        if (j == mi->mi_nattrs) {
          Debug(LDAP_DEBUG_ANY, LDAP_XSTRING(mdb_tool_entry_reindex) ": no index configured for %s\n",
                adv[i]->ad_cname.bv_val);
          return -1;
        }
      }
    }
    mi->mi_nattrs = i;
  }

  e = mdb_tool_entry_get(be, id);

  if (e == NULL) {
    Debug(LDAP_DEBUG_ANY, LDAP_XSTRING(mdb_tool_entry_reindex) ": could not locate id=%ld\n", (long)id);
    return -1;
  }

  MDBX_txn *txi = NULL;
  rc = mdbx_txn_begin(mi->mi_dbenv, NULL, 0, &txi);
  if (rc != 0) {
    Debug(LDAP_DEBUG_ANY,
          "=> " LDAP_XSTRING(mdb_tool_entry_reindex) ": "
                                                     "txn_begin failed: %s (%d)\n",
          mdbx_strerror(rc), rc);
    goto done;
  }

  if (slapMode & SLAP_TRUNCATE_MODE) {
    int i;
    for (i = 0; i < mi->mi_nattrs; i++) {
      rc = mdbx_drop(txi, mi->mi_attrs[i]->ai_dbi, 0);
      if (rc) {
        Debug(LDAP_DEBUG_ANY,
              LDAP_XSTRING(mdb_tool_entry_reindex) ": (Truncate) mdbx_drop(%s) "
                                                   "failed: %s (%d)\n",
              mi->mi_attrs[i]->ai_desc->ad_type->sat_cname.bv_val, mdbx_strerror(rc), rc);
        goto done;
      }
    }
    slapMode ^= SLAP_TRUNCATE_MODE;
  }

  /*
   * just (re)add them for now
   * Use truncate mode to empty/reset index databases
   */

  Debug(LDAP_DEBUG_TRACE, "=> " LDAP_XSTRING(mdb_tool_entry_reindex) "( %ld )\n", (long)id);

  op.o_hdr = &ohdr;
  op.o_bd = be;
  op.o_tmpmemctx = NULL;
  op.o_tmpmfuncs = &ch_mfuncs;

  rc = mdb_tool_index_add(&op, txi, e);

done:
  if (rc == 0) {
    mdb_writes++;
    if (mdb_writes >= mdb_writes_per_commit) {
      MDBX_val key;
      MDB_TOOL_IDL_FLUSH(be, txi);
      rc = mdb_tool_txn_commit(be, txi);
      mdb_writes = 0;
      if (rc != 0) {
        Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_reindex) ": txn_commit failed: %s (%d)\n",
              mdbx_strerror(rc), rc);
        e->e_id = NOID;
      }
      /* Must close the read txn to allow old pages to be reclaimed. */
      if (mdb_tool_txn) {
        rc = mdbx_txn_abort(mdb_tool_txn);
        assert(rc == MDBX_SUCCESS);
        mdb_tool_txn = NULL;
      }
      /* and then reopen it so that tool_entry_next still works. */
      rc = mdbx_txn_begin(mi->mi_dbenv, NULL, MDBX_RDONLY, &mdb_tool_txn);
      assert(rc == MDBX_SUCCESS);
      assert(cursor == NULL);
      rc = mdbx_cursor_open(mdb_tool_txn, mi->mi_id2entry, &cursor);
      assert(rc == MDBX_SUCCESS);
      key.iov_base = &id;
      key.iov_len = sizeof(ID);
      rc = mdbx_cursor_get(cursor, &key, NULL, MDBX_SET);
      assert(rc == MDBX_SUCCESS);
    }

  } else {
    mdb_writes = 0;
    mdb_tool_txn_abort(be, txi);
    Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_reindex) ": txn_aborted! err=%d\n", rc);
    e->e_id = NOID;
  }
  mdb_entry_release(&op, e, 0);

  return rc;
}

ID mdb_tool_entry_modify(BackendDB *be, Entry *e, struct berval *text) {
  int rc;
  struct mdb_info *mdb;
  Operation op = {0};
  Opheader ohdr = {0};

  assert(be != NULL);
  assert(slapMode & SLAP_TOOL_MODE);

  assert(text != NULL);
  assert(text->bv_val != NULL);
  assert(text->bv_val[0] == '\0'); /* overconservative? */

  assert(e->e_id != NOID);

  Debug(LDAP_DEBUG_TRACE, "=> " LDAP_XSTRING(mdb_tool_entry_modify) "( %ld, \"%s\" )\n", (long)e->e_id, e->e_dn);

  mdb = (struct mdb_info *)be->be_private;
  if (!mdb_tool_txn) {
    rc = mdbx_txn_begin(mdb->mi_dbenv, NULL, 0, &mdb_tool_txn);
    if (rc != 0) {
      snprintf(text->bv_val, text->bv_len, "txn_begin failed: %s (%d)", mdbx_strerror(rc), rc);
      Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_modify) ": %s\n", text->bv_val);
      return NOID;
    }
  }

  op.o_hdr = &ohdr;
  op.o_bd = be;
  op.o_tmpmemctx = NULL;
  op.o_tmpmfuncs = &ch_mfuncs;

  /* id2entry index */
  rc = mdb_id2entry_update(&op, mdb_tool_txn, NULL, e);
  if (rc != 0) {
    snprintf(text->bv_val, text->bv_len, "id2entry_update failed: err=%d", rc);
    Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_modify) ": %s\n", text->bv_val);
    goto done;
  }

done:
  if (rc == 0) {
    rc = mdb_tool_txn_commit(be, mdb_tool_txn);
    if (rc != 0) {
      mdb->mi_numads = 0;
      snprintf(text->bv_val, text->bv_len, "txn_commit failed: %s (%d)", mdbx_strerror(rc), rc);
      Debug(LDAP_DEBUG_ANY,
            "=> " LDAP_XSTRING(mdb_tool_entry_modify) ": "
                                                      "%s\n",
            text->bv_val);
      e->e_id = NOID;
    }

  } else {
    mdb_tool_txn_abort(be, mdb_tool_txn);
    snprintf(text->bv_val, text->bv_len, "txn_aborted! %s (%d)", mdbx_strerror(rc), rc);
    Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_modify) ": %s\n", text->bv_val);
    e->e_id = NOID;
  }

  return e->e_id;
}

int mdb_tool_entry_delete(BackendDB *be, struct berval *ndn, struct berval *text) {
  int rc;
  struct mdb_info *mdb;
  Operation op = {0};
  Opheader ohdr = {0};
  Entry *e = NULL;

  assert(be != NULL);
  assert(slapMode & SLAP_TOOL_MODE);

  assert(text != NULL);
  assert(text->bv_val != NULL);
  assert(text->bv_val[0] == '\0'); /* overconservative? */

  assert(ndn != NULL);
  assert(ndn->bv_val != NULL);

  Debug(LDAP_DEBUG_TRACE, "=> " LDAP_XSTRING(mdb_tool_entry_delete) "( %s )\n", ndn->bv_val);

  mdb = (struct mdb_info *)be->be_private;

  assert(cursor == NULL);
  if (cursor) {
    mdbx_cursor_close(cursor);
    cursor = NULL;
  }
  if (!mdb_tool_txn) {
    rc = mdbx_txn_begin(mdb->mi_dbenv, NULL, 0, &mdb_tool_txn);
    if (rc != 0) {
      snprintf(text->bv_val, text->bv_len, "txn_begin failed: %s (%d)", mdbx_strerror(rc), rc);
      Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_delete) ": %s\n", text->bv_val);
      return LDAP_OTHER;
    }
  }

  rc = mdbx_cursor_open(mdb_tool_txn, mdb->mi_dn2id, &cursor);
  if (rc != 0) {
    snprintf(text->bv_val, text->bv_len, "cursor_open failed: %s (%d)", mdbx_strerror(rc), rc);
    Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_delete) ": %s\n", text->bv_val);
    return LDAP_OTHER;
  }

  op.o_hdr = &ohdr;
  op.o_bd = be;
  op.o_tmpmemctx = NULL;
  op.o_tmpmfuncs = &ch_mfuncs;

  rc = mdb_dn2entry(&op, mdb_tool_txn, cursor, ndn, &e, NULL, 0);
  if (rc != 0) {
    snprintf(text->bv_val, text->bv_len, "dn2entry failed: %s (%d)", mdbx_strerror(rc), rc);
    Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_delete) ": %s\n", text->bv_val);
    goto done;
  }

  /* check that we wouldn't orphan any children */
  rc = mdb_dn2id_children(&op, mdb_tool_txn, e);
  if (rc != MDBX_NOTFOUND) {
    switch (rc) {
    case 0:
      snprintf(text->bv_val, text->bv_len,
               "delete failed:"
               " subordinate objects must be deleted first");
      break;
    default:
      snprintf(text->bv_val, text->bv_len, "has_children failed: %s (%d)", mdbx_strerror(rc), rc);
      break;
    }
    rc = -1;
    Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_delete) ": %s\n", text->bv_val);
    goto done;
  }

  /* delete from dn2id */
  rc = mdb_dn2id_delete(&op, cursor, e->e_id, 1);
  if (rc != 0) {
    snprintf(text->bv_val, text->bv_len, "dn2id_delete failed: err=%d", rc);
    Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_delete) ": %s\n", text->bv_val);
    goto done;
  }

  /* deindex values */
  rc = mdb_index_entry_del(&op, mdb_tool_txn, e);
  if (rc != 0) {
    snprintf(text->bv_val, text->bv_len, "entry_delete failed: err=%d", rc);
    Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_delete) ": %s\n", text->bv_val);
    goto done;
  }

  /* do the deletion */
  rc = mdb_id2entry_delete(be, mdb_tool_txn, e);
  if (rc != 0) {
    snprintf(text->bv_val, text->bv_len, "id2entry_update failed: err=%d", rc);
    Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_delete) ": %s\n", text->bv_val);
    goto done;
  }

done:
  /* free entry */
  if (e != NULL) {
    mdb_entry_return(&op, e);
  }

  if (rc == 0) {
    rc = mdbx_txn_commit(mdb_tool_txn);
    if (rc != 0) {
      snprintf(text->bv_val, text->bv_len, "txn_commit failed: %s (%d)", mdbx_strerror(rc), rc);
      Debug(LDAP_DEBUG_ANY,
            "=> " LDAP_XSTRING(mdb_tool_entry_delete) ": "
                                                      "%s\n",
            text->bv_val);
    }

  } else {
    mdbx_txn_abort(mdb_tool_txn);
    snprintf(text->bv_val, text->bv_len, "txn_aborted! %s (%d)", mdbx_strerror(rc), rc);
    Debug(LDAP_DEBUG_ANY, "=> " LDAP_XSTRING(mdb_tool_entry_delete) ": %s\n", text->bv_val);
  }
  mdb_tool_txn = NULL;
  cursor = NULL;

  return rc;
}

#ifdef MDB_TOOL_IDL_CACHING
static void *mdb_tool_index_task(void *ctx, void *ptr) {
  int base = *(int *)ptr;
  Operation op = {0};
  Opheader ohdr = {0};
  AttrIxInfo ai = {{{0}}}, *aio;

  free(ptr);
  op.o_hdr = &ohdr;
  op.o_bd = mdb_tool_ix_be;
  op.o_tmpmemctx = NULL;
  op.o_tmpmfuncs = &ch_mfuncs;
  aio = mdb_tool_axinfo[base];
  mdb_tool_axinfo[base] = &ai;
  LDAP_SLIST_INSERT_HEAD(&op.o_extra, &ai.ai_oe, oe_next);
  while (1) {
    ldap_pvt_thread_mutex_lock(&mdb_tool_index_mutex);
    mdb_tool_index_tcount--;
    if (!mdb_tool_index_tcount)
      ldap_pvt_thread_cond_signal(&mdb_tool_index_cond_main);
    ldap_pvt_thread_cond_wait(&mdb_tool_index_cond_work, &mdb_tool_index_mutex);
    if (slapd_shutdown) {
      mdb_tool_index_tcount--;
      if (!mdb_tool_index_tcount)
        ldap_pvt_thread_cond_signal(&mdb_tool_index_cond_main);
      *aio = ai;
      mdb_tool_axinfo[base] = aio;
      ldap_pvt_thread_mutex_unlock(&mdb_tool_index_mutex);
      break;
    }
    ldap_pvt_thread_mutex_unlock(&mdb_tool_index_mutex);
    mdb_tool_index_rec[base].ir_i =
        mdb_index_recrun(&op, mdb_tool_ix_txn, mdb_tool_info, mdb_tool_index_rec, mdb_tool_ix_id, base);
  }

  return NULL;
}

static int mdb_tool_idl_cmp(const void *v1, const void *v2) {
  const mdb_tool_idl_cache *c1 = v1, *c2 = v2;
  int rc;

  if ((rc = c1->kstr.bv_len - c2->kstr.bv_len))
    return rc;
  return memcmp(c1->kstr.bv_val, c2->kstr.bv_val, c1->kstr.bv_len);
}

static int mdb_tool_idl_flush_one(MDBX_cursor *mc, AttrIxInfo *ai, mdb_tool_idl_cache *ic) {
  mdb_tool_idl_cache_entry *ice;
  MDBX_val key, data[2];
  int i, rc;
  ID id, nid;

  /* Freshly allocated, ignore it */
  if (!ic->head && ic->count <= MDB_IDL_DB_SIZE) {
    return 0;
  }

  key.iov_base = ic->kstr.bv_val;
  key.iov_len = ic->kstr.bv_len;

  if (ic->count > MDB_IDL_DB_SIZE) {
    while (ic->flags & WAS_FOUND) {
      rc = mdbx_cursor_get(mc, &key, data, MDBX_SET);
      if (rc) {
        /* FIXME: find out why this happens */
        ic->flags = 0;
        break;
      }
      if (ic->flags & WAS_RANGE) {
        /* Skip lo */
        rc = mdbx_cursor_get(mc, &key, data, MDBX_NEXT_DUP);

        /* Get hi */
        rc = mdbx_cursor_get(mc, &key, data, MDBX_NEXT_DUP);

        /* Store range hi */
        data[0].iov_base = &ic->last;
        rc = mdbx_cursor_put(mc, &key, data, MDBX_CURRENT);
      } else {
        /* Delete old data, replace with range */
        ic->first = *(ID *)data[0].iov_base;
        mdbx_cursor_del(mc, MDBX_NODUPDATA);
      }
      break;
    }
    if (!(ic->flags & WAS_RANGE)) {
      /* range, didn't exist before */
      nid = 0;
      data[0].iov_len = sizeof(ID);
      data[0].iov_base = &nid;
      rc = mdbx_cursor_put(mc, &key, data, 0);
      if (rc == 0) {
        data[0].iov_base = &ic->first;
        rc = mdbx_cursor_put(mc, &key, data, 0);
        if (rc == 0) {
          data[0].iov_base = &ic->last;
          rc = mdbx_cursor_put(mc, &key, data, 0);
        }
      }
      if (rc) {
        rc = -1;
      }
    }
  } else {
    /* Normal write */
    int n;

    data[0].iov_len = sizeof(ID);
    rc = 0;
    for (ice = ic->head, n = 0; ice; ice = ice->next, n++) {
      int end;
      if (ice->next) {
        end = IDBLOCK;
      } else {
        end = (ic->count - ic->offset) & (IDBLOCK - 1);
        if (!end)
          end = IDBLOCK;
      }
      data[1].iov_len = end;
      data[0].iov_base = ice->ids;
      rc = mdbx_cursor_put(mc, &key, data, MDBX_APPENDDUP | MDBX_MULTIPLE);
      if (rc) {
        rc = -1;
        break;
      }
    }
    if (ic->head) {
      ic->tail->next = ai->ai_flist;
      ai->ai_flist = ic->head;
    }
  }
  ic->head = ai->ai_clist;
  ai->ai_clist = ic;
  return rc;
}

static int mdb_tool_idl_flush_db(MDBX_txn *txn, AttrInfo *ai, AttrIxInfo *ax) {
  MDBX_cursor *mc;
  Avlnode *root;
  int rc;

  mdbx_cursor_open(txn, ai->ai_dbi, &mc);
  root = tavl_end(ai->ai_root, TAVL_DIR_LEFT);
  do {
    rc = mdb_tool_idl_flush_one(mc, ax, root->avl_data);
    if (rc != -1)
      rc = 0;
  } while ((root = tavl_next(root, TAVL_DIR_RIGHT)));
  mdbx_cursor_close(mc);

  return rc;
}

static int mdb_tool_idl_flush(BackendDB *be, MDBX_txn *txn) {
  struct mdb_info *mdb = (struct mdb_info *)be->be_private;
  int rc = 0;
  unsigned int i, dbi;

  for (i = 0; i < mdb->mi_nattrs; i++) {
    if (!mdb->mi_attrs[i]->ai_root)
      continue;
    rc = mdb_tool_idl_flush_db(txn, mdb->mi_attrs[i], mdb_tool_axinfo[i % mdb_tool_threads]);
    tavl_free(mdb->mi_attrs[i]->ai_root, NULL);
    mdb->mi_attrs[i]->ai_root = NULL;
    if (rc)
      break;
  }
  return rc;
}

int mdb_tool_idl_add(BackendDB *be, MDBX_cursor *mc, struct berval *keys, ID id) {
  MDBX_dbi dbi;
  mdb_tool_idl_cache *ic, itmp;
  mdb_tool_idl_cache_entry *ice;
  int i, rc, lcount;
  AttrIxInfo *ax = (AttrIxInfo *)mc;
  AttrInfo *ai = (AttrInfo *)ax->ai_ai;
  mc = ai->ai_cursor;

  dbi = ai->ai_dbi;
  for (i = 0; keys[i].bv_val; i++) {
    itmp.kstr = keys[i];
    ic = tavl_find(ai->ai_root, &itmp, mdb_tool_idl_cmp);

    /* No entry yet, create one */
    if (!ic) {
      MDBX_val key, data;
      ID nid;
      int rc;

      if (ax->ai_clist) {
        ic = ax->ai_clist;
        ax->ai_clist = ic->head;
      } else {
        ic = ch_malloc(sizeof(mdb_tool_idl_cache) + itmp.kstr.bv_len + 4);
      }
      ic->kstr.bv_len = itmp.kstr.bv_len;
      ic->kstr.bv_val = (char *)(ic + 1);
      memcpy(ic->kstr.bv_val, itmp.kstr.bv_val, ic->kstr.bv_len);
      ic->head = ic->tail = NULL;
      ic->last = 0;
      ic->count = 0;
      ic->offset = 0;
      ic->flags = 0;
      tavl_insert(&ai->ai_root, ic, mdb_tool_idl_cmp, avl_dup_error);

      /* load existing key count here */
      key.iov_len = keys[i].bv_len;
      key.iov_base = keys[i].bv_val;
      rc = mdbx_cursor_get(mc, &key, &data, MDBX_SET);
      if (rc == 0) {
        ic->flags |= WAS_FOUND;
        nid = *(ID *)data.iov_base;
        if (nid == 0) {
          ic->count = MDB_IDL_DB_SIZE + 1;
          ic->flags |= WAS_RANGE;
        } else {
          size_t count;

          mdbx_cursor_count(mc, &count);
          ic->count = count;
          ic->first = nid;
          ic->offset = count & (IDBLOCK - 1);
        }
      }
    }
    /* are we a range already? */
    if (ic->count > MDB_IDL_DB_SIZE) {
      ic->last = id;
      continue;
      /* Are we at the limit, and converting to a range? */
    } else if (ic->count == MDB_IDL_DB_SIZE) {
      if (ic->head) {
        ic->tail->next = ax->ai_flist;
        ax->ai_flist = ic->head;
      }
      ic->head = ic->tail = NULL;
      ic->last = id;
      ic->count++;
      continue;
    }
    /* No free block, create that too */
    lcount = (ic->count - ic->offset) & (IDBLOCK - 1);
    if (!ic->tail || lcount == 0) {
      if (ax->ai_flist) {
        ice = ax->ai_flist;
        ax->ai_flist = ice->next;
      } else {
        ice = ch_malloc(sizeof(mdb_tool_idl_cache_entry));
      }
      ice->next = NULL;
      if (!ic->head) {
        ic->head = ice;
      } else {
        ic->tail->next = ice;
      }
      ic->tail = ice;
      if (lcount)
        ice->ids[lcount - 1] = 0;
      if (!ic->count)
        ic->first = id;
    }
    ice = ic->tail;
    if (!lcount || ice->ids[lcount - 1] != id) {
      ice->ids[lcount] = id;
      ic->count++;
    }
  }

  return 0;
}
#endif /* MDB_TOOL_IDL_CACHING */

/* Upgrade from pre 2.4.34 dn2id format */

#include <ac/unistd.h>
#include <lutil_meter.h>

#define STACKSIZ 2048

typedef struct rec {
  ID id;
  size_t len;
  char rdn[512];
} rec;

static int mdb_dn2id_upgrade(BackendDB *be) {
  struct mdb_info *mi = (struct mdb_info *)be->be_private;
  MDBX_txn *mt;
  MDBX_cursor *mc = NULL;
  MDBX_val key, data;
  int rc, writes = 0, depth = 0;
  int enable_meter = 0;
  ID id = 0, *num, count = 0;
  rec *stack;
  lutil_meter_t meter;

  if (!(mi->mi_flags & MDB_NEED_UPGRADE)) {
    Debug(LDAP_DEBUG_ANY, "database %s: No upgrade needed.\n", be->be_suffix[0].bv_val);
    return 0;
  }

  {
    MDBX_stat st;

    mdbx_dbi_stat(mdbx_cursor_txn(cursor), mi->mi_dbis[MDB_ID2ENTRY], &st, sizeof(st));
    if (!st.ms_entries) {
      /* Empty DB, nothing to upgrade? */
      return 0;
    }
    if (isatty(2))
      enable_meter = !lutil_meter_open(&meter, &lutil_meter_text_display, &lutil_meter_linear_estimator, st.ms_entries);
  }

  num = ch_malloc(STACKSIZ * (sizeof(ID) + sizeof(rec)));
  stack = (rec *)(num + STACKSIZ);

  rc = mdbx_txn_begin(mi->mi_dbenv, NULL, 0, &mt);
  if (rc) {
    Debug(LDAP_DEBUG_ANY, "mdb_dn2id_upgrade: mdbx_txn_begin failed, %s (%d)\n", mdbx_strerror(rc), rc);
    goto leave;
  }
  rc = mdbx_cursor_open(mt, mi->mi_dbis[MDB_DN2ID], &mc);
  if (rc) {
    Debug(LDAP_DEBUG_ANY, "mdb_dn2id_upgrade: mdbx_cursor_open failed, %s (%d)\n", mdbx_strerror(rc), rc);
    goto leave;
  }

  key.iov_len = sizeof(ID);
  /* post-order depth-first update */
  for (;;) {
    size_t dkids;
    unsigned char *ptr;

    /* visit */
    key.iov_base = &id;
    stack[depth].id = id;
    rc = mdbx_cursor_get(mc, &key, &data, MDBX_SET);
    if (rc) {
      Debug(LDAP_DEBUG_ANY, "mdb_dn2id_upgrade: mdbx_cursor_get failed, %s (%d)\n", mdbx_strerror(rc), rc);
      goto leave;
    }
    num[depth] = 1;

    rc = mdbx_cursor_count(mc, &dkids);
    if (rc) {
      Debug(LDAP_DEBUG_ANY, "mdb_dn2id_upgrade: mdbx_cursor_count failed, %s (%d)\n", mdbx_strerror(rc), rc);
      goto leave;
    }
    if (dkids > 1) {
      rc = mdbx_cursor_get(mc, &key, &data, MDBX_NEXT_DUP);
    down:
      ptr = (unsigned char *)data.iov_base + data.iov_len - sizeof(ID);
      memcpy(&id, ptr, sizeof(ID));
      depth++;
      memcpy(stack[depth].rdn, data.iov_base, data.iov_len);
      stack[depth].len = data.iov_len;
      continue;
    }

    /* pop: write updated count, advance to next node */
  pop:
    /* update superior counts */
    if (depth)
      num[depth - 1] += num[depth];

    key.iov_base = &id;
    id = stack[depth - 1].id;
    data.iov_base = stack[depth].rdn;
    data.iov_len = stack[depth].len;
    rc = mdbx_cursor_get(mc, &key, &data, MDBX_GET_BOTH);
    if (rc) {
      Debug(LDAP_DEBUG_ANY, "mdb_dn2id_upgrade: mdbx_cursor_get(BOTH) failed, %s (%d)\n", mdbx_strerror(rc), rc);
      goto leave;
    }
    data.iov_base = stack[depth].rdn;
    ptr = (unsigned char *)data.iov_base + data.iov_len;
    memcpy(ptr, &num[depth], sizeof(ID));
    data.iov_len += sizeof(ID);
    rc = mdbx_cursor_del(mc, 0);
    if (rc) {
      Debug(LDAP_DEBUG_ANY, "mdb_dn2id_upgrade: mdbx_cursor_del failed, %s (%d)\n", mdbx_strerror(rc), rc);
      goto leave;
    }
    rc = mdbx_cursor_put(mc, &key, &data, 0);
    if (rc) {
      Debug(LDAP_DEBUG_ANY, "mdb_dn2id_upgrade: mdbx_cursor_put failed, %s (%d)\n", mdbx_strerror(rc), rc);
      goto leave;
    }
    count++;
#if 1
    if (enable_meter)
      lutil_meter_update(&meter, count, 0);
#else
    {
      int len;
      ptr = data.iov_base;
      len = (ptr[0] & 0x7f) << 8 | ptr[1];
      printf("ID: %zu, %zu, %.*s\n", stack[depth].id, num[depth], len, ptr + 2);
    }
#endif
    writes++;
    if (writes == 1000) {
      mdbx_cursor_close(mc);
      mc = NULL;
      rc = mdb_tool_txn_commit(be, mt);
      mt = NULL;
      if (rc) {
        Debug(LDAP_DEBUG_ANY, "mdb_dn2id_upgrade: mdbx_txn_commit failed, %s (%d)\n", mdbx_strerror(rc), rc);
        goto leave;
      }
      rc = mdbx_txn_begin(mi->mi_dbenv, NULL, 0, &mt);
      if (rc) {
        Debug(LDAP_DEBUG_ANY, "mdb_dn2id_upgrade: mdbx_txn_begin(2) failed, %s (%d)\n", mdbx_strerror(rc), rc);
        goto leave;
      }
      rc = mdbx_cursor_open(mt, mi->mi_dbis[MDB_DN2ID], &mc);
      if (rc) {
        Debug(LDAP_DEBUG_ANY, "mdb_dn2id_upgrade: mdbx_cursor_open(2) failed, %s (%d)\n", mdbx_strerror(rc), rc);
        goto leave;
      }
      rc = mdbx_cursor_get(mc, &key, &data, MDBX_GET_BOTH);
      if (rc) {
        Debug(LDAP_DEBUG_ANY, "mdb_dn2id_upgrade: mdbx_cursor_get(2) failed, %s (%d)\n", mdbx_strerror(rc), rc);
        goto leave;
      }
      writes = 0;
    }
    depth--;

    rc = mdbx_cursor_get(mc, &key, &data, MDBX_NEXT_DUP);
    if (rc == 0)
      goto down;
    rc = 0;
    if (depth)
      goto pop;
    else
      break;
  }
leave:
  if (mc) {
    mdbx_cursor_close(mc);
    mc = NULL;
  }
  if (mt) {
    int err = mdb_tool_txn_commit(be, mt);
    if (err) {
      Debug(LDAP_DEBUG_ANY, "mdb_dn2id_upgrade: mdbx_txn_commit(2) failed, %s (%d)\n", mdbx_strerror(err), err);
      if (!rc)
        rc = err;
    }
  }
  ch_free(num);
  if (enable_meter) {
    lutil_meter_update(&meter, count, 1);
    lutil_meter_close(&meter);
  }
  return rc;
}
