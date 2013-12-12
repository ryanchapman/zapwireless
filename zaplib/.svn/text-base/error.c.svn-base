/*--------------------------------------------------------------- 
 * Copyright (c) 1999,2000,2001,2002,2003                              
 * The Board of Trustees of the University of Illinois            
 * All Rights Reserved.                                           
 *--------------------------------------------------------------- 
 * Permission is hereby granted, free of charge, to any person    
 * obtaining a copy of this software (Iperf) and associated       
 * documentation files (the "Software"), to deal in the Software  
 * without restriction, including without limitation the          
 * rights to use, copy, modify, merge, publish, distribute,        
 * sublicense, and/or sell copies of the Software, and to permit     
 * persons to whom the Software is furnished to do
 * so, subject to the following conditions: 
 *
 *     
 * Redistributions of source code must retain the above 
 * copyright notice, this list of conditions and 
 * the following disclaimers. 
 *
 *     
 * Redistributions in binary form must reproduce the above 
 * copyright notice, this list of conditions and the following 
 * disclaimers in the documentation and/or other materials 
 * provided with the distribution. 
 * 
 *     
 * Neither the names of the University of Illinois, NCSA, 
 * nor the names of its contributors may be used to endorse 
 * or promote products derived from this Software without
 * specific prior written permission. 
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
 * NONINFRINGEMENT. IN NO EVENT SHALL THE CONTIBUTORS OR COPYRIGHT 
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, 
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
 * ________________________________________________________________
 * National Laboratory for Applied Network Research 
 * National Center for Supercomputing Applications 
 * University of Illinois at Urbana-Champaign 
 * http://www.ncsa.uiuc.edu
 * ________________________________________________________________ 
 *
 * error.c
 * by Mark Gates <mgates@nlanr.net>
 * -------------------------------------------------------------------
 * error handlers
 * ------------------------------------------------------------------- */
#include "error.h"



#ifdef WIN32
#include "Winsock2.h"
/* -------------------------------------------------------------------
 * Implement a simple Win32 strerror function for our purposes.
 * These error values weren't handled by FormatMessage;
 * any particular reason why not??
 * ------------------------------------------------------------------- */

struct mesg {
    DWORD       err;
    const char* str;
};

