#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include "include/oswrap.h"
#include <libswscale/swscale.h>
#include <x264.h>
#include "include/kiwiray.h"
#include <SDL/SDL.h>
#include "include/speech.h"
#include <stdlib.h>
#ifdef _WIN32
#include <shellapi.h>
#include <windows.h>
#ifndef USESDL
#include "sdl_console.c"
#endif
#endif

// Audio
//#define DISABLE_SPEECH          // Disables speech

// Save the stream
//#define SAVE_STREAM           "server.h264"

// Movement
#define ROT_DZN               0 // Dead-zone
#define ROT_ACC              10 // Acceleration
#define ROT_DMP               3 // Dampening
#define ROT_SEN               1 // Sensitivity

#define MOV_ACC               5 // Acceleration
#define MOV_BRK              10 // Breaking

#define CAM_SEN             0.3 // Sensitivity

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
#define STREAM_FPS           25 // FPS of streamed video (also requested capture FPS)

// Timeouts (in frames, see STREAM_FPS)
#define TIMEOUT_CONNECTION  100 // Before connection is closed if no data has arrived
#define TIMEOUT_CONTROL    7500 // Before control session is ended
#define TIMEOUT_TRUST         8 // Before retransmitting trusted packets
#define TIMEOUT_GLITCH        2 // Before robot stops moving if a connection glitches/is lost

// Client data
struct client_t {
  NET_ADDR         client;
  int              timeout;
  int              timer;
  int              glitch;
  unsigned char    trust_cli;
  unsigned char    trust_srv;
  struct client_t *prev;
  struct client_t *next;
  int              index;
  ctrl_data_t      ctrl;
  int              got_first;
  ctrl_t           last;
  ctrl_t           diff;
  unsigned char    trust_data;
};
typedef struct client_t client_t;

enum exitcode_e {
  EXIT_OK,
  EXIT_NETWORK,
  EXIT_SOCKET,
  EXIT_BIND,
  EXIT_CAPTURE,
  EXIT_SWSCALE,
  EXIT_PICTURE,
  EXIT_AUDIO
};

// Linked buffer
typedef struct {
  char data[ 8192 ];
  int size;
  void* next;
} linked_buf_t;

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
static SDL_mutex *client_mx;

// Trusted data linked_list & mutex
static linked_buf_t *trust_first = NULL;
static linked_buf_t *trust_last = NULL;
static int trust_timeout;
static SDL_mutex *trust_mx;

// Packets
static char pkt_data[ 4 ] = "DATA";
static char pkt_helo[ 4 ] = "HELO";
static char pkt_time[ 4 ] = "TIME";
static char pkt_ctrl[ 4 ] = "CTRL";
static char pkt_lost[ 4 ] = "LOST";
static char pkt_full[ 4 ] = "FULL";
static char pkt_quit[ 4 ] = "QUIT";

// Drive parameters
static          char drive_x; // Strafe
static          char drive_y; // Move
static          char drive_r; // Turn
static unsigned int drive_p; // Pitch
static          long integrate_r = 0;

// Encoding
x264_t* encoder;
x264_picture_t pic_in, pic_out;
struct SwsContext* convertCtx;

// Threads
SDL_Thread    *hReceiver;
SDL_Thread    *hKiwiray;

// Serial
static char *p_serdev = NULL;

// Emoticons
static unsigned char emoticon;

// Queues a packet for trusted (non-lossy) transmission
static void trust_queue( void* data, unsigned char size ) {
  linked_buf_t *p_trust;
  p_trust = malloc( sizeof( linked_buf_t ) );
  if( p_trust ) {
    // Create packet
    memcpy( p_trust->data, data, size );
    p_trust->size = size;
    p_trust->next = NULL;
    // Insert into linked list
    SDL_mutexP( trust_mx );
    if( trust_first ) {
      trust_last->next = p_trust;
    } else {
      trust_first = p_trust;
    }
    trust_last = p_trust;
    SDL_mutexV( trust_mx );
  }
}

