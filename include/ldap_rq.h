/* $ReOpenLDAP$ */
/* Copyright 1992-2018 ReOpenLDAP AUTHORS: please see AUTHORS file.
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

#ifndef LDAP_RQ_H
#define LDAP_RQ_H 1

#include <ldap_cdefs.h>

LDAP_BEGIN_DECL

typedef struct re_s {
  slap_time_t next_sched;
  slap_time_t interval;
  LDAP_STAILQ_ENTRY(re_s) tnext; /* it includes running */
  LDAP_STAILQ_ENTRY(re_s) rnext;
  ldap_pvt_thread_start_t *routine;
  void *arg;
  char *tname;
  char *tspec;
  void *pool_cookie;
} re_t;

typedef struct runqueue_s {
  LDAP_STAILQ_HEAD(l, re_s) task_list;
  LDAP_STAILQ_HEAD(rl, re_s) run_list;
  ldap_pvt_thread_mutex_t rq_mutex;
} runqueue_t;

LDAP_F(struct re_s *)
ldap_pvt_runqueue_insert_ns(struct runqueue_s *rq, slap_time_t interval, ldap_pvt_thread_start_t *routine, void *arg,
                            char *tname, char *tspec);

LDAP_F(struct re_s *)
ldap_pvt_runqueue_insert(struct runqueue_s *rq, int interval_seconds, ldap_pvt_thread_start_t *routine, void *arg,
                         char *tname, char *tspec);

LDAP_F(struct re_s *)
ldap_pvt_runqueue_find(struct runqueue_s *rq, ldap_pvt_thread_start_t *routine, void *arg);

LDAP_F(void)
ldap_pvt_runqueue_remove(struct runqueue_s *rq, struct re_s *entry);

LDAP_F(struct re_s *)
ldap_pvt_runqueue_next_sched(struct runqueue_s *rq, slap_time_t *next_run);

LDAP_F(void)
ldap_pvt_runqueue_runtask(struct runqueue_s *rq, struct re_s *entry);

LDAP_F(void)
ldap_pvt_runqueue_stoptask(struct runqueue_s *rq, struct re_s *entry);

LDAP_F(int)
ldap_pvt_runqueue_isrunning(struct runqueue_s *rq, struct re_s *entry);

LDAP_F(void)
ldap_pvt_runqueue_resched(struct runqueue_s *rq, struct re_s *entry, int defer);

LDAP_F(int)
ldap_pvt_runqueue_persistent_backload(struct runqueue_s *rq);

LDAP_END_DECL

#endif
