@ECHO OFF

ECHO Compiling sam.c...
gcc sam.c -c -I ..\include\sdl -I .\include
IF ERRORLEVEL 1 GOTO ERROR

ECHO Compiling sammain.c...
gcc sammain.c -c -I ..\include\sdl -I .\include
IF ERRORLEVEL 1 GOTO ERROR

ECHO Linking...
gcc sam.o sammain.o -L ../lib-w32 -lmingw32 -lsdlmain -lsdl -mwindows -o bin\sam.exe
IF ERRORLEVEL 1 GOTO ERROR

ECHO Librarian...
ar rcs ..\lib-w32\libsam.a sam.o
IF ERRORLEVEL 1 GOTO ERROR

ECHO Cleaning up...
del *.o
SLEEP 1
strip bin/sam.exe

ECHO Done!

:ERROR
