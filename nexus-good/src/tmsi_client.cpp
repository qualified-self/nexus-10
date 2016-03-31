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
#include <cstdio>

#ifdef _MSC_VER
  #include "win32_compat.h"
  #include "stdint_win32.h"
#else
  #include <stdint.h>
#endif

#include <tmsi.h>
#include "Client.h"
#include "Message.h"

#include <boost/thread.hpp>
#include <boost/bind.hpp>

#include <QApplication>
#include <QMainWindow>
#include <QDialog>
#include <QVBoxLayout>
#include <QDoubleSpinBox>
#include <QLabel>

#include "CBioPlot.h"

#define MNCIPP               (1536)  /**< Maximum number of characters in IP Packet */

#define IPDEF           "localhost"  /**< default ip address */
#define PORTDEF             (16000)  /**< default port */
#define CHNDEF          "ABCDEFGHM"  /**< default channel switch */
#define SDDEF                 (0.0)  /**< default sample time [s] 0.0 == forever */

#define VERSION "$Revision: 0.5 $ $Date: 2010/07/08 22:10:00 $"

int32_t dbg=0x0000;     /**< debug level */
int32_t  vb=0x0000;     /**< verbose level */

int32_t lbl=2;          /**< default channel name selection */

volatile int32_t pressed_CtrlC = 0;

#define NR_OF_CHANNELS   (14)
static const char *chndef_str=CHNDEF;
static const char *DefName1[NR_OF_CHANNELS] = {    "A",    "B",    "C",    "D",  "E",   "F",  "G",   "H","I","J","K","L",     "M","N"};
static const char *DefName2[NR_OF_CHANNELS] = {"C3-A1","C4-C3","A2-C4","A1-A2","ECG","Resp","GSR","Disp","I","J","K","L","Switch","N"};
char    *edfChnName[NR_OF_CHANNELS]; 
char     ipname[MNCIPP];           /**< IP address to listen to */
int32_t  port;                     /**< port number */
int32_t  chn;                      /**< channel selection switch */
float    window_len[NR_OF_CHANNELS]= { 4.0 };  /**< default window length [s] */

tms_channel_data_t  channel[NR_OF_CHANNELS];   /**< channel data */

static CBioPlot     *plot[NR_OF_CHANNELS] = { NULL };
static QMainWindow  *main_window = NULL;
static QApplication *main_app = NULL;


/** Print usage to file 'fp'.
 *  @return number of printed characters.
*/
int32_t tmsi_client_intro(FILE *fp) {
  
  int32_t i,nc=0;
  
  nc+=fprintf(fp,"tmsi_client: %s\n",VERSION); 
  nc+=fprintf(fp,"Usage: tmsi_client [-i <ip>] [-p <port] [-c <CHN>] [-t <sd>] [-l <lbl>] [-v <vb>] [-d <dbg>]\n");
  nc+=fprintf(fp,"   [-A <A,wl>] [-B <B,wl>] ... [-h]\n");
  nc+=fprintf(fp,"  Press CTRL+c to stop capturing bio-data\n");
  nc+=fprintf(fp,"ip   : ip address (default=%s)\n",IPDEF);
  nc+=fprintf(fp,"port : port number (default=%d)\n",PORTDEF);
  nc+=fprintf(fp,"CHN  : channel selection string (default=%s)\n",CHNDEF);
  nc+=fprintf(fp,"lbl  : channel label (default=%d)\n",lbl);
  nc+=fprintf(fp," chn  name1  name2  wl[s]  wl = window length [s]\n");
  for (i=0; i<NR_OF_CHANNELS; i++) {
    nc+=fprintf(fp," %3d %6s %6s %5.1f\n",i,DefName1[i],DefName2[i],window_len[i]);
  }
  nc+=fprintf(fp,"h    : show this manual page\n");
  nc+=fprintf(fp,"vb   : verbose switch (default=0x%02X)\n",vb);
  nc+=fprintf(fp,"        0x01 : show all IP traffic\n");
  nc+=fprintf(fp,"        0x02 : show signal channel names\n");
  nc+=fprintf(fp,"        0x04 : IP packet parsing\n");
  nc+=fprintf(fp,"        0x08 : channel packet\n");
  nc+=fprintf(fp,"dbg  : debug value (default=0x%02X)\n",dbg);
  return(nc);
}

