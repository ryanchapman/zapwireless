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

#include "../zaplib/zaplib.h"
#include "../zaplib/error.h"

zap_config_t *pcfg;

#define		INFO	0
#define		ERROR	1

// Prototypes
unsigned __int32 get_stats( zap_history_t *history, double percentile );
void gather_stats( zap_history_t *history, unsigned __int32 value );


// Generate an approximately unique test id.
unsigned __int32 zap_generate_tid( void )
{
	time_t	tv;

	time( &tv );
	return ( unsigned __int32 )tv;
}


int compare( const void *d1, const void *d2 )
{
	unsigned __int32 *data1 = ( unsigned __int32 * )d1;
	unsigned __int32 *data2 = ( unsigned __int32 * )d2;

	if ( *data1 < *data2 ){
		return -1;
	}
	if ( *data1 > *data2 ){
		return 1;
	}

	return 0;
}
int zap_log_error(zap_config_t *config, char *msg, int type)
{
	FILE            *fileio;

	if(config->debugfile) {
		fileio = fopen( config->debugfile, "a+" );
		if ( !fileio ) {
			fprintf( stderr, "Error, file probably open by another application.\n" );
			return 1;
		}
		if(type == ERROR) {
			fprintf( fileio, "[ERROR]: %s", msg);
		}
		else {
			fprintf( fileio, "[INFO]: %s", msg);
		}

		fprintf( fileio, "\n");
		fclose( fileio );
	}
	return 0;

}
/*This function uses to dump the report to file from start_point to end_point 
It just uses for -D option
*/
int zap_debug_dump_file( zap_config_t *config, 
						  zap_performance_frame_t *perf, 
						  zap_performance_frame_t p,
						  const double throughput,
						  const double total,
						  zap_history_t *history)
{
#ifdef WIN32
	__time64_t      timer;
#else
	time_t          timer;
#endif
	double          walk;
	char            time_str[30];
	FILE            *fileio;
	int				i;
	double race[] =	{ 0.0, 0.5, 0.90, 0.95, 0.990, 0.999 };

#ifdef WIN32
	_time64( &timer );
	sprintf( time_str, "%s", _ctime64( &timer ) );
#else
	time( &timer );
	ctime_r(&timer, time_str);
#endif
	// Take off the newline.
	time_str[24] = 0;

	fileio = fopen( config->debugfile, "a+" );

	if ( !fileio ) {
		fprintf( stderr, "Error, file probably open by another application.\n" );
		return 1;
	}

	fprintf( fileio, "%5d: %s->%s %6d=rx %3d=dr %3d=oo %3d=rp %5d=rx in %7.1fms  %6.1fmbps  %6.1f | ",
		p.batch,
		inet_ntoa2( config->txs_ip_address ),
		inet_ntoa2( config->rxs_ip_address[0] ),
		perf->payloads_received,
		perf->payloads_dropped,
		perf->payloads_outoforder,
		perf->payloads_repeated,
		p.payloads_received,
		( double )( ( double )( p.last_payload_timestamp - p.first_payload_timestamp ) ) / 1000.0,
		throughput,
		( p.batch > 0 )? ( total/( p.batch+1 ) ) : ( total ) );

	for ( i = 0; i < ( ( sizeof race ) / sizeof( double ) ); i++ ) {
		fprintf( fileio, "%4.1f ", get_stats( history, race[i] ) / 1000000.0 );
	}
	fprintf( fileio, "\n");
	fclose( fileio );

	return 0;
}


/*This function uses to log the package drop, it just uses for -L option*/
int zap_pkg_drop_dump_file( zap_config_t *config, 
						  zap_performance_frame_t *perf)
{
#ifdef WIN32
	__time64_t      timer;
#else
	time_t          timer;
#endif
	double          walk;
	char            time_str[30];
	FILE            *fileio;
	char            delimit=',';
	int				new_file;
	int				i;

#ifdef WIN32
	_time64( &timer );
	sprintf( time_str, "%s", _ctime64( &timer ) );
#else
	time( &timer );
	ctime_r(&timer, time_str);
#endif
	// Take off the newline.
	time_str[24] = 0;

	fileio = fopen( config->logfile, "r" );
	if ( !fileio ) {
		new_file = 1;
	} else {
		new_file = 0;
		fclose( fileio );
	}

	fileio = fopen( config->logfile, "a+" );

	if ( !fileio ) {
		fprintf( stderr, "Error, file probably open by another application.\n" );
		return 1;
	}

	// Dump package drop information.
	if ( new_file ) {
		// If a new file, make the first row have text tags for all the columns

		fprintf( fileio, "Zap Version%c", delimit );
		fprintf( fileio, "Filename%c", delimit );
		fprintf( fileio, "Protocol%c", delimit );

		fprintf( fileio, "Invert Open%c", delimit );
		fprintf( fileio, "Tx IP%c", delimit );
		fprintf( fileio, "Rx IP%c", delimit );
		fprintf( fileio, "Multicast%c", delimit );
		fprintf( fileio, "ToS%c", delimit );

		fprintf( fileio, "Samples%c", delimit );
		fprintf( fileio, "Sample Size%c", delimit );
		fprintf( fileio, "Payload Length%c", delimit );
		fprintf( fileio, "Payload Transmit Delay%c", delimit );

		fprintf( fileio, "Payloads Received%c", delimit );
		fprintf( fileio, "Payloads Dropped%c", delimit );
		fprintf( fileio, "Payloads Repeated%c", delimit );
		fprintf( fileio, "Payloads Outoforder%c", delimit );

		fprintf( fileio, "Date%c", delimit );
		fprintf( fileio, "Notes%c", delimit );
		fprintf( fileio, "Tag%c", delimit );
		fprintf( fileio, "Sub Tag%c", delimit );

		fprintf( fileio, "\n" );
	}

	// Add this test's results to the last row of the file

	fprintf( fileio, "%d.%d%c", ZAP_MAJOR_VERSION, ZAP_MINOR_VERSION, delimit );
	fprintf( fileio, "%s%c", config->logfile, delimit );
	if ( config->station_config.tcp ){
		fprintf( fileio, "tcp%c", delimit );
	} else {
		fprintf( fileio, "udp%c", delimit );
	}

	fprintf( fileio, "%s%c", config->open_reverse ? "On" : "Off", delimit );
	fprintf( fileio, "%s:%s%c", inet_ntoa2( config->txs_ip_address ), inet_ntoa2( config->txs_ip_address_ctl ), delimit );
	for ( i = 0; i < ( int ) config->rxs_count; i++ ){
		fprintf( fileio, "%s:%s", inet_ntoa2( config->rxs_ip_address[i] ), inet_ntoa2( config->rxs_ip_address_ctl[i] ) );
	}
	fprintf( fileio, "%c", delimit );
	
	fprintf( fileio, "%s%c",	   config->multi_ip_address ? inet_ntoa2( config->multi_ip_address ) : "Off", delimit );	
	fprintf( fileio, "%.2hhXh%c", ( char ) config->ip_tos, delimit );

	fprintf( fileio, "%d%c", config->station_config.batches, delimit );
	fprintf( fileio, "%d%c", config->station_config.batch_size, delimit );
	fprintf( fileio, "%d%c", config->station_config.payload_length, delimit );
	fprintf( fileio, "%d%c", config->station_config.payload_transmit_delay, delimit );

	fprintf( fileio, "%d%c", perf->payloads_received, delimit );
	fprintf( fileio, "%d%c", perf->payloads_dropped, delimit );
	fprintf( fileio, "%d%c", perf->payloads_repeated, delimit );
	fprintf( fileio, "%d%c", perf->payloads_outoforder, delimit );

	fprintf( fileio, "%s%c", time_str, delimit );
	fprintf( fileio, "%s%c", config->note,delimit );
	fprintf( fileio, "%s%c", config->tag, delimit );
	fprintf( fileio, "%s%c", config->sub, delimit );
	fprintf( fileio, "\n" );
	fclose( fileio );

	return 0;
}

