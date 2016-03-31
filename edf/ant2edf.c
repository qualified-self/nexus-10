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
#include <ctype.h>
#include <inttypes.h>

#include "edf.h"

#define VERSION "ant2edf 0.1 18/12/2009 10:30"

#define MNCN        (1024)

#define INDEF       "pp31_20090918T122951_00000004.bdf"
#define ANTDEF      "pp31_20090918T122951_00000004_ecg.qrs"
#define TXTDEF      ""
#define FMTDEF      FORMAT_SAMP

int32_t  vb =   0x00;
int32_t dbg =   0x00;

/** ant2edf usage
 * @return number of printed characters.
*/
int32_t ant2edf_intro(FILE *fp) {
  
  int32_t nc=0;
  
  nc+=fprintf(fp,"%s\n",VERSION);
  nc+=fprintf(fp,"Usage: ant2edf [-i <in>] [-a <ant>] [-o <out>] [-t <txt>] [-f <fmt>] [-v <vb>] [-d <dbg>] [-h]\n");
  nc+=fprintf(fp,"in   : input EDF/BDF file (default=%s)\n",INDEF);
  nc+=fprintf(fp,"out  : output EDF/BDF file (default=<in>_ant.bdf)\n");
  nc+=fprintf(fp,"ant  : input file with annotation sample numbers (default=%s)\n",ANTDEF);
  nc+=fprintf(fp,"txt  : default annotation text (default=%s)\n",TXTDEF);
  nc+=fprintf(fp,"fmt  : format of the annotation input (default=%i)\n",FMTDEF);
  nc+=fprintf(fp," %i   : annotation positions are specified in sample numbers\n",FORMAT_SAMP);
  nc+=fprintf(fp," %i   : annotation positions are specified in seconds\n",FORMAT_SEC);
  nc+=fprintf(fp," %i   : annotation positions are specified in [ms] since 00:00:00.000\n",FORMAT_MSEC);
  nc+=fprintf(fp,"vb   : verbose switch (default=0x%02X)\n",vb);
  nc+=fprintf(fp," 0x01: show EDF header info\n");
  nc+=fprintf(fp," 0x02: show EDF data\n");
  nc+=fprintf(fp," 0x04: show annotations\n");
  nc+=fprintf(fp," 0x08: show actual annotations\n");
  nc+=fprintf(fp," 0x10: show annotations buffer\n");
  nc+=fprintf(fp," 0x20: show annotation insertion\n");
  nc+=fprintf(fp,"dbg  : debug value (default=0x%02X)\n",dbg);
  nc+=fprintf(fp,"h    : show this manual page\n");
  return(nc);
}

/** reads the options from the command line */
void parse_cmd(int32_t argc, char *argv[], char *iname, char *aname, char *oname,
 char *txt, uint8_t *format) {

  int32_t i;
  char   *cp;          /**< character pointer */
  char    fname[MNCN]; /**< temp file output name */
  int32_t bdf=1;
  int32_t fmt=FMTDEF;
 
  strcpy(iname, INDEF); strcpy(oname,""); strcpy(aname,ANTDEF); strcpy(txt,TXTDEF);
  
  for (i=1; i<argc; i++) {
    if (argv[i][0]!='-') {
      fprintf(stderr,"missing - in argument %s\n",argv[i]);
    } else {
      switch (argv[i][1]) {
        case 'i': strcpy(iname,argv[++i]); break;
        case 'a': strcpy(aname,argv[++i]); break;
        case 'o': strcpy(oname,argv[++i]); break;
        case 't': strcpy(txt,argv[++i]); break;
        case 'f': fmt=strtol(argv[++i],NULL,0); break;
        case 'v': vb=strtol(argv[++i],NULL,0); break;
        case 'd': dbg=strtol(argv[++i],NULL,0); break;
        case 'h': ant2edf_intro(stderr); exit(0);
        default : fprintf(stderr,"can't understand argument %s\n",argv[i]); 
          exit(0);
      }
    }        
  }

  /* strip '.edf' or '.bdf' extension of 'iname' */
  strcpy(fname,iname);
  if ((cp=strstr(fname,".edf"))!=NULL) {*cp='\0'; bdf=0; } else
  if ((cp=strstr(fname,".bdf"))!=NULL) {*cp='\0'; bdf=1; }

  /* check format number */
  if (fmt!=FORMAT_SAMP && fmt!=FORMAT_SEC && fmt!=FORMAT_MSEC)
  {
    fprintf(stderr, "Format %i is unknown\n", fmt);
    exit(1);
  }
  *format = fmt;

  /* make 'oname' out of 'fname' */
  if (strlen(oname)<1) {
    if (bdf==1) {
      sprintf(oname,"%s_ant.bdf",fname);
    } else {
      sprintf(oname,"%s_ant.edf",fname);
    }
  }
} 

