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
#include <unistd.h>
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
#include "Message.h"
#include "RunningAverage.h"

#define MNCIPP               (1536)  /**< Maximum number of characters in IP Packet */

#ifdef _MSC_VER
  #define BTDEF "00:A0:96:1B:49:21"  /**< default bluetooth address for nexus1/NBF4 */
#else
  #define BTDEF "00:A0:96:1B:44:C6"  /**< default bluetooth address */
#endif
#define PORTDEF             (16000)  /**< default port */
#define IDDEF                "pp00"  /**< default measurement name */
#define CHNDEF          "ABCDEFGHM"  /**< default channel switch */
#define SDDEF                 (0.0)  /**< default sample time [s] 0.0 == forever */
#define SRDDEF                  (0)  /**< default log2 of sample rate divider */
#define MDDEF                   (0)  /**< default print switch 0: float 1: integer */

#define VERSION "$Revision: 0.5 $ $Date: 2012/08/03 16:40:00 $"

int32_t dbg=0x0000;     /**< debug level */
int32_t  vb=0x0000;     /**< verbose level */
double   fs=2048.0;     /**< default sample frequence [Hz] */
FILE    *fp=NULL;       /**< TEXT file pointer */
FILE   *fpe=NULL;       /**< EDF file pointer */
FILE   *fpl=NULL;       /**< log file pointer */
int32_t battery_low=0;  /**< battery low counter */
int32_t rec_cnt=0;      /**< EDF/BDF record counter */

volatile int pressed_CtrlC = 0;

#define NR_OF_CHANNELS   (14)
static const char *chndef_str=CHNDEF;
static const char *DefName[NR_OF_CHANNELS] = {"A","B","C","D","E","F","G","H","I","J","K","L","M","N"};
const char *edfChnName[NR_OF_CHANNELS]; 

/** Print usage to file 'fp'.
 *  @return number of printed characters.
*/
int32_t tmsi_server_intro(FILE *fp) {
  
  uint32_t i,nc=0;
  
  nc+=fprintf(fp,"tmsi_server: %s\n",VERSION); 
  nc+=fprintf(fp,"Usage: tmsi_server [-a <in>] [-p <port>] [-i <id>] [-o <out>] [-b <bdf>] [-m <md>] [-c <CHN>]\n");
  nc+=fprintf(fp,"   [-A <A>] [-B <B>] ... [-t <sd>] [-s <srd>] [-v <vb>] [-d <dbg>] [-h]\n");
  nc+=fprintf(fp,"  Press CTRL+c to stop capturing bio-data\n");
  nc+=fprintf(fp,"in   : bluetooth address (default=%s)\n",BTDEF);
  nc+=fprintf(fp,"port : port number (default=%d)\n",PORTDEF);
  nc+=fprintf(fp,"id   : measurement id (default=%s)\n",IDDEF);
  nc+=fprintf(fp,"sd   : sampling duration [s] (0.0 == forever) (default=%6.3f)\n",SDDEF);
  nc+=fprintf(fp,"srd  : log2 of sample rate divider: fs=2048/(1<<srd) (default=%d)\n",SRDDEF);
  nc+=fprintf(fp,"out  : TEXT output file, default is none\n");
  nc+=fprintf(fp,"bdf  : BDF output file (default=<id>_<yyyymmddThhmmss>.bdf)\n");
  nc+=fprintf(fp,"CHN  : channel selection string (default=%s)\n",CHNDEF);
  for (i=0; i<strlen(chndef_str); i++) {
    nc+=fprintf(fp," %1c  : signal name of channel %1C (default=%s)\n",chndef_str[i],chndef_str[i],DefName[i]);
  }
  nc+=fprintf(fp,"md   : print switch 0:float 1:integer samples (default=%d)\n",MDDEF);
  nc+=fprintf(fp,"h    : show this manual page\n");
  nc+=fprintf(fp,"vb   : verbose switch (default=0x%02X)\n",vb);
  nc+=fprintf(fp,"        0x01 : show all IP traffic\n");
  nc+=fprintf(fp,"        0x02 : show signal channel names\n");
  nc+=fprintf(fp,"        0x04 : show channel packets\n");
  nc+=fprintf(fp,"        0x08 : show EDF/BDF header\n");
  nc+=fprintf(fp,"        0x10 : show packet arrival time\n");
  nc+=fprintf(fp,"dbg  : debug value (default=0x%02X)\n",dbg);
  return(nc);
}

