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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#include "tmsi.h"
#include "tmsi_bluez.h"

#define MNCN                 (1024)  /**< Maximum number of characters in file name */

#define INDEF   "00:A0:96:1B:48:BC"  /**< default bluetooth address */
#define OUTDEF  "%Y%m%d_%H%M%S.txt"  /**< default output filename   */
#define CHNDEF                  (0)  /**< default swith report channel */
#define LOGDEF          "nexus.log"  /**< default logging filename  */
#define SDDEF                 (5.0)  /**< default maximum number of samples */
#define SRDDEF                  (0)  /**< default log2 of sample rate divider */

#define VERSION "$Revision: 0.2 $ $Date: 2008/01/07 16:30:00 $"

int32_t dbg=0x0000;                      /**< debug level */
int32_t  vb=0x0000;                      /**< verbose level */

#define NR_OF_CHANNELS   (9)
static const char *edfChnName[NR_OF_CHANNELS] = {"C3-A1","C4-A2","VEOG","HEOG","ECG","Disp","GSR","Resp","Switch"};

double ts[12];

/** Print usage to file 'fp'.
 *  @return number of printed characters.
*/
int32_t single_channel_intro(FILE *fp) {
  
  int32_t nc=0;
  int32_t i;
  
  nc+=fprintf(fp,"single_channel: %s\n",VERSION); 
  nc+=fprintf(fp,"Usage: single_channel [-i <in>] [-o <out>] [-c <chn>] [-t <sd>] [-s <srd>] [-l <log>] [-v <vb>] [-d <dbg>] [-h]\n");
  nc+=fprintf(fp,"  Press CTRL+c to stop capturing bio-data\n");
  nc+=fprintf(fp,"in   : bluetooth address (default=%s)\n",INDEF);
  nc+=fprintf(fp,"out  : output report file (default=%s)\n",OUTDEF);
  nc+=fprintf(fp,"chn  : channel report selection [0..13] (default=%d)\n",CHNDEF);
  nc+=fprintf(fp,"        chn signal\n");
  for (i=0; i<NR_OF_CHANNELS; i++) {
    nc+=fprintf(fp,"       %2d   %s\n",(i<NR_OF_CHANNELS-1 ? i : 13),edfChnName[i]);
  }
  nc+=fprintf(fp,"srd  : log2 of sample rate divider: fs=2048/(1<<srd) (default=%d)\n",SRDDEF);
  nc+=fprintf(fp,"sd   : sampling duration (0.0 = forever) (default=%6.3f)\n",SDDEF);
  nc+=fprintf(fp,"log  : packet logging file (default=%s)\n",LOGDEF);
  nc+=fprintf(fp,"h    : show this manual page\n");
  nc+=fprintf(fp,"vb   : verbose switch (default=0x%02X)\n",vb);
  nc+=fprintf(fp,"        0x01 : log all packets in log\n");
  nc+=fprintf(fp,"dbg  : debug value (default=0x%02X)\n",dbg);
  return(nc);
}

void parse_cmd(int32_t argc, char *argv[], char *iname, char *oname, char *lname,
 int32_t *srd, int32_t *chn, double *sd) {

  int32_t i;          /**< general index */
  time_t now;
  struct tm t0;
  
  strcpy(iname,INDEF); strcpy(oname,""); strcpy(lname,LOGDEF);
  *srd=SRDDEF; *chn=CHNDEF; *sd=SDDEF; 
  
  for (i=1; i<argc; i++) {
    if (argv[i][0]!='-') {
      printf("missing - in argument %s\n",argv[i]);
    } else {
      switch (argv[i][1]) {
        case 'i': strcpy(iname,argv[++i]); break;
        case 'o': strcpy(oname,argv[++i]); break;
        case 'l': strcpy(lname,argv[++i]); break;
        case 'c': *chn=strtol(argv[++i],NULL,0); break;
        case 't': *sd=strtod(argv[++i],NULL); break;
        case 's': *srd=strtol(argv[++i],NULL,0); break;
        case 'v': vb=strtol(argv[++i],NULL,0); break;
        case 'd': dbg=strtol(argv[++i],NULL,0); break;
        case 'h': single_channel_intro(stderr); exit(0); break;
        default : printf("can't understand argument %s\n",argv[i]);
      }
    }
  }
  if (*srd<0) { *srd=0; }
  if (*srd>4) { *srd=4; }
  
  if (strlen(oname)<1) {
    /* use day and current time as file name */
    time(&now); t0=*localtime(&now);
    strftime(oname,MNCN,"%Y%m%d_%H%M%S.txt",&t0);
  }

}

