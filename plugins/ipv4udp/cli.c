#include "cli.h" // This is a client plugin

#define PORT                6979        // Default port

static pluginclient_t  ipv4udp;         // Plugin descriptor
static   pluginhost_t *host;            // RoboCortex descriptor

static       uint32_t  server;          // Server ip
static            int  port = PORT;     // Server port

static            int  initialized;     // Successful initialization

static       NET_SOCK  h_sock;          // Socket
static       NET_ADDR  srv_addr;        // Servers address and port

static           char  buffer[ 8192 ];  // Receive buffer
static            int  size;            // Packet size

static           void *h_thread;        // Receive thread handle

// Receives IPv4 UDP packets and passes them to RoboCortex
int receiver() {
  if( !initialized ) return( 1 );
  while( 1 ) {
    size = net_recv( &h_sock, buffer, 8192, &srv_addr );
    if( size >= 4 ) host->comm_recv( buffer, size );
  }
  return( 0 );
}

// Sends IPv4 UDP packets when called by RoboCortex
void sender( char* data, int size ) {
  if( !initialized ) return;
  net_send( &h_sock, data, size, &srv_addr );
}

// Initializes IPv4 network and sets up UDP socket
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
      // Set server
      net_addr_init( &srv_addr, server, port );
    }
  }

  initialized = 1;

  // Binding this function late allows RoboCortex to detect successful initialization
  ipv4udp.comm_send = sender;

  h_thread = host->thread_start( receiver );
}

// Frees allocated resources
static void closer() {
  if( h_thread ) host->thread_stop( h_thread );
}

// Sets up the plugin descriptor
pluginclient_t *ipv4udp_open( pluginhost_t *p_host ) {
  memcpy( &ipv4udp.ident, "UDP4", 4 );
  host = p_host;
  ipv4udp.close      = closer;
  ipv4udp.init       = init;
  return( &ipv4udp );
}
