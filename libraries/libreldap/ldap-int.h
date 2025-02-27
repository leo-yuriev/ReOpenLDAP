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

#ifndef _LDAP_INT_H
#define _LDAP_INT_H 1

#ifdef LDAP_R_COMPILE
#define LDAP_THREAD_SAFE 1
#endif

#include "lber-int.h"
#include "lutil.h"

#ifdef LDAP_R_COMPILE
#include <ldap_pvt_thread.h>
#endif

#ifdef HAVE_CYRUS_SASL
/* the need for this should be removed */
#ifdef HAVE_SASL_SASL_H
#include <sasl/sasl.h>
#else
#include <sasl.h>
#endif

#define SASL_MAX_BUFF_SIZE (0xffffff)
#define SASL_MIN_BUFF_SIZE 4096
#endif

/* for struct timeval */
#include <ac/time.h>

#undef TV2MILLISEC
#define TV2MILLISEC(tv) (((tv)->tv_sec * 1000) + ((tv)->tv_usec / 1000))

#include "ldap_log.h"
#include "ldap.h"
#include "ldap_pvt.h"

LDAP_BEGIN_DECL

#define LDAP_URL_PREFIX "ldap://"
#define LDAP_URL_PREFIX_LEN STRLENOF(LDAP_URL_PREFIX)
#define LDAPS_URL_PREFIX "ldaps://"
#define LDAPS_URL_PREFIX_LEN STRLENOF(LDAPS_URL_PREFIX)
#define LDAPI_URL_PREFIX "ldapi://"
#define LDAPI_URL_PREFIX_LEN STRLENOF(LDAPI_URL_PREFIX)
#ifdef LDAP_CONNECTIONLESS
#define LDAPC_URL_PREFIX "cldap://"
#define LDAPC_URL_PREFIX_LEN STRLENOF(LDAPC_URL_PREFIX)
#endif
#define LDAP_URL_URLCOLON "URL:"
#define LDAP_URL_URLCOLON_LEN STRLENOF(LDAP_URL_URLCOLON)

#define LDAP_REF_STR "Referral:\n"
#define LDAP_REF_STR_LEN STRLENOF(LDAP_REF_STR)
#define LDAP_LDAP_REF_STR LDAP_URL_PREFIX
#define LDAP_LDAP_REF_STR_LEN LDAP_URL_PREFIX_LEN

#define LDAP_DEFAULT_REFHOPLIMIT 5

#define LDAP_BOOL_REFERRALS 0
#define LDAP_BOOL_RESTART 1
#define LDAP_BOOL_TLS 3
#define LDAP_BOOL_CONNECT_ASYNC 4
#define LDAP_BOOL_SASL_NOCANON 5

#define LDAP_BOOLEANS unsigned long
#define LDAP_BOOL(n) ((LDAP_BOOLEANS)1 << (n))
#define LDAP_BOOL_GET(lo, bool) ((lo)->ldo_booleans & LDAP_BOOL(bool) ? -1 : 0)
#define LDAP_BOOL_SET(lo, bool) ((lo)->ldo_booleans |= LDAP_BOOL(bool))
#define LDAP_BOOL_CLR(lo, bool) ((lo)->ldo_booleans &= ~LDAP_BOOL(bool))
#define LDAP_BOOL_ZERO(lo) ((lo)->ldo_booleans = 0)

/*
 * This structure represents both ldap messages and ldap responses.
 * These are really the same, except in the case of search responses,
 * where a response has multiple messages.
 */

struct ldapmsg {
  ber_int_t lm_msgid;       /* the message id */
  ber_tag_t lm_msgtype;     /* the message type */
  BerElement *lm_ber;       /* the ber encoded message contents */
  struct ldapmsg *lm_chain; /* for search - next msg in the resp */
  struct ldapmsg *lm_chain_tail;
  struct ldapmsg *lm_next; /* next response */
  slap_time_t lm_time;     /* used to maintain cache */
};

