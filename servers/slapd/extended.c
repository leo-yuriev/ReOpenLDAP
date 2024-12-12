/* $ReOpenLDAP$ */
/* Copyright 1990-2018 ReOpenLDAP AUTHORS: please see AUTHORS file.
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

/*
 * LDAPv3 Extended Operation Request
 *	ExtendedRequest ::= [APPLICATION 23] SEQUENCE {
 *		requestName	 [0] LDAPOID,
 *		requestValue	 [1] OCTET STRING OPTIONAL
 *	}
 *
 * LDAPv3 Extended Operation Response
 *	ExtendedResponse ::= [APPLICATION 24] SEQUENCE {
 *		COMPONENTS OF LDAPResult,
 *		responseName	 [10] LDAPOID OPTIONAL,
 *		response	 [11] OCTET STRING OPTIONAL
 *	}
 *
 */

#include "reldap.h"

#include <stdio.h>

#include <ac/socket.h>
#include <ac/string.h>

#include "slap.h"
#include "lber_pvt.h"

static struct extop_list {
  struct extop_list *next;
  struct berval oid;
  slap_mask_t flags;
  SLAP_EXTOP_MAIN_FN *exop_main;
} *supp_exop_list = NULL;

static SLAP_EXTOP_MAIN_FN whoami_extop;

/* This list of built-in extops is for extops that are not part
 * of backends or in external modules.	Essentially, this is
 * just a way to get built-in extops onto the extop list without
 * having a separate init routine for each built-in extop.
 */
static struct {
  const struct berval *oid;
  slap_mask_t flags;
  SLAP_EXTOP_MAIN_FN *exop_main;
} builtin_extops[] = {
#ifdef LDAP_X_TXN
    {&slap_EXOP_TXN_START, 0, txn_start_extop},
    {&slap_EXOP_TXN_END, 0, txn_end_extop},
#endif
    {&slap_EXOP_CANCEL, 0, cancel_extop},
    {&slap_EXOP_WHOAMI, 0, whoami_extop},
    {&slap_EXOP_MODIFY_PASSWD, SLAP_EXOP_WRITES, passwd_extop},
    {NULL, 0, NULL}};

static struct extop_list *find_extop(struct extop_list *list, struct berval *oid);

struct berval *get_supported_extop(int index) {
  struct extop_list *ext;

  /* linear scan is slow, but this way doesn't force a
   * big change on root_dse.c, where this routine is used.
   */
  for (ext = supp_exop_list; ext != NULL && --index >= 0; ext = ext->next) {
    ; /* empty */
  }

  if (ext == NULL)
    return NULL;

  return &ext->oid;
}

int exop_root_dse_info(Entry *e) {
  AttributeDescription *ad_supportedExtension = slap_schema.si_ad_supportedExtension;
  struct berval vals[2];
  struct extop_list *ext;

  vals[1].bv_val = NULL;
  vals[1].bv_len = 0;

  for (ext = supp_exop_list; ext != NULL; ext = ext->next) {
    if (ext->flags & SLAP_EXOP_HIDE)
      continue;

    vals[0] = ext->oid;

    if (attr_merge(e, ad_supportedExtension, vals, NULL)) {
      return LDAP_OTHER;
    }
  }

  return LDAP_SUCCESS;
}

