/* $ReOpenLDAP$ */
/* Copyright 2007-2018 ReOpenLDAP AUTHORS: please see AUTHORS file.
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
 * This work was initially developed by Brian Candler for inclusion
 * in OpenLDAP Software.
 */

#include "reldap.h"

#include <stdio.h>

#include <ac/errno.h>
#include <ac/string.h>
#include <ac/socket.h>
#include <ac/unistd.h>

#include "slap.h"
#include "back-sock.h"

/*
 * FIXME: count the number of concurrent open sockets (since each thread
 * may open one). Perhaps block here if a soft limit is reached, and fail
 * if a hard limit reached
 */

FILE *opensock(const char *sockpath) {
  int fd;
  FILE *fp;
  struct sockaddr_un sockun;

  fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    Debug(LDAP_DEBUG_ANY, "socket create failed\n");
    return (NULL);
  }

  sockun.sun_family = AF_UNIX;
  sprintf(sockun.sun_path, "%.*s", (int)(sizeof(sockun.sun_path) - 1), sockpath);
  if (connect(fd, (struct sockaddr *)&sockun, sizeof(sockun)) < 0) {
    Debug(LDAP_DEBUG_ANY, "socket connect(%s) failed\n", sockpath ? sockpath : "<null>");
    close(fd);
    return (NULL);
  }

  if ((fp = fdopen(fd, "r+")) == NULL) {
    Debug(LDAP_DEBUG_ANY, "fdopen failed\n");
    close(fd);
    return (NULL);
  }

  return (fp);
}
