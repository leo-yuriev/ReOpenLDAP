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
#include <ac/unistd.h>

#include "back-mdb.h"

int mdb_bind(Operation *op, SlapReply *rs) {
  struct mdb_info *mdb = (struct mdb_info *)op->o_bd->be_private;
  Entry *e;
  Attribute *a;

  AttributeDescription *password = slap_schema.si_ad_userPassword;

  MDBX_txn *rtxn;
  mdb_op_info opinfo = {{{0}}}, *moi = &opinfo;

  Debug(LDAP_DEBUG_ARGS, "==> " LDAP_XSTRING(mdb_bind) ": dn: %s\n", op->o_req_dn.bv_val);

  /* allow noauth binds */
  switch (be_rootdn_bind(op, NULL)) {
  case LDAP_SUCCESS:
    /* frontend will send result */
    return rs->sr_err = LDAP_SUCCESS;

  default:
    /* give the database a chance */
    /* NOTE: this behavior departs from that of other backends,
     * since the others, in case of password checking failure
     * do not give the database a chance.  If an entry with
     * rootdn's name does not exist in the database the result
     * will be the same.  See ITS#4962 for discussion. */
    break;
  }

  rs->sr_err = mdb_opinfo_get(op, mdb, 1, &moi);
  switch (rs->sr_err) {
  case 0:
    break;
  default:
    rs->sr_text = "internal error";
    send_ldap_result(op, rs);
    return rs->sr_err;
  }

  rtxn = moi->moi_txn;

  /* get entry with reader lock */
  rs->sr_err = mdb_dn2entry(op, rtxn, NULL, &op->o_req_ndn, &e, NULL, 0);

  switch (rs->sr_err) {
  case MDBX_NOTFOUND:
    rs->sr_err = LDAP_INVALID_CREDENTIALS;
    goto done;
  case 0:
    break;
  case LDAP_BUSY:
    rs->sr_text = "ldap_server_busy";
    goto done;
  default:
    rs->sr_err = LDAP_OTHER;
    rs->sr_text = "internal error";
    goto done;
  }

  ber_dupbv(&op->oq_bind.rb_edn, &e->e_name);

  /* check for deleted */
  if (is_entry_subentry(e)) {
    /* entry is an subentry, don't allow bind */
    Debug(LDAP_DEBUG_TRACE, "entry is subentry\n");
    rs->sr_err = LDAP_INVALID_CREDENTIALS;
    goto done;
  }

  if (is_entry_alias(e)) {
    /* entry is an alias, don't allow bind */
    Debug(LDAP_DEBUG_TRACE, "entry is alias\n");
    rs->sr_err = LDAP_INVALID_CREDENTIALS;
    goto done;
  }

  if (is_entry_referral(e)) {
    Debug(LDAP_DEBUG_TRACE, "entry is referral\n");
    rs->sr_err = LDAP_INVALID_CREDENTIALS;
    goto done;
  }

  switch (op->oq_bind.rb_method) {
  case LDAP_AUTH_SIMPLE:
    a = attr_find(e->e_attrs, password);
    if (a == NULL) {
      rs->sr_err = LDAP_INVALID_CREDENTIALS;
      goto done;
    }

    if (slap_passwd_check(op, e, a, &op->oq_bind.rb_cred, &rs->sr_text) != 0) {
      /* failure; stop front end from sending result */
      rs->sr_err = LDAP_INVALID_CREDENTIALS;
      goto done;
    }

    rs->sr_err = 0;
    break;

  default:
    LDAP_BUG(); /* should not be reachable */
    rs->sr_err = LDAP_STRONG_AUTH_NOT_SUPPORTED;
    rs->sr_text = "authentication method not supported";
  }

done:
  if (moi == &opinfo || --moi->moi_ref < 1) {
    int __maybe_unused rc2 = mdbx_txn_reset(moi->moi_txn);
    assert(rc2 == MDBX_SUCCESS);
    if (moi->moi_oe.oe_key)
      LDAP_SLIST_REMOVE(&op->o_extra, &moi->moi_oe, OpExtra, oe_next);
    if ((moi->moi_flag & (MOI_FREEIT | MOI_KEEPER)) == MOI_FREEIT)
      op->o_tmpfree(moi, op->o_tmpmemctx);
  }
  /* free entry and reader lock */
  if (e != NULL) {
    mdb_entry_return(op, e);
  }

  if (rs->sr_err) {
    send_ldap_result(op, rs);
    rs_send_cleanup(rs);
  }
  /* front end will send result on success (rs->sr_err==0) */
  return rs->sr_err;
}
