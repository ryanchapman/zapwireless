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

#include "../zaplib/zaplib.h"

zap_server_t *pServer;

void zap_server_tx( zap_server_t *server, fd_set *pfd)
{
	unsigned __int32		i;
	zap_station_t			*station;
	unsigned __int64		current_usec = 0;
	unsigned __int64		diff_usec = 0;
	unsigned __int64		tx_packets = 0, tx_packets_valid = 0;
	unsigned __int32		end_batch = 0;
	unsigned __int32		clean_station = 0;
	unsigned __int32		max_packets = 0x0fffffff;

	current_usec = get_current_usecs(  );
	for ( i = 0; i < ZAP_MAX_STATIONS; i++ ) {
		if ( server->stations[i].state == zap_station_state_running_tx ) {
			clean_station = 0;	// If this becomes non-zero, we encountered an unrecoverable error and should close the station.
			// for each transmitting station...
			station = &server->stations[i];
			if ( !station->payload_usec ) {
				station->payload_usec = current_usec;
			}

			// Calculate the amount of time since we last addressed this station.
			diff_usec = current_usec - station->payload_usec;
			// Calculate the number of packets we can transmit, purely based on our rate.
			tx_packets = diff_usec / station->config.payload_transmit_delay;
			if(tx_packets < 0) {
				printf("\n tx_packets has negative value!!!!!!!!! %llu\n", tx_packets);
			}
			// So long as we have packets to transmit...
			if ( (tx_packets > 0) &&																			// Payloads to transmit and...
				( ( station->batch_num < station->config.batches ) || ( station->config.batch_time ) ) &&		// Batches to transmit or timed run and...
				( ( station->batch_num - station->last_completed_batch ) <= station->config.asynchronous ) ) {	// We don't violate asynchronous count.
				// We may have packets to transmit. Now we need to watch for batch boundaries, asynchronous
				// packet transmission limits, and the like.
				// Grow tx_packets_valid as we verify we can transmit frames.
				
				// Check how many we can still send in this batch...
				tx_packets_valid = station->config.batch_size - station->payload_num;
				if ( tx_packets_valid > tx_packets ) {
					tx_packets_valid = tx_packets;
					tx_packets = 0;
					end_batch = station->batch_num;
				} else {
					end_batch = station->batch_num + 1;
				}
				// We are now at a batch boundary. Must see if we can do more than complete this batch.
				while ( tx_packets ) {
					if ( ( end_batch - station->last_completed_batch) > station->config.asynchronous ) {
						// Transmitting into another batch would break asynchronous semantics.
						break;
					}
					if ( ( end_batch ) >= station->config.batches ) {
						// Stop transmitting at last batch.
						break;
					}
					if ( station->config.batch_size > tx_packets ) {
						tx_packets_valid += tx_packets;
						tx_packets = 0;
					} else {
						tx_packets_valid += station->config.batch_size;
						tx_packets -= station->config.batch_size;
					}
					end_batch++;
				}

				// Now tx_packets_valid contains the number of payloads to transmit. Transmit them! ( Hope we don't block!! )				
				while ( (tx_packets_valid > 0) && ( !clean_station ) ) {
					if ( station->config.tcp ) {
						if ( station->s_tcp_count > 1 ) {
							erk;
							clean_station = 1;
						} else {
							if ( zap_send_data( station->config.tcp, station->id, station->batch_num, station->payload_num, station->s_tcp[0], 0, station->config.payload_length ) ) {
								clean_station = 1;
							}
						}
					} else {
						if ( zap_send_data( station->config.tcp, station->id, station->batch_num, station->payload_num, server->udp_socket_tx, station->config.tx_ip, station->config.payload_length ) ) {
							clean_station = 1;
						}
					}

					station->payload_usec += station->config.payload_transmit_delay;	// Estimated departure time.

					tx_packets_valid--;
					station->payload_num ++;
					if ( station->payload_num >= station->config.batch_size ) {
					// go to a new batch!!
						if ( zap_send_data_complete( station ) ) {
							clean_station = 1;
						}
							station->payload_num = 0;
							station->batch_num ++;
							//Sleep( 1000 );  XXX Can be useful for debugging.
						//}
					}

					max_packets--;
					if ( !max_packets ) {
						break;
					}
				}

			}

			// Figure out how long we may need to wait for next transmission.
			if ( tx_packets ) {
				// something kept us from transmitting, thus we're blocked.
				station->blocked = 1;
			} else {
				station->blocked = 0;				
				station->next_event = station->config.payload_transmit_delay - ( ( unsigned __int32 )( current_usec - station->payload_usec ) );
			}

			// If some error occurred, close/reset station.
			if ( clean_station ) {
				zap_clean_station( station, pfd);
			}
		}
	}
}