int zap_control_dump_file( zap_config_t *config, 
						  zap_performance_frame_t *perf, 
						  zap_history_t *rates,
						  double total )
{
#ifdef WIN32
	__time64_t      timer;
#else
	time_t          timer;
#endif
	double          walk;
	char            time_str[30];
	FILE            *fileio;
	char            delimit=',';
	int				new_file;
	int				i;

#ifdef WIN32
	_time64( &timer );
	sprintf( time_str, "%s", _ctime64( &timer ) );
#else
	time( &timer );
	//sprintf( time_str, "%s", ctime( &timer ) );
	ctime_r(&timer, time_str);
#endif
	// Take off the newline.
	time_str[24] = 0;

	fileio = fopen( config->filename, "r" );
	if ( !fileio ) {
		new_file = 1;
	} else {
		new_file = 0;
		fclose( fileio );
	}

	fileio = fopen( config->filename, "a+" );

	if ( !fileio ) {
		fprintf( stderr, "Error, file probably open by another application.\n" );
		return 1;
	}

	// Dump percentiles.
	if ( new_file ) {
		// If a new file, make the first row have text tags for all the columns

		// WARNING -- other software ( VPT ) may now rely on these text tags for testing automation.
		// Removal of elements or editing of this tag text is discouraged.   Please change VPT if necessary.
		fprintf( fileio, "Zap Version%c", delimit );
		fprintf( fileio, "Filename%c", delimit );
		fprintf( fileio, "Protocol%c", delimit );

		fprintf( fileio, "Invert Open%c", delimit );
		fprintf( fileio, "Tx IP%c", delimit );
		fprintf( fileio, "Rx IP%c", delimit );
		fprintf( fileio, "Multicast%c", delimit );
		fprintf( fileio, "ToS%c", delimit );

		fprintf( fileio, "Samples%c", delimit );
		fprintf( fileio, "Sample Size%c", delimit );
		fprintf( fileio, "Payload Length%c", delimit );
		fprintf( fileio, "Payload Transmit Delay%c", delimit );

		fprintf( fileio, "Payloads Received%c", delimit );
		fprintf( fileio, "Payloads Dropped%c", delimit );
		fprintf( fileio, "Payloads Repeated%c", delimit );
		fprintf( fileio, "Payloads Outoforder%c", delimit );
		//fprintf( fileio, "Avg Throughput%c", delimit );

		fprintf( fileio, "Date%c", delimit );
		fprintf( fileio, "Notes%c", delimit );
		fprintf( fileio, "Tag%c", delimit );
		fprintf( fileio, "Sub Tag%c", delimit );

		// XXX percentiles must match below
		for ( walk = 0.0; walk < 0.991; walk += 0.01 ) {    // 1.0% increments from 0% to 99%
			fprintf( fileio, "%4.1f%%%c", walk*100.0, delimit );
		}
		for ( walk = 0.991; walk < 1.001; walk += 0.001 ) { // 0.1% increments from 99% to 100%
			fprintf( fileio, "%4.1f%%%c", walk*100.0, delimit );
		}
		fprintf( fileio, "\n" );
	}

	// Add this test's results to the last row of the file

	fprintf( fileio, "%d.%d%c", ZAP_MAJOR_VERSION, ZAP_MINOR_VERSION, delimit );
	fprintf( fileio, "%s%c", config->filename, delimit );
	if ( config->station_config.tcp ){
		fprintf( fileio, "tcp%c", delimit );
	} else {
		fprintf( fileio, "udp%c", delimit );
	}

	fprintf( fileio, "%s%c", config->open_reverse ? "On" : "Off", delimit );
	fprintf( fileio, "%s:%s%c", inet_ntoa2( config->txs_ip_address ), inet_ntoa2( config->txs_ip_address_ctl ), delimit );
	for ( i = 0; i < ( int ) config->rxs_count; i++ ){
		fprintf( fileio, "%s:%s", inet_ntoa2( config->rxs_ip_address[i] ), inet_ntoa2( config->rxs_ip_address_ctl[i] ) );
	}
	fprintf( fileio, "%c", delimit );
	
	fprintf( fileio, "%s%c",	   config->multi_ip_address ? inet_ntoa2( config->multi_ip_address ) : "Off", delimit );	
	fprintf( fileio, "%.2hhXh%c", ( char ) config->ip_tos, delimit );

	fprintf( fileio, "%d%c", config->station_config.batches, delimit );
	fprintf( fileio, "%d%c", config->station_config.batch_size, delimit );
	fprintf( fileio, "%d%c", config->station_config.payload_length, delimit );
	fprintf( fileio, "%d%c", config->station_config.payload_transmit_delay, delimit );

	fprintf( fileio, "%d%c", perf->payloads_received, delimit );
	fprintf( fileio, "%d%c", perf->payloads_dropped, delimit );
	fprintf( fileio, "%d%c", perf->payloads_repeated, delimit );
	fprintf( fileio, "%d%c", perf->payloads_outoforder, delimit );
	//fprintf( fileio, "%.1f%c", ( config->station_config.batches>0 )?( total/config->station_config.batches ):0, delimit );

	fprintf( fileio, "%s%c", time_str, delimit );
	fprintf( fileio, "%s%c", config->note,delimit );
	fprintf( fileio, "%s%c", config->tag, delimit );
	fprintf( fileio, "%s%c", config->sub, delimit );
	// XXX percentiles must match above!
	for ( walk = 0.0; walk < .991; walk += 0.01 ) {		// 1.0% increments from 0% to 99%
		fprintf( fileio, "%4.1f%c", get_stats( rates, walk )/1000000.0, delimit );
	}
	for ( walk = 0.991; walk < 1.001; walk += 0.001 ) {	// 0.1% increments from 99% to 100%
		fprintf( fileio, "%4.1f%c", get_stats( rates, walk )/1000000.0, delimit );
	}
	fprintf( fileio, "\n" );
	fclose( fileio );

	return 0;
}

