#!/bin/sh

CFLAGS="-ggdb"
LFLAGS="-ggdb"

echo Compiling capture.cpp...
g++ capture.cpp -c $CFLAGS -I./include -o capture.o

echo Compiling srv.c...
gcc srv.c -c  $CFLAGS -I./include -o srv.o 

echo Compiling speech.c...
gcc speech.c -c $CFLAGS -I./include -o speech.o 

echo Compiling oswrap.c...
gcc oswrap.c -c $CFLAGS -I./include -o oswrap.o

echo Compiling utils.c...
gcc utils.c -c $CFLAGS -I./include -o utils.o

echo Compiling plugins/kiwiray/...
gcc plugins/kiwiray/srv.c -c $CFLAGS -I./include -I./plugins -o kiwiray_srv.o

echo Linking...
g++ capture.o srv.o oswrap.o speech.o utils.o kiwiray_srv.o $LFLAGS -L./lib-linux -lsam -lSDL -lcv -lhighgui -lx264 -l swscale -l avutil -l cv -o bin/srv

echo Cleaning up...
rm *.o
#strip srv

echo Done!
