#include "srv.h" // This is a server plugin

#define ROT_DZN                3        // Dead-zone
#define ROT_ACC                6        // Acceleration
#define ROT_DMP                6        // Dampening
#define ROT_SEN              0.5        // Sensitivity
#define ROT_MAX             1000        // Max accumulated rotation

#define MOV_ACC                2        // Acceleration
#define MOV_BRK                5        // Breaking

#define CAM_SEN              0.3        // Sensitivity

#define TIMEOUT_EMOTICON     100        // Before emoticon is removed

// Emoticon list
enum emo_e {
  EMO_IDLE,
  EMO_CONNECTED,
  EMO_HAPPY,
  EMO_ANGRY
};

static pluginclient_t  kiwiray;         // Plugin descriptor
static  pluginhost_t  *host;            // RoboCortex descriptor

static          char   serdev[ 256 ];   // Serial device name

static          char   drive_x;         // Strafe
static          char   drive_y;         // Move
static          char   drive_r;         // Turn
static unsigned  int   drive_p;         // Pitch
static          long   integrate_r;     // Rotational(turn) integration

static          void  *h_thread;        // Communications thread handle
static           int   connected;       // Successfully connected

// Emoticons
static unsigned char   emoticon;
static unsigned  int   emoticon_timeout;
static           int   timeout_emoticon = TIMEOUT_EMOTICON;
const unsigned  char   emotidata[ 4 ][ 24 ] = {
  {
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00011000, 0b00000000,
    0b00000000, 0b00011000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000
  }, {
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00111100, 0b00111100, 0b00111100,
    0b00100100, 0b00100100, 0b00100100,
    0b00100100, 0b00100100, 0b00100100,
    0b00111100, 0b00111100, 0b00111100,
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000
  }, {
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b11100111, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b10000001, 0b00000000,
    0b00000000, 0b01000010, 0b00000000,
    0b00000000, 0b00111100, 0b00000000,
    0b00000000, 0b00000000, 0b00000000
  }, {
    0b00000000, 0b00000000, 0b00000000,
    0b01000010, 0b00000000, 0b00000000,
    0b00100100, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00111100, 0b00000000, 0b00000000,
    0b01000010, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000
  }
};

// Thread: manage KiwiRay serial communications
static int commthread() {
  int b_working = 0;
  unsigned char n;
  unsigned char emotilast = 255;
  char p_pkt[ 64 ] = { ( char )0xFF, 0x00, 0x00, 0x00, 0x00, 0x00 };

  // Initial serial startup
  b_working = ( serial_open( serdev ) == 0 );
  if( !b_working ) {
    printf( "KiwiRay [warning]: Unable to open %s, disabling serial\n", serdev );
    return( 1 );
  }
  b_working = !serial_params( "115200,n,8,1" );
  if( !b_working ) {
    printf( "KiwiRay [warning]: Unable to configure %s, disabling serial\n", serdev );
    serial_close();
    return( 1 );
  }
  while( 1 ) {
    // Re-open on errors
    if( !b_working ) {
      printf( "KiwiRay [warning]: Serial port problem, re-opening...\n" );
      host->thread_delay( 5000 );
      serial_close();
      b_working = ( serial_open( serdev ) == 0 );
      if( b_working ) b_working = serial_params( "115200,n,8,1" );
    }
    p_pkt[ 1 ] = 0x00;               // Drive XYZ
    p_pkt[ 2 ] = -drive_x;           // Strafe X
    p_pkt[ 3 ] = -drive_y;           // Move   Y
    p_pkt[ 4 ] =  drive_r;           // Rotate R
    p_pkt[ 5 ] =  drive_p * CAM_SEN; // Look   Pitch
    p_pkt[ 6 ] =  6;                 // Stepsize = 1:2^6
    if( b_working) b_working = ( serial_write( p_pkt, 7 ) == 7 );
    if( emotilast != emoticon ) {
      p_pkt[ 1 ] = 0x33;             // Display
      p_pkt[ 2 ] = 23;               // 8x8x3 bits (-1)
      for( n = 0; n < 24; n++ ) p_pkt[ 3 + n ] = emotidata[ emoticon ][ n ];
      if( b_working) b_working = ( serial_write( p_pkt, 27 ) == 27 );
      emotilast = emoticon;
    }
    host->thread_delay( 20 ); // roughly 50 times second
  }
  return( 0 );
}

// Handles packets received from client plugin
static void process_data( void* p_data, unsigned char size ) {
  int n;
  char data[ 256 ];
  memcpy( data, p_data, size );
  data[ size ] = 0;
  if( data[ 0 ] == '/' ) {
    printf( "KiwiRay [info]: Command: %s\n", data );
    if( strcmp( data, "/MIRROR" ) == 0 ) {
      host->cap_set( 1, CAP_TOGGLE, NULL, NULL );
    } else {
      memmove( data + 17, data, size );
      memcpy( data, "UNKNOWN COMMAND: ", 17 );
      host->client_send( data, size + 17 );
    }
  } else {
    printf( "KiwiRay [info]: Speaking: %s\n", data );
    for( n = 0; n < size - 1; n++ ) {
      if( data[ n ] == ':' ) {
        switch( data[ n + 1 ] ) {
          case ')': emoticon = EMO_HAPPY;
            break;
          case '(': emoticon = EMO_ANGRY;
            break;
        }
        if( emoticon > 1 ) emoticon_timeout = timeout_emoticon;
        data[ n ] = ' ';
        data[ n + 1 ] = ' ';
      }
    }
    host->speak_text( data );
  }
}

