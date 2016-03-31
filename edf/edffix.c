/** 
 @brief Fix missing number of records in EDF/BDF file and
   truncate file size.

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
  #include "../nexus/inc/win32_compat.h"
  #include "../nexus/inc/stdint_win32.h"
#else
  #ifndef _XOPEN_SOURCE 
    #define _XOPEN_SOURCE  600 // for round()
  #endif
  #include <stdint.h>
  #include <inttypes.h>
  #include <unistd.h>
  #include <alloca.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <math.h>

#include "edf.h"

#define VERSION "edffix 0.1 21/04/2009 22:30"

#define  INDEF "pp07_01_flanker_01_20090203T114938.bdf"
#define  MDTDEF     (0.015)  /**< maximum period of backlight switch [s] */
#define  MNCN        (1024)  /**< Maximum Number of Characters in fileName */
#define  SCLDEF        (24)  /**< startcode length [bits] */
#define  THRDEF      (0.00)  /**< default threshold [uV] */
#define  EVENTDURDEF (0.10)  /**< event duration [s] -dt ... 0 ... dt */
#define  SSFDEF       (256)  /**< GSR subsample factor */

int32_t  vb =  0x00;
int32_t dbg =  0x00;
double   tp = 5.000;    /**< range check window duration [s] */ 

#define MAXTASK (2*NTASK)

task_t  task[MAXTASK];    /**< task array */
int32_t tcnt=0;           /**< task counter */

/** edffix usage
 * @return number of printed characters.
*/
int32_t edffix_intro(FILE *fp) {
  
  int32_t nc=0;
  
  nc+=fprintf(fp,"%s\n",VERSION);
  nc+=fprintf(fp,"Usage: edffix [-i <in>] [-o <out>] [-t <thr>] [-m <mdt>] [-l <scl>] [-e <evd>] [-s <ssf>] [-v <vb>] [-d <dbg>] [-h]\n");
  nc+=fprintf(fp,"in   : input EDF/BDF file (default=%s)\n",INDEF);
  nc+=fprintf(fp,"out  : output EDF/BDF file (default=<in>.cor.bdf)\n");
  nc+=fprintf(fp,"mdt  : maximum period of backlight switch (default=%.4f [s])\n",MDTDEF);
  nc+=fprintf(fp,"thr  : threshold (default=%.2f)\n",THRDEF);
  nc+=fprintf(fp,"scl  : startcode length [bits] (default=%d)\n",SCLDEF);
  nc+=fprintf(fp,"evd  : event duration (default=%.2f)\n",EVENTDURDEF);
  nc+=fprintf(fp,"ssf  : GSR subsample factor (default=%d)\n",SSFDEF);
  nc+=fprintf(fp,"h    : show this manual page\n");
  nc+=fprintf(fp,"vb   : verbose switch (default=0x%03X)\n",vb);
  nc+=fprintf(fp,"  0x01: set tasknumber in rejected epoch channel\n");
  nc+=fprintf(fp,"  0x02: remove backlight switching in display signal\n");
  nc+=fprintf(fp,"  0x04: use threshold <thr> for display signal\n");
  nc+=fprintf(fp,"  0x08: chomp first and last useless records\n");
  nc+=fprintf(fp,"  0x10: ERP compensation for button press action in ECG signal\n");
  nc+=fprintf(fp,"  0x20: enlarge EDF/BDF record duration > 0.5 [s]\n");
  nc+=fprintf(fp,"  0x40: remove EDF/BDF annotations\n");
  nc+=fprintf(fp," 0x100: report GSR signal\n");
  nc+=fprintf(fp," 0x200: clip status signal to 8 bits\n");
  nc+=fprintf(fp,"dbg  : debug value (default=0x%02X)\n",dbg);
  nc+=fprintf(fp," 0x01: show EDF header info\n");
  return(nc);
}

