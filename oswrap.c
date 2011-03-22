#include <string.h>
#include "include/oswrap.h"

#ifdef _WIN32

static WSADATA wsaData;
HANDLE h_serial;

int serial_open( char* device ) {
	h_serial = CreateFile( device, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0 );
	if( h_serial == INVALID_HANDLE_VALUE ) return( -1 );
	return( 0 );
}

// configures port from string of format
//   baudrate,parity,databits,stopbits
// example
//   9600,n,8,1
int serial_params( char* params ) {
	DCB dcb;
	COMMTIMEOUTS cmt;

	memset( &dcb, 0, sizeof( DCB ) );
	dcb.DCBlength = sizeof( DCB );

	// Configure serial parameters
	if( !BuildCommDCB( params, &dcb ) ) return( -1 );
	if( !SetCommState( h_serial, &dcb ) ) return( -1 );

	// Configure buffers
	if( !SetupComm( h_serial,	1024,	1024) ) return( -1 );

	// Configure timeouts	
	cmt.ReadIntervalTimeout = 1000;
	cmt.ReadTotalTimeoutMultiplier = 1000;
	cmt.ReadTotalTimeoutConstant = 1000;
	cmt.WriteTotalTimeoutConstant = 1000;
	cmt.WriteTotalTimeoutMultiplier = 1000;
	if( !SetCommTimeouts( h_serial, &cmt ) ) return( -1 );

	return( 0 );
}

int serial_read( char* p_read, int i_read ) {
	DWORD i_actual = 0;
	if( !ReadFile( h_serial, p_read, i_read,	&i_actual, NULL ) ) return( -1 );
	return( i_actual );
}

int serial_write( char* p_write, int i_write ) {
	DWORD i_actual = 0;
	if( !WriteFile( h_serial, p_write, i_write, &i_actual, NULL ) ) return( -1 );
	return( i_actual );
}

int serial_close() {
	if( !CloseHandle( h_serial ) ) return( -1 );
	return( 0 );
}

int net_init() {
  if( WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) != 0 ) return( -1 );
  return( 0 );
}

#else

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>


int h_serial;
int fd;

int serial_open( char* device ) {
	int h_serial = open( device, O_RDWR | O_NOCTTY | O_NDELAY );
	if( h_serial < 0 ) return( -1 );
	return( 0 );
}

int serial_params( char* params ) {
	struct termios options;
	char* argv[ 4 ];
	unsigned char argc;
	char* p_parse;
	
	// Parse params
	p_parse = params;
	argc = 1;
	argv[ 0 ] = params;
	while( *p_parse != 0 ) {
		if( *p_parse == ',' ) {
			*p_parse = 0;
			if( argc > 3 ) return( -1 );
			argv[ argc++ ] = ++p_parse;
		} else {
			p_parse++;
		}
	}
	
	// Configure baudrate
	tcgetattr( h_serial, &options );
	switch( atoi( argv[ 0 ] ) ) {
		case 1200:
			cfsetispeed( &options, B1200 );
			cfsetospeed( &options, B1200 );
			break;
		case 2400:
			cfsetispeed( &options, B2400 );
			cfsetospeed( &options, B2400 );
			break;
		case 4800:
			cfsetispeed( &options, B9600 );
			cfsetospeed( &options, B9600 );
			break;
		case 19200:
			cfsetispeed( &options, B19200 );
			cfsetospeed( &options, B19200 );
			break;
		case 38400:
			cfsetispeed( &options, B38400 );
			cfsetospeed( &options, B38400 );
			break;
		case 57600:
			cfsetispeed( &options, B57600 );
			cfsetospeed( &options, B57600 );
			break;
		case 115200:
			cfsetispeed( &options, B115200 );
			cfsetospeed( &options, B115200 );
			break;
		default:
			return( -1 );
	}
	
	// Configure parity
	switch( argv[ 1 ][ 0 ] ) {
		case 'n': case 'N':
			options.c_cflag &= ~PARENB;
			break;
		case 'o': case 'O':
			options.c_cflag |= ~PARENB;
			options.c_cflag |= PARODD;
			break;
		case 'e': case 'E':
			options.c_cflag |= ~PARENB;
			options.c_cflag &= ~PARODD;
			break;
		default:
			return( -1 );
	}

	// Configure data bits
	options.c_cflag &= ~CSIZE;
	switch( argv[ 2 ][ 0 ] ) {
		case '8':
			options.c_cflag |= CS8;
			break;
		case '7':
			options.c_cflag |= CS7;
			break;
		default:
			return( -1 );
	}
	
	// Configure stop bits
	switch( argv[ 2 ][ 0 ] ) {
		case '1':
			options.c_cflag &= ~CSTOPB;
			break;
		case '2':
			options.c_cflag |= CSTOPB;
			break;
		default:
			return( -1 );
	}	
	
	// Configure timeouts
	options.c_cc[VMIN] = 0;
	options.c_cc[VTIME] = 1; // Wait for requested data 1/10 seconds
	options.c_cflag |= CLOCAL | CREAD;

	return( tcsetattr(fd, TCSANOW, &options) != 0 ) ;
}

