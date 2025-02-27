/* $ReOpenLDAP$ */
/* Copyright 2016-2018 ReOpenLDAP AUTHORS: please see AUTHORS file.
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
 * This work was developed by Symas Corporation
 * based on back-meta module for inclusion in OpenLDAP Software.
 * This work was sponsored by Ericsson. */

/* This is an altered version */
/*
 * Copyright 1999, Howard Chu, All rights reserved. <hyc@highlandsun.com>
 *
 * Permission is granted to anyone to use this software for any purpose
 * on any computer system, and to alter it and redistribute it, subject
 * to the following restrictions:
 *
 * 1. The author is not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits should appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits should appear in the documentation.
 *
 * 4. This notice may not be removed or altered.
 *
 *
 *
 * Copyright 2016, Symas Corporation
 *
 * This is based on the back-meta/map.c version by Pierangelo Masarati.
 * The previously reported conditions apply to the modified code as well.
 * Changes in the original code are highlighted where required.
 * Credits for the original code go to the author, Howard Chu.
 */

#include "reldap.h"

#include <stdio.h>
#include <ac/string.h>
#include <ac/socket.h>

#include "slap.h"
#include "lutil.h"
#include "../back-ldap/back-ldap.h"
#include "back-asyncmeta.h"

int asyncmeta_mapping_cmp(const void *c1, const void *c2) {
  struct ldapmapping *map1 = (struct ldapmapping *)c1;
  struct ldapmapping *map2 = (struct ldapmapping *)c2;
  int rc = map1->src.bv_len - map2->src.bv_len;
  if (rc)
    return rc;
  return (strcasecmp(map1->src.bv_val, map2->src.bv_val));
}

int asyncmeta_mapping_dup(void *c1, void *c2) {
  struct ldapmapping *map1 = (struct ldapmapping *)c1;
  struct ldapmapping *map2 = (struct ldapmapping *)c2;

  return ((strcasecmp(map1->src.bv_val, map2->src.bv_val) == 0) ? -1 : 0);
}

void asyncmeta_map_init(struct ldapmap *lm, struct ldapmapping **m) {
  struct ldapmapping *mapping;

  assert(m != NULL);

  *m = NULL;

  mapping = (struct ldapmapping *)ch_calloc(2, sizeof(struct ldapmapping));
  if (mapping == NULL) {
    return;
  }

  ber_str2bv("objectclass", STRLENOF("objectclass"), 1, &mapping[0].src);
  ber_dupbv(&mapping[0].dst, &mapping[0].src);
  mapping[1].src = mapping[0].src;
  mapping[1].dst = mapping[0].dst;

  avl_insert(&lm->map, (caddr_t)&mapping[0], asyncmeta_mapping_cmp, asyncmeta_mapping_dup);
  avl_insert(&lm->remap, (caddr_t)&mapping[1], asyncmeta_mapping_cmp, asyncmeta_mapping_dup);
  *m = mapping;
}

int asyncmeta_mapping(struct ldapmap *map, struct berval *s, struct ldapmapping **m, int remap) {
  Avlnode *tree;
  struct ldapmapping fmapping;

  assert(m != NULL);

  /* let special attrnames slip through (ITS#5760) */
  if (bvmatch(s, slap_bv_no_attrs) || bvmatch(s, slap_bv_all_user_attrs) || bvmatch(s, slap_bv_all_operational_attrs)) {
    *m = NULL;
    return 0;
  }

  if (remap == BACKLDAP_REMAP) {
    tree = map->remap;

  } else {
    tree = map->map;
  }

  fmapping.src = *s;
  *m = (struct ldapmapping *)avl_find(tree, (caddr_t)&fmapping, asyncmeta_mapping_cmp);
  if (*m == NULL) {
    return map->drop_missing;
  }

  return 0;
}

