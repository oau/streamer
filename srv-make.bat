@ECHO OFF

SETLOCAL
SET CFLAGS=-g
SET LFLAGS=-g
SET STRIP=NO

ECHO Compiling cam.cpp...
g++ cam.cpp %CFLAGS% -I ./include -c
IF ERRORLEVEL 1 GOTO ERROR

ECHO Compiling srv.c...
gcc srv.c %CFLAGS% -I ./ -I ./include -I ./include/ffmpeg -c
IF ERRORLEVEL 1 GOTO ERROR

ECHO Compiling oswrap.c...
gcc oswrap.c %CFLAGS% -I ./include -c
IF ERRORLEVEL 1 GOTO ERROR

ECHO Compiling speech.c...
gcc speech.c %CFLAGS% -I ./include -I ./include/ffmpeg -c
IF ERRORLEVEL 1 GOTO ERROR

ECHO Linking...
g++ oswrap.o cam.o srv.o speech.o %LFLAGS% -L ./lib-w32                               -lsdl -lkernel32 -lwsock32 -lx264 -lmsvcrt -lswscale -lavutil -lvideoinput -lddraw -ldxguid -lole32 -loleaut32 -lstrmiids -luuid -lsam -o bin/srv.exe
g++ oswrap.o cam.o srv.o speech.o %LFLAGS% -L ./lib-w32 -mwindows -lmingw32 -lsdlmain -lsdl -lkernel32 -lwsock32 -lx264 -lmsvcrt -lswscale -lavutil -lvideoinput -lddraw -ldxguid -lole32 -loleaut32 -lstrmiids -luuid -lsam -o bin/srv_sdl.exe
IF ERRORLEVEL 1 GOTO ERROR

ECHO Cleaning up...
del *.o
IF %STRIP%==YES strip bin/srv.exe
IF %STRIP%==YES strip bin/srv_sdl.exe

ECHO Done!

:ERROR

ENDLOCAL