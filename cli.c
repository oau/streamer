#include <stdio.h>
#include <sys/types.h>
#include <SDL/SDL.h>
#include "include/oswrap.h"
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include "include/cli_term.h"
#include "include/kiwiray.h"
#include "include/sam_queue.h"

#define MAX_RETRY       5
#define MAX_NO_H264   125 // milliseconds/20
#define KIWI_VERSION    2
#define PORT         6979
#define TIMEOUT_EXTRA  10 // milliseconds/20

enum state_e {
  STATE_CONNECTING,
  STATE_QUEUED,
  STATE_ERROR,
  STATE_FULL,
  STATE_LOST,
  STATE_VERSION,
  STATE_STREAMING
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
static char text_contact[] =  "CONTACTING KIWIRAY...";
static char text_error[]   =  "  CONNECTION ERROR!  ";
static char text_queued[] =   " QUEUED FOR CONTROL! ";
static char text_full[] =     "QUEUE IS FULL, SORRY!";
static char text_lost[] =     "   CONNECTION LOST   ";
static char text_quit[] =     "   PRESS Q TO QUIT   ";
static char text_version[] =  "  NEED NEWER CLIENT  ";
static char text_blank[] =    "                     ";
static char text_time[] =     "       0:00:00       ";
static char text_controls[] = "IN CONTROL - USE WASD+MOUSE";
static char text_timeout[] =  "TIME LEFT: 00:00";

static char pkt_h264[ 4 ] = { 0x00, 0x00, 0x00, 0x01 };
static char pkt_data[ 4 ] = "DATA";
static char pkt_helo[ 4 ] = "HELO";
static char pkt_time[ 4 ] = "TIME";
static char pkt_ctrl[ 4 ] = "CTRL";
static char pkt_lost[ 4 ] = "LOST";
static char pkt_full[ 4 ] = "FULL";
static char pkt_quit[ 4 ] = "QUIT";

static SDL_Surface* screen;
static NET_SOCK h_sock;
static linked_buf_t *p_buffer_wr;
static linked_buf_t *p_buffer_rd;
static int volatile state = STATE_CONNECTING;
static int volatile retry = 0;
static int queue_time;
linked_buf_t *extra_first = NULL;
linked_buf_t *extra_last = NULL;
unsigned char extra_id = 0;
SDL_mutex *extra_mx;

int extra_timeout = 0;

static ctrl_data_t ctrl;

void extradata( void* data, unsigned char size ) {
  linked_buf_t *extra;
  extra = malloc( sizeof( linked_buf_t ) );
  
  if( extra ) {

    // Create packet
    memcpy( extra->data, data, size );
    extra->size = size;
    extra->next = NULL;

    // Insert into linked list
    SDL_mutexP( extra_mx );
    if( extra_first ) {
      extra_last->next = extra;
    } else {
      extra_first = extra;
    }
    extra_last = extra;
    SDL_mutexV( extra_mx );
  }
}

char UnicodeChar( int uni ){
  #define INTERNATIONAL_MASK 0xFF80
  #define UNICODE_MASK       0x007F
  
  if( uni == 0 ) return( 0 );
  
  if( ( uni & INTERNATIONAL_MASK ) == 0 ) {
    return( ( char )( toupper( uni & UNICODE_MASK ) ) );
  } else {
    return( '?' );
  }
}
    
void resource_init() {
  SDL_Surface* temp;
  temp = SDL_LoadBMP( "logo.bmp" );
  if( !temp ) printf( "Unable to load logo.bmp\n" );    
  logo = SDL_DisplayFormat( temp );
  SDL_FreeSurface( temp );
}

void resource_clean() {
    SDL_FreeSurface( logo );
}

int receiver( void *unused ) {
  int temp, size;
  while( 1 ) {
    temp = sizeof( srv_addr );
    size = net_recv( &h_sock, p_buffer_wr->data, 32768, &srv_addr );
    if( size >= 4 ) {
      if( memcmp( p_buffer_wr->data, pkt_h264, 4 ) == 0 ) {
        // h264 packet
        state = STATE_STREAMING;
        retry = 0;
        p_buffer_wr->next = malloc( sizeof( linked_buf_t ) );
        linked_buf_t *p_buf = p_buffer_wr;
        p_buffer_wr = p_buffer_wr->next;
        p_buffer_wr->size = 0;
        p_buf->size = size;
      } else if( memcmp( p_buffer_wr->data, pkt_data, 4 ) == 0 ) {
        if( size >= 4 + sizeof( disp_data_t ) ) {
          memcpy( &disp_data, p_buffer_wr->data + 4, sizeof( disp_data_t ) );
          
          SDL_mutexP( extra_mx );
          if( extra_first ) {
            if( disp_data.extra_id == extra_id ) {
              linked_buf_t* p_extra = extra_first;
              extra_first = extra_first->next;
              extra_id++;
              extra_timeout = 0;
              free( p_extra );
            }
          }
          SDL_mutexV( extra_mx );
          
        }
      } else if( memcmp( p_buffer_wr->data, pkt_helo, 4 ) == 0 ) {
        // HELO response
        if( state == STATE_CONNECTING ) {
          if( p_buffer_wr->data[ 4 ] != KIWI_VERSION ) {
            state = STATE_VERSION;
          } else {
            state = STATE_QUEUED;
            retry = 0;
            queue_time = *( int* )&p_buffer_wr->data[ 5 ];
          }
        }
      } else if( memcmp( p_buffer_wr->data, pkt_time, 4 ) == 0 ) {
        // TIME response
        if( state == STATE_QUEUED ) {
          retry = 0;
          queue_time = *( int* )&p_buffer_wr->data[ 4 ];
        }
      } else if( memcmp( p_buffer_wr->data, pkt_lost, 4 ) == 0 ) {
        // Connection LOST response
        state = STATE_LOST;
      } else if( memcmp( p_buffer_wr->data, pkt_full, 4 ) == 0 ) {
        // Queue FULL response
        state = STATE_FULL;
      }
    }
  }
}

// Used by DrawWuLine
void DrawPixel( short x, short y, short c ) {
	if( y > 479 ) return;
	c = ~c;
	Uint32 cv = SDL_MapRGB( screen->format, c, c, c );
	( *( Uint32* )( ( ( uint8_t* )screen->pixels ) + ( y * screen->pitch + x * 4 ) ) ) = cv;
}

// Wu-Line implementation, courtesy of http://www.codeproject.com/KB/GDI/antialias.aspx
void DrawWuLine( short X0, short Y0, short X1, short Y1 ) {
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

//int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow ) {
int main( int argc, char *argv[] ) {
  int                temp;
  int                statec = 0;
  int                laststate = -1;
  char               text[ 256 ];
  char               l_text = -1;
  char               c_text;
  char               p_ctrl[ 8192 ];
  int                i_ctrl;
  AVCodecContext    *pCodecCtx;
  AVCodec           *pCodec;
  AVFrame           *pFrame;
  struct SwsContext *convertCtx;
  AVPacket           avpkt;
  SDL_Thread        *hReceiver;
  SDL_Rect           dstrect;
  SDL_Surface       *live = NULL;
  char						  *p_vis;
  Uint32 rmask, gmask, bmask, amask;
  SDL_Event event;
  int quit = 0;
  int mx, my;
  
  printf( "KiwiRay Client Initializing...\n" );

  if( argc < 2 ) {
    printf( "Need to specify IP-address on command-line\n" );
    printf( "\n" );
    return( 1 );
  }

  if( net_init() < 0 ) {
    printf( "Network initialization failed\n" );
    return( 1 );
  }

  if( net_sock( &h_sock ) < 0 ) {
    printf( "Socket aquire failed\n" );
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
    printf( "Unable to initialize decoder\n" );
    return( 5 );
  }
  avcodec_open( pCodecCtx, pCodec );  

  // Allocate decoder frame
  pFrame = avcodec_alloc_frame();

  SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO );
  SDL_WM_SetCaption( "KiwiRay Client", "KiwiRay Client" );
  screen = SDL_SetVideoMode( 640, 480, 32, 0 ); //SDL_FULLSCREEN );
  
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
    printf( "Unable to allcate SDL surface\n" );
    return( 1 );
  }
  