void asyncmeta_map(struct ldapmap *map, struct berval *s, struct berval *bv, int remap) {
  struct ldapmapping *mapping;
  int drop_missing;

  /* map->map may be NULL when mapping is configured,
   * but map->remap can't */
  if (map->remap == NULL) {
    *bv = *s;
    return;
  }

  BER_BVZERO(bv);
  drop_missing = asyncmeta_mapping(map, s, &mapping, remap);
  if (mapping != NULL) {
    if (!BER_BVISNULL(&mapping->dst)) {
      *bv = mapping->dst;
    }
    return;
  }

  if (!drop_missing) {
    *bv = *s;
  }
}

int asyncmeta_map_attrs(Operation *op, struct ldapmap *at_map, AttributeName *an, int remap, char ***mapped_attrs) {
  int i, x, j;
  char **na;
  struct berval mapped;

  if (an == NULL && op->o_bd->be_extra_anlist == NULL) {
    *mapped_attrs = NULL;
    return LDAP_SUCCESS;
  }

  i = 0;
  if (an != NULL) {
    for (; !BER_BVISNULL(&an[i].an_name); i++)
      /*  */;
  }

  x = 0;
  if (op->o_bd->be_extra_anlist != NULL) {
    for (; !BER_BVISNULL(&op->o_bd->be_extra_anlist[x].an_name); x++)
      /*  */;
  }

  assert(i > 0 || x > 0);

  na = (char **)ber_memcalloc_x(i + x + 1, sizeof(char *), op->o_tmpmemctx);
  if (na == NULL) {
    *mapped_attrs = NULL;
    return LDAP_NO_MEMORY;
  }

  j = 0;
  if (i > 0) {
    for (i = 0; !BER_BVISNULL(&an[i].an_name); i++) {
      asyncmeta_map(at_map, &an[i].an_name, &mapped, remap);
      if (!BER_BVISNULL(&mapped) && !BER_BVISEMPTY(&mapped)) {
        na[j++] = mapped.bv_val;
      }
    }
  }

  if (x > 0) {
    for (x = 0; !BER_BVISNULL(&op->o_bd->be_extra_anlist[x].an_name); x++) {
      if (op->o_bd->be_extra_anlist[x].an_desc && ad_inlist(op->o_bd->be_extra_anlist[x].an_desc, an)) {
        continue;
      }

      asyncmeta_map(at_map, &op->o_bd->be_extra_anlist[x].an_name, &mapped, remap);
      if (!BER_BVISNULL(&mapped) && !BER_BVISEMPTY(&mapped)) {
        na[j++] = mapped.bv_val;
      }
    }
  }

  if (j == 0 && (i > 0 || x > 0)) {
    na[j++] = LDAP_NO_ATTRS;
  }
  na[j] = NULL;

  *mapped_attrs = na;

  return LDAP_SUCCESS;
}