/** reads the options from the command line */
static void parse_cmd(int32_t argc, char *argv[], char *iname, char *oname, double *mdt,
 double *thr, double *evd, int32_t *ssf, int32_t *scl) {

  int32_t i;
  char   *cp;          /**< character pointer */
  char    fname[MNCN]; /**< temp file output name */
 
  strcpy(iname, INDEF); strcpy(oname,""); *mdt=MDTDEF; *evd=EVENTDURDEF; *ssf=SSFDEF;
  *scl=SCLDEF;
  
  for (i=1; i<argc; i++) {
    if (argv[i][0]!='-') {
      fprintf(stderr,"missing - in argument %s\n",argv[i]);
    } else {
      switch (argv[i][1]) {
        case 'i': strcpy(iname,argv[++i]); break;
        case 'o': strcpy(oname,argv[++i]); break;
        case 'v': vb=strtol(argv[++i],NULL,0); break;
        case 'm': *mdt=strtod(argv[++i],NULL); break;
        case 't': *thr=strtod(argv[++i],NULL); break;
        case 'e': *evd=strtod(argv[++i],NULL); break;
        case 'l': *scl=strtol(argv[++i],NULL,0); break;
        case 's': *ssf=strtol(argv[++i],NULL,0); break;
        case 'd': dbg=strtol(argv[++i],NULL,0); break;
        case 'h': edffix_intro(stderr); exit(0);
        default : fprintf(stderr,"can't understand argument %s\n",argv[i]); 
          exit(0);
      }
    }        
  }
  /* strip '.edf' or '.bdf' extension of 'iname' */
  strcpy(fname,iname);
  if ((cp=strstr(fname,".edf"))!=NULL) {*cp='\0';} else
  if ((cp=strstr(fname,".bdf"))!=NULL) {*cp='\0';}

  /* make 'oname' out of 'iname' */
  if (strlen(oname)<1) {
    sprintf(oname,"%s.cor.bdf",fname);
  }
} 

/** Find index of first usefull record in 'edf'
 * @return index>=0, <0 on error
*/
int32_t edf_first_usefull_rec(edf_t *edf) {

  int32_t  chn;       /**<  channel */
  int32_t i,i0;       /**< general index */

  chn=edf_fnd_chn_nr(edf,"RejectedEpochs");
  if (chn<0) {
    fprintf(stderr,"# Error: missing Rejected Epochs channel\n");
    return(-1);
  }
  i=1; i0=i-1;
  while ((i<edf->signal[chn].NrOfSamples) && (edf_get_integer_value(edf, chn, i)<1)) { 
    /* check for state change for overflow to ok */
    if ((edf_get_integer_value(edf, chn, i-1) < 0) && (edf_get_integer_value(edf, chn, i) >= 0)) {
      i0=i; 
    }
    i++; 
  }
  return(i0/edf->signal[chn].NrOfSamplesPerRecord);
}

/** Find index of last usefull record in 'edf'
 * @return index>=0, <0 on error
*/
int32_t edf_last_usefull_rec(edf_t *edf) {

  int32_t  chn;       /**<  channel */
  int32_t i,i0;       /**< general index */

  chn=edf_fnd_chn_nr(edf,"RejectedEpochs");
  if (chn<0) {
    fprintf(stderr,"# Error: missing Rejected Epochs channel\n");
    return(-1);
  }
  i=edf->signal[chn].NrOfSamples-1; i0=i;
  while ((i>1) && (edf_get_integer_value(edf, chn, i) < 1)) { 
    /* check for state change for overflow to ok */
    if ((edf_get_integer_value(edf, chn, i) < 0) && (edf_get_integer_value(edf, chn, i-1) >= 0)) {
      i0=i; 
    }
    i--; 
  }
  return(i0/edf->signal[chn].NrOfSamplesPerRecord);
}

/** Apply threshold 'thr' on display signal in 'edf' struct. 
 * Use 'smin' and 'smax' as new display signal minimum and maximum.
 * @return 0 on success.
*/
int32_t edf_thr_display(edf_t *edf, double thr, double smin, double smax) {
  
  int32_t  chn;       /**< display channel */
  int32_t    i;       /**< general index */
  float   gain;       /**< gain of channel 'chn' */
  int32_t ithr;       /**< integer threshold of display signal */
  
  /* get channel number of display signal */
  if ((chn=edf_fnd_chn_nr(edf,"Disp"))<0) {
    return(-1);
  }

  /* gain for channel 'chn' */
  gain=(float)(edf->signal[chn].PhysicalMax - edf->signal[chn].PhysicalMin) /
              (edf->signal[chn].DigitalMax  - edf->signal[chn].DigitalMin);

  /* convert threshold [uV] to integer value */
  ithr=(int32_t)round(thr/gain);

  fprintf(stderr,"# Info: Apply threshold %8.4f [uV] %d [-] on channel 'Disp' nr %d\n",thr,ithr,chn);

  for (i=0; i<edf->signal[chn].NrOfSamples; i++) {
    if (edf_get_integer_value(edf, chn, i) < ithr) { 
      edf_set_integer_overflow_value(edf, (float) smin, 0, chn, i, gain);
    } else { 
      edf_set_integer_overflow_value(edf, (float) smax, 0, chn, i, gain);
    }
  }
  return(0);              
}

