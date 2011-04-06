#ifndef _ROBOCORTEX_H_
#define _ROBOCORTEX_H_
#include "SDL/SDL_video.h"

#define CORTEX_VERSION       3 // Current protocol revision
#define CFG_TOKEN_MAX_SIZE  32 // Maxmimum length of a token value
#define CFG_VALUE_MAX_SIZE 256 // Maxmimum length of a configuration value

enum font_e {
  FONT_GREEN,
  FONT_RED
};

enum kb_bitmask_e {
  KB_LEFT  = 1,
  KB_RIGHT = 2,
  KB_UP    = 4,
  KB_DOWN  = 8
};

// DISP packet
typedef struct {
  unsigned char trust_srv;
  unsigned char trust_cli;
  int timer;
} disp_data_t;

// Control data
typedef struct {
  long mx;
  long my;
  unsigned char kb; // kb_bitmask_e
} ctrl_t;

// DATA packet
typedef struct {
  unsigned char trust_srv;
  unsigned char trust_cli;
  ctrl_t ctrl;
} ctrl_data_t;

// Linked buffer
struct linked_buf_t {
  char data[ 8192 ];
  int size;
  struct linked_buf_t *next;
};
typedef struct linked_buf_t linked_buf_t;

extern char *config_rc;

extern int config_read_line( char **value, char **token, FILE *f );
extern int config_find_line( char **find_value, char *find_token, FILE *f );
extern int config_plugin( char *ident, char* dst, char* req_token );
extern void config_parse( int( *callback )( char*, char* ) );
extern SDL_Rect *rect( SDL_Rect *r, int x, int y, int w, int h );
#endif