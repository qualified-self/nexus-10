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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <math.h>

#include "edf.h"

#define VERSION "edffix 0.1 21/04/2009 22:30"

#define  INDEF "pp07_01_flanker_01_20090203T114938.bdf"
#define  MNCN     (1024)

#define  BUTTON_DURATION    (0.022)  /**< [s] */

int32_t  vb =  0x00;
int32_t dbg =  0x00;
int32_t chn = 0x1FF;
int32_t  md =  0x00;

/** edffix usage
 * @return number of printed characters.
*/
int32_t edffix_intro(FILE *fp) {
  
  int32_t nc=0;
  
  nc+=fprintf(fp,"%s\n",VERSION);
  nc+=fprintf(fp,"Usage: edfsw [-i <in>] [-o <out>] [-c <chn>] [-v <vb>] [-m <md>] [-d <dbg>] [-h]\n");
  nc+=fprintf(fp,"in   : input EDF/BDF file (default=%s)\n",INDEF);
  nc+=fprintf(fp,"out  : output EDF/BDF file (default=<in>.bdf)\n");
  nc+=fprintf(fp,"chn  : channel selection A:0x01 B:0x02 ... (default=0x%04X)\n",chn);
  nc+=fprintf(fp,"md   : operation mode (default=0x%02X)\n",md);
  nc+=fprintf(fp," 0x01: convert switch nr to single bit\n");
  nc+=fprintf(fp," 0x02: make all sample rate of all channels equal\n");
  nc+=fprintf(fp," 0x04: report all rising edges in 'Switch' channel\n");
  nc+=fprintf(fp,"h    : show this manual page\n");
  nc+=fprintf(fp,"vb   : verbose switch (default=0x%02X)\n",vb);
  nc+=fprintf(fp," 0x01: show EDF header info\n");
  nc+=fprintf(fp," 0x02: show EDF data\n");
  nc+=fprintf(fp,"dbg  : debug value (default=0x%02X)\n",dbg);
  return(nc);
}

/** reads the options from the command line */
static void parse_cmd(int32_t argc, char *argv[], char *iname, char *oname) {

  int32_t i;
 
  strcpy(iname, INDEF); strcpy(oname,"");
  
  for (i=1; i<argc; i++) {
    if (argv[i][0]!='-') {
      fprintf(stderr,"missing - in argument %s\n",argv[i]);
    } else {
      switch (argv[i][1]) {
        case 'i': strcpy(iname,argv[++i]); break;
        case 'o': strcpy(oname,argv[++i]); break;
        case 'c': chn=strtol(argv[++i],NULL,0); break;
        case 'm': md=strtol(argv[++i],NULL,0); break;
        case 'v': vb=strtol(argv[++i],NULL,0); break;
        case 'd': dbg=strtol(argv[++i],NULL,0); break;
        case 'h': edffix_intro(stderr); exit(0);
        default : fprintf(stderr,"can't understand argument %s\n",argv[i]); 
          exit(0);
      }
    }        
  }

  /* make 'oname' out of 'iname' */
  if (strlen(oname)<1) {
    sprintf(oname,"%s.bdf",iname);
  }
} 

/** Set PhysicalDimension of channel 6 and 7 to "uV" 
*/
int32_t fix_signals(edf_t *edf) {
 
  int32_t i;

  for (i=6; i<8; i++) {
    strcpy(edf->signal[i].PhysicalDimension,"uV");
  }
  return(0);
}

int32_t fix_sample_rate(edf_t *edf) {

  int32_t j;  /**< signal counter */
  int32_t i;  /**< sample counter */
  int32_t k;  /**< subsample counter */
  int32_t ssf=1; /**< subsample factor */
  int32_t sc =0; /**< new sample counter */
  
  /* subsample channels */
  for (j=0; j<edf->NrOfSignals; j++) {
    /* subsample signal 4,6,7,8 with 8 and 9 with 2 */
    if (j<4)  { ssf=1; } else
    if (j<8)  { ssf=8; } else { ssf=2; }

    if (ssf>1) {
      /* subsample signal[j] */
      k=0;
      for (i=0; i<edf->signal[j].NrOfSamples; i+=ssf) {
        edf->signal[j].data[k]=edf->signal[j].data[i];
        k++; sc++;
      }
      /* fix number of samples */
      edf->signal[j].NrOfSamples/=ssf;
      edf->signal[j].NrOfSamplesPerRecord/=ssf;
    }
  }
  return(sc);
}

