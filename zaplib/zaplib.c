/*
Copyright (c) 2004-2009, Ruckus Wireless, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Ruckus Wireless nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER OR CONTRIBUTERS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// zap.c : 
//
// This program is designed to be a robust network performance test tool.
//
// It works on a controller/server model. The controller sends commands to the servers instructing them
// to send data to each other. The results of these tests are sent back to the controller to be used there.
//
// The basic test setup is as follows:
//
// 1.  Server X is started, listens on a well known TCP port.
// 2.  Server Y is started.
// 3.  Controller A is started, and configured by user to run a test. Controller chooses a global test ID ( TID ) for this test.
// 4.  Controller A opens a control TCP connection to Server X for test TID.
// 5.  Controller A opens a control TCP connection to Server Y for test TID.
// 6.  Controller A configures Server X to open a TCP connection to Server Y for test TID.
// 7.  Controller A configures Server X to send a UDP datagram to Server Y for test TID. 
//     ( establishes NAT, in case data must flow the other way )
// *   Controller A configures Server <RX> to be ready for data reception.
// 8.  Controller A configures Server <TX> to send data to server <RX>
//     This data may be TCP/UDP/Batched in a variety of forms.
// 9.  Server Y periodically ( configurable ) sends performance updates to Controller A.
//    ... test runs ...
// 10. Controller A may, at any time, abort the test.
//    ... test runs ...
// 11. The controller finishes the test when it feels like it. 
//     The Servers transmit/receive as much data as they were told to.
//
// Random notes:
//
// * IP addresses in unsigned __int32 form are ALWAYS network byte order.
//

#ifdef WIN32
#include <winsock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <io.h>
#include <tchar.h>
#else
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <errno.h>
#ifdef LINUX
#include <asm/errno.h>
#include <asm/ioctls.h>
#endif
#endif

#include <fcntl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include "zaplib.h"
#include "error.h"

#define ZAP_TYPICAL_TIMEOUT_USEC   5*1000*1000

#ifndef NO_MTUDISC
int pmtudisc = -1;  // Path MTU discovery: System default (-1), DONT (0), WANT (1) or DO (2)
#endif // NO_MTUDISC

static char zap_frame_str[11][30] = 
{
		"Data",					// 0
		"Data_Complete",		// 1
		"Data_Comp_Res",		// 2
		"Test_Complete",		// 3
		"Open_Data_Conn",		// 4
		"Open_Control_Conn",	// 5
		"Connect",				// 6
		"Ready",				// 7
		"Test_Start",			// 8
		"Performance_Result",	// 9
		"Null",					// 10
};

char        currPath[_MAX_PATH];


#ifndef WIN32
void closesocket( SOCKET s )
{
	close( s );
}
#endif


#ifdef WIN32
/* list of signal handlers. _NSIG is number of signals defined. */

static SigfuncPtr handlers[ _NSIG ] = { 0};


/* -------------------------------------------------------------------
* Catch_Ctrl_C
*
* dispatches the signal to appropriate signal handler. 
*
* ------------------------------------------------------------------- */

BOOL WINAPI Catch_Ctrl_C( DWORD ctrl_type )
{
	SigfuncPtr	h = NULL;
	int			signo;

	switch ( ctrl_type ) {
		case CTRL_C_EVENT:
			signo = SIGINT;
			h = handlers[ SIGINT ];
			break;

		case CTRL_CLOSE_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			signo = SIGTERM;
			h = handlers[ SIGTERM ];
			break;

		default:
			break;
	}

	if ( h != NULL ) {
		// call the signal handler
		h( signo );
		return TRUE;
	} else {
		return FALSE;
	}
}

/* -------------------------------------------------------------------
* signal
*
* installs a  signal handler. 
* ------------------------------------------------------------------- */
SigfuncPtr signal( int inSigno, SigfuncPtr inFunc )
{
	SigfuncPtr old = NULL;

	if ( inSigno >= 0  &&  inSigno < _NSIG ) {
		old = handlers[ inSigno ];
		handlers[ inSigno ] = inFunc;
	}

	return old;
} /* end signal */

#endif


void net_init( void )
{
	int				err;
#ifdef WIN32
	WORD			wVersionRequested;
	WSADATA			wsaData;
    HANDLE			proc;

	wVersionRequested = MAKEWORD( 3, 3 );

	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 ) {
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */
		fprintf( stderr, "ERROR: Could not initialize network\n" );
		exit( 1 );
	}

    proc = GetCurrentProcess();
    SetPriorityClass(proc, REALTIME_PRIORITY_CLASS);

    SetConsoleCtrlHandler( ( PHANDLER_ROUTINE )Catch_Ctrl_C, TRUE );
#else
    err = nice( -20 ); // should use SCHED_FIFO with sched_priority >0
#endif 
}


int get_data( SOCKET sock, unsigned char *data, unsigned int len, unsigned __int32 *remote_addr)
{
    int				total = 0;
    int				rlen;
    int				addr_len;
    struct			sockaddr_in  addr;
	int				ttsk = 0;
	fd_set			read_fds;
	int				n_fd = 0;
	struct timeval	tv;
	
    while ( total < ( int )len ) {
		FD_ZERO( &read_fds );
		FD_SET( sock, &read_fds );
		N_UPDATE( n_fd, sock );

		tv.tv_sec = 10;
		tv.tv_usec = 0;
		
		ttsk = select( n_fd+1, &read_fds, NULL, NULL, &tv );
		if ( ttsk == 0 ) {	
			WARN_errno( 1, "get_data: Time out" );
			return 1;
		}
		else if ( ttsk == SOCKET_ERROR ) {
			return 1;
		}
        addr_len = sizeof( addr );
		rlen = recv( sock, ( char * )&data[total], len - total, 0);
		if ( rlen <= 0 ) {		
            break;
		}
        total += rlen;
    }

	return total;
}


// takes network byte order and prints it.
char *inet_ntoa2( unsigned __int32 addr )
{
	static char			str[50][50];
	static int			which = 0;
	struct				in_addr	in;
	int					ret = which;

	in.s_addr=addr;

	sprintf( str[which], "%s", inet_ntoa( in ) );
	which++;
	if ( which == 50 ) {
		which = 0;
	}

	return str[ret];
}
	

void net_cleanup( void )
{
#ifdef WIN32
	WSACleanup(  );
#endif // WIN32
}