static int map_attr_value(a_dncookie *dc, AttributeDescription *ad, struct berval *mapped_attr, struct berval *value,
                          struct berval *mapped_value, int remap, void *memctx) {
  struct berval vtmp;
  int freeval = 0;

  asyncmeta_map(&dc->target->mt_rwmap.rwm_at, &ad->ad_cname, mapped_attr, remap);
  if (BER_BVISNULL(mapped_attr) || BER_BVISEMPTY(mapped_attr)) {
#if 0
		/*
		 * FIXME: are we sure we need to search oc_map if at_map fails?
		 */
		asyncmeta_map( &dc->target->mt_rwmap.rwm_oc, &ad->ad_cname, mapped_attr, remap );
		if ( BER_BVISNULL( mapped_attr ) || BER_BVISEMPTY( mapped_attr ) ) {
			*mapped_attr = ad->ad_cname;
		}
#endif
    if (dc->target->mt_rwmap.rwm_at.drop_missing) {
      return -1;
    }

    *mapped_attr = ad->ad_cname;
  }

  if (value == NULL) {
    return 0;
  }

  if (ad->ad_type->sat_syntax == slap_schema.si_syn_distinguishedName) {
    a_dncookie fdc = *dc;

    fdc.ctx = "searchFilterAttrDN";

    switch (asyncmeta_dn_massage(&fdc, value, &vtmp)) {
    case LDAP_SUCCESS:
      if (vtmp.bv_val != value->bv_val) {
        freeval = 1;
      }
      break;

    case LDAP_UNWILLING_TO_PERFORM:
      return -1;

    case LDAP_OTHER:
      return -1;
    }

  } else if (ad->ad_type->sat_equality && ad->ad_type->sat_equality->smr_usage & SLAP_MR_MUTATION_NORMALIZER) {
    if (ad->ad_type->sat_equality->smr_normalize((SLAP_MR_DENORMALIZE | SLAP_MR_VALUE_OF_ASSERTION_SYNTAX), NULL, NULL,
                                                 value, &vtmp, memctx)) {
      return -1;
    }
    freeval = 2;

  } else if (ad == slap_schema.si_ad_objectClass || ad == slap_schema.si_ad_structuralObjectClass) {
    asyncmeta_map(&dc->target->mt_rwmap.rwm_oc, value, &vtmp, remap);
    if (BER_BVISNULL(&vtmp) || BER_BVISEMPTY(&vtmp)) {
      vtmp = *value;
    }

  } else {
    vtmp = *value;
  }

  filter_escape_value_x(&vtmp, mapped_value, memctx);

  switch (freeval) {
  case 1:
    ber_memfree(vtmp.bv_val);
    break;
  case 2:
    ber_memfree_x(vtmp.bv_val, memctx);
    break;
  }

  return 0;
}

