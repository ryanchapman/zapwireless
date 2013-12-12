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


#ifndef ZAPLIB_H 
#define ZAPLIB_H

#ifdef WIN32
#include <winsock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <Psapi.h>
#define		N_UPDATE( i, sock ) i++;
#define		socklen_t			int


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
#include <signal.h>
#include <sys/types.h>
#include <errno.h>


#define		N_UPDATE( i, sock ) if ( sock >= i ) i = sock + 1;
#define		__int8				char
#define		__int16				short
#define		__int32				int
#define		__int64				long long
#define		SOCKET				int
#define     _MAX_PATH           256
#define		INVALID_SOCKET		-1 
#define		SOCKET_ERROR		-1

#endif // !WIN32

#define SLEEP_TIME 500


#define erk		fprintf( stderr, "\n\nErk, occurred line %d, file %s\n\n", __LINE__, __FILE__ )
#define errOut	printf( "%s( %d ) : ", __FILE__, __LINE__ ); printf

#define ZAP_MAJOR_VERSION					1
#define ZAP_MINOR_VERSION					83

#define MAX_PACKET_LEN						65536
#define ZAP_SERVICE_PORT					18301

#define ZAP_MAX_RECEIVERS					20
#define ZAP_MAX_STATIONS					20		// Each server can operate as 20 simultaneous stations, max.
#define ZAPD_LOGFILE_NAME					"ZapdDbg.log"
#define LOG_MESSAGE_OUTPUT_BUFFER_SIZE		256

#define GENERIC_INFO                        1
#define DETAIL_INFO                         2

#define NO_MTUDISC

#ifndef NO_MTUDISC
#if defined(LINUX)
 #define ip_pmtudisc_str(val) \
  (val == IP_PMTUDISC_DONT) ? "IP_PMTUDISC_DONT (Never DF)" : \
  (val == IP_PMTUDISC_WANT) ? "IP_PMTUDISC_WANT (Per route DF)" : \
  (val == IP_PMTUDISC_DO)   ? "IP_PMTUDISC_DO (Always DF)" : \
         "IP_PMTUDISC_WANT (System default)"
#else
#define IP_PMTUDISC_DONT   0 /* Never send DF frames.  */
#define IP_PMTUDISC_WANT   1 /* Use per route hints.  */
#define IP_PMTUDISC_DO     2 /* Always DF.  */
#define ip_pmtudisc_str(val) \
 "unsupported"
#ifndef IP_MTU_DISCOVER
#define IP_MTU_DISCOVER 10
#endif
#endif
extern int pmtudisc;  // Path MTU discovery: System default (-1), DONT (0), WANT (1) or DO (2)
#endif // NO_MTUDISC

typedef struct {
	unsigned __int32 *data;
	int gather_count, gather_max;
	int order;						// If 0, then big #s are good, else big #s are bad.
} zap_history_t;


//
// Rx/Tx payload state. An array of this state of approximately batch_size will be needed to successfully run.
//
typedef struct {
	unsigned __int32		batch_number;						// packet batch number.
	unsigned __int32		payload_number;						// packet payload number.
	unsigned __int32		success;							// success/failure indication. ( Sort of a "valid" indication as well )
	__int64					timestamp;							// Rx/Tx timestamp.
} zap_payload_track_t;


//
// Rx/Tx batch state. An array of this state will be used for tracking stats/etc associated with batches.
//
typedef struct {
	unsigned __int64		total_time;
	unsigned __int64		first_frame_arrival_time;
	unsigned __int64		second_frame_arrival_time;
	unsigned __int64		last_frame_arrival_time;
	unsigned __int32		payload_bytes;
	unsigned __int32		frames_out_of_order;
	unsigned __int32		frames_repeated;
	unsigned __int32		frames_skipped;
	unsigned __int32		frames_received;
	unsigned __int32		success;
} zap_sample_track_t;