  p_buffer_wr = malloc( sizeof( linked_buf_t ) );;
  p_buffer_rd = p_buffer_wr;
  p_buffer_wr->size = 0;
  
  sam_open();
  
  extra_mx = SDL_CreateMutex();
  
  hReceiver = SDL_CreateThread( receiver, NULL );
  
  while( !quit ) {
    
    sam_poll();

    SDL_GetRelativeMouseState( &mx, &my );
    SDL_WarpMouse( 320, 240 );
    ctrl.ctrl_mx += mx;
    ctrl.ctrl_my += my;
    
    char xxx[ 100 ];
    sprintf( xxx, "%i, %i        ", ctrl.ctrl_mx, ctrl.ctrl_my );
    term_write( 1, 10, xxx, 1 );

    if( state != laststate ) {
      l_text = -1;
      term_crem();
      laststate = state;
      // Clear screen
      dstrect.x = 0;
      dstrect.y = 0;
      dstrect.w = 640;
      dstrect.h = 480;
      SDL_FillRect( screen, &dstrect, 0 );
      term_clear();
      // Initialize view
      if( state != STATE_STREAMING ) {
        dstrect.x = 170;
        dstrect.y = 60;
        SDL_BlitSurface( logo, NULL, screen, &dstrect );
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
      
      memcpy( p_ctrl + 4, &ctrl, sizeof( ctrl_data_t ) );
      i_ctrl += sizeof( ctrl_data_t );

      SDL_mutexP( extra_mx );
      if( extra_timeout == 0 ) {
        // Extra information?
        if( extra_first ) {
          // Identifier
          p_ctrl[ i_ctrl++ ] = extra_id;
          // Data
          memcpy( p_ctrl + i_ctrl, extra_first->data, extra_first->size );
          i_ctrl += extra_first->size;
          extra_timeout = TIMEOUT_EXTRA;
        }
      } else {
        extra_timeout--;
      }
      SDL_mutexV( extra_mx );
    
      net_send( &h_sock, p_ctrl, i_ctrl, &srv_addr );
      if( ++retry == MAX_NO_H264 ) state = STATE_LOST;
      
      if( p_buffer_rd->size ) {

        // Decode frame
        avpkt.data = p_buffer_rd->data;
        avpkt.size = p_buffer_rd->size;
        avpkt.flags = AV_PKT_FLAG_KEY;
        if( avcodec_decode_video2( pCodecCtx, pFrame, &temp, &avpkt ) < 0 ) {
          printf( "Decoding error\n" );
        } else {
          SDL_LockSurface( frame );      

          const uint8_t * data[1] = { frame->pixels };
          int linesize[1] = { frame->pitch };

          convertCtx = sws_getContext( pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
            frame->w, frame->h, PIX_FMT_BGR24, SWS_AREA, NULL, NULL, NULL);

          sws_scale( convertCtx, (const uint8_t**) pFrame->data, pFrame->linesize, 0,
            pCodecCtx->height, (uint8_t * const*) data, linesize );

          sws_freeContext( convertCtx );

          SDL_UnlockSurface( frame );
          
          if( live ) SDL_FreeSurface( live );
          live = SDL_DisplayFormat( frame );
    
          temp = disp_data.timer / 25;
          text_timeout[ 15 ] = '0' + ( ( temp % 60 ) % 10 );
          text_timeout[ 14 ] = '0' + ( ( temp % 60 ) / 10 );
          temp /= 60;
          text_timeout[ 12 ] = '0' + ( ( temp % 60 ) % 10 );
          text_timeout[ 11 ] = '0' + ( ( temp % 60 ) / 10 );
          term_write( 1, 1, text_controls, 0 );
          term_write( 1, 2, text_timeout, 0 );
          
        }
        linked_buf_t *p_buf = p_buffer_rd;
        p_buffer_rd = p_buffer_rd->next;
        free( p_buf );
        
      }
      
      if( live ) {
      	// Draw video
      	SDL_BlitSurface( live, NULL, screen, NULL );
      } else {
	      // Clear screen
	      dstrect.x = 0;
	      dstrect.y = 0;
	      dstrect.w = 640;
	      dstrect.h = 480;
	      SDL_FillRect( screen, &dstrect, 0 );
      }
      
      if( sam_vis( &p_vis ) == 0 ) {
      	// TODO: draw visualization
      	for( temp = 0; temp < 636; temp += 4 ) {
      		DrawWuLine( temp, 440 + p_vis[ temp ], temp + 4, 440 + ( p_vis[ temp + 4 ] ) );
      	}
      	DrawWuLine( temp, 440 + p_vis[ temp ], temp + 3, 440 + ( p_vis[ temp + 3 ] ) );
      }

    } else {      

      switch( state ) {
        case STATE_CONNECTING: // connecting
          if( statec == 0 ) {
            if( ++retry == MAX_RETRY ) {
              state = STATE_ERROR;
            } else {
              net_send( &h_sock, pkt_helo, 4, &srv_addr );
            }
          }
          break;
        case STATE_QUEUED: // queued
          temp = queue_time / 25;
          text_time[ 13 ] = '0' + ( ( temp % 60 ) % 10 );
          text_time[ 12 ] = '0' + ( ( temp % 60 ) / 10 );
          temp /= 60;
          text_time[ 10 ] = '0' + ( ( temp % 60 ) % 10 );
          text_time[  9 ] = '0' + ( ( temp % 60 ) / 10 );
          temp /= 60;
          text_time[  7 ] = '0' + temp;
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

    term_draw();

    SDL_UpdateRect( screen, 0, 0, 0, 0 );
    
    while( SDL_PollEvent( &event ) && !quit ) {
      switch( event.type ) {
        case SDL_QUIT:
          quit = 1;
          break;
        case SDL_KEYDOWN:
          if( l_text >= 0 ) {
            switch( event.key.keysym.sym ) {
              case SDLK_RETURN:
                if( l_text > 0 ) {
                  // Send to server
                  memset( text + strlen( text ), 0, 255 - strlen( text ) );
                  extradata( text, strlen( text ) );
                  // Queue for local playback
                  sam_queue( text );
                }
                	
              case SDLK_ESCAPE:
                l_text = -1;
                text[ l_text ] = 0;
                term_white( 1, 28, 38 );
                term_crem();
                break;
              case SDLK_BACKSPACE:
                if( l_text > 0 ) {
                  text[ --l_text ] = 0;
                  term_white( l_text + 2, 28, 1 );
                  term_cins( l_text + 2, 28 );
                }
                break;
              default:
                c_text = UnicodeChar( event.key.keysym.unicode );
                if( term_knows( c_text ) && l_text < 37 ) {
                  text[ l_text++ ] = c_text;
                  text[ l_text ] = 0;
                  term_write( l_text + 1, 28, &text[ l_text - 1 ], 0 );
                  term_cins( l_text + 2, 28 );
                }
                
            }
          } else {
            switch( event.key.keysym.sym ) {
              case SDLK_q:
                quit = 1;
                break;
              case SDLK_t:
                if( state == STATE_STREAMING ) {
                  l_text = 0;
                  text[ 0 ] = 0x00;
                  term_write( 1, 28, ">", 0 );
                  term_cins( 2, 28 );
                  ctrl.ctrl_kb = 0;
                }
                break;
              case SDLK_a:
                ctrl.ctrl_kb |= KB_LEFT;
                break;
              case SDLK_d:
                ctrl.ctrl_kb |= KB_RIGHT;
                break;
              case SDLK_w:
                ctrl.ctrl_kb |= KB_UP;
                break;
              case SDLK_s:
                ctrl.ctrl_kb |= KB_DOWN;
                break;
            }
          }
          break;
        case SDL_KEYUP:
          switch( event.key.keysym.sym ) {
            case SDLK_a:
              ctrl.ctrl_kb &= ~KB_LEFT;
              break;
            case SDLK_d:
              ctrl.ctrl_kb &= ~KB_RIGHT;
              break;
            case SDLK_w:
              ctrl.ctrl_kb &= ~KB_UP;
              break;
            case SDLK_s:
              ctrl.ctrl_kb &= ~KB_DOWN;
              break;
          }
          break;
      }
    }

  	SDL_Delay( 20 );  
  }
  net_send( &h_sock, pkt_quit, 4, &srv_addr );

  resource_clean();
  avcodec_close( pCodecCtx );
  SDL_DestroyMutex( extra_mx );
  SDL_FreeSurface( frame );
  SDL_Quit();
}