static int asyncmeta_int_filter_map_rewrite(a_dncookie *dc, Filter *f, struct berval *fstr, int remap, void *memctx) {
  int i;
  Filter *p;
  struct berval atmp, vtmp, *tmp;
  static struct berval
      /* better than nothing... */
      ber_bvfalse = BER_BVC("(!(objectClass=*))"),
      ber_bvtf_false = BER_BVC("(|)"),
      /* better than nothing... */
      ber_bvtrue = BER_BVC("(objectClass=*)"), ber_bvtf_true = BER_BVC("(&)"),
#if 0
			/* no longer needed; preserved for completeness */
			ber_bvundefined = BER_BVC( "(?=undefined)" ),
#endif
      ber_bverror = BER_BVC("(?=error)"), ber_bvunknown = BER_BVC("(?=unknown)"), ber_bvnone = BER_BVC("(?=none)");
  ber_len_t len;

  assert(fstr != NULL);
  BER_BVZERO(fstr);

  if (f == NULL) {
    ber_dupbv_x(fstr, &ber_bvnone, memctx);
    return LDAP_OTHER;
  }

  switch ((f->f_choice & SLAPD_FILTER_MASK)) {
  case LDAP_FILTER_EQUALITY:
    if (map_attr_value(dc, f->f_av_desc, &atmp, &f->f_av_value, &vtmp, remap, memctx)) {
      goto computed;
    }

    fstr->bv_len = atmp.bv_len + vtmp.bv_len + (sizeof("(=)") - 1);
    fstr->bv_val = ber_memalloc_x(fstr->bv_len + 1, memctx);

    snprintf(fstr->bv_val, fstr->bv_len + 1, "(%s=%s)", atmp.bv_val, vtmp.bv_len ? vtmp.bv_val : "");

    ber_memfree_x(vtmp.bv_val, memctx);
    break;

  case LDAP_FILTER_GE:
    if (map_attr_value(dc, f->f_av_desc, &atmp, &f->f_av_value, &vtmp, remap, memctx)) {
      goto computed;
    }

    fstr->bv_len = atmp.bv_len + vtmp.bv_len + (sizeof("(>=)") - 1);
    fstr->bv_val = ber_memalloc_x(fstr->bv_len + 1, memctx);

    snprintf(fstr->bv_val, fstr->bv_len + 1, "(%s>=%s)", atmp.bv_val, vtmp.bv_len ? vtmp.bv_val : "");

    ber_memfree_x(vtmp.bv_val, memctx);
    break;

  case LDAP_FILTER_LE:
    if (map_attr_value(dc, f->f_av_desc, &atmp, &f->f_av_value, &vtmp, remap, memctx)) {
      goto computed;
    }

    fstr->bv_len = atmp.bv_len + vtmp.bv_len + (sizeof("(<=)") - 1);
    fstr->bv_val = ber_memalloc_x(fstr->bv_len + 1, memctx);

    snprintf(fstr->bv_val, fstr->bv_len + 1, "(%s<=%s)", atmp.bv_val, vtmp.bv_len ? vtmp.bv_val : "");

    ber_memfree_x(vtmp.bv_val, memctx);
    break;

  case LDAP_FILTER_APPROX:
    if (map_attr_value(dc, f->f_av_desc, &atmp, &f->f_av_value, &vtmp, remap, memctx)) {
      goto computed;
    }

    fstr->bv_len = atmp.bv_len + vtmp.bv_len + (sizeof("(~=)") - 1);
    fstr->bv_val = ber_memalloc_x(fstr->bv_len + 1, memctx);

    snprintf(fstr->bv_val, fstr->bv_len + 1, "(%s~=%s)", atmp.bv_val, vtmp.bv_len ? vtmp.bv_val : "");

    ber_memfree_x(vtmp.bv_val, memctx);
    break;

  case LDAP_FILTER_SUBSTRINGS:
    if (map_attr_value(dc, f->f_sub_desc, &atmp, NULL, NULL, remap, memctx)) {
      goto computed;
    }

    /* cannot be a DN ... */

    fstr->bv_len = atmp.bv_len + (STRLENOF("(=*)"));
    fstr->bv_val = ber_memalloc_x(fstr->bv_len + 128, memctx); /* FIXME: why 128 ? */

    snprintf(fstr->bv_val, fstr->bv_len + 1, "(%s=*)", atmp.bv_val);

    if (!BER_BVISNULL(&f->f_sub_initial)) {
      len = fstr->bv_len;

      filter_escape_value_x(&f->f_sub_initial, &vtmp, memctx);

      fstr->bv_len += vtmp.bv_len;
      fstr->bv_val = ber_memrealloc_x(fstr->bv_val, fstr->bv_len + 1, memctx);

      snprintf(&fstr->bv_val[len - 2], vtmp.bv_len + 3,
               /* "(attr=" */ "%s*)", vtmp.bv_len ? vtmp.bv_val : "");

      ber_memfree_x(vtmp.bv_val, memctx);
    }

    if (f->f_sub_any != NULL) {
      for (i = 0; !BER_BVISNULL(&f->f_sub_any[i]); i++) {
        len = fstr->bv_len;
        filter_escape_value_x(&f->f_sub_any[i], &vtmp, memctx);

        fstr->bv_len += vtmp.bv_len + 1;
        fstr->bv_val = ber_memrealloc_x(fstr->bv_val, fstr->bv_len + 1, memctx);

        snprintf(&fstr->bv_val[len - 1], vtmp.bv_len + 3,
                 /* "(attr=[init]*[any*]" */ "%s*)", vtmp.bv_len ? vtmp.bv_val : "");
        ber_memfree_x(vtmp.bv_val, memctx);
      }
    }

    if (!BER_BVISNULL(&f->f_sub_final)) {
      len = fstr->bv_len;

      filter_escape_value_x(&f->f_sub_final, &vtmp, memctx);

      fstr->bv_len += vtmp.bv_len;
      fstr->bv_val = ber_memrealloc_x(fstr->bv_val, fstr->bv_len + 1, memctx);

      snprintf(&fstr->bv_val[len - 1], vtmp.bv_len + 3,
               /* "(attr=[init*][any*]" */ "%s)", vtmp.bv_len ? vtmp.bv_val : "");

      ber_memfree_x(vtmp.bv_val, memctx);
    }

    break;

  case LDAP_FILTER_PRESENT:
    if (map_attr_value(dc, f->f_desc, &atmp, NULL, NULL, remap, memctx)) {
      goto computed;
    }

    fstr->bv_len = atmp.bv_len + (STRLENOF("(=*)"));
    fstr->bv_val = ber_memalloc_x(fstr->bv_len + 1, memctx);

    snprintf(fstr->bv_val, fstr->bv_len + 1, "(%s=*)", atmp.bv_val);
    break;

  case LDAP_FILTER_AND:
  case LDAP_FILTER_OR:
  case LDAP_FILTER_NOT:
    fstr->bv_len = STRLENOF("(%)");
    fstr->bv_val = ber_memalloc_x(fstr->bv_len + 128, memctx); /* FIXME: why 128? */

    snprintf(fstr->bv_val, fstr->bv_len + 1, "(%c)",
             f->f_choice == LDAP_FILTER_AND  ? '&'
             : f->f_choice == LDAP_FILTER_OR ? '|'
                                             : '!');

    for (p = f->f_list; p != NULL; p = p->f_next) {
      int rc;

      len = fstr->bv_len;

      rc = asyncmeta_int_filter_map_rewrite(dc, p, &vtmp, remap, memctx);
      if (rc != LDAP_SUCCESS) {
        return rc;
      }

      fstr->bv_len += vtmp.bv_len;
      fstr->bv_val = ber_memrealloc_x(fstr->bv_val, fstr->bv_len + 1, memctx);

      snprintf(&fstr->bv_val[len - 1], vtmp.bv_len + 2,
               /*"("*/ "%s)", vtmp.bv_len ? vtmp.bv_val : "");

      ber_memfree_x(vtmp.bv_val, memctx);
    }

    break;

  case LDAP_FILTER_EXT:
    if (f->f_mr_desc) {
      if (map_attr_value(dc, f->f_mr_desc, &atmp, &f->f_mr_value, &vtmp, remap, memctx)) {
        goto computed;
      }

    } else {
      BER_BVSTR(&atmp, "");
      filter_escape_value_x(&f->f_mr_value, &vtmp, memctx);
    }

    /* FIXME: cleanup (less ?: operators...) */
    fstr->bv_len = atmp.bv_len + (f->f_mr_dnattrs ? STRLENOF(":dn") : 0) +
                   (!BER_BVISEMPTY(&f->f_mr_rule_text) ? f->f_mr_rule_text.bv_len + 1 : 0) + vtmp.bv_len +
                   (STRLENOF("(:=)"));
    fstr->bv_val = ber_memalloc_x(fstr->bv_len + 1, memctx);

    snprintf(fstr->bv_val, fstr->bv_len + 1, "(%s%s%s%s:=%s)", atmp.bv_val, f->f_mr_dnattrs ? ":dn" : "",
             !BER_BVISEMPTY(&f->f_mr_rule_text) ? ":" : "",
             !BER_BVISEMPTY(&f->f_mr_rule_text) ? f->f_mr_rule_text.bv_val : "", vtmp.bv_len ? vtmp.bv_val : "");
    ber_memfree_x(vtmp.bv_val, memctx);
    break;

  case SLAPD_FILTER_COMPUTED:
    switch (f->f_result) {
    /* FIXME: treat UNDEFINED as FALSE */
    case SLAPD_COMPARE_UNDEFINED:
    computed:;
      if (META_BACK_TGT_NOUNDEFFILTER(dc->target)) {
        return LDAP_COMPARE_FALSE;
      }
      /* fallthru */

    case LDAP_COMPARE_FALSE:
      if (META_BACK_TGT_T_F(dc->target)) {
        tmp = &ber_bvtf_false;
        break;
      }
      tmp = &ber_bvfalse;
      break;

    case LDAP_COMPARE_TRUE:
      if (META_BACK_TGT_T_F(dc->target)) {
        tmp = &ber_bvtf_true;
        break;
      }

      tmp = &ber_bvtrue;
      break;

    default:
      tmp = &ber_bverror;
      break;
    }

    ber_dupbv_x(fstr, tmp, memctx);
    break;

  default:
    ber_dupbv_x(fstr, &ber_bvunknown, memctx);
    break;
  }

  return 0;
}