void cleanup_exit( int err )
{
	net_cleanup(  );
	exit( err );
}


unsigned __int64 get_current_second(void)
{
	time_t seconds;

	seconds = time (NULL);

	return seconds;
	
}

// This routing gets the current time. 
// Used for differential calculations only.
__int64 get_current_usecs( void )
{
#ifdef WIN32
	static LARGE_INTEGER	time_divisor;
	static int				initialized = 0;
	__int64					ticks;

	if ( !initialized ) {
		QueryPerformanceFrequency( ( LARGE_INTEGER * ) &time_divisor );
		initialized = 1;
	}

	QueryPerformanceCounter( ( LARGE_INTEGER * ) &ticks );
	ticks *= 1000000LL;
	ticks /= time_divisor.QuadPart;


#else // !WIN32

	__int64			ticks;
	struct timeval	tv;
	struct timezone tz;

	gettimeofday( &tv, &tz);

	ticks =  (__int64)tv.tv_usec;
	ticks +=  (__int64)tv.tv_sec * 1000000;
#endif // !WIN32
	return ticks;
}


/*
 * Relinquish processor so other tasks can run.
 */
void zap_relinquish( void )
{
#ifdef WIN32
    Sleep( 1 );
#else
    sched_yield(  );
#endif
}


void zap_clean_station( zap_station_t *station, fd_set *pfd)
{
	int			i;

	if ( station->state != zap_station_state_off ) {
	}
	for ( i = 0; i < ZAP_MAX_RECEIVERS; i++ ) {
		if ( station->s_tcp[i] != INVALID_SOCKET ) {
			if ( FD_ISSET( station->s_tcp[i], pfd ) ){
				FD_CLR( station->s_tcp[i], pfd );
			}
			shutdown( station->s_tcp[i], SHUT_WR );
#ifdef WIN32
			Sleep( SLEEP_TIME );
#else
			usleep( SLEEP_TIME );
#endif
			closesocket( station->s_tcp[i] );
			station->s_tcp[i] = INVALID_SOCKET;
		}
	}

	if ( station->s_control != INVALID_SOCKET ) {
		if ( FD_ISSET( station->s_control, pfd ) ){
			FD_CLR( station->s_control, pfd );
		}
		shutdown( station->s_control, SHUT_WR );
#ifdef WIN32
		Sleep( SLEEP_TIME );
#else
		usleep( SLEEP_TIME );
#endif
		closesocket( station->s_control );
		station->s_control = INVALID_SOCKET;
	}

	station->s_tcp_count = 0;

	station->state = zap_station_state_off;

	memset( &station->sample, 0, sizeof( station->sample ) );

	station->id = 0;
}

int zap_find_station( unsigned __int32 tid, zap_server_t *server, zap_station_t **station, unsigned __int32 add)
{
	unsigned __int32		i;
	unsigned __int32		useme = 0xffffffff;

	for ( i = 0; i < ZAP_MAX_STATIONS; i++ ) {
		if ( server->stations[i].id == tid ) {
			*station = &( server->stations[i] );
			return 0;
		}
		if ( ( useme > ZAP_MAX_STATIONS ) &&
			( server->stations[i].state == zap_station_state_off ) ) {
			useme = i;
		}
	}

	if ( add ) {
		if ( useme < ZAP_MAX_STATIONS ) {
			*station = &( server->stations[useme] );
			( *station )->id = tid;
			( *station )->state = zap_station_state_init;

			return 0;
		}
	}
	return 1;
}

int zap_read_frame( SOCKET s, unsigned __int32 tcp, zap_frame_t **rx_frame, unsigned __int32 *remote_ip, struct timeval *tv)
{
	static unsigned char	frame_space[65536];
	zap_frame_t				*frame;
	unsigned __int32		remainder;
	int						len;
    struct sockaddr_in		addr;
	int						addr_len = sizeof( addr );

	frame = ( zap_frame_t * ) &frame_space[0];
	if ( tcp ) {

		if ( get_data( s, &frame_space[0], sizeof( zap_header_t ), remote_ip ) != sizeof( zap_header_t ) ) {
			return 1;
		}
		if ( ( ntohl( frame->header.length ) + sizeof( zap_header_t ) ) > 65536 ) {
			errOut( "Unexpectedly large frame size %d\n", frame->header.length );
			return 1;
		}

		remainder = ntohl( frame->header.length ) - sizeof( zap_header_t );
		if ( remainder ) {
			if ( get_data( s, &frame_space[sizeof( zap_header_t )], remainder, remote_ip ) != remainder ) {
				return 1;
			}
		}
	} else {
#ifdef WIN32
        len = recvfrom( s, ( char * )&frame_space[0], sizeof( frame_space ), 0, ( struct sockaddr * )&addr, &addr_len );
#else
        struct msghdr   msg;
        struct iovec    iov;
        char            ctrl[CMSG_SPACE(sizeof(struct timeval))];
        struct cmsghdr  *cmsg = (struct cmsghdr *) &ctrl;

        addr.sin_addr.s_addr    = INADDR_ANY;
        addr.sin_port           = htons(ZAP_SERVICE_PORT);
        addr.sin_family         = AF_INET;

        msg.msg_control      = (char *) ctrl;
        msg.msg_controllen   = sizeof(ctrl);

        msg.msg_name         = &addr;
        msg.msg_namelen      = sizeof(addr);
        msg.msg_iov          = &iov;
        msg.msg_iovlen       = 1;
        iov.iov_base         = frame_space;
        iov.iov_len          = sizeof(frame_space);

        len = recvmsg(s, &msg, 0);
        if (cmsg->cmsg_level == SOL_SOCKET &&
            cmsg->cmsg_type  == SCM_TIMESTAMP &&
            cmsg->cmsg_len   == CMSG_LEN(sizeof(struct timeval))) {
            if (tv)
                memcpy(tv, CMSG_DATA(cmsg), sizeof(struct timeval));
        }
#endif
		if ( len < 0 ) {
#ifdef WIN32
			errOut( "Received socket error on read.\n");
#endif
			return 1;
		} else {
			if ( len < sizeof( zap_header_t ) ) {
				erk; 
				return 1; 
			}
			if ( len != ntohl( frame->header.length ) ) { 
				erk; 
				return 1; 
			}
		}
		if ( remote_ip ) {
			*remote_ip = addr.sin_addr.s_addr;
		}
	}

	if ( ( ntohl( frame->header.zap_minor_vers ) != ZAP_MINOR_VERSION ) ||
		( ntohl( frame->header.zap_major_vers ) != ZAP_MAJOR_VERSION ) ) {
		errOut( "Zap version incompatibility, Version %d.%d vs %d.%d\n", 
			ZAP_MAJOR_VERSION, 
			ZAP_MINOR_VERSION, 
			ntohl( frame->header.zap_major_vers ),
			ntohl( frame->header.zap_minor_vers ) );
		return 1;
	}

	*rx_frame = frame;
	return 0;
}