#ifdef WITH_TLS
struct ldaptls {
  char *lt_certfile;
  char *lt_keyfile;
  char *lt_dhfile;
  char *lt_cacertfile;
  char *lt_cacertdir;
  char *lt_ciphersuite;
  char *lt_crlfile;
  char *lt_randfile; /* OpenSSL only */
  char *lt_ecname;   /* OpenSSL only */
  int lt_protocol_min;
  struct berval lt_cacert;
  struct berval lt_cert;
  struct berval lt_key;
};
#endif

typedef struct ldaplist {
  struct ldaplist *ll_next;
  void *ll_data;
} ldaplist;

/*
 * structure representing get/set'able options
 * which have global defaults.
 * Protect access to this struct with ldo_mutex
 * ldap_log.h:ldapoptions_prefix must match the head of this struct.
 */
struct ldapoptions {
  short ldo_valid;
#define LDAP_UNINITIALIZED 0x0
#define LDAP_INITIALIZED 0x1
#define LDAP_VALID_SESSION 0x2
#define LDAP_TRASHED_SESSION 0xFF
  int ldo_debug;

  ber_int_t ldo_version;
  ber_int_t ldo_deref;
  ber_int_t ldo_timelimit;
  ber_int_t ldo_sizelimit;

  /* per API call timeout */
  struct timeval ldo_tm_api;
  struct timeval ldo_tm_net;

  LDAPURLDesc *ldo_defludp;
  int ldo_defport;
  char *ldo_defbase;
  char *ldo_defbinddn; /* bind dn */

  /*
   * Per connection tcp-keepalive settings (Linux only,
   * ignored where unsupported)
   */
  ber_int_t ldo_keepalive_idle;
  ber_int_t ldo_keepalive_probes;
  ber_int_t ldo_keepalive_interval;

  int ldo_refhoplimit; /* limit on referral nesting */

  /* LDAPv3 server and client controls */
  LDAPControl **ldo_sctrls;
  LDAPControl **ldo_cctrls;

  /* LDAP rebind callback function */
  LDAP_REBIND_PROC *ldo_rebind_proc;
  void *ldo_rebind_params;
  LDAP_NEXTREF_PROC *ldo_nextref_proc;
  void *ldo_nextref_params;
  LDAP_URLLIST_PROC *ldo_urllist_proc;
  void *ldo_urllist_params;

  /* LDAP connection callback stack */
  ldaplist *ldo_conn_cbs;

  LDAP_BOOLEANS ldo_booleans; /* boolean options */

#define LDAP_LDO_NULLARG , 0, 0, 0, 0, {0}, {0}, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

#ifdef LDAP_CONNECTIONLESS
#define LDAP_IS_UDP(ld) ((ld)->ld_options.ldo_is_udp)
  void *ldo_peer; /* struct sockaddr* */
  char *ldo_cldapdn;
  int ldo_is_udp;
#define LDAP_LDO_CONNECTIONLESS_NULLARG , 0, 0, 0
#else
#define LDAP_LDO_CONNECTIONLESS_NULLARG
#endif

#ifdef WITH_TLS
  /* tls context */
  void *ldo_tls_ctx;
  LDAP_TLS_CONNECT_CB *ldo_tls_connect_cb;
  void *ldo_tls_connect_arg;
  struct ldaptls ldo_tls_info;
#define ldo_tls_certfile ldo_tls_info.lt_certfile
#define ldo_tls_keyfile ldo_tls_info.lt_keyfile
#define ldo_tls_dhfile ldo_tls_info.lt_dhfile
#define ldo_tls_ecname ldo_tls_info.lt_ecname
#define ldo_tls_cacertfile ldo_tls_info.lt_cacertfile
#define ldo_tls_cacertdir ldo_tls_info.lt_cacertdir
#define ldo_tls_ciphersuite ldo_tls_info.lt_ciphersuite
#define ldo_tls_protocol_min ldo_tls_info.lt_protocol_min
#define ldo_tls_crlfile ldo_tls_info.lt_crlfile
#define ldo_tls_randfile ldo_tls_info.lt_randfile
#define ldo_tls_cacert ldo_tls_info.lt_cacert
#define ldo_tls_cert ldo_tls_info.lt_cert
#define ldo_tls_key ldo_tls_info.lt_key
  int ldo_tls_mode;
  int ldo_tls_require_cert;
  int ldo_tls_impl;
  int ldo_tls_crlcheck;
  char *ldo_tls_pin_hashalg;
  struct berval ldo_tls_pin;
#define LDAP_LDO_TLS_NULLARG , 0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0}, 0, 0, 0, 0, 0, {0, 0}
#else
#define LDAP_LDO_TLS_NULLARG
#endif

#ifdef HAVE_CYRUS_SASL
  char *ldo_def_sasl_mech;    /* SASL Mechanism(s) */
  char *ldo_def_sasl_realm;   /* SASL realm */
  char *ldo_def_sasl_authcid; /* SASL authentication identity */
  char *ldo_def_sasl_authzid; /* SASL authorization identity */