/** Clip 'Status' signal to lower 'n' bits
 * @return 0 on success
*/
int32_t edf_clip_status(edf_t *edf, int32_t n) {

  int32_t  stc;      /**< status channel index */
  int32_t  i;        /**< sample index */
  int32_t  msk = (1<<n) -1; /**< mask */
  
  /* get channel index of Switch signal */
  if ((stc=edf_fnd_chn_nr(edf,"Status"))<0) {
    fprintf(stderr,"# Warning: missing Status channel\n");
    return(-1);
  }

  /* quick and dirty subsampling of GSR signal with factor 8 */
  for (i=0; i<edf->signal[stc].NrOfSamples; i++) {
    edf->signal[stc].data[i] &= msk;
  }
  return(0);
}


/** Check GSR signal during whole session with sub-sample factor 'ssf'
 * @return 0 on success
*/
int32_t edf_chk_gsr(edf_t *edf, int32_t ssf) {
  
  int32_t  gsrc;      /**< GSR channel index */
  int32_t  tskc;      /**< task/Rejected Epoch channel index */
  float    fs;        /**< sample frequency [Hz] of GSR channel */
  int32_t  a,b;       /**< NrOfSamplesPerRecord of GSR and task channel */
  float    gain;      /**< gain of channel 'chn' */
  int32_t  i,j;       /**< general index */
  float    sample;    /**< sample */
  float    gsr;       /**< GSR sample */
  int32_t  tsk;       /**< task number */
  
  /* get channel index of Switch signal */
  if ((gsrc=edf_fnd_chn_nr(edf,"GSR"))<0) {
    fprintf(stderr,"# Error: missing GSR channel\n");
    return(-1);
  }

  /* get channel index of RejectedEpochs signal */
  tskc=edf_fnd_chn_nr(edf,"RejectedEpochs");
  if (tskc<0) {
    fprintf(stderr,"# Error: missing Rejected Epochs channel\n");
    return(-2);
  }

  /* gain for GSR channel */
  gain=(float)(edf->signal[gsrc].PhysicalMax - edf->signal[gsrc].PhysicalMin) /
              (edf->signal[gsrc].DigitalMax  - edf->signal[gsrc].DigitalMin);
              
  /* sample rate of ECG channel */
  fs = (float) (edf->signal[gsrc].NrOfSamplesPerRecord / edf->RecordDuration); 
  
  a=edf->signal[gsrc].NrOfSamplesPerRecord;
  b=edf->signal[tskc].NrOfSamplesPerRecord;
  fprintf(stderr,"# Info: fs %f a %d b %d\n",fs,a,b);
  fprintf(stderr,"    t  GSR tsk\n");
  
  /* quick and dirty subsampling of GSR signal with factor 8 */
  for (i=0; i<edf->signal[gsrc].NrOfSamples; i+=ssf) {
    sample=gain*edf_get_integer_value(edf, gsrc, i);
    /* translate [uV] into [uSiemens] according Ger-Jan de Vries */
    /* sample represents GSR resistance in [kOhm] */
    gsr = (float) (724.64 / (sample/1000.0 + 3.116));
    j=(b*i)/a;
    tsk=edf_get_integer_value(edf, tskc, j);
    fprintf(stderr," %.4f %.2f %2d\n",(i/fs),gsr,tsk);
  }

  return(0);
}

