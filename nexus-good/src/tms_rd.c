/** @file tms_rd.c
 *
 * @ingroup NEXUS
 *
 * $Id:  $
 *
 * @brief decode TMSi config and measurements files.
 *
** @Copyright

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
#else
#include <stdint.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#include "tmsi.h"

#define VERSION "$Revision: 0.4 Date 2014-03-15 17:20 $"

#define INDEF     "00000000.SMP"   /**< default input file */
#define CHNDEF    "ABCDEFGHM"      /**< default channel switch */

int32_t lbl  = 2;  /* 1:NFB@UvT 2:NFB@MGGz */

#define NR_OF_CHANNELS   (14)
char *defName1[NR_OF_CHANNELS] = {"C3-A1","C4-A2", "VEOG", "HEOG","ECG","Disp","GSR","Resp","I","J","K","L","Switch","N"};
char *defName2[NR_OF_CHANNELS] = { "C3A1", "C4C3", "A2C4", "A1A2","ECG","Resp","GSR","Disp","I","J","K","L","Switch","N"};

char *edfMsg  = NULL;
char *edfMsg1 = "NFB @ UvT";
char *edfMsg2 = "NFB @ MGGz";
char *pid     = "";

const char *edfChnName[NR_OF_CHANNELS];

int32_t dbg = 0x0000;
int32_t vb  = 0x0000;

/** tms_cfg usage
 * @return number of printed characters.
*/
int32_t tms_cfg_intro(FILE *fp) {
  
  int32_t nc=0;
  int32_t i;
  
  nc+=fprintf(fp,"Convert TMSi config/measurement file to text: %s\n",VERSION);
  nc+=fprintf(fp,"Usage: tms_rd [-i <in>] [-o <bdf>] [-t <txt>] [-l <lbl>] [-m edfMsg]\n");
  nc+=fprintf(fp," [-c <chn>] [-A ChA] [-B ChB] .. [-K ChK] [-P <pid>] [-v <vb>] [-d <dbg>] [-h]\n");
  nc+=fprintf(fp,"in   : TMSi Mobi8/MindMedia Nexus-10 sample file (default=%s)\n",INDEF);
  nc+=fprintf(fp,"bdf  : BDF output file (default=<in>.bdf)\n");
  nc+=fprintf(fp,"txt  : text output file (default: none)\n");
  nc+=fprintf(fp,"chn  : channel selection string (default=%s)\n",CHNDEF);
  nc+=fprintf(fp,"lbl  : channel labeling 1:NFB@UvT 2:NFB@MGGz (default: %d)\n",lbl);
  nc+=fprintf(fp,"        chn %6s %6s\n","UvT","MGGz");
  for (i=0; i<NR_OF_CHANNELS; i++) {
    nc+=fprintf(fp,"        %1C %6s %6s\n",'A'+i,defName1[i],defName2[i]);
  }
  nc+=fprintf(fp,"A..H : EDF/BDF signal names for channel A..H\n");
  nc+=fprintf(fp,"M    : EDF/BDF message for the header\n");
  nc+=fprintf(fp,"pid  : patient ident (default: '%s')\n",pid);
  nc+=fprintf(fp,"h    : show this manual page\n");
  nc+=fprintf(fp,"vb   : verbose switch (default=0x%02X)\n",vb);
  nc+=fprintf(fp,"  0x01 : show configuration\n");
  nc+=fprintf(fp,"  0x02 : show patient/measurement info of configuration\n");
  nc+=fprintf(fp,"  0x04 : record info\n");
  nc+=fprintf(fp,"dbg  : debug value (default=0x%02X)\n",dbg);
  return(nc);
}

