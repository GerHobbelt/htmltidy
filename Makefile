# Makefile - for tidy - HTML parser and pretty printer
#
#  CVS Info :
#
#     $Author$ 
#     $Date$ 
#     $Revision$ 
#
#  Copyright (c) 1998-2000 World Wide Web Consortium
#  (Massachusetts Institute of Technology, Institut National de
#  Recherche en Informatique et en Automatique, Keio University).
#  All Rights Reserved.
#
#  Contributing Author(s):
#
#     Dave Raggett <dsr@w3.org>
#     Terry Teague <terry_teague@users.sourceforge.net>
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

CC= gcc

INCLDIR= ./include/
SRCDIR= ./src/
OBJDIR= ./

DEBUGFLAGS=-g -DDMALLOC
CFLAGS= -I $(INCLDIR)
OTHERCFLAGS=
LIBS=-lc
DEBUGLIBS=-ldmalloc

INSTALLDIR= /usr/local/
MANPAGESDIR= /usr/local/man/

OFILES=		$(OBJDIR)attrs.o         $(OBJDIR)istack.o        $(OBJDIR)parser.o        $(OBJDIR)tags.o \
		$(OBJDIR)entities.o      $(OBJDIR)lexer.o         $(OBJDIR)pprint.o        $(OBJDIR)clean.o \
		$(OBJDIR)localize.o      $(OBJDIR)config.o        $(OBJDIR)tidy.o

CFILES=		$(SRCDIR)attrs.c         $(SRCDIR)istack.c        $(SRCDIR)parser.c        $(SRCDIR)tags.c \
		$(SRCDIR)entities.c      $(SRCDIR)lexer.c         $(SRCDIR)pprint.c        $(SRCDIR)clean.c \
		$(SRCDIR)localize.c      $(SRCDIR)config.c        $(SRCDIR)tidy.c

HFILES=		$(INCLDIR)platform.h $(INCLDIR)html.h

tidy:		$(OFILES)
		$(CC) $(CFLAGS) $(OTHERCFLAGS) -o tidy $(OFILES) $(LIBS)

$(OFILES):	$(HFILES) Makefile
		$(CC) $(CFLAGS) $(OTHERCFLAGS) $(SRCDIR)$*.c -c
                
tab2space:	$(SRCDIR)tab2space.c
		$(CC) $(CFLAGS) $(OTHERCFLAGS) -o tab2space $(SRCDIR)tab2space.c $(LIBS)

all:		tidy tab2space

debug:
	@$(MAKE) CFLAGS='$(CFLAGS) $(DEBUGFLAGS)' LIBS='$(LIBS) $(DEBUGLIBS)' all

clean:
		rm -f $(OFILES) tab2space.o  tidy tab2space

install: tidy
	mkdir -p $(INSTALLDIR)bin
	cp -f tidy $(INSTALLDIR)bin
	if [ -f "man_page.txt" ] ; then \
		mkdir -p $(MANPAGESDIR)man1; \
		cp -f man_page.txt $(MANPAGESDIR)man1/tidy.1; \
	fi
	-cd $(INSTALLDIR)bin; \
	chmod 755 tidy; \
	chgrp bin tidy; \
	chown bin tidy;
