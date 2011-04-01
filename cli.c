#include <stdio.h>
#include <SDL/SDL.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include "oswrap.h"
#include "kiwiray.h"
#include "speech.h"
#include "cli_term.h"

#define MAX_RETRY       5
#define MAX_NO_H264   125 // milliseconds/20
#define KIWI_VERSION    2
#define PORT         6979
#define TIMEOUT_TRUST  10 // milliseconds/20

#define FULLSCREEN      0 // start fullscreen

enum state_e {
  STATE_CONNECTING,
  STATE_QUEUED,
  STATE_ERROR,
  STATE_FULL,
  STATE_LOST,
  STATE_VERSION,
  STATE_STREAMING
};

enum km_e {
  KM_LEFT,
  KM_RIGHT,
  KM_UP,
  KM_DOWN,
  KM_SIZE
};

enum kbd_layout_e {
  KL_QWERTY,
  KL_DVORAK,
  KL_AZERTY,
  KL_SIZE
};

typedef struct {
  char data[ 8192 ];
  int size;
  void* next;
} linked_buf_t;

static NET_ADDR srv_addr;
static int port = PORT;

static disp_data_t disp_data;
static SDL_Surface *logo;
static SDL_Surface *help;
static char text_contact[] =  "CONTACTING KIWIRAY1...";
static char text_error[]   =  "   CONNECTION ERROR   ";
static char text_queued[] =   "  QUEUED FOR CONTROL  ";
static char text_full[] =     " QUEUE IS FULL, SORRY ";
static char text_lost[] =     "  CONNECTION IS LOST  ";
static char text_quit[] =     "   PRESS H FOR HELP   ";
static char text_version[] =  " WRONG CLIENT VERSION ";
static char text_blank[] =    "                      ";
static char text_time[] =     "       00:00:00       ";
static char text_controls[] = "IN CONTROL - PRESS H FOR HELP";
static char text_timeout[] =  "TIME LEFT: 00:00";

static char pkt_h264[ 4 ] = { 0x00, 0x00, 0x00, 0x01 };
static char pkt_data[ 4 ] = "DATA";
static char pkt_helo[ 4 ] = "HELO";
static char pkt_time[ 4 ] = "TIME";
static char pkt_ctrl[ 4 ] = "CTRL";
static char pkt_lost[ 4 ] = "LOST";
static char pkt_full[ 4 ] = "FULL";
static char pkt_quit[ 4 ] = "QUIT";

static   SDL_Surface *screen;                          // Screen surface
static           int  screen_w, screen_h, screen_bpp;  // Screen parameters
static      NET_SOCK  h_sock;                          // Socket handle
static  linked_buf_t *p_buffer_last;                   // Decoding buffer
static  linked_buf_t *p_buffer_first;
static volatile  int  state = STATE_CONNECTING;        // Client state
static volatile  int  retry = 0;                       // Used for retransmissions and timeouts
static           int  queue_time;                      // Time left before FUN
static  linked_buf_t *trust_first = NULL;              // Non-lossy packet buffer
static  linked_buf_t *trust_last = NULL;
static unsigned char  trust_srv = 0xFF;                // Non-lossy transmission counters
static unsigned char  trust_cli = 0x00;
static     SDL_mutex *trust_mx;                        // Non-lossy buffer access mutex
static           int  trust_timeout = 0;               // Non-lossy retransmission timeout
static           int  b_cursor_grabbed;                // Is cursor currently "grabbed"?
static         Uint8  draw_red, draw_green, draw_blue; // Drawing color
static unsigned char  layout = KL_QWERTY;
static        SDLKey  keymap[ KM_SIZE ];               // Keyboard remapping
static  unsigned int  message_timeout = 0;

// Display message
static void message( char* text ) {
  term_write( 1, 27, text, 1 );
  message_timeout = 125;
}