  /* SASL Security Properties */
  struct sasl_security_properties ldo_sasl_secprops;
#define LDAP_LDO_SASL_NULLARG , 0, 0, 0, 0, {0}
#else
#define LDAP_LDO_SASL_NULLARG
#endif

#ifdef HAVE_GSSAPI
  unsigned ldo_gssapi_flags;
#define LDAP_GSSAPI_OPT_DO_NOT_FREE_GSS_CONTEXT 0x0001
#define LDAP_GSSAPI_OPT_ALLOW_REMOTE_PRINCIPAL 0x0002
  unsigned ldo_gssapi_options;
#define LDAP_LDO_GSSAPI_NULLARG , 0, 0
#else
#define LDAP_LDO_GSSAPI_NULLARG
#endif

#ifdef LDAP_R_COMPILE
  ldap_pvt_thread_mutex_t ldo_mutex;
#define LDAP_LDO_MUTEX_NULLARG , LDAP_PVT_MUTEX_NULL
#else
#define LDAP_LDO_MUTEX_NULLARG
#endif
};

/*
 * structure for representing an LDAP server connection
 */
typedef struct ldap_conn {
  Sockbuf *lconn_sb;
#ifdef HAVE_CYRUS_SASL
  void *lconn_sasl_authctx; /* context for bind */
  void *lconn_sasl_sockctx; /* for security layer */
  void *lconn_sasl_cbind;   /* for channel binding */
#endif
#ifdef HAVE_GSSAPI
  void *lconn_gss_ctx; /* gss_ctx_id_t */
#endif
  int lconn_refcnt;
  slap_time_t lconn_created;   /* time */
  slap_time_t lconn_lastused;  /* time */
  int lconn_rebind_inprogress; /* set if rebind in progress */
  char ***lconn_rebind_queue;  /* used if rebind in progress */
  int lconn_status;
#define LDAP_CONNST_NEEDSOCKET 1
#define LDAP_CONNST_CONNECTING 2
#define LDAP_CONNST_CONNECTED 3
  LDAPURLDesc *lconn_server;
  BerElement *lconn_ber; /* ber receiving on this conn. */

  struct ldap_conn *lconn_next;
} LDAPConn;

/*
 * structure used to track outstanding requests
 */
typedef struct ldapreq {
  ber_int_t lr_msgid; /* the message id */
  int lr_status;      /* status of request */
#define LDAP_REQST_COMPLETED 0
#define LDAP_REQST_INPROGRESS 1
#define LDAP_REQST_CHASINGREFS 2
#define LDAP_REQST_NOTCONNECTED 3
#define LDAP_REQST_WRITING 4
  int lr_refcnt;              /* count of references */
  int lr_outrefcnt;           /* count of outstanding referrals */
  int lr_abandoned;           /* the request has been abandoned */
  ber_int_t lr_origid;        /* original request's message id */
  int lr_parentcnt;           /* count of parent requests */
  ber_tag_t lr_res_msgtype;   /* result message type */
  ber_int_t lr_res_errno;     /* result LDAP errno */
  char *lr_res_error;         /* result error string */
  char *lr_res_matched;       /* result matched DN string */
  BerElement *lr_ber;         /* ber encoded request contents */
  LDAPConn *lr_conn;          /* connection used to send request */
  struct berval lr_dn;        /* DN of request, in lr_ber */
  struct ldapreq *lr_parent;  /* request that spawned this referral */
  struct ldapreq *lr_child;   /* first child request */
  struct ldapreq *lr_refnext; /* next referral spawned */
  struct ldapreq *lr_prev;    /* previous request */
  struct ldapreq *lr_next;    /* next request */
} LDAPRequest;

