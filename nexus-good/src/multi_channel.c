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
  #include "stdint_win32.h"
#else
  #include <stdint.h>
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <signal.h>

#include "tmsi.h"
#include "tmsi_bluez.h"
#include "edf.h"

#define MNCN                 (1024)  /**< Maximum number of characters in file name */

#define OUTDEF                   ""  /**< default text output file name */
#define IDDEF                "pp00"  /**< default measurement id */
#define BDFDEF      "%Y%m%dT%H%M%S"  /**< default BDF output file name */
#define CHNDEF          "ABCDEFGHI"  /**< default channel switch */
#define LOGDEF          "nexus.log"  /**< default logging filename  */
#define SDDEF                 (5.0)  /**< default sample time [s] */
#define SRDDEF                  (1)  /**< default log2 of sample rate divider */
#define MDDEF                   (0)  /**< default print switch 0: float 1: integer */

#define VERSION "$Revision: 0.3 $ $Date: 2009/01/19 16:30:00 $"

int32_t dbg=0x0000;  /**< debug level */
int32_t  vb=0x0000;  /**< verbose level */
double   fs=2048.0;  /**< default sample frequence [Hz] */
FILE    *fp=NULL;    /**< TEXT file pointer */
FILE   *fpe=NULL;    /**< EDF file pointer */
FILE   *fpl=NULL;    /**< log file pointer */

int32_t switch_chn_nr =12;  /**< switch channel nr */

int32_t rec_cnt=0;   /**< EDF/BDF record counter */

#define NR_OF_CHANNELS   (9)
static const char *edfChnName[NR_OF_CHANNELS] = {"C3-A1","C4-A2","VEOG","HEOG","ECG","Disp","GSR","Resp","Switch"};

/** Evaluate shell variables in path name 'a' into 'b'
 * @return path depth before evaluation
*/
int32_t get_path(char *b, const char *a) {

  char tmp[MNCN];
  char *dir;
  char *chk;
  int32_t i=0;
  
  strcpy(tmp,a); dir=strtok(tmp,"/"); b[0]='\0';
  while (dir!=NULL) {
    if (dir[0]=='$') {
      dir++; /* skip first character */
      chk=getenv(dir);
      if (chk!=NULL) {
        strcat(b,getenv(dir));
      }
    } else {
      strcat(b,"/"); strcat(b,dir);
    }
    dir=strtok(NULL,"/"); i++;
  }
  return(i);  
}

/** Print usage to file 'fp'.
 *  @return number of printed characters.
*/
int32_t multi_channel_intro(FILE *fp) {
  
  int32_t nc=0;
  int32_t i;
  
  nc+=fprintf(fp,"multi_channel: %s\n",VERSION); 
  nc+=fprintf(fp,"Usage: multi_channel [-a <in>] [-o <out>] [-e <edf>] [-c <CHN>] [-I <id>] [-m <md>] [-t <sd>] [-s <srd>] [-l <log>] [-v <vb>] [-d <dbg>] [-h]\n");
  nc+=fprintf(fp,"  Press CTRL+c to stop capturing bio-data\n");
  nc+=fprintf(fp,"in   : bluetooth address (default=%s)\n","$NEXUS_BADR");
  nc+=fprintf(fp,"id   : measurement id (default=%s)\n",IDDEF);
  nc+=fprintf(fp,"out  : TEXT output file (default=None, txt=timestamped file)\n",OUTDEF);
  nc+=fprintf(fp,"edf  : EDF/BDF output file (default=%s.bdf)\n",OUTDEF);
  nc+=fprintf(fp,"CHN  : channel selection string (default=%s)\n",CHNDEF);
  nc+=fprintf(fp,"        chn signal\n");
  for (i=0; i<NR_OF_CHANNELS; i++) {
    nc+=fprintf(fp,"        %1C   %s\n",'A'+i,edfChnName[i]);
  }
  nc+=fprintf(fp,"md   : print switch 0:float 1:integer samples (default=%d)\n",MDDEF);
  nc+=fprintf(fp,"srd  : log2 of sample rate divider: fs=2048/(1<<srd) (default=%d)\n",SRDDEF);
  nc+=fprintf(fp,"sd   : sampling duration [s] (0.0 is sampling forever) (default=%6.3f)\n",SDDEF);
  nc+=fprintf(fp,"log  : packet logging file (default=%s)\n",LOGDEF);
  nc+=fprintf(fp,"h    : show this manual page\n");
  nc+=fprintf(fp,"vb   : verbose switch (default=0x%02X)\n",vb);
  nc+=fprintf(fp,"        0x01 : log all packets in log\n");
  nc+=fprintf(fp,"dbg  : debug value (default=0x%02X)\n",dbg);
  return(nc);
}