// Switches keyboard layout
static void set_layout( unsigned char new_layout ) {
  // Mapping schemes for QWERTY (WASD) Dvorak (,AOE) and AZERTY (ZQSD)
  layout = new_layout % KL_SIZE;
  switch( layout ) {
    case KL_QWERTY:
      keymap[ KM_UP ]    = SDLK_w;
      keymap[ KM_LEFT ]  = SDLK_a;
      keymap[ KM_DOWN ]  = SDLK_s;
      keymap[ KM_RIGHT ] = SDLK_d;
      message( "SWITCHED TO QWERTY" );
      break;
    case KL_DVORAK:
      keymap[ KM_UP ]    = SDLK_COMMA;
      keymap[ KM_LEFT ]  = SDLK_a;
      keymap[ KM_DOWN ]  = SDLK_o;
      keymap[ KM_RIGHT ] = SDLK_e;
      message( "SWITCHED TO DVORAK" );
      break;
    case KL_AZERTY:
      keymap[ KM_UP ]    = SDLK_z;
      keymap[ KM_LEFT ]  = SDLK_q;
      keymap[ KM_DOWN ]  = SDLK_s;
      keymap[ KM_RIGHT ] = SDLK_d;
      message( "SWITCHED TO AZERTY" );
      break;
  }
}

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
void trust_handler( char* data, int size ) {
  if( size == 0 ) return;
  trust_srv++;
  //data[ size ] = 0;
  //term_write( 2, 10, data, 0 );
}

// Convert unicode to ascii - kind of a hack
static char UnicodeChar( int uni ){
  #define INTERNATIONAL_MASK 0xFF80
  #define UNICODE_MASK       0x007F
  if( uni == 0 ) return( 0 );
  if( ( uni & INTERNATIONAL_MASK ) == 0 ) {
    return( ( char )( toupper( uni & UNICODE_MASK ) ) );
  } else {
    return( '?' );
  }
}

// Initialize graphical resources   
static void resource_init() {
  SDL_Surface* temp;
  temp = SDL_LoadBMP( "logo.bmp" );
  if( !temp ) printf( "KiwiDriveClient [error]: Unable to load logo.bmp\n" );
  logo = SDL_DisplayFormat( temp );
  SDL_FreeSurface( temp );
  temp = SDL_LoadBMP( "help.bmp" );
  if( !temp ) printf( "KiwiDriveClient [error]: Unable to load help.bmp\n" );
  help = SDL_DisplayFormat( temp );
  SDL_FreeSurface( temp );
  SDL_SetColorKey( help, SDL_SRCCOLORKEY, SDL_MapRGB( screen->format, 0xFF, 0x00, 0xFF ) );
}

// Free graphical resources
static void resource_clean() {
    SDL_FreeSurface( logo );
}

static void SetColor( Uint8 red, Uint8 green, Uint8 blue ) {
  draw_red = red;
  draw_green = green;
  draw_blue = blue;
}

// Used by DrawWuLine
static void DrawPixel( short x, short y, Uint8 a ) {
  Uint8 cr, cg, cb;
  Uint32 cv;
  if( y > 479 || x < 0 || x > 639 ) return;

  cv = ( *( Uint32* )( ( ( uint8_t* )screen->pixels ) + ( y * screen->pitch + x * 4 ) ) );
  
  SDL_GetRGB( cv, screen->format, &cr, &cg, &cb );
  cr = ( ( ( int )cr * a ) + ( ( int )draw_red   * ( 255 - a ) ) ) >> 8;
  cg = ( ( ( int )cg * a ) + ( ( int )draw_green * ( 255 - a ) ) ) >> 8;
  cb = ( ( ( int )cb * a ) + ( ( int )draw_blue  * ( 255 - a ) ) ) >> 8;
  cv = SDL_MapRGB( screen->format, cr, cg, cb );
  
  ( *( Uint32* )( ( ( uint8_t* )screen->pixels ) + ( y * screen->pitch + x * 4 ) ) ) = cv;
}