int do_extended(Operation *op, SlapReply *rs) {
  struct berval reqdata = {0, NULL};
  ber_len_t len;

  Debug(LDAP_DEBUG_TRACE, "%s do_extended\n", op->o_log_prefix);

  if (op->o_protocol < LDAP_VERSION3) {
    Debug(LDAP_DEBUG_ANY, "%s do_extended: protocol version (%d) too low\n", op->o_log_prefix, op->o_protocol);
    send_ldap_discon(op, rs, LDAP_PROTOCOL_ERROR, "requires LDAPv3");
    rs->sr_err = SLAPD_DISCONNECT;
    goto done;
  }

  if (ber_scanf(op->o_ber, "{m" /*}*/, &op->ore_reqoid) == LBER_ERROR) {
    Debug(LDAP_DEBUG_ANY, "%s do_extended: ber_scanf failed\n", op->o_log_prefix);
    send_ldap_discon(op, rs, LDAP_PROTOCOL_ERROR, "decoding error");
    rs->sr_err = SLAPD_DISCONNECT;
    goto done;
  }

  if (ber_peek_tag(op->o_ber, &len) == LDAP_TAG_EXOP_REQ_VALUE) {
    if (ber_scanf(op->o_ber, "m", &reqdata) == LBER_ERROR) {
      Debug(LDAP_DEBUG_ANY, "%s do_extended: ber_scanf failed\n", op->o_log_prefix);
      send_ldap_discon(op, rs, LDAP_PROTOCOL_ERROR, "decoding error");
      rs->sr_err = SLAPD_DISCONNECT;
      goto done;
    }
  }

  if (get_ctrls(op, rs, 1) != LDAP_SUCCESS) {
    Debug(LDAP_DEBUG_ANY, "%s do_extended: get_ctrls failed\n", op->o_log_prefix);
    return rs->sr_err;
  }

  Statslog(LDAP_DEBUG_STATS, "%s EXT oid=%s\n", op->o_log_prefix, op->ore_reqoid.bv_val);

  /* check for controls inappropriate for all extended operations */
  if (get_manageDSAit(op) == SLAP_CONTROL_CRITICAL) {
    send_ldap_error(op, rs, LDAP_UNAVAILABLE_CRITICAL_EXTENSION, "manageDSAit control inappropriate");
    goto done;
  }

  /* FIXME: temporary? */
  if (reqdata.bv_val) {
    op->ore_reqdata = &reqdata;
  }

  op->o_bd = frontendDB;
  rs->sr_err = frontendDB->be_extended(op, rs);

  /* clean up in case some overlay set them? */
  if (!BER_BVISNULL(&op->o_req_ndn)) {
    if (!BER_BVISNULL(&op->o_req_dn) && op->o_req_ndn.bv_val != op->o_req_dn.bv_val) {
      op->o_tmpfree(op->o_req_dn.bv_val, op->o_tmpmemctx);
    }
    op->o_tmpfree(op->o_req_ndn.bv_val, op->o_tmpmemctx);
    BER_BVZERO(&op->o_req_dn);
    BER_BVZERO(&op->o_req_ndn);
  }

done:
  return rs->sr_err;
}

int fe_extended(Operation *op, SlapReply *rs) {
  struct extop_list *ext = NULL;

  ext = find_extop(supp_exop_list, &op->ore_reqoid);
  if (ext == NULL) {
    Debug(LDAP_DEBUG_ANY, "%s do_extended: unsupported operation \"%s\"\n", op->o_log_prefix, op->ore_reqoid.bv_val);
    send_ldap_error(op, rs, LDAP_PROTOCOL_ERROR, "unsupported extended operation");
    goto done;
  }

  op->ore_flags = ext->flags;

  Debug(LDAP_DEBUG_ARGS, "do_extended: oid=%s\n", op->ore_reqoid.bv_val);

  { /* start of OpenLDAP extended operation */
    BackendDB *bd = op->o_bd;

    rs->sr_err = (ext->exop_main)(op, rs);

    if (rs->sr_err != SLAPD_ABANDON) {
      if (rs->sr_err == LDAP_REFERRAL && rs->sr_ref == NULL) {
        rs->sr_ref = referral_rewrite(default_referral, NULL, NULL, LDAP_SCOPE_DEFAULT);
        if (!rs->sr_ref)
          rs->sr_ref = default_referral;
        else
          rs->sr_flags |= REP_REF_MUSTBEFREED;
        if (!rs->sr_ref) {
          rs->sr_err = LDAP_UNWILLING_TO_PERFORM;
          rs->sr_text = "referral missing";
        }
      }

      if (op->o_bd == NULL)
        op->o_bd = bd;
      send_ldap_extended(op, rs);
      rs_send_cleanup(rs);
    }

    if (rs->sr_rspoid != NULL) {
      free((char *)rs->sr_rspoid);
      rs->sr_rspoid = NULL;
    }

    if (rs->sr_rspdata != NULL) {
      ber_bvfree(rs->sr_rspdata);
      rs->sr_rspdata = NULL;
    }
  } /* end of OpenLDAP extended operation */

done:;
  return rs->sr_err;
}

