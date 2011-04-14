@ECHO OFF

SETLOCAL
SET CFLAGS=-g
SET LFLAGS=-g
SET STRIP=NO

ECHO Compiling plugins/ipv4udp/...
gcc ipv4udp/srv.c -c %CFLAGS% -I../include -I. -o ipv4udp_srv.o
IF ERRORLEVEL 1 GOTO ERROR
gcc ipv4udp/cli.c -c %CFLAGS% -I../include -I. -o ipv4udp_cli.o
IF ERRORLEVEL 1 GOTO ERROR

ECHO Compiling plugins/kiwiray/...
gcc kiwiray/srv.c -c %CFLAGS% -I../include -I. -o kiwiray_srv.o
IF ERRORLEVEL 1 GOTO ERROR
gcc kiwiray/cli.c -c %CFLAGS% -I../include -I. -o kiwiray_cli.o
IF ERRORLEVEL 1 GOTO ERROR

ECHO Compiling plugins/monitor/...
gcc monitor/srv.c -c %CFLAGS% -I../include -I. -o monitor_srv.o
IF ERRORLEVEL 1 GOTO ERROR
gcc monitor/cli.c -c %CFLAGS% -I../include -I. -o monitor_cli.o
IF ERRORLEVEL 1 GOTO ERROR

ECHO Librarian...
ar rcs ..\lib-w32\librcplug_srv.a ipv4udp_srv.o kiwiray_srv.o monitor_srv.o
IF ERRORLEVEL 1 GOTO ERROR
ar rcs ..\lib-w32\librcplug_cli.a ipv4udp_cli.o kiwiray_cli.o monitor_cli.o
IF ERRORLEVEL 1 GOTO ERROR

ECHO Cleaning up...
del *.o
IF %STRIP%==YES strip ..\lib-w32\librcplug_srv.a
IF %STRIP%==YES strip ..\lib-w32\librcplug_cli.a

ECHO Done!

:ERROR

ENDLOCAL