int
zap_socket( unsigned __int32 buff_size, int tcp, SOCKET *sock)
{
    int                 err;
    SOCKET              s;
	int					value;

	if ( tcp ) {
		s = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
		if ( s == INVALID_SOCKET ) {
			return 1;
		}
		value = 1;
		err = setsockopt( s, IPPROTO_TCP, TCP_NODELAY, ( const char * )&value, sizeof( value ) );
		if ( err ) { 
			erk;
			return 1; 
		}
	} else {
		s = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
		if ( s == INVALID_SOCKET ) {
			return 1;
		}
	}

#ifndef NO_MTUDISC
	if ( pmtudisc != -1 ) {
        /* Configure PMTU discovery */
		value = pmtudisc;
		err = setsockopt( s, IPPROTO_IP, IP_MTU_DISCOVER, ( const char * )&value, sizeof( value ) );
		if ( err ) { 
			erk;
			return 1; 
		}
	}
#endif // NO_MTUDISC 

	if ( buff_size ) {
        /* Configure buffering */
		err = setsockopt( s, SOL_SOCKET, SO_SNDBUF, ( const char * )&buff_size, sizeof( buff_size ) );
		if ( err ) {
			return 1;
		}
		err = setsockopt( s, SOL_SOCKET, SO_RCVBUF, ( const char * )&buff_size, sizeof( buff_size ) );
		if ( err ) {
			return 1;
		}
	}

	*sock = s;

	return 0;
}


int zap_bind( SOCKET sock)
{
    struct sockaddr_in  addr;
	int                 rv;

    addr.sin_addr.s_addr	= INADDR_ANY;
    addr.sin_family			= AF_INET;
    addr.sin_port			= htons( ZAP_SERVICE_PORT );

#ifdef WIN32
	if ( rv = bind( sock, ( struct sockaddr * )&addr, sizeof( addr ) ) ) {
		WARN_errno( rv == SOCKET_ERROR, "zap_bind" );
		return 1;
	}
#else
	do {
		bind( sock, ( struct sockaddr * )&addr, sizeof( addr ) );
		usleep(SLEEP_TIME);
	}while(errno == EADDRINUSE);

#endif
	return 0;
}


int zap_listen( SOCKET sock)
{
	int       rv;

	if ( zap_bind( sock ) ) {
		return 1;
	}
	if ( rv= listen( sock, 5 ) ) {
		WARN_errno( rv == SOCKET_ERROR, "zap_listen" );
		return 1;
	}

	return 0;
}

static void zap_set_tos( SOCKET sock, unsigned __int32 *tos)
{
	int			size_tos, err ;

	size_tos = sizeof( tos );

    err = setsockopt( sock, IPPROTO_IP, IP_TOS, ( const char * )tos, size_tos );
	if ( err == SOCKET_ERROR ) {
#ifdef WIN32
			fprintf( stdout, "err: Could not set IP_TOS socket option. Administrator privileges required? err = %d\n", WSAGetLastError());
#else
			fprintf( stdout, "err: Could not set IP_TOS socket option. Administrator privileges required? err = %d\n", errno );
#endif
	}
}

int zap_accept( zap_server_t *server, SOCKET sock, zap_station_t *station_cleaned)
{
    struct sockaddr_in  addr;
	int					addrlen = sizeof( addr );
	SOCKET				new_sock;
	int					sockbuf_size;
	int					new_size;
	zap_frame_t			*frame;
	zap_station_t		*station;
	unsigned __int32	*src, *dst;
	unsigned __int32	i;
	unsigned __int32	max_payload_outstanding = 0;
	unsigned __int32	max_batch_outstanding = 0;

	new_sock = accept( sock, ( struct sockaddr * )&addr, &addrlen );
	if ( ( new_sock == INVALID_SOCKET )  || !new_sock ) {
		errOut( "Received unexpected INVALID_SOCKET accepting from %s.\n", inet_ntoa2( addr.sin_addr.s_addr ) );
		erk; 
		return 1; 
	}
	// Set no_delay TCP option.
	i = 1;
	if ( setsockopt( new_sock, IPPROTO_TCP, TCP_NODELAY, ( const char * )&i, sizeof( i ) ) ) {
		closesocket( new_sock );
		return 1;
	}

	// Read a frame...
	if ( zap_read_frame( new_sock, 1, &frame, NULL, NULL ) ) {
		closesocket( new_sock );
		return 1;
	}
	if ( zap_find_station( ntohl( frame->header.zap_test_id ), server, &station, 1 ) ) {
		closesocket( new_sock );
		return 1;
	}

	switch ( ntohl( frame->header.zap_frame_type ) ) {
		case zap_type_open_control_conn:
			if ( station->s_control != INVALID_SOCKET ) {
				erk;
				closesocket( new_sock );
				return 1;
			}

			station_cleaned = station;
			// Copy in configuration, with byte-order fixing.
			src = ( unsigned __int32 * ) &( frame->payload.open_control.config );
			dst = ( unsigned __int32 * ) &( station->config );
			for ( i = 0; i < ( sizeof( station->config ) / 4 ); i++ ) {
				*dst = ntohl( *src );
				dst++;
				src++;
			}

			station->s_control = new_sock;
			station->state = zap_station_state_rx_config;
			if ( !station->config.tx ) {
				station->state = zap_station_state_running_rx;
			}

			station->blocked = 0;
			station->next_event = 0;
			station->batch_num = 0;
			station->sample_num = 0;
			station->last_completed_batch = 0xffffffff;
			station->batch_start_usec = 0;
			station->payload_usec = 0;
			station->payload_num = 0;

			max_payload_outstanding = station->config.batch_size * station->config.asynchronous;
			max_batch_outstanding = station->config.asynchronous;

			memset( &station->sample, 0, sizeof( station->sample ) );

			// Resize UDP socket to max of all active stations.
			sockbuf_size = 64*1024;
			for ( i = 0; i < ZAP_MAX_STATIONS; i++ ) {
				if ( server->stations[i].state != zap_station_state_off ) {
					if ( !server->stations[i].config.tcp ) {
						new_size = server->stations[i].config.batch_size * server->stations[i].config.payload_length;
						if ( new_size > sockbuf_size ) {
							sockbuf_size = new_size;
						}
					}
				}
			}
			if ( setsockopt( server->udp_socket_tx, SOL_SOCKET, SO_SNDBUF, ( const char * )&sockbuf_size, sizeof( sockbuf_size ) ) ) { 
				erk;
			}
			if ( setsockopt( server->udp_socket_rx, SOL_SOCKET, SO_RCVBUF, ( const char * )&sockbuf_size, sizeof( sockbuf_size ) ) ) {
				erk;
			}
			// Set Tos Bit.  FIXME:  Because different stations use the same TX socket, we need
			// to resolve conflicts if the stations specifies different TOS values.  There is no
			// easy way to do this.
			zap_set_tos( server->udp_socket_tx, &station->config.ip_tos );
			if ( zap_send_ready( station->id, new_sock ) ) {
				erk;
				//zap_clean_station( station );
				return 1;
			}
			break;
		case zap_type_open_data_conn:
			// A new data connection! Whee!!!
			station_cleaned = station;
			if ( station->s_tcp_count < ZAP_MAX_RECEIVERS ) {
				if ( station->s_tcp[station->s_tcp_count] == INVALID_SOCKET ) {
					station->s_tcp[station->s_tcp_count] = new_sock;
					station->s_tcp_count++;
					zap_set_tos( new_sock, &station->config.ip_tos );
					if ( zap_send_ready( station->id, new_sock ) ) {
						erk;
						//zap_clean_station( station );
						return 1;
					}

					return 0;
				}
			}
			erk;
			//printf( "zap_accept - zap_clean_station ", __LINE__, __FILE__ );
			//zap_clean_station( station );
			return 1;

			break;
		default:
			erk;
			//printf( "zap_accept - zap_clean_station ", __LINE__, __FILE__ );
			//zap_clean_station( station );
			return 1;
	}

	return 0;
}