/* Use 'channel' switch to select edf channels
* @return number of wanted channels
*/
int32_t edf_select_channels(edf_t *edf, int32_t channel) {

  int32_t i,j;  /**< channel numbers */
 
  /* move wanted signals upwards */
  i=0;
  for (j=0; j < edf->NrOfSignals; j++) {
    if ((channel&(1<<j))!=0) {
      if (i!=j) {
        memcpy(&edf->signal[i],&edf->signal[j],sizeof(edf_signal_t)); 
      }
      i++;
    }
  }
  // fprintf(stderr,"# Info: %d channel left\n",i);
  edf->NrOfSignals=i;
  edf->NrOfHeaderBytes=(i+1)*256;
  /* return number of wanted channels */
  return(i);
}

/** Calculate number of one bits on integer 'a'
 * @return bit weigth of 'a'
*/
int32_t bit_weight(int32_t a) {
  
  int32_t bw=0;
  
  while (a>0) {
    if (a&0x01) { bw++; }
    a=a>>1;
  }
  return(bw);
}
  
/* Make all sample rates of all channels equal 
* @return 0 on success, -1 on failure
*/
int32_t edf_equal_sample_rate(edf_t *edf) {

  int32_t j;          /**< signal counter */
  int32_t i;          /**< sample counter */
  int32_t k;          /**< resample counter */
  int32_t nsrr=1;     /**< maximum number of samples per record */
  int32_t mpf=1;      /**< multiplex factor */
  int32_t *data=NULL; /**< resample values */
  int32_t rv=0;
  
  for (j=0; j<edf->NrOfSignals; j++) {
    if (nsrr < edf->signal[j].NrOfSamplesPerRecord) {
      nsrr=edf->signal[j].NrOfSamplesPerRecord;
    }
  }
  for (j=0; j<edf->NrOfSignals; j++) {
    if (nsrr > edf->signal[j].NrOfSamplesPerRecord) {
      mpf=nsrr/edf->signal[j].NrOfSamplesPerRecord;
      if (bit_weight(mpf)!=1) {
        fprintf(stderr,"# Error: equal_sample_rate chan %d multiplex factor %d is not power of 2\n",j,mpf);
        rv=-1;
        continue;
      }
      /* allocate space for resampled signal[j] */
      data=(int32_t *)calloc(sizeof(int),edf->signal[j].NrOfSamples*mpf);
      /* resample signal[j] */
      for (i=0; i<edf->signal[j].NrOfSamples; i++) {
        for (k=0; k<mpf; k++) {
          data[i*mpf+k]=edf->signal[j].data[i];
        }
      }
      /* free old samples */
      free(edf->signal[j].data);
      /* link new resamples ones */
      edf->signal[j].data=data;
      /* fix number of samples */
      edf->signal[j].NrOfSamples*=mpf;
      edf->signal[j].NrOfSamplesPerRecord*=mpf;
    }
  }
  return(rv);
}

/** Convert all switch pulse in channel 'chn' of EDF struct 'edf' into single bits
 *  @return number of found pulses
*/
int32_t edf_cvt_switch(edf_t *edf, int32_t channel) {

  int32_t      i,j;   /**< general index */
  int32_t      ir=0;  /**< rising edge index */
  double   fs;    /**< sampling frequency */
  double   dur;   /**< pulse duration */
  int32_t      key;   /**< which key was press */
  int32_t      cnt[7]; /**< pulse counter */
  int32_t      sum=0;  /**< total pulse counter */
  
  fs=edf->signal[channel].NrOfSamplesPerRecord / edf->RecordDuration;
  
  for (j=0; j<6; j++) { cnt[j]=0; }
  
  /* check rising edge of signal 'channel' */
  for (i=1; i<edf->signal[channel].NrOfSamples; i++) {
    if ((edf_get_integer_value(edf, channel, i-1) < 1) && (edf_get_integer_value(edf, channel, i) >= 1)) {
      /* found rising edge */
      ir=i;
    } else
    if ((edf_get_integer_value(edf, channel, i-1) >= 1) && (edf_get_integer_value(edf, channel, i) < 1)) {
      /* found falling edge */
      dur=(i-ir)/fs;
      key=(int)round(dur/BUTTON_DURATION);
      if (key<=0) {
        fprintf(stderr,"# Warning: key duration %6.3f too small at %6.3f [s] >> key=1\n",dur,(ir/fs));
        key=1;
      }
      if (key>5) {
        fprintf(stderr,"# Warning: key duration %6.3f too large at %6.3f [s] >> key=2\n",dur,(ir/fs));
        key=6;
      }
      edf->signal[channel].data[ir]=(1<<key);
      for (j=ir+1; j<i; j++) {
        edf->signal[channel].data[j]=0; 
      }
      cnt[key]++;
    }
  }
  sum=0;
  for (j=0; j<6; j++) {
    fprintf(stderr,"# key %d cnt %d\n",j,cnt[j]);
    sum+=cnt[j];
  }
  return(sum);
}