//
// Station configuration.
//
// IMPORTANT: IT IS REQUIRED THAT THIS STRUCTURE CONTAIN ONLY 32 bit INTS!
//
typedef struct
{
	unsigned __int32		payload_transmit_delay;				// usecs to delay between transmitting each payload bytes.
	unsigned __int32		batch_transmit_delay;				// usecs to delay between transmitting each batch. 
																// ( after in/out of band responses have been received )
	unsigned __int32		payload_timeout;					// payload->payload timeout.
	unsigned __int32		batch_timeout;						// batch->batch timeout.
	unsigned __int32		payload_length;						// The payload length of each transmission.
	unsigned __int32		batches;							// The number of batches of transmissions to send.
	unsigned __int32		batch_size;							// The number of payloads per batch.
	unsigned __int32		batch_time;							// The time, in microseconds ( approx ) of each batch.
																// batch_size and batch_time are mutually exclusive,
																// and should be set to zero to disable.
	unsigned __int32		asynchronous;						// The degree to which multiple batches of traffic may
																// be outstanding. >= 1 for all cases.
	unsigned __int32		batch_inband_response;				// Whether there will be a response on batch
																// completion. Allows equivalent of an echo service.
	unsigned __int32		batch_completion;					// whether there will be a completion message sent tx->rx via TCP
																// at the end of each batch, which would then be followed by a 
																// rx->tx completion response message.
	unsigned __int32		batch_report_rate;					// The rate at which batch reports are sent. 1 = every batch report,
																// 10 = every 10 batches report, etc, 0 = none, etc.

	unsigned __int32		tcp;								// If set, indicates TCP payload. Else UDP payload. (TCP support is deprecated)
	unsigned __int32		max_test_time;						// The maximum time of the test, in seconds.

	unsigned __int32		tx;									// Are we the transmitter or receiver?
	unsigned __int32		tx_ip;								// The IP address to which UDP frames are sent.

	unsigned __int32		buf_required;						// Buffer space required to hold all outstanding frames.
        unsigned __int32		ip_tos;						// IP ToS
} zap_station_config_t;


typedef enum {
	zap_station_state_off,				// Station is not active at all.
	zap_station_state_init,				// Station has seen connections, but has no config.
	zap_station_state_rx_config,		// Station has connections, config.
	zap_station_state_running_tx,		// Station is running, transmitter.
	zap_station_state_running_rx,		// Station is running, receiver.
	zap_station_state_complete			// Run has completed.
} zap_station_state_enum;


// All the state associated with a station.
typedef struct
{
	unsigned __int32		id;							// Unique test ID.
	zap_station_state_enum	state;						// State of this station. ( active, testing, waiting for config, etc )

	zap_station_config_t	config;
	unsigned __int32		blocked;					// ( tx ) If non-zero, this station is blocked waiting for someone else to do something.
	unsigned __int32		next_event;					// ( tx ) How long before the next tx event for this station.

	SOCKET					s_control;					// TCP Control socket.
	SOCKET					s_tcp[ZAP_MAX_RECEIVERS];	// TCP data socket, in-band.
	unsigned __int32		s_tcp_count;

	unsigned __int32		batch_num;					// The current batch we are working on.
	unsigned __int32		sample_num;					// The current sample we are working on.
	unsigned __int32		last_completed_batch;		// The last completed batch.
	__int64					batch_start_usec;			// The time the current batch started.
	unsigned __int32		payload_num;				// The next payload to be received, OR the next payload to transmit.
	__int64					payload_usec;				// The timestamp of said payload.

	zap_payload_track_t		*payload_xx_deprecated;		// Array of payload tracking state.
	zap_sample_track_t		sample;						// The sample we are currently tracking.

} zap_station_t;


// All the state local to a server.
typedef struct 
{
	zap_station_t			stations[ZAP_MAX_STATIONS];		// Station state.

	SOCKET					tcp_socket;						// TCP socket for accepting connections.
	SOCKET					udp_socket_rx;					// UDP socket for receiving all UDP data.
	SOCKET					udp_socket_tx;					// UDP socket for transmitting all UDP data.
} zap_server_t;


