#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include "include/oswrap.h"
#include <libswscale/swscale.h>
#include <x264.h>
#include "include/kiwiray.h"
#include <SDL/SDL.h>
#include "include/sam_queue.h"
#ifdef _WIN32
#include <shellapi.h>
#endif

// Audio
//#define DISABLE_SPEECH          // Disables SAM

// Protocol
#define KIWI_VERSION          2 // Protocol revision
#define DEFAULT_PORT       6979 // Default port
#define MAX_CLIENTS          10 // Max number of clients allowed in the quuee

// Capture (device may not be capable and return another size)
#define CAP_WIDTH           640 // Requested capture width
#define CAP_HEIGHT          480 // Requested capture height

// Streaming
#define STREAM_WIDTH        640 // Width of streamed video
#define STREAM_HEIGHT       480 // Height of streamed video
#define STREAM_FPS           25 // FPS of streamed video

// Timeouts
#define TIMEOUT_CONNECTION   50 // In frames, see STREAM_FPS
#define TIMEOUT_CONTROL    7500 // In frames, see STREAM_FPS

// Client data
struct client_t {
  NET_ADDR client;
  int timeout;
  int timer;
  unsigned char extra_id;
  struct client_t *prev;
  struct client_t *next;
  int index;
  ctrl_data_t ctrl;
  unsigned char extra_data;
};
typedef struct client_t client_t;

// Sockets
static NET_SOCK h_sock;
static NET_ADDR srv_addr;
static NET_ADDR cli_addr;

// Locals
static int quit     = 0; // Time to quit (SIGINT etc.)
static int do_intra = 0; // Time to intra-refresh (New client connected)

// Clients linked-list array
static client_t  clients[ MAX_CLIENTS ];
static client_t *client_first = NULL;
static client_t *client_last  = NULL;

// Initialize client data
static void clients_init() {
  int n;
  for( n = 0; n < MAX_CLIENTS; n++ ) {
    clients[ n ].timeout = 0;
    clients[ n ].index   = n;
  }
}

// Find client index
static client_t *clients_find( NET_ADDR *client ) {
  int n;
  for( n = 0; n < MAX_CLIENTS; n++ ) {
    if( clients[ n ].timeout ) {
      if( net_addr_get( &clients[ n ].client ) == net_addr_get( client ) ) {
        if( net_port_get( &clients[ n ].client ) == net_port_get( client ) ) {
          return( &clients[ n ] );
        }
      }
    }
  }
  return( NULL );
}

// Attach client, return index or -1 if queue is full
static client_t *clients_attach( NET_ADDR *client ) {
  int n;
  
  // Iterate client table
  for( n = 0; n < MAX_CLIENTS; n++ ) {
  	
  	// Find free client entry
    if( !clients[ n ].timeout ) {
    	
    	// Attach client to table
      printf( "KiwiRayServer [info]: Client %i connected\n", n );
      memcpy( &clients[ n ].client, client, sizeof( NET_ADDR ) );
      clients[ n ].next = NULL;
      clients[ n ].prev = client_last;
      clients[ n ].extra_id = 0xFF;
      if( client_first ) {
        client_last->next = &clients[ n ];
      } else {
        client_first = &clients[ n ];
        do_intra = 1; // Intra-refresh needed
      }
      client_last = &clients[ n ];

      clients[ n ].timeout = TIMEOUT_CONNECTION;
      clients[ n ].timer   = TIMEOUT_CONTROL;
      
      return( &clients[ n ] );
    }
  }
  return( NULL );
}

// Return queue time for specific client into buf[ offset ]
static char * queue_time( char buf[], int offset, client_t *p_client ) {
  client_t *cli = client_first;
  int total = 0;
  while( cli ) {
    if( cli == p_client ) break;
    total += cli->timer;
    cli = cli->next;
  }
  *( int* )&buf[ offset ] = total;
  return( buf );
}