int zap_connect( unsigned __int32 remote_ip, int tcp, SOCKET sock, unsigned __int32 usec_timeout)
{
    struct sockaddr_in  addr;
	struct				timeval tv;
	fd_set				write_fds;
	int					n_fd = 0;
	int					err, rv;
	int					so_error;
	int					so_error_len;
	int					my_err;
	char				*my_str;
	int					ttsk =0;
	unsigned long		non_blocking;

	if ( !tcp ) {
		return 0;
	}

    addr.sin_addr.s_addr= remote_ip;
    addr.sin_family		= AF_INET;
    addr.sin_port		= htons( ZAP_SERVICE_PORT );

	non_blocking = 1;
#ifdef WIN32
	if ( ioctlsocket( sock, FIONBIO, &non_blocking ) ) {
		return 1;
	}
#else
	if ( ioctl( sock, FIONBIO, &non_blocking ) ) {
		return 1;
	}
#endif
	err = connect( sock, ( struct sockaddr * ) &addr, sizeof( addr ) );

#ifdef WIN32
	if ( err != 0 ){
		my_err = WSAGetLastError(  );
		my_str = ( char * )winsock_strerror( my_err );
		fflush( 0 );

		if ( err != SOCKET_ERROR ) {
			fprintf( stdout, "\n%s: Error: %s ( %d ), line %d\n", __FUNCTION__, my_str, my_err, __LINE__ ); 
			return 1; 
		}
		if ( my_err != WSAEWOULDBLOCK ) {
			fprintf( stdout, "\n%s: Error: %s ( %d ), line %d\n",  __FUNCTION__, my_str, my_err, __LINE__ ); 
			return 1;
		}
	}
#else
	if ( err != 0 ){
		my_err = ( int ) errno;
		my_str = ( char* ) strerror( my_err );
		fflush( 0 );

		if ( err != -1 ) {
			fprintf( stdout, "\n%s: Error: %s ( %d ), line %d\n", __FUNCTION__, my_str, my_err, __LINE__ ); 
			return 1; 
		}
		if ( errno != EINPROGRESS ) { 
			fprintf( stdout, "\n%s: Error: %s ( %d ), line %d\n", __FUNCTION__, my_str, my_err, __LINE__ ); 
			return 1; 
		}
	}
#endif

	FD_ZERO( &write_fds );
	FD_SET( sock, &write_fds );
	N_UPDATE( n_fd, sock );
	tv.tv_sec = usec_timeout / 1000000;
	tv.tv_usec = usec_timeout % 1000000;

	ttsk = select( n_fd + 1, NULL, &write_fds, NULL, &tv );
	if ( ttsk != 1 ) {
		// Not writable, must not have opened correctly. Or soon enough.
		//erk;
		return 1;
	}

	so_error_len = sizeof( so_error );
	if ( rv = getsockopt( sock, SOL_SOCKET, SO_ERROR, ( char * )&so_error, &so_error_len ) ) { 
		WARN_errno( rv == SOCKET_ERROR, "zap_connect - getsockopt" );
		return 1; 
	}

	if ( so_error ) { 
		//erk; 
		return 1; 
	}/* Connect failed. No service or some such... */

	non_blocking = 0;
#ifdef WIN32
	if ( ioctlsocket( sock, FIONBIO, &non_blocking ) ) {
		return 1;
	}
#else
	if ( ioctl( sock, FIONBIO, &non_blocking ) ) {
		return 1;
	}
#endif

	return 0;
}


// Wait for ready response, but not too long.
int zap_get_ready( SOCKET s, unsigned __int32 tcp, unsigned __int32 usecs)
{
	zap_frame_t			*rx_frame;

	// Read response.
	if ( zap_read_frame( s, 1, &rx_frame, NULL, NULL ) ) {
		return 1;
	}

	if ( ntohl( rx_frame->header.zap_frame_type ) != zap_type_ready ) {
		return 1;
	}
	return 0;
}


