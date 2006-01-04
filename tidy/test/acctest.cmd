@echo off

REM execute all test cases of the accessibility test suite
REM
REM (c) 2006 (W3C) MIT, ERCIM, Keio University
REM See tidy.c for the copyright notice.
REM
REM <URL:http://tidy.sourceforge.net/>
REM
REM CVS Info:
REM
REM    $Author$
REM    $Date$
REM    $Revision$

echo Running ACCESS TEST suite > ACCERR.TXT
set FAILEDACC=
for /F "tokens=1,2*" %%i in (accesscases.txt) do call onetesta.cmd %%i %%j %%k
echo FAILED [%FAILEDACC%] ...