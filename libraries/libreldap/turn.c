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
/* ACKNOWLEDGEMENTS:
 * This program was originally developed by Kurt D. Zeilenga for inclusion in
 * OpenLDAP Software.
 */

/*
 * LDAPv3 Turn Operation Request
 */

#include "reldap.h"

#include <stdio.h>
#include <ac/stdlib.h>

#include <ac/socket.h>
#include <ac/string.h>
#include <ac/time.h>

#include "ldap-int.h"
#include "ldap_log.h"

int ldap_turn(LDAP *ld, int mutual, const char *identifier, LDAPControl **sctrls, LDAPControl **cctrls, int *msgidp) {
#ifdef LDAP_EXOP_X_TURN
  BerElement *turnvalber = NULL;
  struct berval *turnvalp = NULL;
  int rc;

  turnvalber = ber_alloc_t(LBER_USE_DER);
  if (mutual) {
    ber_printf(turnvalber, "{bs}", mutual, identifier);
  } else {
    ber_printf(turnvalber, "{s}", identifier);
  }
  ber_flatten(turnvalber, &turnvalp);

  rc = ldap_extended_operation(ld, LDAP_EXOP_X_TURN, turnvalp, sctrls, cctrls, msgidp);
  ber_free(turnvalber, 1);
  return rc;
#else
  return LDAP_CONTROL_NOT_FOUND;
#endif
}

int ldap_turn_s(LDAP *ld, int mutual, const char *identifier, LDAPControl **sctrls, LDAPControl **cctrls) {
#ifdef LDAP_EXOP_X_TURN
  BerElement *turnvalber = NULL;
  struct berval *turnvalp = NULL;
  int rc;

  turnvalber = ber_alloc_t(LBER_USE_DER);
  if (mutual) {
    ber_printf(turnvalber, "{bs}", 0xFF, identifier);
  } else {
    ber_printf(turnvalber, "{s}", identifier);
  }
  ber_flatten(turnvalber, &turnvalp);

  rc = ldap_extended_operation_s(ld, LDAP_EXOP_X_TURN, turnvalp, sctrls, cctrls, NULL, NULL);
  ber_free(turnvalber, 1);
  return rc;
#else
  return LDAP_CONTROL_NOT_FOUND;
#endif
}