void zap_server_rx( zap_server_t *server, fd_set *pfd)
{
	int					fd_count;
	struct timeval		tv;
	int					result;
	__int64				current_usec;
	__int64				usec_delay = 100000;
	__int64				delta;
	unsigned __int32	i, j;
	zap_station_t		*station, 
						*stationcleaned= NULL;
	int                 rv;

	FD_ZERO( pfd );
	fd_count = 0;

	FD_SET( server->tcp_socket, pfd );
	N_UPDATE( fd_count, server->tcp_socket );
	FD_SET( server->udp_socket_rx, pfd );
	N_UPDATE( fd_count, server->udp_socket_rx );

	current_usec = get_current_usecs(  );

	for ( i = 0; i < ZAP_MAX_STATIONS; i++ ) {
		station = &( server->stations[i] );

		// Find the next transmit event...
		switch( station->state ) {
			case zap_station_state_running_tx:
				if ( !station->blocked ) {
					if ( station->next_event < usec_delay ) {
						usec_delay = station->next_event;
					}
				}
				//usec_delay = 0;
				break;
			case zap_station_state_running_rx:
				delta = current_usec - station->payload_usec;
				if ( station->payload_usec ) {
					if ( delta > station->config.payload_timeout )  {
						fprintf( stderr, "Station payload timeout\n" );
						zap_clean_station( station, pfd);
					} else {
						if ( ( station->config.payload_timeout - delta ) < usec_delay ) {
							usec_delay = station->config.payload_timeout - delta;
						}
					}
				}
				break;
			default:
				break;
		}

		// Add sockets...
		if ( station->s_control != INVALID_SOCKET ) {
			FD_SET( station->s_control, pfd );
			N_UPDATE( fd_count, station->s_control );
		}
		for ( j = 0; j < station->s_tcp_count; j++ ) {
			if ( station->s_tcp[j] != INVALID_SOCKET ) {
				FD_SET( station->s_tcp[j], pfd );
				N_UPDATE( fd_count, station->s_tcp[j] );
			}
		}
	}

	tv.tv_sec = ( long )( usec_delay / 1000000 );
	tv.tv_usec = ( long )( usec_delay % 1000000 );
	result = select( fd_count+1, pfd, NULL, NULL, &tv );
	if(result) {
		// receive data...
		if ( FD_ISSET( server->tcp_socket, pfd ) ) {
			stationcleaned = NULL;
			rv = zap_accept( server, server->tcp_socket, stationcleaned );
			if ( ( stationcleaned != NULL ) && rv ){
				zap_clean_station( stationcleaned, pfd );
			}
		}
		if ( FD_ISSET( server->udp_socket_rx, pfd ) ) {
			stationcleaned = NULL;
			rv = zap_rx_data( server, server->udp_socket_rx, 0, pfd, stationcleaned );
			if ( ( stationcleaned != NULL ) && rv ) {
				zap_clean_station( stationcleaned, pfd );
			}
		}

		for ( i = 0; i < ZAP_MAX_STATIONS; i++ ) {
			station = &( server->stations[i] );
			stationcleaned = station;

			if ( station->s_control != INVALID_SOCKET ) {
				if ( FD_ISSET( station->s_control, pfd ) ) {
					if ( zap_rx_data( server, station->s_control, 1, pfd, stationcleaned ) ) {
						zap_clean_station( stationcleaned, pfd );
					}
				}
			}
			for ( j = 0; j < station->s_tcp_count; j++ ) {
				if ( station->s_tcp[j] != INVALID_SOCKET ) {
					if ( FD_ISSET( station->s_tcp[j], pfd ) ) {
						if( zap_rx_data( server, station->s_tcp[j], 1, pfd, stationcleaned ) ) {
							zap_clean_station( stationcleaned, pfd );
						}
					}
				}
			}
		}
	}
}

void ParseCommandLine( int argc, const char **argv)
{
	int			i;

#ifndef NO_MTUDISC
	for ( i = 1; i < argc; i++ ) {
		if ( argv[i][0] == '-' ) {
			switch ( argv[i][1] ) {
				case 'M':			// Path MTU Discovery
					pmtudisc = !strcmp(&argv[i][2], "dont") ? IP_PMTUDISC_DONT :
					           !strcmp(&argv[i][2], "want") ? IP_PMTUDISC_WANT :
					           !strcmp(&argv[i][2], "do") ? IP_PMTUDISC_DO : -1;
					break;
				default:
					break;
			}
		}
	}
#else
	char		 buf[LOG_MESSAGE_OUTPUT_BUFFER_SIZE];

	for ( i= argc - 1; i >= 1; i-- ) {
        strcpy( buf, argv[i] );
	}
#endif // NO_MTUDISC
}

