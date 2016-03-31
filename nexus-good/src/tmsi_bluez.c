/** @Copyright

This software and associated documentation files (the "Software") are 
copyright ©  2010 Koninklijke Philips Electronics N.V. All Rights Reserved.

A copyright license is hereby granted for redistribution and use of the 
Software in source and binary forms, with or without modification, provided 
that the following conditions are met:
 1. Redistributions of source code must retain the above copyright notice, 
    this copyright license and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice, 
    this copyright license and the following disclaimer in the documentation 
    and/or other materials provided with the distribution.
 3. Neither the name of Koninklijke Philips Electronics N.V. nor the names 
    of its subsidiaries may be used to endorse or promote products derived 
    from the Software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS 
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, 
OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifdef _MSC_VER
  #include "win32_compat.h"
  #include <winsock2.h>
  #include <ws2bth.h>
  #pragma comment(lib, "ws2_32.lib")
#else
  #include <stdint.h>
  #include <sys/ioctl.h>
  #include <unistd.h>
  #include <sys/time.h>
  #include <termios.h>
  #include <sys/socket.h>
  #include <bluetooth/bluetooth.h>
  #include <bluetooth/rfcomm.h>
#endif


#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <malloc.h>


#include "tmsi_bluez.h"
#include "tmsi.h"

static int32_t socket_fd = -1;

/* To catch Ctrl-C
 */
void signal_handler(int sig) {
  
  (void) sig;    // Avoid warning about unused parameter

  fprintf(stderr, "# Info: Caught Ctrl-C, closing file descriptor to socket %d\n", socket_fd);
  tms_close_port();
//  signal(SIGINT, SIG_DFL);
#ifdef _MSC_VER
  abort();
#else
  kill(getpid(), SIGKILL);
#endif
}

/** Open bluetooth device 'fname' to TMSi aquisition device
 *  Nexus-10 or Mobi-8.
 * @return socket >0 on success <0 on failure
 */
int32_t tms_open_port(char *fname) {
 
  uint8_t    msg[8];   /**< BT message buffer */
  int32_t    br=0;     /**< bytes read */
  int32_t    cnt=0;    /**< byte counter */

#ifdef _MSC_VER
  /* set sig handler to catch ^c */
  //signal(SIGINT, signal_handler);

  SOCKADDR_BTH addr = {0, 0, 0, 0};
  int iAddrLen      = sizeof(addr);
  u_long iMode      = 0;

  /* allocate a socket */
  socket_fd = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
  if ( INVALID_SOCKET == socket_fd ) {
    fprintf(stderr,"# Error socket() call failed. WSAGetLastError = [%d]\n", WSAGetLastError());
    socket_fd = -1;
    return socket_fd; 
  }

  fprintf(stderr,"# Info: Try to open bluetooth socket to MAC address %s\n",fname);

  /* set the connection parameters (who to connect to) */
  if ( 0 != WSAStringToAddress(fname, AF_BTH, NULL, (LPSOCKADDR)&addr, &iAddrLen) ) {
    fprintf(stderr,"# Error unable to get address of the remote radio having name %s\n", fname);	
    closesocket (socket_fd);
    socket_fd = -1;
    return socket_fd; 
  }

  /* open connection to TMSi hardware */
  addr.port = 1;   // Or BT_PORT_ANY ??
  if ( SOCKET_ERROR == connect(socket_fd, (struct sockaddr *) &addr,  sizeof(SOCKADDR_BTH))) {
    fprintf(stderr,"# Error  connect() call failed. WSAGetLastError=[%d]\n", WSAGetLastError());
    closesocket (socket_fd);
    socket_fd = -1;
    return socket_fd; 
  }

  /* make connection non blocking */
  iMode = 1;
  if (NO_ERROR != ioctlsocket(socket_fd, FIONBIO, &iMode)){
    fprintf(stderr,"# Error set non-blocking call failed. WSAGetLastError=[%d]\n", WSAGetLastError());
    close (socket_fd);
    socket_fd = -1;
    return socket_fd; 
  }

  fprintf(stderr,"# Successfully opened non-blocking socket %d\n",socket_fd);

#else

   /* set sig handler to catch ^c */
  //signal(SIGINT, signal_handler);

  struct  sockaddr_rc addr = { 0 };
  int32_t status;
  int32_t flags;

  /* allocate a socket */
  socket_fd = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
  if (socket_fd < 0) {
    fprintf(stderr,"# Error: Build Bluetooth socket error!\n");
    return(-2);
  }

  fprintf(stderr,"# Info: Try to open bluetooth socket to MAC address %s\n",fname);
  /* set the connection parameters (who to connect to) */
  addr.rc_family = AF_BLUETOOTH;
  addr.rc_channel = (uint8_t) 1;
  str2ba(fname, &addr.rc_bdaddr );

  /* open connection to TMSi hardware */
  status = connect(socket_fd, (struct sockaddr *)&addr, sizeof(addr));
  if (status<0) { 
    fprintf(stderr,"# Error: socket connection failed %d\n",status);
    close(socket_fd); 
    socket_fd = -1;
    return(-1); 
  }

  /* make connection non blocking */
  if (-1 == (flags = fcntl(socket_fd, F_GETFL, 0))){
    flags = 0;
  }
  fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);

  fprintf(stderr,"# Successfully opened non-blocking socket %d\n",socket_fd);


#endif

  /* read all bytes available in bluetooth buffer */
  cnt=0;
  while ((br=BtReadBytes(socket_fd,msg,1))>0) { 
    cnt+=br; usleep(100);
  } 

  if (cnt>0) {
    fprintf(stderr,"# Info: found %d bytes in bluetooth buffer\n",cnt);
  }
  
  /* return socket */
  return(socket_fd);
}


/** Close socket file descriptor 'socket_fd'.
 * @return 0 on successm, errno on failure.
 */
int32_t tms_close_port() {

#ifdef  _MSC_VER
  if (closesocket(socket_fd) != 0){
    int sockError = WSAGetLastError();
    fprintf(stderr, "# Info: close_port: Error closing port - %d", sockError);
    return sockError;
  }
#else
  if (close(socket_fd) == -1) {
    perror("# Info: close_port: Error closing port - ");
    return errno;
  }
#endif
  socket_fd = -1;
  return(0);
}