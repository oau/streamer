#include <stdio.h>
#include "robocortex.h"
#include "oswrap.h"
#include <SDL/SDL_keysym.h>
#include <SDL/SDL_video.h>

enum event_e {
  E_KEYDOWN,
  E_KEYUP,
  E_BUTTONDOWN,
  E_BUTTONUP,
  E_MOVE
};

typedef struct {
  // Binds the specified key
  int ( *key_bind )( int key );
  // Free (un-bind) the specified key
  void ( *key_free )( int key );
  // Hooks the keyboard (piping all keyboard events to plugin)
  int ( *keyboard_hook )();
  // Release (un-hook) the keyboard
  void ( *keyboard_release )();
  // Hooks the cursor (piping all cursor events to plugin)
  int ( *cursor_hook )( int show );
  // Release (un-hook) the cursor
  void ( *cursor_release )();
  // Move cursor (only when hooked)
  void ( *cursor_move )( int x, int y );
  // Insert flashing text caret at specified location
  void ( *text_cins )( unsigned char x, unsigned char y );
  // Remove flashing text caret
  void ( *text_crem )();
  // Write text to screen
  void ( *text_write )( unsigned char x, unsigned char y, char *text, unsigned char font );
  // Clears text from screen
  void ( *text_clear )( unsigned char x, unsigned char y, unsigned char count );
  // Returns whether the character is valid as text or not
  int ( *text_valid )( char ascii );
  // Send a data packet to the server
  void ( *server_send )( void* data, unsigned char size );
  // Adds a line of text (max 32 characters) to the help window
  void ( *help_add )( char* text );
  // Use TTS to play back the specified text
  void ( *speak_text )( char* );
  // Draws wu-lines
  void ( *draw_wuline )( int x0, int y0, int x1, int y1, uint32_t color );
} pluginhost_t;

typedef struct {
  // Plugin identifier - must be unique, but match on server/client side
  uint32_t ident;
  // Called once, after plugin_open, to initialize the plugin
  void ( *init       )();
  // Called when shutting down, release all resources
  void ( *close      )();
  // Called when a data packet is received from the server
  void ( *recv       )( void *data, unsigned char size );
  // Called for each frame, allows plugin to draw to screen
  void ( *draw       )( SDL_Surface *screen );
  // Called when a keyboard event occurs
  void ( *keyboard   )( int event, int key, char ascii );
  // Called when a cursor event occurs
  void ( *cursor     )( int event, int x, int y );  
  // Called when an event has occurred causing all hooks to be lost
  void ( *lost       )();
} pluginclient_t;