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

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <iostream>

#ifdef _MSC_VER
  #include "win32_compat.h"
  #include "stdint_win32.h"
#else
  #include <stdint.h>
#endif

#include "tmsi.h"
#include "tmsi_bluez.h"
#include "edf.h"

#include "Server.h"
#include "Client.h"
#include "Message.h"

#include "tms_ip.h"

#define MNCIPP               (1536)  /**< Maximum number of characters in IP Packet */

static int32_t vb=0x0000;  /**<  verbose level */

static Server *server = NULL;
static Client *client = NULL;

/** Set verbose level of this module tms_ip
* @return old verbose level
*/
int32_t tms_ip_set_vb(int32_t tms_vb) {

  int32_t old_vb=vb;
  
  vb=tms_vb;
  return(old_vb);
}

/** Setup server on 'port'
 */
int32_t tms_ip_setup_server(int32_t port) {

  server = new Server(port);
  fprintf(stderr,"# Info: start transmission of IP text sample packets on port %d\n",port); 
  return(0); 
}

/** Setup client on 'ipname' and 'port'
 */
int32_t tms_ip_setup_client(char *ipname, int32_t port) {

  client = new Client(ipname,port);
  return(0); 
}

/** Send tms packet 'channel' with 'n' channels and selecting 'msk'
 *   as IP string t=<value> [<name>=<value>] on time 't'.
 *  @return number of characters send. 
 */
int32_t tms_ip_send(double t, tms_channel_data_t *channel, int32_t n, int32_t msk) {

  std::string msg;         /**< message to ip server */
  char    buff[MNCIPP];    /**< temporary buffer for text */
  int32_t i,j;             /**< general index */
  
  msg.clear();
  snprintf(buff,sizeof(buff)-1," t=%.8f",t);
  msg += buff;
  /* Check all channels */
  for (j=0; j<n; j++) {
    /* is channel 'j' active? */
    if ((msk)&(1<<j)) {
      /* write all samples of channel 'j' */
      for (i=0; i<channel[j].ns; i++) {
        if (i<channel[j].rs) {
          /* write value of sample 'i' */
          snprintf(buff,sizeof(buff)-1," %1C%d=%.4f",('A'+j),i,channel[j].data[i].sample);
        } else {
          /* write NaNs of sample 'i' */
          snprintf(buff,sizeof(buff)-1," %1C%d=NaN",('A'+j),i);
        }
        msg += buff;
      }
    }
  }
  msg += "\n";
  if (vb&0x01) {
    fprintf(stderr,"%s",msg.c_str());
  }
  /* send msg */
  server->sendMessage(msg.c_str());
  return(msg.size());
}

/** Send character array 'msg' as server.
 *  @return number of characters send. 
 */
int32_t tms_ip_send_str(char *msg) {

  if (vb&0x01) {
    fprintf(stderr,"%s",msg);
  }
  /* send msg */
  server->sendMessage(msg);
  return(strlen(msg));
}

/** Send character array 'msg' as client.
 *  @return number of characters send. 
 */
int32_t tms_ip_send_str_client(char *msg) {

  if (vb&0x01) {
    fprintf(stderr,"%s",msg);
  }
  /* send msg */
  client->sendMessage(msg);
  return(strlen(msg));
}

/** Send array 'a' with 'n' samples and 'name' of channel 'chn' at time 't' and 'dt' steps
 *   as IP string 'name chn t dt n a[0] ... a[n-1]'.
 *  @return number of characters send. 
 */
int32_t tms_ip_send_array(int32_t chn, char *name, double t, double dt, int32_t n, float *a) {

  std::string msg;       /**< message to ip server */
  char    buff[MNCIPP];  /**< temporary buffer for text */
  int32_t i;             /**< general index */
  
  msg.clear();
  snprintf(buff,sizeof(buff)-1,"a %d %.6f %.6f %s %d",chn,t,dt,name,n);
  msg += buff;
  /* Check all samples */
  for (i=0; i<n; i++) {
    /* write value of sample 'i' */
    snprintf(buff,sizeof(buff)-1," %.4f",a[i]);
    msg += buff;
  }
  msg += "\n";
  if (vb&0x01) {
    fprintf(stderr,"%s",msg.c_str());
  }
  /* send msg */
  server->sendMessage(msg.c_str());
  return(msg.size());
}
  

