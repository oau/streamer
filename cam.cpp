#ifdef _WIN32
#include <stdio.h>
#include <videoinput.h>

videoInput VI;

static int width;;
static int height;
static int dev; 
static unsigned char * buffer;

extern "C" int cam_init( char *device, int fps, int *w, int *h ) {
  int size;
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
  
  return( 0 );
}

extern "C" unsigned char * cam_fetch() {
  if( VI.isFrameNew( dev ) ) {
    VI.getPixels( dev, buffer, false, true ); // BGR, flipped
  }
  return( buffer );
}

extern "C" void cam_close() {
	VI.stopDevice( dev );	
}

#else
#include <stdio.h>
#include "opencv/cv.h"
#include "opencv/highgui.h"

static int width;;
static int height;
static unsigned char * buffer;

struct CvCapture *capture;
extern "C" int cam_init( char *device, int fps, int *w, int *h ) {
    int size, dev;
    dev = atoi( device );
    // Initialize camera
    capture = cvCaptureFromCAM( dev );
    if( !capture ) {
      fprintf( stderr, "!!! Cannot open initialize webcam!\n" );
      return( -1 );
    }

    IplImage* img = 0;
    img = cvQueryFrame( capture );
    width = ( int )cvGetCaptureProperty( capture, CV_CAP_PROP_FRAME_WIDTH );
    height = ( int )cvGetCaptureProperty( capture, CV_CAP_PROP_FRAME_HEIGHT );
    size = img->imageSize;

    buffer = new unsigned char[ size ];
    
    *w = width;
    *h = height;
    return( 0 );
}

extern "C" unsigned char * cam_fetch() {
    IplImage* img = 0;
    if( !cvGrabFrame( capture ) ) {              // capture a frame 
        printf( "Could not grab a frame\n" );
        return( NULL );
    }
    img = cvRetrieveFrame( capture );           // retrieve the captured frame
    memcpy( buffer, img->imageData, img->imageSize );
    return( buffer );
}

extern "C" void cam_close() {
	// TODO: ?
}

#endif