void parse_cmd(int32_t argc, char *argv[], char *iname, char *id, char *oname,
 char *ename, char *lname, int32_t *chn, double *sd, int32_t *srd, int32_t *md) {

  int32_t i;          /**< general index */
  time_t now;
  struct tm t0;
  char   name[MNCN];

  /* use TMSI as default input device */
  get_path(iname,"$NEXUS_BADR");
  if (strlen(iname)<2) {
    fprintf(stderr,"# Error: missing shell variable $NEXUS_BADR !!!\n");
  }
  fprintf(stderr,"# NEXUS_BADR %s\n",iname);
  
  strcpy(oname,""); strcpy(ename,""); strcpy(lname,LOGDEF); strcpy(lname,LOGDEF);
  *chn=tms_chn_sel(CHNDEF); *sd=SDDEF; *srd=SRDDEF; *md=MDDEF; strcpy(id,IDDEF);
  
  for (i=1; i<argc; i++) {
    if (argv[i][0]!='-') {
      printf("missing - in argument %s\n",argv[i]);
    } else {
      switch (argv[i][1]) {
        case 'a': strcpy(iname,argv[++i]); break;
        case 'I': strcpy(id,argv[++i]); break;
        case 'o': strcpy(oname,argv[++i]); break;
        case 'e': strcpy(ename,argv[++i]); break;
        case 'l': strcpy(lname,argv[++i]); break;
        case 'c': *chn=tms_chn_sel(argv[++i]); break;
        case 'm': *md=strtol(argv[++i],NULL,0); break;
        case 't': *sd=strtod(argv[++i],NULL); break;
        case 's': *srd=strtol(argv[++i],NULL,0); break;
        case 'v': vb=strtol(argv[++i],NULL,0); break;
        case 'd': dbg=strtol(argv[++i],NULL,0); break;
        case 'h': multi_channel_intro(stderr); exit(0); break;
        default : printf("can't understand argument %s\n",argv[i]);
      }
    }
  }
// fprintf(stderr,"# channel selection 0x%04X\n",*chn);
  if (*srd<0) { *srd=0; }
  if (*srd>4) { *srd=4; }
  
  /* use day and current time as file name */
  time(&now); t0=*localtime(&now);
//  if (strlen(oname)<1) {
//    strftime(oname,MNCN,"%Y%m%dT%H%M%S.txt",&t0);
//  }
  strftime(name,MNCN,"%Y%m%dT%H%M%S",&t0);
  
  if (strlen(ename)<1) {
    sprintf(ename,"%s_%s.bdf",id,name);
  }
  
  if (strcmp(oname,"txt")==0) {
    sprintf(oname,"%s_%s.txt",id,name);
  }
}

void sig_handler(int32_t sig) {

  fprintf(stdout,"\nStop grabbing with signal %d\n",sig);
#ifndef _MSC_VER
  raise(SIGALRM);
#endif
  tms_shutdown();
  tms_close_log();
  if (fp!=NULL) { fclose(fp); }
  if (fpe!=NULL) { 
    /* set record counter in EDF/BDF output file */
    edf_set_record_cnt(fpe,rec_cnt);
    fclose(fpe); 
  }
  exit(sig);
}


