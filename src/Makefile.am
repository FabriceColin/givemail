## Process this file with automake to produce Makefile.in

## Created by Anjuta

INCLUDES = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" 

noinst_HEADERS = \
	AuthSQL.h \
	Base64.h \
	Campaign.h \
	CampaignSQL.h \
	ConfigurationFile.h \
	CSVParser.h \
	DBStatusUpdater.h \
	DBUsageLogger.h \
	Daemon.h \
	DomainAuth.h \
	DomainKeys.h \
	DomainLimits.h \
	DomainsMap.h \
	HMAC.h \
	Key.h \
	LibESMTPProvider.h \
	LibETPANProvider.h \
	MessageDetails.h \
	MySQLBase.h \
	Process.h \
	QuotedPrintable.h \
	Recipient.h \
	Resolver.h \
	SQLDB.h \
	SMTPOptions.h \
	SMTPProvider.h \
	SMTPSession.h \
	StatusUpdater.h \
	Substitute.h \
	Threads.h \
	TimeConverter.h \
	Timer.h \
	URLEncoding.h \
	UsageLogger.h \
	WebAPI.h \
	XmlMessageDetails.h

noinst_LTLIBRARIES = libMailUtils.la libCommon.la libMailCore.la

lib_LTLIBRARIES = libgivemail.la

if HAVE_FASTCGI
bin_PROGRAMS = csv2givemail givemail givemaild webapi-key-manager webapi.fcgi
else
if USE_DB
bin_PROGRAMS = csv2givemail givemail givemaild
else
bin_PROGRAMS = csv2givemail givemail
endif
endif

libMailUtils_la_SOURCES = \
	Base64.cc \
	DomainAuth.cc \
	DomainLimits.cc \
	MessageDetails.cc \
	Recipient.cc \
	Resolver.cc \
	SMTPOptions.cc \
	StatusUpdater.cc \
	Substitute.cc

libCommon_la_SOURCES = \
	Campaign.cc \
	ConfigurationFile.cc \
	CSVParser.cc \
	Daemon.cc \
	DomainKeys.cc \
	DomainsMap.cc \
	HMAC.cc \
	Key.cc \
	Process.cc \
	Threads.cc \
	TimeConverter.cc \
	UsageLogger.cc \
	XmlMessageDetails.cc

if USE_DB
if USE_MYSQL
libCommon_la_SOURCES += \
	CampaignSQL.cc \
	DBStatusUpdater.cc \
	MySQLBase.cc \
	SQLDB.cc
endif
endif

libMailCore_la_SOURCES = \
	QuotedPrintable.cc \
	SMTPProvider.cc \
	SMTPSession.cc \
	Timer.cc

if USE_LIBESMTP
libMailCore_la_SOURCES += \
	LibESMTPProvider.cc
endif
if USE_LIBETPAN
libMailCore_la_SOURCES += \
	LibETPANProvider.cc
endif

csv2givemail_SOURCES = \
	csv2givemail.cc

csv2givemail_LDFLAGS = \
	-rdynamic

csv2givemail_LDADD = \
	-lgivemail -lCommon \
	@RESOLV_LIB@ \
	-lctemplate \
	@SMTP_LIBS@ \
	@LIBXML_LIBS@ \
	@HTTP_LIBS@ \
	$(libdomainkeysdir)/libdomainkeys.a \
	@OPENSSL_LIBS@ \
	@DB_LIBS@

csv2givemail_DEPENDENCIES = libCommon.la libgivemail.la

givemail_SOURCES = \
	givemail.cc

givemail_LDFLAGS = \
	-rdynamic

givemail_LDADD = \
	-lgivemail -lCommon \
	@RESOLV_LIB@ \
	-lctemplate \
	@SMTP_LIBS@ \
	@LIBXML_LIBS@ \
	@HTTP_LIBS@ \
	$(libdomainkeysdir)/libdomainkeys.a \
	@OPENSSL_LIBS@ \
	@DB_LIBS@

givemail_DEPENDENCIES = libCommon.la libgivemail.la

givemaild_SOURCES = \
	givemaild.cc

givemaild_LDFLAGS = \
	-rdynamic

givemaild_LDADD = \
	-lgivemail -lCommon \
	@RESOLV_LIB@ \
	-lctemplate \
	@LIBXML_LIBS@ \
	@HTTP_LIBS@ \
	@DB_LIBS@

givemaild_DEPENDENCIES = libCommon.la libgivemail.la

libgivemail_la_SOURCES =

libgivemail_la_LDFLAGS = -module -version-info 0:0:0 -shared -nostartfiles

libgivemail_la_LIBADD = \
	-lMailCore -lMailUtils \
	@RESOLV_LIB@ \
	-lctemplate \
	@SMTP_LIBS@ \
	@OPENSSL_LIBS@

libgivemail_la_DEPENDENCIES = libMailUtils.la libMailCore.la

webapi_key_manager_SOURCES = \
	AuthSQL.cc \
	webapi-key-manager.cc

webapi_key_manager_LDFLAGS = \
	-rdynamic

webapi_key_manager_LDADD = \
	-lCommon -lMailUtils \
	@RESOLV_LIB@ \
	-lctemplate \
	@LIBXML_LIBS@ \
	@HTTP_LIBS@ \
	@OPENSSL_LIBS@ \
	@DB_LIBS@

webapi_key_manager_DEPENDENCIES = libMailUtils.la libCommon.la

webapi_fcgi_SOURCES = \
	AuthSQL.cc \
	DBUsageLogger.cc \
	URLEncoding.cc \
	WebAPI.cc \
	webapi-fcgi.cc

webapi_fcgi_LDFLAGS = \
	-rdynamic

webapi_fcgi_LDADD = \
	-lCommon -lMailUtils \
	@RESOLV_LIB@ \
	-lctemplate \
	@LIBXML_LIBS@ \
	@HTTP_LIBS@ \
	@OPENSSL_LIBS@ \
	@FASTCGI_LIBS@ \
	@DB_LIBS@

webapi_fcgi_DEPENDENCIES = libMailUtils.la libCommon.la

AM_CXXFLAGS = \
	-g -Wall \
	-fPIC \
	@SMTP_CFLAGS@ \
	@LIBXML_CFLAGS@ \
	@HTTP_CFLAGS@ \
	@OPENSSL_CFLAGS@ \
	@FASTCGI_CFLAGS@ \
	@DB_CFLAGS@ \
	-I$(libdomainkeysdir)