/*
 * structure for client cache
 */
#define LDAP_CACHE_BUCKETS 31 /* cache hash table size */
typedef struct ldapcache {
  LDAPMessage *lc_buckets[LDAP_CACHE_BUCKETS]; /* hash table */
  LDAPMessage *lc_requests;                    /* unfulfilled reqs */
  long lc_timeout;                             /* request timeout */
  ber_len_t lc_maxmem;                         /* memory to use */
  ber_len_t lc_memused;                        /* memory in use */
  int lc_enabled;                              /* enabled? */
  unsigned long lc_options;                    /* options */
#define LDAP_CACHE_OPT_CACHENOERRS 0x00000001
#define LDAP_CACHE_OPT_CACHEALLERRS 0x00000002
} LDAPCache;

/*
 * structure containing referral request info for rebind procedure
 */
typedef struct ldapreqinfo {
  ber_len_t ri_msgid;
  int ri_request;
  char *ri_url;
} LDAPreqinfo;

/*
 * structure representing an ldap connection
 */

struct ldap_common {
  Sockbuf *ldc_sb; /* socket descriptor & buffer */
#define ld_sb ldc->ldc_sb

  unsigned short ldc_lberoptions;
#define ld_lberoptions ldc->ldc_lberoptions

  /* protected by msgid_mutex */
  ber_len_t ldc_msgid;
#define ld_msgid ldc->ldc_msgid

  /* do not mess with these */
  /* protected by req_mutex */
  LDAPRequest *ldc_requests; /* list of outstanding requests */
  /* protected by res_mutex */
  LDAPMessage *ldc_responses; /* list of outstanding responses */
#define ld_requests ldc->ldc_requests
#define ld_responses ldc->ldc_responses

  /* protected by abandon_mutex */
  ber_len_t ldc_nabandoned;
  ber_int_t *ldc_abandoned; /* array of abandoned requests */
#define ld_nabandoned ldc->ldc_nabandoned
#define ld_abandoned ldc->ldc_abandoned

  /* unused by libreldap */
  LDAPCache *ldc_cache; /* non-null if cache is initialized */
#define ld_cache ldc->ldc_cache

  /* do not mess with the rest though */

  /* protected by conn_mutex */
  LDAPConn *ldc_defconn; /* default connection */
#define ld_defconn ldc->ldc_defconn
  LDAPConn *ldc_conns; /* list of server connections */
#define ld_conns ldc->ldc_conns
  void *ldc_selectinfo; /* platform specifics for select */
#define ld_selectinfo ldc->ldc_selectinfo

  /* ldap_common refcnt - free only if 0 */
  /* protected by ldc_mutex */
  unsigned int ldc_refcnt;
#define ld_ldcrefcnt ldc->ldc_refcnt

  /* protected by ldo_mutex */
  struct ldapoptions ldc_options;
#define ld_options ldc->ldc_options

#define ld_valid ld_options.ldo_valid
#define ld_debug ld_options.ldo_debug

#define ld_deref ld_options.ldo_deref
#define ld_timelimit ld_options.ldo_timelimit
#define ld_sizelimit ld_options.ldo_sizelimit

#define ld_defbinddn ld_options.ldo_defbinddn
#define ld_defbase ld_options.ldo_defbase
#define ld_defhost ld_options.ldo_defhost
#define ld_defport ld_options.ldo_defport

#define ld_refhoplimit ld_options.ldo_refhoplimit

#define ld_sctrls ld_options.ldo_sctrls
#define ld_cctrls ld_options.ldo_cctrls
#define ld_rebind_proc ld_options.ldo_rebind_proc
#define ld_rebind_params ld_options.ldo_rebind_params
#define ld_nextref_proc ld_options.ldo_nextref_proc
#define ld_nextref_params ld_options.ldo_nextref_params
#define ld_urllist_proc ld_options.ldo_urllist_proc
#define ld_urllist_params ld_options.ldo_urllist_params

#define ld_version ld_options.ldo_version

#ifdef LDAP_R_COMPILE
  ldap_pvt_thread_mutex_t ldc_mutex;
  ldap_pvt_thread_mutex_t ldc_msgid_mutex;
  ldap_pvt_thread_mutex_t ldc_conn_mutex;
  ldap_pvt_thread_mutex_t ldc_req_mutex;
  ldap_pvt_thread_mutex_t ldc_res_mutex;
  ldap_pvt_thread_mutex_t ldc_abandon_mutex;
#define ld_ldopts_mutex ld_options.ldo_mutex
#define ld_ldcmutex ldc->ldc_mutex
#define ld_msgid_mutex ldc->ldc_msgid_mutex
#define ld_conn_mutex ldc->ldc_conn_mutex
#define ld_req_mutex ldc->ldc_req_mutex
#define ld_res_mutex ldc->ldc_res_mutex
#define ld_abandon_mutex ldc->ldc_abandon_mutex
#endif
};

