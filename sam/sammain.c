#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "sam.h"
#ifdef _WIN32
#include "windows.h"
#endif 

#define USESDL


#ifdef USESDL
	#include <SDL/SDL.h>
	#include <SDL/SDL_audio.h>
#endif

int bufferpos;
char buffer[ 22050 * 10 ];
char input[ 256 ];

static int scale=50;
static int singmode = 0;
static unsigned char speed = 72;
static unsigned char pitch = 64;
static unsigned char mouth = 128;
static unsigned char throat = 128;


void WriteWav(char* filename, unsigned char* buffer, int bufferlength)
{

        FILE *file = fopen(filename, "wb");
        if (file == NULL) return;
        //RIFF header
        fwrite("RIFF", 4, 1,file);
        unsigned int filesize=bufferlength + 12 + 16 + 8 - 8;
        fwrite(&filesize, 4, 1, file);
        fwrite("WAVE", 4, 1, file);

        //format chunk
        fwrite("fmt ", 4, 1, file);
        unsigned int fmtlength = 16;
        fwrite(&fmtlength, 4, 1, file);
        unsigned short int format=1; //PCM
        fwrite(&format, 2, 1, file);
        unsigned short int channels=1;
        fwrite(&channels, 2, 1, file);
        unsigned int samplerate = 7000;
        fwrite(&samplerate, 4, 1, file);
        fwrite(&samplerate, 4, 1, file); // bytes/second
        unsigned short int blockalign = 1;
        fwrite(&blockalign, 2, 1, file);
        unsigned short int bitspersample=8;
        fwrite(&bitspersample, 2, 1, file);

        //data chunk
        fwrite("data", 4, 1, file);
        fwrite(&bufferlength, 4, 1, file);
        fwrite(buffer, bufferlength, 1, file);

        fclose(file);

}

void printUsage() {
		printf("usage: sam [options] Word1 Word2 ....\n");
    printf("options\n");
    printf("	-phonetic 		enters phonetic mode. (see below)\n");
    printf("	-pitch number		set pitch value (default=64)\n");
    printf("	-speed number		set speed value (default=72)\n");
    printf("	-throat number		set mouth value (default=128)\n");
    printf("	-mouth number		set throat value (default=128)\n");
    printf("	-wav filename		output to wav instead of libsdl\n");
    printf("	-sing			special treatment of pitch\n");
    printf("	-debug			print additional debug messages\n");
    printf("\n");
    		
    printf("     VOWELS                            VOICED CONSONANTS	\n");
    printf("IY           f(ee)t                    R        red		\n");
    printf("IH           p(i)n                     L        allow		\n");
    printf("EH           beg                       W        away		\n");
    printf("AE           Sam                       W        whale		\n");
    printf("AA           pot                       Y        you		\n");
    printf("AH           b(u)dget                  M        Sam		\n");
    printf("AO           t(al)k                    N        man		\n");
    printf("OH           cone                      NX       so(ng)		\n");
    printf("UH           book                      B        bad		\n");
    printf("UX           l(oo)t                    D        dog		\n");
    printf("ER           bird                      G        again		\n");
    printf("AX           gall(o)n                  J        judge		\n");
    printf("IX           dig(i)t                   Z        zoo		\n");
    printf("				       ZH       plea(s)ure	\n");
    printf("   DIPHTHONGS                          V        seven		\n");
    printf("EY           m(a)de                    DH       (th)en		\n");
    printf("AY           h(igh)						\n");
    printf("OY           boy						\n");
    printf("AW           h(ow)                     UNVOICED CONSONANTS	\n");
    printf("OW           slow                      S         Sam		\n");
    printf("UW           crew                      Sh        fish		\n");
    printf("                                       F         fish		\n");
    printf("                                       TH        thin		\n");
    printf(" SPECIAL PHONEMES                      P         poke		\n");
    printf("UL           sett(le) (=AXL)           T         talk		\n");
    printf("UM           astron(omy) (=AXM)        K         cake		\n");
    printf("UN           functi(on) (=AXN)         CH        speech		\n");
    printf("Q            kitt-en (glottal stop)    /H        a(h)ead	\n");	
}