int zap_config( unsigned __int32 tid, SOCKET s, zap_station_config_t *conf)
{
	zap_station_config_t	net_conf;
	unsigned __int32		*walk_src, *walk_dst;
	zap_frame_t				frame;
	int						frame_length;
	int						i, rv;

	i = sizeof( net_conf );

	// Move to network byte order...
	walk_src = ( unsigned __int32 * ) conf;
	walk_dst = ( unsigned __int32 * ) &net_conf;
	for ( i = 0; i < ( sizeof( net_conf ) / 4 ); i++ ) {
		*walk_dst = htonl( *walk_src );
		walk_src++;
		walk_dst++;
	}

	frame_length = sizeof( zap_header_t ) + sizeof( zap_open_control_frame_t );
	frame.header.length = htonl( frame_length );
	frame.header.zap_frame_type = htonl( zap_type_open_control_conn );
	frame.header.zap_major_vers = htonl( ZAP_MAJOR_VERSION );
	frame.header.zap_minor_vers = htonl( ZAP_MINOR_VERSION );
	frame.header.zap_test_id = htonl( tid );
	memcpy( &frame.payload.open_control.config, &net_conf, sizeof( net_conf ) );
	
	if ( ( rv = send( s, ( const char * ) &frame, frame_length, 0 ) ) != frame_length ){
		WARN_errno( rv == SOCKET_ERROR, "zap_config - send" );
		return 1;
	}

	// Wait for ready response.

	if ( zap_get_ready( s, 1, ZAP_TYPICAL_TIMEOUT_USEC ) ) {
		printf("\n[%s-%d]: Time out for waiting response\n", __FUNCTION__, __LINE__);
		return 1;
	}

	return 0;
}

int zap_send_performance_report( zap_station_t *station, zap_performance_frame_t *perf)
{
	zap_frame_t			frame;
	int					length;
	int					i, rv;
	unsigned __int32	*src, *dst;

	length = sizeof( zap_header_t ) + sizeof( zap_performance_frame_t );
	frame.header.length = htonl( length );
	frame.header.zap_frame_type = htonl( zap_type_performance_result );
	frame.header.zap_major_vers = htonl( ZAP_MAJOR_VERSION );
	frame.header.zap_minor_vers = htonl( ZAP_MINOR_VERSION );
	frame.header.zap_test_id = htonl( station->id );

	src = ( unsigned __int32 * ) perf;
	dst = ( unsigned __int32 * ) &( frame.payload.performance );
	for ( i = 0; i < ( sizeof( zap_performance_frame_t ) / 4 ); i++ ) {
		*dst = htonl( *src );
		dst++;
		src++;
	}

	/*send the report to zap */
	if ( ( rv = send( station->s_control, ( const char * )&frame, length, 0 ) ) != length ){
		WARN_errno( rv == SOCKET_ERROR, "zap_send_performance_report - send" );

		return 1;
	}

	return 0;
}

int zap_send_data(unsigned __int32 tcp,  
				  unsigned __int32 tid, 
				  unsigned __int32 batch, 
				  unsigned __int32 payload, 
				  SOCKET s, 
				  unsigned __int32 remote_ip, 
				  unsigned __int32 payload_length)
{
	static char		frame_bytes[65536];
	zap_frame_t		*frame = ( zap_frame_t * )&frame_bytes[0];
	int				rv;
	int						ttsk = 0;
	fd_set					write_fds;
	int						n_fd = 0;
	struct timeval			tv;

	if( tcp ) {
		FD_ZERO( &write_fds );
		FD_SET( s, &write_fds );
		N_UPDATE( n_fd, s );

		tv.tv_sec = 10;
		tv.tv_usec = 0;

		ttsk = select( n_fd+1, NULL, &write_fds, NULL, &tv );
		if ( ttsk == 0 ) {	
			WARN_errno( 1, "zap_send_data: Time out" );
			return 1;
		}
		else if ( ttsk == SOCKET_ERROR ) {
			return 1;
		}
	}

	if ( payload_length < ( sizeof( zap_header_t ) + sizeof( zap_data_frame_t ) ) ) {
		payload_length = sizeof( zap_header_t ) + sizeof( zap_data_frame_t );
	}

	frame->header.length = htonl( payload_length );
	frame->header.zap_frame_type = htonl( zap_type_data );
	frame->header.zap_major_vers = htonl( ZAP_MAJOR_VERSION );
	frame->header.zap_minor_vers = htonl( ZAP_MINOR_VERSION );
	frame->header.zap_test_id = htonl( tid );
	frame->payload.data.batch_number = htonl( batch );
	frame->payload.data.payload_number = htonl( payload );

	if ( remote_ip ) {
		struct sockaddr_in		addr;

		addr.sin_addr.s_addr = remote_ip;
		addr.sin_family		 = AF_INET;
		addr.sin_port		 = htons( ZAP_SERVICE_PORT );
		if ( ( rv = sendto( s, ( const char * )frame, payload_length, 0, ( struct sockaddr * )&addr, sizeof( addr ) ) ) != payload_length ){
			return 1;
		}
	} else {
		if ( ( rv = send( s, ( const char * )frame, payload_length, 0 ) ) != payload_length ){
			return 1;
		}
	}

	return 0;
}

int zap_send_null_frame( unsigned __int32 tid, SOCKET s, unsigned __int32 remote_ip)
{
	zap_frame_t				frame;
	int						frame_length, rv;
    struct sockaddr_in		addr;

    addr.sin_addr.s_addr= remote_ip;
    addr.sin_family		= AF_INET;
    addr.sin_port		= htons( ZAP_SERVICE_PORT );

	frame_length = sizeof( zap_header_t );
	frame.header.length = htonl( frame_length );
	frame.header.zap_frame_type = htonl( zap_type_null );
	frame.header.zap_major_vers = htonl( ZAP_MAJOR_VERSION );
	frame.header.zap_minor_vers = htonl( ZAP_MINOR_VERSION );
	frame.header.zap_test_id = htonl( tid );

	// Send request.
	if ( ( rv= sendto( s, ( const char * ) &frame, frame_length, 0, ( struct sockaddr * ) &addr, sizeof( addr ) ) ) != frame_length ){
		WARN_errno( rv == SOCKET_ERROR, "zap_send_null_frame - sendto" );
		return 1;
	}

	return 0;
}