/** Check mussle contraction ERP during answersing on cognitive tasks
 * for 'dt' [s] around start of answer.
 * @note 'm' return number of samples in 'erp'.
 * @return pointer to 'm' samples of 'erp', NULL on error
*/
float *edf_chk_ecg(edf_t *edf, double dt, int32_t *m) {
  
  int32_t  swtc;      /**< display channel index */
  int32_t  ecgc;      /**< ECG channel index */
  float    fs;        /**< sample frequency [Hz] */
  int32_t  n;         /**< sample -n ... 0 ... n */
  float   *erp=NULL;  /**< ERP samples */
  float    gain;      /**< gain of channel 'chn' */
  int32_t  i,j;       /**< general index */
  int32_t  cnt=0;     /**< event counter */
  float    sample;    /**< sample in ERP epoch */
  float    smin,smax; /**< minimum and maximum in ERP epoch */
  int32_t  imin,imax; /**< minimum and maximum location in ERP epoch */
  
  /* get channel index of Switch signal */
  if ((swtc=edf_fnd_chn_nr(edf,"Switch"))<0) {
    fprintf(stderr,"# Error: missing display channel\n");
    return(erp);
  }
  /* get channel index of ECG signal */
  if ((ecgc=edf_fnd_chn_nr(edf,"ECG"))<0) {
    fprintf(stderr,"# Error: missing ECG channel\n");
    return(erp);
  }

  /* gain for ECG channel */
  gain=(float)(edf->signal[ecgc].PhysicalMax - edf->signal[ecgc].PhysicalMin) /
              (edf->signal[ecgc].DigitalMax  - edf->signal[ecgc].DigitalMin);
              
  /* sample rate of ECG channel */
  fs = (float) (edf->signal[ecgc].NrOfSamplesPerRecord / edf->RecordDuration);  
  if (edf->signal[ecgc].NrOfSamplesPerRecord != edf->signal[swtc].NrOfSamplesPerRecord) {
    fprintf(stderr,"# Error: number of samples per record Switch and ECG unequal!!!\n");
  }
  
  /* number of samples ERP */
  n=(int32_t)round(fs*dt); (*m)=2*n+1;
  
  fprintf(stderr,"# Info: fs %f n %d\n",fs,n);

  /* allocate space for ERP samples */
  if ((erp=(float *)malloc((*m)*sizeof(float)))==NULL) {
    fprintf(stderr,"# Error not enough memory to allocate %d ECG samples!!!\n",(*m));
    return(erp);
  }
  
  if (vb&0x60) { fprintf(stderr," cnt   t   erp\n"); }
 
  /* clear ERP */
  for (i=-n; i<n; i++) { erp[n+i]=0; }
  cnt=0;
  /* check all rising edges of switch */
  for (j=n; j<edf->signal[swtc].NrOfSamples; j++) {
    if (edf_get_integer_value(edf, swtc, j) > edf_get_integer_value(edf, swtc, j-1)) {
      smin=gain*edf_get_integer_value(edf, ecgc, j); smax=smin; imin=0; imax=0;
      for (i=-n; i<n; i++) {
        sample=gain*edf_get_integer_value(edf, ecgc, j+i);
        /* find minimum and maximum sample */
        if (sample<smin) { smin=sample; imin=i; }
        if (sample>smax) { smax=sample; imax=i; }
      }
      if (vb&0x80) {
        fprintf(stderr," %3d %.4f %d %.3f %d %.3f\n",
          cnt,j/fs,imin,smin,imax,smax);
      }
      /* take imax as synchronisation point */
      for (i=-n; i<n; i++) {
        sample=gain*edf_get_integer_value(edf, ecgc, j+i /* +imax */);
        erp[n+i]+=sample;
        if (vb&0x20) {
          fprintf(stderr," %3d %.4f %.3f\n",cnt,i/fs,sample);
        }
      }
      cnt++;
    }
  }
  
  /* average all samples */
  if (cnt>0) {
    for (i=-n; i<n; i++) { 
      erp[n+i]/=cnt;
      if (vb&0x40) {
        fprintf(stderr," %3d %.4f %.3f\n",cnt,i/fs,gain*erp[n+i]);
      }
    }
  }
  
  return(erp); 
}

