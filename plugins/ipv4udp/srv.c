#include "srv.h" // This is a server plugin

#define PORT                6979        // Default port

static pluginclient_t  ipv4udp;         // Plugin descriptor
static   pluginhost_t *host;            // RoboCortex descriptor

static            int  port = PORT;     // Default port

static            int  initialized;     // Successful initialization

static       NET_SOCK  h_sock;          // Socket
static       NET_ADDR  srv_addr;        // Servers address and port
static       NET_ADDR  cli_addr;        // Client address and port

static           char  buffer[ 8192 ];  // Receive buffer
static            int  size;            // Packet size

static           void *h_thread;        // Receive thrad handle

static       remote_t  remote = { &cli_addr, sizeof( NET_ADDR ), &ipv4udp };

// Receives IPv4 UDP packets and passes them to RoboCortex
int receiver() {
  if( !initialized ) return( 1 );
  while( 1 ) {
    net_addr_init( &cli_addr, NET_ADDR_ANY, 0 );
    size = net_recv( &h_sock, buffer, 8192, &cli_addr );
    if( size >= 4 ) comm_recv( buffer, size, &remote );
  }
  return( 0 );
}

// Sends IPv4 UDP packets when called by RoboCortex
void sender( char* data, int size, remote_t *remote ) {
  if( !initialized ) return;
  net_send( &h_sock, data, size, ( NET_ADDR* )remote->addr );
}

// Initializes IPv4 network and sets up UDP server socket
static void init() {
  char temp[ CFG_VALUE_MAX_SIZE ];

  // Configuration
  if( host->cfg_read( temp, "port" ) ) port = atoi( temp );

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
      // Bind socket to PORT
      net_addr_init( &srv_addr, NET_ADDR_ANY, port );
      if( net_bind( &h_sock, &srv_addr ) < 0 ) {
        fprintf( stderr, "IPv4 UDP  [error]: Socket bind failed\n" );
        return;
      }
    }
  }
  initialized = 1;
  h_thread = host->thread_start( receiver );
}

// Frees allocated resources
static void close() {
  if( h_thread ) host->thread_stop( h_thread );
}

// Sets up the plugin descriptor
pluginclient_t *ipv4udp_open( pluginhost_t *p_host ) {
  memcpy( &ipv4udp.ident, "UDP4", 4 );
  host = p_host;
  ipv4udp.close      = close;
  ipv4udp.init       = init;
  ipv4udp.comm_send  = sender;
  return( &ipv4udp );
}