int extop_register_ex(const struct berval *exop_oid, slap_mask_t exop_flags, SLAP_EXTOP_MAIN_FN *exop_main,
                      unsigned do_not_replace) {
  struct berval oidm = BER_BVNULL;
  struct extop_list *ext;
  int insertme = 0;

  if (!exop_main) {
    return -1;
  }

  if (exop_oid == NULL || BER_BVISNULL(exop_oid) || BER_BVISEMPTY(exop_oid)) {
    return -1;
  }

  if (numericoidValidate(NULL, (struct berval *)exop_oid) != LDAP_SUCCESS) {
    oidm.bv_val = oidm_find(exop_oid->bv_val);
    if (oidm.bv_val == NULL) {
      return -1;
    }
    oidm.bv_len = strlen(oidm.bv_val);
    exop_oid = &oidm;
  }

  for (ext = supp_exop_list; ext; ext = ext->next) {
    if (bvmatch(exop_oid, &ext->oid)) {
      if (do_not_replace != 0) {
        break;
      }
      return -1;
    }
  }

  if (do_not_replace == 0 || ext == NULL) {
    ext = ch_calloc(1, sizeof(struct extop_list) + exop_oid->bv_len + 1);
    if (ext == NULL) {
      return (-1);
    }

    ext->oid.bv_val = (char *)(ext + 1);
    memcpy(ext->oid.bv_val, exop_oid->bv_val, exop_oid->bv_len);
    ext->oid.bv_len = exop_oid->bv_len;
    ext->oid.bv_val[ext->oid.bv_len] = '\0';

    insertme = 1;
  }

  ext->flags = exop_flags;
  ext->exop_main = exop_main;

  if (insertme) {
    ext->next = supp_exop_list;
    supp_exop_list = ext;
  }

  return 0;
}

int extop_unregister(const struct berval *exop_oid, SLAP_EXTOP_MAIN_FN *exop_main, unsigned unused_flags) {
  struct berval oidm = BER_BVNULL;
  struct extop_list *ext, **extp;
  (void)unused_flags;

  /* oid must be given */
  if (exop_oid == NULL || BER_BVISNULL(exop_oid) || BER_BVISEMPTY(exop_oid)) {
    return -1;
  }

  /* if it's not an oid, check if it's a macto */
  if (numericoidValidate(NULL, (struct berval *)exop_oid) != LDAP_SUCCESS) {
    oidm.bv_val = oidm_find(exop_oid->bv_val);
    if (oidm.bv_val == NULL) {
      return -1;
    }
    oidm.bv_len = strlen(oidm.bv_val);
    exop_oid = &oidm;
  }

  /* lookup the oid */
  for (extp = &supp_exop_list; *extp; extp = &(*extp)->next) {
    if (bvmatch(exop_oid, &(*extp)->oid)) {
      /* if ext_main is given, only remove if it matches */
      if (exop_main != NULL && (*extp)->exop_main != exop_main) {
        return -1;
      }
      break;
    }
  }

  if (*extp == NULL) {
    return -1;
  }

  ext = *extp;
  *extp = (*extp)->next;

  ch_free(ext);
  return 0;
}

int extops_init(void) {
  int i;

  for (i = 0; builtin_extops[i].oid != NULL; i++) {
    extop_register((struct berval *)builtin_extops[i].oid, builtin_extops[i].flags, builtin_extops[i].exop_main);
  }

  return 0;
}

int extops_destroy(void) {
  struct extop_list *ext;

  /* we allocated the memory, so we have to free it, too. */
  while ((ext = supp_exop_list) != NULL) {
    supp_exop_list = ext->next;
    ch_free(ext);
  }
  return 0;
}

static struct extop_list *find_extop(struct extop_list *list, struct berval *oid) {
  struct extop_list *ext;

  for (ext = list; ext; ext = ext->next) {
    if (bvmatch(&ext->oid, oid))
      return (ext);
  }
  return NULL;
}

const struct berval slap_EXOP_WHOAMI = BER_BVC(LDAP_EXOP_WHO_AM_I);

static int whoami_extop(Operation *op, SlapReply *rs) {
  struct berval *bv;

  if (op->ore_reqdata != NULL) {
    /* no request data should be provided */
    rs->sr_text = "no request data expected";
    return LDAP_PROTOCOL_ERROR;
  }

  Statslog(LDAP_DEBUG_STATS, "%s WHOAMI\n", op->o_log_prefix);

  op->o_bd = op->o_conn->c_authz_backend;
  if (backend_check_restrictions(op, rs, (struct berval *)&slap_EXOP_WHOAMI) != LDAP_SUCCESS) {
    return rs->sr_err;
  }

  bv = (struct berval *)ch_malloc(sizeof(struct berval));
  if (op->o_dn.bv_len) {
    bv->bv_len = op->o_dn.bv_len + STRLENOF("dn:");
    bv->bv_val = ch_malloc(bv->bv_len + 1);
    memcpy(bv->bv_val, "dn:", STRLENOF("dn:"));
    memcpy(&bv->bv_val[STRLENOF("dn:")], op->o_dn.bv_val, op->o_dn.bv_len);
    bv->bv_val[bv->bv_len] = '\0';

  } else {
    bv->bv_len = 0;
    bv->bv_val = NULL;
  }

  rs->sr_rspdata = bv;
  return LDAP_SUCCESS;
}
