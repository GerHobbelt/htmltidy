# Makefile [Makefile.am] - for tidy - HTML parser and pretty printer
#
#  CVS Info :
#
#     $Author: creitzel $ 
#     $Date: 2003/03/19 18:37:37 $ 
#     $Revision: 1.3 $ 
#
#  Copyright (c) 1998-2003 World Wide Web Consortium
#  (Massachusetts Institute of Technology, European Research 
#  Consortium for Informatics and Mathematics, Keio University).
#  All Rights Reserved.
#
#  Contributing Author(s):
#
#     Dave Raggett <dsr@w3.org>
#     Terry Teague <terry_teague@users.sourceforge.net>
#     Pradeep Padala<ppadala@users.sourceforge.net>
#
#  The contributing author(s) would like to thank all those who
#  helped with testing, bug fixes, and patience.  This wouldn't
#  have been possible without all of you.
#
#  COPYRIGHT NOTICE:
#
#  This software and documentation is provided "as is," and
#  the copyright holders and contributing author(s) make no
#  representations or warranties, express or implied, including
#  but not limited to, warranties of merchantability or fitness
#  for any particular purpose or that the use of the software or
#  documentation will not infringe any third party patents,
#  copyrights, trademarks or other rights. 
#
#  The copyright holders and contributing author(s) will not be
#  liable for any direct, indirect, special or consequential damages
#  arising out of any use of the software or documentation, even if
#  advised of the possibility of such damage.
#
#  Permission is hereby granted to use, copy, modify, and distribute
#  this source code, or portions hereof, documentation and executables,
#  for any purpose, without fee, subject to the following restrictions:
#
#  1. The origin of this source code must not be misrepresented.
#  2. Altered versions must be plainly marked as such and must
#     not be misrepresented as being the original source.
#  3. This Copyright notice may not be removed or altered from any
#     source or altered source distribution.
# 
#  The copyright holders and contributing author(s) specifically
#  permit, without fee, and encourage the use of this source code
#  as a component for supporting the Hypertext Markup Language in
#  commercial products. If you use this source code in a product,
#  acknowledgment is not required but would be appreciated.
#

SUBDIRS = src console include test

#TODO: Pull man page from htmldoc
#installmanpage:
#	if [ -f "$(TOPDIR)/htmldoc/man_page.txt" ] ; then \
#    	if [ ! -d "$(maninst)/man1" ]; then mkdir -p "$(maninst)/man1"; fi; \
#    	cp -f $(TOPDIR)/htmldoc/man_page.txt "$(maninst)/man1/tidy.1"; \
#	fi

# [i_a] instruct AC_LOCAL to correctly include the ./m4 directory too
ACLOCAL_AMFLAGS = -I m4


SEVEN_ZIP_OPTIONS=a -t7z -m0=lzma -mx=9 -mfb=273 -md=64m -ms=on
# SEVEN_ZIP_OPTIONS=a -si




PACKAGE = @PACKAGE_TARNAME@
TAR_FILENAME_POSTFIX = @TAR_FILENAME_POSTFIX@
VER_SUFFIX = @VER_SUFFIX@
VERSION = @VERSION@
ARCHIVE_BASE_NAME = @PACKAGE_TARNAME@-@PACKAGE_VERSION@-@TAR_FILENAME_POSTFIX@

SILENT = @SILENT@

ECHO = echo
E = $(SILENT) $(ECHO)



#
# [i_a] WARNING: do NOT name these other than $(distdir).tar.* as other names, e.g.
#         $(ARCHIVE_BASE_NAME).src.tar.*
#       will NOT work as the generated makefile code cannot cope with this flexibility (yet).
#
#       It's that or I have overlooked a autoconf/automake/auto-whatever option... :-S
#
DIST_ARCHIVES =
if OS_HAS_TAR
if OS_HAS_GZIP
DIST_ARCHIVES += $(distdir).tar.gz
endif
if OS_HAS_BZIP2
DIST_ARCHIVES += $(distdir).tar.bz2
endif
if OS_HAS_COMPRESS
DIST_ARCHIVES += $(distdir).tar.Z
endif
endif






DISTCLEANFILES =
CLEANFILES = *~ *.bak
MAINTAINERCLEANFILES = configure.in aclocal.m4 Makefile.in



