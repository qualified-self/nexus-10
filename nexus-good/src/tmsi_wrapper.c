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



#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>

#include "tmsi.h"
#include "tmsi_bluez.h"
#include "tmsi_wrapper.h"
#include "tms_ip.h"
#include "edf.h"
  
static int32_t  vb               = 0x0000;
static int32_t  tms_chn_cnt      = 14;
static int32_t  tms_ip_port      = 16000;
static int32_t  disp_chn_nr      =  5;
static int32_t  switch_chn_nr    = 12;
static int32_t  tms_chn_msk      = 0x10FF;

static int32_t  dev              = 0;  /**< 1:Nexus 2:Holst 3:BDF file */
static int32_t  keep_notifying   = false;
static FILE    *fpe              = NULL;
static int32_t  channel_switch   = 0x10FF; /**< All channels of the Nexus */
static int32_t  format           = 1;      /**< BDF format */
static float    sample_freq      = 1024.0; /**< [Hz] default value for sample rate divider == 1 */
const  float    BUTTON_DELTA     = 0.0220; /**< button delta duration [s] */
const  float    STIMULI_DUR      = 0.5;    /**< stimuli duration [s] */
static int32_t  measure_response = 0;      /**< measure response */
static int32_t  rec_cnt          = 0;      /**< EDF/BDF record counter */
static edf_t    edf; 

typedef struct EDGE_T {
  int32_t sc;    /**< edge expect after this sample count */
  int32_t dur;   /**< wait 'dur' clock ticks for edges */
  int32_t vmin;  /**< minimum value */
  int32_t scmin; /**< sample count of minimum value */
  int32_t vmax;  /**< maximum value */
  int32_t scmax; /**< sample count of maximum value */
  int32_t thr;   /**< threshold value */
} edge_t;

#define MAX_EDGE       (5)
#define MAX_EDGE_DUR (0.3)       /**< [s] maximum waiting time for edges in stimuli or sync channel */
#define MAX_RESPONSE (1.5)       /**< [s] maximum response time */

static edge_t edge[MAX_EDGE];
static int32_t expect_edge = 0; 

static CallbackInt       _callback_function_button  =NULL;
static CallbackIntDouble _callback_function_response=NULL;

static int port = -1;
static tms_channel_data_t *channel = NULL;

int tmsw_close() {

  if ((dev==1) && (port>0)) {
    fprintf(stderr,"# Closing Nexus\n");
    tms_shutdown();
    tms_close_port(port);
    fclose(fpe);
  }
  
  return(0);
}

/** Find channel index of 'name' in 'list' of 'n' names.
 * @return channel number 0...n-1 or -1 on failure.
*/
int32_t fnd_chn_idx(const char **list, int32_t n, const char *name) {

  int32_t i;          /**< signal counter */
  int32_t nr=-1;
   
  for (i=0; i<n; i++) {
    if (strcmp(list[i],name)==0) { 
      nr=i; break; 
    }
  }
  return(nr);
}

/** Open bluetooth connection with Nexus on MAC address 'bt_addr'.
 * @param ip_port send tms packets as text string on 'ip_port' if 'ip_port'>0
 * @param a_format 0:None 1:BDF (binary) 2:TXT
 * @return negative (error code) on failure, positive (port no) on success
 */
