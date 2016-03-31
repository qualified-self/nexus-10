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

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <math.h>

#include "edf.h"

#define VERSION "$Revision: edf2rsp 0.1 05/08/2011 21:30 $"

#define  INDEF "pp00_01_flanker_20090302T161501.bdf"
#define  MNCN                (1024)  /**< maximum of characters in file name */

int32_t  vb = 0x00;
int32_t dbg = 0x00;

/** edf2rsp usage
 * @return number of printed characters.
*/
int32_t edf2rsp_intro(FILE *fp) {
  
  int32_t nc=0;
  
  nc+=fprintf(fp,": %s\n",VERSION);
  nc+=fprintf(fp,"Usage: edf2rsp [-i <in>] [-o <out>] [-r <rpt>] [-v <vb>] [-d <dbg>] [-h]\n");
  nc+=fprintf(fp,"in   : input EDF/BDF file (default=%s)\n",INDEF);
  nc+=fprintf(fp,"out  : bdf file with filtered respiration signal + minima and maxima annotations (default=<in>.rsp)\n");
  nc+=fprintf(fp,"rpt  : output file with respiration minima and maxima time + level (default=<in>.rsp)\n");
  nc+=fprintf(fp,"vb   : verbose switch (default=0x%02X)\n",vb);
  nc+=fprintf(fp,"  0x01: show EDF header info\n");
  nc+=fprintf(fp,"  0x02: show filtering of respiration signal\n");
  nc+=fprintf(fp,"  0x04: show event data\n");
  nc+=fprintf(fp,"  0x08: show peak detection internals\n");
  nc+=fprintf(fp,"dbg  : debug value (default=0x%02X)\n",dbg);
  return(nc);
}

/** reads the options from the command line */
static void parse_cmd(int32_t argc, char *argv[], char *iname, char *oname, char *rname) {

  int32_t i;
  char   *cp;          /**< character pointer */
  char    fname[MNCN]; /**< temp file output name */
 
  strcpy(iname, INDEF); strcpy(oname,""); strcpy(rname,"");
  
  for (i=1; i<argc; i++) {
    if (argv[i][0]!='-') {
      fprintf(stderr,"missing - in argument %s\n",argv[i]);
    } else {
      switch (argv[i][1]) {
        case 'i': strcpy(iname,argv[++i]); break;
        case 'o': strcpy(oname,argv[++i]); break;
        case 'r': strcpy(rname,argv[++i]); break;
        case 'v': vb=strtol(argv[++i],NULL,0); break;
        case 'd': dbg=strtol(argv[++i],NULL,0); break;
        case 'h': edf2rsp_intro(stderr); exit(0);
        default : fprintf(stderr,"can't understand argument %s\n",argv[i]); 
          exit(0);
      }
    }        
  }
  /* strip '.edf' or '.bdf' extension of 'iname' */
  strcpy(fname,iname);
  if ((cp=strstr(fname,".edf"))!=NULL) {*cp='\0';} else
  if ((cp=strstr(fname,".bdf"))!=NULL) {*cp='\0';}

  /* make 'rname' out of 'iname' */
  if (strlen(rname)<1) {
    sprintf(rname,"%s.rsp",fname);
  }
  /* make 'oname' out of 'iname' */
  if (strlen(oname)<1) {
    sprintf(oname,"%s_rsp.bdf",fname);
  }
} 

/** Look for event codes in 'Status' channel of 'edf'
 * @return number of event changes found
*/
int32_t event_timing(FILE *fp, edf_t *edf, int32_t chn) {

  int32_t i,i0;     /**< sample index */
  int32_t nst,st;   /**< (new) status */
  int32_t cnt = 0;  /**< event counter */
  float   fs;       /**< sample frequency */
  int32_t dsc  = 1; /**< minimum samples between events */
 
  /* sample frequency */
  fs=edf->signal[chn].NrOfSamplesPerRecord / edf->RecordDuration;
  fprintf(fp,"# fs %12.2f\n",fs);
  fprintf(fp," %8s %9s %9s %4s\n","idx","t","dur","code");
  
  /* run length coding of status signals */
  st=edf->signal[chn].data[0]&0xFF; i0=0;
  for (i=1; i<edf->signal[chn].NrOfSamples; i++) {
    nst = edf->signal[chn].data[i]&0xFF;
    if (nst != st) {
      if (i-i0 > dsc) {
        if (vb&0x04) {
          fprintf(stderr," %8d %9.4f %9.4f %3d\n", cnt, i0/fs, (i-i0)/fs, st);
        }
        if (st > 0) {
          fprintf(fp," %8d %9.4f %9.4f %4d\n", cnt, i0/fs, (i-i0)/fs, st);
        }
        cnt++;
      }
      st = nst;
      i0 = i; 
    } 
  }
  
  return(cnt);
}