#ifdef USESDL

int pos = 0;
void MixAudio(void *unused, Uint8 *stream, int len){
	int i;
	if( pos >= bufferpos ) return;
	if( ( bufferpos - pos ) < len ) len = bufferpos - pos;
	for( i = 0; i < len; i++ ) {
		stream[ i ] = buffer[ pos ];
		pos++;
	}
}


void OutputSound() {
	bufferpos /= scale;	
  //extern void mixaudio(void *unused, Uint8 *stream, int len);
  SDL_AudioSpec fmt;

  fmt.freq = 22050;
  fmt.format = AUDIO_U8;
  fmt.channels = 1;
  fmt.samples = 2048;
  fmt.callback = MixAudio;
  fmt.userdata = NULL;

  /* Open the audio device and start playing sound! */
  if ( SDL_OpenAudio(&fmt, NULL) < 0 ) {
      printf("Unable to open audio: %s\n", SDL_GetError());
      exit(1);
  }
  SDL_PauseAudio(0);
  //SDL_Delay((bufferpos)/7);
  
  while( pos < bufferpos ) {
  	SDL_Delay( 100 );
  }

  SDL_CloseAudio();
	
}
#endif	



int main(int argc, char **argv)
{
	int debug=0;
	int phonetic=0;
	int wavfilenameposition=-1;
	unsigned char output[256] = "";
	
	if (argc <= 1)
	{
		printUsage();
		return 1;
	}

	input[0]=0;
	strcat(input, " ");

	int i = 1;
	while(i < argc)
	{
		if (argv[i][0] != '-')
		{
			strcat(input, argv[i]);
			strcat(input, " ");
		} else
		{
			if (strcmp(&argv[i][1], "wav")==0)
			{
				 wavfilenameposition = i+1;
				 i++;
			} else
			if (strcmp(&argv[i][1], "singmode")==0)
			{
				singmode = 1;
			} else
			if (strcmp(&argv[i][1], "phonetic")==0)
			{
				phonetic = 1;
			} else
			if (strcmp(&argv[i][1], "debug")==0)
			{
				debug = 1;
			} else
			if (strcmp(&argv[i][1], "pitch")==0)
			{
				pitch = atoi(argv[i+1]);
				i++;
			} else
			if (strcmp(&argv[i][1], "speed")==0)
			{
				speed = atoi(argv[i+1]);
				i++;
			} else
			if (strcmp(&argv[i][1], "mouth")==0)
			{
				mouth = atoi(argv[i+1]);
				i++;
			} else
			if (strcmp(&argv[i][1], "throat")==0)
			{
				throat = atoi(argv[i+1]);
				i++;
			} else
			{
				printUsage();
				return 1;
			}
		}
		
		i++;
	} //while
	
	if ((pitch == 0) || (speed == 0) || (mouth == 0) || (throat == 0))
	{
		printUsage();
		return 1;
	}

	strcat(input, " ");
	for(i=0; input[i] != 0; i++)
		input[i] = toupper(input[i]);

	if (debug)
	{
		printf("say: %s\n", input);
	}


	strcat(input, " \x1B");
	
	if (!phonetic) {
		if (sam_phenomes(input)<0) return;
		if (debug) printf("text translation: %s\n", output);
	}


	strcat(input, " \x9b\0");
	
#ifdef USESDL
	if ( SDL_Init(SDL_INIT_AUDIO) < 0 ) {
        	printf("Unable to init SDL: %s\n", SDL_GetError());
        	exit(1);
  }
	atexit(SDL_Quit);
#endif


	sam_params( scale, singmode, speed, pitch, mouth, throat );
	bufferpos = sizeof( buffer );
	if ( sam_speak( buffer, &bufferpos, input ) < 0 ) {
		printUsage();
		return 1;
	}
	
	if (wavfilenameposition > 0) {
		WriteWav(argv[wavfilenameposition], buffer, bufferpos/scale);
	} else {
#ifdef USESDL
		OutputSound();
#endif	
	}
	
	if (debug) sam_debug();	
	
	return 0;

}

#ifdef _WIN32
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