int32_t tmsw_open(char* bt_addr, int32_t sample_rate_div, char* filename, char* task,
 int32_t a_channel_switch, int32_t a_format, const char **edfChnName, int32_t chn_cnt, int32_t ip_port) {
  
  int32_t tms_vb = 0x0;
  time_t now;             /**< wall clock time [s] since 1970-1-1 00:00:00 */
  FILE   *fpi;            /**< EDF/BDF file pointer */
  
  tms_set_vb(tms_vb);
  
  if (strstr(bt_addr,":")!=NULL)   { 
    dev=1; /* Nexus */ 
  } else 
  if (strstr(bt_addr,"USB")!=NULL) { 
    dev=2; /* Holst */ 
    fprintf(stderr,"# Error: Holst input not implemented yet\n");
    return(-1);
  } else {
    dev=3; /* edf/bdf input */
  }
  
  channel_switch = a_channel_switch;
  
  tms_chn_cnt = chn_cnt;
  
  tms_ip_port = ip_port;
   
  if (dev==1) {
    format = a_format;

    tms_chn_cnt = 14;
    tms_chn_msk=0x10FF;

    /* Nexus input */
    fprintf(stderr,"# Opening bt address %s\n", bt_addr);

    if ((port = tms_open_port(bt_addr))<1) {
      fprintf(stderr,"# Error opening Bluetooth port %d\n", port);
      return -1;
    }
    fprintf(stderr,"# Bluetooth communication uses port: %d\n", port);

    if ( tms_init(port, sample_rate_div) < 0 ) {
      fprintf(stderr,"# Error: Could not initialize Nexus");
      return -2;
    }

    sample_freq = (float)tms_get_sample_freq();
    fprintf(stderr,"# Nexus: initialized for task %s with sample frequency %5.1f [Hz]\n",
       task,sample_freq);

    /* allocate space for channel data */
    channel = tms_alloc_channel_data();
    

    if (format>0) {
      fprintf(stderr,"# Opening %s file %s\n",(format==1 ? "BDF" : "TXT"), filename);
      if ((fpe = fopen(filename, (format == 1 ? "wb" : "w"))) == NULL) {
        perror("");
        return -3;
      } 
    } 

    if (format==1) {    

      time(&now);
      /* write EDF/BDF headers */
      edfWriteHdr(fpe,channel,tms_get_number_of_channels(),channel_switch,
       filename,task,&now,channel[0].ns/tms_get_sample_freq(),edfChnName);
    }

    /* display channel nr */
    if ((disp_chn_nr=fnd_chn_idx(edfChnName,chn_cnt,"Disp"))<0) {
      fprintf(stderr,"# Error: can't find channel index of signal 'Disp' in edf name list\n");
      port=-1;
    }
    
    /* switch channel nr */
    switch_chn_nr=12;
    fprintf(stderr,"# Info: channel index disp %d switch %d\n",disp_chn_nr,switch_chn_nr);
    
    
  } else
  if (dev==3) {
    /* no output needed with file input */
    format = 0;
    /** EDD/BDF file input */
    fprintf(stderr,"# Open EDF/BDF file %s\n",bt_addr);
    if ((fpi=fopen(bt_addr,"r"))==NULL) {
      perror(NULL); return(-1);
    }
    /* read EDF/BDF main and signal headers */
    edf_rd_hdr(fpi,&edf);
    /* read EDF/BDF samples */
    edf_rd_samples(fpi,&edf);
    /* close input file */
    fclose(fpi);
    
    tms_chn_msk=0x01FF;

    /* allocate space for data samples */
    if ((channel=edf_alloc_channel_data(&edf))==NULL) {
      fprintf(stderr,"# main: alloc_channel_data problem!\n");
      exit(-1);
    }
    /* use default positive number for port to return */
    port=1;
    
    /* Use sample frequency of channel '0' */
    sample_freq=edf.signal[0].NrOfSamplesPerRecord / edf.RecordDuration;
    
    /* display channel nr */
    if ((disp_chn_nr=edf_fnd_chn_nr(&edf,"Disp"))<0) {
      fprintf(stderr,"# Error: can't find channel nr of signal 'Disp'\n");
      port=-1;
    }
    
    /* switch channel nr */
    if ((switch_chn_nr=edf_fnd_chn_nr(&edf,"Switch"))<0) {
      fprintf(stderr,"# Error: can't find channel nr of signal 'Switch'\n");
      port=-1;
    }
    fprintf(stderr,"# Info: channel nr disp %d switch %d\n",disp_chn_nr,switch_chn_nr);
  }
  
  fprintf(stderr,"# Info: IP text sample message at port %d\n",tms_ip_port);
  if (tms_ip_port>0) {
    /* setup IP port transmission */
    tms_ip_setup_server(tms_ip_port);
  }
  
  return port;
}

void tmsw_stop_notifying() {

  keep_notifying = false;
}

void tmsw_set_callback_response(CallbackIntDouble f_response) {

  if (_callback_function_response == NULL) {
    _callback_function_response = f_response;
    fprintf(stderr,"# Info: tms_set_callback_response registered\n");
  }
}

void tmsw_set_callback_function(CallbackInt f_button) {

  if (_callback_function_button == NULL) {
    _callback_function_button = f_button;
    fprintf(stderr,"# Info: tms_set_callback_function for button press registered\n");
  }
}

void tmsw_battery_chk(tms_channel_data_t *channel) {

  static int chk=0;
  
  if (channel[switch_chn_nr].data[0].isample & 0x02) {
    chk++;
    if (chk==1) {
      fprintf(stderr,"# Battery nearly empty\n");
      fprintf(stdout,"Bat low\n");
    }
  }
}

void tmsw_expect_edge() {

  expect_edge++;
}

void tmsw_measure_response(int32_t on) {

  measure_response=on;
}