int asyncmeta_filter_map_rewrite(a_dncookie *dc, Filter *f, struct berval *fstr, int remap, void *memctx) {
  int rc;
  a_dncookie fdc;
  struct berval ftmp;
  static char *dmy = "";

  rc = asyncmeta_int_filter_map_rewrite(dc, f, fstr, remap, memctx);

  if (rc != LDAP_SUCCESS) {
    return rc;
  }

  fdc = *dc;
  ftmp = *fstr;

  fdc.ctx = "searchFilter";

  switch (rewrite_session(fdc.target->mt_rwmap.rwm_rw, fdc.ctx, (!BER_BVISEMPTY(&ftmp) ? ftmp.bv_val : dmy), fdc.conn,
                          &fstr->bv_val)) {
  case REWRITE_REGEXEC_OK:
    if (!BER_BVISNULL(fstr)) {
      fstr->bv_len = strlen(fstr->bv_val);

    } else {
      *fstr = ftmp;
    }
    Debug(LDAP_DEBUG_ARGS, "[rw] %s: \"%s\" -> \"%s\"\n", fdc.ctx, BER_BVISNULL(&ftmp) ? "" : ftmp.bv_val,
          BER_BVISNULL(fstr) ? "" : fstr->bv_val);
    rc = LDAP_SUCCESS;
    break;

  case REWRITE_REGEXEC_UNWILLING:
    if (fdc.rs) {
      fdc.rs->sr_err = LDAP_UNWILLING_TO_PERFORM;
      fdc.rs->sr_text = "Operation not allowed";
    }
    rc = LDAP_UNWILLING_TO_PERFORM;
    break;

  case REWRITE_REGEXEC_ERR:
    if (fdc.rs) {
      fdc.rs->sr_err = LDAP_OTHER;
      fdc.rs->sr_text = "Rewrite error";
    }
    rc = LDAP_OTHER;
    break;
  }

  if (fstr->bv_val == dmy) {
    BER_BVZERO(fstr);

  } else if (fstr->bv_val != ftmp.bv_val) {
    /* NOTE: need to realloc mapped filter on slab
     * and free the original one, until librewrite
     * becomes slab-aware
     */
    ber_dupbv_x(&ftmp, fstr, memctx);
    ch_free(fstr->bv_val);
    *fstr = ftmp;
  }

  return rc;
}