// Count down all client timers, kill active client if it's timer reaches zero
static void clients_tick() {
  int n;
  
  // Count down timeouts
  for( n = 0; n < MAX_CLIENTS; n++ ) {
  	if( clients[ n ].timeout ) {
  		clients[ n ].timeout--;
  		if( clients[ n ].timeout == 0 ) {
        printf( "KiwiRayServer [info]: Client disconnected (ping timeout)\n", client_first->index );
  			if( clients[ n ].prev ) clients[ n ].prev->next = clients[ n ].next;
  			if( clients[ n ].next ) clients[ n ].next->prev = clients[ n ].prev;
  		}
  	}
 	}
  	
  if( client_first ) {
  	
  	// Count down timer
    if( --client_first->timer == 0 || client_first->timeout == 0 ) {
    	
    	// Time for client change
      printf( "KiwiRayServer [info]: Client disconnected (time up)\n", client_first->index );
      client_first = client_first->next;
      if( client_first ) {
      	client_first->prev = NULL;
	      do_intra = 1; // Intra-refresh needed
      }
      
    }    
  }
}

void extradata_handler( char* data, int size ) {
  data[ size ] = 0;
#ifndef DISABLE_SPEECH
  sam_queue( data );
#endif
}

int receiver( void *unused ) {
  int temp, size;
  client_t *p_client;
  char buffer[ 4096 ];
  while( 1 ) {
    SDL_Delay( 1 );
    temp = sizeof( NET_ADDR );
    size = net_recv( &h_sock, buffer, 32768, &cli_addr );
    
    // Find attached client
    p_client = clients_find( &cli_addr );
    if( p_client ) {
      if( size >= 4 ) {
        if( memcmp( buffer, "HELO", 4 ) == 0 ) {
          // Re-send HELO+version+time
          buffer[ 4 ] = KIWI_VERSION;
          net_send( &h_sock, queue_time( buffer, 5, p_client ), 5 + sizeof( int ), &cli_addr );
        } else if( memcmp( buffer, "TIME", 4 ) == 0 ) {
          // Request queue time
          net_send( &h_sock, queue_time( buffer, 4, p_client ), 4 + sizeof( int ), &cli_addr );
          // Send TIME+time
          p_client->timeout = TIMEOUT_CONNECTION;
        } else if( memcmp( buffer, "QUIT", 4 ) == 0 ) {
          // Abort connectionj
          p_client->timeout = 0;
        } else if( memcmp( buffer, "CTRL", 4 ) == 0 ) {
          // Copy control data
          if( size >= 4 + sizeof( ctrl_data_t ) ) {
            p_client->timeout = TIMEOUT_CONNECTION;
            memcpy( &p_client->ctrl, buffer + 4, sizeof( ctrl_data_t ) );
            if( size > 4 + sizeof( ctrl_data_t ) ) {
              if( ( ( p_client->extra_id + 1 ) & 0xFF ) == buffer[ 4 + sizeof( ctrl_data_t ) ] ) {
                p_client->extra_id++;
                extradata_handler( buffer + 5 + sizeof( ctrl_data_t ), size - 5 - sizeof( ctrl_data_t ) );
              }
            }
          }
          // TODO: handle
        }
      }
    } else {
      // Client not attached
      if( size == 4 ) {
        if( memcmp( buffer, "HELO", 4 ) == 0 ) {
          // Handshake, attach
          p_client = clients_attach( &cli_addr );
          if( p_client ) {
            // Connection accepted, send HELO+version+time
            buffer[ 4 ] = KIWI_VERSION;
            net_send( &h_sock, queue_time( buffer, 5, p_client ), 5 + sizeof( int ), &cli_addr );
          } else {
            // Server is full, send FULL
            net_send( &h_sock, "FULL", 4, &cli_addr );
          }
        } else {
          // Unknown connection, send LOST
          net_send( &h_sock, "LOST", 4, &cli_addr );
        }
      }
    }
  }
}

static void terminate( int z ) {
  printf( "\nKiwiRayServer [info]: SIGINT received, shutting down...\n\n" );
  quit = 1;
}