int32_t split_str(char *msg, char *a, char *b) {
 
  int32_t  i;
  char    *token;
           
  if (vb&0x08) { fprintf(stderr,"# Info msg '%s' ",msg); }
  strcpy(a,""); b=strcpy(b,"");
  /* split 'msg' string */
  token=strtok(msg,","); i=0;
  while (token!=NULL) {
    switch (i) {
      case 0: strcpy(a,token); break;
      case 1: strcpy(b,token); break;
      default: break;
    }
    token=strtok(NULL,","); i++;
  }
  if (vb&0x08) { fprintf(stderr,"name '%s' wl '%s' i %d\n",a,b,i); }
  return(i);
}

void parse_cmd(int32_t argc, char *argv[]) {

  int32_t i,idx;          /**< general index */
  time_t  now;
  struct  tm t0;
  char    msg[MNCN];
  char    name[MNCN];     /**< channel name */
  char    wl[MNCN];       /**< window length [s] */
  char  **cp;             /**< parsing character pointer */
  
  strcpy(ipname,IPDEF); port=PORTDEF; chn=tms_chn_sel((char *)CHNDEF);
  
  /* copy default channel names */
  for (idx=0; idx<NR_OF_CHANNELS; idx++) {
    switch (lbl) { 
      case 2:    
        edfChnName[idx]=(char *)DefName2[idx]; break;
      default:    
        edfChnName[idx]=(char *)DefName1[idx]; break;
    }   
    window_len[idx]=4.0;
  }
  
  for (i=1; i<argc; i++) {
    if (argv[i][0]!='-') {
      fprintf(stderr,"# Warning: missing - in argument %s\n",argv[i]);
    } else {
      switch (argv[i][1]) {
        case 'i': strcpy(ipname,argv[++i]); break;
        case 'c': chn=tms_chn_sel(argv[++i]); break;
        case 'p': port=strtol(argv[++i],NULL,0); break;
        case 'v': vb=strtol(argv[++i],NULL,0); break;
        case 'd': dbg=strtol(argv[++i],NULL,0); break;
        case 'h': tmsi_client_intro(stderr); exit(0); break;
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
        case 'G':
        case 'H':
        case 'I': 
        case 'J': 
        case 'K': 
        case 'M': 
        case 'N': 
          idx=(int32_t)(argv[i][1]-'A'); i++;
          edfChnName[idx]=argv[i];
          split_str(argv[i],name,wl);
          window_len[idx]=strtod(wl,NULL);
          if (window_len[idx]<=0.0) { window_len[idx]= 4.0; }
          break;
        case 'l':
          lbl=strtol(argv[++i],NULL,0);
          /* copy default channel names */
          for (idx=0; idx<NR_OF_CHANNELS; idx++) {
            switch (lbl) { 
              case 2:    
                edfChnName[idx]=(char *)DefName2[idx]; break;
              default:    
                edfChnName[idx]=(char *)DefName1[idx]; break;
            }   
          }
          break;
        default : fprintf(stderr,"# Error: can't understand argument %s\n",argv[i]);
      }
    }
  }
  
  if (vb&0x02) {
    fprintf(stderr,"# channel selection: 0x%04X\n",chn);
    for (i=0; i<NR_OF_CHANNELS; i++) {
      if (chn&(1<<i)) {
        fprintf(stderr,"# channel %1C %s\n",('A'+i),edfChnName[i]);
      }
    }
  }
}

/* of course, Microsoft has their own way of handling signals... */
#ifdef _MSC_VER
BOOL sig_handler(DWORD ctrltype)
#else
void sig_handler(int32_t sig) 
#endif
{
  fprintf(stderr, "# Info: Received Ctrl+C, stopping capture\n");
  pressed_CtrlC = 1;
#ifdef _MSC_VER
  return TRUE;
#endif
}

