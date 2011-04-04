#include <stdio.h>
#define MAX_DEVICES 10

#ifdef _WIN32
#include <videoinput.h>

typedef struct {
  unsigned char dev;
  videoInput VI;
} capture_t;


static int width;;
static int height;
static capture_t *devs[ MAX_DEVICES ];
static int devs_count = 0;
static unsigned char * buffer;

extern "C" int capture_init( char *device, int fps, int *w, int *h ) {
  int size, dev;
  capture_t *p_dev = new capture_t();
  dev = -1;
  if( device ) {
  	if( strlen( device ) ) {
  		dev = atoi( device );
  	}
  }
  
  //Prints out a list of available devices and returns num of devices found
  if( dev >= 0 ) {
    if( !p_dev->VI.setupDevice( dev, *w, *h ) ) return( -1 );
  } else {
    int numDevices = p_dev->VI.listDevices();    
    return( -1 );
  }
  
  p_dev->VI.setIdealFramerate( dev, fps );	
  
  // Automatically reconnect on freeze, may fix bugs with some devices/drivers
  p_dev->VI.setAutoReconnectOnFreeze( dev, true, 25 );
  
  width   = p_dev->VI.getWidth ( dev );
  height  = p_dev->VI.getHeight( dev );
  size    = p_dev->VI.getSize  ( dev );
  *w = width;
  *h = height;
  
  buffer = new unsigned char[ size ];
  
  p_dev->dev = dev;
  devs[ devs_count ] = p_dev;
  
  return( devs_count++ );
}

extern "C" unsigned char * capture_fetch( int dev ) {
  if( devs[ dev ]->VI.isFrameNew( dev ) ) {
    devs[ dev ]->VI.getPixels( devs[ dev ]->dev, buffer, false, true ); // BGR, flipped
  }
  return( buffer );
}

extern "C" void capture_close() {
	unsigned int dev;
	for( dev = 0; dev < devs_count; dev++ ) {
  	devs[ dev ]->VI.stopDevice( devs[ dev ]->dev );	
  	devs[ dev ] = NULL;
  }
  devs_count = 0;
}

#else
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