// Handles trusted data packets
void trust_handler( client_t *p_client, char* data, int size ) {
  unsigned char n;
  if( size == 0 ) return;
  data[ size ] = 0;
  p_client->trust_cli++;
  printf( "KiwiRayServer [info]: Client %i says: %s\n", p_client->index, data );  
  //trust_queue( data, size ); // TODO: remove
  for( n = 0; n < size - 1; n++ ) {
    if( data[ n ] == ':' ) {
      switch( data[ n + 1 ] ) {
        case ')':
          emoticon = 2;
          break;
        case '(':
          emoticon = 3;
          break;
      }
      data[ n ] = ' ';
      data[ n + 1 ] = ' ';
    }
  }
#ifndef DISABLE_SPEECH
  speech_queue( data );
#endif
}

// Clear trusted queue
static void trust_clear() {
  linked_buf_t *p_trust;
  SDL_mutexP( trust_mx );
  while( trust_first ) {
    p_trust = trust_first->next;
    free( trust_first );
    trust_first = p_trust;
  }
  SDL_mutexV( trust_mx );
  trust_timeout = 0;
}

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
  client_t *p_ret = NULL;
  SDL_mutexP( client_mx );
  for( n = 0; n < MAX_CLIENTS; n++ ) {
    if( clients[ n ].timeout ) {
      if( net_addr_get( &clients[ n ].client ) == net_addr_get( client ) ) {
        if( net_port_get( &clients[ n ].client ) == net_port_get( client ) ) {
          p_ret = &clients[ n ];
          break;
        }
      }
    }
  }
  SDL_mutexV( client_mx );
  return( p_ret );
}

// Add client, return index or -1 if queue is full
static client_t *clients_add( NET_ADDR *client ) {
  int n;
  client_t *p_ret = NULL;
  // Iterate client table
  SDL_mutexP( client_mx );
  for( n = 0; n < MAX_CLIENTS; n++ ) {
  	// Find free client entry
    if( !clients[ n ].timeout ) {
    	// Add client to table
      printf( "KiwiRayServer [info]: Client %i connected\n", n );
      memcpy( &clients[ n ].client, client, sizeof( NET_ADDR ) );
      clients[ n ].next = NULL;
      clients[ n ].prev = client_last;
      clients[ n ].trust_cli = 0xFF;
      clients[ n ].trust_srv = 0x00;
      clients[ n ].got_first = 0;
      clients[ n ].timeout = TIMEOUT_CONNECTION;
      clients[ n ].timer   = TIMEOUT_CONTROL;
      if( client_first ) {
        client_last->next = &clients[ n ];
      } else {
        do_intra = 1; // Intra-refresh needed
        trust_clear();
        client_first = &clients[ n ];
        emoticon = 1;
      }
      client_last = &clients[ n ];
      p_ret = &clients[ n ];
      break;
    }
  }
  SDL_mutexV( client_mx );
  return( p_ret );
}

static void clients_diff( client_t *p_client ) {
  p_client->diff.mx = p_client->ctrl.ctrl.mx - p_client->last.mx;
  p_client->diff.my = p_client->ctrl.ctrl.my - p_client->last.my;
  p_client->diff.kb = p_client->ctrl.ctrl.kb ^ p_client->last.kb;
  memcpy( &p_client->last, &p_client->ctrl.ctrl, sizeof( ctrl_t ) );
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
  SDL_mutexP( client_mx );
  for( n = 0; n < MAX_CLIENTS; n++ ) {
  	if( clients[ n ].timeout ) {
  		clients[ n ].timeout--;
  		if( clients[ n ].timeout == 0 ) {
        printf( "KiwiRayServer [info]: Client %i disconnected (ping timeout)\n", n );
  			if( clients[ n ].prev ) clients[ n ].prev->next = clients[ n ].next;
  			if( clients[ n ].next ) clients[ n ].next->prev = clients[ n ].prev;
  		}
  	}
 	}
  if( client_first ) {
  	// Count down timer
    if( client_first->glitch ) client_first->glitch--;
    if( --client_first->timer == 0 || client_first->timeout == 0 ) {
    	// Time for client switch
      printf( "KiwiRayServer [info]: Client %i disconnected (time up)\n", client_first->index );
      client_first = client_first->next;
    	emoticon = 0;
      if( client_first ) {
        emoticon = 1;
      	client_first->prev = NULL;
	      do_intra = 1; // Intra-refresh needed
      }
	    trust_clear();
    }    
  }
  SDL_mutexV( client_mx );
}