/** Initialize tms channel data array from 'msg'
 * @return timestamp
 */
double init_tms_data(char *msg) {
  
  int32_t  i,j,k;
  char    *token;
  double   t;      /**< timestamp of this packet */
  
  /* parse 'msg' string */
  token=strtok(msg," ="); i=0;
  while (token!=NULL) {
    if (vb&0x04) { fprintf(stderr,"%d %s\n",i,token); }
    if (i%2==0) {
      /* label */
      if (token[0]=='t') { 
        j=-1; /* time */ 
      } else { 
        /* channel A ... N */
        j=token[0]-'A';
        if (j>=NR_OF_CHANNELS) {
          fprintf(stderr,"# Warning: channel %c is out of range A...N\n",token[0]);
        }
        k=strtol(&token[1],NULL,0);
        if (vb&0x04) { fprintf(stderr,"j %d k %d\n",j,k); }
        channel[j].ns=k+1;
      }
    } else {      
      /* value */
      if (j==-1) { 
        t=strtof(token,NULL); 
      }
    }
    token=strtok(NULL," ="); i++;
  }
  
  /* allocate storage space for all values per channel */
  for (j=0; j<NR_OF_CHANNELS; j++) {
    channel[j].rs=0; channel[j].sc=0; channel[j].td=1.0;
    channel[j].data=(tms_data_t *)calloc(channel[j].ns,sizeof(tms_data_t));
  }
  
  return(t);
}

/** Get tms channel data array from 'msg'
 * @return timestamp
 */
double get_tms_data(char *msg) {
  
  int32_t  i,j,k;
  char    *token;
  double   t;      /**< timestamp of this packet */
  
  /* reset received symbol counter of all channels */
  for (j=0; j<NR_OF_CHANNELS; j++) { channel[j].rs=0; }
  
  /* parse 'msg' string */
  token=strtok(msg," ="); i=0;
  while (token!=NULL) {
    if (vb&0x04) { fprintf(stderr,"%d %s\n",i,token); }
    if (i%2==0) {
      /* label */
      if (token[0]=='t') { 
        j=-1; /* time */ 
      } else { 
        /* channel A...N */
        j=token[0]-'A';
        k=strtol(&token[1],NULL,0);
        if (vb&0x04) { fprintf(stderr,"j %d k %d\n",j,k); }
      }
    } else {      
      /* value */
      if (j==-1) { 
        t=strtod(token,NULL); 
      } else {
        if ((j>=0) && (j<NR_OF_CHANNELS) && (k>=0) && (k<channel[j].ns)) {
          channel[j].data[k].sample=strtof(token,NULL);
          channel[j].rs=k+1; channel[j].sc++;
        }
      }
    }
    token=strtok(NULL," ="); i++;
  }
  
  return(t);
}

/** Set tick duration per channel from packet duration 'dt'
 * @return 0 always
 */
int32_t set_tick_duration(double dt) {
  
  int32_t j;
  
  /* set tick duraction for channel 'j' */
  for (j=0; j<NR_OF_CHANNELS; j++) {
    channel[j].td=dt/channel[j].ns;
  }
  return(0);
}