/** Simple low-pass forward and backward filter (filtfilt) on 
 *   channel 'chn' of 'edf' struct.
 * @return 0 always
*/
int32_t rsp_filtfilt(edf_t *edf, int32_t chn) {

  double  fs;                 /**< sample frequency [Hz] */
  double  gain;               /**< gain of channel 'chn' */
  int32_t lenShift1;          /**< length filter 1 */
  int32_t iShift1 = 0;        /**< shift index filter 1 */
  double  *Shift1 = NULL;     /**< shift register 1 */
  double  Sum1 = 0.0;         /**< sum of all samples in 'Shift1' */
  int32_t lenShift2;          /**< length filter 2 */
  int32_t iShift2 = 0;        /**< shift index filter 2 */
  double  *Shift2 = NULL;     /**< shift register 2 */
  int32_t i;                  /**< sample index */
  int32_t yi;                 /**< integer representation of 'sample' */
  int32_t ovf;                /**< overflow bit of 'sample' */
  double  sample;             /**< respiration sample */
  double  fsample;            /**< filtered respiration sample */
  double  smean;              /**< sample mean */
  double  filtered = 0.0;     /**< sum of all samples in 'Shift2' */

  /* sample frequency of channel 'chn'*/
  fs=edf->signal[chn].NrOfSamplesPerRecord / edf->RecordDuration;
   
  /* set filter lengths */
  lenShift1 = (int32_t)round(fs/2);
  lenShift2 = (int32_t)round(fs/3);
  /* allocate space for shift registers and initialize to zero */
  Shift1=(double *)calloc(lenShift1, sizeof(double));
  Shift2=(double *)calloc(lenShift2, sizeof(double));
  
  /* gain of channel 'chn' */
  gain=(float)(edf->signal[chn].PhysicalMax - edf->signal[chn].PhysicalMin) /
              (edf->signal[chn].DigitalMax  - edf->signal[chn].DigitalMin);
  
  /* forward filtering */
  for (i=0; i < edf->signal[chn].NrOfSamples; i++) {

    /* get integer value and overflow bit of sample 'i' in channel 'chn' */
    yi = edf_get_integer_overflow_value(edf, chn, i, &ovf);
    sample = gain*yi;

    /* Low pass filter built from 2-stage summing and shift registers */
    Sum1 -= Shift1[iShift1];
    Shift1[iShift1] = sample;
    Sum1 += sample;

    filtered -= Shift2[iShift2];
    Shift2[iShift2] = Sum1;
    filtered += Sum1;

    if (++iShift1 >= lenShift1) { iShift1 = 0; }
    if (++iShift2 >= lenShift2) { iShift2 = 0; }
    
    fsample = filtered/(lenShift1*lenShift2);
    if (vb&0x02) {
      fprintf(stderr,"#f %5d %.1f %.1f\n",i, sample, fsample/gain);
    }

    /* set new integer value with previous overflow bit */
    edf_set_integer_overflow_value(edf, fsample, ovf, chn, i, gain);
  }
  
  /* sample mean */
  smean = 0.0;
  /* backward filtering to make result phase neutral */
  for (i=edf->signal[chn].NrOfSamples-1; i>=0; i--) {

    /* get integer value and overflow bit of sample 'i' in channel 'chn' */
    yi = edf_get_integer_overflow_value(edf, chn, i, &ovf);
    sample = gain*yi;

    /* Low pass filter built from 2-stage summing and shift registers */
    Sum1 -= Shift1[iShift1];
    Shift1[iShift1] = sample;
    Sum1 += sample;

    filtered -= Shift2[iShift2];
    Shift2[iShift2] = Sum1;
    filtered += Sum1;

    if (++iShift1 >= lenShift1) { iShift1 = 0; }
    if (++iShift2 >= lenShift2) { iShift2 = 0; }

    fsample = filtered/(lenShift1*lenShift2);

    smean += fsample;

    if (vb&0x02) {
      fprintf(stderr,"#b %5d %.1f %.1f\n",i, sample, fsample/gain);
    }
    
    /* set new integer value with previous overflow bit */
    edf_set_integer_overflow_value(edf, fsample, ovf, chn, i, gain);
  }
  smean /= edf->signal[chn].NrOfSamples;
  
  /* make filtered respiration signal DC free */
  for (i=0; i < edf->signal[chn].NrOfSamples; i++) {
    /* get integer value and overflow bit of sample 'i' in channel 'chn' */
    sample = gain*edf_get_integer_overflow_value(edf, chn, i, &ovf);
    /* subtract mean value */
    sample -= smean;
    /* set new integer value with previous overflow bit */
    edf_set_integer_overflow_value(edf, sample, ovf, chn, i, gain);
  }

  fprintf(stderr,"# Info: fs %9.3f [Hz] mean %12.3f [uV]\n", fs, smean);

  free(Shift1); 
  free(Shift2);
  
  return(0);
}

