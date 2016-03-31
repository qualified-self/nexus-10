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

#ifndef TMSI_WRAPPER_H
#define TMSI_WRAPPER_H

#include <string.h>

typedef void (*CallbackIntDouble)(int32_t bn, double rt);

typedef void (*CallbackInt)(int32_t bn);

/** Open bluetooth connection with Nexus on MAC address 'bt_addr'.
 * @param a_format 0:None 1:BDF (binary) 2:TXT
 * @param ip_port send tms packets as text string on 'ip_port' if 'ip_port'>0
 * @return negative (error code) on failure, positive (port no) on success
 */
int32_t tmsw_open(char* bt_addr, int32_t sample_rate_div, char* filename, char* task,
 int32_t a_channel_switch, int32_t a_format, const char **edfChnName, int32_t chn_cnt, int32_t ip_port);

int32_t tmsw_close();

void tmsw_expect_edge();

void tmsw_set_callback_response(CallbackIntDouble f_response);

void tmsw_set_callback_function(CallbackInt f_button);

void tmsw_measure_response(int32_t on);

void tmsw_notify_on_switch();

void tmsw_stop_notifying();

#endif