/** reads the options from the command line */
static void parse_cmd(int32_t argc, char *argv[], char *iname, int32_t *chn,
 char *ename, char *tname) {

  int32_t i,j;         /**< general index */
  char   *fname;       /**< filename pointer */
  char    path[MNCN];  /**< temp path name */
  char   *cp;          /**< character pointer */
 
  strcpy(iname, INDEF); strcpy(ename,""); strcpy(tname,""); //strcpy(fname,"");
  *chn = tms_chn_sel((char *)CHNDEF);
  
  switch (lbl) {
    case 2: 
      edfMsg=edfMsg2; break;
    default:
      edfMsg=edfMsg1; break;
  }

  for (i=0; i<NR_OF_CHANNELS; i++) {
    switch (lbl) {
      case 2:
        edfChnName[i]=defName2[i]; break;
      default:
        edfChnName[i]=defName1[i]; break;
    }  
  }
  
  for (i=1; i<argc; i++) {
    if (argv[i][0]!='-') {
      fprintf(stderr,"missing - in argument %s\n",argv[i]);
    } else {
      switch (argv[i][1]) {
        case 'i': strcpy(iname,argv[++i]); break;
        case 'o': strcpy(ename,argv[++i]); break;
        case 'c': *chn=tms_chn_sel(argv[++i]); break;
        case 't': strcpy(tname,argv[++i]); break;
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
         j=argv[i][1]-'A';
          if ((j>=0) && (j<NR_OF_CHANNELS)) { 
            edfChnName[j]=argv[++i];
          }
          break;
        case 'm': edfMsg=argv[++i]; break;
        case 'P': pid=argv[++i]; break;
        case 'l': lbl=strtol(argv[++i],NULL,0); break;
        case 'v': vb=strtol(argv[++i],NULL,0); break;
        case 'd': dbg=strtol(argv[++i],NULL,0); break;
        case 'h': tms_cfg_intro(stderr); exit(0);
        default : fprintf(stderr,"can't understand argument %s\n",argv[i]); 
          exit(0);
      }
    }        
  }
  
  strcpy(path, iname);

#ifdef _MSC_VER
  {
      char filename[MAX_PATH];
      char extension[MAX_PATH];
      _splitpath(path, NULL, NULL, filename, extension);
      sprintf(path, "%s%s", filename, extension);
      fname = path;
  }
#else
  fname = (char *)basename(path);
#endif

  if ((cp=strstr(fname,".SMP"))!=NULL) {*cp='\0'; } else
  if ((cp=strstr(fname,".smp"))!=NULL) {*cp='\0'; }
    
  /* make 'ename' out of 'iname' without extension */
  if (strlen(ename)<1) {
    sprintf(ename,"%s",fname);
  }  
  
  if (strlen(pid)<1) { pid = ename; }

} 

int32_t prt_chn(FILE *fp, tms_channel_data_t *chd) {

  int32_t nc=0;
  int32_t i,j;
  
  for (j=0; j<14; j++) {
    nc+=fprintf(fp," %2d %2d %2d :",j,chd[j].ns,chd[j].rs);
    for (i=0; i<chd[j].rs; i++) {
      nc+=fprintf(fp," %6d",chd[j].data[i].isample);
    }
    nc+=fprintf(fp,"\n");
  }
  return(nc);
}