/** Compare two integers (passed by reference); used by qsort() below
 * @return 0 if a and b are equal, -1 if a is smaller, 1 if a is larger than b
 */
static inline int32_t comp_func(const void* ap, const void* bp) {

  int32_t a = *((int32_t *) ap);
  int32_t b = *((int32_t *) bp);

  if (a<b) { return(-1); } else
  if (a>b) { return( 1); } else { return(0); }
}

/** Simple peak-to-peak detection on respiration channel 'chn' of 'edf' struct
 * with epochs of 'dt' [sec].
 * @return median peak-to-peak over all epochs
*/
double rsp_p2p(edf_t *edf, int32_t chn, double dt) {

  double  fs;          /**< sample frequency [Hz] */
  double  gain;        /**< gain of channel 'chn' */
  int32_t i,j,k;       /**< general index */
  int32_t ns;          /**< sample per epoch */
  int32_t ne;          /**< number of epochs */
  int32_t sample;      /**< sample */
  int32_t ymin,ymax;   /**< min and max of this epoch */
  int32_t *p2p = NULL; /**< peak-to-peak per epoch */
  double  md=0.0;      /**< p2p median */
  
  /* sample frequency of channel 'chn'*/
  fs=edf->signal[chn].NrOfSamplesPerRecord / edf->RecordDuration;
   
  /* gain of channel 'chn' */
  gain=(float)(edf->signal[chn].PhysicalMax - edf->signal[chn].PhysicalMin) /
              (edf->signal[chn].DigitalMax  - edf->signal[chn].DigitalMin);

  /* samples epoch */            
  ns = (int32_t)round(dt*fs);
  /* number of epochs */
  ne = edf->signal[chn].NrOfSamples/ns;
  /* allocate space for the 'ne' p2p values */
  p2p = (int32_t *)calloc(ne+1, sizeof(int32_t));
  
  if (vb&0x10) { 
    fprintf(stderr,"# Info: ns %d ne %d\n", ns, ne); 
    fprintf(stderr,"# %2s %9s\n", "ec", "p2p [uV]"); 
  }
  
  i=0; j=0; k=0;
  while (i < edf->signal[chn].NrOfSamples) {
    sample = edf_get_integer_value(edf, chn, i);
    if (j==ns) {
      /* end of epoch reached */
      if (i>0) {
        p2p[k] = ymax - ymin; 
        if (vb&0x10) { fprintf(stderr,"# %2d %9.1f\n", k, gain*p2p[k]); }
        k++; 
      }
      ymin = sample; ymax = sample; j=0;
    } else {
      j++;
    }
    /* range check */
    if (sample < ymin) { ymin = sample; }
    if (sample > ymax) { ymax = sample; }
    /* next sample */ 
    i++;
  }
  
  /* sort data */
  qsort( p2p, ne, sizeof(p2p[0]), comp_func);

  /* select median p2p */
  md = gain*p2p[ne/2];
  /* select smallest */
  md = gain*p2p[0];
  
  /* free p2p array */
  free(p2p);
  
  return(md);
}


