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

#ifndef TMS_IP_H
#define TMS_IP_H

#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Set verbose level of this module tms_ip
* @return old verbose level
*/
int32_t tms_ip_set_vb(int32_t tms_vb);

/** Setup server on 'port'
 */
int32_t tms_ip_setup_server(int32_t port);

/** Setup client on 'ipname' and 'port'
 */
int32_t tms_ip_setup_client(char *ipname, int32_t port);

/** Send tms packet 'channel' with 'n' channels and selecting 'msk'
 *   as IP string t=<value> [<name>=<value>] on time 't'.
 *  @return number of characters send. 
 */
int32_t tms_ip_send(double t, tms_channel_data_t *channel, int32_t n, int32_t msk);

/** Send array 'a' with 'n' samples and 'name' of channel 'chn' at time 't' and 'dt' steps
 *   as IP string 'name chn t dt n a[0] ... a[n-1]'.
 *  @return number of characters send. 
 */
int32_t tms_ip_send_array(int32_t chn, char *name, double t, double dt, int32_t n, float *a);

/** Send character array 'msg' as server.
 *  @return number of characters send. 
 */
int32_t tms_ip_send_str(char *msg);

/** Send character array 'msg' as client.
 *  @return number of characters send. 
 */
int32_t tms_ip_send_str_client(char *msg);


#ifdef __cplusplus
}
#endif

#endif //TMS_IP_H
