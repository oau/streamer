#include <stdio.h>
#include <SDL/SDL.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include "oswrap.h"
#include "robocortex.h"
#include "speech.h"
#include "cli_term.h"
#include "plugins/cli.h"
#include "sdl_console.h"

// Plugins
#define MAX_PLUGINS           16
extern pluginclient_t *kiwiray_open( pluginhost_t* );
extern pluginclient_t *ipv4udp_open( pluginhost_t* );

// Protocol
#define MAX_RETRY              5 // Maximum number of retransmissions of lost packets

// Configuration
#define CLIENT_RPS            50 // Client refreshes per second

// Timeouts (in refreshes, see CLIENT_RPS)
#define TIMEOUT_TRUST         16 // Before retransmitting trusted packets
#define TIMEOUT_STREAM       125 // Before considering connection lost

// Screen defaults
#define SCREEN_WIDTH         640 // Width
#define SCREEN_HEIGHT        480 // Height
#define SCREEN_BPP            32 // Color depth
#define SCREEN_FS              0 // Start in fullscreen

// Client state
enum state_e {
  STATE_CONNECTING,
  STATE_QUEUED,
  STATE_ERROR,
  STATE_FULL,
  STATE_LOST,
  STATE_VERSION,
  STATE_STREAMING
};

// Keyboard mapping
enum km_e {
  KM_LEFT,
  KM_RIGHT,
  KM_UP,
  KM_DOWN,
  KM_SIZE
};

// Keyboard layouts
enum kbd_layout_e {
  KL_QWERTY,
  KL_DVORAK,
  KL_AZERTY,
  KL_SIZE
};

// Exit code list
enum exitcode_e {
  EXIT_OK,
  EXIT_CONFIG,
  EXIT_DECODER,
  EXIT_SURFACE,
  EXIT_COMMS
};

// Texts
static           char  text_contact[] =  "ESTABLISHING CORTEX...";
static           char  text_error[]   =  "   CONNECTION ERROR   ";
static           char  text_queued[] =   "  QUEUED FOR CONTROL  ";
static           char  text_full[] =     " QUEUE IS FULL, SORRY ";
static           char  text_lost[] =     "  CONNECTION IS LOST  ";
static           char  text_quit[] =     "   PRESS H FOR HELP   ";
static           char  text_version[] =  " WRONG CLIENT VERSION ";
static           char  text_blank[] =    "                      ";
static           char  text_time[] =     "       00:00:00       ";
static           char  text_controls[] = "IN CONTROL - PRESS H FOR HELP";
static           char  text_timeout[] =  "TIME LEFT: 00:00";

// Packet types
static           char  pkt_h264[ 4 ] = { 0x00, 0x00, 0x00, 0x01 };
static           char  pkt_data[ 4 ] = "DATA";
static           char  pkt_helo[ 4 ] = "HELO";
static           char  pkt_time[ 4 ] = "TIME";
static           char  pkt_ctrl[ 4 ] = "CTRL";
static           char  pkt_lost[ 4 ] = "LOST";
static           char  pkt_full[ 4 ] = "FULL";
static           char  pkt_quit[ 4 ] = "QUIT";

// Locals
static    disp_data_t  disp_data;                       // Data from latest DISP packet
static    SDL_Surface *spr_logo;                        // Sprites
static    SDL_Surface *spr_box;
static    SDL_Surface *screen;                          // Screen surface
static            int  screen_w = SCREEN_WIDTH;         // Resolution
static            int  screen_h = SCREEN_HEIGHT;
static            int  screen_bpp = SCREEN_BPP;
static            int  fullscreen = SCREEN_FS;          // Fullscreen mode active
static   linked_buf_t *p_buffer_last;                   // Decoding buffer
static   linked_buf_t *p_buffer_first;
static  volatile  int  state = STATE_CONNECTING;        // Client state
static  volatile  int  retry = 0;                       // Used for retransmissions and timeouts
static            int  queue_time;                      // Time left before FUN
static   linked_buf_t *trust_first = NULL;              // Non-lossy packet buffer
static   linked_buf_t *trust_last = NULL;
static  unsigned char  trust_srv = 0xFF;                // Non-lossy transmission counters
static  unsigned char  trust_cli = 0x00;
static      SDL_mutex *trust_mx;                        // Non-lossy buffer access mutex
static            int  trust_timeout = 0;               // Non-lossy retransmission timeout
static            int  cursor_grabbed;                  // Cursor is grabbed
static          Uint8  draw_red, draw_green, draw_blue; // Drawing color
static  unsigned char  layout = KL_QWERTY;
static         SDLKey  keymap[ KM_SIZE ];               // Keyboard remapping
static   unsigned int  message_timeout = 0;
static    ctrl_data_t  ctrl;                            // Part of CTRL packet
static            int  help_shown;                      // Help is displayed
static           void  ( *comm_send )( char*, int );    // Communications handler

