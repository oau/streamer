#ifndef _OSWRAP_H_
#define _OSWRAP_H_

// Math
#define MIN( a, b ) ( a < b ? a : b )
#define MAX( a, b ) ( a > b ? a : b )

#ifdef _WIN32

#include <stdint.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

//#define THR_HANDLE HANDLE
//#define THR_ID DWORD

#define NET_SOCK SOCKET
#define NET_ADDR SOCKADDR_IN
#define NET_INVALID_SOCKET INVALID_SOCKET
#define NET_ADDR_ANY INADDR_ANY

//#define THR_DECL DWORD WINAPI
//#define THR_ARGS LPVOID lpParam
//#define THR_RET( n ) return

#else

#include <sys/socket.h>       /*  socket definitions        */
#include <sys/types.h>        /*  socket types              */
#include <arpa/inet.h>        /*  inet (3) funtions         */
#include <unistd.h>           /*  misc. UNIX functions      */
#include <stdint.h>

#define NET_SOCK int
#define NET_ADDR struct sockaddr_in
#define SOCKADDR struct sockaddr
#define NET_INVALID_SOCKET -1
#define NET_ADDR_ANY INADDR_ANY

//#define THR_HANDLE pthread_t
//#define THR_ID int

//#define THR_DECL void
//#define THR_ARGS void*ptr
//#define THR_RET( n ) return

#endif

//typedef THR_DECL( *thr_func )( THR_ARGS );

// Network API
int      net_init     ();
int      net_sock     ( NET_SOCK *h_sock );
void     net_addr_init( NET_ADDR *p_addr, uint32_t addr, uint16_t port );
uint32_t net_addr_get ( NET_ADDR *p_addr );
uint16_t net_port_get ( NET_ADDR *p_addr );
int      net_recv     ( NET_SOCK *h_sock, void* p_buf, int size, NET_ADDR *p_addr );
int      net_send     ( NET_SOCK *h_sock, void* p_buf, int size, NET_ADDR *p_addr );
int      net_bind     ( NET_SOCK *h_sock, NET_ADDR *p_addr );
uint32_t net_dtoa     ( char* dotted_ip );

// Thread API
//int  thr_create( THR_HANDLE *p_receiver_h, THR_ID *p_receiver_id, thr_func p_func );
//void thr_sleep ( int ms );

// Serial API
int serial_open  ( char* device );
int serial_params( char* params );
int serial_read  ( char* p_read, int i_read );
int serial_write ( char* p_write, int i_write );
int serial_close ();

// Camera API
unsigned char * capture_fetch( int dev );
int             capture_init ( char* device, int fps, int *w, int *h );
void            capture_free ();

#endif