/* -------------------------------------------------------------------
* zapd_exit
*
* Quietly exits. 
* ------------------------------------------------------------------- */

void zapd_exit( int inSigno )
{
	static int	num = 0;
	int			i, j;
	int			rlen;
	char		buf[1000];
	
	memset(buf, 'a', sizeof(buf));
	for ( i = 0; i < ZAP_MAX_STATIONS; i++ ) {
		zap_station_t *pStation = &( pServer->stations[i] );

		for ( j = 0; j < ZAP_MAX_RECEIVERS; j++ ) {
			if ( pStation->s_tcp[j] != INVALID_SOCKET ) {
				shutdown( pStation->s_tcp[j], SHUT_RDWR );
#ifdef WIN32
				Sleep( SLEEP_TIME );
#else
				usleep( SLEEP_TIME );
#endif
				closesocket( pStation->s_tcp[j] );
			}
		}

		if ( pStation->s_control != INVALID_SOCKET ) {
			shutdown( pStation->s_control, SHUT_RDWR );
#ifdef WIN32
			Sleep( SLEEP_TIME );
#else
			usleep( SLEEP_TIME );
#endif
			closesocket( pStation->s_control );
		}
	}
	if ( num++ == 0 ) {
		fflush( 0 );
		exit( 0 );
	}
} /* end zapd_exit */


int main( int argc, const char* argv[] )
{
	zap_server_t		server;
	fd_set				fd;
	int					i, j;
	extern char         currPath;
	int					bOptVal;
	int					bOptLen = sizeof( int );

	ParseCommandLine( argc, argv );

	InitLog(  );
	// Remove the log file if it is existing
#ifdef WIN32
		_unlink( &currPath );
#else
		unlink( &currPath );
#endif

	pServer = &server;
	// init value for log variables
    printf("\n%s version %d.%d, Copyright (C) 2004-2009 Ruckus Wireless, Inc.\n",
		argv[0],
        ZAP_MAJOR_VERSION,
        ZAP_MINOR_VERSION );
	printf("Built %s at %s\n", __DATE__, __TIME__ );

	signal( SIGTERM, zapd_exit );
	signal( SIGINT, zapd_exit );
#ifndef WIN32
    // Ignore broken pipes
    signal(SIGPIPE,SIG_IGN);
#endif

	net_init(  );

	memset( &server, 0, sizeof( server ) );
	for ( i = 0; i < ZAP_MAX_STATIONS; i++ ) {
		server.stations[i].s_control = INVALID_SOCKET;
		for ( j= 0; j < ZAP_MAX_RECEIVERS; j++ ) {
			server.stations[i].s_tcp[j]= INVALID_SOCKET;
		}
		server.stations[i].s_tcp_count = 0;
		server.stations[i].state = zap_station_state_off;
	}

	// Create/Listen on TCP + UDP socket for Data/Control connections.

	// Create sockets.
	if ( zap_socket( 0, 1, &( server.tcp_socket )) )	{
		exit_error( "Could not create TCP socket\n" );
	}

	if ( zap_socket( 0, 0, &( server.udp_socket_rx ) ) )	{
		exit_error( "Could not create UDP rx socket\n" );
	} 
#ifndef WIN32
    else {
        int val=1;
        if (setsockopt( server.udp_socket_rx, SOL_SOCKET, SO_TIMESTAMP, ( const char * )&val, sizeof( val ) )) {
            exit_error( "Could not set UDP rx opt\n" );
        }
    }
#endif
	if ( zap_socket( 0, 0, &( server.udp_socket_tx ) ) )	{
		exit_error( "Could not create UDP tx socket\n" );
	}

	if ( setsockopt( server.tcp_socket, SOL_SOCKET, SO_REUSEADDR, ( char* )&bOptVal, bOptLen ) == SOCKET_ERROR ) {
		exit_error( "Set socket option SO_REUSEADDR failed on TCP socket\n" );
	}

	// Listen on TCP socket.
	if ( zap_listen( server.tcp_socket ) ) {
		exit_error( "Could not listen on TCP server socket\n" );
	}

	// Bind UDP rx socket.
	if ( zap_bind( server.udp_socket_rx ) ) {
		exit_error( "Could not bind UDP rx socket\n" );
	}

	printf("Zapd service started\n" );
	while ( 1 ) {
		zap_server_tx( &server, &fd );
		// Bind UDP rx socket.
		zap_server_rx( &server, &fd );
	}

	// Never reached
	return 0;
}

