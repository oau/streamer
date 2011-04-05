@ECHO OFF

SETLOCAL
SET CFLAGS=-g
SET LFLAGS=-g
SET STRIP=NO

ECHO Compiling capture.cpp...
g++ capture.cpp -c %CFLAGS% -I./include
IF ERRORLEVEL 1 GOTO ERROR

ECHO Compiling srv.c...
gcc srv.c -c %CFLAGS% -I./include -I./include/ffmpeg
IF ERRORLEVEL 1 GOTO ERROR

ECHO Compiling oswrap.c...
gcc oswrap.c -c %CFLAGS% -I./include
IF ERRORLEVEL 1 GOTO ERROR

ECHO Compiling speech.c...
gcc speech.c -c %CFLAGS% -I./include -I./include/ffmpeg
IF ERRORLEVEL 1 GOTO ERROR

ECHO Compiling plugins/kiwiray/...
gcc plugins/kiwiray/srv.c -c %CFLAGS% -I./include -I./plugins -o kiwiray_srv.o
IF ERRORLEVEL 1 GOTO ERROR

ECHO Linking...
g++ oswrap.o capture.o srv.o speech.o kiwiray_srv.o %LFLAGS% -L ./lib-w32                               -lsdl -lkernel32 -lwsock32 -lx264 -lmsvcrt -lswscale -lavutil -lvideoinput -lddraw -ldxguid -lole32 -loleaut32 -lstrmiids -luuid -lsam -o bin/srv.exe
g++ oswrap.o capture.o srv.o speech.o kiwiray_srv.o %LFLAGS% -L ./lib-w32 -mwindows -lmingw32 -lsdlmain -lsdl -lkernel32 -lwsock32 -lx264 -lmsvcrt -lswscale -lavutil -lvideoinput -lddraw -ldxguid -lole32 -loleaut32 -lstrmiids -luuid -lsam -o bin/srv_sdl.exe
IF ERRORLEVEL 1 GOTO ERROR

ECHO Cleaning up...
del *.o
IF %STRIP%==YES strip bin/srv.exe
IF %STRIP%==YES strip bin/srv_sdl.exe

ECHO Done!

:ERROR

ENDLOCAL