/* -------------------------------------------------------------------
* zap_exit
*
* Quietly exits. This protects some against being called multiple
* times. ( TODO: should use a mutex to ensure ( num++ == 0 ) is atomic. )
* ------------------------------------------------------------------- */

void zap_exit( int inSigno )
{
	static int		num = 0;
	unsigned int	i;

	if ( pcfg != NULL ) {
		for ( i = 0; i < pcfg->rxs_count; i++ ) {
			if ( pcfg->rxs_socket_ctl[i] != INVALID_SOCKET ) {
				shutdown( pcfg->rxs_socket_ctl[i], SHUT_WR );
				closesocket( pcfg->rxs_socket_ctl[i] );
			}
		}

		// Close control connections. ( Implicitly closes data connections. )
		if ( pcfg->txs_socket_ctl != INVALID_SOCKET ) {
			shutdown( pcfg->txs_socket_ctl, SHUT_WR );
			closesocket( pcfg->txs_socket_ctl );
		}
	}

	if ( num++ == 0 ) {
		fflush( 0 );
		exit( 0 );
	}
} /* end sig_exit */


// 
// Returns: 
//   0 = Normal response processed.
//   1 = Test complete message received.
//  <0 = Error.
int zap_control_process_rx( zap_config_t *config, SOCKET s, zap_history_t *history, zap_performance_frame_t *perf, double* total, double* throughput_array )
{
	static double r[] =		{ 0.0, 0.5, 0.90, 0.95, 0.990, 0.999 };
	const char				*histarr[] = {"0%", 
									      "50%",
						                  "90%",
							              "95%",
							              "99%",
							              "99.9%"};
	zap_performance_frame_t p;
	int						i, j, k;
	unsigned __int32		*src, *dst;
	zap_frame_t				*frame;
	double					throughput;
	int                     len, lendst, lensrc;
	char                    buf[15];
	unsigned __int32		temp;

	if ( zap_read_frame( s, 1, &frame, NULL, NULL ) ){
		return -1;
	}

	src = ( unsigned __int32 * ) &( frame->payload.performance );
	dst = ( unsigned __int32 * ) &p;
	for ( i = 0; i < ( sizeof( p ) / 4 ); i++ ) {
		*dst = ntohl( *src );
		dst++;
		src++;
	}

	if ( ntohl( frame->header.zap_frame_type ) == zap_type_performance_result ) {
		perf->payloads_dropped += p.payloads_dropped;
		perf->payloads_outoforder += p.payloads_outoforder;
		perf->payloads_received += p.payloads_received;
		perf->payloads_repeated += p.payloads_repeated;

		gather_stats( history, p.bits_per_second );

		throughput = ( double )( ( double )p.bits_per_second ) / 1000000.0;

		if((config->average != 0) && (config->average >= p.batch + 1) ) {
			throughput_array[p.batch] =  throughput;
			*total = *total + throughput;
		}
		else if ((config->average != 0) && (config->average < p.batch + 1)) {
			*total = *total + throughput - throughput_array[0];
			for(k = 0; k < config->average - 1; k++){
				//Delete the first element and shitf left one element
				throughput_array[k] = throughput_array[k+1];
			}
			throughput_array[k] = throughput;
		}
		else{ // config->average == 0
			*total = *total + throughput;
		}
		
		if((config->average == 0) || config->average >= p.batch +1 ){
			temp = p.batch + 1;
		}
		else {
			temp = config->average;
		}

		printf( "%5d: %s->%s %6d=rx %3d=dr %3d=oo %3d=rp %5d=rx in %7.1fms  %6.1fmbps  %6.1f | ",
			p.batch,
			inet_ntoa2( config->txs_ip_address ),
			inet_ntoa2( config->rxs_ip_address[0] ),
			perf->payloads_received,
			perf->payloads_dropped,
			perf->payloads_outoforder,
			perf->payloads_repeated,
			p.payloads_received,
			( double )( ( double )( p.last_payload_timestamp - p.first_payload_timestamp ) ) / 1000.0,
			throughput,
			( p.batch > 0 )? ( *total/( temp ) ) : ( *total ) );

		for ( i = 0; i < ( ( sizeof r ) / sizeof( double ) ); i++ ) {
			printf( "%4.1f ", get_stats( history, r[i] ) / 1000000.0 );
		}
		printf( "\n" );
		//Check to make sure packages drop and user wants to log the file
		if (perf->payloads_dropped && config->logfile) {
			//Show message to user
			printf("%d packets drop. Take a look at %s file for mode detail\n", perf->payloads_dropped, config->logfile);
			//Dump to file
			zap_pkg_drop_dump_file(config, perf);
			//Exit
			zap_exit( 1 );
			cleanup_exit( 1 );
		}
		if(config->debugfile && p.batch >= config->start_point && p.batch <= config->end_point){
			zap_debug_dump_file( config, perf, p, throughput, *total , history);
		}
		if ( ( p.batch + 1 ) == config->station_config.batches ) {
			// print for 
			lensrc = ( int )strlen( inet_ntoa2( config->txs_ip_address ) );
			lendst = ( int )strlen( inet_ntoa2( config->rxs_ip_address[0] ) );
			printf( "%5s: ", "#" );
			for ( i=0; i < lensrc - 3; i++ ) {
				printf( " " );
			}
			printf( "src  " );
			for ( i=0; i < lendst - 3; i++ ) {
				printf( " " );
			}
			printf( "dst        rx     dr     oo     rp       rx       b_time    b_thrput     avg | " );
			for ( i = 0; i < ( ( sizeof r ) / sizeof( double ) ); i++ ) {
				sprintf( buf, "%4.1f ", get_stats( history, r[i] ) / 1000000.0 );
				len = ( int )strlen( buf );
				for ( j= 0; j < ( len - ( int )strlen( histarr[i] ) - 1 ); j++ ) {
					printf( " " );
				}
				printf( "%s ", histarr[i] );
			}
			printf( "\n" );

			fprintf( stdout, " Test apparently complete\n" );
			return 1;
		}

		fflush( stdout );

		return 0;
	}

	if ( ntohl( frame->header.zap_frame_type ) == zap_type_test_complete ) {
	    return 1;
	}

	return -1;
}