struct ldap {
  /* thread shared */
  struct ldap_common *ldc;

  /* thread specific */
  ber_int_t ld_errno;
  char *ld_error;
  char *ld_matched;
  char **ld_referrals;

  ldap_gentle_shutdown_f ld_gentle_shutdown;
};

#define LDAP_VALID(ld) ((ld)->ld_valid == LDAP_VALID_SESSION)
#define LDAP_TRASHED(ld) ((ld)->ld_valid == LDAP_TRASHED_SESSION)
#define LDAP_TRASH(ld) ((ld)->ld_valid = LDAP_TRASHED_SESSION)

#ifdef LDAP_R_COMPILE
LDAP_V(ldap_pvt_thread_mutex_t) ldap_int_resolv_mutex;
LDAP_V(ldap_pvt_thread_mutex_t) ldap_int_hostname_mutex;
LDAP_V(int) ldap_int_stackguard;

#ifdef HAVE_GSSAPI
LDAP_V(ldap_pvt_thread_mutex_t) ldap_int_gssapi_mutex;
#endif
#endif

#ifdef LDAP_R_COMPILE
#define LDAP_MUTEX_LOCK(mutex) ldap_pvt_thread_mutex_lock(mutex)
#define LDAP_MUTEX_UNLOCK(mutex) ldap_pvt_thread_mutex_unlock(mutex)
#define LDAP_ASSERT_MUTEX_OWNER(mutex) LDAP_PVT_THREAD_ASSERT_MUTEX_OWNER(mutex)
#else
#define LDAP_MUTEX_LOCK(mutex) ((void)0)
#define LDAP_MUTEX_UNLOCK(mutex) ((void)0)
#define LDAP_ASSERT_MUTEX_OWNER(mutex) ((void)0)
#endif

#define LDAP_NEXT_MSGID(ld, id)                                                                                        \
  do {                                                                                                                 \
    LDAP_MUTEX_LOCK(&(ld)->ld_msgid_mutex);                                                                            \
    (id) = ++(ld)->ld_msgid;                                                                                           \
    LDAP_MUTEX_UNLOCK(&(ld)->ld_msgid_mutex);                                                                          \
  } while (0)

/*
 * in abandon.c
 */

LDAP_F(int)
ldap_int_bisect_find(ber_int_t *v, ber_len_t n, ber_int_t id, int *idxp);
LDAP_F(int)
ldap_int_bisect_insert(ber_int_t **vp, ber_len_t *np, int id, int idx);
LDAP_F(int)
ldap_int_bisect_delete(ber_int_t **vp, ber_len_t *np, int id, int idx);

/*
 * in add.c
 */

LDAP_F(BerElement *)
ldap_build_add_req(LDAP *ld, const char *dn, LDAPMod **attrs, LDAPControl **sctrls, LDAPControl **cctrls,
                   ber_int_t *msgidp);

/*
 * in lbase64.c
 */

LDAP_F(int) ldap_int_decode_b64_inplace(struct berval *value);

/*
 * in compare.c
 */

