### Основной репозиторий перемещен на [GitFlic](https://gitflic.ru/project/erthink/reopenldap)

Весной 2022, без каких-либо предупреждений или пояснений, администрация
Github удалила мой аккаунт и все проекты. Через несколько месяцев, без
какого-либо моего участия или уведомления, проекты были
восстановлены/открыты в статусе "public read-only archive" из какой-то
неполноценной резервной копии. Эти действия Github я расцениваю как
злонамеренный саботаж, а сам сервис Github считаю навсегда утратившим
какое-либо доверие.

Вследствие произошедшего, никогда и ни при каких условиях, я не буду
размещать на Github первоисточники (aka origins) моих проектов, либо
как-либо полагаться на инфраструктуру Github.

Тем не менее, понимая что пользователям моих проектов удобнее получать к
ним доступ именно на Github, я не хочу ограничивать их свободу или
создавать неудобство, и поэтому размещаю на Github зеркала (aka mirrors)
репозиториев моих проектов. При этом ещё раз акцентирую внимание, что
это только зеркала, которые могут быть заморожены, заблокированы или
удалены в любой момент, как это уже было в 2022.

### The origin has been migrated to [GitFlic](https://gitflic.ru/project/erthink/reopenldap)

In the spring of 2022, without any warnings or explanations, the Github
administration deleted my account and all projects. A few months later,
without any involvement or notification from me, the projects were
restored/opened in the "public read-only archive" status from some kind
of incomplete backup. I regard these actions of Github as malicious
sabotage, and I consider the Github service itself to have lost any
trust forever.

As a result of what has happened, I will never, under any circumstances,
post the primary sources (aka origins) of my projects on Github, or rely
in any way on the Github infrastructure.

Nevertheless, realizing that it is more convenient for users of my
projects to access them on Github, I do not want to restrict their
freedom or create inconvenience, and therefore I place mirrors of my
project repositories on Github. At the same time, I would like to
emphasize once again that these are only mirrors that can be frozen,
blocked or deleted at any time, as was the case in 2022.

--------------------------------------------------------------------------------

ReOpenLDAP
==========

Production-ready replacement for OpenLDAP on Linux:

 - A lot of bug fixing and code quality improvement.
 - A number of new features, most of which deal with highload and multi-master clustering.
 - Bundled with all known contributed extensions.
 - Clean build without warnings from modern compilers.
 - But only Linux supported, e.g no Windows, Mac OS, FreeBSD, Solaris or HP-UX.

##### ReOpenLDAP is currently running in telcos across Russia:
 * Several clusters in full mesh multi-master replication topology, mostly with four nodes as a two geographically distributed pairs.
 * Up to 100 million records and up to 100 GB of data on each node.
 * Up to 10K updates and up to 25K searches per second.

No other LDAP server can provide such level of performance nowadays
due to replication troubles, inadequate performance or high risk of a crash.
Therefore ReopenLDAP also known as "TelcoLDAP" - the telco-oriented fork of OpenLDAP.


Краткая история
---------------