int asyncmeta_referral_result_rewrite(a_dncookie *dc, BerVarray a_vals, void *memctx) {
  int i, last;

  assert(dc != NULL);
  assert(a_vals != NULL);

  for (last = 0; !BER_BVISNULL(&a_vals[last]); last++)
    ;
  last--;

  for (i = 0; !BER_BVISNULL(&a_vals[i]); i++) {
    struct berval dn, olddn = BER_BVNULL;
    int rc;
    LDAPURLDesc *ludp;

    rc = ldap_url_parse(a_vals[i].bv_val, &ludp);
    if (rc != LDAP_URL_SUCCESS) {
      /* leave attr untouched if massage failed */
      continue;
    }

    /* FIXME: URLs like "ldap:///dc=suffix" if passed
     * thru ldap_url_parse() and ldap_url_desc2str()
     * get rewritten as "ldap:///dc=suffix??base";
     * we don't want this to occur... */
    if (ludp->lud_scope == LDAP_SCOPE_BASE) {
      ludp->lud_scope = LDAP_SCOPE_DEFAULT;
    }

    ber_str2bv(ludp->lud_dn, 0, 0, &olddn);

    rc = asyncmeta_dn_massage(dc, &olddn, &dn);
    switch (rc) {
    case LDAP_UNWILLING_TO_PERFORM:
      /*
       * FIXME: need to check if it may be considered
       * legal to trim values when adding/modifying;
       * it should be when searching (e.g. ACLs).
       */
      ber_memfree(a_vals[i].bv_val);
      if (last > i) {
        a_vals[i] = a_vals[last];
      }
      BER_BVZERO(&a_vals[last]);
      last--;
      i--;
      break;

    default:
      /* leave attr untouched if massage failed */
      if (!BER_BVISNULL(&dn) && olddn.bv_val != dn.bv_val) {
        char *newurl;

        ludp->lud_dn = dn.bv_val;
        newurl = ldap_url_desc2str(ludp);
        free(dn.bv_val);
        if (newurl == NULL) {
          /* FIXME: leave attr untouched
           * even if ldap_url_desc2str failed...
           */
          break;
        }

        ber_memfree_x(a_vals[i].bv_val, memctx);
        ber_str2bv_x(newurl, 0, 1, &a_vals[i], memctx);
        ber_memfree(newurl);
        ludp->lud_dn = olddn.bv_val;
      }
      break;
    }

    ldap_free_urldesc(ludp);
  }

  return 0;
}

