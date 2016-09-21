DEBIANFILES = \
	README.Debian					\
	changelog.in					\
	compat						\
	control.in					\
	copyright					\
	rules						\
	vainfo.install					\
	$(NULL)

DEBIANGENFILES = \
	changelog					\
	control						\
	$(NULL)

EXTRA_DIST = $(DEBIANFILES)

DISTCLEANFILES = $(DEBIANGENFILES)

dist_noinst_DATA = $(DEBIANGENFILES)

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = Makefile.in