/** Report rising edges of channel 'Switch' to file 'fp'
 *  @return number of found pulses
*/
int32_t edf_rpt_switch_edge(FILE *fp, edf_t *edf, int32_t channel) {

  int32_t  i,j;   /**< general index */
  double   fs;    /**< sampling frequency */
  
  fs=edf->signal[channel].NrOfSamplesPerRecord / edf->RecordDuration;
  
  fprintf(fp," %4s %9s %12s\n","nr","sc","t");
  
  /* check rising edge of signal 'chn' */
  j=0;
  for (i=1; i<edf->signal[channel].NrOfSamples; i++) {
    if ((edf_get_integer_value(edf, channel, i-1) < 1) && (edf_get_integer_value(edf, channel, i) >= 1)) {
      fprintf(fp," %4d %9d %12.4f\n", j, i, (i/fs));
      j++;
    }
  }
  return(j);
}


/** main */
int32_t main(int32_t argc, char *argv[]) {

  char  iname[MNCN];         /**< input file name */
  char  oname[MNCN];         /**< output ECG file name */
  FILE *fp;
  edf_t edf;
  int32_t  swc;              /**< switch channel number */
  int32_t  cnt;              /**< number of buttons pressed */
  double  tchk;              /**< time of first low battery warning */
  int32_t   nc=0;            /**< bytes written */
  
  parse_cmd(argc,argv,iname,oname);
  
  fprintf(stderr,"# Open EDF/BDF file %s\n",iname);
  if ((fp=fopen(iname,"r"))==NULL) {
    perror(""); return(-1);
  }
  
  /* read EDF/BDF main and signal headers */
  edf_rd_hdr(fp,&edf);

  /* read EDF/BDF samples */
  edf_rd_samples(fp,&edf);
  
  fclose(fp);

  if (0>1) {  
    /* fix signal dimensions */
    fix_signals(&edf);
  
    /* processing on samples */
    fix_sample_rate(&edf);
  }

  edf_select_channels(&edf,chn);

  if (vb&0x01) {
    edf_prt_hdr(stderr,&edf);
  }
  
  swc=edf_fnd_chn_nr(&edf,"Switch");
  if (swc<0) {
    fprintf(stderr,"# Warning: missing Switch channel\n"); 
  } else {
    fprintf(stderr,"# Switch channel nr %d\n",swc);
  
    /* check battery level of Nexus-10 */
    if ((tchk=edf_chk_battery(&edf,swc))>0.0) {
      fprintf(stderr,"# Warning: battery low at %8.3f [s]\n",tchk);
    } 
  }
  if (md&0x04) {
    if (swc<0) {
      swc = edf_fnd_chn_nr(&edf, "MARKER");
    }
    edf_rpt_switch_edge(stderr, &edf, swc);
  }
  
  if (md&0x02) {
    /* equal sample rate success */
    if (edf_equal_sample_rate(&edf)!=0) {
      fprintf(stderr,"# Error: not possible to make equal sample rates\n");
      exit(-1);
    }
  }
    
  if ((swc>0) && (md&0x01)) {
    cnt=edf_cvt_switch(&edf,swc);
    fprintf(stderr,"# %d button pressed\n",cnt);
  }
  
  if (md!=0x04) {
    fprintf(stderr,"# Write EDF/BDF file %s\n",oname);
    nc=edf_wr_file(oname,&edf,0,edf.NrOfDataRecords-1);
    if (nc<=0) {
      fprintf(stderr,"# Error: writing problems to file %s\n",oname);
    } else {
      fprintf(stderr,"# Info: writing %d bytes to file %s\n",nc,oname);
    }
  }
  
  if (vb&0x02) {
    /* print EDF/BDF samples */
    edf_prt_samples(stderr,&edf,0xFFFFFFFF);
  }
  
  return(0);
} 
