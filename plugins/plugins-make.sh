#!/bin/sh

CFLAGS="-ggdb"

mkdir ../lib-linux

echo Compiling plugins/ipv4udp/...
gcc ipv4udp/srv.c -c $CFLAGS -I../include -I. -o ipv4udp_srv.o
gcc ipv4udp/cli.c -c $CFLAGS -I../include -I. -o ipv4udp_cli.o

echo Compiling plugins/kiwiray/...
gcc kiwiray/srv.c -c $CFLAGS -I../include -I. -o kiwiray_srv.o
gcc kiwiray/cli.c -c $CFLAGS -I../include -I. -o kiwiray_cli.o

echo Compiling plugins/monitor/...
gcc monitor/srv.c -c $CFLAGS -I../include -I. -o monitor_srv.o
gcc monitor/cli.c -c $CFLAGS -I../include -I. -o monitor_cli.o

echo Librarian...
ar rcs ..\lib-linux\librcplug_srv.a ipv4udp_srv.o kiwiray_srv.o monitor_srv.o
ar rcs ..\lib-linux\librcplug_cli.a ipv4udp_cli.o kiwiray_cli.o monitor_cli.o

echo Cleaning up...
rm *.o
#strip ..\lib-linux\librcplug_srv.a
#strip ..\lib-linux\librcplug_cli.a

echo Done!

:ERROR

ENDLOCAL