int serial_read( char* p_read, int i_read ) {
	return( read( h_serial, p_read, i_read ) );
}

int serial_write( char* p_write, int i_write ) {
	return( write( h_serial, p_write, i_write ) );
}

int serial_close() {
	return( close( fd ) );
}

int net_init() {
}

#endif

int net_sock( NET_SOCK *h_sock ) {
  *h_sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
  if( *h_sock < 0 ) return( -1 );
  return( 0 );
}

void net_addr_init( NET_ADDR *p_addr, uint32_t addr, uint16_t port ) {
  memset( p_addr, 0, sizeof( NET_ADDR ) );
  p_addr->sin_family = AF_INET;
  p_addr->sin_addr.s_addr = htonl( addr );
  p_addr->sin_port = htons( port );
}

uint32_t net_addr_get( NET_ADDR *p_addr ) {
  return( ntohl( p_addr->sin_addr.s_addr ) );
}

uint16_t net_port_get( NET_ADDR *p_addr ) {
  return( ntohs( p_addr->sin_port ) );
}

// Return number of bytes received or < 0 on error
int net_recv( NET_SOCK *h_sock, void* p_buf, int size, NET_ADDR *p_addr ) {
  int temp = sizeof( NET_ADDR );
  return( recvfrom( *h_sock, p_buf, size, 0, ( SOCKADDR* )p_addr, &temp ) );
}

// Return number of bytes sent or < 0 on error
int net_send( NET_SOCK *h_sock, void* p_buf, int size, NET_ADDR *p_addr ) {
  return( sendto( *h_sock, p_buf, size, 0, ( SOCKADDR* )p_addr, sizeof( NET_ADDR ) ) );
}

// Return 0 on success else < 0
int net_bind( NET_SOCK *h_sock, NET_ADDR *p_addr ) {
  if( bind( *h_sock, ( SOCKADDR* )p_addr, sizeof( NET_ADDR ) ) < 0 ) {
    return( -1 );
  } else {
    return( 0 );
  }
}

// Return address from dotted-ip string
uint32_t net_dtoa( char* dotted_ip ) {
  return( ntohl( inet_addr( dotted_ip ) ) );
}

#ifdef _WIN32

/*
// Return 0 on success else < 0
int thr_create( THR_HANDLE *p_receiver_h, THR_ID *p_receiver_id, thr_func p_func ) {
  *p_receiver_h = CreateThread( NULL, 0, p_func, 0, 0, p_receiver_id );
  return( 0 );
}

void thr_sleep( int ms ) {
  Sleep( ms );
}
*/

#else // UNIX

/*
int thr_create( THR_HANDLE *p_receiver_h, THR_ID *p_receiver_id, thr_func p_func ) {
    *p_receiver_id = pthread_create(p_receiver_h, NULL, p_func, NULL);
}

void thr_sleep( int ms ) {

    struct timespec sleepTime;
    struct timespec r;

    sleepTime.tv_sec=0;
    sleepTime.tv_nsec=1000000*ms;
    nanosleep(&sleepTime,&r);
}
*/

#endif