void dump_stats( zap_history_t *history, FILE *fileio, char delimit )
{
    int					i;
    unsigned __int32	mb;
    int					total;

	if ( !history || !history->data ){
		return;
	}
	i = 0;
	for ( mb = 0; mb < ( history->data[history->gather_count - 1] + 1 ); mb += 1000 ) {
        total = 0;
        while ( history->data[i] < mb ) {
            i++;
            total++;
        }
        fprintf( fileio, "%d%c", total, delimit );
    }
}


unsigned __int32 get_stats( zap_history_t *history, double percentile )
{
    int			 offset;
    
    if ( !history->gather_max ) {
        return 0;
    }

    offset = ( int )( ( double ) history->gather_count * percentile );

	if ( offset >= ( history->gather_count - 1 ) ){
		offset = history->gather_count - 1;
	}

	if ( history->order ) {
		return history->data[offset];
	} else {
		return history->data[history->gather_count - offset - 1];
	}

}

void gather_stats( zap_history_t *history, unsigned __int32 value )
{
    if ( history->gather_count == history->gather_max ) {
        if ( !history->gather_max ) {
            history->gather_max = 512;
        } else {
            history->gather_max *= 2;
        }
        history->data = ( unsigned int * )realloc( history->data, history->gather_max * sizeof( history->data[0] ) );
    }
	history->data[history->gather_count] = value;
	history->gather_count++;
    qsort( history->data, history->gather_count, sizeof( history->data[0] ), compare );
}



