## Process this file with automake to produce Makefile.in

SUBDIRS = src po

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp \
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) \
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

EXTRA_DIST = README COPYING.LIB AUTHORS ChangeLog ChangeLog-previous INSTALL NEWS TODO Doxyfile \
	intltool-extract.in intltool-merge.in intltool-update.in \
	conf/*.conf conf/*.sql conf/*.logrotate solaris \
	sample-api-calls/*.xml sample-emails/*.xml scripts/givemaild \
	givemail.spec src/*.1

if HAVE_FASTCGI
man_MANS = src/csv2givemail.1 src/givemail.1 src/givemaild.1 src/webapi-key-manager.1
else
man_MANS = src/csv2givemail.1 src/givemail.1 src/givemaild.1
endif

documentation:
	@doxygen

manuals:
	@help2man -Nn "Extract recipients from a CSV file" src/csv2givemail >src/csv2givemail.1
	@help2man -Nn "Send emails out en masse" src/givemail >src/givemail.1
	@help2man -Nn "Monitor the campaigns database" src/givemaild >src/givemaild.1
if HAVE_FASTCGI
	@help2man -Nn "Manage WebAPI keys" src/webapi-key-manager >src/webapi-key-manager.1
endif

solaris-pkg:
	cd solaris && pkgmk -o -r / -d /tmp -f Prototype
	cd /tmp && pkgtrans -s . givemail-$(VERSION)-sol8-sparc GSIgivemail && gzip -9 givemail-$(VERSION)-sol8-sparc

install-data-local:
	@mkdir -p $(DESTDIR)$(sysconfdir)/givemail/conf.d
	$(INSTALL_DATA) $(srcdir)/conf/*.sql $(DESTDIR)$(sysconfdir)/givemail
	$(INSTALL_DATA) $(srcdir)/conf/*.conf $(DESTDIR)$(sysconfdir)/givemail/conf.d
	@mkdir -p $(DESTDIR)$(sysconfdir)/logrotate.d
	@install -m 644 -p $(srcdir)/conf/givemaild.logrotate $(DESTDIR)$(sysconfdir)/logrotate.d/givemaild
	@mkdir -p $(DESTDIR)$(sysconfdir)/rc.d/init.d
	@install -m 0755 scripts/givemaild $(DESTDIR)$(sysconfdir)/rc.d/init.d
if HAVE_FASTCGI
	@mkdir -p $(DESTDIR)$(datadir)/givemail/sample-api-calls
	$(INSTALL_DATA) $(srcdir)/sample-api-calls/*.xml $(DESTDIR)$(datadir)/givemail/sample-api-calls/
endif
	@mkdir -p $(DESTDIR)$(datadir)/givemail/sample-emails
	$(INSTALL_DATA) $(srcdir)/sample-emails/*.xml $(DESTDIR)$(datadir)/givemail/sample-emails/
	@mkdir -p $(DESTDIR)$(datadir)/givemail
	@mkdir -p $(DESTDIR)/var/run/givemail
if HAVE_FASTCGI
	@mkdir -p $(DESTDIR)/var/www/cgi-bin/
	@mv $(DESTDIR)/usr/bin/webapi.fcgi $(DESTDIR)/var/www/cgi-bin/
endif

uninstall-local:
	@rm -rf $(DESTDIR)$(sysconfdir)/givemail

