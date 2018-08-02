/*    crl.h
 *    "CertificateRevokationList" ASN.1 module
 * encode/decode/extracting/matching/free C src. This file was generated by
 * modified eSMACC compiler Fri Jan 21 11:25:24 2005 The generated files are
 * strongly encouraged to be compiled as a module for OpenLDAP Software
 */

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

#include "asn-incl.h"

#ifndef _crl_h_
#define _crl_h_

#ifdef __cplusplus
extern "C" {
#endif
#include "componentlib.h"

#define V1 0
#define V2 1
#define V3 2

typedef ComponentInt ComponentVersion; /* INTEGER { V1 (0), V2 (1), V3 (2) }  */

#define MatchingComponentVersion MatchingComponentInt

#define ExtractingComponentVersion ExtractingComponentInt

#define BDecComponentVersion BDecComponentInt

#define GDecComponentVersion GDecComponentInt

typedef ComponentInt ComponentCertificateSerialNumber; /* INTEGER */

#define MatchingComponentCertificateSerialNumber MatchingComponentInt

#define ExtractingComponentCertificateSerialNumber ExtractingComponentInt

#define BDecComponentCertificateSerialNumber BDecComponentInt

#define GDecComponentCertificateSerialNumber GDecComponentInt

typedef ComponentOid ComponentAttributeType; /* OBJECT IDENTIFIER */

#define MatchingComponentAttributeType MatchingComponentOid

#define ExtractingComponentAttributeType ExtractingComponentOid

#define BDecComponentAttributeType BDecComponentOid

#define GDecComponentAttributeType GDecComponentOid

typedef struct AlgorithmIdentifier /* SEQUENCE */
{
  Syntax *syntax;
  ComponentDesc *comp_desc;
  struct berval identifier;
  char id_buf[MAX_IDENTIFIER_LEN];
  ComponentOid algorithm;           /* OBJECT IDENTIFIER */
  ComponentAnyDefinedBy parameters; /* ANY DEFINED BY algorithm OPTIONAL */
} ComponentAlgorithmIdentifier;

int MatchingComponentAlgorithmIdentifier PROTO((char *oid,
                                                ComponentSyntaxInfo *,
                                                ComponentSyntaxInfo *v2));

void *ExtractingComponentAlgorithmIdentifier PROTO(
    (void *mem_op, ComponentReference *cr, ComponentAlgorithmIdentifier *comp));

int BDecComponentAlgorithmIdentifier PROTO((void *mem_op, GenBuf *b,
                                            AsnTag tagId0, AsnLen elmtLen0,
                                            ComponentAlgorithmIdentifier **v,
                                            AsnLen *bytesDecoded, int mode));

int GDecComponentAlgorithmIdentifier PROTO((void *mem_op, GenBuf *b,
                                            ComponentAlgorithmIdentifier **v,
                                            AsnLen *bytesDecoded, int mode));

typedef struct Time /* CHOICE */
{
  Syntax *syntax;
  ComponentDesc *comp_desc;
  struct berval identifier;
  char id_buf[MAX_IDENTIFIER_LEN];
  enum TimeChoiceId { TIME_UTCTIME, TIME_GENERALIZEDTIME } choiceId;
  union TimeChoiceUnion {
    ComponentUTCTime *utcTime;                 /* < unknown type id ?! > */
    ComponentGeneralizedTime *generalizedTime; /* < unknown type id ?! > */
  } a;
} ComponentTime;

int MatchingComponentTime PROTO((char *oid, ComponentSyntaxInfo *,
                                 ComponentSyntaxInfo *v2));

void *ExtractingComponentTime PROTO((void *mem_op, ComponentReference *cr,
                                     ComponentTime *comp));

int BDecComponentTime PROTO((void *mem_op, GenBuf *b, AsnTag tagId0,
                             AsnLen elmtLen0, ComponentTime **v,
                             AsnLen *bytesDecoded, int mode));

int GDecComponentTime PROTO((void *mem_op, GenBuf *b, ComponentTime **v,
                             AsnLen *bytesDecoded, int mode));

typedef struct Extension /* SEQUENCE */
{
  Syntax *syntax;
  ComponentDesc *comp_desc;
  struct berval identifier;
  char id_buf[MAX_IDENTIFIER_LEN];
  ComponentOid extnID;     /* OBJECT IDENTIFIER */
  ComponentBool *critical; /* BOOLEAN DEFAULT FALSE */
  ComponentOcts extnValue; /* OCTET STRING */
} ComponentExtension;

int MatchingComponentExtension PROTO((char *oid, ComponentSyntaxInfo *,
                                      ComponentSyntaxInfo *v2));

void *ExtractingComponentExtension PROTO((void *mem_op, ComponentReference *cr,
                                          ComponentExtension *comp));

int BDecComponentExtension PROTO((void *mem_op, GenBuf *b, AsnTag tagId0,
                                  AsnLen elmtLen0, ComponentExtension **v,
                                  AsnLen *bytesDecoded, int mode));

int GDecComponentExtension PROTO((void *mem_op, GenBuf *b,
                                  ComponentExtension **v, AsnLen *bytesDecoded,
                                  int mode));

typedef struct AttributeTypeAndValue /* SEQUENCE */
{
  Syntax *syntax;
  ComponentDesc *comp_desc;
  struct berval identifier;
  char id_buf[MAX_IDENTIFIER_LEN];
  ComponentAttributeType type; /* AttributeType */
  ComponentAnyDefinedBy value; /* ANY DEFINED BY type */
} ComponentAttributeTypeAndValue;

int MatchingComponentAttributeTypeAndValue PROTO((char *oid,
                                                  ComponentSyntaxInfo *,
                                                  ComponentSyntaxInfo *v2));

void *ExtractingComponentAttributeTypeAndValue
    PROTO((void *mem_op, ComponentReference *cr,
           ComponentAttributeTypeAndValue *comp));

int BDecComponentAttributeTypeAndValue
    PROTO((void *mem_op, GenBuf *b, AsnTag tagId0, AsnLen elmtLen0,
           ComponentAttributeTypeAndValue **v, AsnLen *bytesDecoded, int mode));

int GDecComponentAttributeTypeAndValue
    PROTO((void *mem_op, GenBuf *b, ComponentAttributeTypeAndValue **v,
           AsnLen *bytesDecoded, int mode));

typedef ComponentList
    ComponentExtensions; /* SEQUENCE SIZE 1..MAX OF Extension */

int MatchingComponentExtensions PROTO((char *oid, ComponentSyntaxInfo *,
                                       ComponentSyntaxInfo *v2));

void *ExtractingComponentExtensions PROTO((void *mem_op, ComponentReference *cr,
                                           ComponentExtensions *comp));

int BDecComponentExtensions PROTO((void *mem_op, GenBuf *b, AsnTag tagId0,
                                   AsnLen elmtLen0, ComponentExtensions **v,
                                   AsnLen *bytesDecoded, int mode));

int GDecComponentExtensions PROTO((void *mem_op, GenBuf *b,
                                   ComponentExtensions **v,
                                   AsnLen *bytesDecoded, int mode));

typedef struct TBSCertListSeqOfSeq /* SEQUENCE */
{
  Syntax *syntax;
  ComponentDesc *comp_desc;
  struct berval identifier;
  char id_buf[MAX_IDENTIFIER_LEN];
  ComponentCertificateSerialNumber
      userCertificate;                     /* CertificateSerialNumber */
  ComponentTime *revocationDate;           /* Time */
  ComponentExtensions *crlEntryExtensions; /* Extensions OPTIONAL */
} ComponentTBSCertListSeqOfSeq;

int MatchingComponentTBSCertListSeqOfSeq PROTO((char *oid,
                                                ComponentSyntaxInfo *,
                                                ComponentSyntaxInfo *v2));

void *ExtractingComponentTBSCertListSeqOfSeq PROTO(
    (void *mem_op, ComponentReference *cr, ComponentTBSCertListSeqOfSeq *comp));

int BDecComponentTBSCertListSeqOfSeq PROTO((void *mem_op, GenBuf *b,
                                            AsnTag tagId0, AsnLen elmtLen0,
                                            ComponentTBSCertListSeqOfSeq **v,
                                            AsnLen *bytesDecoded, int mode));

int GDecComponentTBSCertListSeqOfSeq PROTO((void *mem_op, GenBuf *b,
                                            ComponentTBSCertListSeqOfSeq **v,
                                            AsnLen *bytesDecoded, int mode));

typedef ComponentList
    ComponentTBSCertListSeqOf; /* SEQUENCE OF TBSCertListSeqOfSeq */

int MatchingComponentTBSCertListSeqOf PROTO((char *oid, ComponentSyntaxInfo *,
                                             ComponentSyntaxInfo *v2));

void *ExtractingComponentTBSCertListSeqOf PROTO(
    (void *mem_op, ComponentReference *cr, ComponentTBSCertListSeqOf *comp));

int BDecComponentTBSCertListSeqOf PROTO((void *mem_op, GenBuf *b, AsnTag tagId0,
                                         AsnLen elmtLen0,
                                         ComponentTBSCertListSeqOf **v,
                                         AsnLen *bytesDecoded, int mode));

int GDecComponentTBSCertListSeqOf PROTO((void *mem_op, GenBuf *b,
                                         ComponentTBSCertListSeqOf **v,
                                         AsnLen *bytesDecoded, int mode));

typedef ComponentList
    ComponentRelativeDistinguishedName; /* SET OF AttributeTypeAndValue */

int MatchingComponentRelativeDistinguishedName PROTO((char *oid,
                                                      ComponentSyntaxInfo *,
                                                      ComponentSyntaxInfo *v2));

void *ExtractingComponentRelativeDistinguishedName
    PROTO((void *mem_op, ComponentReference *cr,
           ComponentRelativeDistinguishedName *comp));

int BDecComponentRelativeDistinguishedName PROTO(
    (void *mem_op, GenBuf *b, AsnTag tagId0, AsnLen elmtLen0,
     ComponentRelativeDistinguishedName **v, AsnLen *bytesDecoded, int mode));

int GDecComponentRelativeDistinguishedName
    PROTO((void *mem_op, GenBuf *b, ComponentRelativeDistinguishedName **v,
           AsnLen *bytesDecoded, int mode));

typedef ComponentList
    ComponentRDNSequence; /* SEQUENCE OF RelativeDistinguishedName */

int MatchingComponentRDNSequence PROTO((char *oid, ComponentSyntaxInfo *,
                                        ComponentSyntaxInfo *v2));

void *ExtractingComponentRDNSequence PROTO((void *mem_op,
                                            ComponentReference *cr,
                                            ComponentRDNSequence *comp));

int BDecComponentRDNSequence PROTO((void *mem_op, GenBuf *b, AsnTag tagId0,
                                    AsnLen elmtLen0, ComponentRDNSequence **v,
                                    AsnLen *bytesDecoded, int mode));

int GDecComponentRDNSequence PROTO((void *mem_op, GenBuf *b,
                                    ComponentRDNSequence **v,
                                    AsnLen *bytesDecoded, int mode));

typedef struct Name /* CHOICE */
{
  Syntax *syntax;
  ComponentDesc *comp_desc;
  struct berval identifier;
  char id_buf[MAX_IDENTIFIER_LEN];
  enum NameChoiceId { NAME_RDNSEQUENCE } choiceId;
  union NameChoiceUnion {
    ComponentRDNSequence *rdnSequence; /* RDNSequence */
  } a;
} ComponentName;

int MatchingComponentName PROTO((char *oid, ComponentSyntaxInfo *,
                                 ComponentSyntaxInfo *v2));

void *ExtractingComponentName PROTO((void *mem_op, ComponentReference *cr,
                                     ComponentName *comp));

int BDecComponentName PROTO((void *mem_op, GenBuf *b, AsnTag tagId0,
                             AsnLen elmtLen0, ComponentName **v,
                             AsnLen *bytesDecoded, int mode));

int GDecComponentName PROTO((void *mem_op, GenBuf *b, ComponentName **v,
                             AsnLen *bytesDecoded, int mode));

typedef struct TBSCertList /* SEQUENCE */
{
  Syntax *syntax;
  ComponentDesc *comp_desc;
  struct berval identifier;
  char id_buf[MAX_IDENTIFIER_LEN];
  ComponentVersion *version;                      /* Version OPTIONAL */
  ComponentAlgorithmIdentifier *signature;        /* AlgorithmIdentifier */
  ComponentName *issuer;                          /* Name */
  ComponentTime *thisUpdate;                      /* Time */
  ComponentTime *nextUpdate;                      /* Time OPTIONAL */
  ComponentTBSCertListSeqOf *revokedCertificates; /* TBSCertListSeqOf */
  ComponentExtensions *crlExtensions; /* [0] EXPLICIT Extensions OPTIONAL */
} ComponentTBSCertList;

int MatchingComponentTBSCertList PROTO((char *oid, ComponentSyntaxInfo *,
                                        ComponentSyntaxInfo *v2));

void *ExtractingComponentTBSCertList PROTO((void *mem_op,
                                            ComponentReference *cr,
                                            ComponentTBSCertList *comp));

int BDecComponentTBSCertList PROTO((void *mem_op, GenBuf *b, AsnTag tagId0,
                                    AsnLen elmtLen0, ComponentTBSCertList **v,
                                    AsnLen *bytesDecoded, int mode));

int GDecComponentTBSCertList PROTO((void *mem_op, GenBuf *b,
                                    ComponentTBSCertList **v,
                                    AsnLen *bytesDecoded, int mode));

typedef struct CertificateList /* SEQUENCE */
{
  Syntax *syntax;
  ComponentDesc *comp_desc;
  struct berval identifier;
  char id_buf[MAX_IDENTIFIER_LEN];
  ComponentTBSCertList *tbsCertList;                /* TBSCertList */
  ComponentAlgorithmIdentifier *signatureAlgorithm; /* AlgorithmIdentifier */
  ComponentBits signature;                          /* BIT STRING */
} ComponentCertificateList;

int MatchingComponentCertificateList PROTO((char *oid, ComponentSyntaxInfo *,
                                            ComponentSyntaxInfo *v2));

void *ExtractingComponentCertificateList PROTO(
    (void *mem_op, ComponentReference *cr, ComponentCertificateList *comp));

int BDecComponentCertificateList PROTO((void *mem_op, GenBuf *b, AsnTag tagId0,
                                        AsnLen elmtLen0,
                                        ComponentCertificateList **v,
                                        AsnLen *bytesDecoded, int mode));

int GDecComponentCertificateList PROTO((void *mem_op, GenBuf *b,
                                        ComponentCertificateList **v,
                                        AsnLen *bytesDecoded, int mode));

typedef struct Validity /* SEQUENCE */
{
  Syntax *syntax;
  ComponentDesc *comp_desc;
  struct berval identifier;
  char id_buf[MAX_IDENTIFIER_LEN];
  ComponentTime *notBefore; /* Time */
  ComponentTime *notAfter;  /* Time */
} ComponentValidity;

int MatchingComponentValidity PROTO((char *oid, ComponentSyntaxInfo *,
                                     ComponentSyntaxInfo *v2));

void *ExtractingComponentValidity PROTO((void *mem_op, ComponentReference *cr,
                                         ComponentValidity *comp));

int BDecComponentValidity PROTO((void *mem_op, GenBuf *b, AsnTag tagId0,
                                 AsnLen elmtLen0, ComponentValidity **v,
                                 AsnLen *bytesDecoded, int mode));

int GDecComponentValidity PROTO((void *mem_op, GenBuf *b, ComponentValidity **v,
                                 AsnLen *bytesDecoded, int mode));

/* ========== Object Declarations ========== */

/* ========== Object Set Declarations ========== */
#ifdef __cplusplus
extern "C" {
#endif

#endif /* conditional include of crl.h */
