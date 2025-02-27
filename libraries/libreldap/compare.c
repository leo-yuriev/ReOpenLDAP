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

#include <ac/socket.h>
#include <ac/string.h>
#include <ac/time.h>

#include "ldap-int.h"
#include "ldap_log.h"

/* The compare request looks like this:
 *	CompareRequest ::= SEQUENCE {
 *		entry	DistinguishedName,
 *		ava	SEQUENCE {
 *			type	AttributeType,
 *			value	AttributeValue
 *		}
 *	}
 */

BerElement *ldap_build_compare_req(LDAP *ld, const char *dn, const char *attr, struct berval *bvalue,
                                   LDAPControl **sctrls, LDAPControl **cctrls, int *msgidp) {
  BerElement *ber;
  int rc;

  /* create a message to send */
  if ((ber = ldap_alloc_ber_with_options(ld)) == NULL) {
    return (NULL);
  }

  LDAP_NEXT_MSGID(ld, *msgidp);
  rc = ber_printf(ber, "{it{s{sON}N}", /* '}' */
                  *msgidp, LDAP_REQ_COMPARE, dn, attr, bvalue);
  if (rc == -1) {
    ld->ld_errno = LDAP_ENCODING_ERROR;
    ber_free(ber, 1);
    return (NULL);
  }

  /* Put Server Controls */
  if (ldap_int_put_controls(ld, sctrls, ber) != LDAP_SUCCESS) {
    ber_free(ber, 1);
    return (NULL);
  }

  if (ber_printf(ber, /*{*/ "N}") == -1) {
    ld->ld_errno = LDAP_ENCODING_ERROR;
    ber_free(ber, 1);
    return (NULL);
  }

  return (ber);
}

/*
 * ldap_compare_ext - perform an ldap extended compare operation.  The dn
 * of the entry to compare to and the attribute and value to compare (in
 * attr and value) are supplied.  The msgid of the response is returned.
 *
 * Example:
 *	struct berval bvalue = { "secret", sizeof("secret")-1 };
 *	rc = ldap_compare( ld, "c=us@cn=bob",
 *		"userPassword", &bvalue,
 *		sctrl, cctrl, &msgid )
 */
int ldap_compare_ext(LDAP *ld, const char *dn, const char *attr, struct berval *bvalue, LDAPControl **sctrls,
                     LDAPControl **cctrls, int *msgidp) {
  int rc;
  BerElement *ber;
  ber_int_t id;

  Debug(LDAP_DEBUG_TRACE, "ldap_compare\n");

  assert(ld != NULL);
  assert(LDAP_VALID(ld));
  assert(dn != NULL);
  assert(attr != NULL);
  assert(msgidp != NULL);

  /* check client controls */
  rc = ldap_int_client_controls(ld, cctrls);
  if (rc != LDAP_SUCCESS)
    return rc;

  ber = ldap_build_compare_req(ld, dn, attr, bvalue, sctrls, cctrls, &id);
  if (!ber)
    return ld->ld_errno;

  /* send the message */
  *msgidp = ldap_send_initial_request(ld, LDAP_REQ_COMPARE, dn, ber, id);
  return (*msgidp < 0 ? ld->ld_errno : LDAP_SUCCESS);
}

int ldap_compare_ext_s(LDAP *ld, const char *dn, const char *attr, struct berval *bvalue, LDAPControl **sctrl,
                       LDAPControl **cctrl) {
  int rc;
  int msgid = 0;
  LDAPMessage *res;

  rc = ldap_compare_ext(ld, dn, attr, bvalue, sctrl, cctrl, &msgid);

  if (rc != LDAP_SUCCESS)
    return (rc);

  if (ldap_result(ld, msgid, LDAP_MSG_ALL, (struct timeval *)NULL, &res) == -1 || !res)
    return (ld->ld_errno);

  return (ldap_result2error(ld, res, 1));
}