/** Check for respiration minima or maxima on respiration channel 'name'
 * @returns 1 on even extreme, -1 on odd extreme and else 0
*/
int32_t rsp_detect(FILE *fp, edf_t *edf, char *name) {

  char    newname[MNCN];       /**< new channel name */
  int32_t chn;                 /**< channel number of respiration signal */
  double  fs;                  /**< sample frequency [Hz] */
  double  gain;                /**< gain of channel 'chn' */
  int32_t i;                   /**< sample index */
  int32_t yi;                  /**< integer representation of 'filtered' */
  int32_t ovf;                 /**< overflow bit of 'filtered' */
  double  filtered = 0.0;      /**< sum of all samples in 'Shift2' */

  double  mindist =  1500.0;   /**< minimum distance between local minimum and maximum */
  double  maxdist = 10000.0;   /**< maximum distance between local minimum and maximum */

  double  tmpDist = 0.0;       // Angular distance of tmporary breath extreme
  double  tmpExtreme = 0.0;    // tmporary breath extreme
  double  extreme = 0.0;       // Last extreme
  int32_t period = -1;         // Current sample offset from last extreme, -1 triggers init
  int32_t toggle = 1;          // Half-beat toggle
  int32_t lastPeriod = 0;      // Previous beat slice period in samples
  double  filteredPeriod = 0;  // Current average breathing period
  double  rate;                // Current breathing rate per minute (available, but not used)

  double  dist;                /**< distance (from extreme or from tmpExtreme) */
  edf_annot_t *p = NULL;       /**< EDF/BDF annotation array */
  int32_t chunksize = 32;      /**< chunk size of growing annotation array 'p' */
  int32_t maxsize = chunksize; /**< maximum size 'p' */
  int32_t cnt = 0;             /**< annotation counter */
  int32_t nxt = 0;             /**< sample counter since last found local extreme */
  int32_t idx = 0;             /**< index of last found local extreme */
  double  dt;                  /**< respiration period [s] */
  
  /* make new signal name */
  sprintf(newname,"%s%s", name, "flt" );

  /* copy channel 'name' */
  if ((chn = edf_cpy_chn(edf, name, newname)) < 0) {
    return(0);
  }
 
  /* simple forward and backward low pass filtering */
  rsp_filtfilt(edf, chn);

  /* sample frequency of channel 'chn'*/
  fs=edf->signal[chn].NrOfSamplesPerRecord / edf->RecordDuration;
   
  /* gain for channel 'chn' */
  gain=(float)(edf->signal[chn].PhysicalMax - edf->signal[chn].PhysicalMin) /
              (edf->signal[chn].DigitalMax  - edf->signal[chn].DigitalMin);

  /* get median p2p of all 20 [sec] epochs */
  mindist =  0.1*rsp_p2p(edf, chn, 20.0);
  maxdist = 10.0*mindist;
  
  fprintf(stderr,"# Info: mindist %9.3f [uV] maxdist %9.3f [uV]\n", mindist, maxdist);
  
  /* allocate space for the annotation array 'p' */
  p = (edf_annot_t *)calloc(maxsize, sizeof(edf_annot_t));

  if (vb&0x08) { 
    fprintf(stderr," %6s %6s %6s %12s %12s %12s %12s %12s %6s\n", 
     "i", "nxt", "period", "filtered", "extreme", "dist", "tmpExtreme", "tmpDist", "state"); 
  }
  /* check all respiration samples */
  cnt=0;
  for (i=0; i < edf->signal[chn].NrOfSamples; i++) {

    /* get integer value and overflow bit of sample 'i' in channel 'chn' */
    yi = edf_get_integer_overflow_value(edf, chn, i, &ovf);
    filtered = gain*yi;

    if (vb&0x08) { 
      fprintf(stderr," %6d %6d %6d %12.3f %12.3f %12.3f %12.3f %12.3f", 
       i, nxt, period, filtered, extreme, dist, tmpExtreme, tmpDist); 
    }
    if (++period == 0) { // On first round
      extreme = filtered;
      tmpDist = -1; // Force tmpExtreme update on next sample 
      if (vb&0x08) { fprintf(stderr," first\n"); }
    } else {
      dist = fabs(filtered - extreme);
      if (dist > tmpDist) { // Typical: still moving away from extreme, move tmpExtreme
        tmpExtreme = filtered;
        tmpDist = dist;
        if (vb&0x08) { fprintf(stderr," move\n"); }
        nxt = 0;
     } else { // Moving backwards
        dist = fabs(filtered - tmpExtreme);
        if (dist > mindist) { // Moved back more than mindist from tmpExtreme: new extreme found
          if (tmpDist < maxdist) { // Discard big motion artifacts for rate
            // Full period consists of 2 slices (typically unequal length in- and exhale):
            // So add previous slice period and filter a bit further:
            filteredPeriod += 0.2*((double)(period + lastPeriod)/fs - filteredPeriod);
            rate = 60.0/filteredPeriod;
            lastPeriod = period;
          }
          if (vb&0x08) { fprintf(stderr," fnd\n"); }
         
          /* check available storage space */
          if (cnt>=maxsize) {
            /* extend with 'chunksize' items */
            maxsize += chunksize;
            p = (edf_annot_t *)realloc(p, maxsize*sizeof(edf_annot_t));
          }
          
          /* store new found extreme */
          p[cnt].sc = idx; 
          p[cnt].sec = idx/fs; 
          if (cnt==0) { 
            strncpy(p[cnt].txt, "unkRSP", ANNOTTXTLEN-1); 
          } else {
            strncpy(p[cnt].txt, 
             (toggle == 1
             ? "maxRSP" : "minRSP"), ANNOTTXTLEN-1);
          }
          cnt++;
         
          period = 0; // Shift time base
          extreme = tmpExtreme; // Remember new extreme
          tmpExtreme = filtered; // Move tmpExtreme
          tmpDist = dist;

          toggle *= -1;
        } else {
          if (nxt==0) { idx=i; }
          if (vb&0x08) { fprintf(stderr," next\n"); }
          nxt++;
        }
      }
    }
  }

  /* merge 'cnt' annotation in 'p' into 'edf' struct */
  edf_add_annot(edf, p, cnt);
 
  fprintf(fp, " %3s %9s %9s %9s %12s %7s\n", "nr","sc","t","annot","level","dt");

  for (i=0; i<cnt; i++) {    
    /* get integer value and overflow bit of sample 'i' in channel 'chn' */
    yi = edf_get_integer_overflow_value(edf, chn, p[i].sc, &ovf);

    fprintf(fp," %3d %9lld %9.3f %9s %12.3f", i, p[i].sc, p[i].sec, p[i].txt, gain*yi);
    if (i>1) {
      dt = p[i].sec - p[i-2].sec;
      fprintf(fp," %7.3f\n", dt);
    } else {
      fprintf(fp," %7s\n", "NA");
    }
  }
  
  /* free annotation array */
  free(p);
  
  return(cnt);
}

