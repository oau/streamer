#!/bin/sh

CFLAGS="-ggdb"
LFLAGS="-ggdb"

echo Compiling capture.cpp...
g++ capture.cpp -c $CFLAGS -I./include -o capture.o

echo Compiling srv.c...
gcc srv.c -c  $CFLAGS -I./include -o srv.o 

echo Compiling srv.c...
gcc speech.c -c $CFLAGS -I./include -o speech.o 

echo Compiling oswrap.c...
gcc oswrap.c -c $CFLAGS -I./include -o oswrap.o

echo Linking...
g++ capture.o srv.o oswrap.o speech.o $LFLAGS -L./lib-linux -lsam -lSDL -lcv -lhighgui -lx264 -l swscale -l avutil -l cv -o bin/srv

echo Cleaning up...
rm *.o
#strip srv

echo Done!
