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
	DomainLimits.h \
	DomainsMap.h \
	HMAC.h \
	Key.h \
	LibESMTPProvider.h \
	LibETPANProvider.h \
	MessageDetails.h \
	MySQLBase.h \
	OpenDKIM.h \
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

libMailCore_la_SOURCES = \
	OpenDKIM.cc \
	QuotedPrintable.cc \
	SMTPMessage.cc \
	SMTPProvider.cc \
	SMTPSession.cc \
	Timer.cc

if USE_LIBETPAN
libMailCore_la_SOURCES += \
	LibETPANProvider.cc
endif
if USE_LIBESMTP
libMailCore_la_SOURCES += \
	LibESMTPProvider.cc
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

csv2givemail_SOURCES = \
	csv2givemail.cc

csv2givemail_LDFLAGS = \
	-rdynamic

csv2givemail_LDADD = \
	-lMailCore -lMailUtils -lCommon \
	@RESOLV_LIB@ \
	-lctemplate \
	@SMTP_LIBS@ \
	@LIBXML_LIBS@ \
	@HTTP_LIBS@ \
	@OPENSSL_LIBS@ \
	@OPENDKIM_LIBS@ \
	@SASL_LIBS@ \
	@DB_LIBS@ \
	@PTHREAD_LIBS@

csv2givemail_DEPENDENCIES = libCommon.la libMailUtils.la libMailCore.la

givemail_SOURCES = \
	givemail.cc

givemail_LDFLAGS = \
	-rdynamic

givemail_LDADD = \
	-lMailCore -lMailUtils -lCommon \
	@RESOLV_LIB@ \
	-lctemplate \
	@SMTP_LIBS@ \
	@LIBXML_LIBS@ \
	@HTTP_LIBS@ \
	@OPENSSL_LIBS@ \
	@OPENDKIM_LIBS@ \
	@SASL_LIBS@ \
	@DB_LIBS@ \
	@PTHREAD_LIBS@

givemail_DEPENDENCIES = libCommon.la libMailUtils.la libMailCore.la

givemaild_SOURCES = \
	givemaild.cc

givemaild_LDFLAGS = \
	-rdynamic

givemaild_LDADD = \
	-lMailCore -lMailUtils -lCommon \
	@RESOLV_LIB@ \
	-lctemplate \
	@LIBXML_LIBS@ \
	@HTTP_LIBS@ \
	@DB_LIBS@ \
	@PTHREAD_LIBS@

givemaild_DEPENDENCIES = libCommon.la libMailUtils.la libMailCore.la

libgivemail_la_SOURCES =

libgivemail_la_LDFLAGS = -module -version-info 0:0:0 -shared -nostartfiles

libgivemail_la_LIBADD = \
	-lMailCore -lMailUtils -lCommon \
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
	-lMailCore -lMailUtils -lCommon \
	@RESOLV_LIB@ \
	-lctemplate \
	@LIBXML_LIBS@ \
	@HTTP_LIBS@ \
	@OPENSSL_LIBS@ \
	@DB_LIBS@

webapi_key_manager_DEPENDENCIES = libCommon.la libMailUtils.la

webapi_fcgi_SOURCES = \
	AuthSQL.cc \
	DBUsageLogger.cc \
	URLEncoding.cc \
	WebAPI.cc \
	webapi-fcgi.cc

webapi_fcgi_LDFLAGS = \
	-rdynamic

webapi_fcgi_LDADD = \
	-lMailUtils -lCommon \
	@RESOLV_LIB@ \
	-lctemplate \
	@LIBXML_LIBS@ \
	@HTTP_LIBS@ \
	@OPENSSL_LIBS@ \
	@FASTCGI_LIBS@ \
	@DB_LIBS@

webapi_fcgi_DEPENDENCIES = libCommon.la libMailUtils.la

AM_CXXFLAGS = \
	-g -Wall \
	-fPIC \
	@SMTP_CFLAGS@ \
	@LIBXML_CFLAGS@ \
	@HTTP_CFLAGS@ \
	@OPENSSL_CFLAGS@ \
	@OPENDKIM_CFLAGS@ \
	@SASL_CFLAGS@ \
	@FASTCGI_CFLAGS@ \
	@DB_CFLAGS@