// Help texts
static           char  help[ 16 ][ 33 ] = {
  { "CONTROLS: WASD + MOUSE" },
  { "L: TOGGLE QWERTY, DVORAK, AZERTY" },
  { "H: SHOW/HIDE HELP" },
  { "F: TOGGLE FULL-SCREEN" },
  { "   WHEN IN WINDOWED MODE, USED" },
  { "   MOUSE-LEFT TO GRAB/UNGRAB" },
  { "ESCAPE: QUIT" },
  { "" },
};
static  unsigned char help_count = 8;

// Plugins
static   pluginhost_t  host;
static pluginclient_t *plug;
static pluginclient_t *plugs[ MAX_PLUGINS ];
static            int  plugs_count;
static pluginclient_t *cursor_hook;
static pluginclient_t *keyboard_hook;
static pluginclient_t *keyboard_binds[ SDLK_LAST ];

// Configuration
static           char config_default[] = "cli.rc"; // Default configuration file

/* == TRUSTED COMMUNICATIONS ==================================================================== */

// Queues a packet for trusted (non-lossy) transmission
static void trust_queue( uint32_t ident, void* data, unsigned char size ) {
  linked_buf_t *p_trust;
  p_trust = malloc( sizeof( linked_buf_t ) );
  if( p_trust ) {
    // Create packet
    memcpy( p_trust->data, &ident, 4 );
    p_trust->data[ 4 ] = size;
    memcpy( p_trust->data + 5, data, size );
    p_trust->size = size + 5;
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
static void trust_handler( char* data, int size ) {
  int len, pid;
  uint32_t ident;
  if( size == 0 ) return;
  trust_srv++;
  // Pass data to plugins
  while( size > 5 ) {
    ident = *( uint32_t* )data; data += 4;
    len = *data++;
    size -= 5;
    if( size >= len ) {
      // plugin->recv
      for( pid = 0; pid < MAX_PLUGINS && ( plug = plugs[ pid ] ) != NULL; pid++ ) {
        if( plug->ident == ident ) {
          if( plug->recv ) plug->recv( data, len );
        }
      }
    }
    size -= len;
  }
}

/* == GRAPHICS HELPERS ========================================================================== */

// Sets drawing color
static void draw_color( Uint8 red, Uint8 green, Uint8 blue ) {
  draw_red = red;
  draw_green = green;
  draw_blue = blue;
}

// Draws an alpha-blended pixel, used by draw_wu
static void draw_pixel( short x, short y, Uint8 a ) {
  Uint8 cr, cg, cb;
  Uint32 cv;
  if( y >= screen_h || x < 0 || x >= screen_w ) return;

  cv = ( *( Uint32* )( ( ( uint8_t* )screen->pixels ) + ( y * screen->pitch + x * 4 ) ) );

  SDL_GetRGB( cv, screen->format, &cr, &cg, &cb );
  cr = ( ( ( int )cr * a ) + ( ( int )draw_red   * ( 255 - a ) ) ) >> 8;
  cg = ( ( ( int )cg * a ) + ( ( int )draw_green * ( 255 - a ) ) ) >> 8;
  cb = ( ( ( int )cb * a ) + ( ( int )draw_blue  * ( 255 - a ) ) ) >> 8;
  cv = SDL_MapRGB( screen->format, cr, cg, cb );

  ( *( Uint32* )( ( ( uint8_t* )screen->pixels ) + ( y * screen->pitch + x * 4 ) ) ) = cv;
}

// Draws a wu-line. Implementation, courtesy of http://www.codeproject.com/KB/GDI/antialias.aspx
static void draw_wu( short X0, short Y0, short X1, short Y1 ) {
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
   draw_pixel( X0, Y0, BaseColor );
   // Draw horizontal, vertical, diagonal
   if( ( DeltaY = Y1 - Y0 ) == 0 ) {
      while( DeltaX-- != 0 ) {
         X0 += XDir;
         draw_pixel( X0, Y0, BaseColor );
      }
   } else if( DeltaX == 0 ) {
      do {
         Y0++;
         draw_pixel( X0, Y0, BaseColor );
      } while ( --DeltaY != 0 );
   } else if( DeltaX == DeltaY ) {
      do {
         X0 += XDir;
         Y0++;
         draw_pixel( X0, Y0, BaseColor );
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
           draw_pixel( X0, Y0, BaseColor + Weighting );
           draw_pixel( X0 + XDir, Y0, BaseColor + ( Weighting ^ WeightingComplementMask ) );
        }
     } else {
       ErrorAdj = ( ( unsigned long) DeltaY << 16) / (unsigned long) DeltaX;
       while( --DeltaX ) {
          ErrorAccTemp = ErrorAcc;
          ErrorAcc += ErrorAdj;
          if ( ErrorAcc <= ErrorAccTemp ) Y0++;
          X0 += XDir;
          Weighting = ErrorAcc >> IntensityShift;
          draw_pixel( X0, Y0, BaseColor + Weighting );
          draw_pixel( X0, Y0 + 1, BaseColor + ( Weighting ^ WeightingComplementMask ) );
       }
     }
     draw_pixel( X1, Y1, BaseColor );
   }
   SDL_UnlockSurface( screen );
}

// Draws a popup-box
static void draw_box( unsigned char x, unsigned char y, unsigned char w, unsigned char h, SDL_Surface *s ) {
  SDL_Rect src, dst;
  unsigned char xx, yy;
  SDL_BlitSurface( spr_box, rect( &src, 0, 0, 32, 32 ), s, rect( &dst, x << 4, y << 4, 32, 32 ) );
  SDL_BlitSurface( spr_box, rect( &src, 48, 0, 32, 32 ), s, rect( &dst, ( x + w - 2 ) << 4, y << 4, 32, 32 ) );
  SDL_BlitSurface( spr_box, rect( &src, 0, 48, 32, 32 ), s, rect( &dst, x << 4, ( y + h - 2 ) << 4, 32, 32 ) );
  SDL_BlitSurface( spr_box, rect( &src, 48, 48, 32, 32 ), s, rect( &dst, ( x + w - 2 ) << 4, ( y + h - 2 ) << 4, 32, 32 ) );
  for( xx = x + 2; xx < x + w - 2; xx++ ) {
    SDL_BlitSurface( spr_box, rect( &src, 32, 0, 16, 32 ), s, rect( &dst, xx << 4, y << 4, 16, 16 ) );
    SDL_BlitSurface( spr_box, rect( &src, 32, 48, 16, 32 ), s, rect( &dst, xx << 4, ( y + h - 2 ) << 4, 16, 16 ) );
  }
  for( yy = y + 2; yy < y + h - 2; yy++ ) {
    SDL_BlitSurface( spr_box, rect( &src, 0, 32, 32, 16 ), s, rect( &dst, x << 4, yy << 4, 16, 16 ) );
    SDL_BlitSurface( spr_box, rect( &src, 48, 32, 32, 16 ), s, rect( &dst, ( x + w - 2 ) << 4, yy << 4, 16, 16 ) );
    for( xx = x + 2; xx < x + w - 2; xx++ ) {
      SDL_BlitSurface( spr_box, rect( &src, 32, 32, 16, 16 ), s, rect( &dst, xx << 4, yy << 4 , 16, 16 ) );
    }
  }
}

// Draws temproray message
static void draw_message( char* text ) {
  term_write( 1, term_h - 3, text, FONT_RED );
  message_timeout = 125;
}

// Load sprites
static void sprites_init() {
  spr_logo = SDL_LoadBMP( "logo.bmp" );
  if( !spr_logo ) printf( "RoboCortex [error]: Unable to load logo.bmp\n" );
  spr_logo = SDL_DisplayFormat( spr_logo );
  spr_box = SDL_LoadBMP( "box.bmp" );
  if( !spr_box ) printf( "RoboCortex [error]: Unable to load box.bmp\n" );
  /* // Use bitmap "reserved" as alpha:
  spr_box->format->Amask = 0xFF000000;
  spr_box->format->Ashift = 24;
  spr_box->flags |= SDL_SRCALPHA;
  */
  // Use magenta color keying:
  spr_box = SDL_DisplayFormat( spr_box );
  SDL_SetColorKey( spr_box, SDL_SRCCOLORKEY, SDL_MapRGB( screen->format, 0xFF, 0x00, 0xFF ) );
}

// Free sprites
static void sprites_free() {
    SDL_FreeSurface( spr_logo );
}

// Draws out the help screen
static void draw_help( int draw ) {
  unsigned char n;
  unsigned char y = ( term_h - help_count ) >> 1;
  for( n = 0; n < help_count; n++ ) {
    if( draw ) {
     term_write( ( term_w - 32 ) >> 1, y + n, help[ n ], FONT_GREEN );
    } else {
      term_white( ( term_w - 32 ) >> 1, y + n, 32 );
    }
  }
}

/* == CURSOR HELPERS ============================================================================ */

// Grab the cursor (locks cursor to our app)
static void cursor_grab( int b_grab ) {
  SDL_GrabMode ret;
  ret = SDL_WM_GrabInput( b_grab ? SDL_GRAB_ON : SDL_GRAB_OFF );
  cursor_grabbed = ( ret & SDL_GRAB_ON ) != 0;
  SDL_ShowCursor( cursor_grabbed ? SDL_DISABLE : SDL_ENABLE );
}

// Poll cursor with warping support and grabbing detection
// Adds any movement to incremental variables ix and iy
static void cursor_poll( long *ix, long *iy ) {
  static int ready = 0;
  static int lx = 0, ly = 0;
  int cx = 0, cy = 0;
  int warp = 0;

  if( cursor_hook != NULL || !cursor_grabbed ) ready = 0;
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
  if( cursor_grabbed ) ready = 1;
  lx = cx; ly = cy;
}

/* == KEYBOARD HELPERS ========================================================================== */

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
      draw_message( "SWITCHED TO QWERTY" );
      break;
    case KL_DVORAK:
      keymap[ KM_UP ]    = SDLK_COMMA;
      keymap[ KM_LEFT ]  = SDLK_a;
      keymap[ KM_DOWN ]  = SDLK_o;
      keymap[ KM_RIGHT ] = SDLK_e;
      draw_message( "SWITCHED TO DVORAK" );
      break;
    case KL_AZERTY:
      keymap[ KM_UP ]    = SDLK_z;
      keymap[ KM_LEFT ]  = SDLK_q;
      keymap[ KM_DOWN ]  = SDLK_s;
      keymap[ KM_RIGHT ] = SDLK_d;
      draw_message( "SWITCHED TO AZERTY" );
      break;
  }
  help[ 0 ][ 10 ] = toupper( keymap[ KM_UP ] );
  help[ 0 ][ 11 ] = toupper( keymap[ KM_LEFT ] );
  help[ 0 ][ 12 ] = toupper( keymap[ KM_DOWN ] );
  help[ 0 ][ 13 ] = toupper( keymap[ KM_RIGHT ] );
  draw_help( help_shown );
  ctrl.ctrl.kb = 0;
}