/*
 * I don't like this much, but we need two different
 * functions because different heap managers may be
 * in use in back-ldap/meta to reduce the amount of
 * calls to malloc routines, and some of the free()
 * routines may be macros with args
 */
int asyncmeta_dnattr_rewrite(a_dncookie *dc, BerVarray a_vals) {
  struct berval bv;
  int i, last;

  assert(a_vals != NULL);

  for (last = 0; !BER_BVISNULL(&a_vals[last]); last++)
    ;
  last--;

  for (i = 0; !BER_BVISNULL(&a_vals[i]); i++) {
    switch (asyncmeta_dn_massage(dc, &a_vals[i], &bv)) {
    case LDAP_UNWILLING_TO_PERFORM:
      /*
       * FIXME: need to check if it may be considered
       * legal to trim values when adding/modifying;
       * it should be when searching (e.g. ACLs).
       */
      ch_free(a_vals[i].bv_val);
      if (last > i) {
        a_vals[i] = a_vals[last];
      }
      BER_BVZERO(&a_vals[last]);
      last--;
      break;

    default:
      /* leave attr untouched if massage failed */
      if (!BER_BVISNULL(&bv) && bv.bv_val != a_vals[i].bv_val) {
        ch_free(a_vals[i].bv_val);
        a_vals[i] = bv;
      }
      break;
    }
  }

  return 0;
}

int asyncmeta_dnattr_result_rewrite(a_dncookie *dc, BerVarray a_vals) {
  struct berval bv;
  int i, last;

  assert(a_vals != NULL);

  for (last = 0; !BER_BVISNULL(&a_vals[last]); last++)
    ;
  last--;

  for (i = 0; !BER_BVISNULL(&a_vals[i]); i++) {
    switch (asyncmeta_dn_massage(dc, &a_vals[i], &bv)) {
    case LDAP_UNWILLING_TO_PERFORM:
      /*
       * FIXME: need to check if it may be considered
       * legal to trim values when adding/modifying;
       * it should be when searching (e.g. ACLs).
       */
      ber_memfree(a_vals[i].bv_val);
      if (last > i) {
        a_vals[i] = a_vals[last];
      }
      BER_BVZERO(&a_vals[last]);
      last--;
      break;

    default:
      /* leave attr untouched if massage failed */
      if (!BER_BVISNULL(&bv) && a_vals[i].bv_val != bv.bv_val) {
        ber_memfree(a_vals[i].bv_val);
        a_vals[i] = bv;
      }
      break;
    }
  }

  return 0;
}