EXTRA_DIST = \
	m4/ac_func_memmove.m4 \
	m4/ac_func_snprintf.m4 m4/ax_arg_enable.m4 \
	m4/ax_cflags_warn_all.m4 \
	m4/ax_cflags_gcc_option.m4 \
	m4/ac___function___c99_vpredef_vars.m4 \
	m4/ax_cc_pragma_pack.m4 \
	config_vpath.sh \
	cleanup.bat bootstrap.sh CREDITS README.win32 BUGS \
	README.maintainer MAILINGLIST \
	build/gmake/* \
	build/gnuauto/* \
	build/rpm/* \
	build/msvc/*.vcproj \
	build/msvc/*.sln \
	build/msvc/*.def \
	build/msvc/*.dsp \
	build/msvc/*.dsw \
	build/msvc/*.vc6 \
	build/readme.txt \
	contrib/* \
	htmldoc/* \
	htmldoc/api/* \
	autogen.sh \
	create-distribs.bat \
	create-distribs.exclude \
	create-distribs.exe.exclude \
	create-distribs.src.exclude \
	fix_crlf.sh \
	readme.txt
    




if MAINTAINER_MODE

# ChangeLog: NEWS
# 	-( test "X$(cvs2cl)" != "X" && $(cvs2cl) --prune --accum -U ../CVSROOT/users )

endif






doc:
	VERSION=$(VERSION) ;            \
	export VERSION;                 \
	doxygen htmldoc/doxygen.cfg
#	@cp documentation/*.jpg documentation/html/
#	@sed -e "s/\$$VERSION/$(VERSION)/g" doxygen_manual.tex >../latex/doxygen_manual.tex
#	@sed -e "s/\$$VERSION/$(VERSION)/g" doxygen.sty >../latex/doxygen.sty
#	@epstopdf doxygen_logo.eps --outfile=../latex/doxygen_logo.pdf


html-dist: doc
	tar cvzf htmltidy-html-documentation.$(VERSION).tar.gz htmldoc/*






#
# with conditionals in automake-processed makefiles like these, pay SPECIAL attention ot
#   http://sources.redhat.com/ml/automake/2000-10/msg00108.html
# --> use AM_CONDITIONAL instead of makefile ifeq/ifneq/...
#

dist-gzip: distdir
	-rm -f $(ARCHIVE_BASE_NAME).src.tar.gz
if OS_HAS_TAR
if OS_HAS_GZIP
	$(TAR) -c $(distdir) | GZIP=$(GZIP_ENV) $(GZIP) -c >$(ARCHIVE_BASE_NAME).src.tar.gz
endif
endif
	$(am__remove_distdir)

dist-bzip2: distdir
	-rm -f $(ARCHIVE_BASE_NAME).src.tar.bz2
if OS_HAS_TAR
if OS_HAS_BZIP2
	$(TAR) -c $(distdir) | $(BZIP2) -9 -c >$(ARCHIVE_BASE_NAME).src.tar.bz2
endif
endif
	$(am__remove_distdir)

dist-lzma: distdir
	-rm -f $(ARCHIVE_BASE_NAME).src.tar.lzma
if OS_HAS_TAR
if OS_HAS_LZMA
	$(TAR) -c $(distdir) | $(LZMA) -9 -c >$(ARCHIVE_BASE_NAME).src.tar.lzma
endif
endif
	$(am__remove_distdir)

dist-tarZ: distdir
	-rm -f $(ARCHIVE_BASE_NAME).src.tar.Z
if OS_HAS_TAR
if OS_HAS_COMPRESS
	$(TAR) -c $(distdir) | $(COMPRESS) -c >$(ARCHIVE_BASE_NAME).src.tar.Z
endif
endif
	$(am__remove_distdir)

dist-7z: distdir
	-rm -f $(ARCHIVE_BASE_NAME).src.7z
if OS_HAS_SEVEN_ZIP
	$(SEVEN_ZIP) $(SEVEN_ZIP_OPTIONS) $(ARCHIVE_BASE_NAME).src.7z $(distdir)
endif
	$(am__remove_distdir)

dist-shar: distdir
	-rm -f $(ARCHIVE_BASE_NAME).src.shar.gz
if OS_HAS_SHAR
if OS_HAS_GZIP
	$(SHAR) $(distdir) | GZIP=$(GZIP_ENV) $(GZIP) -c >$(ARCHIVE_BASE_NAME).src.shar.gz
endif
endif
	$(am__remove_distdir)

dist-zip: distdir
	-rm -f $(ARCHIVE_BASE_NAME).zip
if OS_HAS_ZIP
	$(ZIP) -rq $(ARCHIVE_BASE_NAME).src.zip $(distdir)
endif
	$(am__remove_distdir)



dist-win32-bin: distdir
	$(SILENT) echo "Creating MSVC binary archives from `pwd`";
	-rm -f $(ARCHIVE_BASE_NAME).Win32.bin.7z
	-rm -f $(ARCHIVE_BASE_NAME).Win32.bin.zip
	-rm -f $(ARCHIVE_BASE_NAME).Win32.bin.tar.gz
	-rm -f $(ARCHIVE_BASE_NAME).Win32.bin.tar.bz2
	-mkdir build/msvc/bin
if OS_HAS_TAR
if OS_HAS_GZIP
	$(TAR) -c $(WIN32_BIN_EXTRA) `find build/msvc/bin/ -name '*.exe' -o -name '*.dll' ` | GZIP=$(GZIP_ENV) $(GZIP) -c >$(ARCHIVE_BASE_NAME).Win32.bin.tar.gz
endif
if OS_HAS_BZIP2
	$(TAR) -c $(WIN32_BIN_EXTRA) `find build/msvc/bin/ -name '*.exe' -o -name '*.dll' ` | $(BZIP2) -9 -c >$(ARCHIVE_BASE_NAME).Win32.bin.tar.bz2
endif
if OS_HAS_COMPRESS
endif
endif
if OS_HAS_SEVEN_ZIP
	$(SEVEN_ZIP) $(SEVEN_ZIP_OPTIONS) $(ARCHIVE_BASE_NAME).Win32.bin.7z $(WIN32_BIN_EXTRA) `find build/msvc/bin/ -name '*.exe' -o -name '*.dll' `
endif
if OS_HAS_ZIP
	$(ZIP) -rq $(ARCHIVE_BASE_NAME).Win32.bin.zip $(WIN32_BIN_EXTRA) `find build/msvc/bin/ -name '*.exe' -o -name '*.dll' `
endif


distribution: distdir dist-win32-bin
	$(SILENT) echo "Creating src archives";
	-rm -f $(ARCHIVE_BASE_NAME).src.tar.gz
	-rm -f $(ARCHIVE_BASE_NAME).src.tar.bz2
	-rm -f $(ARCHIVE_BASE_NAME).src.tar.lzma
	-rm -f $(ARCHIVE_BASE_NAME).src.tar.Z
	-rm -f $(ARCHIVE_BASE_NAME).src.7z
	-rm -f $(ARCHIVE_BASE_NAME).src.shar.gz
	-rm -f $(ARCHIVE_BASE_NAME).zip
if OS_HAS_TAR
if OS_HAS_GZIP
	$(TAR) -c $(distdir) | GZIP=$(GZIP_ENV) $(GZIP) -c >$(ARCHIVE_BASE_NAME).src.tar.gz
endif
if OS_HAS_BZIP2
	$(TAR) -c $(distdir) | $(BZIP2) -9 -c >$(ARCHIVE_BASE_NAME).src.tar.bz2
endif
if OS_HAS_LZMA
	$(TAR) -c $(distdir) | $(LZMA) -9 -c >$(ARCHIVE_BASE_NAME).src.tar.lzma
endif
if OS_HAS_COMPRESS
	$(TAR) -c $(distdir) | $(COMPRESS) -c >$(ARCHIVE_BASE_NAME).src.tar.Z
endif
endif
if OS_HAS_SEVEN_ZIP
	$(SEVEN_ZIP) $(SEVEN_ZIP_OPTIONS) $(ARCHIVE_BASE_NAME).src.7z $(distdir)
endif
if OS_HAS_SHAR
if OS_HAS_GZIP
	$(SHAR) $(distdir) | GZIP=$(GZIP_ENV) $(GZIP) -c >$(ARCHIVE_BASE_NAME).src.shar.gz
endif
endif
if OS_HAS_ZIP
	$(ZIP) -rq $(ARCHIVE_BASE_NAME).src.zip $(distdir)
endif
	$(am__remove_distdir)



#
# [i_a] see the comment at 'DIST_ARCHIVES' why we need to copy the archives to the 'regular'
#       $(distdir).tar.* name(s) here -- if we don't 'make distcheck' will fail very quickly.
#
dist dist-all: distribution



#
# when hook is called, configure has not yet run in the dist dir...
#
#
# EXTRA: if the 'bin' subdir does not exist for some reason, do not fail due to that omission; just create
# it. This is done to ensure the dist-win32 target builds within a 'make distcheck' run.
#
#dist-hook: dist-gzip dist-bzip2 dist-tarZ
# ^^^ you'll get an infinite loop when you add these dependencies!
dist-hook:
	cd $(top_distdir)
	$(E) "   ************************************"
	$(E) "   dist-hook"
	$(E) "   ************************************"
	-mkdir build/msvc/bin
	$(SILENT) echo "Copying the archive(s) to standard $(distdir).tar.xxx' names suitable for distcheck";
	-rm -f $(distdir).tar.gz
	-rm -f $(distdir).tar.bz2
	-rm -f $(distdir).tar.Z
if OS_HAS_TAR
if OS_HAS_GZIP
	$(LN_S) $(ARCHIVE_BASE_NAME).src.tar.gz $(distdir).tar.gz
endif
if OS_HAS_BZIP2
	$(LN_S) $(ARCHIVE_BASE_NAME).src.tar.bz2 $(distdir).tar.bz2
endif
if OS_HAS_COMPRESS
	$(LN_S) $(ARCHIVE_BASE_NAME).src.tar.Z $(distdir).tar.Z
endif
endif



clean-local:
	-rm -f $(distdir).*
	-rm -f $(ARCHIVE_BASE_NAME).*



distcheck-hook:
	$(E) "   ************************************"
	$(E) "   distcheck-hook!"
	$(E) "   ************************************"
	cd $(distdir)/_build \
		&& echo "rm -rf xyz"








.PHONY: depend dist-7z dist-maint dist-msvc distribution dist-all 