void tmsw_check_for_stimuli(tms_channel_data_t *channel) {

  static int32_t threshold=0;
  static int32_t threshold_found=0;  /* 0:no action 1:look for proper threshold 2:found threshold */
  int32_t i,j,sc;
  int32_t isample;
  static int32_t state=0; /**< signal/sync state */
  int32_t edge_done=0;
  int32_t edge_usefull=0;
  int32_t edge_ok=0;
  double t=0.0;           /**< current timestamp */
  static double tr=0.0;   /**< timestamp of rising edge */
  static double tf=0.0;   /**< timestamp of falling edge */
  static int32_t sw=0;    /**< switch state 0:wait for rise 1:wait for fall 2:wait for restart */
  static double tsw=0.0;  /**< timestamp of first rising edge switch */
  double tsd=0.0;         /**< switch duration */
  static int32_t bt=0;    /**< button type */
  double rt=0.0;          /**< reaction time [s] */
  static int32_t err1=0,err2=0;   /**< callback function pointer missing counter */
  static int32_t fnd_edge=0;      /**< found rising edge in sync channel */
  int32_t dist,max_dist;  /**< distance between 'vmax' and 'vmin' */
        
  /* catch hint for draw sync */
  if (expect_edge==1) {
    /* current sample counter */
    sc=channel[disp_chn_nr].sc;
    /* current sample value */
    isample = channel[disp_chn_nr].data[0].isample;    
    for (j=0; j<MAX_EDGE; j++) {
      if (j==0) {
        edge[j].sc=sc;
      } else {
        edge[j].sc=edge[j-1].sc+edge[j-1].dur;
      }
      edge[j].dur=(int32_t)round(MAX_EDGE_DUR/channel[disp_chn_nr].td);
      edge[j].vmin=isample; edge[j].scmin=sc;
      edge[j].vmax=isample; edge[j].scmax=sc;
      
      fprintf(stderr,"# Info: edge %d sc %d dur %d\n",j,edge[j].sc,edge[j].dur);
    }
    threshold_found=1;
    expect_edge++;
  }

  /* check display channel samples */
  for (i=0; i<channel[disp_chn_nr].rs; i++) {
    /* check for overflow */
    if (channel[disp_chn_nr].data[i].flag<0x04) {
      
      /* current sample number */
      sc=channel[disp_chn_nr].sc+i;
      /* current time [s] */  
      t=channel[disp_chn_nr].td*sc;
      /* current integer sample value */
      isample = channel[disp_chn_nr].data[i].isample;
      
      /* find threshold and edges in stimuli signal */
      /* try to estimate a proper threshold value */
      if (threshold_found==1) {
        edge_done=0;
        /* get min and max values of each active edge */
        for (j=0; j<MAX_EDGE; j++) {
          /* check if current sample counter is in range of this edge 'j' */
          if ((sc >= edge[j].sc) && (sc < edge[j].sc + edge[j].dur)) {
             if (edge[j].vmin > isample) { 
               edge[j].vmin = isample; edge[j].scmin = sc;
             }
             if (edge[j].vmax < isample) { 
               edge[j].vmax = isample; edge[j].scmax = sc;
            }
          }
          if (sc > edge[j].sc + edge[j].dur) {
            edge_done++;
          }
          if (edge[j].scmin != edge[j].scmax) { edge_usefull++; }
        }
        /* check if all edge are expired */
        if (edge_done==MAX_EDGE) {
          /* count all valid edges */
          edge_ok=0; 
          for (j=0; j<MAX_EDGE; j++) {
            if (edge[j].scmin != edge[j].scmax) { edge_ok++; }
          }
          threshold=0; dist=0; max_dist=0;
          if (edge_ok>0) {
            fprintf(stderr,"# %2s %9s %9s %9s %9s %9s\n","j","vmin","scmin","vmax","scmax","thr");
            for (j=0; j<MAX_EDGE; j++) {
              if (edge[j].scmin != edge[j].scmax) {
                edge[j].thr=(edge[j].vmax + edge[j].vmin)/2;
                /* select threshold with maximum distance between 'vmax' and 'vmin' */
                dist=edge[j].vmax - edge[j].vmin;
                if (dist > max_dist) {
                  max_dist = dist;
                  threshold = edge[j].thr;
                }
                fprintf(stderr,"# %2d %9d %9d %9d %9d %9d\n",j,edge[j].vmin,edge[j].scmin,
                  edge[j].vmax,edge[j].scmax,edge[j].thr);
              }
            }
          }
          if (edge_ok<(MAX_EDGE/2)) {
            if (edge_usefull>1) {
              fprintf(stderr,"# Warning: found only %d edges. Not enough!!!\n",edge_ok);
            }
          }
          if (edge_ok>0) {
            fprintf(stderr,"# threshold %d %.3f [uV]\n", threshold, 0.2384186*threshold);
            threshold_found=2;
          } else {
            if (edge_usefull>1) {
              fprintf(stderr,"# Error: No valid edge found!!!\n");
            }
          }
        }
      }
      
      /* look for edge in sync channel */
      if (threshold_found==2) {
        if ((state==0) && (isample>threshold)) {
          /* valid rising edge if distance to previous falling edge is large enough */
          state=1; if ((t-tf)>STIMULI_DUR) { tr=t; }
          /* clear button type */
          bt=0;
          /* found rising edge in sync channel */
          fnd_edge=1;
        } else 
        if ((state==1) && (isample<=threshold)) {
          /* falling edge */
          state=0; tf=t;
        }
      }
    }
  }
  
  /* check switch channel samples */
  for (i=0; i<channel[switch_chn_nr].rs; i++) {
    
    /* current sample number */
    sc=channel[switch_chn_nr].sc+i;
    /* current time [s] */
    t=channel[switch_chn_nr].td*sc;
    /* current sample */
    isample = channel[switch_chn_nr].data[i].isample;
    
    //fprintf(stderr,"sw %d isample 0x%04X\n",sw,isample);
    
    if ((sw==0) && ((isample&0x01)==0x01)) {
      /* rising switch press */
      tsw=t; sw=1;
      if (vb&0x01) {
        fprintf(stderr,"# Info: found rising switch at sc %d t %9.3f [s]\n",sc,tsw);
      }
    } else 
    if ((sw==1) && ((isample&0x01)==0x00)) {
      /* falling switch press */
      tsd=t-tsw; bt=(int32_t)round(tsd / BUTTON_DELTA); 
      if (vb&0x01) {
        fprintf(stderr,"# Info: found button %d at sc %d t %9.3f [s] with duration %6.3f [s]\n",bt,sc,t,tsd);
      }
      /* call calculate which button from on duration */
      if (_callback_function_button!=NULL) {
        
        /* richard van de wolf mouse knoppen kastje */
        if (bt==3) { bt=2; } else
        if (bt==4) { bt=1; }
        
        _callback_function_button(bt);
      } else {
        if (err1==0) {
          fprintf(stderr,"# Error: Missing _callback_function_button!!!\n");
          err1++;
        }
      }
      if (measure_response==0) { 
        /* look for next button press when we are not measuring response timing */
        sw=0; 
      } else { 
        /* wait for next button press when we are measuring response timing */
        sw=2; 
      }
    }
  }
  
  /* call callback if maximum response time is expired */       
  if ((threshold_found==2) && (fnd_edge==1) && ((t-tr)>MAX_RESPONSE)) {

    /* measure response time */
    if (tsw>tr) { rt=tsw-tr; } else { rt=MAX_RESPONSE; }

    /* report response time */
    if (measure_response==1) {
      if (_callback_function_response!=NULL) {
        
        /* richard van de wolf mouse knoppen kastje */
        //if (bt==3) { bt=2; } else
        //if (bt==4) { bt=1; }
        
        _callback_function_response(bt,rt);
      } else {
        if (err2==0) {
          fprintf(stderr,"# Warning: Missing _callback_function_response!!!\n");
          err2++;
        }      
      }
    }
    /* wait for next rising edge in display channel */
    fnd_edge=0;
    /* look for new switch presses */
    sw=0;
  }
}

