#include "cli.h"

#define PORT           6979 // Default port

static pluginclient_t  ipv4udp;
static   pluginhost_t *host;

static       uint32_t  server; // Server ip

static            int  port = PORT;

static            int  initialized;

static       NET_SOCK  h_sock;
static       NET_ADDR  srv_addr;
static       NET_ADDR  cli_addr;

static           char  buffer[ 8192 ];
static            int  size;

static           void *h_thread;

// This thread receives IPv4 UDP packets
int receiver() {
  if( !initialized ) return( 1 );
  while( 1 ) {
    size = net_recv( &h_sock, buffer, 8192, &srv_addr );
    if( size >= 4 ) host->comm_recv( buffer, size );
  }
  return( 0 );
}

// This function sends IPv4 UDP packets
void sender( char* data, int size ) {
  if( !initialized ) return;
  net_send( &h_sock, data, size, &srv_addr );
}

static void init() {
  char temp[ CFG_VALUE_MAX_SIZE ];

  // Configuration
  if( host->cfg_read( temp, "server" ) ) server = net_dtoa( temp );
  if( host->cfg_read( temp, "port" ) ) port = atoi( temp );
    
  if( !server ) {
    printf( "IPv4 UDP [error]: No server specified/wrong format\n" );
    return;
  }

  // Initialize network
  if( net_init() < 0 ) {
    fprintf( stderr, "IPv4 UDP [error]: Network initialization failed\n" );
    return;
  } else {
    // Aquire socket
    if( net_sock( &h_sock ) < 0 ) {
      fprintf( stderr, "IPv4 UDP  [error]: Socket aquire failed\n" );
      return;
    } else {
      net_addr_init( &srv_addr, server, port );
    }
  }
  initialized = 1;
  ipv4udp.comm_send = sender;
  h_thread = host->thread_start( receiver );  
}

static void close() {
  if( h_thread ) host->thread_stop( h_thread );
}

// Called when plugin is loaded
// Allocate required resources
pluginclient_t *ipv4udp_open( pluginhost_t *p_host ) {
  memcpy( &ipv4udp.ident, "UDP4", 4 );
  host = p_host;
  ipv4udp.close      = close;
  ipv4udp.init       = init;
  return( &ipv4udp );
}