void sig_handler(int32_t sig)  {

  fprintf(stdout,"# Stop grabbing with signal %d\n",sig);
  tms_shutdown();
  /* close bluetooth socket */
  tms_close_port();
  tms_close_log();
  exit(sig);
}

int32_t print_ts() {
 
  int32_t i;
  int32_t nc=0;
  
  for (i=1; i<12; i++) {
    if (ts[i]>0.0) {
      nc+=fprintf(stderr,"ts[%1d] %12.6f dt %12.6f\n",i,ts[i]-ts[0],ts[i]-ts[i-1]);
    }
  }
  return(nc);
}

int32_t main(int32_t argc, char *argv[]) {

  int32_t fd;                    /**< bluetooth socket file descriptor */
  FILE *fp=NULL;                 /**< report file pointer */
  FILE *fpl=NULL;                /**< log file pointer */
  char iname[MNCN];              /**< bluetooth address */
  char oname[MNCN];              /**< output report file */
  char lname[MNCN];              /**< output logging file */
  int32_t srd;                   /**< log2 of sample rate divider */
  int32_t chn;                   /**< channel switch */
  double  sd;                    /**< maximum number of samples */
  tms_channel_data_t *channel;   /**< channel data */
  int32_t i,j;
  int32_t mpc=0;                 /**< missed packet count (>=0) or error code (<0) */
  int32_t lost=0;                /**< lost connection counter */
  
  struct sigaction sig_action;
  memset( &sig_action, 0, sizeof(sig_action) );
  sig_action.sa_handler = sig_handler;
  sigaction( SIGINT, &sig_action, NULL );

  /* parse command line arguments */
  parse_cmd(argc,argv,iname,oname,lname,&srd,&chn,&sd);
  
  fprintf(stderr,"oname %s\n",oname);

  /* set debug level in tms module */
  tms_set_vb(vb);
  
  memset(ts,0,sizeof(ts));
  
  ts[0]=get_time();
 
  /* open log file for verbose output */
  if (vb>0) {
    fpl=tms_open_log(lname,"w");
  }
  
  ts[1]=get_time();
  /* open bluetooth socket */
  if ((fd=tms_open_port(iname))<0) {
    fprintf(stderr,"# Error: Can't connect to Nexus!\n");
    print_ts(); exit(-1);
  } 
  ts[2]=get_time();

  ts[3]=get_time();
  /* start capture */
  if (tms_init(fd,srd)<0) {
    fprintf(stderr,"# Error: Can't initialize Nexus!\n");
    print_ts(); exit(-1);
  }
  ts[4]=get_time();
 
  channel=tms_alloc_channel_data();
  if (channel==NULL) {
    fprintf(stderr,"# main: tms_alloc_channel_data problem!!basesamplerate!\n");
    print_ts(); exit(-1);
  }
 
  if (chn > tms_get_number_of_channels() || chn < 0) {
    fprintf(stderr,"Incorrect channel number %d\n", chn);
    print_ts(); exit(-1);
  }
  ts[5]=get_time();

  fprintf(stderr,"Write data values to file: %s\n",oname);
  if (strcmp(oname,"stdout")==0) {
    fp=stdout;
  } else
  if ((fp=fopen(oname,"w"))==NULL) {
    perror(oname); print_ts(); exit(-1);
  }
    
  ts[6]=get_time();

  fprintf(fp,"#%9s %8s %1c%8s %1c%8s %2s\n",
             "t[s]", "nr", 'A'+chn, "sample", 'A'+chn, "isample" , "fl" );
  j=0;
  /* grab sample to 'sd' seconds or forever if 'sd==0.0' */
  while ((sd==0.0) || (tms_elapsed_time(channel) < sd)) {
  
    if (j+7<12) { ts[j+7]=get_time(); } j++;
   
    mpc=tms_get_samples(channel);
    if (mpc<0) {
      fprintf(stderr,"# Error: connection lost !!!\n");
      usleep(1000*1000);
      lost++;
      if (lost>4) { break; }
      continue;
    } else {
      for (i=0; i<channel[chn].rs; i++) {
        fprintf(fp," %9.4f %8d %9g %9d %2d\n",
               (channel[chn].sc+i)*channel[chn].td, 
               channel[chn].sc+i, 
               channel[chn].data[i].sample,
               channel[chn].data[i].isample, 
               channel[chn].data[i].flag);
      }
    }
  }
  
  print_ts();
   
  fclose(fp);
  /* shutdown bluetooth capture */
  tms_shutdown();
  /* close bluetooth socket */
  tms_close_port();
  /* close log file */
  tms_close_log();

  return(0);
}
