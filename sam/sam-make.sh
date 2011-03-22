#!/bin/sh

echo Compiling sam.c...
gcc sam.c -c -I ../include/sdl -I ./include

echo Compiling sammain.c...
gcc sammain.c -c -I ../include/sdl -I ./include

echo Linking...
gcc sam.o sammain.o -L -lSDLmain -lSDL -o bin/sam

echo Librarian...
ar rcs ../lib-linux/libsam.a sam.o

echo Cleaning up...
rm *.o
#sleep 1
#strip bin/sam.exe

echo Done!

