@ECHO OFF

ECHO Compiling cli.c...
gcc cli.c -g -I ./include -I ./include/ffmpeg -c
IF ERRORLEVEL 1 GOTO ERROR

ECHO Compiling cli_term.c...
gcc cli_term.c -g -I ./include -c
IF ERRORLEVEL 1 GOTO ERROR

ECHO Compiling oswrap.c...
gcc oswrap.c -g -I ./include -c
IF ERRORLEVEL 1 GOTO ERROR

ECHO Compiling speech.c...
gcc speech.c -g -I ./include -I ./include/ffmpeg -c
IF ERRORLEVEL 1 GOTO ERROR

ECHO Resources...
windres cli-w32.rc -O coff -o cli.res

ECHO Linking...
gcc oswrap.o cli_term.o cli.o speech.o cli.res -g -I ./include -L ./lib-w32 -mwindows -lmingw32 -lsdlmain -lsdl -lavcodec -lavutil -lwsock32 -lmsvcrt -lswscale -lsam -o bin/cli.exe
IF ERRORLEVEL 1 GOTO ERROR

ECHO Cleaning up...
del *.o
del *.res
REM strip bin/cli.exe

ECHO Done!

:ERROR
