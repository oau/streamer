#include <stdio.h>
#include <string.h>
#include "robocortex.h"

char *config_rc;

static char line[ CFG_VALUE_MAX_SIZE + CFG_TOKEN_MAX_SIZE + 3 ] = { ' ' };

// Reads next token=value pair from .rc file
int config_read_line( char **value, char **token, FILE *f ) {
  char *es, *ps, *pe, *pl;
  while( !feof( f ) ) {
    if( fgets( line + 1, sizeof( line ) - 1, f ) != NULL ) {
      // Remove line endings
      es = line; while( *es != 0 ) if( *es == 10 || *es == 13 ) *es++ = ' '; else es++;
      // Find entry start
      es = line; while( *es == ' ' && *es != 0 ) if( *++es == '#' ) *es = 0;
      if( *es != 0  ) {
        // Find entry end
        pe = es; while( *pe != ' ' && *pe != 0 ) if( *++pe == '#' ) *pe = 0;
        if( *pe != 0 ) {
          // Null-terminate
          *pe = 0;
          // Find parameter start
          ps = pe + 1; while( *ps == ' ' && *ps != 0 ) if( *++ps == '#' ) *ps = 0;
          if( *ps != 0 ) {
            // Find parameter end
            pe = ps;
            while( *pe != 0 ) {
              if( *pe != ' ' ) pl = pe;
              if( *++pe == '#' ) *pe = 0;
            }
            // Null-terminate
            *++pl = 0;
            //printf( "Configuration [info]: %s=%s\n", es, ps );
            *token = es;
            *value = ps;
            return( 1 );
          }
        }
      }
    }
  }
  return( 0 );
}

// Search for and return the specified token(=value) pair
int config_find_line( char **find_value, char *find_token, FILE *f ) {
  char *value, *token;
  while( config_read_line( &value, &token, f ) ) {
    if( strcmp( token, find_token ) == 0 ) {
      if( *find_value == NULL ) {
        strcpy( *find_value, value );
        return( 1 );
      } else {
        if( strcmp( value, *find_value ) == 0 ) {
          return( 1 );
        }
      }
    }
  }
  return( 0 );
}

// Opens configuration file
static FILE* config_open() {
  FILE *cf;
  cf = fopen( config_rc, "r" );
  if( cf == NULL ) printf( "Config [error]: Cannot open %s\n", config_rc );
  return( cf );
}

// Reads plugin configuration
int config_plugin( uint32_t i_ident, char* dst, char* req_token ) {
  int ret = 0;
  char *value, *token;
  char ident[ 5 ], *p_ident = ident;
  *( uint32_t* )ident = i_ident;
  ident[ 4 ] = 0;
  FILE *cf;
  if( cf = config_open() ) {
    if( config_find_line( &p_ident, "plugin", cf ) ) {
      while( config_read_line( &value, &token, cf ) ) {
        if( strcmp( token, "plugin" ) == 0 ) break;
        if( strcmp( token, req_token ) == 0 ) {
          strcpy( dst, value );
          ret = 1;
          break;
        }
      }
    }
    fclose( cf );
  }
  return( ret );
}

// Parse whole file using callback
void config_parse( int( *callback )( char*, char* ) ) {
  char *value, *token;
  FILE *cf;
  if( cf = config_open() ) {
    printf( "Config [info]: Reading rc...\n" );
    while( config_read_line( &value, &token, cf ) ) {
      if( callback( value, token ) ) break;
    }
    callback( NULL, NULL );
    fclose( cf );
  }
}

// Fills an SDL_Rect struct and returns its pointer
SDL_Rect *rect( SDL_Rect *r, int x, int y, int w, int h ) {
  r->x = x; r->y = y; r->w = w; r->h = h;
  return( r );
}

// Convert unicode to uppercase only ascii - kind of a hack
char unicode_ascii( int uni ) {
  #define INTERNATIONAL_MASK 0xFF80
  #define UNICODE_MASK       0x007F
  if( uni == 0 ) return( 0 );
  if( ( uni & INTERNATIONAL_MASK ) == 0 ) {
    return( ( char )( toupper( uni & UNICODE_MASK ) ) );
  } else {
    return( 0 );
  }
}