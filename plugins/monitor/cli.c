#include "cli.h" // This is a client plugin

static pluginclient_t  monitor;      // Plugin descriptor
static pluginhost_t   *host;            // RoboCortex descriptor

static           char  p_text[ 256 ];   // Input buffer
static           char  l_text = -1;     // Current size of p_text

// Hooks were lost, reset input
static void lost_hooks() {
  host->text_clear( 1, host->text_rows - 2, 38 );
  host->text_crem();
  l_text = -1;
}

// Handles keyboard events
static void key_event( int event, int key, char ascii ) {
  char cmd[] = "/CAMERA n";
  if( event == E_KEYDOWN ) {
    if( key >= SDLK_0 || key <= SDLK_9 ) {
      cmd[ 8 ] = key;
      host->server_send( cmd, 9 );
    }
  }
}

// Initializes hooks and help
static void init() {
  int n;
  for( n = SDLK_0; n <= SDLK_9; n++ ) host->key_bind( n );
  host->help_add( "0: CAMERA OVERVIEW" );
  host->help_add( "1-9: SELECT CAMERA" );
}

// Handles messages from server plugin
static void message( void* data, unsigned char size ) {
  char msg[ 256 ];
  memcpy( msg, data, size );
  msg[ size ] = 0;
  host->draw_message( msg );
}

// Sets up the plugin descriptor
pluginclient_t *monitor_open( pluginhost_t *p_host ) {
  memcpy( &monitor.ident, "MON9", 4 );
  host = p_host;
  monitor.init       = init;
  monitor.keyboard   = key_event;
  monitor.lost       = lost_hooks;
  monitor.recv       = message;
  return( &monitor );
}