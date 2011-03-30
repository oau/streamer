@ECHO OFF

ECHO Compiling cam.cpp...
g++ cam.cpp -g -I ./include -c
IF ERRORLEVEL 1 GOTO ERROR

ECHO Compiling srv.c...
gcc srv.c -g -I ./ -I ./include -I ./include/ffmpeg -c
IF ERRORLEVEL 1 GOTO ERROR

ECHO Compiling oswrap.c...
gcc oswrap.c -g -I ./include -c
IF ERRORLEVEL 1 GOTO ERROR

ECHO Compiling speech.c...
gcc speech.c -g -I ./include -I ./include/ffmpeg -c
IF ERRORLEVEL 1 GOTO ERROR

ECHO Linking...
g++ oswrap.o cam.o srv.o speech.o -g -L ./lib-w32 -lkernel32 -lwsock32 -lx264 -lmsvcrt -lswscale -lavutil -lvideoinput -lddraw -ldxguid -lole32 -loleaut32 -lstrmiids -luuid -lsdl -lsam -o bin/srv.exe
g++ oswrap.o cam.o srv.o speech.o -g -L ./lib-w32 -DUSESDL -mwindows -lmingw32 -lsdlmain -lsdl -lkernel32 -lwsock32 -lx264 -lmsvcrt -lswscale -lavutil -lvideoinput -lddraw -ldxguid -lole32 -loleaut32 -lstrmiids -luuid -lsam -o bin/srv_sdl.exe
IF ERRORLEVEL 1 GOTO ERROR

ECHO Cleaning up...
del *.o
REM strip bin/srv.exe
REM strip bin/srv_sdl.exe

ECHO Done!

:ERROR