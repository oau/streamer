#include <stdio.h>
#include "robocortex.h"
#include "oswrap.h"
#include <SDL/SDL_video.h>

#define CAP_DISABLE 0
#define CAP_ENABLE  1
#define CAP_TOGGLE  2
#define CAP_NOP     3

typedef struct {
  // Requests a parameter from the configuration file
  int      ( *cfg_read     )( char *value, char *token );
  // Start a new thread
  void    *( *thread_start )( int( *fp_thread )() );
  // Forcibly kill the specified thread
  void     ( *thread_stop  )( void *h_thread );
  // Block (delay) thread for the specified number of milliseconds
  void     ( *thread_delay )( int ms );
  // Use TTS to play back the specified text
  void     ( *speak_text   )( char *text );
  // Send a data packet to the client
  void     ( *client_send  )( void* data, unsigned char size );
  // Get capture device parameters
  void     ( *cap_get      )( int device, int *w, int *h, int *enabled, SDL_Rect *src, SDL_Rect *dst );
  // Set capture device parameters
  void     ( *cap_set      )( int device, int enabled, SDL_Rect *src, SDL_Rect *dst );
  // Process a received packet
  void     ( *comm_recv    )( char* data, int size, remote_t *addr );
  // Valid in tick(), when client is connected only
  // Contains control/steering information
  ctrl_t  *ctrl; // Current values
  ctrl_t  *diff; // Difference since last call
  // Valid in tick() only
  // Contains final YUV420P image information for analysis/modification
  int      stream_stride[ 3 ];
  uint8_t *stream_plane[ 3 ];
  int      stream_w;
  int      stream_h;
  int      cap_count;
} pluginhost_t;

typedef struct {
  // Plugin identifier - must be unique, but match on server/client side
  uint32_t ident;
  // Called once, after plugin_open, to initialize the plugin
  void ( *init       )();
  // Called when shutting down, release all resources
  void ( *close      )();
  // Called when connection is lost or glitches, stop moving or begin return to starting position
  void ( *still      )();
  // Called when the connection status changes
  void ( *connected  )( int connected );
  // Called when an image is captured from any of the capture devices
  // Captured BGR24 image may be analysed and/or modified here
  void ( *capture    )( int device, int w, int h, uint8_t *data );
  // Called each frame
  // Control/steering information should be processed here
  // Final YUV420P image may be analysed and/or modified here
  void ( *tick       )();
  // Called when a data packet is received from the client
  void ( *recv       )( void *data, unsigned char size );
  // Called when a video packet has been encoded for transmission to client
  void ( *stream     )( char *packet, int size );
  // Called when a packet needs to be sent to remote end
  void ( *comm_send  )( char* data, int size, remote_t *addr );
} pluginclient_t;