int main(int argc, char *argv[]) {

  int32_t fd;                    /**< bluetooth socket file descriptor */
  char    iname[MNCN];           /**< bluetooth address */
  char    id[MNCN];              /**< measurement id */
  char    oname[MNCN];           /**< Text output report file */
  char    ename[MNCN];           /**< EDF/BDF output report file */
  char    lname[MNCN];           /**< output logging file */
  int32_t chn;                   /**< channel switch */
  double  sd;                    /**< sampling duration [s] */
  int32_t srd;                   /**< log2 of sample rate divider */
  int32_t md;                    /**< print switch 0: float 1: integer */
  tms_channel_data_t *channel;   /**< channel data */
  int32_t mpc=0;                 /**< missed packet counter */
  double  wct,wct0;              /**< wall clock time [s] since 1-1-1970 00:00:00 */
  int32_t blk=0;                 /**< block counter */
  time_t  now;                   /**< now */
  int32_t chk=0;                 /**< battery low check */
  int32_t button;                /**< button status */
  double  tsw=0.0;               /**< rising edge of button */
  int32_t lost=0;                /**< lost connection counter */
  int32_t tsc=0;                 /**< total sample counter */
  
  (void)signal(SIGINT,sig_handler);

  /* parse command line arguments */
  parse_cmd(argc,argv,iname,id,oname,ename,lname,&chn,&sd,&srd,&md);
  
  /* set debug level in tms module */
  tms_set_vb(vb);
  
  /* open log file for verbose output */
  if (vb>0) {
    fpl=tms_open_log(lname,"w");
  }
  
  /* open bluetooth socket */
  if ((fd=tms_open_port(iname))<0) {
    fprintf(stderr,"# Error: Can't connect to Nexus %s\n",iname);
    exit(-1);
  }
  
  /* start capture */
  if (tms_init(fd,srd)<0) {
    fprintf(stderr,"# Error: Can't initialize Nexus %s\n",iname);
    exit(-1);
  }
   
  channel=tms_alloc_channel_data();
  if (channel==NULL) {
    fprintf(stderr,"# Error: tms_alloc_channel_data problem!! basesamplerate!\n");
    exit(-1);
  }
  fs=tms_get_sample_freq();
  
  switch_chn_nr=tms_get_number_of_channels()-2;
  
  fprintf(stderr,"# Info: fs %.1f switch channel nr %d\n", fs, switch_chn_nr);
  
  if (strlen(oname)>1) {
    fprintf(stderr,"# Write data values to TEXT file: %s\n",oname);
    if (strcmp(oname,"stdout")==0) {
      fp=stdout;
    } else
    if ((fp=fopen(oname,"w"))==NULL) {
      perror(oname); exit(-1);
    }
  }

  fprintf(stderr,"# Write data to BDF file: %s\n",ename);
  if ((fpe=fopen(ename,"wb"))==NULL) {
    perror(ename); exit(-1);
  }

  /* use current wall clock time as start time of BDF file */
  time(&now);

  /* write EDF/BDF headers */
  edfWriteHdr(fpe,channel,tms_get_number_of_channels(),chn,ename,
   "General Nexus-10 recording",&now,(channel[0].ns/fs),edfChnName);

  /* start wall clock time */
  wct0=get_time(); wct=0.0;
  blk=0; tsc=0;
  
  while (/*(tms_elapsed_time(channel) < sd) && */(wct < sd)) {
  
    /* get samples */
    mpc=tms_get_samples(channel); 

    /* elapsed wall clock time */
    wct=get_time()-wct0;
    
    if (mpc<0) {
      fprintf(stderr,"# Error: connection lost !!!\n");
      lost++;
//      if (lost>10) { exit(-1); }
      continue;
    }
    tsc += channel[0].rs;
    blk++;

    chk+=tms_chk_battery(channel,switch_chn_nr);
    if (chk==1) {
      fprintf(stderr,"# Battery low %9.3f [s] after start\n",wct);
    }

    button=tms_chk_button(channel,switch_chn_nr,&tsw);
    if (button>0) {
      fprintf(stderr,"# button t %9.3f nr %d\n",tsw,button);
    }

    if (fp!=NULL) {
      /* write samples to TEXT output file */
      tms_prt_samples(fp,channel,chn,md,tms_get_number_of_channels());
    }
    /* write samples to EDF/BDF output file */
    edfWriteSamples(fpe,1,channel,chn,mpc);
    rec_cnt+=mpc+1;
  }

  if (chk>0) {
    fprintf(stderr,"# Battery low for %d times\n",chk);
  }
  /* report BDF file name and block counter */
  fprintf(stdout,"bdf %s\n",ename);
  fprintf(stdout,"blk %d\n",blk);
  fprintf(stdout,"fs %.3f real-fs %.3f\n", fs, tsc/wct);
  
  if (fp!=NULL) { fclose(fp); }
  if (fpe!=NULL) { 
    /* set record counter in EDF/BDF output file */
    edf_set_record_cnt(fpe,rec_cnt);
    fclose(fpe); 
  }

  /* shutdown bluetooth capture */
  tms_shutdown();  
  /* close bluetooth socket */
  tms_close_port(fd);
  /* close log file */
  tms_close_log();

  return(0);
}