void parse_cmd(int32_t argc, char *argv[], char *btname, int32_t *port, int32_t* md,
 int32_t *chn, double *sd, int32_t *srd, char *id, char *bname, char *oname) {

  int32_t i,idx;          /**< general index */
  time_t now;
  struct tm t0;
  char   name[MNCN];
  
  strcpy(btname,BTDEF); strcpy(id,IDDEF); strcpy(bname,"DEFAULT"); strcpy(oname,"");
  *chn=tms_chn_sel((char *)CHNDEF); *sd=SDDEF; *srd=SRDDEF; *port=PORTDEF;
  
  /* copy default channel names */
  for (idx=0; idx<NR_OF_CHANNELS; idx++) {
    edfChnName[idx]=(char *)DefName[idx];
  }
  
  for (i=1; i<argc; i++) {
    if (argv[i][0]!='-') {
      printf("missing - in argument %s\n",argv[i]);
    } else {
      switch (argv[i][1]) {
        case 'a': strcpy(btname,argv[++i]); break;
        case 'i': strcpy(id,argv[++i]); break;
        case 'b': strcpy(bname,argv[++i]); break;
        case 'o': strcpy(oname,argv[++i]); break;
        case 'c': *chn=tms_chn_sel(argv[++i]); break;
        case 'p': *port=strtol(argv[++i],NULL,0); break;
        case 'm': *md=strtol(argv[++i],NULL,0); break;
        case 't': *sd=strtod(argv[++i],NULL); break;
        case 's': *srd=strtol(argv[++i],NULL,0); break;
        case 'v': vb=strtol(argv[++i],NULL,0); break;
        case 'd': dbg=strtol(argv[++i],NULL,0); break;
        case 'h': tmsi_server_intro(stderr); exit(0); break;
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
          idx=(int32_t)(argv[i][1]-'A');
          edfChnName[idx]=argv[++i];
          break;
        default : printf("can't understand argument %s\n",argv[i]);
      }
    }
  }
  
  /* clip sample rate divider */
  if (*srd<0) { *srd=0; }
  if (*srd>4) { *srd=4; }
  
  if (vb&0x02) {
    fprintf(stderr,"# channel selection: 0x%04X\n",*chn);
    for (i=0; i<NR_OF_CHANNELS-1; i++) {
      if ((*chn)&(1<<i)) {
        fprintf(stderr,"# channel %1C %s\n",('A'+i),edfChnName[i]);
      }
    }
    if ((*chn)&(1<<12)) {
      fprintf(stderr,"# channel %1C %s\n",'M',edfChnName[NR_OF_CHANNELS-1]);
    }
  }

  /* use day and current time as file name */
  time(&now); t0=*localtime(&now);
  if ((strcmp(bname,"DEFAULT")==0) && (strlen(id)>1)) {
    strftime(name,MNCN,"%Y%m%dT%H%M%S",&t0);
    sprintf(bname,"%s_%s.bdf",id,name);
  }
}

/* of course, Microsoft has their own way of handling signals... */
#ifdef _MSC_VER
BOOL sig_handler( DWORD ctrltype )
#else
void sig_handler(int sig) 
#endif
{
  fprintf(stderr, "# Info: Received Ctrl+C, stopping capture\n");
  pressed_CtrlC = 1;
#ifdef _MSC_VER
  (void) ctrltype;
  return TRUE;
#else
  (void) sig;
#endif
}

