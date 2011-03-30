#!/bin/sh

echo Compiling cli.c...
gcc cli.c -c -o cli.o

echo Compiling cli_term.c...
gcc cli_term.c -c -o cli_term.o

echo Compiling oswrap.c...
gcc oswrap.c -c -o oswrap.o

echo Compiling srv.c...
gcc speech.c -c $CFLAGS -o speech.o

echo Linking...
gcc cli.o oswrap.o cli_term.o speech.o -L./lib-linux -lsam -l SDL -l avcodec -l avutil -l swscale -lz -o bin/cli

echo Cleaning up...
rm *.o
sleep 1
#strip cli

echo Done!

#:ERROR
