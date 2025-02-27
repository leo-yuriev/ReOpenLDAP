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

#include "reldap.h"

#include <stdio.h>
#include <ac/stdlib.h>

#include <ac/ctype.h>
#include <ac/string.h>
#include <ac/time.h>

#include "ldap-int.h"

struct entrything {
  char **et_vals;
  LDAPMessage *et_msg;
  int (*et_cmp_fn)(const char *a, const char *b);
};

static int et_cmp(const void *aa, const void *bb);

int ldap_sort_strcasecmp(const void *a, const void *b) { return (strcasecmp(*(char *const *)a, *(char *const *)b)); }

static int et_cmp(const void *aa, const void *bb) {
  int i, rc;
  const struct entrything *a = (const struct entrything *)aa;
  const struct entrything *b = (const struct entrything *)bb;

  if (a->et_vals == NULL && b->et_vals == NULL)
    return (0);
  if (a->et_vals == NULL)
    return (-1);
  if (b->et_vals == NULL)
    return (1);

  for (i = 0; a->et_vals[i] && b->et_vals[i]; i++) {
    if ((rc = a->et_cmp_fn(a->et_vals[i], b->et_vals[i])) != 0) {
      return (rc);
    }
  }

  if (a->et_vals[i] == NULL && b->et_vals[i] == NULL)
    return (0);
  if (a->et_vals[i] == NULL)
    return (-1);
  return (1);
}

int ldap_sort_entries(LDAP *ld, LDAPMessage **chain, const char *attr, /* NULL => sort by DN */
                      int (*cmp)(const char *, const char *)) {
  int i, count = 0;
  struct entrything *et;
  LDAPMessage *e, *ehead = NULL, *etail = NULL;
  LDAPMessage *ohead = NULL, *otail = NULL;
  LDAPMessage **ep;

  assert(ld != NULL);

  /* Separate entries from non-entries */
  for (e = *chain; e; e = e->lm_chain) {
    if (e->lm_msgtype == LDAP_RES_SEARCH_ENTRY) {
      count++;
      if (!ehead)
        ehead = e;
      if (etail)
        etail->lm_chain = e;
      etail = e;
    } else {
      if (!ohead)
        ohead = e;
      if (otail)
        otail->lm_chain = e;
      otail = e;
    }
  }

  if (count < 2) {
    /* zero or one entries -- already sorted! */
    if (ehead) {
      etail->lm_chain = ohead;
      *chain = ehead;
    } else {
      *chain = ohead;
    }
    return 0;
  }

  if ((et = (struct entrything *)LDAP_MALLOC(count * sizeof(struct entrything))) == NULL) {
    ld->ld_errno = LDAP_NO_MEMORY;
    return (-1);
  }

  e = ehead;
  for (i = 0; i < count; i++) {
    et[i].et_cmp_fn = cmp;
    et[i].et_msg = e;
    if (attr == NULL) {
      char *dn;

      dn = ldap_get_dn(ld, e);
      et[i].et_vals = ldap_explode_dn(dn, 1);
      LDAP_FREE(dn);
    } else {
      et[i].et_vals = ldap_get_values(ld, e, attr);
    }

    e = e->lm_chain;
  }

  qsort(et, count, sizeof(struct entrything), et_cmp);

  ep = chain;
  for (i = 0; i < count; i++) {
    *ep = et[i].et_msg;
    ep = &(*ep)->lm_chain;

    LDAP_VFREE(et[i].et_vals);
  }
  *ep = ohead;
  (*chain)->lm_chain_tail = otail ? otail : etail;

  LDAP_FREE((char *)et);

  return (0);
}

int ldap_sort_values(LDAP *ld, char **vals, int (*cmp)(const void *, const void *)) {
  int nel;

  for (nel = 0; vals[nel] != NULL; nel++)
    ; /* NULL */

  qsort(vals, nel, sizeof(char *), cmp);

  return (0);
}