ReOpenLDAP был создан в 2015 году для решения проблем, возникших при использовании оригинального
[OpenLDAP](https://ru.wikipedia.org/wiki/OpenLDAP) в инфраструктуре [ПАО «МегаФон»](https://corp.megafon.ru/),
где LDAP-сервер был задействован в одной из подсистем инфраструктуры.

 > NGDR представляет собой UDR (User Data Repository), согласно стандарту [3GPP 23.335](https://portal.3gpp.org/desktopmodules/Specifications/SpecificationDetails.aspx?specificationId=845),
 > и является централизованным узлом для хранения данных обо всех видах услуг абонентов
 > в ИТ-инфраструктуре оператора связи.

Подобное применение предполагало промышленную эксплуатацию в режиме 24×7 специфического LDAP-каталога,
размером 10-100 миллионов записей, в высоконагруженном сценарии (до 10К обновлений и до 50К чтений в секунду),
и в топологии мульти-мастер.

Можно сказать, что ReOpenLDAP появился вынужденно, в результате как некачественности родительского OpenLDAP,
так и отказа принимать исправления. Symas Corp, как основные разработчики, коммитеры и владельцы кода OpenLDAP,
не смогли решить возникшие проблемы, поэтому было решено попробовать сделать это самостоятельно.

Как впоследствии выяснилось, ошибок в коде было кратно больше, чем можно было предполагать.
Поэтому было затрачено больше усилий чем планировалось, а ReOpenLDAP по-прежнему представляет
определённую ценность и (по имеющейся информации) является единственным LDAP-сервером,
полноценно и надёжно поддерживающим мульти-мастер топологию для RFC-4533, в том числе в высоконагруженных сценариях.


Features and Change List
------------------------

Below is a list of main new features of ReOpenLDAP,
for a description ones please see the corresponding man pages after installation,
i.e. `man --manpath=CONFIGURED_PREFIX/share/man slapd.conf`.

For latest news and changes please refer to the [NEWS.md](NEWS.md) and [ChangeLog](ChangeLog).

List of changes emerged from OpenLDAP project could be seen in the [CHANGES.OpenLDAP](CHANGES.OpenLDAP).

#### Added features:
 * multi-master replication is working properly and robustly (it seems no other LDAP server can do this)
 * `reopenldap [iddqd] [idkfa]`
 * `quorum { [vote-sids ...] [vote-rids ...] [auto-sids] [auto-rids] [require-sids ...] [require-rids ...] [all-links] }`
 * `quorum limit-concurrent-refresh`
 * `biglock { none | local | common }`
 * storage (mdb backend): dreamcatcher & oom-handler (ITS#7974), lifo & coalesce (ITS#7958)
 * `syncprov-showstatus { none | running | all }`
 * syncrepl's `requirecheckpresent` option
 * `keepalive <idle>:<probes>:<interval>` for incoming connections
 * built-in memory checker called 'Hipagut', including ls-malloc
 * support for OpenSSL 1.1.x, Mozilla NSS, GnuTLS and LibreSSL 2.5.x
 * ready for LTO (Link-Time Optimization) by GCC and clang.


Support
-------

ReOpenLDAP is intended for use in scenarios of heavy industrial operation using synchronization/replication in multi-master mode and full-mesh topology.
This assumes that the installation and operation will be handled by in-house system administrators or qualified specialists with relevant experience.

Thus, you should rely on your own strength, and seek my support only to fix a bugs you have discovered.
With this you can count on free support under the generally accepted terms of use of open source code.
If you need more then I think it's wise considering paid support.

Nonetheless, please note and understand that I do not have the ability to provide a full-fledged support for documentation,
including assembly and/or installation manuals, etc.


Installation
------------

Traditional triade `./configure --prefix=YOUR_INSTALLATION_PREFIX YOUR_OPTIONS` && `make` && `make install`.
However the `configure` will absent, in case you use development or a snapshot versions,
so you need run the `./bootstrap` to build them.

For more information please see the local `INSTALL` file after the `./bootstrap` was done.


`configure`'s options
---------------------

Below is a main configure's options, to see full list please run `./configure --help`,
for instance both `--libexecdir=DIR` and `--sysconfdir=DIR` are provided.

```
Fine tuning of the installation directories:
    ...
  --libexecdir=DIR        program executables [EPREFIX/libexec]
  --sysconfdir=DIR        read-only single-machine data [PREFIX/etc]
  --sharedstatedir=DIR    modifiable architecture-independent data [PREFIX/com]
  --localstatedir=DIR     modifiable single-machine data [PREFIX/var]
  --runstatedir=DIR       modifiable per-process data [LOCALSTATEDIR/run]
  --libdir=DIR            object code libraries [EPREFIX/lib]
  --includedir=DIR        C header files [PREFIX/include]
  --oldincludedir=DIR     C header files for non-gcc [/usr/include]
  --datarootdir=DIR       read-only arch.-independent data root [PREFIX/share]
  --datadir=DIR           read-only architecture-independent data [DATAROOTDIR]
  --infodir=DIR           info documentation [DATAROOTDIR/info]
    ...

Optional Features:
    ...
  --enable-debug          enable debug logging no|yes|extra [yes]
  --enable-ci             enable Continuous Integration stuff no|yes [no]
  --enable-syslog         enable syslog support [auto]
  --enable-contrib        enable extra plugins and overlays no|yes|broken [no]
  --enable-experimental   enable experimental and developing features no|yes [no]
  --enable-check          enable internal checking and assertions no|yes|always|default [no]
  --enable-hipagut        enable internal memory allocation debugger no|yes|always|extra [no]
  --enable-proctitle      enable proctitle support [yes]
  --enable-referrals      enable LDAPv2+ Referrals (experimental) [no]
  --enable-ipv6           enable IPv6 support [auto]
  --enable-local          enable AF_LOCAL (AF_UNIX) socket support [auto]
  --enable-deprecated     enable deprecated interfaces of libreldap no|yes [no]
  --enable-valgrind       Whether to enable Valgrind on the unit tests
    ...

SLAPD (Standalone LDAP Daemon) Options:
  --enable-slapd	  enable building slapd [yes]
    --enable-dynacl	  enable run-time loadable ACL support (experimental) [no]
    --enable-aci	  enable per-object ACIs (experimental) no|yes|mod [no]
    --enable-cleartext	  enable cleartext passwords [yes]
    --enable-crypt	  enable crypt(3) passwords [no]
    --enable-lmpasswd	  enable LAN Manager passwords [no]
    --enable-spasswd	  enable (Cyrus) SASL password verification [no]
    --enable-modules	  enable dynamic module support [yes]
    --enable-rewrite	  enable DN rewriting in back-ldap and rwm overlay [auto]
    --enable-rlookups	  enable reverse lookups of client hostnames [no]
    --enable-slapi        enable SLAPI support (experimental) [no]
    --enable-slp          enable SLPv2 support [no]
    --enable-wrappers	  enable tcp wrapper support [no]

SLAPD Backend Options:
    --enable-backends	  enable all stable/non-experimental backends no|yes|mod
    --enable-mdb	  enable MDBX database backend no|yes|mod [yes]
    --enable-hdb	  enable Hierarchical Berkeley DB backend (obsolete) no|yes|mod [no]
    --enable-bdb	  enable Berkeley DB backend (obsolete) no|yes|mod [no]
    --enable-dnssrv	  enable dnssrv backend (experimental) no|yes|mod [no]
    --enable-ldap	  enable ldap backend no|yes|mod [no]
    --enable-meta	  enable metadirectory backend no|yes|mod [no]
    --enable-asyncmeta	  enable asynchronous metadirectory backend (experimental) no|yes|mod [no]
    --enable-monitor	  enable monitor backend no|yes|mod [yes]
    --enable-ndb	  enable MySQL NDB Cluster backend (experimental) no|yes|mod [no]
    --enable-null	  enable null backend no|yes|mod [no]
    --enable-passwd	  enable passwd backend no|yes|mod [no]
    --enable-perl	  enable perl backend no|yes|mod [no]
    --enable-relay  	  enable relay backend (experimental) no|yes|mod [yes]
    --enable-shell	  enable shell backend no|yes|mod [no]
    --enable-sock	  enable sock backend no|yes|mod [no]
    --enable-sql	  enable SQL backend (experimental and buggy) no|yes|mod [no]
    --enable-wt		  enable WiredTiger backend no|yes|mod [no]

SLAPD Overlay Options:
    --enable-overlays	  enable all available overlays no|yes|mod
    --enable-accesslog	  In-Directory Access Logging overlay no|yes|mod [no]
    --enable-auditlog	  Audit Logging overlay no|yes|mod [no]
    --enable-autoca	  Automatic Certificate Authority overlay no|yes|mod [no]
    --enable-collect	  Collect overlay no|yes|mod [no]
    --enable-constraint	  Attribute Constraint overlay no|yes|mod [no]
    --enable-dds  	  Dynamic Directory Services overlay no|yes|mod [no]
    --enable-deref	  Dereference overlay no|yes|mod [no]
    --enable-dyngroup	  Dynamic Group overlay no|yes|mod [no]
    --enable-dynlist	  Dynamic List overlay no|yes|mod [no]
    --enable-memberof	  Reverse Group Membership overlay no|yes|mod [no]
    --enable-ppolicy	  Password Policy overlay no|yes|mod [no]
    --enable-pcache	  Proxy Cache overlay no|yes|mod [no]
    --enable-refint	  Referential Integrity overlay no|yes|mod [no]
    --enable-retcode	  Return Code testing overlay no|yes|mod [no]
    --enable-rwm       	  Rewrite/Remap overlay no|yes|mod [no]
    --enable-seqmod	  Sequential Modify overlay no|yes|mod [no]
    --enable-sssvlv	  ServerSideSort/VLV overlay no|yes|mod [no]
    --enable-syncprov	  Syncrepl Provider overlay no|yes|mod [yes]
    --enable-translucent  Translucent Proxy overlay no|yes|mod [no]
    --enable-unique       Attribute Uniqueness overlay no|yes|mod [no]
    --enable-valsort      Value Sorting overlay no|yes|mod [no]

Optional Packages:
    ...
  --with-cyrus-sasl	  with Cyrus SASL support [auto]
  --with-gssapi		  with GSSAPI support [auto]
  --with-fetch		  with fetch(3) URL support [auto]
  --with-tls		  with TLS/SSL support auto|openssl|gnutls|moznss [auto]
  --with-yielding-select  with implicitly yielding select [auto]
  --with-mp               with multiple precision statistics auto|longlong|long|bignum|gmp [auto]
  --with-odbc             with specific ODBC support iodbc|unixodbc|auto [auto]

Some influential environment variables:
  ...
  EXTRA_CFLAGS
              Extra build-time CFLAGS, e.g. -Wall -Werror. Alternatively, ones
              can be specified or overridden by invocation 'make
              EXTRA_CFLAGS="a b c"'
  ...
  KRB5_CFLAGS C compiler flags for KRB5, overriding pkg-config
  KRB5_LIBS   linker flags for KRB5, overriding pkg-config
  HEIMDAL_CFLAGS
              C compiler flags for HEIMDAL, overriding pkg-config
  HEIMDAL_LIBS
              linker flags for HEIMDAL, overriding pkg-config
  LIBSODIUM_CFLAGS
              C compiler flags for LIBSODIUM, overriding pkg-config
  LIBSODIUM_LIBS
              linker flags for LIBSODIUM, overriding pkg-config
  UUID_CFLAGS C compiler flags for UUID, overriding pkg-config
  UUID_LIBS   linker flags for UUID, overriding pkg-config
  OPENSSL_CFLAGS
              C compiler flags for OPENSSL, overriding pkg-config
  OPENSSL_LIBS
              linker flags for OPENSSL, overriding pkg-config
  GNUTLS_CFLAGS
              C compiler flags for GNUTLS, overriding pkg-config
  GNUTLS_LIBS linker flags for GNUTLS, overriding pkg-config
  MOZNSS_CFLAGS
              C compiler flags for MOZNSS, overriding pkg-config
  MOZNSS_LIBS linker flags for MOZNSS, overriding pkg-config
```