// Thread handles KiwiRay communications
int kiwiray( void *unused ) {
  int b_working = 0;
  unsigned char n;
  unsigned char emotilast = 255;
  char p_pkt[ 64 ] = {  0xFF, 0x00, 0x00, 0x00, 0x00, 0x00 };
  const unsigned char emotidata[ 4 ][ 24 ] = {
    {
      0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b00011000, 0b00000000,
      0b00000000, 0b00011000, 0b00000000,
      0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b00000000, 0b00000000
    }, {
      0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b00000000, 0b00000000,
      0b00111100, 0b00111100, 0b00111100,
      0b00100100, 0b00100100, 0b00100100,
      0b00100100, 0b00100100, 0b00100100,
      0b00111100, 0b00111100, 0b00111100,
      0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b00000000, 0b00000000
    }, {
      0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b11100111, 0b00000000,
      0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b10000001, 0b00000000,
      0b00000000, 0b01000010, 0b00000000,
      0b00000000, 0b00111100, 0b00000000,
      0b00000000, 0b00000000, 0b00000000
    }, {
      0b00000000, 0b00000000, 0b00000000,
      0b01000010, 0b00000000, 0b00000000,
      0b00100100, 0b00000000, 0b00000000,
      0b00000000, 0b00000000, 0b00000000,
      0b00000000, 0b00000000, 0b00000000,
      0b00111100, 0b00000000, 0b00000000,
      0b01000010, 0b00000000, 0b00000000,
      0b00000000, 0b00000000, 0b00000000
    }
  };

  // Initial serial startup
  b_working = ( serial_open( p_serdev ) == 0 );
  if( !b_working ) {
    printf( "KiwiRayServer [warning]: Unable to open %s, disabling serial\n", p_serdev );
    return( 0 );
  }
  b_working = !serial_params( "115200,n,8,1" );
  if( !b_working ) {
    printf( "KiwiRayServer [warning]: Unable to configure %s, disabling serial\n", p_serdev );
    serial_close();
    return( 0 );
  }
  while( 1 ) {
    // Re-open on errors
    if( !b_working ) {
      printf( "KiwiRayServer [warning]: Serial port problem, re-opening...\n" );
      SDL_Delay( 5000 );
      serial_close();
      b_working = ( serial_open( p_serdev ) == 0 );
      if( b_working ) b_working = serial_params( "115200,n,8,1" );
    }
    p_pkt[ 1 ] = 0x00;               // Drive XYZ
    p_pkt[ 2 ] = -drive_x;           // Strafe X
    p_pkt[ 3 ] = -drive_y;           // Move   Y
    p_pkt[ 4 ] =  drive_r;           // Rotate R
    p_pkt[ 5 ] =  drive_p * CAM_SEN; // Look   Pitch
    p_pkt[ 6 ] =  6;                 // Stepsize = 1:2^6
    if( b_working) b_working = ( serial_write( p_pkt, 7 ) == 7 );
    if( emotilast != emoticon ) {
      p_pkt[ 1 ] = 0x33;             // Display
      p_pkt[ 2 ] = 23;               // 8x8x3 bits (-1)
      for( n = 0; n < 24; n++ ) {
        p_pkt[ 3 + n ] = emotidata[ emoticon ][ n ];
      }
      //memcpy( &p_pkt[ 3 ], emotidata[ emoticon ], 24 );
      if( b_working) b_working = ( serial_write( p_pkt, 27 ) == 27 );
      emotilast = emoticon;
    }
    SDL_Delay( 20 ); // roughly 50 times second
  }
}

