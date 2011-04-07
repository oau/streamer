#!/bin/sh

echo Compiling cli.c...
gcc cli.c -c -I./include -o cli.o

echo Compiling cli_term.c...
gcc cli_term.c -c -I./include -o cli_term.o

echo Compiling oswrap.c...
gcc oswrap.c -c -I./include -o oswrap.o

echo Compiling srv.c...
gcc speech.c -c -I./include -o speech.o

echo Compiling plugins/ipv4udp/...
gcc plugins/ipv4udp/cli.c -c $CFLAGS -I./include -I./plugins -o ipv4udp_cli.o

echo Compiling plugins/kiwiray/...
gcc plugins/kiwiray/cli.c -c $CFLAGS -I./include -I./plugins -o kiwiray_cli.o

echo Compiling utils.c...
gcc utils.c -c $CFLAGS -I./include -o utils.o

echo Linking...
gcc cli.o oswrap.o cli_term.o speech.o utils.o ipv4udp_cli.o kiwiray_cli.o -L./lib-linux -lsam -l SDL -l avcodec -l avutil -l swscale -lz -o bin/cli

echo Cleaning up...
rm *.o
#strip cli

echo Done!
