NULL =
ACLOCAL_AMFLAGS=-I m4
SUBDIRS = src po
EXTRA_DIST = autogen.sh intltool-extract.in intltool-merge.in intltool-update.in scripts meson.build misc README.md INSTALL.md
MAINTAINERCLEANFILES =		\
	$(srcdir)/aclocal.m4	\
	$(builddir)/config	\
	$(NULL)

-include $(top_srcdir)/git.mk

dist_bin_SCRIPTS = scripts/src-to-paps

# Install the pango_markup.outlang package
papssharedir = $(prefix)/share/$(PACKAGE)
papsshare_DATA = misc/pango_markup.outlang

check-gettext:
	@if test x$(USE_NLS) != "xyes" ; then echo "Missing gettext. Rerun configure and check for" \
	"'checking whether to use NLS... yes'!" ; exit 1 ; fi

update-po: check-gettext
	@find $(srcdir)/src/ -name "*.cc" -print | sort > $(srcdir)/po/POTFILES.in.2 ; \
	if diff $(srcdir)/po/POTFILES.in $(srcdir)/po/POTFILES.in.2 >/dev/null 2>&1 ; then \
		rm -f $(srcdir)/po/POTFILES.in.2 ; \
	else \
		mv $(srcdir)/po/POTFILES.in.2 $(srcdir)/po/POTFILES.in ; \
	fi
	cd po && $(MAKE) $(AM_MAKEFLAGS) update-po

force-update-gmo: check-gettext
	touch po/*.po
	cd po && $(MAKE) $(AM_MAKEFLAGS) update-gmo

force-update-gmo-%: check-gettext
	@language=`echo $@ | sed s/force-update-gmo-//` ; \
	if test ! -f po/$$language.po ; then echo "file po/$$language.po does not exist" ; exit 1 ; fi ; \
	touch po/$$language.po ; \
	cd po && $(MAKE) $(AM_MAKEFLAGS) update-gmo

.PHONY: check-gettext update-po update-gmo force-update-gmo