#ifndef SHUT_RD
#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2
#endif // SHUT_RD

// 
// The test configuration and state.. ( Controller state )
//
typedef struct {
	unsigned __int32		tid;									// test ID.
	unsigned __int32		txs_ip_address;							// Transmit station IP address, data.
	unsigned __int32		txs_ip_address_ctl;						// Transmit station IP address, control.
	SOCKET					txs_socket_ctl;							// Socket for communicating with tx.
	zap_station_config_t	station_config;							// Station configuration.
	unsigned __int32		rxs_count;								// Number of receive stations.
	unsigned __int32		rxs_ip_address[ZAP_MAX_RECEIVERS];		// Receive station IP addresses, data.
	unsigned __int32		rxs_ip_address_ctl[ZAP_MAX_RECEIVERS];	// Receive station IP addresses, control.
	SOCKET					rxs_socket_ctl[ZAP_MAX_RECEIVERS];		// Sockets for communicating with rx.
	unsigned __int32		open_reverse;							// Indicates data connections should be opened from rx->tx.
	unsigned __int32		multi_ip_address;						// IP Address of multicast
	unsigned __int32		ip_tos;									// ToS to use
	unsigned __int32		server;									// Indicates this is a server, not a controller.
	unsigned __int32		test_seconds;							// Test Duration in seconds
	__int64					results_timeout;						// Max amount of time to wait for results
	char					*filename;								// Log the report 
	char					*logfile;								// Log file when package drops
	char					*debugfile;								// Dump some errors code and show some segments of result.
	unsigned __int32		start_point;							//Start row for dump file
	unsigned __int32		end_point;								//End row for dump file
	char					*tag;									// Tag String
	char					*sub;									// Subtag String
	char					*note;									// Note String
	unsigned __int32		average;
} zap_config_t;


typedef enum {
	zap_type_data,									// 0 Data frame. May or may not be "echoed," depending on configuration.
	zap_type_data_complete,							// 1 Data complete frame- sent to indicate a batch has been fully sent.
	zap_type_data_complete_response,				// 2 Data complete response - sent to indicate completion frame has been received.
	zap_type_test_complete,							// 3 Test complete- sent by server to controller indicating the test is complete.
	zap_type_open_data_conn,						// 4 Frame sent at the beginning of a new data connection.
	zap_type_open_control_conn,						// 5 Frame sent at the beginning of a new control connection.
	zap_type_connect,								// 6 Frame sent to tell a station to connect to another station.
	zap_type_ready,									// 7 Frame sent to indicate the station is ready.
	zap_type_test_start,							// 8 Frame sent to start the test, once all is configured.
	zap_type_performance_result,					// 9 Frame sent from station to controller reporting performance.
	zap_type_null,									// 10 Null frame.
} zap_frame_enum;
typedef struct {
	unsigned __int32		payloads_received;				// Number of payloads received during the interval.
	unsigned __int32		payloads_dropped;				// Number of payloads missing during the interval. ( May be counted as out of order )
	unsigned __int32		payloads_outoforder;			// Number of payloads out of order during the interval.
	unsigned __int32		payloads_repeated;				// Number of payloads that we have already received. may be back to back
															// or seriously out of order.
	unsigned __int32		batch;							// ID of the first batch this frame describe.
	unsigned __int32		first_payload_timestamp;		// Microsecond timestamp of the first payload
	unsigned __int32		last_payload_timestamp;			// Microsecond timestamp of the last payload
	unsigned __int32		bits_per_second;				// Calculated bits per second during the interval. ( as accurate as receiver can see )
} zap_performance_frame_t;

typedef struct {
	unsigned __int32		batch_number;
	unsigned __int32		payload_number;
} zap_data_frame_t;

typedef struct {
	unsigned __int32		batch_number;
} zap_data_complete_frame_t;

typedef struct {
	unsigned __int32		batch_number;
} zap_data_complete_response_frame_t;

typedef struct {
	zap_station_config_t	config;
} zap_open_control_frame_t;

