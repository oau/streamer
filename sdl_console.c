// Makes application an SDL console application instead of a windowed application
// Useful for applications that need SDL_audio or other non-graphical subsystems
// Used instead of -mwindows -lmingw32 -lsdlmain
#ifdef _WIN32

int  main( int argc, char *argv[] );

PCHAR* CommandLineToArgvA( PCHAR CmdLine, int* _argc ) {
  PCHAR *argv;
  PCHAR _argv;
  ULONG  len;
  CHAR   a;
  ULONG  i, j;
  ULONG  argc = 0;

  BOOLEAN in_QM = FALSE;
  BOOLEAN in_TEXT = FALSE;
  BOOLEAN in_SPACE = TRUE;

  len = strlen( CmdLine );
  i = ( ( len + 2 ) / 2 ) * sizeof( PVOID ) + sizeof( PVOID );

  argv = ( PCHAR* )malloc( i + ( len + 2 ) * sizeof( CHAR ) );
  _argv = ( PCHAR )( ( ( PUCHAR ) argv ) + i );
  argv[ 0 ] = _argv;

  i = 0;
  j = 0;

  while( a = CmdLine[i] ) {
    if( in_QM ) {
      if( a == '\"' ) {
        in_QM = FALSE;
      } else {
        _argv[ j ] = a;
        j++;
      }
    } else {
      switch( a ) {
	      case '\"':
          in_QM = TRUE;
          in_TEXT = TRUE;
          if( in_SPACE ) {
              argv[ argc ] = _argv + j;
              argc++;
          }
          in_SPACE = FALSE;
          break;
	      case ' ':
	      case '\t':
	      case '\n':
	      case '\r':
          if( in_TEXT ) {
            _argv[ j ] = '\0';
            j++;
          }
          in_TEXT = FALSE;
          in_SPACE = TRUE;
          break;
	      default:
          in_TEXT = TRUE;
          if( in_SPACE ) {
              argv[ argc ] = _argv + j;
              argc++;
          }
          _argv[ j ] = a;
          j++;
          in_SPACE = FALSE;
          break;
      }
    }
    i++;
  }
  _argv[ j ] = '\0';
  argv[ argc ] = NULL;

  *_argc = argc;
	return( argv );
}

int CALLBACK WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow ) {
	LPSTR *argv;
	int argc;
	int ret;
	LPSTR lpCmdLine2;
	
	lpCmdLine2 = malloc( strlen( lpCmdLine ) + 5 );
	strcpy( lpCmdLine2, "srv " );
	strcat( lpCmdLine2, lpCmdLine );

	argv = CommandLineToArgvA( lpCmdLine2, &argc );
	if ( argv == NULL ) {
		printf( "Console [error]: Command line parsing failed\n" );
		return( 5 );
	}
	
	ret = main( argc, argv );

	free( lpCmdLine2 );
	free( argv );
	return( ret );
}

#endif