int32_t main(int32_t argc, char *argv[]) {

  char        iname[MNCN];  /**< input EDF/BDF file */
  char        aname[MNCN];  /**< input annotation sample position file */
  char        oname[MNCN];  /**< output EDF/BDF file */
  char        txt[MNCN];    /**< annotation text */
  edf_annot_t *ap=NULL;      /**< annotation array */
  int32_t     na=0;         /**< number of annotations in array 'p' */
  int32_t     i;            /**< general index */
  edf_t       edf;          /**< EDF/BDF structure */
  FILE       *fp;           /**< file pointer */
  int32_t     nc=0;         /**< bytes written */
  uint8_t     format;       /**< input format of the annotations (samples or seconds) */
  
  /** reads the options from the command line */
  parse_cmd(argc,argv,iname,aname,oname,txt,&format);
  
  edf_set_vb(vb>>4);
  
  /** check annotation string length */
  if (strlen(txt)>ANNOTTXTLEN) {
    fprintf(stderr,"Warning: annotation text %s longer than %d characters!\n",txt,ANNOTTXTLEN-1);
    txt[ANNOTTXTLEN-1]='\0';
  }

  /* read annotations in file 'aname' */ 
  ap=edf_read_annot(aname,txt,&na,format);
  if (na<1) {
    fprintf(stderr,"# Error: no annotations found\n");
    return(-1);
  }

  /* read EDF/BDF main and signal headers and EDF/BDF samples */
  fprintf(stderr,"# Open EDF/BDF file %s\n",iname);
  if ((fp=fopen(iname,"r"))==NULL) {
    perror(""); return(-1);
  }
  edf_rd_hdr(fp,&edf);
  edf_rd_samples(fp,&edf);
  fclose(fp);

  /* if the annotations were specified in samples, calculate the timestamp in
   * seconds of each annotation */
  if (format==FORMAT_SAMP) {
    /* channel with the most samples per record determines the samplerate */
    int32_t imsr=0; 
    for (int32_t j=1; j<edf.NrOfSignals; j++) {
      if (edf.signal[j].NrOfSamplesPerRecord > edf.signal[imsr].NrOfSamplesPerRecord) {
        imsr=j;
      }
    }
    double fs=edf.signal[imsr].NrOfSamplesPerRecord / edf.RecordDuration;
    fprintf(stderr,"# Info: fs %.1f [samples/s]\n", fs);

    /* calculate all timestamps */
    for (int32_t j=0; j<na; j++) {
      ap[j].sec = ap[j].sc/fs;
    }
  }

  /* if the annotations were specified in [ms] since 00:00:00.000, 
   *  calculate the timestamp in [s] of each annotation */
  if (format==FORMAT_MSEC) {
    /* get start time of recording */
    struct tm ts = *localtime(&edf.StartDateTime);  
    double t0 = ts.tm_sec + 60.0*(ts.tm_min + 60.0*ts.tm_hour);
  
    if (vb&0x04) { fprintf(stderr,"# t0 %.3f [s]\n",t0); }

    /* calculate all timestamps */
    for (int32_t j=0; j<na; j++) {
      ap[j].sec = ap[j].sec - t0;
    }
  }

  /* output annotations  to screen */
  if (vb&0x04) {
    fprintf(stderr,"#%6s %6s %6s %s\n","idx","sc","sec","txt");
    for (i=0; i<na; i++) {
      fprintf(stderr," %6d %8"PRIi64" %f %s\n",i,ap[i].sc,ap[i].sec,ap[i].txt);
    }
  }
  
  if (strlen(txt)>1) {
    /* use command line annotation text i.s.o. text from input file */
    for (int32_t j=0; j<na; j++) {
      strcpy(ap[j].txt,txt);
    }
  }

  /* add 'na' annotations in 'p' into 'edf' */
  edf_add_annot(&edf,ap,na);
  
  /* write EDF/BDF headers and samples */
  fprintf(stderr,"# Write annotated EDF/BDF file %s\n",oname);
  nc=edf_wr_file(oname,&edf,0,edf.NrOfDataRecords-1);
  if (nc<=0) {
    fprintf(stderr,"# Error: writing problems to file %s\n",oname);
  } else {
    fprintf(stderr,"# Info: writing %d bytes to file %s\n",nc,oname);
  }
    
  if (vb&0x02) {
    /* print EDF/BDF samples */
    edf_prt_samples(stderr,&edf,0xFFFFFFFF);
  }

  return(0);
}




















