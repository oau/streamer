#!/bin/sh

echo Compiling cli.c...
gcc cli.c -c -I./include -o cli.o

echo Compiling cli_term.c...
gcc cli_term.c -c -I./include -o cli_term.o

echo Compiling oswrap.c...
gcc oswrap.c -c -I./include -o oswrap.o

echo Compiling srv.c...
gcc speech.c -c -I./include -o speech.o

echo Compiling utils.c...
gcc utils.c -c $CFLAGS -I./include -o utils.o

echo Linking...
gcc cli.o oswrap.o cli_term.o speech.o utils.o -L./lib-linux -lsam -l SDL -l avcodec -l avutil -l swscale -lz -lrcplug_cli -o bin/cli

echo Cleaning up...
rm *.o
#strip cli

echo Done!