int zap_send_data_complete( zap_station_t *station)
{
	zap_frame_t				frame;
	int						frame_length, rv;

	if ( station->s_tcp[0] == INVALID_SOCKET) {
		return 1;
	}

	frame_length = sizeof( zap_header_t ) + sizeof( zap_data_complete_frame_t );
	frame.header.length = htonl( frame_length );
	frame.header.zap_frame_type = htonl( zap_type_data_complete );
	frame.header.zap_major_vers = htonl( ZAP_MAJOR_VERSION );
	frame.header.zap_minor_vers = htonl( ZAP_MINOR_VERSION );
	frame.header.zap_test_id = htonl( station->id );
	frame.payload.data_complete.batch_number = htonl( station->batch_num );

	// Send request.
	if ( ( rv = send( station->s_tcp[0], ( const char * ) &frame, frame_length, 0 ) ) != frame_length ){
		WARN_errno( rv == SOCKET_ERROR, "zap_send_data_complete - send" );
		return 1;
	}

	return 0;
}

int zap_send_data_complete_response( zap_station_t *station, unsigned __int32 batch_number)
{
	zap_frame_t			frame;
	int					frame_length, rv;

	if ( station->s_tcp[0] == INVALID_SOCKET) {
		return 1;
	}

	frame_length = sizeof( zap_header_t ) + sizeof( zap_data_complete_response_frame_t );
	frame.header.length = htonl( frame_length );
	frame.header.zap_frame_type = htonl( zap_type_data_complete_response );
	frame.header.zap_major_vers = htonl( ZAP_MAJOR_VERSION );
	frame.header.zap_minor_vers = htonl( ZAP_MINOR_VERSION );
	frame.header.zap_test_id = htonl( station->id );
	frame.payload.data_complete_response.batch_number = htonl( batch_number );

	// Send complete response.
	if ( ( rv = send( station->s_tcp[0], ( const char * ) &frame, frame_length, 0 ) ) != frame_length ){
		WARN_errno( rv == SOCKET_ERROR, "zap_send_data_complete_response - send" );
		return 1;
	}

	return 0;
}

int zap_send_open_data_connection( unsigned __int32 tid, SOCKET s)
{
	zap_frame_t				frame;
	int						frame_length, rv;

	frame_length = sizeof( zap_header_t );
	frame.header.length = htonl( frame_length );
	frame.header.zap_frame_type = htonl( zap_type_open_data_conn );
	frame.header.zap_major_vers = htonl( ZAP_MAJOR_VERSION );
	frame.header.zap_minor_vers = htonl( ZAP_MINOR_VERSION );
	frame.header.zap_test_id = htonl( tid );

	// Send request.
	if ( ( rv = send( s, ( const char * ) &frame, frame_length, 0 ) ) != frame_length ){
		WARN_errno( rv == SOCKET_ERROR, "zap_send_open_data_connection - send" );
		return 1;
	}

	if ( zap_get_ready( s, 1, ZAP_TYPICAL_TIMEOUT_USEC ) ) {
		return 1;
	}

	return 0;
}

int zap_send_ready( unsigned __int32 tid, SOCKET s)
{
	zap_frame_t				frame;
	int						frame_length, rv;

	frame_length = sizeof( zap_header_t );
	frame.header.length = htonl( frame_length );
	frame.header.zap_frame_type = htonl( zap_type_ready );
	frame.header.zap_major_vers = htonl( ZAP_MAJOR_VERSION );
	frame.header.zap_minor_vers = htonl( ZAP_MINOR_VERSION );
	frame.header.zap_test_id = htonl( tid );

	// Send request.
	if ( ( rv = send( s, ( const char * ) &frame, frame_length, 0 ) ) != frame_length ){
		WARN_errno( rv == SOCKET_ERROR, "zap_send_ready - send" );
		return 1;
	}

	return 0;
}

int zap_data_connect( unsigned __int32 tid, SOCKET s, unsigned __int32 remote_station)
{
	zap_frame_t				frame;
	int						frame_length, rv;

	frame_length = sizeof( zap_header_t ) + sizeof( zap_connect_frame_t );
	frame.header.length = htonl( frame_length );
	frame.header.zap_frame_type = htonl( zap_type_connect );
	frame.header.zap_major_vers = htonl( ZAP_MAJOR_VERSION );
	frame.header.zap_minor_vers = htonl( ZAP_MINOR_VERSION );
	frame.header.zap_test_id = htonl( tid );
	frame.payload.connect.remote_ip = remote_station;

	// Send config request.
	if ( ( rv = send( s, ( const char * ) &frame, frame_length, 0 ) ) != frame_length ){
		WARN_errno( rv == SOCKET_ERROR, "zap_data_connect - send" );
		return 1;
	}
	if ( zap_get_ready( s, 1, ZAP_TYPICAL_TIMEOUT_USEC ) ) {
		return 1;
	}

	return 0;
}


int zap_test_start( unsigned __int32 tid, SOCKET s)
{
	zap_frame_t				frame;
	int						frame_length, rv;

	frame_length = sizeof( zap_header_t );
	frame.header.length = htonl( frame_length );
	frame.header.zap_frame_type = htonl( zap_type_test_start );
	frame.header.zap_major_vers = htonl( ZAP_MAJOR_VERSION );
	frame.header.zap_minor_vers = htonl( ZAP_MINOR_VERSION );
	frame.header.zap_test_id = htonl( tid );

	// Send config request.
	if ( ( rv = send( s, ( const char * ) &frame, frame_length, 0 ) ) != frame_length ){
		WARN_errno( rv == SOCKET_ERROR, "zap_test_start - send" );
		return 1;
	}
	if ( zap_get_ready( s, 1, ZAP_TYPICAL_TIMEOUT_USEC ) ) {
		return 1;
	}

	return 0;
}


int zap_test_complete( unsigned __int32 tid, SOCKET s)
{
	zap_frame_t				frame;
	int						frame_length, rv;

	frame_length = sizeof( zap_header_t );
	frame.header.length = htonl( frame_length );
	frame.header.zap_frame_type = htonl( zap_type_test_complete );
	frame.header.zap_major_vers = htonl( ZAP_MAJOR_VERSION );
	frame.header.zap_minor_vers = htonl( ZAP_MINOR_VERSION );
	frame.header.zap_test_id = htonl( tid );

	// Send config request.
	if ( ( rv = send( s, ( const char * ) &frame, frame_length, 0 ) ) != frame_length ){
		WARN_errno( rv == SOCKET_ERROR, "zap_test_start - send" );
		return 1;
	}

	if ( zap_get_ready( s, 1, ZAP_TYPICAL_TIMEOUT_USEC ) ) {
		return 1;
	}

	return 0;
}