/** main */
int32_t main(int32_t argc, char *argv[]) {

  char    iname[MNCN];      /**< input file name */
  char    oname[MNCN];      /**< output file name */
  FILE   *fp;
  edf_t   edf;
  int64_t fileLength=0LL;
  int64_t edfLength=0LL;
  int32_t recordSize;
  int32_t scl;              /**< startcode length */
  int32_t dsc;              /**< display channel number */
  int32_t swc;              /**< switch channel number */
  double  tchk;             /**< battery low start time */
  double  mdt;
  double  thr=THRDEF;       /**< new threshold [uV] of display signal */
  int32_t bl_cnt=0;         /**< backlight correction counter */
  double  err=0.0;          /**< missing or erronous packets percentage */
  int32_t i;                /**< general index */
  int32_t rfirst,rlast;     /**< first and last usefull record index */
  int32_t nc=0;             /**< bytes written to EDF/BDF file */
  float  *erp;              /**< ERP samples */
  int32_t m=0;              /**< number of samples in 'erp' */
  double  evd;              /**< event duration [s] -evd ... 0 ... evd */
  int32_t ssf;              /**< GSR subsample factor */
  int32_t blk;              /**< merge 'blk' EDF/BDF records in one record */
  int32_t acn;              /**< annotation channel nr */
  edf_signal_t tmp;         /**< temp EDF/BDF signal data pointer */
  edf_annot_t *p = NULL;    /**< pointer to annotations struct */
  int32_t na;               /**< number of annotations */
  int32_t dc=0;             /**< discontiunity counter */
  
  parse_cmd(argc, argv, iname, oname, &mdt, &thr, &evd, &ssf, &scl);

  edf_set_vb(vb >> 16);
  
  fprintf(stderr,"# Open EDF/BDF file %s\n",iname);
  if ((fp=fopen(iname,"r+"))==NULL) {
    perror(""); return(-1);
  }
    
  /* read EDF/BDF main and signal headers */
  edf_rd_hdr(fp,&edf);
  
  /* fix BDF/EDF header if needed */
  edf_fix_hdr_version(fp);  
 
  if (dbg&0x01) {
    edf_prt_hdr(stderr,&edf);
  }

  recordSize=edf_get_record_size(&edf);
  fileLength=edf_get_file_size(fp);
  edfLength =edf.NrOfHeaderBytes + edf.NrOfDataRecords * recordSize;
  fprintf(stderr,"# Info: file Length %" PRIu64 " edf Length %" PRIu64 ". \n", fileLength, edfLength);
  /* calculate new nr of data records */
  edf.NrOfDataRecords=(int32_t)((fileLength - edf.NrOfHeaderBytes)/recordSize);
  /* set record counter */
  if (edf_set_record_cnt(fp,edf.NrOfDataRecords)!=0) {
    perror("edf_set_record_cnt problem");
  } else {
    fprintf(stderr,"# Info: Set record counter to %d\n",edf.NrOfDataRecords);
  }
  
  if ((strcmp(edf.DataFormatVersion,"EDF+D")==0) || (strcmp(edf.DataFormatVersion,"BDF+D")==0)) {
    fprintf(stderr,"# Info: found EDF+D/BDF+D file: check for continues time-axis\n");
    vb |= 0x400;
  }
  if (vb>0) {
    fprintf(stderr,"# Info: read data\n");
    /* read EDF/BDF signal headers */
    edf_rd_samples(fp,&edf);  
  }

  fclose(fp);
  
  if (vb&0x0400) {
    p = edf_get_annot(&edf, &na);
    /* check for skipped records */
    for (i=1; i<edf.NrOfDataRecords; i++) {
      if (fabs((edf.rts[i] - edf.rts[i-1] - edf.RecordDuration)/edf.RecordDuration) > 0.0001) {
        fprintf(stderr,"# Warning: time axis gap at record %d of %.1f [s]\n",
          i, edf.rts[i] - edf.rts[i-1]);
        dc++;
      }
    }
  }

  if (vb==0) { exit(0); }

  /* check battery level of Nexus-10 */
  swc=edf_fnd_chn_nr(&edf,"Switch");
  if ((swc>=0) && ((tchk=edf_chk_battery(&edf,swc))>0.0)) {
    fprintf(stderr, "# Warning: battery low at %8.3f [s] on channel %d\n", tchk, swc);
  } 
 
  /* Check if Rejected Epochs channel is already in EDF/BDF input file */
  if ((swc>=0) && (edf_fnd_chn_nr(&edf, "RejectedEpochs")<0)) {
    /* check for missing packets */
    err=edf_chk_miss(&edf);
    if (err>0.0) {
      fprintf(stderr,"# Warning: %.3f [%%] samples missing or with delta overflow\n",err);
    }
  }
  
  if (vb&0x02) {
    /* count the number of times backlight switching correction in needed */
    bl_cnt=edf_remove_switching_backlight(&edf,mdt);
  } 
  
  if (vb&0x04){
    /* apply new threshold to display signal */
    edf_thr_display(&edf, thr, -1000.0, 1000.0);
  }
           
  dsc=edf_fnd_chn_nr(&edf,"Disp");
  if ((dsc>=0) && (edf.NrOfDataRecords>1)) {
    /* count number of tasks */
    tcnt=edf_fnd_tasks(&edf, scl, tp, task, MAXTASK);

    fprintf(stderr,"#%6s %9s %9s %8s\n","task","start[s]","end[s]","dur[s]");
    /* mark all available tasks */
    for (i=0; i<tcnt; i++) {
      fprintf(stderr,"# %6s %9.3f %9.3f %8.3f\n",
        task[i].name,task[i].ts,task[i].te,task[i].te-task[i].ts);
      if ((task[i].cs>0) && (task[i].ce>0)) {
        edf_mark(&edf,i+1,task[i].ts,task[i].te);
      }
    }
    
  }
  
  blk=1; 
  if (vb&0x20) {
    /* split Record Duration > 1 [s] */
    while (edf.RecordDuration>1) { blk *=2; edf.RecordDuration /=2; }
    edf.NrOfDataRecords *= blk;
    for (i=0; i<edf.NrOfSignals; i++) {
       edf.signal[i].NrOfSamplesPerRecord /= blk;
    }
    if (blk>1) {
      fprintf(stderr,"# Info: split one record to %d records EDF/BDF record\n",blk);
    }
    
    /* merges records until duration > 0.5 [s] */
    blk=1;
    while (edf.RecordDuration<0.5) { blk *=2; edf.RecordDuration *=2; }
    edf.NrOfDataRecords /= blk;
    for (i=0; i<edf.NrOfSignals; i++) {
       edf.signal[i].NrOfSamplesPerRecord *= blk;
    }
    if (blk>1) {
      fprintf(stderr,"# Info: merge %d records to one EDF/BDF record\n",blk);
    }
  } 
  
  if (vb&0x40) {
    if (edf.bdf==1) {
      acn=edf_fnd_chn_nr(&edf,"BDF Annotations");
    } else {
      acn=edf_fnd_chn_nr(&edf,"EDF Annotations");
    }
    if (acn<0) {
      fprintf(stderr,"# Warning: missing %s annotation channel\n",
       (edf.bdf==1 ? "BDF" : "EDF"));
    } else {
      if (acn!=edf.NrOfSignals-1) {
        fprintf(stderr,"# Info: swapping channel %d with %d\n",acn,edf.NrOfSignals-1);
        memcpy(&tmp                          , &edf.signal[edf.NrOfSignals-1], sizeof(edf_signal_t));
        memcpy(&edf.signal[edf.NrOfSignals-1], &edf.signal[acn]              , sizeof(edf_signal_t));
        memcpy(&edf.signal[acn]              , &tmp                          , sizeof(edf_signal_t));
      }
      /* remove last (annotation) channel */
      fprintf(stderr,"# Info: remove annotation channel %d\n",edf.NrOfSignals-1);
      strcpy(edf.DataFormatVersion,(edf.bdf==1 ? "BDF" : "EDF"));
      edf.NrOfHeaderBytes -= 256;
      edf.NrOfSignals -=1;
    }
  }
  
  rfirst=0; rlast=edf.NrOfDataRecords-1;
  
  if (vb&0x08) {
    rfirst=edf_first_usefull_rec(&edf);
    rlast=edf_last_usefull_rec(&edf);
    fprintf(stderr,"# Info: Chomp first %.3f [s] and last %.3f [s] of useless signals\n",
              rfirst*edf.RecordDuration,(edf.NrOfDataRecords-rlast)*edf.RecordDuration);
    fprintf(stderr,"# Info: rfirst %d rlast %d NrOfDataRecords %d\n",
              rfirst,rlast,edf.NrOfDataRecords);
  }
  
  if (vb&0x10) {
    erp=edf_chk_ecg(&edf,evd,&m);
  }
  
  if (vb&0x100) {
    edf_chk_gsr(&edf,ssf);
  }
  
  if (vb&0x200) {
    edf_clip_status(&edf,8);
  }
  
  if ((vb&0x26C) || (bl_cnt>1) || (dc>0)) {

    fprintf(stderr,"# Info: Write EDF/BDF file %s",oname);
    if (bl_cnt>0) { fprintf(stderr," with corrected display signal"); }
    if (vb&0x20)  { fprintf(stderr," with %d blocks merged",blk); }
    if (dc>0)     { fprintf(stderr," with %d discontinuties marked",dc); }
    fprintf(stderr,"\n"); 
    if (dc>0) { 
      /* change to continues format */
      edf.DataFormatVersion[4]='C';
      /* rewrite annotation in continue order ! */
      edf_add_annot(&edf, p, na);
    }
    nc=edf_wr_file(oname,&edf,rfirst,rlast);
    if (nc<=0) {
      fprintf(stderr,"# Error: writing problems to file %s\n",oname);
    } else {
      fprintf(stderr,"# Info: writing %d bytes to file %s\n",nc,oname);
    }
  }
  
  return(0);
} 