int zap_compile_results( zap_config_t *config, zap_history_t *rate_history, zap_performance_frame_t *perf )
{
	fd_set					fd;
	int						n_fd = 0;
	unsigned __int32		i;
	struct timeval			tv;
	SOCKET					s[ZAP_MAX_RECEIVERS + 1];
	unsigned __int32		s_count;
	int						retval = 0;
	unsigned __int32		complete = 0;
	int						result;
    __int64					endtime;   
	int						ttsk = 0;
	double					total= 0;
	unsigned __int32		count = 0;
	double					*throughput_array = NULL;	//Store value of each throughput when Zap receives package.

    endtime = get_current_usecs( ) + 1000000*config->test_seconds;
	memset( perf, 0, sizeof( perf ) );

	s_count = config->rxs_count + 1;

	// Get a nice array of sockets we're concerned with.
	for ( i = 0; i < config->rxs_count; i++ ) {
		s[i] = config->rxs_socket_ctl[i];
	}
	s[i] = config->txs_socket_ctl;

	//Allocate memory for throughput array
	if(config->average != 0) {
		throughput_array = (double *)malloc(config->average * sizeof(double));
		if(throughput_array == NULL) {
			exit_error("Can not allocate memory for throughput array\n");
		}
	}

	while ( !complete && ( get_current_usecs() - endtime < 0 ) ) {
		// Setup select.
		FD_ZERO( &fd );
		n_fd = 0;
		for ( i = 0; i < s_count; i++ )  {
			FD_SET( s[i], &fd );
			N_UPDATE( n_fd, s[i] );
		}
		tv.tv_sec = ( long )( config->results_timeout / 1000000 );
		tv.tv_usec = ( long )( config->results_timeout % 1000000 );

		// Wait for data.
		ttsk = select( n_fd+1, &fd, NULL, NULL, &tv );
		if ( ttsk == 0 ) {
		// Timeout
#ifdef WIN32 // Operation now in progress, so waiting for receiving data. 
			if(	WSAGetLastError() != 0 || count >= 2) {
#else
			if(errno != EINPROGRESS || count >= 2)  {
#endif
				WARN_errno( 1, "zap_compile_results: Time out");
				zap_log_error(config, "zap_compile_results: Time out", ERROR);
				complete = 1;
				retval = 1;
			}
			else {
				count++;
				printf("\nConnection is slow,waiting for receiving data\n");
				zap_log_error(config, "Connection is slow,waiting for receiving data", ERROR);
			}
		} else {
			for ( i = 0; i < s_count; i++ ) {
				if ( FD_ISSET( s[i], &fd ) ) {
					result = zap_control_process_rx( config, s[i], rate_history, perf, &total, throughput_array );
					if ( result < 0 ) {
						zap_exit( 1 );
						cleanup_exit( 1 );
					}
					if ( result > 0 ) {
						// Got a complete message- ignore this 
						unsigned int j;
						for ( j = i; j < ( s_count - 1 ); j++ ) {
							s[j] = s[j + 1];
						}
						s_count--;
						printf( "scount--\n" );
						if ( s_count == 1 ) { // Only the tx channel left...
						    printf( "complete\n" );
							complete = 1;
							retval = 0;
							break;
						} else {
							i--;
						}
					}
				}
			}
		}
	}

	// Dump results to file.
	if ( config->filename ) {
		if ( zap_control_dump_file( config, perf, rate_history, total ) ) {
			exit_error( "Could not output results\n" );
		}
	}
	// Deallocate memory of throughput_array
	if(throughput_array != NULL) {
		free(throughput_array);
	}
	if(config->debugfile != NULL){
		free(config->debugfile);
	}

	return retval;
}


//
// zap_controller - Controls zap servers to send data 'round.
//
// returns : 0 on success, non-zero on error.
//
int 
zap_controller( zap_config_t *config )
{
	unsigned __int32		i;
	zap_history_t			rate_history = {NULL, 0, 0, 0};
	zap_performance_frame_t perf;

	memset( &perf, 0, sizeof( perf ) );

	// Select test ID
	config->tid = zap_generate_tid(  );


	// Open control connection to transmitter.
	if ( zap_socket( config->station_config.buf_required, 1, &config->txs_socket_ctl ) ) {
		errOut( "Could not allocate tx station socket.\n" );
		zap_log_error(config, "Could not allocate tx station socket.", ERROR);
		cleanup_exit( 1 );
	}

	if ( zap_connect( config->txs_ip_address_ctl, 1, config->txs_socket_ctl, 5000000 ) ) {
		zap_log_error(config, "Could not connect to tx station.", ERROR);
		errOut( "Could not connect to tx station %s.\n", inet_ntoa2( config->txs_ip_address_ctl ) );
		cleanup_exit( 1 );
	}

	// Open control connection to receivers.
	for ( i = 0; i < config->rxs_count; i++ ) {
		if ( zap_socket( config->station_config.buf_required, 1, &config->rxs_socket_ctl[i] ) ) {
			zap_log_error(config, "Could not allocate rx station socket.", ERROR);
			exit_error( "Could not allocate rx station socket.\n" );
		}
		if ( zap_connect( config->rxs_ip_address_ctl[i], 1, config->rxs_socket_ctl[i], 5000000 ) ) {
			zap_log_error(config, "Could not connect to rx station.", ERROR);
			exit_error( "Could not connect to rx station.\n" );
		}
	}

	// Configure Receivers.
	config->station_config.tx = 0;
	config->station_config.ip_tos = config->ip_tos;
	for ( i = 0; i < config->rxs_count; i++ ) {
		if ( zap_config( config->tid, config->rxs_socket_ctl[i], &config->station_config ) ) {
			zap_log_error(config, "Could not configure rx station. Possible version incompatibility.", ERROR);
			exit_error( "Could not configure rx station. Possible version incompatibility.\n" );
		}
	}

	// configure Transmitter.
	config->station_config.tx = 1;
	if ( zap_config( config->tid, config->txs_socket_ctl, &config->station_config ) ) {
		zap_log_error(config, "Could not configure tx station. Possible version incompatibility.", ERROR);
		exit_error( "Could not configure tx station. Possible version incompatibility.\n" );
	}

	// Open data connections in the appropriate directions.
	for ( i = 0; i < config->rxs_count; i++ ) {
		if ( config->open_reverse ) {
		    printf( "reverse\n" );
			if ( zap_data_connect( config->tid, config->rxs_socket_ctl[i], config->txs_ip_address ) ) {
				zap_log_error(config, "Could not connect rx to tx station via TCP.", ERROR);
				exit_error( "Could not connect rx to tx station via TCP\n" );
			}
		} else {
			if ( zap_data_connect( config->tid, config->txs_socket_ctl, config->rxs_ip_address[i] ) ) {
				zap_log_error(config, "Could not connect rx to tx station via TCP.", ERROR);
				exit_error( "Could not connect tx to rx station via TCP\n" );
			}
		}
	}

	// Indicate the test should start.
	if ( zap_test_start( config->tid, config->txs_socket_ctl ) ) {
		zap_log_error(config, "Could not start test.", ERROR);
		exit_error( "Could not start test\n" );
	}
	
	// Listen/wait for responses. ( Any TCP connection closing aborts test!!! )
	if ( zap_compile_results( config, &rate_history, &perf ) ) {
		zap_log_error(config, "Could not get responses.", ERROR);
		exit_error( "Could not get responses \n" );
	}

	for ( i = 0; i < config->rxs_count; i++ ) {
		zap_test_complete( config->tid, config->rxs_socket_ctl[i] );
		shutdown( config->rxs_socket_ctl[i], SHUT_WR );

#ifdef WIN32
		Sleep( SLEEP_TIME );
#else
		usleep( SLEEP_TIME );
#endif

		closesocket( config->rxs_socket_ctl[i] );
	}

	// Close control connections. ( Implicitly closes data connections. )
	zap_test_complete( config->tid, config->txs_socket_ctl );
	shutdown( config->txs_socket_ctl, SHUT_WR );

#ifdef WIN32
	Sleep( SLEEP_TIME );
#else
	usleep( SLEEP_TIME );
#endif

	closesocket( config->txs_socket_ctl );

	return 0;
}

//
// Parse the command-line arguments for zap.
//
int zap_parse_args( int argc, char *argv[], zap_config_t *config )
{
	int							i, j, k;
    int                         len;
    char						*found;
	unsigned __int32			number=0;
	unsigned __int32			ip_d;
	unsigned __int32			ip_c;
	unsigned __int32			value;
	unsigned __int32			stop_value;
	unsigned __int32			bit_rate = 0;
	unsigned __int32			frames = 0;
	float						fl;
	int							reverse = 0;
	FILE			            *fileio;
	int							payload_length_flag = 0;
	int							batch_time_flag = 0;

	memset( config, 0, sizeof ( *config ) );
	config->tag = "";
	config->sub = "";
	config->note= "";

//	// DEFAULTS!
	//
	// The default is designed to do the equivalent of the old "zap x.x.x.x --client"
	//
	// In this case, there is only one IP address, and that is the IP address of the transmitter. The 
	// receiver IP address will be 127.0.0.1 in this case. If the receiver IP address is 127.0.0.1, we
	// really want to open the connection in reverse, since the transmitter won't really be able to open
	// a connection to 127.0.0.1 and expect it to get where we really want it to go. It's like testing
	// NAT by default, which is kinda cute.
	//
	config->station_config.asynchronous			= 1;		// At most one batch outstanding at a time.
	config->station_config.batch_completion		= 1;		// Yes, send frames to acknowledge completion.
	config->station_config.batch_inband_response= 0;		// No inband response required. ( i.e. not echo service )
	config->station_config.batch_report_rate = 1;			// 1 report per batch.
	config->station_config.batch_size = 50;					// 50 payloads per batch. This option is mutually exclusive from batch_time value
	config->station_config.batch_time = 50000;				// Run batch with 50000 milisecond, don't care how many payloads on a batch
															// This option is mutually exclusive from batch_size value
	config->station_config.batch_timeout = 1000000;			// 1 second timeout per batch.
	config->station_config.batch_transmit_delay = 1;		// No delay between batches. Well, 1 usec, but effectively zero.
	config->station_config.batches = 1000;					// 1000 batches.
	config->station_config.payload_length = 1472;			// Max size UDP frame.
	config->station_config.payload_timeout = 100000;		// 1/10 sec
	config->station_config.payload_transmit_delay = 1;		// No delay between payloads. Well, 1 usec, but effectively zero.
	config->station_config.tcp = 0;							// Use UDP by default (TCP support is deprecated).
	config->station_config.tx_ip = 0;						// Learn the IP address to which to transmit UDP frames.

	config->open_reverse = 0;
	config->results_timeout = 60 * 1000000;					// 60 second timeout waiting for results.

	config->txs_ip_address = 0;
	config->txs_ip_address_ctl = 0;

	config->rxs_count = 0;
	config->server = 0;
    config->test_seconds = 30000; // big enough to not matter
	config->debugfile = NULL;

	for ( i = 1; i < argc; i++ ) {
		if ( argv[i][0] == '-' ) {
			switch ( argv[i][1] ) {
				case 's':
				case 'd':
				case 'm':
					for ( j = 2; j < ( int )strlen( argv[i] ); j++ ) {
						if ( argv[i][j] == ',' ) {
							argv[i][j] = ' ';
						}
					}
					// Parse 1 or 2 IP addys.
					ip_d = 0;
					ip_c = 0;
					ip_d = inet_addr( &argv[i][2] );
					if ( ip_d ) {
						for ( j = 2; j < ( int )strlen( argv[i] ); j++ ) {
							if ( argv[i][j] == ' ' ) {
								ip_c = inet_addr( &argv[i][j+1] );
								break;
							}
						}
					}
					break;
				case 'l':
				case 'a':
				case 'o':
				case 'f':
				case 'X':
				case 'n':
				case 'p':
				case 'w':
					if ( sscanf( &argv[i][2], "%d", &value ) != 1 ) {
						// Bad scan..
						return 1;
					}
					break;
				case 'D':
					for ( j = 2; j < ( int )strlen( argv[i] ); j++ ) {
						if ( argv[i][j] == ',' ) {
							argv[i][j] = ' ';
						}
					}
					/*Get debug file name*/
					for ( j = 0; j < ( int )strlen( argv[i] ); j++ ) {
						if ( argv[i][j] == ' ' ) {
							config->debugfile = (char*)malloc(j * sizeof(char));
							strncpy(config->debugfile, argv[i] + 2, j-2);
							config->debugfile[j-2] = '\0';
							break;
						}
					}
					/*Get the start point*/
					for ( k = j+1; k < ( int )strlen( argv[i] ); k++ ) {
						if ( argv[i][k] == ' ' ) {
							char temp[10];
							strncpy(temp, argv[i]+j, k-j);
							value = atoi(temp);
						}
					}
					/*Get end point*/
					for ( k = j+1; k < ( int )strlen( argv[i] ); k++ ) {
						if ( argv[i][k] == ' ' ) {
							if ( sscanf( &argv[i][k+1], "%d", &stop_value ) != 1 ) {
								// Bad scan..
								return 1;
							}
						}
					}
					break;
			case 'q':
			
			    // For those options with numeric args, grab the numeric arg.
			    found = 0;
			    number = strtoul( &( argv[i][2] ), &found, 0 );
				if ( !found ) {
                    return 1;
				}
			    break;

				case 'r':
					if ( sscanf( &argv[i][2], "%f", &fl ) != 1 ) {
						// Bad scan..
						return 1;
					}
					if ( fl <= 0 ) {
						return 1; 
					}
					fl *= 1000000.0;
					value = ( unsigned __int32 ) fl;
					break;
				default:
					break;
			}
			switch ( argv[i][1] ) {
				case 's':			// Source IP address
					if ( config->txs_ip_address ) {
						// return 1;		// Allow re-specifying IP addy on command line.
					}
					if ( ip_d ) {
						config->txs_ip_address = ip_d;
						if ( ip_c ) {
							config->txs_ip_address_ctl = ip_c;
						} else {
							config->txs_ip_address_ctl = ip_d;
						}
					} else {
						return 1;
					}
					break;
				case 'd':			// Destionation IP address
					if ( config->rxs_count >= ZAP_MAX_RECEIVERS ) {
						return 1;
					}
					if ( ip_d ) {
						config->rxs_ip_address[config->rxs_count] = ip_d;
						if ( ip_c ) {
							config->rxs_ip_address_ctl[config->rxs_count] = ip_c;
						} else {
							config->rxs_ip_address_ctl[config->rxs_count] = ip_d;
						}
						config->rxs_count++;
					} else {
						return 1;
					}
					break;
				case 'i':			// Invert connection opening.
					config->open_reverse = 1;
					break;
				case 'l':			// Payload length
					if ( value > 65527 ) {
						return 1;
					}
					if ( value < ( sizeof( zap_header_t ) + sizeof( zap_data_frame_t ) ) ) {
						value = sizeof( zap_header_t ) + sizeof( zap_data_frame_t );
					}
					config->station_config.payload_length = value;
					payload_length_flag = 1;
					break;
				case 'f':
					frames = value;
					break;
#ifndef NO_MTUDISC
				case 'M':			// Path MTU Discovery
					pmtudisc = !strcmp(&argv[i][2], "dont") ? IP_PMTUDISC_DONT :
					           !strcmp(&argv[i][2], "want") ? IP_PMTUDISC_WANT :
					           !strcmp(&argv[i][2], "do") ? IP_PMTUDISC_DO : -1;
					break;
#endif // NO_MTUDISC
				case 'a':			// Batch size- Average size- Sample size
					if ( value == 0 ) {
						value = 1;
					}
					config->station_config.batch_size = value;
					if (batch_time_flag == 0){
						// Disable -p option
						config->station_config.batch_time = 0;
					}
					break;
				case 'p':
					config->station_config.batch_time = value;
					batch_time_flag = 1;
					break;
				case 'q':
					  config->ip_tos = number;
					 break;
				case 'o':			// Max batches outstanding.
					if ( value == 0 ) {
						value = 1;
					}
					config->station_config.asynchronous = value;
					break;
				case 'm':			// Multicast IP
					break;
				case 'r':			// Data rate.
					bit_rate = value;
					break;
				case 'F':
					config->filename = &argv[i][2];
					break;
				case 'L':
					config->logfile = &argv[i][2];
					break;
				case 'D':
					if (stop_value < value) {
						printf("Invalid value. Stop point always more than start point!\n");
						return 1;
					}
					if ((value == 0) && (stop_value == 0) ) {
						//Log all results.
						value = 0;
						stop_value = 0xFFFFFFFF;
					}
					config->start_point = value;
					config->end_point = stop_value;
					break;
				case 'w':
					config->average = value;
					break;
				case 'T':
					config->tag = &argv[i][2];
					break;
				case 'S':
					config->sub = &argv[i][2];
					break;
				case 'N':
					config->note = &argv[i][2];
					break;
				case 'X':
					config->test_seconds = value;
                    printf( "Test will terminate in approximately %d seconds.\n", value );
                    config->station_config.batches = 1000000; // override batches
					break;
				case 'n':
					config->station_config.batches = value;
					break;
				case 'R':
					reverse = 1;
					break;
                case '-':
                    if ( argv[i][2] == 0 ) {
                        fprintf( stderr, "Error: Expecting more than just --\n" );
                        return 1;
                    }
                    len = ( int ) strlen( &argv[i][2] );
                    if ( strncmp( &argv[i][2], "server", len ) == 0 ) {
						config->server = 1;
                    } else {
                        fprintf( stderr, "Error: unexpected -- option received\n" );
                        return 1;
                    }
                    break;
				default:
					// Error.
					return 1;
			}
		}
	}

	// Check args.
	if ( !config->txs_ip_address_ctl ) {
		fprintf( stderr, "Error- Expecting at least one IP address with which to play\n" );
		return 1;
	}
	if ( !config->txs_ip_address ) {
		config->txs_ip_address = config->txs_ip_address_ctl;
	}

	if ( !config->rxs_count ) {
		config->rxs_count = 1;
		config->rxs_ip_address_ctl[0] = inet_addr( "127.0.0.1" );
		config->open_reverse = 1;
	}

	for ( i = 0; i < ( int ) config->rxs_count; i++ ) {
		if ( !config->rxs_ip_address_ctl[i] ) {
			fprintf( stderr, "Error- no rx_ctl IP address. Weird\n" );
			return 1;
		}
		if ( !config->rxs_ip_address[i] ) {
			config->rxs_ip_address[i] = config->rxs_ip_address_ctl[i];
		}
	}
	if ( config->station_config.tcp ) {
		if ( frames ) {
			config->station_config.payload_length *=frames;
		}
		if ( config->station_config.payload_length > 65535 ) {
			fprintf( stderr, "Error- cannot send length %d\n", config->station_config.payload_length );
			return 1;
		}
	} else {
		if(frames) {
			frames--;
			config->station_config.payload_length += /*1480*/config->station_config.payload_length*frames;
		}
		if ( config->station_config.payload_length > 65527 ) {
			fprintf( stderr, "Error- cannot send length %d\n", config->station_config.payload_length );
			return 1;
		}
	}	

	// Check if the bit rate must be set.
	if ( bit_rate ) {
		double bps, bpp, usecs_p;

		bps = ( double )bit_rate;
		bpp = ( double )config->station_config.payload_length * 8;

		usecs_p = ( 1000000.0 / bit_rate ) * bpp;
		config->station_config.payload_transmit_delay = ( unsigned __int32 ) usecs_p;
		if ( config->station_config.payload_transmit_delay == 0 ) {
			config->station_config.payload_transmit_delay = 1;
		}

		//bit_rate
	}

	if ( reverse ) {
		if ( config->rxs_count != 1 ) {
			fprintf( stderr, "Error- can only reverse source/destination for unicast\n" );
			return 1;
		}
		ip_d = config->rxs_ip_address[0];
		ip_c = config->rxs_ip_address_ctl[0];
		config->rxs_ip_address[0] = config->txs_ip_address;
		config->rxs_ip_address_ctl[0] = config->txs_ip_address_ctl;
		config->txs_ip_address = ip_d;
		config->txs_ip_address_ctl = ip_c;
	}

	config->station_config.buf_required = config->station_config.batch_size * config->station_config.payload_length * config->station_config.asynchronous;
	//Print the current engaging information, it includes -p, -n, -l, -q option
	fprintf(stdout, "Engaging default options -p%d -n%d -l%d -q0x%x\n", config->station_config.batch_time, 
		config->station_config.batches, config->station_config.payload_length, config->ip_tos);
#ifndef NO_MTUDISC
	if (pmtudisc != -1) {
		fprintf(stdout, "Engaging explicit option %s\n", ip_pmtudisc_str(pmtudisc));
	}
#endif // NO_MTUDISC
	fprintf(stdout, "\n");

	//Dump this information to log file in the case we set -D option
	if(config->debugfile){
		fileio = fopen( config->debugfile, "a+" );
		fprintf(fileio, "Engaging default options -p%d -n%d -l%d -q0x%x\n", config->station_config.batch_time, 
			config->station_config.batches, config->station_config.payload_length, config->ip_tos);
#ifndef NO_MTUDISC
		if (pmtudisc != -1) {
			fprintf(fileio, "Engaging explicit option %s\n", ip_pmtudisc_str(pmtudisc));
		}
#endif // NO_MTUDISC
		fprintf(fileio, "\n");
		fclose(fileio);
	}
	return 0;
}


int zap( int argc, char *argv[] )
{
	zap_config_t			config;
	
    fprintf( stdout, "%s version %d.%d, Copyright ( C ) 2004-2009 Ruckus Wireless, Inc. All Rights Reserved.\n",
		argv[0],
        ZAP_MAJOR_VERSION,
        ZAP_MINOR_VERSION );
	fprintf( stdout, "Built %s at %s\n", __DATE__, __TIME__ );

	memset( ( char * )&config, 0, sizeof( config ) );

	if ( zap_parse_args( argc, argv, &config ) ) {
		fprintf( stderr, "%s [options] -s<IP_D>[,<IP_C>] [options]\n", argv[0] );
		fprintf( stderr, "   Zap has the following options: NOTE: options are CaSe sensitive!\n" );
		fprintf( stderr, "     -s<IP_D>[,<IP_C>]  - Specify the IP address of the source station. Traffic will be sent FROM this station.\n" );
		fprintf( stderr, "                          <IP_D> The IP address the station will use for data communication.\n" );
		fprintf( stderr, "                          <IP_C> The ( optional ) IP address the controller will use to communicate with the station.\n" );
		fprintf( stderr, "                          This option is required.\n" );
		fprintf( stderr, "     -d<IP_D>[,<IP_C>]  - Specify the IP address of the destination station. Traffic will be sent TO this station.\n" );
		fprintf( stderr, "                          <IP_D> The IP address the station will use for data communication.\n" );
		fprintf( stderr, "                          <IP_C> The ( optional ) IP address the controller will use to communicate with the station.\n" );
		fprintf( stderr, "                          This value defaults to the local station. Multiple may be specified for multicast.\n" );
		fprintf( stderr, "     -i                 - Invert Connection Opening - Opens initial connection from destination to source.\n" );
		fprintf( stderr, "                          Used to alieviate testing issues with NAT devices. Defaults off.\n" );
		fprintf( stderr, "     -l<length>         - Specifies a payload length of <length>. The largest payload in a single packet is:\n" );
		fprintf( stderr, "                          UDP: 1472 bytes.\n" );
		fprintf( stderr, "     -f<frames>         - Specifies how many full-length frames you want to use for framelength. Takes\n" );
		fprintf( stderr, "                          fragmentation into account. Completely overrides -l\n" );
#ifndef NO_MTUDISC
		fprintf( stderr, "     -M<pmtudisc hint>  - Specifies Path MTU Discovery (PMTUDISC) strategy. Options are: \n");
		fprintf( stderr, "                          do (always DF); want (do and fragment locally); dont (never DF).\n");
#endif // NO_MTUDISC
		fprintf( stderr, "     -a<sample size>    - <sample size> is the number of payloads sampled for performance measurement.\n" );
		fprintf( stderr, "                          This is the near-equivalent of zing's -E option. Defaults to 50.\n" );
		fprintf( stderr, "                          This option is mutually exclusive from -p option.\n" );
		fprintf( stderr, "     -p<period>         - Each sample lasts the specified ( in microseconds ) period.\n" );
		fprintf( stderr, "                          Successfully crosses sample boundaries.\n" );
		fprintf( stderr, "                          This option is mutually exclusive from -a option.\n" );
		fprintf( stderr, "     -q#                - Send frames with the specified 8 bit DSCP ( TOS bits ).\n" );
		fprintf( stderr, "                          NOTE: The frames coming back will NOT have these bits set, necessarily.\n" );
		fprintf( stderr, "                          On WinXP, the following registry key must be REG_DWORD,\n" );
		fprintf( stderr, "                          and ZERO to enable TOS bit setting: Local Machine\\System\\CurrentControlSet\\\n" );
		fprintf( stderr, "                          Services\\Tcpip\\Parameters\\DisableUserTOSSetting\n" );
		fprintf( stderr, "                          Common fields for Lowest  #: 0x08, 0x20: Background\n" );
		fprintf( stderr, "                          Common fields for Low     #: 0x00, 0x18: Best Effort\n" );
		fprintf( stderr, "                          Common fields for High    #: 0x28, 0xa0: Video\n" );
		fprintf( stderr, "                          Common fields for Highest #: 0x30, 0xe0: Voice\n" );
		fprintf( stderr, "                          Common fields for Unknown #: 0x88:       Video 2?\n" );
		fprintf( stderr, "     -n<number samples> - Specifies the number of samples in the test. Defaults to 1000.\n" );
		fprintf( stderr, "     -o<outstanding>    - Specifies the number of samples that may be outstanding ( in the air )\n" );
		fprintf( stderr, "                          at any given time. Defaults to 1.\n" );
		fprintf( stderr, "     -m<IP_M>           - Specify the destination multicast address to use for data communication.\n" );
		fprintf( stderr, "                          Defaults to none. Setting this enables multicast transmission via UDP.\n" );
		fprintf( stderr, "     -r<mbps>           - Controls the rate in mbits/s for transmitting data. Decimal values accepted\n" );
		fprintf( stderr, "                          Defaults to a very high data rate.\n" );
		fprintf( stderr, "     -R                 - Reverses the direction ( swaps source and destination ). Only works with unicast\n" );
		fprintf( stderr, "     -F<filename>       - Dump results ( appended ) to a comma seperated value file <filename>\n" );
		fprintf( stderr, "     -L<logfilename>    - Dump the log file and stop zap tool when some packet drop\n" );
		fprintf( stderr, "     -D<file><,start,stop>\n" );
		fprintf( stderr, "                        - Dump result to file for debugging\n" );
		fprintf( stderr, "                          Instruction: -Dfilename,start_point,stop_point.\n" );
		fprintf( stderr, "                          In order to dump all log file, using -Dfilname,0,0\n" );
		fprintf( stderr, "     -w<value>          - Get the average value of throughput\n");		
		fprintf( stderr, "     -T<tag>            - Tag used to describe this test case within the dumped file, required with -F\n" );
		fprintf( stderr, "     -S<sub>            - Sub tag used to describe this test case within the dumped file.\n" );
		fprintf( stderr, "     -N<note>           - Note used to describe this test case within the dumped file.\n" );
		fprintf( stderr, "     -X<sec>            - Test for specified number of seconds.\n" );
		fprintf( stderr, "     --server           - Runs zap in server mode. No other arguments required.\n" );

		return 1;
	} 

	pcfg = &config;
	return zap_controller( &config );
}


int main( int argc, char* argv[] )
{
	int			err;

	pcfg = NULL;
	signal( SIGTERM, zap_exit );
	signal( SIGINT, zap_exit );
#ifndef WIN32
    // Ignore broken pipes
    signal(SIGPIPE,SIG_IGN);
#endif

	net_init(  );

	err = zap( argc, argv );
	cleanup_exit( err );
}
