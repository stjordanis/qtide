rem run in Qt console
rem run qmake first and after .pro changes

c:
cd c:\jqt
del /Q bin\*.*
del /Q lib\build\*.*
del /Q main\build\*.*

cd c:\jqt\lib
call qmake
call make
copy /Y bin\* ..\bin

cd c:\jqt\main
call qmake
call make
copy /Y bin\* ..\bin

cd c:\jqt
copy /Y bin\jqt.dll "c:\j7\bin"
copy /Y bin\jqt.exet "c:\j7\bin"

dir "c:\j7\bin\jqt*"