/** main */
int32_t main(int32_t argc, char *argv[]) {

  char    iname[MNCN]; /**< input file name */
  char    oname[MNCN]; /**< report file name */
  char    rname[MNCN]; /**< output EDF/BDF file name */
  FILE   *fp;          /**< file pointer for input EDF file and output timing file */
  edf_t   edf;         /**< EDF struct */
  int32_t cnt=0;       /**< respiration period counter */
  int32_t nc=0;        /**< bytes written to EDF/BDF output file */
  
  parse_cmd(argc, argv, iname, oname, rname);
  
  edf_set_vb(vb>>8);
  
  fprintf(stderr,"# Info: open EDF/BDF file %s\n",iname);
  if ((fp=fopen(iname,"r"))==NULL) {
    perror(""); return(-1);
  }
  
  /* read EDF/BDF main and signal headers */
  edf_rd_hdr(fp,&edf);

  /* read EDF/BDF samples */
  edf_rd_samples(fp,&edf);
  
  fclose(fp);
  
  /* chech ranges on all channels */
  edf_range_chk(&edf);
  /* show range check results */
  edf_range_cnt(&edf);
  
  fprintf(stderr,"# Info: write respiration timing file to %s\n",rname);
  if ((fp=fopen(rname,"w"))==NULL) {
    perror(""); return(-1);
  }
  cnt = rsp_detect(fp, &edf, "Resp");
  
  /* print edf header */
  if (vb&0x01) { edf_prt_hdr(stderr, &edf); }
  
  fprintf(stderr,"# Info: found %d respiration periods\n",cnt);

  fclose(fp);
    
  fprintf(stderr,"# Info: write filtered respiraton + annotations to file %s\n", oname);
  nc = edf_wr_file(oname, &edf, 0, edf.NrOfDataRecords-1);
  if (nc <= 0) {
    fprintf(stderr,"# Error: writing problems to file %s\n",oname);
  } else {
    fprintf(stderr,"# Info: writing %d bytes to file %s\n", nc, oname);
  }
  
  /* remove all edf struct */
  edf_free(&edf); 
    
  return(0);
} 