// Wu-Line implementation, courtesy of http://www.codeproject.com/KB/GDI/antialias.aspx
static void DrawWuLine( short X0, short Y0, short X1, short Y1 ) {
   unsigned short IntensityShift, ErrorAdj, ErrorAcc;
   unsigned short ErrorAccTemp, Weighting, WeightingComplementMask;
   short DeltaX, DeltaY, Temp, XDir;
   SDL_LockSurface( screen );
   // Locked parameters
   #define BaseColor 0
   #define NumLevels 256
   #define IntensityBits 8
   // Precomputing
   if( Y0 > Y1 ) {
      Temp = Y0; Y0 = Y1; Y1 = Temp;
      Temp = X0; X0 = X1; X1 = Temp;
   }
   if( ( DeltaX = X1 - X0 ) >= 0 ) {
      XDir = 1;
   } else {
      XDir = -1;
      DeltaX = -DeltaX;
   }
   // Draw initial pixel   
   DrawPixel( X0, Y0, BaseColor );
   // Draw horizontal, vertical, diagonal
   if( ( DeltaY = Y1 - Y0 ) == 0 ) {
      while( DeltaX-- != 0 ) {
         X0 += XDir;
         DrawPixel( X0, Y0, BaseColor );
      }
   } else if( DeltaX == 0 ) {
      do {
         Y0++;
         DrawPixel( X0, Y0, BaseColor );
      } while ( --DeltaY != 0 );
   } else if( DeltaX == DeltaY ) {
      do {
         X0 += XDir;
         Y0++;
         DrawPixel( X0, Y0, BaseColor );
      } while ( --DeltaY != 0 );
   } else {
     // Draw X/Y major
     ErrorAcc = 0;
     IntensityShift = 16 - IntensityBits;
     WeightingComplementMask = NumLevels - 1;
     if( DeltaY > DeltaX ) {
        ErrorAdj = ( ( unsigned long ) DeltaX << 16 ) / ( unsigned long ) DeltaY;
        while( --DeltaY ) {
           ErrorAccTemp = ErrorAcc;
           ErrorAcc += ErrorAdj;
           if( ErrorAcc <= ErrorAccTemp ) X0 += XDir;
           Y0++;
           Weighting = ErrorAcc >> IntensityShift;
           DrawPixel( X0, Y0, BaseColor + Weighting );
           DrawPixel( X0 + XDir, Y0, BaseColor + ( Weighting ^ WeightingComplementMask ) );
        }
     } else {
       ErrorAdj = ( ( unsigned long) DeltaY << 16) / (unsigned long) DeltaX;
       while( --DeltaX ) {
          ErrorAccTemp = ErrorAcc;
          ErrorAcc += ErrorAdj;
          if ( ErrorAcc <= ErrorAccTemp ) Y0++;
          X0 += XDir;
          Weighting = ErrorAcc >> IntensityShift;
          DrawPixel( X0, Y0, BaseColor + Weighting );
          DrawPixel( X0, Y0 + 1, BaseColor + ( Weighting ^ WeightingComplementMask ) );
       }
     }
     DrawPixel( X1, Y1, BaseColor );
   }

   SDL_UnlockSurface( screen );
}

// Grab the cursor (locks cursor to our app)
static void cursor_grab( int b_grab ) {
  SDL_GrabMode ret; 
  ret = SDL_WM_GrabInput( b_grab ? SDL_GRAB_ON : SDL_GRAB_OFF );
  b_cursor_grabbed = ( ret & SDL_GRAB_ON ) != 0;
  SDL_ShowCursor( b_cursor_grabbed ? SDL_DISABLE : SDL_ENABLE );
}

// Poll cursor with warping support and grabbing detection
// Adds any movement to incremental variables ix and iy
static void cursor_poll( long *ix, long *iy ) {
  static int ready = 0;
  static int lx = 0, ly = 0;
  int cx = 0, cy = 0;
  int warp = 0;
  if( !b_cursor_grabbed ) ready = 0;
  SDL_GetMouseState( &cx, &cy );
  if( ready ) {
    *ix += cx - lx;
    *iy += cy - ly;
    if( cx > ( ( screen_w >> 1 ) + ( screen_w >> 2 ) )
     || cx < ( ( screen_w >> 1 ) - ( screen_w >> 2 ) ) ) {
      cx = screen_w >> 1;
      warp = 1;
    } 
    if( cy > ( ( screen_h >> 1 ) + ( screen_h >> 2 ) )
     || cy < ( ( screen_h >> 1 ) - ( screen_h >> 2 ) ) ) {
      cy = screen_h >> 1;
      warp = 1;
    }
    if( warp ) SDL_WarpMouse( cx, cy );
  }
  if( b_cursor_grabbed ) ready = 1;
  lx = cx; ly = cy;
}

