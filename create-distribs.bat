rem
rem create distribution files for www.hebbut.net
rem
                          
set name=libhtmltidy-0.99.0.GerH-20090206

rmdir /q /s  .\distrib
mkdir .\distrib

pushd .\distrib

del *.bak
del *.exe 
del *.tar
del *.bz2

popd

                                                       
rem create 7z files for the source distros, etc.

7z a -r -ssw -y -x@create-distribs.exclude -x@create-distribs.src.exclude .\distrib\%name%.full-src.7z *
7z a -r -ssw -y -x@create-distribs.exclude -x@create-distribs.exe.exclude  .\distrib\%name%.bin-win32.7z *.exe *.dll build\msvc\bin\*.* ChangeLog* ReadMe* COPYING* CREDITS* NEWS* AUTHORS*
                                                          
                                                          