typedef struct {
	unsigned __int32		remote_ip;
        unsigned __int32		ip_tos;        
} zap_connect_frame_t;

typedef struct {
	unsigned __int32		zap_major_vers;					// Zap major version.
	unsigned __int32		zap_minor_vers;					// Zap minor version.
	unsigned __int32		zap_test_id;					// 32 bit "unique" test ID.
	zap_frame_enum			zap_frame_type;					// The type of frame defined this is.
	unsigned __int32		length;							// The length of this frame, including header.
} zap_header_t;

typedef struct {
	zap_header_t	header;
	union {
		zap_data_frame_t					data;
		zap_data_complete_frame_t			data_complete;
		zap_data_complete_response_frame_t 	data_complete_response;
		zap_open_control_frame_t			open_control;
		zap_connect_frame_t					connect;
		zap_performance_frame_t				performance;
	} payload;
} zap_frame_t;

#ifdef WIN32
/* under windows, emulate unix signals */
enum {
	SIGINT,
	SIGTERM,
	SIGPIPE,
	_NSIG
};

typedef void Sigfunc( int );                         // Define function pointer
typedef Sigfunc *SigfuncPtr;                       // Define function pointer

void zap_exit( int inSigno );                      // called when the Ctrl+C is pressed on zap
void zapd_exit( int inSigno );                     // called when the Ctrl+C is pressed on zapd
SigfuncPtr signal( int inSigno, SigfuncPtr inFunc );
BOOL WINAPI Catch_Ctrl_C( DWORD ctrl_type );         // called when the Ctrl+C is pressed on zap/zapd
#endif


void exit_error( char *str );
void zap_clean_station( zap_station_t *station, fd_set *fd);

int zap_find_station( unsigned __int32 tid, zap_server_t *server, zap_station_t **station, unsigned __int32 add);
int zap_read_frame( SOCKET s, unsigned __int32 tcp, zap_frame_t **rx_frame, unsigned __int32 *remote_ip, struct timeval *tv);
int zap_socket( unsigned __int32 buff_size, int tcp, SOCKET *sock);
int zap_bind( SOCKET sock);
int zap_listen( SOCKET sock);
int zap_accept( zap_server_t *server, SOCKET sock, zap_station_t *station_cleaned);
int zap_connect( unsigned __int32 remote_ip, int tcp, SOCKET sock, unsigned __int32 usec_timeout);
int zap_get_ready( SOCKET s, unsigned __int32 tcp, unsigned __int32 usecs);
int zap_config( unsigned __int32 tid, SOCKET s, zap_station_config_t *conf);
int zap_send_data( unsigned __int32 tcp, unsigned __int32 tid, unsigned __int32 batch, unsigned __int32 payload, SOCKET s, unsigned __int32 remote_ip, unsigned __int32 payload_length);
int zap_send_performance_report( zap_station_t *station, zap_performance_frame_t *perf);
int zap_send_null_frame( unsigned __int32 tid, SOCKET s, unsigned __int32 remote_ip);
int zap_send_open_data_connection( unsigned __int32 tid, SOCKET s);
int zap_send_ready( unsigned __int32 tid, SOCKET s);
int zap_data_connect( unsigned __int32 tid, SOCKET s, unsigned __int32 remote_station);
int zap_test_start( unsigned __int32 tid, SOCKET s);
int zap_test_complete( unsigned __int32 tid, SOCKET s);
int zap_control_process_rx( zap_config_t *config, SOCKET s, zap_history_t *history, zap_performance_frame_t *perf, double* total, double* throughput_array );
int zap_compile_results( zap_config_t *config, zap_history_t *rate_history, zap_performance_frame_t *perf );

int zap_send_data_complete( zap_station_t *station);
int zap_rx_data( zap_server_t *server, SOCKET sock, int tcp, fd_set *fd, zap_station_t *station_cleaned);


char *inet_ntoa2( unsigned __int32 addr );
__int64 get_current_usecs( void );
void net_init( void );
void cleanup_exit( int err );
void InitLog(  );
#endif