// Draws out the help screen
void help_draw() {
  term_write( 4, 11, "CONTROLS: WASD + MOUSE", 0 );
  term_write( 4, 12, "L: TOGGLE QWERTY, DVORAK, AZERTY", 0 );
  term_write( 4, 13, "H: SHOW/HIDE HELP", 0 );
  term_write( 4, 14, "T: OPEN SPEECH/COMMAND PROMPT", 0 );
  term_write( 4, 15, "F: TOGGLE FULL-SCREEN", 0 );
  term_write( 4, 16, "   WHEN IN WINDOWED MODE, USED", 0 );
  term_write( 4, 17, "   MOUSE-LEFT TO GRAB/UNGRAB", 0 );
  term_write( 4, 18, "ESCAPE: QUIT", 0 );
}

// Clears the help screen
void help_clear() {
  int n;
  for( n = 11; n <= 18; n++ ) term_white( 4, n, 32 );
}

// Thread handles reception of UDP packets
static int receiver( void *unused ) {
  int temp, size;
  while( 1 ) {
    temp = sizeof( srv_addr );
    size = net_recv( &h_sock, p_buffer_last->data, 8192, &srv_addr );
    if( size >= 4 ) {
      
      // H264
      if( memcmp( p_buffer_last->data, pkt_h264, 4 ) == 0 ) {
        // h264 packet
        state = STATE_STREAMING;
        retry = 0;
        
        // Push decoder buffer to queue
        p_buffer_last->next = malloc( sizeof( linked_buf_t ) );
        linked_buf_t *p_buf = p_buffer_last;
        p_buffer_last = p_buffer_last->next;
        p_buffer_last->size = 0;
        p_buf->size = size;
      
      // DATA
      } else if( memcmp( p_buffer_last->data, pkt_data, 4 ) == 0 ) {
        if( size >= 4 + sizeof( disp_data_t ) ) {
          memcpy( &disp_data, p_buffer_last->data + 4, sizeof( disp_data_t ) );

          // Check if outgoing trusted data recieved, free trusted buffers
          SDL_mutexP( trust_mx );
          if( trust_first ) {
            if( disp_data.trust_cli == trust_cli ) {
              linked_buf_t* p_trust = trust_first;
              trust_first = trust_first->next;
              trust_cli++;
              trust_timeout = 0;
              free( p_trust );
            }
          }
          SDL_mutexV( trust_mx );

          // Handle incoming trusted data
          if( ( ( trust_srv + 1 ) & 0xFF ) == disp_data.trust_srv ) {
            trust_handler( p_buffer_last->data + 4 + sizeof( disp_data_t ), size - 4 - sizeof( disp_data_t ) );
          }

        }
        
      // HELO
      } else if( memcmp( p_buffer_last->data, pkt_helo, 4 ) == 0 ) {
        if( state == STATE_CONNECTING ) {
          
          // Go to queued only if version is correct
          if( p_buffer_last->data[ 4 ] != KIWI_VERSION ) {
            state = STATE_VERSION;
          } else {
            state = STATE_QUEUED;
            retry = 0;
            queue_time = *( int* )&p_buffer_last->data[ 5 ];
          }
        }
        
      // TIME
      } else if( memcmp( p_buffer_last->data, pkt_time, 4 ) == 0 ) {
        
        // Update queue time
        if( state == STATE_QUEUED ) {
          retry = 0;
          queue_time = *( int* )&p_buffer_last->data[ 4 ];
        }
        
      // LOST
      } else if( memcmp( p_buffer_last->data, pkt_lost, 4 ) == 0 ) {
        
        // Connection was lost (server don't know who we are)
        state = STATE_LOST;
        
      // FULL
      } else if( memcmp( p_buffer_last->data, pkt_full, 4 ) == 0 ) {
        
        // Connection could not be established (server queue is full)
        state = STATE_FULL;
        
      }
    }
  }
}

