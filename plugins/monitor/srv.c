#include "srv.h" // This is a server plugin

static pluginclient_t  monitor;         // Plugin descriptor
static  pluginhost_t  *host;            // RoboCortex descriptor
static unsigned char   active = 255;
// Handles packets received from client plugin
static void process_data( void* p_data, unsigned char size ) {
  int n;
  char data[ 256 ];
  memcpy( data, p_data, size );
  data[ size ] = 0;
  
  if( strcmp( data, "/CAMERA 0" ) >= 0 && strcmp( data, "/CAMERA 9" ) <= 0 ) {
    active = data[ 8 ] - '0';
    active--;
  } else {
    memmove( data + 17, data, size );
    memcpy( data, "UNKNOWN COMMAND: ", 17 );
    host->client_send( data, size + 17 );
  }
}

// Tick: updates cameras
static void tick() {
  int n;
  float f;
  SDL_Rect src, dst;
  int w, h, e;
  int block = 0;
  
  for( n = 0; n < host->cap_count; n++ ) {
    if( n != active ) {
      host->cap_get( n, &w, &h, &e, &src, &dst );
      f = MIN( 1, MAX( 1.0/3, ( float )dst.w / ( float )host->stream_w ) );
      if( f > 1.0/3 ) f = MAX( 1.0/3, f * 0.95 );
      if( f > 1.1/3 ) block = 1;
      dst.x = ( ( n % 3 ) * ( host->stream_w / 3 ) ) * ( 1 - ( ( f - (1.0/3) ) / (2.0/3) ) );
      dst.y = ( ( n / 3 ) * ( host->stream_h / 3 ) ) * ( 1 - ( ( f - (1.0/3) ) / (2.0/3) ) );
      dst.w = host->stream_w * f;
      dst.h = host->stream_h * f;
      host->cap_set( n, CAP_ENABLE, &src, &dst );
    }
  }  

  if( active < host->cap_count && !block ) {  
    host->cap_get( active, &w, &h, &e, &src, &dst );
    f = MIN( 1, MAX( 1.0/3, ( float )dst.w / ( float )host->stream_w ) );
    if( f < 1 ) f = MIN( 1.0, f / 0.95 );
    dst.x = ( ( active % 3 ) * ( host->stream_w / 3 ) ) * ( 1 - ( ( f - (1.0/3) ) / (2.0/3) ) );
    dst.y = ( ( active / 3 ) * ( host->stream_h / 3 ) ) * ( 1 - ( ( f - (1.0/3) ) / (2.0/3) ) );
    dst.w = host->stream_w * f;
    dst.h = host->stream_h * f;
    host->cap_set( active, CAP_ENABLE, &src, &dst );
    host->cap_zorder( active, host->cap_count );
  }
}

static void init() {
  int n;
  SDL_Rect src, dst;
  int w, h, e;
  for( n = 0; n < host->cap_count; n++ ) {
    host->cap_get( n, &w, &h, &e, &src, &dst );
    dst.x = ( n % 3 ) * ( host->stream_w / 3 );
    dst.y = ( n / 3 ) * ( host->stream_h / 3 );
    dst.w = ( host->stream_w / 3 );
    dst.h = ( host->stream_h / 3 );
    host->cap_set( n, CAP_ENABLE, &src, &dst );
  }
}

// Sets up the plugin descriptor
pluginclient_t *monitor_open( pluginhost_t *p_host ) {
  memcpy( &monitor.ident, "MON9", 4 );
  host = p_host;
  monitor.init       = init;
  monitor.tick       = tick;
  monitor.recv       = process_data;
  return( &monitor );
}
