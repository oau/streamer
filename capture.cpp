#define MAX_DEVICES 10

#ifdef _WIN32
#include <stdio.h>
#include <videoinput.h>

videoInput VI;

static int width;;
static int height;
static int devs[ MAX_DEVICES ];
static int devs_count = 0;
static unsigned char * buffer;

extern "C" int capture_init( char *device, int fps, int *w, int *h ) {
  int size, dev;
  dev = -1;
  if( device ) {
  	if( strlen( device ) ) {
  		dev = atoi( device );
  	}
  }
  //Prints out a list of available devices and returns num of devices found
  if( dev >= 0 ) {
    if( !VI.setupDevice( dev, *w, *h ) ) return( -1 );
  } else {
    int numDevices = VI.listDevices();    
    return( -1 );
  }
  
  VI.setIdealFramerate( dev, fps );	
  
  // Automatically reconnect on freeze, may fix bugs with some devices/drivers
  VI.setAutoReconnectOnFreeze( dev, true, 25 );
  
  width   = VI.getWidth ( dev );
  height  = VI.getHeight( dev );
  size    = VI.getSize  ( dev );
  *w = width;
  *h = height;
  
  buffer = new unsigned char[ size ];
  
  devs[ devs_count ] = dev;
  
  return( devs_count++ );
}

extern "C" unsigned char * capture_fetch( int dev ) {
  if( VI.isFrameNew( dev ) ) {
    VI.getPixels( devs[ dev ], buffer, false, true ); // BGR, flipped
  }
  return( buffer );
}

extern "C" void capture_close() {
	unsigned int n;
	for( n = 0; n < devs_count; n++ ) {
  	VI.stopDevice( devs[ n ] );	
  }
  devs_count = 0;
}

#else
#include <stdio.h>
#include "opencv/cv.h"
#include "opencv/highgui.h"

static int width;;
static int height;
//static unsigned char * buffer;
typedef struct {
  unsigned char *buffer;
  struct CvCapture *capture;
} capture_t;

static capture_t devs[ MAX_DEVICES ];
static int devs_count = 0;

extern "C" int capture_init( char *device, int fps, int *w, int *h ) {
    int size, dev;
    dev = atoi( device );
    // Initialize camera
    devs[ devs_count ].capture = cvCaptureFromCAM( dev );
    if( !capture ) return( -1 );

    IplImage* img = 0;
    img = cvQueryFrame( devs[ devs_count ].capture );
    width = ( int )cvGetCaptureProperty( devs[ devs_count ].capture, CV_CAP_PROP_FRAME_WIDTH );
    height = ( int )cvGetCaptureProperty( devs[ devs_count ].capture, CV_CAP_PROP_FRAME_HEIGHT );
    size = img->imageSize;

    devs[ devs_count ].buffer = new unsigned char[ size ];
    
    *w = width;
    *h = height;
    return( devs_count++ );
}

extern "C" unsigned char * capture_fetch( int dev ) {
    IplImage* img = 0;
    if( !cvGrabFrame( devs[ dev ].capture ) ) {              // capture a frame 
        printf( "Could not grab a frame\n" );
        return( NULL );
    }
    img = cvRetrieveFrame( devs[ dev ].capture );           // retrieve the captured frame
    memcpy( devs[ dev ].buffer, img->imageData, img->imageSize );
    return( devs[ dev ].buffer );
}

extern "C" void capture_close() {
	// TODO: ?
	devs_count = 0;
}

#endif
