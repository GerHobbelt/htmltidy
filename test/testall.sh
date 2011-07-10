#! /bin/sh

#
# testall.sh - execute all testcases for regression testing
#
# (c) 1998-2003 (W3C) MIT, ERCIM, Keio University
# See tidy.c for the copyright notice.
#
# <URL:http://tidy.sourceforge.net/>
#
# CVS Info:
#
#    $Author$
#    $Date$
#    $Revision$
#
# set -x

VERSION='$Id'

while read bugNo expected
do
#  echo Testing $bugNo | tee -a testall.log
  ./testone.sh $bugNo $expected "$@" | tee -a testall.log
done < testcases.txt