// Tick: updates motion and timeouts
static void tick() {
	int temp;

	if( !connected ) return; // Important - host->ctrl/diff not valid!
	
	// Emoticon timeout
	if( emoticon_timeout ) {
	  if( --emoticon_timeout == 0 ) emoticon = EMO_CONNECTED;
	}

  // Handle movement X and Y
  if( host->ctrl->kb & KB_LEFT  ) {
    drive_x = ( drive_x > -( 127 - MOV_ACC ) ? drive_x - MOV_ACC : -127 );
  } else if( drive_x < 0 ) {
    drive_x = ( drive_x < -MOV_BRK ? drive_x + MOV_BRK : 0 );
  }
  if( host->ctrl->kb & KB_RIGHT ) {
    drive_x = ( drive_x <  ( 127 - MOV_ACC ) ? drive_x + MOV_ACC :  127 );
  } else if( drive_x > 0 ) {
    drive_x = ( drive_x >  MOV_BRK ? drive_x - MOV_BRK : 0 );
  }
  if( host->ctrl->kb & KB_UP    ) {
    drive_y = ( drive_y > -( 127 - MOV_ACC ) ? drive_y - MOV_ACC : -127 );
  } else if( drive_y < 0 ) {
    drive_y = ( drive_y < -MOV_BRK ? drive_y + MOV_BRK : 0 );
  }
  if( host->ctrl->kb & KB_DOWN  ) {
    drive_y = ( drive_y <  ( 127 - MOV_ACC ) ? drive_y + MOV_ACC :  127 );
  } else if( drive_y > 0 ) {
    drive_y = ( drive_y >  MOV_BRK ? drive_y - MOV_BRK : 0 );
  }

  // Handle movement R
  integrate_r -= ( drive_r * ROT_SEN );
  integrate_r += host->diff->mx;
  integrate_r = MAX( MIN( integrate_r, ROT_MAX ), -ROT_MAX );
  if( integrate_r > ROT_DZN ) {
    drive_r = ( drive_r <  ( 127 - ROT_ACC ) ? drive_r + ROT_ACC :  127 );
    if( drive_r > integrate_r / ROT_DMP + ROT_DZN ) drive_r = integrate_r / ROT_DMP + ROT_DZN;
  } else if( integrate_r < -ROT_DZN ) {
    drive_r = ( drive_r > -( 127 - ROT_ACC ) ? drive_r - ROT_ACC : -127 );
    if( drive_r < integrate_r / ROT_DMP - ROT_DZN ) drive_r = integrate_r / ROT_DMP - ROT_DZN;
  } else {
    drive_r = 0;
  }

  // Handle camera pitch
  if( ( long )drive_p + host->diff->my > 255 / CAM_SEN ) {
    drive_p = 255 / CAM_SEN;
  } else if( ( long )drive_p + host->diff->my < 0 ) {
    drive_p = 0;
  } else {
    drive_p = drive_p + host->diff->my;
  }
}

// Connection has glitched: stops all motion
static void stop_moving() {
  drive_x = 0;
  drive_y = 0;
  drive_r = 0;
  drive_p = 165 / CAM_SEN;
  integrate_r = 0;
}

// Frees allocated resources
static void closer() {
  if( h_thread ) host->thread_stop( h_thread );
}

// Switches emoticon based on connection status
static void connect_status( int status ) {
  connected = status;
  emoticon = ( connected ? EMO_CONNECTED : EMO_IDLE );
}

// Initializes serial communications
static void init() {
  char temp[ CFG_VALUE_MAX_SIZE ];
  // Configuration
  if( host->cfg_read( temp, "timeout_emoticon" ) ) timeout_emoticon = atoi( temp );
  // Initialise serial
  if( host->cfg_read( serdev, "commport" ) ) {
    h_thread = host->thread_start( commthread );
  } else {
    printf( "KiwiRay [warning]: Configuration - commport missing, disabling serial\n" );
  }
}

// Sets up the plugin descriptor
pluginclient_t *kiwiray_open( pluginhost_t *p_host ) {
  memcpy( &kiwiray.ident, "KIWI", 4 );
  host = p_host;
  kiwiray.init       = init;
  kiwiray.close      = closer;
  kiwiray.still      = stop_moving;
  kiwiray.tick       = tick;
  kiwiray.recv       = process_data;
  kiwiray.connected  = connect_status;
  return( &kiwiray );
}