void tmsw_notify_on_switch() {

  int32_t mpc=0;
  double  t;

  keep_notifying = true;
  
  while (keep_notifying) {
  
    if (dev==1) {
      mpc=tms_get_samples(channel);
    } else 
    if (dev==3) {
      /* get next block of EDF/BDF samples */
      edf_rd_chn(&edf,channel);
      /* wait for wall clock time to pass */
      usleep((int32_t)round(1e6*channel[0].ns*channel[0].td));
      /* input file should have no missing samples */
      mpc=0;
      /* if channel block is incomplete the input file has ended */
      if (channel[0].rs<channel[0].ns) {
        keep_notifying=false;
      }
    }
    rec_cnt+=mpc+1;

    if (format==1) {  
      /* write samples to BDF output file */
      edfWriteSamples(fpe, 1, channel, channel_switch, mpc);
    } else
    if (format==2) {
      /* write samples to TEXT output file */
      tms_prt_samples(fpe, channel, channel_switch, 0, tms_get_number_of_channels() );
    }
    
    if (tms_ip_port>0) {
      t=channel[0].sc*channel[0].td;
      tms_ip_send(t,channel, tms_chn_cnt, tms_chn_msk); 
    }

    tmsw_battery_chk(channel);

    tmsw_check_for_stimuli(channel);

  }
  
  if (format==1) {
    /* set record counter in EDF/BDF output file */
    if (edf_set_record_cnt(fpe,rec_cnt)!=0) {
      fprintf(stderr,"# Error: problem in setting record counter %d in BDF file!\n",rec_cnt);
    }
  }
}