int main( int argc, char *argv[] ) {
  int                temp;                       // Various uses
  int                statec = 0;                 // State counter, used for retransmissions
  int                laststate = -1;             // Used to detect state changes
  char               p_text[ 256 ];              // Command text input
  char               l_text = -1;                // Tracks size of p_text
  char               c_text;                     // Used for unicode text input translation
  char               p_ctrl[ 8192 ];             // CTRL packet buffer
  int                i_ctrl;                     // Tracks size of p_ctrl
  AVCodecContext    *pCodecCtx;                  // FFMPEG codec context
  AVCodec           *pCodec;                     // Pointer to FFMPEG codec (H264)
  AVFrame           *pFrame;                     // Used in the decoding process
  struct SwsContext *convertCtx;                 // Used in the scaling/conversion process
  AVPacket           avpkt;                      // Used in the decoding process
  SDL_Thread        *hReceiver;                  // Thread handle to UDP receiver
  SDL_Rect           rect;                       // Used for various graphics operations
  SDL_Surface       *live = NULL;                // Live decoded video surface
  char              *p_vis;                      // Pointer to speech visualization data
  int                b_fullscreen;               // Are we in fullscreen mode?
  Uint32             rmask, gmask, bmask, amask; // Masking (endianness)
  SDL_Event          event;                      // Events
  int                quit = 0;                   // Time to quit?
  ctrl_data_t        ctrl;                       // Part of CTRL packet
  int                b_help = 0;                 // Help is displayed?

  printf( "KiwiDriveClient [info]: OHAI!\n" );

  set_layout( KL_QWERTY );

  if( argc < 2 ) {
    printf( "KiwiDriveClient [error]: Need to specify IP-address on command-line\n" );
    printf( "\n" );
    return( 1 );
  }

  if( net_init() < 0 ) {
    printf( "KiwiDriveClient [error]: Network initialization failed\n" );
    return( 1 );
  }

  if( net_sock( &h_sock ) < 0 ) {
    printf( "KiwiDriveClient [error]: Socket aquire failed\n" );
    return( 1 );
  };
  
  // Port from args
  if( argc > 2 ) port = atoi( argv[ 2 ] );
  
  net_addr_init( &srv_addr, net_dtoa( argv[ 1 ] ), port );

  // Initialize decoder
  avcodec_init();
  avcodec_register_all();
  pCodecCtx = avcodec_alloc_context();
  pCodec = avcodec_find_decoder( CODEC_ID_H264 );
  av_init_packet( &avpkt );
  if( !pCodec ) {
    printf( "KiwiDriveClient [error]: Unable to initialize decoder\n" );
    return( 5 );
  }
  avcodec_open( pCodecCtx, pCodec );  

  // Allocate decoder frame
  pFrame = avcodec_alloc_frame();

  SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO );
  SDL_WM_SetCaption( "KiwiRay Client", "KiwiRay Client" );
  
  b_fullscreen = FULLSCREEN;
  screen_w = 640;
  screen_h = 480;
  screen_bpp = 32;
  screen = SDL_SetVideoMode( screen_w, screen_h, screen_bpp, ( b_fullscreen ? SDL_FULLSCREEN : 0 ) );
  // TODO: need check if fullscreen was possible?
  cursor_grab( b_fullscreen );

  SDL_EnableUNICODE( SDL_ENABLE );
  SDL_EnableKeyRepeat( 400, 50 );

  resource_init();
  term_init( screen );

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;
#endif

  SDL_Surface* frame = SDL_CreateRGBSurface( SDL_SWSURFACE, 640, 480, 24, 0, 0, 0, 0 );

  if( !frame ) {
    printf( "KiwiDriveClient [error]: Unable to allcate SDL surface\n" );
    return( 1 );
  }
  
  p_buffer_last = malloc( sizeof( linked_buf_t ) );;
  p_buffer_first = p_buffer_last;
  p_buffer_last->size = 0;
  
  speech_open();
  
  trust_mx = SDL_CreateMutex();

  // Create receiving thread
  hReceiver = SDL_CreateThread( receiver, NULL );
  
  while( !quit ) {

    speech_poll();
    cursor_poll( &ctrl.ctrl.mx, &ctrl.ctrl.my );

    /*
    // Debug print cursor
    char xxx[ 100 ];
    sprintf( xxx, "%i, %i        ", ctrl.ctrl.mx, ctrl.ctrl.my );
    term_write( 1, 10, xxx, 1 );
    */

    if( state != laststate ) {
      if( state == STATE_STREAMING ) ctrl.ctrl.kb = 0;
      l_text = -1;
      term_crem();
      laststate = state;
      term_clear();
      if( b_help ) help_draw();
      // Initialize view
      if( state != STATE_STREAMING ) {
        switch( state ) {
          case STATE_CONNECTING:
            term_write( 9, 24, text_contact, 0 );
            term_write( 9, 25, text_blank, 0 );
            break;
          case STATE_QUEUED:
            term_write( 9, 24, text_queued, 0 );
            break;
          case STATE_ERROR:
            term_write( 9, 24, text_error, 1 );
            term_write( 9, 25, text_blank, 0 );
            break;
          case STATE_FULL:
            term_write( 9, 24, text_full, 1 );
            term_write( 9, 25, text_blank, 0 );
            break;
          case STATE_LOST:
            term_write( 9, 24, text_lost, 1 );
            term_write( 9, 25, text_blank, 0 );
            break;
          case STATE_VERSION:
            term_write( 9, 24, text_version, 1 );
            term_write( 9, 25, text_blank, 0 );
            break;
        }
        term_write( 9, 26, text_quit, 0 );
      }
    }

    if( state == STATE_STREAMING ) {

      // Connected & streaming, buld CTRL packet
      memcpy( p_ctrl, pkt_ctrl, 4 );
      i_ctrl = 4;
      ctrl.trust_cli = trust_cli;
      ctrl.trust_srv = trust_srv;
      memcpy( p_ctrl + 4, &ctrl, sizeof( ctrl_data_t ) );
      i_ctrl += sizeof( ctrl_data_t );

      // Append trusted data if any
      SDL_mutexP( trust_mx );
      if( trust_timeout == 0 ) {
        if( trust_first ) {
          memcpy( p_ctrl + i_ctrl, trust_first->data, trust_first->size );
          i_ctrl += trust_first->size;
          trust_timeout = TIMEOUT_TRUST;
        }
      } else {
        trust_timeout--;
      }
      SDL_mutexV( trust_mx );

      // Send CTRL packet
      net_send( &h_sock, p_ctrl, i_ctrl, &srv_addr );
      if( ++retry == MAX_NO_H264 ) state = STATE_LOST;
      
      if( p_buffer_first->size ) {

        // Decode frame
        avpkt.data = p_buffer_first->data;
        avpkt.size = p_buffer_first->size;
        avpkt.flags = AV_PKT_FLAG_KEY;
        if( avcodec_decode_video2( pCodecCtx, pFrame, &temp, &avpkt ) < 0 ) {
          printf( "KiwiDriveClient [info]: Decoding error (packet loss)\n" );
        } else {
          SDL_LockSurface( frame );      

          const uint8_t * data[1] = { frame->pixels };
          int linesize[1] = { frame->pitch };

          // Create scaling & color-space conversion context
          convertCtx = sws_getContext( pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
            frame->w, frame->h, PIX_FMT_BGR24, SWS_AREA, NULL, NULL, NULL);

          // Scale and convert the frame
          sws_scale( convertCtx, (const uint8_t**) pFrame->data, pFrame->linesize, 0,
            pCodecCtx->height, (uint8_t * const*) data, linesize );

          // Cleanup
          sws_freeContext( convertCtx );

          SDL_UnlockSurface( frame );
          
          // Convert to display format for fast blitting
          if( live ) SDL_FreeSurface( live );
          live = SDL_DisplayFormat( frame );
    
          // Update control timer
          temp = disp_data.timer / 25;
          text_timeout[ 15 ] = '0' + ( ( temp % 60 ) % 10 );
          text_timeout[ 14 ] = '0' + ( ( temp % 60 ) / 10 );
          temp /= 60;
          text_timeout[ 12 ] = '0' + ( ( temp % 60 ) % 10 );
          text_timeout[ 11 ] = '0' + ( ( temp % 60 ) / 10 );
          term_write( 1, 1, text_controls, 0 );
          term_write( 1, 2, text_timeout, 0 );
        }

        // Pop decoding buffer from queue
        linked_buf_t *p_buf = p_buffer_first;
        p_buffer_first = p_buffer_first->next;
        free( p_buf );
        
      }
      
      if( live ) {
        // Draw video
        SDL_BlitSurface( live, NULL, screen, NULL );
      } else {
        // Clear screen
        rect.x = 0;
        rect.y = 0;
        rect.w = 640;
        rect.h = 480;
        SDL_FillRect( screen, &rect, 0 );
      }
      
      if( speech_vis( &p_vis ) == 0 ) {
        for( temp = 0; temp < 636; temp += 4 ) {
          SetColor( 0x3F, 0x00, 0x3F );
          DrawWuLine( temp + 1, 441 + p_vis[ temp ], temp + 5, 441 + ( p_vis[ temp + 4 ] ) );
          DrawWuLine( temp - 1, 439 + p_vis[ temp ], temp + 3, 439 + ( p_vis[ temp + 4 ] ) );
          DrawWuLine( temp + 1, 439 + p_vis[ temp ], temp + 5, 439 + ( p_vis[ temp + 4 ] ) );
          DrawWuLine( temp - 1, 441 + p_vis[ temp ], temp + 3, 441 + ( p_vis[ temp + 4 ] ) );
          SetColor( 0x00, 0x00, 0x00 );
          DrawWuLine( temp + 2, 442 + p_vis[ temp ], temp + 6, 442 + ( p_vis[ temp + 4 ] ) );
        }
        SetColor( 0x3F, 0x00, 0x3F );
        DrawWuLine( temp + 1, 441 + p_vis[ temp ], temp + 4, 441 + ( p_vis[ temp + 3 ] ) );
        DrawWuLine( temp - 1, 439 + p_vis[ temp ], temp + 2, 439 + ( p_vis[ temp + 3 ] ) );
        DrawWuLine( temp + 1, 439 + p_vis[ temp ], temp + 4, 439 + ( p_vis[ temp + 3 ] ) );
        DrawWuLine( temp - 1, 441 + p_vis[ temp ], temp + 2, 441 + ( p_vis[ temp + 3 ] ) );
        SetColor( 0x00, 0x00, 0x00 );
        DrawWuLine( temp + 2, 442 + p_vis[ temp ], temp + 5, 442 + ( p_vis[ temp + 3 ] ) );

        
        SetColor( 0x3F, 0xFF, 0x3F );
        for( temp = 0; temp < 636; temp += 4 ) {
          DrawWuLine( temp, 440 + p_vis[ temp ], temp + 4, 440 + ( p_vis[ temp + 4 ] ) );
        }
        DrawWuLine( temp, 440 + p_vis[ temp ], temp + 3, 440 + ( p_vis[ temp + 3 ] ) );
      }

    } else {      

      // Clear screen
      rect.x = 0;
      rect.y = 0;
      rect.w = 640;
      rect.h = 480;
      SDL_FillRect( screen, &rect, 0 );
      // Draw logo
      rect.x = 170;
      rect.y = 60;
      SDL_BlitSurface( logo, NULL, screen, &rect );

      switch( state ) {
        case STATE_CONNECTING:
          if( statec == 0 ) {
            if( ++retry == MAX_RETRY ) {
              state = STATE_ERROR;
            } else {
              net_send( &h_sock, pkt_helo, 4, &srv_addr );
            }
          }
          break;
        case STATE_QUEUED:

          // Draw queue time
          temp = queue_time / 25;

          text_time[ 14 ] = '0' + ( ( temp % 60 ) % 10 );
          text_time[ 13 ] = '0' + ( ( temp % 60 ) / 10 );
          temp /= 60;
          text_time[ 11 ] = '0' + ( ( temp % 60 ) % 10 );
          text_time[ 10 ] = '0' + ( ( temp % 60 ) / 10 );
          temp /= 60;
          text_time[  8 ] = '0' + ( temp % 10 );
          text_time[  7 ] = '0' + ( temp / 10 );
          term_write( 9, 25, text_time, 0 );
          if( statec == 0 ) {
            if( ++retry == MAX_RETRY ) {
              state = STATE_ERROR;
            } else {
              net_send( &h_sock, pkt_time, 4, &srv_addr );
            }
          }
          break;
      }
      if( statec ) statec--; else statec = 100;
    
    }

    // Draw help overlay
    if( b_help ) {
      rect.x = 48;
      rect.y = 160;
      rect.w = help->w;
      rect.h = help->h;
      SDL_BlitSurface( help, NULL, screen, &rect );
    }

    // Clear messages
    if( message_timeout ) {
      if( --message_timeout == 0 ) term_white( 1, 27, 38 );
    }
    
    // Draw terminal overlay
    term_draw();

    // Refresh screen
    SDL_UpdateRect( screen, 0, 0, 0, 0 );

    while( SDL_PollEvent( &event ) && !quit ) {
      switch( event.type ) {
        case SDL_QUIT:
          // Set time to quit
          quit = 1;
          break;
          
        case SDL_ACTIVEEVENT:
          if( event.active.state & SDL_APPINPUTFOCUS ) {
            if( event.active.gain == 1 ) {
              cursor_grab( b_fullscreen );
            } else {
              cursor_grab( 0 );
            }
          }
          break;
          
        case SDL_MOUSEBUTTONDOWN:
          // Toggle cursor grabbing
          if( event.button.button == SDL_BUTTON_LEFT && !b_fullscreen ) {
            cursor_grab( !b_cursor_grabbed );
          }
          break;
          
        case SDL_KEYDOWN:
          if( l_text >= 0 ) {
            switch( event.key.keysym.sym ) {
              case SDLK_BACKSPACE:
                // Remove last character
                if( l_text > 0 ) {
                  p_text[ --l_text ] = 0;
                  term_white( l_text + 2, 28, 1 );
                  term_cins( l_text + 2, 28 );
                  break;
                }
                //...

              case SDLK_RETURN:
                if( l_text > 0 ) {
                  // Send to server
                  memset( p_text + strlen( p_text ), 0, 255 - strlen( p_text ) );
                  trust_queue( p_text, strlen( p_text ) );
                  // Queue for local playback
                  speech_queue( p_text );
                }
                //...

              case SDLK_ESCAPE:
                // Abort text input
                l_text = -1;
                p_text[ l_text ] = 0;
                term_white( 1, 28, 38 );
                term_crem();
                break;
                
              default:
                // Attempt to insert character
                c_text = UnicodeChar( event.key.keysym.unicode );
                if( term_knows( c_text ) && l_text < 37 ) {
                  p_text[ l_text++ ] = c_text;
                  p_text[ l_text ] = 0;
                  term_write( l_text + 1, 28, &p_text[ l_text - 1 ], 0 );
                  term_cins( l_text + 2, 28 );
                }
                
            }
          } else {
            switch( event.key.keysym.sym ) {
              case SDLK_ESCAPE: // Quit
                quit = 1;
                break;
                
              case SDLK_f: // Toggle fullscreen
                b_fullscreen = !b_fullscreen;
                screen = SDL_SetVideoMode( screen_w, screen_h, screen_bpp, ( b_fullscreen ? SDL_FULLSCREEN : 0 ) );
                // TODO: need check if fullscreen was possible?
                cursor_grab( b_fullscreen );
                laststate = -1;
                break;
                
              case SDLK_l: // Switch keyboard layout
                set_layout( layout + 1 );
                break;
                
              case SDLK_t: // Text input
                if( state == STATE_STREAMING ) {
                  l_text = 0;
                  p_text[ 0 ] = 0x00;
                  term_write( 1, 28, ">", 0 );
                  term_cins( 2, 28 );
                  ctrl.ctrl.kb = 0;
                }
                break;
                
              case SDLK_h: // Toggle help
                b_help = !b_help;
                if( b_help ) {
                  help_draw();
                } else {
                  help_clear();
                }

              default: // WASD control scheme
                if( event.key.keysym.sym == keymap[ KM_LEFT ] ) {
                  ctrl.ctrl.kb |= KB_LEFT;
                } else if( event.key.keysym.sym == keymap[ KM_RIGHT ] ) {
                  ctrl.ctrl.kb |= KB_RIGHT;
                } else if( event.key.keysym.sym == keymap[ KM_UP ] ) {
                  ctrl.ctrl.kb |= KB_UP;
                } else if( event.key.keysym.sym == keymap[ KM_DOWN ] ) {
                  ctrl.ctrl.kb |= KB_DOWN;
                }
                break;

            }
          }
          break;
        case SDL_KEYUP:
          // WASD control scheme
          switch( event.key.keysym.sym ) {
            case SDLK_a:
              ctrl.ctrl.kb &= ~KB_LEFT;
              break;
            case SDLK_d:
              ctrl.ctrl.kb &= ~KB_RIGHT;
              break;
            case SDLK_w:
              ctrl.ctrl.kb &= ~KB_UP;
              break;
            case SDLK_s:
              ctrl.ctrl.kb &= ~KB_DOWN;
              break;
          }
          break;
      }
    }

    SDL_Delay( 20 );  
  }

  // Send QUIT
  net_send( &h_sock, pkt_quit, 4, &srv_addr );

  // Clean up
  resource_clean();
  avcodec_close( pCodecCtx );
  SDL_DestroyMutex( trust_mx );
  SDL_FreeSurface( frame );
  SDL_Quit();

  printf( "KiwiDriveClient [info]: KTHXBYE!\n" );

}