LDAP_F(BerElement *)
ldap_build_compare_req(LDAP *ld, const char *dn, const char *attr, struct berval *bvalue, LDAPControl **sctrls,
                       LDAPControl **cctrls, ber_int_t *msgidp);

/*
 * in delete.c
 */

LDAP_F(BerElement *)
ldap_build_delete_req(LDAP *ld, const char *dn, LDAPControl **sctrls, LDAPControl **cctrls, ber_int_t *msgidp);

/*
 * in extended.c
 */

LDAP_F(BerElement *)
ldap_build_extended_req(LDAP *ld, const char *reqoid, struct berval *reqdata, LDAPControl **sctrls,
                        LDAPControl **cctrls, ber_int_t *msgidp);

/*
 * in init.c
 */

LDAP_V(struct ldapoptions) ldap_int_global_options;
#define LDAP_INT_GLOBAL_OPT() (&ldap_int_global_options)

LDAP_F(void) ldap_int_initialize(struct ldapoptions *, int *);
LDAP_F(void) ldap_int_initialize_global_options(struct ldapoptions *, int *);

/* memory.c */
/* simple macros to realloc for now */
#define LDAP_MALLOC(s) (ber_memalloc_x((s), NULL))
#define LDAP_CALLOC(n, s) (ber_memcalloc_x((n), (s), NULL))
#define LDAP_REALLOC(p, s) (ber_memrealloc_x((p), (s), NULL))
#define LDAP_FREE(p) (ber_memfree_x((p), NULL))
#define LDAP_VFREE(v) (ber_memvfree_x((void **)(v), NULL))
#define LDAP_STRDUP(s) (ber_strdup_x((s), NULL))
#define LDAP_STRNDUP(s, l) (ber_strndup_x((s), (l), NULL))

#define LDAP_MALLOCX(s, x) (ber_memalloc_x((s), (x)))
#define LDAP_CALLOCX(n, s, x) (ber_memcalloc_x((n), (s), (x)))
#define LDAP_REALLOCX(p, s, x) (ber_memrealloc_x((p), (s), (x)))
#define LDAP_FREEX(p, x) (ber_memfree_x((p), (x)))
#define LDAP_VFREEX(v, x) (ber_memvfree_x((void **)(v), (x)))
#define LDAP_STRDUPX(s, x) (ber_strdup_x((s), (x)))
#define LDAP_STRNDUPX(s, l, x) (ber_strndup_x((s), (l), (x)))

/*
 * in error.c
 */
LDAP_F(void) ldap_int_error_init(void);

/*
 * in modify.c
 */

LDAP_F(BerElement *)
ldap_build_modify_req(LDAP *ld, const char *dn, LDAPMod **mods, LDAPControl **sctrls, LDAPControl **cctrls,
                      ber_int_t *msgidp);

/*
 * in modrdn.c
 */

LDAP_F(BerElement *)
ldap_build_moddn_req(LDAP *ld, const char *dn, const char *newrdn, const char *newSuperior, int deleteoldrdn,
                     LDAPControl **sctrls, LDAPControl **cctrls, ber_int_t *msgidp);

/*
 * in unit-int.c
 */
LDAP_F(void) ldap_int_utils_init(void);

/*
 * in cache.c
 */
LDAP_F(void)
ldap_add_request_to_cache(LDAP *ld, ber_tag_t msgtype, BerElement *request);
LDAP_F(void) ldap_add_result_to_cache(LDAP *ld, LDAPMessage *result);
LDAP_F(int) ldap_check_cache(LDAP *ld, ber_tag_t msgtype, BerElement *request);

/*
 * in controls.c
 */
LDAP_F(int)
ldap_int_put_controls(LDAP *ld, LDAPControl *const *ctrls, BerElement *ber);

LDAP_F(int) ldap_int_client_controls(LDAP *ld, LDAPControl **ctrlp);

/*
 * in dsparse.c
 */
LDAP_F(int)
ldap_int_next_line_tokens(char **bufp, ber_len_t *blenp, char ***toksp);

/*
 * in open.c
 */
