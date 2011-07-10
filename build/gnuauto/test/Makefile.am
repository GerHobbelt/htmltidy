## Process this file with automake to produce Makefile.in

## $Id: Makefile.am,v 1.7 2004/08/18 13:26:30 vanbaal Exp $

## This script is free software; you can distribute it and/or modify it
## under the terms of the GNU GPL.  See the file COPYING.

## Copyright (C) 2003, 2004 Joost van Baal
## Copyright (C) 2007-2009 Ger Hobbelt <ger@hobbelt.com>


DISTCLEANFILES =
CLEANFILES = *~ *.bak $(refdir)/*~ $(refdir)/*.bak


#if OS_HAS_GNU_TIME
#TIMER          = @GNU_TIME@ -v
#else
#TIMER          = @TIME@
#endif

TIDY           = @abs_top_builddir@/console/tidy






SILENT = @SILENT@




testdir = $(datadir)/input
refdir = $(srcdir)/output




ECHO = echo
E = $(SILENT) $(ECHO)





test_SCRIPTS = \
	testall.sh


EXTRA_DIST = \
	testone.sh \
	*.cmd \
    	accessTest\* \
    	input\* \
    	output\* \
    	*.txt \
    	*.sh 
	




# TESTS = test_basics test_other 
TESTS =

check_PROGRAMS =

XFAIL_TESTS =
#	exectest_test




test: test_basics test_other 


tidy_binaries: 
	(cd $(top_builddir)/src && $(MAKE) $(AM_MAKEFLAGS) all);
	


check-local:                    \
		test_basics             \
		test_other 
	$(E) "*******************************************"
	$(E) "*               TESTING DONE!"
	$(E) "*******************************************"







cleanup_test:
	-rm -rf *.temp.*


clean-local: cleanup_test



#
# because automake doesn't produce a 'make check' rule that I want...
#
# original code:
# check-am: all-am
#	$(MAKE) $(AM_MAKEFLAGS) $(check_PROGRAMS)
#	$(MAKE) $(AM_MAKEFLAGS) check-TESTS check-local
#
# Le Hack:
#
#check-am: all-am
#	$(SILENT)\
#	srcdir=$(srcdir); export srcdir; \
#	builddir=$(builddir); export builddir; \
#	$(MAKE) $(AM_MAKEFLAGS) $(check_PROGRAMS); \
#	$(MAKE) $(AM_MAKEFLAGS) check-TESTS check-local; \
#	$(MAKE) $(AM_MAKEFLAGS) cleanup_test


.PHONY:




test_basics:
	$(SILENT)\
	./testall.sh

    
test_other:
	$(SILENT)\
	./testaccess.sh


    
