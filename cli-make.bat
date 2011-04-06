@ECHO OFF

SETLOCAL
SET CFLAGS=-g
SET LFLAGS=-g
SET STRIP=NO

ECHO Compiling cli.c...
gcc cli.c %CFLAGS% -I ./include -I ./include/ffmpeg -c
IF ERRORLEVEL 1 GOTO ERROR

ECHO Compiling cli_term.c...
gcc cli_term.c %CFLAGS% -I ./include -c
IF ERRORLEVEL 1 GOTO ERROR

ECHO Compiling oswrap.c...
gcc oswrap.c %CFLAGS% -I ./include -c
IF ERRORLEVEL 1 GOTO ERROR

ECHO Compiling speech.c...
gcc speech.c %CFLAGS% -I ./include -I ./include/ffmpeg -c
IF ERRORLEVEL 1 GOTO ERROR

ECHO Compiling utils.c...
gcc utils.c -c %CFLAGS% -I./include

ECHO Compiling plugins/kiwiray/...
gcc plugins/kiwiray/cli.c -c %CFLAGS% -I./include -I./plugins -o kiwiray_cli.o
IF ERRORLEVEL 1 GOTO ERROR

ECHO Resources...
windres cli-w32.rc -O coff -o cli.res

ECHO Linking...
g++ oswrap.o cli_term.o cli.o speech.o utils.o kiwiray_cli.o cli.res %LFLAGS% -I ./include -L ./lib-w32 -mwindows -lmingw32 -lsdlmain -lsdl -lavcodec -lavutil -lwsock32 -lmsvcrt -lswscale -lsam -o bin/cli.exe
g++ oswrap.o cli_term.o cli.o speech.o utils.o kiwiray_cli.o cli.res %LFLAGS% -I ./include -L ./lib-w32                               -lsdl -lavcodec -lavutil -lwsock32 -lmsvcrt -lswscale -lsam -o bin/cli_nosdl.exe
IF ERRORLEVEL 1 GOTO ERROR

ECHO Cleaning up...
del *.o
del *.res
IF %STRIP%==YES strip bin/cli.exe

ECHO Done!

:ERROR

ENDLOCAL

IF "%1"=="-run" GOTO RUN

GOTO FINAL

:RUN
ECHO Running application...
cd bin
cli
cd ..
GOTO FINAL

:FINAL