LDAP_F(int) ldap_open_defconn(LDAP *ld);
LDAP_F(int)
ldap_int_open_connection(LDAP *ld, LDAPConn *conn, LDAPURLDesc *srvlist, int async);
LDAP_F(int) ldap_int_check_async_open(LDAP *ld, ber_socket_t sd);

/*
 * in os-ip.c
 */
#ifndef HAVE_POLL
LDAP_V(int) ldap_int_tblsize;
LDAP_F(void) ldap_int_ip_init(void);
#endif

LDAP_F(int)
ldap_int_timeval_dup(struct timeval **dest, const struct timeval *tm);
LDAP_F(int)
ldap_connect_to_host(LDAP *ld, Sockbuf *sb, int proto, LDAPURLDesc *srv, int async);
LDAP_F(int)
ldap_int_poll(LDAP *ld, ber_socket_t s, struct timeval *tvp, int wr);

#if defined(WITH_TLS) || defined(HAVE_CYRUS_SASL)
LDAP_V(char *) ldap_int_hostname;
LDAP_F(char *) ldap_host_connected_to(Sockbuf *sb, const char *host);
#endif

LDAP_F(int) ldap_int_select(LDAP *ld, struct timeval *timeout);
LDAP_F(void *) ldap_new_select_info(void);
LDAP_F(void) ldap_free_select_info(void *sip);
LDAP_F(void) ldap_mark_select_write(LDAP *ld, Sockbuf *sb);
LDAP_F(void) ldap_mark_select_read(LDAP *ld, Sockbuf *sb);
LDAP_F(void) ldap_mark_select_clear(LDAP *ld, Sockbuf *sb);
LDAP_F(void) ldap_clear_select_write(LDAP *ld, Sockbuf *sb);
LDAP_F(int) ldap_is_read_ready(LDAP *ld, Sockbuf *sb);
LDAP_F(int) ldap_is_write_ready(LDAP *ld, Sockbuf *sb);

LDAP_F(int)
ldap_int_connect_cbs(LDAP *ld, Sockbuf *sb, ber_socket_t *s, LDAPURLDesc *srv, struct sockaddr *addr);

LDAP_F(int)
ldap_pvt_tcpkeepalive(int fd, ber_int_t idle, ber_int_t probes, ber_int_t interval);

/*
 * in os-local.c
 */
#ifdef LDAP_PF_LOCAL
LDAP_F(int)
ldap_connect_to_path(LDAP *ld, Sockbuf *sb, LDAPURLDesc *srv, int async);
#endif /* LDAP_PF_LOCAL */

/*
 * in request.c
 */
LDAP_F(ber_int_t)
ldap_send_initial_request(LDAP *ld, ber_tag_t msgtype, const char *dn, BerElement *ber, ber_int_t msgid);
LDAP_F(BerElement *) ldap_alloc_ber_with_options(LDAP *ld);
LDAP_F(void) ldap_set_ber_options(LDAP *ld, BerElement *ber);

LDAP_F(int)
ldap_send_server_request(LDAP *ld, BerElement *ber, ber_int_t msgid, LDAPRequest *parentreq, LDAPURLDesc **srvlist,
                         LDAPConn *lc, LDAPreqinfo *bind, int noconn, int m_res);
LDAP_F(LDAPConn *)
ldap_new_connection(LDAP *ld, LDAPURLDesc **srvlist, int use_ldsb, int connect, LDAPreqinfo *bind, int m_req,
                    int m_res);
LDAP_F(LDAPRequest *) ldap_find_request_by_msgid(LDAP *ld, ber_int_t msgid);
LDAP_F(void) ldap_return_request(LDAP *ld, LDAPRequest *lr, int freeit);
LDAP_F(void) ldap_free_request(LDAP *ld, LDAPRequest *lr);
LDAP_F(void)
ldap_free_connection(LDAP *ld, LDAPConn *lc, int force, int unbind);
LDAP_F(void) ldap_dump_connection(LDAP *ld, LDAPConn *lconns, int all);
LDAP_F(void) ldap_dump_requests_and_responses(LDAP *ld);
LDAP_F(int)
ldap_chase_referrals(LDAP *ld, LDAPRequest *lr, char **errstrp, int sref, int *hadrefp);
LDAP_F(int)
ldap_chase_v3referrals(LDAP *ld, LDAPRequest *lr, char **refs, int sref, char **referralsp, int *hadrefp);
LDAP_F(int) ldap_append_referral(LDAP *ld, char **referralsp, char *s);
LDAP_F(int) ldap_int_flush_request(LDAP *ld, LDAPRequest *lr);