/* == COMMUNICATIONS ============================================================================ */

// Processes a data packet
static void comm_recv( char *buffer, int size ) {
  if( size >= 4 ) {
    memcpy( p_buffer_last->data, buffer, size );

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
        if( p_buffer_last->data[ 4 ] != CORTEX_VERSION ) {
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

/* == CONFIGURATION ============================================================================= */

static int config_set( char *value, char *token ) {
  if( token != NULL ) {
    if( strcmp( token, "width" ) == 0 ) {
      screen_w = atoi( value );
    } else if( strcmp( token, "height" ) == 0 ) {
      screen_h = atoi( value );
    } else if( strcmp( token, "bpp" ) == 0 ) {
      screen_bpp = atoi( value );
    } else if( strcmp( token, "fullscreen" ) == 0 ) {
      fullscreen = atoi( value );
    } else if( strcmp( token, "plugin" ) == 0 ) {
      return( 1 );
    } else printf( "Config [warning]: unknown entry %s\n", token );
  }
  return( 0 );
}

/* == PLUGIN SYSTEM ============================================================================= */

static int plug_keybind( int key ) {
  // TODO: block reserved keys
  if( keyboard_binds[ key ] == NULL ) {
    keyboard_binds[ key ] = plug;
    return( 1 );
  }
  return( 0 );
}

static void plug_keyfree( int key ) {
  if( keyboard_binds[ key ] == plug ) keyboard_binds[ key ] = NULL;
}

static int plug_keyhook() {
  if( keyboard_hook == NULL ) {
    ctrl.ctrl.kb = 0;
    keyboard_hook = plug;
    return( 1 );
  }
  return( 0 );
}

static void plug_keyrelease() {
  if( keyboard_hook == plug ) keyboard_hook = NULL;
}

static int plug_csrhook( int show ) {
  if( cursor_hook == NULL ) {
    cursor_hook = plug;
    SDL_ShowCursor( show ? SDL_ENABLE : SDL_DISABLE );
    return( 1 );
  }
  return( 0 );
}

static void plug_csrrelease() {
  if( cursor_hook == plug ) {
    cursor_hook = NULL;
    SDL_ShowCursor( cursor_grabbed ? SDL_DISABLE : SDL_ENABLE );
  }
}

static void plug_csrmove( int x, int y ) {
  if( cursor_hook == plug ) SDL_WarpMouse( x, y );
}

static void plug_send( void *data, unsigned char size ) {
  trust_queue( plug->ident, data, size );
}

static void plug_help( char *text ) {
  if( strlen( text ) <= 32 && help_count < 16 ) strcpy( help[ help_count++ ], text );
}

static void plug_wu( int x0, int y0, int x1, int y1, uint32_t color ) {
  draw_color( color >> 16, color >> 8, color );
  draw_wu( x0, y0, x1, y1 );
}

static int plug_cfg( char* dst, char* req_token ) {
  return( config_plugin( plug->ident, dst, req_token ) );
}

static int plug_thread( void *pThread ) {
  return( ( ( int( * )() )pThread )() );
}

static void* plug_thrstart( int( *pThread )() ) {
  return( SDL_CreateThread( plug_thread, ( void* )pThread ) );
}

static void plug_thrstop( void* pHandle ) {
  SDL_KillThread( pHandle );
}

static void plug_thrdelay( int delay ) {
  SDL_Delay( delay );
}

static void load_plugins() {
  int pid;
  host.thread_start     = plug_thrstart;
  host.thread_stop      = plug_thrstop;
  host.thread_delay     = plug_thrdelay;
  host.cfg_read         = plug_cfg;
  host.key_bind         = plug_keybind;
  host.key_free         = plug_keyfree;
  host.keyboard_hook    = plug_keyhook;
  host.keyboard_release = plug_keyrelease;
  host.cursor_hook      = plug_csrhook;
  host.cursor_release   = plug_csrrelease;
  host.cursor_move      = plug_csrmove;
  host.text_cins        = term_cins;
  host.text_crem        = term_crem;
  host.text_write       = term_write;
  host.text_clear       = term_white;
  host.text_valid       = term_knows;
  host.server_send      = plug_send;
  host.help_add         = plug_help;
  host.speak_text       = speech_queue;
  host.draw_wuline      = plug_wu;
  host.draw_box         = draw_box;
  host.text_cols        = term_w;
  host.text_rows        = term_h;
  host.comm_recv        = comm_recv;

  printf( "RoboCortex [info]: Loading plugins...\n" );
  // Load plugins
  plugs[ plugs_count++ ] = kiwiray_open( &host );
  plugs[ plugs_count++ ] = ipv4udp_open( &host );
  printf( "RoboCortex [info]: Initializing plugins...\n" );
  // plugin->init
  for( pid = 0; pid < MAX_PLUGINS && ( plug = plugs[ pid ] ) != NULL; pid++ ) {
    if( plug->init ) plug->init();
    if( plug->comm_send ) {
      if( comm_send ) printf( "RoboCortex [error]: Multiple communications plugins loaded\n" );
      comm_send = plug->comm_send;
    }
  }
  printf( "RoboCortex [info]: Plugins loaded and initialized\n" );
}

static void unload_plugins() {
  int pid;
  // plugin->close
  for( pid = 0; pid < MAX_PLUGINS && ( plug = plugs[ pid ] ) != NULL; pid++ )
    if( plug->close ) plug->close();
}

/* == MAIN THREAD =============================================================================== */

int main( int argc, char *argv[] ) {
  int                pid;                        // Plugin iteration
  int                temp;                       // Various uses
  int                statec = 0;                 // State counter, used for retransmissions
  int                laststate = -1;             // Used to detect state changes
  char               ascii;                      // Used for unicode text input translation
  char               p_ctrl[ 8192 ];             // CTRL packet buffer
  int                i_ctrl;                     // Tracks size of p_ctrl
  AVCodecContext    *pCodecCtx;                  // FFMPEG codec context
  AVCodec           *pCodec;                     // Pointer to FFMPEG codec (H264)
  AVFrame           *pFrame;                     // Used in the decoding process
  struct SwsContext *convertCtx;                 // Used in the scaling/conversion process
  AVPacket           avpkt;                      // Used in the decoding process
  SDL_Rect           r;                          // Used for various graphics operations
  SDL_Surface       *live = NULL;                // Live decoded video surface
  char              *p_vis;                      // Pointer to speech visualization data
  Uint32             rmask, gmask, bmask, amask; // Masking (endianness)
  SDL_Event          event;                      // Events
  int                quit = 0;                   // Time to quit?
  Uint32             time_target;                // Timing target
  Sint32             time_diff;                  // Timing differential
  FILE              *cf;                         // Configuration file

  printf( "RoboCortex [info]: OHAI!\n" );

  // Read configuration file
  config_rc = ( argc > 1 ? argv[ 1 ] : config_default );
  config_parse( config_set );

  // Validate Configuration
  if( screen_w & ~3 != screen_w || screen_h & ~3 != screen_h ) {
    printf( "Config [error]: Width and height must be a multiple of 16\n" );
    exit( EXIT_CONFIG );
  }
  if( screen_w > 4096 || screen_h > 4096 ) {
    printf( "Config [error]: Width or height must not exceed 4096\n" );
    exit( EXIT_CONFIG );
  }
  if( screen_w < 640 || screen_h < 480 ) {
    printf( "Config [error]: Minimum resolution is 640x480\n" );
    exit( EXIT_CONFIG );
  }

  // Initialize decoder
  avcodec_init();
  avcodec_register_all();
  pCodecCtx = avcodec_alloc_context();
  pCodec = avcodec_find_decoder( CODEC_ID_H264 );
  av_init_packet( &avpkt );
  if( !pCodec ) {
    printf( "RoboCortex [error]: Unable to initialize decoder\n" );
    exit( EXIT_DECODER );
  }
  avcodec_open( pCodecCtx, pCodec );

  // Allocate decoder frame
  pFrame = avcodec_alloc_frame();

  SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO );
  SDL_WM_SetCaption( "KiwiRay Client", "KiwiRay Client" );

  screen = SDL_SetVideoMode( screen_w, screen_h, screen_bpp, ( fullscreen ? SDL_FULLSCREEN : 0 ) );
  // TODO: need check if fullscreen was possible?
  cursor_grab( fullscreen );

  SDL_EnableUNICODE( SDL_ENABLE );
  SDL_EnableKeyRepeat( 400, 50 );

  sprites_init();
  term_init( screen );

  set_layout( KL_QWERTY );

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

  SDL_Surface* frame = SDL_CreateRGBSurface( SDL_SWSURFACE, screen_w, screen_h, 24, 0, 0, 0, 0 );

  if( !frame ) {
    printf( "RoboCortex [error]: Unable to allcate SDL surface\n" );
    exit( EXIT_SURFACE );
  }

  p_buffer_last = malloc( sizeof( linked_buf_t ) );;
  p_buffer_first = p_buffer_last;
  p_buffer_last->size = 0;

  speech_open();

  trust_mx = SDL_CreateMutex();

  // Load plugins
  load_plugins();
  atexit( unload_plugins );
  if( comm_send == NULL ) {
    printf( "RoboCortex [error]: No communications plugin loaded\n" );
    exit( EXIT_COMMS );
  }

  time_target = SDL_GetTicks();
  while( !quit ) {
    
    speech_poll();
    cursor_poll( &ctrl.ctrl.mx, &ctrl.ctrl.my );

    if( state != laststate ) {
      if( state == STATE_STREAMING ) ctrl.ctrl.kb = 0;
      if( keyboard_hook ) if( ( plug = keyboard_hook )->lost ) plug->lost();
      if( cursor_hook ) if( ( plug = cursor_hook )->lost && plug != keyboard_hook ) plug->lost();
      keyboard_hook = NULL;
      cursor_hook = NULL;
      term_crem();
      laststate = state;
      term_clear();
      draw_help( help_shown );
      // Initialize view
      if( state != STATE_STREAMING ) {
        switch( state ) {
          case STATE_CONNECTING:
            term_write( ( term_w - 22 ) >> 1, ( term_h >> 1 ) + 9, text_contact, FONT_GREEN );
            term_write( ( term_w - 22 ) >> 1, ( term_h >> 1 ) + 10, text_blank, FONT_GREEN );
            break;
          case STATE_QUEUED:
            term_write( ( term_w - 22 ) >> 1, ( term_h >> 1 ) + 9, text_queued, FONT_GREEN );
            break;
          case STATE_ERROR:
            term_write( ( term_w - 22 ) >> 1, ( term_h >> 1 ) + 9, text_error, FONT_RED );
            term_write( ( term_w - 22 ) >> 1, ( term_h >> 1 ) + 10, text_blank, FONT_GREEN );
            break;
          case STATE_FULL:
            term_write( ( term_w - 22 ) >> 1, ( term_h >> 1 ) + 9, text_full, FONT_RED );
            term_write( ( term_w - 22 ) >> 1, ( term_h >> 1 ) + 10, text_blank, FONT_GREEN );
            break;
          case STATE_LOST:
            term_write( ( term_w - 22 ) >> 1, ( term_h >> 1 ) + 9, text_lost, FONT_RED );
            term_write( ( term_w - 22 ) >> 1, ( term_h >> 1 ) + 10, text_blank, FONT_GREEN );
            break;
          case STATE_VERSION:
            term_write( ( term_w - 22 ) >> 1, ( term_h >> 1 ) + 9, text_version, FONT_RED );
            term_write( ( term_w - 22 ) >> 1, ( term_h >> 1 ) + 10, text_blank, FONT_GREEN );
            break;
        }
        term_write( ( term_w - 22 ) >> 1, ( term_h >> 1 ) + 11, text_quit, FONT_GREEN );
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
      comm_send( p_ctrl, i_ctrl );
      if( ++retry == TIMEOUT_STREAM ) state = STATE_LOST;

      if( p_buffer_first->size ) {

        // Decode frame
        avpkt.data = ( unsigned char* )p_buffer_first->data;
        avpkt.size = p_buffer_first->size;
        avpkt.flags = AV_PKT_FLAG_KEY;
        if( avcodec_decode_video2( pCodecCtx, pFrame, &temp, &avpkt ) < 0 ) {
          printf( "RoboCortex [info]: Decoding error (packet loss)\n" );
        } else {
          SDL_LockSurface( frame );

          const uint8_t * data[1] = { frame->pixels };
          int linesize[1] = { frame->pitch };

          // Create scaling & color-space conversion context
          convertCtx = sws_getContext( pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
            frame->w, frame->h, PIX_FMT_RGB24, SWS_AREA, NULL, NULL, NULL);

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
          term_write( 1, 1, text_controls, FONT_GREEN );
          if( disp_data.timer == 0 ) {
            term_white( 1, 2, strlen( text_timeout ) );
          } else {
            temp = disp_data.timer / 25;
            text_timeout[ 15 ] = '0' + ( ( temp % 60 ) % 10 );
            text_timeout[ 14 ] = '0' + ( ( temp % 60 ) / 10 );
            temp /= 60;
            text_timeout[ 12 ] = '0' + ( ( temp % 60 ) % 10 );
            text_timeout[ 11 ] = '0' + ( ( temp % 60 ) / 10 );
            term_write( 1, 2, text_timeout, FONT_GREEN );
          }
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
        SDL_FillRect( screen, rect( &r, 0, 0, screen->w, screen->h ), 0 );
      }

      // TODO: add resolution (width) support
      if( speech_vis( &p_vis ) == 0 ) {
        for( temp = 0; temp < 636; temp += 4 ) {
          draw_color( 0x00, 0x3F, 0x00 );
          draw_wu( temp + 1, screen_h - 39 + p_vis[ temp ], temp + 5, screen_h - 39 + ( p_vis[ temp + 4 ] ) );
          draw_wu( temp - 1, screen_h - 41 + p_vis[ temp ], temp + 3, screen_h - 41 + ( p_vis[ temp + 4 ] ) );
          draw_wu( temp + 1, screen_h - 41 + p_vis[ temp ], temp + 5, screen_h - 41 + ( p_vis[ temp + 4 ] ) );
          draw_wu( temp - 1, screen_h - 39 + p_vis[ temp ], temp + 3, screen_h - 39 + ( p_vis[ temp + 4 ] ) );
          draw_color( 0x00, 0x00, 0x00 );
          draw_wu( temp + 2, screen_h - 38 + p_vis[ temp ], temp + 6, screen_h - 38 + ( p_vis[ temp + 4 ] ) );
        }
        draw_color( 0x00, 0x3F, 0x00 );
        draw_wu( temp + 1, screen_h - 39 + p_vis[ temp ], temp + 4, screen_h - 39 + ( p_vis[ temp + 3 ] ) );
        draw_wu( temp - 1, screen_h - 41 + p_vis[ temp ], temp + 2, screen_h - 41 + ( p_vis[ temp + 3 ] ) );
        draw_wu( temp + 1, screen_h - 41 + p_vis[ temp ], temp + 4, screen_h - 41 + ( p_vis[ temp + 3 ] ) );
        draw_wu( temp - 1, screen_h - 39 + p_vis[ temp ], temp + 2, screen_h - 39 + ( p_vis[ temp + 3 ] ) );
        draw_color( 0x00, 0x00, 0x00 );
        draw_wu( temp + 2, screen_h - 38 + p_vis[ temp ], temp + 5, screen_h - 38 + ( p_vis[ temp + 3 ] ) );
        draw_color( 0x3F, 0xFF, 0x3F );
        for( temp = 0; temp < 636; temp += 4 ) {
          draw_wu( temp, screen_h - 40 + p_vis[ temp ], temp + 4, screen_h - 40 + ( p_vis[ temp + 4 ] ) );
        }
        draw_wu( temp, screen_h - 40 + p_vis[ temp ], temp + 3, screen_h - 40 + ( p_vis[ temp + 3 ] ) );
      }

    } else {

      // Clear screen
      SDL_FillRect( screen, rect( &r, 0, 0, screen->w, screen->h ), 0 );
      // Draw logo
      SDL_BlitSurface( spr_logo, NULL, screen, rect( &r, ( screen_w - spr_logo->w ) >> 1, ( screen_h >> 1 ) - spr_logo->h + 80, 0, 0 ) );

      switch( state ) {
        case STATE_CONNECTING:
          if( statec == 0 ) {
            if( ++retry == MAX_RETRY ) {
              state = STATE_ERROR;
            } else {
              comm_send( pkt_helo, 4 );
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
          term_write( ( term_w - 22 ) >> 1, ( term_h >> 1 ) + 10, text_time, FONT_GREEN );
          if( statec == 0 ) {
            if( ++retry == MAX_RETRY ) {
              state = STATE_ERROR;
            } else {
              comm_send( pkt_time, 4 );
            }
          }
          break;
      }
      if( statec ) statec--; else statec = 100;

    }

    // Allow plugins to draw
    for( pid = 0; pid < MAX_PLUGINS && ( plug = plugs[ pid ] ) != NULL; pid++ )
      if( plug->draw ) plug->draw( screen );

    // Draw help overlay
    if( help_shown ) draw_box( ( term_w - 34 ) >> 1, ( term_h - 2 - help_count ) >> 1, 34, help_count + 2, screen );

    // Clear messages
    if( message_timeout ) {
      if( --message_timeout == 0 ) term_white( 1, term_h - 3, 38 );
    }

    // Draw terminal overlay
    term_draw();

    // Refresh screen
    SDL_UpdateRect( screen, 0, 0, 0, 0 );

    while( SDL_PollEvent( &event ) && !quit ) {
      switch( event.type ) {
        case SDL_QUIT:
          quit = 1; // Set time to quit
          break;

        case SDL_ACTIVEEVENT:
          if( event.active.state & SDL_APPINPUTFOCUS ) {
            if( event.active.gain == 1 ) {
              cursor_grab( fullscreen ); // Window gained focus
            } else {
              cursor_grab( 0 ); // Window lost focus
            }
          }
          break;

        case SDL_MOUSEMOTION:
          if( state == STATE_STREAMING && cursor_hook != NULL ) {
            plug = cursor_hook; // Notify plugin
            if( plug->cursor ) plug->cursor( E_MOVE, event.motion.x, event.motion.y );
          }
          break;

        case SDL_MOUSEBUTTONDOWN:
          if( cursor_hook == NULL ) {
            // Toggle cursor grabbing
            if( event.button.button == SDL_BUTTON_LEFT && !fullscreen ) {
              cursor_grab( !cursor_grabbed );
            }
          } else if( state == STATE_STREAMING ) {
            plug = cursor_hook; // Notify plugin
            if( plug->cursor ) plug->cursor( E_BUTTONDOWN, event.motion.x, event.motion.y );
          }
          break;

        case SDL_MOUSEBUTTONUP:
          if( state == STATE_STREAMING && cursor_hook != NULL ) {
            plug = cursor_hook; // Notify plugin
            if( plug->cursor ) plug->cursor( E_BUTTONUP, event.motion.x, event.motion.y );
          }
          break;

        case SDL_KEYDOWN:
          if( keyboard_hook == NULL ) {
            switch( event.key.keysym.sym ) {
              case SDLK_ESCAPE: // Quit
                quit = 1;
                break;

              case SDLK_f: // Toggle fullscreen
                fullscreen = !fullscreen;
                screen = SDL_SetVideoMode( screen_w, screen_h, screen_bpp, ( fullscreen ? SDL_FULLSCREEN : 0 ) );
                // TODO: need check if fullscreen was possible?
                cursor_grab( fullscreen );
                laststate = -1;
                break;

              case SDLK_l: // Switch keyboard layout
                set_layout( layout + 1 );
                break;

              case SDLK_h: // Toggle help
                draw_help( help_shown = !help_shown );
                break;

              default: // WASD control scheme
                if( event.key.keysym.sym == keymap[ KM_LEFT ] ) {
                  ctrl.ctrl.kb |= KB_LEFT;
                } else if( event.key.keysym.sym == keymap[ KM_RIGHT ] ) {
                  ctrl.ctrl.kb |= KB_RIGHT;
                } else if( event.key.keysym.sym == keymap[ KM_UP ] ) {
                  ctrl.ctrl.kb |= KB_UP;
                } else if( event.key.keysym.sym == keymap[ KM_DOWN ] ) {
                  ctrl.ctrl.kb |= KB_DOWN;
                } else if( state == STATE_STREAMING && keyboard_binds[ event.key.keysym.sym ] != NULL ) {
                  plug = keyboard_binds[ event.key.keysym.sym ]; // Notify plugin
                  if( plug->keyboard ) plug->keyboard( E_KEYDOWN, event.key.keysym.sym, unicode_ascii( event.key.keysym.unicode ) );
                }
                break;

            }
          } else if( state == STATE_STREAMING ) {
            plug = keyboard_hook;
            if( plug->keyboard ) plug->keyboard( E_KEYDOWN, event.key.keysym.sym, unicode_ascii( event.key.keysym.unicode ) );
          }
          break;
        case SDL_KEYUP:
          if( state == STATE_STREAMING && keyboard_hook == NULL ) {
            // WASD control scheme
            if( event.key.keysym.sym == keymap[ KM_LEFT ] ) {
              ctrl.ctrl.kb &= ~KB_LEFT;
            } else if( event.key.keysym.sym == keymap[ KM_RIGHT ] ) {
              ctrl.ctrl.kb &= ~KB_RIGHT;
            } else if( event.key.keysym.sym == keymap[ KM_UP ] ) {
              ctrl.ctrl.kb &= ~KB_UP;
            } else if( event.key.keysym.sym == keymap[ KM_DOWN ] ) {
              ctrl.ctrl.kb &= ~KB_DOWN;
            } else if( keyboard_binds[ event.key.keysym.sym ] != NULL ) {
              plug = keyboard_binds[ event.key.keysym.sym ]; // Notify plugin
              if( plug->keyboard ) plug->keyboard( E_KEYUP, event.key.keysym.sym, unicode_ascii( event.key.keysym.unicode ) );
            }
          } else if( keyboard_hook ) {
            plug = keyboard_hook; // Notify plugin
            if( plug->keyboard ) plug->keyboard( E_KEYUP, event.key.keysym.sym, unicode_ascii( event.key.keysym.unicode ) );
          }
          break;
      }
    }

    // Delay 1/CLIENT_RPS seconds, constantly correct for processing overhead
    time_diff = SDL_GetTicks() - time_target;
    if( time_diff > 10000 / CLIENT_RPS ) {
      printf( "RoboCortex [warning]: Cannot keep up with desired RPS\n" );
      time_target = SDL_GetTicks();
      time_diff = 0;
    }
    if( time_diff > 1000 / CLIENT_RPS ) {
      time_diff = 1000 / CLIENT_RPS;
    }
    time_target += 1000 / CLIENT_RPS;
    //printf( "Next target %i\n", time_target );
    SDL_Delay( ( 1000 / CLIENT_RPS ) - time_diff );
    //printf( "%i\n", ( 1000 / CLIENT_RPS ) - time_diff );

  }

  // Send QUIT
  comm_send( pkt_quit, 4 );

  // Clean up
  sprites_free();
  avcodec_close( pCodecCtx );
  SDL_DestroyMutex( trust_mx );
  SDL_FreeSurface( frame );
  SDL_Quit();

  printf( "RoboCortex [info]: KTHXBYE!\n" );

  exit( EXIT_OK );
}