// Gather statistics, etc, for this batch, and report them if necessary.
int zap_batch_done( zap_station_t *station )
{
	station->sample.frames_skipped += station->config.batch_size - station->payload_num;
	station->sample.first_frame_arrival_time = 0;
	station->sample.last_frame_arrival_time = 0;

	station->batch_num++;
	station->payload_num = 0;

	return 0;
}


int zap_batch_report( zap_station_t *station)
{
	zap_performance_frame_t		perf;
	unsigned __int64			bps;
	unsigned __int64			diff_usecs;
	unsigned __int64			temp;

	bps = 0;
	bps = station->sample.frames_received * station->config.payload_length * 8;   // Bits
	bps *= 1000000;  // Compensate for dividing by microseconds instead of seconds...
	diff_usecs = station->sample.total_time;
	if ( diff_usecs ) {
		bps /= diff_usecs;
	} else {
		bps = 0;
	}
	temp = 0;
	temp =  bps;
	perf.bits_per_second = ( unsigned __int32 ) bps;
	perf.first_payload_timestamp = 0;
	perf.last_payload_timestamp = ( unsigned __int32 )station->sample.total_time;
	perf.payloads_received = station->sample.frames_received;
	if ( station->config.batch_time ) {
		perf.payloads_dropped = station->sample.frames_skipped;
	} else {
		perf.payloads_dropped = station->sample.frames_skipped;
	}
	perf.payloads_outoforder = station->sample.frames_out_of_order;
	perf.payloads_repeated = station->sample.frames_repeated;
	perf.batch = station->sample_num;
	memset( &station->sample, 0, sizeof( station->sample ) );

	station->sample_num ++;

	return ( zap_send_performance_report( station, &perf ) );
}


int zap_batch_skip( zap_station_t *station, unsigned __int32 new_batch)
{
	zap_performance_frame_t	perf;

	fprintf( stdout, "zap_batch_skip\n" );
	while ( station->batch_num < new_batch ) {
		if ( !station->config.batch_time ) {
			perf.bits_per_second = 0;
			perf.first_payload_timestamp = 0;
			perf.last_payload_timestamp = 0;
			perf.batch = station->batch_num;
			perf.payloads_dropped = station->config.batch_size;
			perf.payloads_outoforder = 0;
			perf.payloads_repeated = 0;
			perf.payloads_received = 0;
			zap_send_performance_report( station, &perf );
		}
		station->batch_num++;
		station->payload_num = 0;
	}

	return 0;
}

int zap_process_data_complete(zap_station_t *station, zap_frame_t *f)
{
	unsigned __int32	remote_ip;

	if (station->batch_num == ntohl( f->payload.data_complete.batch_number )) {
		station->sample.first_frame_arrival_time = 0;
		station->sample.last_frame_arrival_time = 0;

		station->batch_num++;
		station->payload_num = 0;

		if ( !station->config.batch_time ) {
			if ( zap_batch_report( station ) ) {
				erk; 
				return 1; 
			}
		}
	}
	// This is just an indication we're done with our current batch.
	if ( station->config.batch_completion ) {
		return zap_send_data_complete_response( station, ntohl( f->payload.data_complete.batch_number ) );
	}

	return 0;
}


int zap_process_data( zap_station_t *station, zap_frame_t *frame, struct timeval *tv)
{
	unsigned __int32		rx_batch;
	unsigned __int32		rx_payload;
	__int64					usecs = get_current_usecs(  );

#ifndef WIN32
    if (tv) {
        usecs = tv->tv_usec;
        usecs += tv->tv_sec * 1000000;
    }
#endif
#if 0
	fprintf( stdout, " Rx batch = %3d, pay = %3d\n", 
		ntohl( frame->payload.data.batch_number ), 
		ntohl( frame->payload.data.payload_number ) );
#endif 
	rx_batch = ntohl( frame->payload.data.batch_number );
	rx_payload = ntohl( frame->payload.data.payload_number );

	// sanity checks...
	if ( rx_payload >= station->config.batch_size ) { 
		erk; 
		return 1; 
	}

	// Something get mussed?
	if ( station->batch_num < rx_batch ) {
		if ( zap_batch_done( station ) ) {
			erk;
			return 1; 
		}// Indicate that the current batch is done
		if ( !station->config.batch_time ) {
			if ( zap_batch_report( station ) ) { 
				erk;
				return 1; 
			}
		}
		if ( zap_batch_skip( station, rx_batch ) ) {
			erk; 
			return 1; 
		}// Process any skipped batches/frames.
		// return 0;  XXX ?
	}

	if ( station->batch_num > rx_batch ) {
		// Seriously out-of-order frame... Toss it
		return 0;
	}
	// We should be on the correct batch number now.

	if ( !station->sample.first_frame_arrival_time ) {
		station->sample.first_frame_arrival_time = usecs;
		station->payload_num = rx_payload + 1;
		return 0;
	}
	if ( !station->sample.last_frame_arrival_time ) {
		station->sample.last_frame_arrival_time = usecs;
		station->payload_num = rx_payload + 1;
		return 0;
	}
	station->sample.total_time += ( usecs - station->sample.last_frame_arrival_time );
	station->sample.last_frame_arrival_time = usecs;
	station->sample.payload_bytes += ntohl( frame->header.length );
	station->sample.frames_received++;

	// Check for various error conditions.
	if ( ( rx_payload < station->payload_num ) || ( station->batch_num > rx_batch ) ) {
		station->sample.frames_out_of_order++;
	}

	if ( rx_payload > station->payload_num ) {
		station->sample.frames_skipped += ( rx_payload - station->payload_num );
	}

	if ( rx_payload >= station->payload_num ) {
		station->payload_num = rx_payload + 1;
	}
	// If it 'twas the last packet, terminate this batch.
	if ( rx_payload == ( station->config.batch_size - 1 ) ) {
		if ( !station->config.batch_time ) {
			if ( zap_batch_report( station ) ) { 
				return 1; 
			}
		}
		if ( zap_batch_done( station ) ) {
			return 1;
		}

	}
	if ( station->config.batch_time &&
		( ( unsigned __int32 )station->sample.total_time >= station->config.batch_time ) ) {
		// Sample done!		
		if ( zap_batch_report( station ) ) { 
			erk; 
			return 1; 
		}
	}

	return 0;
}