// Thread handles reception of UDP packets
int receiver( void *unused ) {
  int size;
  client_t *p_client;
  char buffer[ 8192 ];
  while( 1 ) {
    net_addr_init( &cli_addr, NET_ADDR_ANY, 0 ); // TODO: is this really necessary?
    size = net_recv( &h_sock, buffer, 8192, &cli_addr );
    // Find client
    p_client = clients_find( &cli_addr );
    if( p_client ) {
      if( size >= 4 ) {
        if( memcmp( buffer, pkt_helo, 4 ) == 0 ) {
          // Re-send HELO+version+time
          buffer[ 4 ] = KIWI_VERSION;
          net_send( &h_sock, queue_time( buffer, 5, p_client ), 5 + sizeof( int ), &cli_addr );
        } else if( memcmp( buffer, pkt_time, 4 ) == 0 ) {
          // Send TIME+time
          net_send( &h_sock, queue_time( buffer, 4, p_client ), 4 + sizeof( int ), &cli_addr );
          SDL_mutexP( client_mx );
          p_client->timeout = TIMEOUT_CONNECTION;
          SDL_mutexV( client_mx );
        } else if( memcmp( buffer, pkt_quit, 4 ) == 0 ) {
          // Abort connection
          SDL_mutexP( client_mx );
          p_client->timeout = 0;
          SDL_mutexV( client_mx );
        } else if( memcmp( buffer, pkt_ctrl, 4 ) == 0 ) {
          // Copy control data
          if( size >= 4 + sizeof( ctrl_data_t ) ) {
            SDL_mutexP( client_mx );
            p_client->timeout = TIMEOUT_CONNECTION;
            p_client->glitch = TIMEOUT_GLITCH;
            SDL_mutexV( client_mx );
            memcpy( &p_client->ctrl, buffer + 4, sizeof( ctrl_data_t ) );
            // Initial control data, reset diff
            if( !p_client->got_first ) {
              p_client->got_first = 1;
              memcpy( &p_client->last, &p_client->ctrl.ctrl, sizeof( ctrl_t ) );
            }
            // Check if outgoing trusted data recieved, free trusted buffers
            SDL_mutexP( trust_mx );
            if( trust_first ) {
              if( p_client->ctrl.trust_srv == p_client->trust_srv ) {
                linked_buf_t* p_trust = trust_first;
                trust_first = trust_first->next;
                p_client->trust_srv++;
                trust_timeout = 0;
                free( p_trust );
              }
            }
            SDL_mutexV( trust_mx );
            // Handle incoming trusted data
            if( ( ( p_client->trust_cli + 1 ) & 0xFF ) == p_client->ctrl.trust_cli ) {
              trust_handler( p_client, buffer + 4 + sizeof( ctrl_data_t ), size - 4 - sizeof( ctrl_data_t ) );
            }
          }
        }
      }
    } else {
      // Client unknown
      if( size >= 4 ) {
        if( memcmp( buffer, pkt_helo, 4 ) == 0 ) {
          // Handshake, add
          p_client = clients_add( &cli_addr );
          if( p_client ) {
            // Connection accepted, send HELO+version+time
            buffer[ 4 ] = KIWI_VERSION;
            net_send( &h_sock, queue_time( buffer, 5, p_client ), 5 + sizeof( int ), &cli_addr );
          } else {
            // Server is full, send FULL
            net_send( &h_sock, pkt_full, 4, &cli_addr );
          }
        } else {
          // Unknown connection, send LOST
          net_send( &h_sock, pkt_lost, 4, &cli_addr );
        }
      }
    }
  }
}

void terminate( int z ) {
  printf( "\nKiwiRayServer [info]: SIGINT received, shutting down...\n\n" );
  quit = 1;
}