int32_t main(int32_t argc, char *argv[]) {

  int32_t fd;                    /**< bluetooth socket file descriptor */
  char    btname[MNCN];          /**< bluetooth address */
  char    id[MNCN];              /**< measurement id */
  char    oname[MNCN];           /**< text output file name */
  char    bname[MNCN];           /**< BDF output file name */
  int32_t port;                  /**< port number */
  int32_t chn;                   /**< channel switch */
  double  sd;                    /**< sampling duration [s] */
  int32_t srd;                   /**< log2 of sample rate divider */
  tms_channel_data_t *channel;   /**< channel data */
  int32_t mpc=0;                 /**< missed packet counter */
  int32_t i,j;                   /**< general index */
  std::string msg;               /**< message to ip server */
  double t;                      /**< current time */
  FILE   *fp =NULL;              /**< text log file */
  FILE   *fpe=NULL;              /**< EDF/BDF log file */
  int32_t md;                    /**< print switch 0: float 1: integer */
  time_t  now;                   /**< now */
  char buff[MNCN];               /**< temporary buffer for text */
  int32_t dev=1;                 /**< device 1:Nexus 2:Holst (not implemented yet) 3:BDF/EDF */
  edf_t   edf;                   /**< EDF/BDF data structure */
  FILE   *fpi;                   /**< EDF/BDF input file pointer */
  int32_t lost=0;                /**< total packets lost */
  double  wct,wct0,wct1,wct2;    /**< wall clock time */ 
  int32_t sw_chn=12;             /**< switch channel number */  
  int32_t chn_cnt=8;             /**< total number of channels */
  
#ifdef _MSC_VER
  if (SetConsoleCtrlHandler( (PHANDLER_ROUTINE) sig_handler,TRUE)<=0) {
    fprintf(stderr,"# Error in SetConsoleCtrlHandler\n");
    return(-1);
  }
#else
  (void)signal(SIGINT,sig_handler);
#endif

  /* parse command line arguments */
  parse_cmd(argc,argv,btname,&port,&md,&chn,&sd,&srd,id,bname,oname);

  Server server(port);

  if (strstr(btname,"/dev/tty.")!=NULL) { dev=1; /* Nexus */    } else
  if (strchr(btname,'/'        )!=NULL) { dev=3; /* filename */ } else
  if (strchr(btname,'\\'       )!=NULL) { dev=3; /* filename */ } else
  if (strstr(btname,":"        )!=NULL) { dev=1; /* Nexus */    } else
  if (strstr(btname,"USB"      )!=NULL) { dev=2; /* Holst */    } else {
                                          dev=3; /* EDF/BDF input */
  }

  switch (dev) {
    case 1: /* Live Nexus input */
      /* set debug level in tms module */
      tms_set_vb(vb>>8);

      fprintf(stderr,"# Opening bluetooth port on MAC address %s\n",btname);
      if ((fd=tms_open_port(btname))<0) {
        fprintf(stderr, "# Error: Couldn't open nexus port\n");
        exit(2);
      }

      tms_init(fd,srd);

      channel=tms_alloc_channel_data();
      if (channel==NULL) {
        fprintf(stderr,"# Error: tms_alloc_channel_data problem!! basesamplerate!\n");
        exit(-1);
      }
      
      /* sample frequency */
      fs=tms_get_sample_freq();
      fprintf(stderr,"# Info: fs %.1f device %s\n", fs, tms_get_device_name());
      
      /* number of channels */
      chn_cnt=tms_get_number_of_channels();
      /* limit channel selection to maximum of channels */
      chn &= (1<<chn_cnt)-1;

      /* switch channel of Nexus */
      sw_chn=chn_cnt-2; 
      break;
      
    case 3: /* BDF/EDF input */
      fprintf(stderr,"# Open EDF/BDF file %s\n",btname);
      if ((fpi=fopen(btname,"rb"))==NULL) {
        perror(NULL); exit(-1);
      }
      // edf_set_vb(255);
      /* read EDF/BDF main and signal headers */
      edf_rd_hdr(fpi,&edf);
      // edf_prt_hdr(stderr,&edf);
      /* read EDF/BDF samples */
      edf_rd_samples(fpi,&edf);
      /* close input file */
      fclose(fpi);
      fprintf(stderr,"# %d EDF/BDF datarecord read\n",edf.NrOfDataRecords);

      /* allocate space for data samples */
      if ((channel=edf_alloc_channel_data(&edf))==NULL) {
        fprintf(stderr,"# Error: alloc_channel_data problem!\n");
        exit(-1);
      }

      /* switch channel nr */
      /* Not sure why this is needed;  commented out because Marjolein uses bdfs with only 2 recorded channels */
      if ((sw_chn=edf_fnd_chn_nr(&edf,"Switch"))>=0) {
        fprintf(stderr,"# Info: Found signal 'Switch' at channel nr %d\n",sw_chn);
      }
      
      /* channel count */
      chn_cnt=edf.NrOfSignals;
      
      /* read input file to the end if default duration is used */
      if (sd==SDDEF) { sd=0.0; }

      /* Use sample frequency of channel '0' */
      fs=edf.signal[0].NrOfSamplesPerRecord / edf.RecordDuration;
      break;
    default:
      fprintf(stderr,"# Error: missing device %d\n",dev);
      exit(-1);
      break;
  }
  fprintf(stderr,"# Info: channel count %d chn 0x%04X\n",chn_cnt,chn);
  
  if (strlen(oname)>1) {
    fprintf(stderr,"# write data to text file: %s\n",oname);
    if ((fp=fopen(oname,"w"))==NULL) {
      perror(oname);
    }
  }

  /* only with live recording a BDF output file is needed */
  if (((dev==1) || (dev==2)) && (strlen(bname)>1)) {
    fprintf(stderr,"# write data to BDF file: %s\n",bname);
    if ((fpe=fopen(bname,"wb"))==NULL) {
      perror(bname);
    }
    /* use current wall clock time as start time of BDF file */
    time(&now);
    if (vb&0x08) {
      fprintf(stderr,"# Info: fpe %p channel %p chn_cnt %d chn 0x%04X bname %s dt %.5f edfname %s\n",
       fpe,channel,chn_cnt,chn,bname,(channel[0].ns/fs),edfChnName[0]);
    }
    /* write EDF/BDF headers */
    edfWriteHdr(fpe,channel,chn_cnt,chn,bname,"Nexus-10 via tmsi_server",
        &now,(channel[0].ns/fs),edfChnName);
  }
  
  /* start timestamp wall clock time [s] since 1-1-1970 00:00:00 */
  wct0=get_time(); wct=0.0; lost=0;
  if (vb&0x10) {
    fprintf(stderr," %s %s\n", "cnt", "ta");
  }
  while ( ((sd==0.0) || (((wct2=get_time())-wct0) < sd))  && !pressed_CtrlC) {
 
    /* get samples, (print text) and edf/bdf file */
    switch (dev) {
      case 1: /* Nexus */
        mpc=tms_get_samples(channel);
        if (vb&0x10) {
          fprintf(stderr," %d %.6f\n", rec_cnt, get_time()-wct0);
        }
        if (mpc<0) {
          lost++; wct1=get_time(); wct=wct1-wct0;
          fprintf(stderr,"# Error: %d connection lost at %9.3f [s] Wait 0.1 [s]!\n",lost,wct);
          usleep(100*1000);
          /* insert gap to indicate missing packets */
          mpc = (int32_t)round((get_time() - wct1)*fs/channel[0].ns);
 
          if (lost>7) {
             //pressed_CtrlC = 1; continue;
            fprintf(stderr,"# Error: connection reset, wait 5 [s]\n");
            /* shutdown bluetooth capture */
            tms_shutdown();  
            /* wait 5 [s] */
            usleep(5000*1000);
            /* close bluetooth socket */
            tms_close_port();
            
            fprintf(stderr,"# Reopening bluetooth port on MAC address %s\n",btname);
            while ((fd=tms_open_port(btname))<0) {
               fprintf(stderr, "# Error: Couldn't open nexus port. Wait 5 [s]\n");
               if (pressed_CtrlC == 1) { continue; }
               usleep(5000*1000);
            }
            /* initialize tms data capture */
            tms_init(fd,srd);
            lost = 0;
            
            /* insert gap to indicate missing packets */
            mpc = (int32_t)round((get_time() - wct1)*fs/channel[0].ns);
          }
        }
        /* check if we missed packets */
        if (mpc>0) {
          fprintf(stderr,"# Info: inserted %d packets after connection loss!!\n",mpc);
          /* flag all samples in this packet with 'dynamic overflow' */
          tms_flag_samples(channel,chn_cnt,0x02);
        }
        if (fpe!=NULL) {
          /* write samples to BDF output file */
          edfWriteSamples(fpe,1,channel,chn,mpc);
          /* flush BDF output file to see progress and spread NFS load */
          fflush(fpe);
        }
        rec_cnt+=mpc+1;
        break;

      case 3: /* get next block of EDF/BDF samples */
        edf_rd_chn(&edf,channel);
        /* wait for wall clock time to pass */
        usleep((int32_t)round(1e6*channel[0].ns/fs) );
        /* input file should have no missing samples */
        mpc=0;
        /* if channel block is incomplete the input file has ended */
        if (channel[0].rs<channel[0].ns) {
          pressed_CtrlC = 1; continue;
        }
        break;

      default:
        break;
    }  
    
    if (vb&0x04) {
      tms_prt_channel_data(stderr,channel,chn_cnt,1);
    }

    /* calculate current time [s] out of channel A or '0' */
    t=channel[0].sc*channel[0].td;
        
    /* write samples to TEXT output file */
    if (fp!=NULL) {
      tms_prt_samples(fp,channel,chn,md,chn_cnt);
    }

    if ((sw_chn>=0) && (tms_chk_battery(channel,sw_chn)==1)) {
      if (battery_low==0) {
        fprintf(stderr,"# Warning: battery low at %.3f [s]\n",t);
      }
      battery_low++;
    }

    msg.clear();
    snprintf(buff,sizeof(buff)-1," %.8f",t);
    msg += buff;
    for (j=0; j<chn_cnt; j++) {
      /* is channel 'j' selected */
      if (chn&(1<<j)) {
        /* write all samples of channel 'j' */
        for (i=0; i<channel[j].ns; i++) {
          if (i<channel[j].rs) {
            /* write value of sample 'i' */
            snprintf(buff,sizeof(buff)-1," %.4f",channel[j].data[i].sample);
            msg += buff;
          }
        }
      }
    }
    msg += "\n";
    if (vb&0x01) {
      fprintf(stderr,"%s",msg.c_str());
    }
    /* send msg */
    server.sendMessage(msg.c_str());
  }

  if (battery_low>0) {
    fprintf(stderr,"# Warning: %d times battery low encountered\n",battery_low);
  }

  /* close text output */
  if (fp!=NULL) { fclose(fp); }

  /* update record counter and close BDF output file */
  if (fpe!=NULL) { 
    fprintf(stderr,"# Info: %d EDF/BDF records written\n",rec_cnt);
    /* set record counter in BDF output file */
    edf_set_record_cnt(fpe,rec_cnt);
    fclose(fpe); 
  }
  
  if (dev==1) {
    /* shutdown bluetooth capture */
    tms_shutdown();  
    /* close bluetooth socket */
    tms_close_port();
  }

  return(0);
}