int32_t plot_samples(int32_t argc, char *argv[]) {

  int32_t  lc=0;             /**< line counter */
  char     msg[MNCIPP];      /**< IP address to listen to */
  double   t0,t1,t;          /**< (start) timestamp */
  int32_t  i,j;              /**< general indexs */
  float    sample;           /**< current sample */
  
  /* don't start right away, but give the UI some time to build */
  usleep( 500*1000);
  
  Client client(ipname,port);

  while (!pressed_CtrlC) {
    strncpy(msg,client.getMessage().c_str(),sizeof(msg)-1);
    if (vb&0x01) { /* show IP traffic */
      fprintf(stderr,"%s",msg);
    }
    if (msg[1]!='t') { continue; } 
    if (lc==0) { 
      /* init tms data structure with first IP packet and get start timestamp */
      t0=init_tms_data(msg); 
      fprintf(stderr,"# Info: start time %lf [s]\n",t0);
    } else {
      t=get_tms_data(msg);
      if (vb&0x08) { fprintf(stderr,"# Info: t %lf [s] dt %lf [s]\n",t,t-t1); }
      if (lc==1) { 
        fprintf(stderr,"# Info: packet frequency %lf [Hz]\n",1.0/(t-t0));
        /* set tick duration per channel */
        set_tick_duration(t-t0); 
      }
    }
    
    if (vb&0x08) { tms_prt_channel_data(stderr,channel,NR_OF_CHANNELS,1); }
    
    /* plot all received samples */
    for (j=0; j<NR_OF_CHANNELS; j++) {
      if (chn&(1<<j)) {
        for (i=0; i<channel[j].rs; i++) {
          sample=channel[j].data[i].sample;
          if (!isnan(sample)) { 
            if (vb&0x10) {
              fprintf(stderr,"# Info: chn %d i %d t %.6f dt\n",j,i,t+i*channel[j].td);
            }
            plot[j]->AddSample( t+i*channel[j].td, sample,  0 );
          }
        }
      }
    }
     
    lc++; t1=t;
  }
  
  return(0);
}

int32_t main(int32_t argc, char *argv[]) {

  int32_t  lc=0;             /**< line counter */
  char     title[MNCIPP];    /**< Window title */
  double   t0,t1,t;          /**< (start) timestamp */
  int32_t  i,j,k;
  int32_t  chn_cnt=0;        /**< active channel count */
  int32_t  plot_per_row=3;   /**< number of plots per row */
  
  parse_cmd(argc,argv);

  // start the biodata collection in a separate thread
  boost::thread thread_data( boost::bind(plot_samples, argc, argv) );

  // start the ui application
  main_app = new QApplication(argc,argv);
 
  /* Build UI */
  for (j=0; j<NR_OF_CHANNELS; j++) {
    plot[j] = new CBioPlot( 1, window_len[j], edfChnName[j] );
  }

  QGridLayout *myLayout = new QGridLayout;
  myLayout->setHorizontalSpacing( 16 );
  myLayout->setVerticalSpacing  (  6 );
  myLayout->setContentsMargins(0,0,0,0);

  /* count active channels */
  chn_cnt=0;
  for (j=0; j<NR_OF_CHANNELS; j++) {
    if (chn&(1<<j)) { chn_cnt++; }
  } 
  fprintf(stderr,"# chn 0x%04X chn_cnt %d\n", chn, chn_cnt);
  
  if (chn_cnt<5) { plot_per_row=2; } else { plot_per_row=3; }
  /* Add only wanted channel to myLayout */
  i=0; k=0;
  for (j=0; j<NR_OF_CHANNELS; j++) {
    if (chn&(1<<j)) {
      myLayout->addWidget( plot[j], k, i );
      i++; if (i==plot_per_row) { i=0; k++; }
    }
  } 

  myLayout->setRowStretch( 0, 1 );
  if (chn_cnt>3) { myLayout->setRowStretch( 1, 1 ); }
  if (chn_cnt>6) { myLayout->setRowStretch( 2, 1 ); }

  QFrame * my_frame = new QFrame();
  my_frame->setLayout( myLayout );

  main_window = new QMainWindow();
  main_window->resize(1000,800);
  main_window->setCentralWidget( my_frame );
  main_window->show();
  //main_window->showMaximized();
  snprintf(title,sizeof(title)-1,"%s: IP %s port %d","Nexus IP Viewer",ipname,port);
  main_window->setWindowTitle(title);
 
  main_app->exec();

  // clean up the UI part
  delete main_app;

  // stop the hal data collection process
  pressed_CtrlC = 1;

  thread_data.join();

  return(0);
}
