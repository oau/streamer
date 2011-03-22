#include <SDL/SDL.h>
#include "include/sam/sam.h"

#define VIS_SIZE 640

struct sam_list_t {
	char phenomes[ 256 ];
	struct sam_list_t *next;
};
typedef struct sam_list_t sam_list_t;

// SAM linked-list
static sam_list_t *sam_first = NULL;
static sam_list_t *sam_last = NULL;

// SAM buffers
static unsigned char sam_buffer[ 22050 * 10 ];
static int           sam_size;
static int           sam_pos;
static int           sam_speaking = 0;

// Visualization
static unsigned char vis_buffer[ VIS_SIZE ];
static int  do_vis = 0;
static int ctr = 0;
int sam_vis( unsigned char **buffer ) {
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
			vis_buffer[ 0 ] = ( char )( f - dc );
		  if( len > VIS_SIZE ) len = VIS_SIZE;
			for( i = 1; i < len; i++ ) {
				f += ( ( ( float )stream[ i ] ) - f ) / 10;
				dc += ( f - dc ) / 20;
				vis_buffer[ i ] = ( char )( f - dc );
			}
			for( ; i < VIS_SIZE; i++ ) {
				f += ( ( ( float )0 ) - f ) / 10;
				dc += ( f - dc ) / 20;
				vis_buffer[ i ] = ( char )( f - dc );
			}
		}
	}
}

void sam_open() {
  SDL_AudioSpec fmt;
  fmt.freq     = 11025;
  fmt.format   = AUDIO_U8;
  fmt.channels = 1;
  fmt.samples  = 22050/25;
  fmt.callback = sdl_mixer;
  fmt.userdata = NULL;
  if ( SDL_OpenAudio( &fmt, NULL ) < 0 ) {
    printf( "SAM: Unable to open SDL audio\n" );
  }
  printf( "SAM: Initialized\n" );
}

static void sam_play() {
	sam_size    /= SAM_SCALE;
  sam_pos      = 0;
  sam_speaking = 1;
	SDL_PauseAudio( 0 );
}

void sam_poll() {
  sam_list_t *p_sam;
	if( sam_speaking == 2 ) {
    SDL_PauseAudio( 1 );
    sam_speaking = 0;
	}
	if( sam_speaking == 0 ) {
		if( sam_first ) {
  		// Set parameters
  		sam_params( SAM_SCALE, SAM_SING, SAM_SPEED, SAM_PITCH, SAM_MOUTH, SAM_THROAT );
  		sam_size = sizeof( sam_buffer );
  		if( sam_speak( sam_buffer, &sam_size, sam_first->phenomes ) == 0 ) sam_play();
  		
  		// Queue pop
  		p_sam = sam_first->next;
  		free( sam_first );
  		sam_first = p_sam;
		}
	}
}

void sam_queue( char* speak ) {
  sam_list_t *p_sam = malloc( sizeof( sam_list_t ) );
  // Copy and convert to phenomes
  memset( p_sam->phenomes, 0, 256 );
  strcpy( p_sam->phenomes, speak );
  sam_phenomes( p_sam->phenomes );
  strcat( p_sam->phenomes, " \x9b\0" ); // Is this really necessary?

 	// Queue push
	p_sam->next = NULL;
	if( sam_first ) {
		sam_last->next = p_sam;
	} else {
		sam_first = p_sam;
	}  
	sam_last = p_sam;
	
	sam_poll();

}

int sam_state() {
	return( sam_speaking );
}