/*
 * in result.c:
 */
LDAP_F(const char *) ldap_int_msgtype2str(ber_tag_t tag);

/*
 * in search.c
 */
LDAP_F(BerElement *)
ldap_build_search_req(LDAP *ld, const char *base, ber_int_t scope, const char *filter, char **attrs,
                      ber_int_t attrsonly, LDAPControl **sctrls, LDAPControl **cctrls, ber_int_t timelimit,
                      ber_int_t sizelimit, ber_int_t deref, ber_int_t *msgidp);

/*
 * in unbind.c
 */
LDAP_F(int)
ldap_ld_free(LDAP *ld, int close, LDAPControl **sctrls, LDAPControl **cctrls);

LDAP_F(int)
ldap_send_unbind(LDAP *ld, Sockbuf *sb, LDAPControl **sctrls, LDAPControl **cctrls);

/*
 * in url.c
 */
LDAP_F(LDAPURLDesc *) ldap_url_dup(LDAPURLDesc *ludp);

LDAP_F(LDAPURLDesc *) ldap_url_duplist(LDAPURLDesc *ludlist);

LDAP_F(int)
ldap_url_parsehosts(LDAPURLDesc **ludlist, const char *hosts, int port);

LDAP_F(char *) ldap_url_list2hosts(LDAPURLDesc *ludlist);

/*
 * in cyrus.c
 */

LDAP_F(int) ldap_int_sasl_init(void);

LDAP_F(int) ldap_int_sasl_open(LDAP *ld, LDAPConn *conn, const char *host);
LDAP_F(int) ldap_int_sasl_close(LDAP *ld, LDAPConn *conn);

LDAP_F(int)
ldap_int_sasl_external(LDAP *ld, LDAPConn *conn, const char *authid, ber_len_t ssf);

LDAP_F(int) ldap_int_sasl_get_option(LDAP *ld, int option, void *arg);
LDAP_F(int) ldap_int_sasl_set_option(LDAP *ld, int option, void *arg);
LDAP_F(int)
ldap_int_sasl_config(struct ldapoptions *lo, int option, const char *arg);

LDAP_F(int)
ldap_int_sasl_bind(LDAP *ld, const char *, const char *, LDAPControl **, LDAPControl **,

                   /* should be passed in client controls */
                   unsigned flags, LDAP_SASL_INTERACT_PROC *interact, void *defaults, LDAPMessage *result,
                   const char **rmech, int *msgid);

/* in sasl.c */

LDAP_F(BerElement *)
ldap_build_bind_req(LDAP *ld, const char *dn, const char *mech, struct berval *cred, LDAPControl **sctrls,
                    LDAPControl **cctrls, ber_int_t *msgidp);

/* in schema.c */
LDAP_F(char *)
ldap_int_parse_numericoid(const char **sp, int *code, const int flags);

/*
 * in tls.c
 */
LDAP_F(int) ldap_int_tls_start(LDAP *ld, LDAPConn *conn, LDAPURLDesc *srv);

LDAP_F(void) ldap_int_tls_destroy(struct ldapoptions *lo);

/*
 *	in getvalues.c
 */
LDAP_F(char **) ldap_value_dup(char *const *vals);

/*
 *	in gssapi.c
 */
#ifdef HAVE_GSSAPI
LDAP_F(int) ldap_int_gssapi_get_option(LDAP *ld, int option, void *arg);
LDAP_F(int) ldap_int_gssapi_set_option(LDAP *ld, int option, void *arg);
LDAP_F(int)
ldap_int_gssapi_config(struct ldapoptions *lo, int option, const char *arg);
LDAP_F(void) ldap_int_gssapi_close(LDAP *ld, LDAPConn *lc);
#endif

LDAP_END_DECL

#endif /* _LDAP_INT_H */