// Cleanup
void encoder_close()   { x264_encoder_close( encoder );                }
void picture_close()   { x264_picture_clean( &pic_in );                }
void sws_close()       { sws_freeContext( convertCtx );                }
void trust_mx_close()  { SDL_DestroyMutex( trust_mx );                 }
void client_mx_close() { SDL_DestroyMutex( client_mx );                }
void kiwiray_close()   { SDL_KillThread( hKiwiray );                   }
void receiver_close()  { SDL_KillThread( hReceiver );                  }
void close_message()   { printf( "KiwiRayServer [info]: KTHXBYE!\n" ); }

int main( int argc, char *argv[] ) {
  int            n;
	int            cap_w, cap_h;
  int            stride;
  int            port = DEFAULT_PORT;
  const uint8_t *data;
  x264_param_t   param;
  x264_nal_t    *nals;
  int            i_nals;
  int            frame_size;
  unsigned char  p_buffer[ 65536 ] __attribute__ ((aligned));
  unsigned int   i_buffer;
  int            pl, pm, pt = 0;
  int            nalc = 0, nalb = 0;
  disp_data_t    disp;
  int            temp;
  //Uint32         ms_diff, ms_last;
  Uint32         time_target;
  Sint32         time_diff;
#ifdef SAVE_STREAM
  FILE          *f;
#endif

  printf( "KiwiRayServer [info]: OHAI!\n\n" );

  atexit( close_message );

  clients_init();
  
	signal( SIGINT, terminate );

  // Initialize network
  if( net_init() < 0 ) {
    fprintf( stderr, "KiwiRayServer [error]: Network initialization failed\n" );
    exit( EXIT_NETWORK );
  } else {
  	
    // Aquire socket
    if( net_sock( &h_sock ) < 0 ) {
      fprintf( stderr, "KiwiRayServer [error]: Socket aquire failed\n" );
      exit( EXIT_SOCKET );
    } else {

		  // Port from args
		  if( argc > 3 ) port = atoi( argv[ 3 ] );
    	
      // Bind socket to PORT
      net_addr_init( &srv_addr, NET_ADDR_ANY, port );
      if( net_bind( &h_sock, &srv_addr ) < 0 ) {
        fprintf( stderr, "KiwiRayServer [error]: Socket bind failed\n" );
        exit( EXIT_BIND );
      }
    }
  }
  net_addr_init( &cli_addr, NET_ADDR_ANY, 0 );

  if( SDL_Init( SDL_INIT_AUDIO ) == 0 ) {
    atexit( SDL_Quit );
  } else {
    exit( EXIT_AUDIO );
  }

  // Initialize video capture device
  cap_w = CAP_WIDTH;
  cap_h = CAP_HEIGHT;
  if( argc > 1 ) {
    if( cam_init( argv[ 1 ], STREAM_FPS, &cap_w, &cap_h ) < 0 ) {
      fprintf( stderr, "KiwiRayServer [error]: Unable to open capture device %s\n", argv[ 1 ] );
      exit( EXIT_CAPTURE );
    }
    if( cap_w != CAP_WIDTH || cap_h != CAP_HEIGHT ) {
      printf( "KiwiRayServer [warning]: Captured video is %ix%i, not %ix%i\n\n", cap_w, cap_h, CAP_WIDTH, CAP_HEIGHT );
    }
    atexit( cam_close );
  } else {
    cam_init( NULL, STREAM_FPS, &cap_w, &cap_h );
    exit( EXIT_OK );
  }

  // Initialize encoder
  x264_param_default_preset( &param, "medium", "zerolatency" );
  
  param.i_width   = STREAM_WIDTH;
  param.i_height  = STREAM_HEIGHT;
  param.i_fps_num = STREAM_FPS;
  
  // Settings as explained by http://x264dev.multimedia.cx/archives/249
  
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

/*x264_param_parse( &param, "no-cabac", NULL );			  *//* Disable CABAC.
  																												 It's been said it's unsuitable for this
  																												 type of H.264 application. */
  																										  /* Further investigation shows that this is
  																										     not correct, using CABAC will not cause
  																										     performance spikes in this instance, and
  																										     will offload some of the encoders work
  																										     onto the decoder resulting in better
  																										     server performance */
  
  param.b_annexb = 1;																		/* Use Annex-B packaging.
  																											   This appends a marker at the start of
  																											   each NAL unit. */
  
  param.i_frame_reference = 1;													/* Needed for intra-refresh. */

  x264_param_apply_profile( &param, "high" );						/* Apply HIGH profile.
  																												 Allows for better compression, but needs
  																												 to be supported by the decoder. We use
  																												 FFMPEG, which does support this profile. */

  // Open encoder
  encoder = x264_encoder_open( &param );
  atexit( encoder_close );
  
  // Allocate I420 picture
  if( x264_picture_alloc( &pic_in, X264_CSP_I420, STREAM_WIDTH, STREAM_HEIGHT ) == 0 ) {
    atexit( picture_close );
  } else {
    exit( EXIT_PICTURE );
  }

  // Initialize color space convertor
  convertCtx = sws_getContext( cap_w, cap_h, PIX_FMT_BGR24, STREAM_WIDTH, STREAM_HEIGHT, PIX_FMT_YUV420P, SWS_FAST_BILINEAR, NULL, NULL, NULL );
  if( convertCtx ) {
    atexit( sws_close );
  } else {
    fprintf( stderr, "KiwiRayServer [error]: Cannot initialize the conversion context" );
    exit( EXIT_SWSCALE );
  }

  stride = cap_w * 3; // RGB stride, width * 3

  // Create receiving thread
  hReceiver = SDL_CreateThread( receiver, NULL );
  atexit( receiver_close );
  
  // Initialize serial
  if( argc > 2 ) {
    p_serdev = argv[ 2 ];
    hKiwiray = SDL_CreateThread( kiwiray, NULL );
    atexit( kiwiray_close );
  } else {
    printf( "KiwiRayServer [warning]: Argument 2 missing, disabling serial\n" );
  }

#ifndef DISABLE_SPEECH
  speech_open();
  speech_queue( "INITIALIZED AND READY FOR CONNECTION" );
#endif

  printf( "\nKiwiRayServer [info]: listening on port %i...\n", port );

  trust_mx = SDL_CreateMutex();
  atexit( trust_mx_close );
  client_mx = SDL_CreateMutex();
  atexit( client_mx_close );

#ifdef SAVE_STREAM
  f = fopen( SAVE_STREAM, "wb" );
#endif

  time_target = SDL_GetTicks();

  while( !quit ) {

  	speech_poll();

    // Fetch latest picture from capture device
    data = ( uint8_t * )cam_fetch();

		// Scaling and coding as explained by http://stackoverflow.com/questions/2940671/how-to-encode-series-of-images-into-h264-using-x264-api-c-c

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

#ifdef SAVE_STREAM
    fwrite( p_buffer, 1, i_buffer, f );
#endif

    // Largest packet
    if( pl > pt ) pt = pl;

    // Client connected?
    SDL_mutexP( client_mx );
    temp = ( client_first ? 1 : 0 );
    // Stop moving if control packets are not arriving
    if( temp ) {
      if( client_first->glitch == 0 ) {
        client_first->ctrl.ctrl.kb = 0;
      }
    }
    SDL_mutexV( client_mx );
    if( temp ) {

      // Calculate control differentials 
      clients_diff( client_first );

      // Handle movement X and Y
      if( client_first->ctrl.ctrl.kb & KB_LEFT  ) {
        drive_x = ( drive_x > -( 127 - MOV_ACC ) ? drive_x - MOV_ACC : -127 );
      } else if( drive_x < 0 ) {
        drive_x = ( drive_x < -MOV_BRK ? drive_x + MOV_BRK : 0 );
      }
      if( client_first->ctrl.ctrl.kb & KB_RIGHT ) {
        drive_x = ( drive_x <  ( 127 - MOV_ACC ) ? drive_x + MOV_ACC :  127 );
      } else if( drive_x > 0 ) {
        drive_x = ( drive_x >  MOV_BRK ? drive_x - MOV_BRK : 0 );
      }
      if( client_first->ctrl.ctrl.kb & KB_UP    ) {
        drive_y = ( drive_y > -( 127 - MOV_ACC ) ? drive_y - MOV_ACC : -127 );
      } else if( drive_y < 0 ) {
        drive_y = ( drive_y < -MOV_BRK ? drive_y + MOV_BRK : 0 );
      }
      if( client_first->ctrl.ctrl.kb & KB_DOWN  ) {
        drive_y = ( drive_y <  ( 127 - MOV_ACC ) ? drive_y + MOV_ACC :  127 );
      } else if( drive_y > 0 ) {
        drive_y = ( drive_y >  MOV_BRK ? drive_y - MOV_BRK : 0 );
      }

      // Handle movement R
      integrate_r -= ( drive_r * ROT_SEN );
      integrate_r += client_first->diff.mx;
      if( integrate_r > ROT_DZN ) {
        drive_r = ( drive_r <  ( 127 - ROT_ACC ) ? drive_r + ROT_ACC :  127 );
        if( drive_r > integrate_r / ROT_DMP + ROT_DZN ) drive_r = integrate_r / ROT_DMP + ROT_DZN;
      } else if( integrate_r < -ROT_DZN ) {
        drive_r = ( drive_r > -( 127 - ROT_ACC ) ? drive_r - ROT_ACC : -127 );
        if( drive_r < integrate_r / ROT_DMP - ROT_DZN ) drive_r = integrate_r / ROT_DMP - ROT_DZN;
      } else {
        drive_r = 0;
      }
      
      // Handle camera pitch
      if( ( long )drive_p + client_first->diff.my > 255 / CAM_SEN ) {
        drive_p = 255 / CAM_SEN;
      } else if( ( long )drive_p + client_first->diff.my < 0 ) {
        drive_p = 0;
      } else {
        drive_p = drive_p + client_first->diff.my;
      }

    	// Send H.264 frame
      net_send( &h_sock, p_buffer, i_buffer, &client_first->client );
      
      // Build DATA packet
      memcpy( p_buffer, "DATA", 4 );
      disp.timer    = client_first->timer;
      disp.trust_cli = client_first->trust_cli;
      disp.trust_srv = client_first->trust_srv;
      memcpy( p_buffer + 4, &disp, sizeof( disp_data_t ) );
      i_buffer = 4 + sizeof( disp_data_t );

      // Append trusted data if any
      SDL_mutexP( trust_mx );
      if( trust_timeout == 0 ) {
        // Trusted data?
        if( trust_first ) {
          memcpy( p_buffer + i_buffer, trust_first->data, trust_first->size );
          i_buffer += trust_first->size;
          trust_timeout = TIMEOUT_TRUST;
        }
      } else {
        trust_timeout--;
      }
      SDL_mutexV( trust_mx );

      // Send DATA packet
      net_send( &h_sock, p_buffer, i_buffer, &client_first->client );
      
    } else {
      drive_x = 0;
      drive_y = 0;
      drive_r = 0;
      drive_p = 165 / CAM_SEN;
      integrate_r = 0;
    }

    // Delay 1/STREAM_FPS seconds, constantly correct for processing overhead
    time_diff = SDL_GetTicks() - time_target;
    if( time_diff > 1000 / STREAM_FPS ) { 
      time_diff = 0;
      time_target = SDL_GetTicks(); // Reset on overflow
      printf( "KiwiRayServer [warning]: Encoder cannot keep up with desired FPS\n" );
    }
    if( time_diff < 0 ) {
      time_diff = 0;
      printf( "KiwiRayServer [error]: SDL_Delay returns too fast\n" );
    }
    time_target += 1000 / STREAM_FPS;
    SDL_Delay( ( 1000 / STREAM_FPS ) - time_diff );

    // Tick client timers
    clients_tick();

  }

#ifdef SAVE_STREAM
  fclose( f );
#endif

  printf( "KiwiRayServer [info]: NAL units: %i, %i bytes\n", nalc, nalb );
  printf( "KiwiRayServer [info]: Largest packet: %i\n\n", pt );

  exit( EXIT_OK );
}
