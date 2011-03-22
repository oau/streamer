#!/bin/sh

CFLAGS="-ggdb" 
LFLAGS="-ggdb" 
echo Compiling cam.cpp...
g++ cam.cpp -c $CFLAGS -o cam.o

echo Compiling srv.c...
gcc srv.c -c  $CFLAGS -o srv.o 

echo Compiling srv.c...
gcc sam_queue.c -c $CFLAGS -o sam_queue.o 

echo Compiling oswrap.c...
gcc oswrap.c -c $CFLAGS -o oswrap.o

echo Linking...
g++ cam.o srv.o oswrap.o sam_queue.o $LFLAGS -L./lib-linux -lsam -lSDL -lcv -lhighgui -lx264 -l swscale -l avutil -l cv -o bin/srv

echo Cleaning up...
rm srv.o
rm cam.o
sleep 1
#strip srv

#ECHO Done!

#:ERROR
