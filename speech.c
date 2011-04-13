#include <math.h>
#include <SDL/SDL.h>
#include "include/sam/sam.h"

struct sam_list_t {
	char phenomes[ 256 ];
	struct sam_list_t *next;
};
typedef struct sam_list_t sam_list_t;

// SAM linked-list and mutex
static sam_list_t *sam_first = NULL;
static sam_list_t *sam_last = NULL;
static  SDL_mutex *sam_mx;

// SAM buffers
static char          sam_buffer[ 22050 * 10 ];
static int           sam_size;
static int           sam_pos;
static int           sam_speaking = 0;

// Visualization
static unsigned char vis_buffer[ 160 ];
static unsigned char vis_mul[ 160 ];
static int  do_vis = 0;
static int ctr = 0;

static int opened;

int speech_vis( unsigned char **buffer ) {
	do_vis = 1;
	if( sam_speaking == 1 ) {
		*buffer = vis_buffer;
		return( 0 );
	}
	return( -1 );
}

static void sdl_mixer( void *unused, Uint8 *stream, int len ) {
	int i;
	float f,dc;
	if( sam_pos >= sam_size ) {
    if( sam_speaking == 1 ) sam_speaking = 2;
	} else {
		if( ( sam_size - sam_pos ) < len ) len = sam_size - sam_pos;
		for( i = 0; i < len; i++ ) {
			stream[ i ] = sam_buffer[ sam_pos++ ];
		  sam_pos++;
		}
		if( do_vis ) {
			f = stream[ 0 ];
			dc = f;
			vis_buffer[ 0 ] = ( ( ( int )( f - dc ) ) * vis_mul[ i ] ) >> 8;
		  if( len > 160 ) len = 160;
			for( i = 1; i < len; i++ ) {
				f += ( ( ( float )stream[ i << 1 ] ) - f ) / 10;
				dc += ( f - dc ) / 20;
				vis_buffer[ i ] = ( ( ( int )( f - dc ) ) * vis_mul[ i ] ) >> 8;
			}
			for( ; i < 160; i++ ) {
				f += ( ( ( float )0 ) - f ) / 10;
				dc += ( f - dc ) / 20;
				vis_buffer[ i ] = ( ( ( int )( f - dc ) ) * vis_mul[ i ] ) >> 8;
			}
		}
	}
}

void speech_open() {
  int n;
  float m;
  sam_mx = SDL_CreateMutex();
  SDL_AudioSpec fmt;
  fmt.freq     = 11025;
  fmt.format   = AUDIO_U8;
  fmt.channels = 1;
  fmt.samples  = 1024;
  fmt.callback = sdl_mixer;
  fmt.userdata = NULL;
  for( n = 0; n < 160; n++ ) {
    vis_mul[ n ] = ( unsigned char )( ( 1.0 - cos( ( ( float )n ) / 80.0 * 3.1415f ) ) * 127 );
  }
  if ( SDL_OpenAudio( &fmt, NULL ) < 0 ) {
    fprintf( stderr, "SAM [error]: Unable to open SDL audio\n" );
  } else {
    printf( "SAM [info]: Initialized\n" );
    opened = 1;
  }
}

static void sam_play() {
	sam_size    /= SAM_SCALE;
  sam_pos      = 0;
  sam_speaking = 1;
	SDL_PauseAudio( 0 );
}

void speech_poll() {
  sam_list_t *p_sam;
	if( sam_speaking == 2 ) {
    SDL_PauseAudio( 1 );
    sam_speaking = 0;
	}
	if( sam_speaking == 0 ) {
    SDL_mutexP( sam_mx );
		if( sam_first ) {
  		// Queue pop
      p_sam = sam_first;
      sam_first = p_sam->next;
      SDL_mutexV( sam_mx );
  		// Set parameters
  		sam_params( SAM_SCALE, SAM_SING, SAM_SPEED, SAM_PITCH, SAM_MOUTH, SAM_THROAT );
  		sam_size = sizeof( sam_buffer );
  		if( sam_speak( sam_buffer, &sam_size, p_sam->phenomes ) == 0 ) sam_play();
  		free( p_sam );
		} else {
      SDL_mutexV( sam_mx );
		}
	}
}

void speech_queue( char* speak ) {
  sam_list_t *p_sam = malloc( sizeof( sam_list_t ) );

  if( !opened ) return;
    
  if( p_sam == NULL ) {
    printf( "WHAT THE HELL? NO MEMORY?\n" );
    return;
  }

  // Copy and convert to phenomes
  memset( p_sam->phenomes, 0, 256 );
  strcpy( p_sam->phenomes, speak );
  sam_phenomes( p_sam->phenomes );
  strcat( p_sam->phenomes, " \x9b\0" ); // TODO: is this really necessary?

 	// Queue push
  SDL_mutexP( sam_mx );
	p_sam->next = NULL;
	if( sam_first ) {
		sam_last->next = p_sam;
	} else {
		sam_first = p_sam;
	}  
	sam_last = p_sam;
  SDL_mutexV( sam_mx );
	
	speech_poll();

}

void speech_free() {
  SDL_DestroyMutex( sam_mx );
}

int speech_state() {
	return( sam_speaking );
}