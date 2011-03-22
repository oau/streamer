#include <SDL/SDL.h>
#include <stdio.h>
#include "include/cli_term.h"

typedef struct {
  unsigned char cchar;
  unsigned char nchar;
  unsigned char cfont;
  unsigned char nfont;
  int tick;
} termchar_t;

static termchar_t term[ 40 ][ 30 ];
static SDL_Surface *font[ 2 ];
static const char font_chars[] = "#!\"c % '()|+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static unsigned char write_index = 0;
static SDL_Surface *screen;
static char cx = -1, cy, ct;

#define ABS( v ) ( v < 0 ? -v : v )

void term_cins( unsigned char x, unsigned char y ) {
  cx = x;
  cy = y;
  ct = 0;
}

void term_crem() {
  cx = -1;
}

void term_init( SDL_Surface *scr ) {
  SDL_Surface* temp;
  char *p_pixel;

  screen = scr;

  temp = SDL_LoadBMP( "font1.bmp" );
  if( !temp ) printf( "KiwiDriveClient [error]: Unable to load font1.bmp\n" );
  font[ 0 ] = SDL_DisplayFormat( temp );
  SDL_FreeSurface( temp );
  
  temp = SDL_LoadBMP( "font2.bmp" );
  if( !temp ) printf( "KiwiDriveClient [error]: Unable to load font2.bmp\n" );
  font[ 1 ] = SDL_DisplayFormat( temp );
  SDL_FreeSurface( temp );
  
  SDL_SetColorKey( font[ 0 ], SDL_SRCCOLORKEY, SDL_MapRGB( screen->format, 0xFF, 0x00, 0xFF ) );
  SDL_SetColorKey( font[ 1 ], SDL_SRCCOLORKEY, SDL_MapRGB( screen->format, 0xFF, 0x00, 0xFF ) );
}

void term_clear() {
  memset( term, 0, sizeof( term ) );
}

void term_draw() {
  unsigned char x, y;
  unsigned char i, j;
  write_index = 0;
  SDL_Rect srcrect;
  SDL_Rect dstrect;
  srcrect.w = 16;
  srcrect.h = 16;
  for( y = 0; y < 30; y++ ) {
    for( x = 0; x < 40; x++ ) {
      if( term[ x ][ y ].tick < 15 ) {
        if( ++term[ x ][ y ].tick == 0 ) {
          term[ x ][ y ].cchar = term[ x ][ y ].nchar;
          term[ x ][ y ].cfont = term[ x ][ y ].nfont;
        }
      }

      if( term[ x ][ y ].cchar ) {
        dstrect.x = x << 4;
        dstrect.y = y << 4;
        srcrect.x = term[ x ][ y ].cchar << 4;
        srcrect.y = ( term[ x ][ y ].tick >= 0 ? term[ x ][ y ].tick << 4 : 240 );
        SDL_BlitSurface( font[ term[ x ][ y ].cfont ], &srcrect, screen, &dstrect );
      }
    }
  }
  if( cx >= 0 ) { 
    srcrect.x = 0;
    srcrect.y = ( ct > 15 ? 240 : ct << 4 );
    dstrect.x = cx << 4;
    dstrect.y = cy << 4;
    SDL_BlitSurface( font[ 0 ], &srcrect, screen, &dstrect );
    if( ++ct == 32 ) ct = 0;
  }
}

void term_write( unsigned char x, unsigned char y, char *s, unsigned char f ) {
  unsigned char i, j, t = 0;
  int len = strlen( s );
  for( i = 0; i < len; i++ ) {
    for( j = 0; j < sizeof( font_chars ) - 1; j++ ) {
      if( font_chars[ j ] == s[ i ] ) {
        if( term[ x ][ y ].nchar != j || term[ x ][ y ].nfont != f ) {
          term[ x ][ y ].nchar = j;
          term[ x ][ y ].nfont = f;
          if( t == 0 && write_index == 0 ) {
            term[ x ][ y ].cchar = j;
            term[ x ][ y ].cfont = f;
            term[ x ][ y ].tick = 0;
          } else {
            term[ x ][ y ].tick = -( t + write_index * 5 );
          }
          t++;
        }
        x++;
        break;
      }
    }
  }
  write_index++;
}

void term_white( unsigned char x, unsigned char y, unsigned char c ) {
  while( c-- ) {
    memset( &term[ x++ ][ y ], 0, sizeof( termchar_t ) );
  }
}

void term_close() {
  SDL_FreeSurface( font[ 0 ] );
  SDL_FreeSurface( font[ 1 ] );
}

int term_knows( char c ) {
    int j;
    for( j = 0; j < sizeof( font_chars ) - 1; j++ ) {
      if( font_chars[ j ] == c ) return( 1 );
    }
    return( 0 );
}