/** main */
int32_t main(int32_t argc, char *argv[]) {

  char iname[MNCN];          /**< TMSi Mobi-8 / MindMedia Nexus-10 input file name */
  FILE   *fpi=NULL;          /**< input file pointer */
  char ename[MNCN];          /**< output file extension name */
  FILE   *fpe=NULL;          /**< EDF/BDF file pointer */
  char tname[MNCN];          /**< text output file name */
  FILE   *fpo=NULL;          /**< text file pointer */
  uint8_t header[0x600];     /**< config */
  int32_t br,bp;             /**< number of bytes read/parsed */
  int32_t bidx=0;            /**< byte index */
  tms_config_t cfg;          /**< TMSi config */
  tms_measurement_hdr_t hdr; /**< TMSi measurement header */
  int32_t i,j,k;             /**< general index */
  int32_t sample;            /**< sample */
  int32_t tc;                /**< time counter */
  uint8_t buf[4096];         /**< byte buffer */
  int32_t cnt[64];           /**< sample counter per channel */
  int32_t overflow[64];      /**< overflow counter per channel */
  int32_t ba;                /**< bytes available */
  int32_t value[64];         /**< current value per channel */
  int32_t chnsel;            /**< channel selection */
  int32_t max_period;        /**< maximum period over all channels */
  tms_channel_data_t *chd;   /**< channel data block pointer */
  int32_t rec_cnt=0;         /**< EDF/BDF record counter */
  int32_t ns_max;            /**< maximum number of samples per channel over all channels */
//char    id[MNCN];          /**< init Id number */
  char    stime[MNCN];       /**< start time */
  char    bname[MNCN];       /**< full EDF/BDF output file name */
  char    msg[MNCN];         /**< recording message */
//int32_t min_deci;          /**< smallest decimation of quickest channel */
  float   fs;                /**< sample frequency of quickest channel */
  int32_t blk_cnt=1;         /**< block counter to block duration above 0.5 [s] */
  int32_t ok=0;              /**< EDF/BDF block is full */
  int32_t chnedf = 0xFFFF;   /**< write all channels to edf file */
  
  parse_cmd(argc, argv, iname, &chnedf, ename, tname);
  
  /* open config file */
  fprintf(stderr,"# Read TMSi Mobi-8/MindMedia Nexus-10 measurement data from file %s\n",iname); 
  if ((fpi=fopen(iname,"rb"))==NULL) {
    fprintf(stderr,"Can't open filename `%s': %s\n",iname,strerror(errno)); 
    exit(1);
  }
  /* read config file */
  br=fread(header,1,sizeof(header),fpi);
  if (br<1024) {
    fprintf(stderr,"# Error: config size %d < 1024\n",br);
    return(-1);
  }

  /* parse buffer 'hdr' to TMSi config struct 'cfg' */
  bidx=0;
  bp=tms_get_cfg(header,&bidx,&cfg);

  /* sanity check */
  if (cfg.version!=0x0314) {
    fprintf(stderr,"# Error missing version number 0x0314\n");
    return(-1);
  }
  if (vb&0x01) {
    /* show TMSi config struct 'cfg' */
    tms_prt_cfg(stderr,&cfg,vb&0x02);
  }
  
  /* check rate rate and sample rate divider */
  if (abs(2048/(1<<cfg.sampleRateDiv) - cfg.sampleRate)>1) {
    fprintf(stderr,"#Warning: sampleRate %d Sample Rate Divider %d\n",
      cfg.sampleRate, cfg.sampleRateDiv);
    cfg.sampleRate=2048/(1<<cfg.sampleRateDiv);
  }
  /* sample frequency depends also on smallest available decimation */
  fs=1.0f*cfg.sampleRate/(cfg.mindecimation+1);

  if (strlen(tname)>1) {
    /* open output text file */  
    fprintf(stderr,"# Write data samples to text file %s\n",tname);
    if (strcmp(tname,"stdout")==0) { fpo=stdout; } else { fpo=fopen(tname,"w"); }
    if (fpo==NULL) {
      fprintf(stderr,"Can't open filename `%s': %s\n",tname,strerror(errno)); 
      exit(1);
    }
  }
  
  if (br>=0x600) {
    bidx=0x400;
    /* measurement header available */
    bp=tms_get_measurement_hdr(header,&bidx,&hdr);
    /* print TMSi measurement_hdr struct 'hdr' in output file */
    tms_prt_measurement_hdr(stderr,&hdr);
    fprintf(stderr,"# sample rate %.1f [Hz]\n",fs);
    fprintf(stderr,"# initId 0x%08X\n",cfg.initId);
  }
  
  /* calculate maximum period over all channels */
  max_period=0; chnsel=0;
  for (j=0; j<cfg.nrOfChannels; j++) {
    /* channel 'j' active and needed */
    if (cfg.storageType[j].delta>0) {
      chnsel|=(1<<j);
      if (max_period < cfg.storageType[j].period) {
        max_period = cfg.storageType[j].period;
      }
    }
  }
  fprintf(stderr,"# chnsel 0x%04X max_period %d\n",chnsel,max_period);
  
  ns_max=0;
  /* allocate space for all channels overhead */
  chd=(tms_channel_data_t *)calloc(cfg.nrOfChannels,sizeof(tms_channel_data_t));
  for (j=0; j<cfg.nrOfChannels; j++) {
    /* channel 'j' active */
    if (cfg.storageType[j].delta>0) {
      chd[j].rs=0;
      chd[j].ns=max_period / cfg.storageType[j].period;
      if (ns_max < chd[j].ns) { ns_max = chd[j].ns; }
      /* reset sample counter so that it will start after first packet with '0' */
      chd[j].sc=-chd[j].ns;
      /* set clock tick duration per channel */
      chd[j].td=ns_max/(chd[j].ns*1.0*fs);
    }
    /* reset overflow counter per channel */
    overflow[j]=0; cnt[j]=0; 
    /* current value */
    value[j]=0;
  }
  
  blk_cnt=1;
  while (blk_cnt * (ns_max/fs)< 0.5) { blk_cnt *=2; }
  fprintf(stderr,"# blk_cnt %d\n",blk_cnt);
  
  /* allocate space for all channels sample data */
  if (vb&0x04) {
    fprintf(stderr,"# %3s %4s %2s %4s %8s\n","chn","ns","rs","sc","td [s]");
  }
  for (j=0; j<cfg.nrOfChannels; j++) {
    chd[j].ns *= blk_cnt;
    chd[j].data=(tms_data_t *)calloc(chd[j].ns,sizeof(tms_data_t));
    if (vb&0x04) {
      fprintf(stderr,"  %3d %4d %2d %4d %8.6f\n",j,chd[j].ns,chd[j].rs,chd[j].sc,chd[j].td);
    }
  } 
    
  /* Added person info and start time to BDF file name */
  if ( strchr(ename,'/')==NULL 
#ifdef WIN32
      && strchr(ename,'\\')==NULL 
#endif
     ) {
    strftime(stime,MNCN,"%Y%m%dT%H%M%S",localtime(&hdr.startTime));
    sprintf(bname,"%s_%s.bdf",ename,stime);
  } else {
    strcpy(bname,ename);
  }

  /* open output BDF file */  
  fprintf(stderr,"# Write data samples to BDF file %s\n",bname);
  if ((fpe=fopen(bname,"wb"))==NULL) {
    fprintf(stderr,"Can't open filename `%s': %s\n",bname,strerror(errno)); 
    exit(1);
  }
  rec_cnt=0;
    
  sprintf(msg,"%s Nexus-10: SerialNr %d HWNr 0x%04X SWNr 0x%04X",edfMsg,
      hdr.frontendSerialNr,hdr.frontendHWNr,hdr.frontendSWNr); 
 
  /* write EDF/BDF headers */
  edfWriteHdr(fpe, chd, cfg.nrOfChannels, chnsel&chnedf, pid, msg, &hdr.startTime,
   blk_cnt*(ns_max/fs), edfChnName);
  fflush(fpe);
  
  if (fpo!=NULL) {
    /* print header */
    fprintf(fpo,"#%10s %9s","t[s]","nr");
    for (j=0; j<cfg.nrOfChannels; j++) {
      /* print channel name */
      if (cfg.storageType[j].delta>0) {
        fprintf(fpo," %7s%1c","",'A'+j);
      }
    }
    fprintf(fpo,"\n");
  }
   
  /* start at channel 0, time tick 0 and empty byte buffer */
  j=0; tc=0; i=sizeof(buf); ba=i;  
  /* parse measurement data */
  while (!feof(fpi)) {
    if ((j==0) && (fpo!=NULL)) {
      fprintf(fpo," %10.4f %9d",(1.0*tc*(cfg.mindecimation+1)/cfg.sampleRate),tc);
    }
    if (cfg.storageType[j].delta>0) {
      /* used channel */
      if ((tc%cfg.storageType[j].period)==0) {
        /* one extra sample in channel 'j' */
        cnt[j]++;
        /* byte length of this sample */
        br=cfg.storageType[j].delta;
        /* fetch sample for buffer */
#if 0
        if (i+br>sizeof(buf)) {
          /* buffer hasn't enough measurement bytes */
          if (i<sizeof(buf)) {
            /* shift last unparsed bytes to begin of buffer */
            memmove(&buf[0],&buf[i],sizeof(buf)-i);
          }
          i=sizeof(buf)-i;
          /* fill buffer to the last byte */
          ba=i+fread(&buf[i],1,(sizeof(buf)-i),fpi);
          i=0;
        }
#else
        i=0; ba=fread(&buf[i],1,br,fpi);
#endif
        /* convert to integer and print it */
        sample=tms_get_int(buf,&i,br);
        /* sign extension */    
        sample=(sample<<(32-8*br))>>(32-8*br);
        /* check storage delta */
        if (cfg.storageType[j].delta<3) {
          /* sum successive delta's to get orginal value back */
          value[j]+=sample;
        } else {
          /* no differentation of this sample, sample is orginal value */
          value[j]=sample;
        }

        /* overflow check */
        if (sample!=cfg.storageType[j].overflow) {
          /* print current value */
          if (fpo!=NULL) { fprintf(fpo," %8d",value[j]); }
        } else {
          chd[j].data[chd[j].rs].flag=0x01; 
          overflow[j]++;
          /* map overflow to NAN */
          if (fpo!=NULL) { fprintf(fpo," %8s","nan"); }
        }
        /* fill chn with data samples */
        chd[j].data[chd[j].rs].isample=value[j]; 
        chd[j].rs++; chd[j].sc++;
      } else { 
        /* skip this sample */
        if (fpo!=NULL) { fprintf(fpo," %8s","nan"); }
      }
    }
    /* next channel */
    j++;
    if (j==cfg.nrOfChannels) { 
      /* next time tick */
      j=0; tc++;
      ok=1; for (k=0; k<cfg.nrOfChannels; k++) { ok = ok && chd[k].rs==chd[k].ns; }
      if (ok==1) {
        if (vb&0x08) {
          /* write samples to TEXT output file */
          tms_prt_samples(stderr, chd, chnsel&chnedf, chnsel&chnedf, cfg.nrOfChannels);
        }
        edfWriteSamples(fpe, 1, chd, chnsel&chnedf, 0);
        rec_cnt++; 
        /* reset sample counters */
        for (k=0; k<cfg.nrOfChannels; k++) { chd[k].rs=0; }
      }
      if (fpo!=NULL) { fprintf(fpo,"\n"); }
    }
  }
  fprintf(stderr,"# %d EDF/BDF records written\n",rec_cnt);

  /* overview per channel */
  for (k=0; k<4; k++) {
    /* print header */
    switch (k) {
      case 0:
        fprintf(stderr,"#%10s %9s","t[s]","nr");
        break;
      case 1:
        fprintf(stderr,"#%20s","cnt");
        break;
      case 2:
        fprintf(stderr,"#%20s","overflow");
        break;
      case 3:
        fprintf(stderr,"#%20s","overflow[%]");
        break;
    }
    for (j=0; j<cfg.nrOfChannels; j++) {
      /* print channel name */
      if (cfg.storageType[j].delta>0) {
        switch (k) {
          case 0: 
            fprintf(stderr," %8s",edfChnName[j]);
            break;
          case 1: 
            fprintf(stderr," %8d",cnt[j]);
            break;
          case 2: 
            fprintf(stderr," %8d",overflow[j]);
            break;
          case 3: 
            fprintf(stderr," %8.3f",(100.0*overflow[j]/cnt[j]));
            break;
        }    
      }
    }
    fprintf(stderr,"\n");
  }
    
  /* close input file */
  fclose(fpi);
  /* close output file */
  if (fpo!=NULL) { fclose(fpo); }

  /* seek to 'NrOfDataRecords' position of EDF/BDF file */
  if (fseek(fpe, 236L, SEEK_SET)==0) { 
    /* print record count into correct position */
    fprintf(fpe,"%-8d",rec_cnt);
  } else {
    fprintf(stderr,"# Error: seek problem to set 'NrOfDataRecords' in BDF file\n");
  }
  /* close BDF file */
  fclose(fpe);

  return(0);
}