const struct mesg error_mesgs[] =
{
    { WSAEINTR,              "Interrupted function call."},                       // 4
    { WSAEACCES,             "Permission denied"},                                // 13
    { WSAEFAULT,             "Bad address"},                                      // 14
    { WSAEINVAL,             "Invalid argument."},                                // 22
    { WSAEMFILE,             "Too many open files."},                             // 24
    { WSAEWOULDBLOCK,        "Resource temporarily unavailable"},                 // 35
    { WSAEINPROGRESS,        "Operation now in progress"},                        // 36
    { WSAEALREADY,           "Operation already in progress"},                    // 37
    { WSAENOTSOCK,           "Socket operation on non-socket."},                  // 38
    { WSAEDESTADDRREQ,       "Destination address required"},                     // 39
    { WSAEMSGSIZE,           "Message too long"},                                 // 40
    { WSAEPROTOTYPE,         "Protocol wrong type for socket"},                   // 41
    { WSAENOPROTOOPT,        "Bad protocol option."},                             // 42
    { WSAEPROTONOSUPPORT,    "Protocol not supported"},                           // 43
    { WSAESOCKTNOSUPPORT,    "Socket type not supported."},                       // 44
    { WSAEOPNOTSUPP,         "Operation not supported"},                          // 45
    { WSAEPFNOSUPPORT,       "Protocol family not supported"},                    // 46
    { WSAEAFNOSUPPORT,       "Address family not supported by protocol family"},  // 47
    { WSAEADDRINUSE,         "Address already in use"},                           // 48
    { WSAEADDRNOTAVAIL,      "Cannot assign requested address"},                  // 49
    { WSAENETDOWN,           "Network is down"},                                  // 50
    { WSAENETUNREACH,        "Network is unreachable"},                           // 51
    { WSAENETRESET,          "Network dropped connection on reset"},              // 52
    { WSAECONNABORTED,       "Software caused connection abort"},                 // 53
    { WSAECONNRESET,         "Connection reset by peer"},                         // 54
    { WSAENOBUFS,            "No buffer space available."},                       // 55
    { WSAEISCONN,            "Socket is already connected."},                     // 56
    { WSAENOTCONN,           "Socket is not connected"},                          // 57
    { WSAESHUTDOWN,          "Cannot send after socket shutdown"},                // 58
    { WSAETIMEDOUT,          "Connection timed out."},                            // 60
    { WSAECONNREFUSED,       "Connection refused"},                               // 61
    { WSAEHOSTDOWN,          "Host is down"},                                     // 64
    { WSAEHOSTUNREACH,       "No route to host"},                                 // 65
    { WSAEPROCLIM,           "Too many processes."},                              // 67
    { WSASYSNOTREADY,        "Network subsystem is unavailable."},                // 91
    { WSAVERNOTSUPPORTED,    "WINSOCK.DLL version out of range."},                // 92
    { WSANOTINITIALISED,     "Successful WSAStartup not yet performed."},         // 93
    { WSAEDISCON,            "Graceful shutdown in progress."},                   // 101
    { WSASYSCALLFAILURE,     "System call failure."},                             // 107
    { WSATYPE_NOT_FOUND,     "Class type not found."},                            // 109
    { WSAHOST_NOT_FOUND,     "Host not found."},                                  // 1001
    { WSATRY_AGAIN,          "Non-authoritative host not found."},                // 1002
    { WSANO_RECOVERY,        "This is a non-recoverable error."},                 // 1003
    { WSANO_DATA,            "Valid name, no data record of requested type."},    // 1004
    { WSA_INVALID_HANDLE,    "Specified event object handle is invalid."},        // 
    { WSA_INVALID_PARAMETER, "One or more parameters are invalid."},              // 
    { WSA_IO_INCOMPLETE,     "Overlapped I/O event object not in signaled state."}, // 
    { WSA_IO_PENDING,        "Overlapped operations will complete later."},       // 
    { WSA_NOT_ENOUGH_MEMORY, "Insufficient memory available."},                   // 
    { WSA_OPERATION_ABORTED, "Overlapped operation aborted."},                    // 
    { 0,                     "No error."}

    /* These appeared in the documentation, but didn't compile.
     * { WSAINVALIDPROCTABLE,   "Invalid procedure table from service provider." },
     * { WSAINVALIDPROVIDER,    "Invalid service provider version number." },
     * { WSAPROVIDERFAILEDINIT, "Unable to initialize a service provider." },
     */

}; /* end error_mesgs[] */

//char* winsock_strerror( DWORD inErrno );
/* -------------------------------------------------------------------
 * winsock_strerror
 *
 * returns a string representing the error code. The error messages
 * were taken from Microsoft's online developer library.
 * ------------------------------------------------------------------- */

char* winsock_strerror( unsigned long inErrno ) {
    char* str = "Unknown error";
    int i;
    for ( i = 0; i < sizeof(error_mesgs); i++ ) {
        if ( error_mesgs[i].err == inErrno ) {
            str = error_mesgs[i].str;
            break;
        }
    }

    return str;
} /* end winsock_strerror */
#else
#include <sys/socket.h>
#include <errno.h>
#endif /* WIN32 */

/* -------------------------------------------------------------------
 * warn
 *
 * Prints message and return
 * ------------------------------------------------------------------- */

void warn( const char *inMessage, const char *inFile, int inLine ) {
    fflush( 0 );

#ifdef NDEBUG
    fprintf( stderr, "%s failed\n", inMessage );
#else

    /* while debugging output file/line number also */
    fprintf( stderr, "%s failed (%s:%d)\n", inMessage, inFile, inLine );
#endif
} /* end warn */

/* -------------------------------------------------------------------
 * warn_errno
 *
 * Prints message and errno message, and return.
 * ------------------------------------------------------------------- */

void warn_errno( const char *inMessage, const char *inFile, int inLine ) {
    int my_err;
    char* my_str;

    /* get platform's errno and error message */
#ifdef WIN32
    my_err = WSAGetLastError();
    my_str = winsock_strerror( my_err );
#else
    my_err = (int) errno;
    my_str = (char*) strerror( my_err );
#endif

    fflush( 0 );

#ifdef NDEBUG
    fprintf( stderr, "%s failed: %s\n", inMessage, my_str );
#else

    /* while debugging output file/line number and errno value also */
    fprintf( stderr, "%s failed (%s:%d): %s (%d)\n",
             inMessage, inFile, inLine, my_str, my_err );
#endif
} /* end warn_errno */