int zap_rx_data( zap_server_t *server, SOCKET sock, int tcp, fd_set *fd, zap_station_t *station_cleaned)
{
	zap_frame_t			*frame;
	zap_station_t		*station;
	__int64				usecs;
	unsigned __int32	remote_ip;
	unsigned __int32	read_frame = 1;
	struct timeval      tv;

	while ( read_frame ) {
		// Read a frame...
		if ( zap_read_frame( sock, tcp, &frame, &remote_ip, (tcp)?NULL:(&tv) ) ) {
			return 1;
		}
		if ( zap_find_station( ntohl( frame->header.zap_test_id ), server, &station, 0 ) ) {
			return 1;
		}
		station_cleaned = station;
		
		switch( station->state ) {
			case zap_station_state_rx_config:
			case zap_station_state_running_tx:
			case zap_station_state_running_rx:
				break;
			case zap_station_state_complete:
				erk;
			case zap_station_state_init:
				erk;
			case zap_station_state_off:
				erk;
			default:
				erk;
				return 1;
		}

		switch ( ntohl( frame->header.zap_frame_type ) ) {
			case zap_type_null:
				// If we are the transmitter, and if we don't know who to transmit to, remember this remote IP.
				if ( ( station->config.tx_ip == 0 ) && ( station->config.tx ) ) {
					station->config.tx_ip = remote_ip;
				}
				break;

			case zap_type_data:
				if ( station->state != zap_station_state_running_rx ) {
					erk;
					return 1;
				}
				if ( zap_process_data( station, frame, (tcp)?(NULL):(&tv) ) ) {	
					return 1; 
				}
				break;

			case zap_type_data_complete:
				if ( station->state != zap_station_state_running_rx ) {
					erk;
					return 1;
				}
				if ( zap_process_data_complete(station, frame ) ) { 
					erk;
					return 1; 
				}
				break;

			case zap_type_data_complete_response:
				if ( station->state != zap_station_state_running_tx ) {
					erk;
					return 1;
				}
				station->last_completed_batch = ntohl( frame->payload.data_complete.batch_number );
				break;

			case zap_type_connect:
				if ( station->s_tcp_count < ZAP_MAX_RECEIVERS ) {
					// Create socket.
					if ( zap_socket( station->config.buf_required, 1, &( station->s_tcp[station->s_tcp_count] ) ) ) { 
						erk;
						return 1; 
					}
					if( station->config.ip_tos ) {
					    zap_set_tos( station->s_tcp[station->s_tcp_count], &station->config.ip_tos );
					}

					// Connect socket.
					if ( zap_connect( frame->payload.connect.remote_ip, 1, station->s_tcp[station->s_tcp_count], ZAP_TYPICAL_TIMEOUT_USEC ) ) { 
						return 1; 
					}

					// Send a "open data connection" message so the other side has a clue.
				if ( zap_send_open_data_connection( station->id, station->s_tcp[station->s_tcp_count] ) ) { 
						//erk; 
						printf("\n[%s-%d]: Can not open data connection\n", __FUNCTION__, __LINE__);			
						return 1; 
					}
					// Send a null UDP frame to "open" the UDP connection.
					if ( zap_send_null_frame( station->id, server->udp_socket_tx, frame->payload.connect.remote_ip ) ) { 
						erk; 
						return 1; 
					}
					// If we are the transmitter, and if we don't know who to transmit to, remember this remote IP.
					if ( ( station->config.tx_ip == 0 ) && ( station->config.tx ) ) {
						station->config.tx_ip = frame->payload.connect.remote_ip;
					}

					station->s_tcp_count++;
					if ( zap_send_ready( station->id, sock ) ) {
						erk; 
						return 1; 
					}
					return 0;
				}
				return 1;
			case zap_type_test_start:

				if ( ( station->config.tx ) &&
					( station->state == zap_station_state_rx_config ) ) {
					if ( zap_send_ready( station->id, sock ) ) {
						erk; 
						return 1; 
					}
					usecs = get_current_usecs(  );
					station->state = zap_station_state_running_tx;
					station->batch_start_usec = 0;
					station->payload_usec = 0;
					station->blocked = 0;
					return 0;
				} else {
					erk;
					return 1;
				}
				break;
			case zap_type_test_complete:
				if ( zap_send_ready( station->id, sock) ) {
					erk; 
					return 1; 
				}
				return 1;

			case zap_type_open_data_conn:
				erk;
			case zap_type_open_control_conn:
				erk;
			case zap_type_performance_result:
				erk;
			case zap_type_ready:
				erk;
			default:
				erk;
				return 1;
				break; /*code unreachable*/
		}

		read_frame = 0;
#ifdef WIN32
		if ( ioctlsocket( sock, FIONREAD, &read_frame ) ) {
			return 1;
		}
#else
		if ( ioctl( sock, FIONREAD, &read_frame ) ) {
			return 1;
		}
#endif
	}
	return 0;
}


void exit_error( char *str )
{
	fprintf( stderr, "Error: %s", str );
	cleanup_exit( 1 );
}

void InitLog(  )
{
	char          szModuleName[_MAX_PATH];
	char          *pname;
	int           len;
#ifdef WIN32
	HANDLE        proc;
#endif 

	memset( &currPath, 0, _MAX_PATH );
#ifdef WIN32
	proc = GetCurrentProcess(  );
	
	if ( GetModuleFileNameEx( proc, NULL, (LPSTR)szModuleName, _MAX_PATH ) ){
		pname= ( char* ) _tcsrchr( &szModuleName, '\\' );
		len = ( int ) ( pname - ( char* )szModuleName );
		strncpy( &currPath, ( const char * ) &szModuleName, len );
	}
	else {
		strncpy( &currPath, ( const char * ) ".", 1 );
	}
	strncat( &currPath, ( const char * ) ZAPD_LOGFILE_NAME, ( size_t ) strlen( ZAPD_LOGFILE_NAME ) );
#else
	strncpy( currPath, ".", 1 );
	strncat( currPath, ( const char * ) ZAPD_LOGFILE_NAME, ( size_t ) strlen( ZAPD_LOGFILE_NAME ) );
#endif 
}
