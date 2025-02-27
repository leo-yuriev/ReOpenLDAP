/* $ReOpenLDAP$ */
/* Copyright 2001-2018 ReOpenLDAP AUTHORS: please see AUTHORS file.
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

/* ACKNOWLEDGEMENTS:
 * This work was initially developed by Pierangelo Masarati for inclusion
 * in OpenLDAP Software.
 */

#include "reldap.h"

#include <stdio.h>
#include <ac/string.h>

#include "slap.h"
#include "back-monitor.h"

/*
 * initializes backend subentries
 */
int monitor_subsys_backend_init(BackendDB *be, monitor_subsys_t *ms) {
  monitor_info_t *mi;
  Entry *e_backend = NULL, **ep;
  int i;
  monitor_entry_t *mp;
  monitor_subsys_t *ms_database;
  BackendInfo *bi;

  mi = (monitor_info_t *)be->be_private;

  ms_database = monitor_back_get_subsys(SLAPD_MONITOR_DATABASE_NAME);
  if (ms_database == NULL) {
    Debug(LDAP_DEBUG_ANY, "monitor_subsys_backend_init: "
                          "unable to get "
                          "\"" SLAPD_MONITOR_DATABASE_NAME "\" "
                          "subsystem\n");
    return -1;
  }

  if (monitor_cache_get(mi, &ms->mss_ndn, &e_backend)) {
    Debug(LDAP_DEBUG_ANY,
          "monitor_subsys_backend_init: "
          "unable to get entry \"%s\"\n",
          ms->mss_ndn.bv_val);
    return (-1);
  }

  mp = (monitor_entry_t *)e_backend->e_private;
  mp->mp_children = NULL;
  ep = &mp->mp_children;
  int rc = -1;

  i = -1;
  LDAP_STAILQ_FOREACH(bi, &backendInfo, bi_next) {
    char buf[BACKMONITOR_BUFSIZE];
    BackendDB *be;
    struct berval bv;
    int j;
    Entry *e;

    i++;

    bv.bv_len = snprintf(buf, sizeof(buf), "cn=Backend %d", i);
    bv.bv_val = buf;

    e = monitor_entry_stub(&ms->mss_dn, &ms->mss_ndn, &bv, mi->mi_oc_monitoredObject, NULL, NULL);

    if (e == NULL) {
      Debug(LDAP_DEBUG_ANY,
            "monitor_subsys_backend_init: "
            "unable to create entry \"cn=Backend %d,%s\"\n",
            i, ms->mss_ndn.bv_val);
      goto bailout;
    }

    ber_str2bv(bi->bi_type, 0, 0, &bv);
    attr_merge_normalize_one(e, mi->mi_ad_monitoredInfo, &bv, NULL);
    attr_merge_normalize_one(e_backend, mi->mi_ad_monitoredInfo, &bv, NULL);

    attr_merge_normalize_one(e, mi->mi_ad_monitorRuntimeConfig,
                             bi->bi_cf_ocs == NULL ? (struct berval *)&slap_false_bv : (struct berval *)&slap_true_bv,
                             NULL);

    if (bi->bi_controls) {
      int j;

      for (j = 0; bi->bi_controls[j]; j++) {
        ber_str2bv(bi->bi_controls[j], 0, 0, &bv);
        attr_merge_one(e, slap_schema.si_ad_supportedControl, &bv, &bv);
      }
    }

    j = -1;
    LDAP_STAILQ_FOREACH(be, &backendDB, be_next) {
      char buf[SLAP_LDAPDN_MAXLEN];
      struct berval dn;

      j++;

      if (be->bd_info != bi) {
        continue;
      }

      snprintf(buf, sizeof(buf), "cn=Database %d,%s", j, ms_database->mss_dn.bv_val);

      ber_str2bv(buf, 0, 0, &dn);
      attr_merge_normalize_one(e, slap_schema.si_ad_seeAlso, &dn, NULL);
    }

    mp = monitor_entrypriv_create();
    if (mp == NULL) {
      goto bailout;
    }
    e->e_private = (void *)mp;
    mp->mp_info = ms;
    mp->mp_flags = ms->mss_flags | MONITOR_F_SUB;

    if (monitor_cache_add(mi, e)) {
      Debug(LDAP_DEBUG_ANY,
            "monitor_subsys_backend_init: "
            "unable to add entry \"cn=Backend %d,%s\"\n",
            i, ms->mss_ndn.bv_val);
      goto bailout;
    }

    *ep = e;
    ep = &mp->mp_next;
  }

  rc = 0;

bailout:
  monitor_cache_release(mi, e_backend);
  return rc;
}