int main( int argc, char *argv[] ) {
  int n;
	int cap_w, cap_h;
  int stride;
  int port = DEFAULT_PORT;
  const uint8_t *data;
  x264_param_t param;
  struct SwsContext* convertCtx;
  x264_picture_t pic_in, pic_out;
  x264_nal_t* nals;
  int i_nals;
  int frame_size;
  unsigned char p_buffer[ 65536 ] __attribute__ ((aligned));
  unsigned int i_buffer;
  int pl, pm, pt = 0;
  int nalc = 0, nalb = 0;
  SDL_Thread *hReceiver;
  disp_data_t disp;

  if(argc < 2){
      printf("usage: %s <camdevice>\n",argv[0]);
      return 0;
  }

  printf( "KiwiRayServer [info]: OHAI!\n\n" );

  clients_init();
  
	signal( SIGINT, terminate );

  // Initialize network
  if( net_init() < 0 ) {
    printf( "KiwiRayServer [error]: Network initialization failed\n" );
    return( 1 );
  } else {
  	
    // Aquire socket
    if( net_sock( &h_sock ) < 0 ) {
      printf( "KiwiRayServer [error]: Socket aquire failed\n" );
      return( 2 );
    } else {

		  // Port from args
		  if( argc > 2 ) port = atoi( argv[ 2 ] );
    	
      // Bind socket to PORT
      net_addr_init( &srv_addr, NET_ADDR_ANY, port );
      if( net_bind( &h_sock, &srv_addr ) < 0 ) {
        printf( "KiwiRayServer [error]: Socket bind failed\n" );
        return( 3 );
      }
    }
  }
  net_addr_init( &cli_addr, NET_ADDR_ANY, 0 );

  SDL_Init( SDL_INIT_AUDIO );

  // Initialize video capture device
  cap_w = CAP_WIDTH;
  cap_h = CAP_HEIGHT;
  if( argc > 1 ) {
    if( cam_init( argv[ 1 ], STREAM_FPS, &cap_w, &cap_h ) < 0 ) {
      printf( "KiwiRayServer [error]: Unable to open capture device %s\n", argv[ 1 ] );
      return( 1 );
    }
    if( cap_w != CAP_WIDTH || cap_h != CAP_HEIGHT ) {
      printf( "KiwiRayServer [warning]: Captured video is %ix%i, not %ix%i\n\n", cap_w, cap_h, CAP_WIDTH, CAP_HEIGHT );
    }
  } else {
    cam_init( NULL, STREAM_FPS, &cap_w, &cap_h );
    return( 0 );
  }

  // Initialize encoder
  x264_param_default_preset( &param, "medium", "zerolatency" );
  
  param.i_width   = STREAM_WIDTH;
  param.i_height  = STREAM_HEIGHT;
  param.i_fps_num = STREAM_FPS;
  
  x264_param_parse( &param, "slice-max-size", "8192" ); /* Practically disables slicing.
  																											   Slicing is the splitting of frame data into
  																											   a series of NALs, each having a maximum size
  																											   so that they can be transported over
  																											   interfaces that has a limited packet size/MTU */
  
  x264_param_parse( &param, "vbv-maxrate", "500" );     /* Set VBV mode and max bitrate (kbps).
  																												 VBV is variable bitrate, which means the rate
  																												 will vary depending on how complex the scene
  																												 is at the moment - detail, motion, etc. */
  
  x264_param_parse( &param, "vbv-bufsize", "30" );		  /* Enable single-frame VBV.
  																												 This will cap all frames so that they only
  																												 contain a maximum amount of information,
  																												 which in turn means that each frame can
  																												 always be sent in one packet and packetss
  																												 will be of a much more unform size. */
  
  x264_param_parse( &param, "crf", "20" );							/* Constant Rate Factor.
  																												 Tells VBV to target a specific quality. */
  
  x264_param_parse( &param, "intra-refresh", NULL );		/* Enable intra-refresh.
  																												 Intra-refresh allows single-frame VBV to
  																												 work well by disabling I-frames and
  																												 replacing them with a periodic refresh
  																												 that scans across the image, refreshing
  																												 only a smaller portion each frame.
  																												 I-frames can be seen as a full-frame
  																												 refresh that needs no other data to decode
  																												 the picture, and takes a lot more space
  																												 than P/B (differential) frames. */
  
  x264_param_parse( &param, "no-cabac", NULL );					/* Disable CABAC.
  																												 It's been said it's unsuitable for this
  																												 type of H.264 application. */
  
  param.b_annexb = 1;																		/* Use Annex-B packaging.
  																											   This appends a marker at the start of
  																											   each NAL unit. */
  
  param.i_frame_reference = 1;													/* Needed for intra-refresh. */

  x264_param_apply_profile( &param, "high" );						/* Apply HIGH profile.
  																												 Allows for better compression, but needs
  																												 to be supported by the decoder. We use
  																												 FFMPEG, which does support this profile. */

  // Open encoder
  x264_t* encoder = x264_encoder_open( &param );
  
  // Allocate I420 picture
  x264_picture_alloc( &pic_in, X264_CSP_I420, STREAM_WIDTH, STREAM_HEIGHT );

  // Initialize color space convertor
  convertCtx = sws_getContext( cap_w, cap_h, PIX_FMT_BGR24, STREAM_WIDTH, STREAM_HEIGHT, PIX_FMT_YUV420P, SWS_FAST_BILINEAR, NULL, NULL, NULL );
  if( !convertCtx ) {
    printf( "KiwiRayServer [error]: Cannot initialize the conversion context" );
    return( 3 );
  } 

  stride = cap_w * 3; // RGB stride, width * 3

  // Create receiving thread
  //thr_create( &hReceiver, &iReceiver, receiver );
  hReceiver = SDL_CreateThread( receiver, NULL );

  printf( "\nKiwiRayServer [info]: listening on port %i...\n", port );

#ifndef DISABLE_SPEECH
  sam_open();
  sam_queue( "INITIALIZED AND READY FOR CONNECTION" );
#endif

  while( !quit ) {
  	
  	sam_poll();
    
    // Fetch latest picture from capture device
    data = ( uint8_t * )cam_fetch();

    // Convert to YUV I420
    sws_scale( convertCtx, &data, &stride, 0, cap_h, pic_in.img.plane, pic_in.img.i_stride );

    // Encode frame    
    if( do_intra ) {
      do_intra = 0;
      x264_encoder_intra_refresh( encoder );
    }
    frame_size = x264_encoder_encode( encoder, &nals, &i_nals, &pic_in, &pic_out );

    // Iterate NALs
    pl = 0;
    pm = 0;
    i_buffer = 0;
    for( n = 0; n < i_nals; n++ ) {
      
      // Track payload sizes
      pl += nals[ n ].i_payload;
      if( nals[ n ].i_payload > pm ) pm = nals[ n ].i_payload;

      // Concatenate into a linear buffer
      memcpy( p_buffer + i_buffer, nals[ n ].p_payload, nals[ n ].i_payload );
      i_buffer += nals[ n ].i_payload;

      // Total counters
      nalc += 1;
      nalb += nals[ n ].i_payload;
    }
    
    // Largest packet
    if( pl > pt ) pt = pl;

    // Client connected?
    if( client_first ) {
    	
    	// Send H.264 frame
      net_send( &h_sock, p_buffer, i_buffer, &client_first->client );
      
      // Send DATA+disp_data
      memcpy( p_buffer, "DATA", 4 );
      disp.timer    = client_first->timer;
      disp.extra_id = client_first->extra_id;
      memcpy( p_buffer + 4, &disp, sizeof( disp_data_t ) );
      net_send( &h_sock, p_buffer, 4 + sizeof( disp_data_t ), &client_first->client );
    }

    // Get roughly 25fps
    SDL_Delay( 1000 / STREAM_FPS );

    // Tick client timers
    clients_tick();
  }
  
  // Cleanup
  sws_freeContext( convertCtx );
  x264_picture_clean( &pic_in );
  x264_encoder_close( encoder );
  SDL_Quit();
  cam_close();


  printf( "KiwiRayServer [info]: NAL units: %i, %i bytes\n", nalc, nalb );
  printf( "KiwiRayServer [info]: Largest packet: %i\n\n", pt );

  printf( "KiwiRayServer [info]: KTHXBYE!\n" );

  return( 0 );
}

#include "sdl_console.c"
