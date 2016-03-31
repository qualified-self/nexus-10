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
#include <inttypes.h>

#include "edf.h"

#define VERSION "$Revision: edf2txt 0.1 06/02/2009 20:30 $"

#define    INDEF     "pp07_01_flanker_01_20090203T114938.bdf"
#define   CHNDEF     (0x0000)
#define   LBSDEF     "ECG,Event,Resp,EDA"
#define LBHEXDEF     "Status"
#define   VALDEF     "ECG,Resp"
#define     MNCN       (1024)

int32_t  vb = 0x00;
int32_t dbg = 0x00;
int32_t hdr =    1;

/** edf2txt usage
 * @return number of printed characters.
*/
int32_t edf2txt_intro(FILE *fp) {
  
  int32_t nc=0;
  
  nc+=fprintf(fp,": %s\n",VERSION);
  nc+=fprintf(fp,"Usage: edf2txt [-i <in>] [-o <out>] [-c <chn>] [-L <lbs>] [-H <hex>] [-V <val>] [-k <hdr>] [-v <vb>] [-d <dbg>] [-h]\n");
  nc+=fprintf(fp,"in   : input EDF/BDF file (default=%s)\n",INDEF);
  nc+=fprintf(fp,"out  : output EDF/BDF file (default=<in>.txt)\n");
  nc+=fprintf(fp,"chn  : channel switch (default=0x%04X)\n",CHNDEF);
  nc+=fprintf(fp,"lbs  : channel selection via labels (default=%s)\n",LBSDEF);
  nc+=fprintf(fp,"val  : channel selection for float output (default=%s)\n",VALDEF);
  nc+=fprintf(fp,"hex  : channel selection for hex integer representation output (default=%s)\n",LBHEXDEF);
  nc+=fprintf(fp,"hdr  : show header in txt output file (0:off 1:on) (default=%d)\n",hdr);
  nc+=fprintf(fp,"h    : show this manual page\n");
  nc+=fprintf(fp,"vb   : verbose switch (default=0x%02X)\n",vb);
  nc+=fprintf(fp,"  0x01: show EDF header info\n");
  nc+=fprintf(fp,"  0x02: show EDF data\n");
  nc+=fprintf(fp,"  0x04: show sorted EDF data\n");
  nc+=fprintf(fp,"  0x08: show channel selectorsorted EDF data\n");
  nc+=fprintf(fp,"dbg  : debug value (default=0x%02X)\n",dbg);
  return(nc);
}

/** reads the options from the command line */
static void parse_cmd(int32_t argc, char *argv[], char *iname, char *oname, 
 char *lbs, char *lbhex, char *val, uint64_t *chn) {

  int32_t i;
 
  strcpy(iname, INDEF); strcpy(oname,""); strcpy(lbs, LBSDEF); strcpy(val, VALDEF);
  
  for (i=1; i<argc; i++) {
    if (argv[i][0]!='-') {
      fprintf(stderr,"missing - in argument %s\n",argv[i]);
    } else {
      switch (argv[i][1]) {
        case 'i': strcpy(iname,argv[++i]); break;
        case 'o': strcpy(oname,argv[++i]); break;
        case 'c': *chn=strtoll(argv[++i],NULL,0); break;
        case 'k': hdr=strtoll(argv[++i],NULL,0); break;
        case 'L': strncpy(lbs,argv[++i],MNCN-1); break;
        case 'V': strncpy(val,argv[++i],MNCN-1); break;
        case 'H': strncpy(lbhex,argv[++i],MNCN-1); break;
        case 'v': vb=strtol(argv[++i],NULL,0); break;
        case 'd': dbg=strtol(argv[++i],NULL,0); break;
        case 'h': edf2txt_intro(stderr); exit(0);
        default : fprintf(stderr,"can't understand argument %s\n",argv[i]); 
          exit(0);
      }
    }        
  }

  /* make 'oname' out of 'iname' */
  if (strlen(oname)<1) {
    sprintf(oname,"%s.txt",iname);
  }
} 

/** main */
int32_t main(int32_t argc, char *argv[]) {

  char     iname[MNCN];     /**< input file name */
  char     oname[MNCN];     /**< output ECG file name */
  char     lbs[4*MNCN];     /**< channel selection labels */
  char     lbhex[4*MNCN];   /**< channel selection labels */
  char     lbval[4*MNCN];     /**< channel selection labels for float output */
  FILE    *fp;
  edf_t    edf;
  uint64_t chn;
  uint64_t hex = 0LL;
  uint64_t val = 0LL;
  uint8_t  FirstChannel;
  int      i,j;
  float    q[11]={ 0.001,0.01,0.02,0.05,0.25,0.50,0.75,0.95,0.98,0.99,0.999};
  int32_t *qi;
  
  parse_cmd(argc, argv, iname, oname, lbs, lbhex, lbval, &chn);
  
  edf_set_vb(vb>>8);
    
  fprintf(stderr,"# Open EDF/BDF file %s\n",iname);
  if ((fp=fopen(iname,"r"))==NULL) {
    perror(""); return(-1);
  }
  
  /* read EDF/BDF main and signal headers */
  edf_rd_hdr(fp,&edf);

  /* print headers */
  if (vb&0x01) {
    edf_prt_hdr(stderr,&edf);
  }
  
  /* merge channel labels with selection bit vector */
  chn|=edf_chn_selection(lbs,&edf);
  
  hex=edf_chn_selection(lbhex,&edf);

  val=edf_chn_selection(lbval,&edf);

  if (vb&0x08) {
    fprintf(stderr,"# Info: channel selector 0x%012llX\n",chn);
  }
  
  if (chn>0) {
    /* read EDF/BDF samples */
    edf_rd_samples(fp,&edf); 
  } 
  fclose(fp);
  
  if (chn==0LL) { return(0); }

  /* find the first channel to be output */
  FirstChannel = 0;
  while ( ( chn & (1LL<<FirstChannel) ) == 0 ) { FirstChannel++; }

  /* check if all other channels have the same sampling freq as the first */
  for (j=0; j<edf.NrOfSignals; j++)
  {
    if ( chn & (1LL<<j) )
    {
      if ( edf.signal[j].NrOfSamplesPerRecord != edf.signal[FirstChannel].NrOfSamplesPerRecord )
      {
        fprintf(stderr, "Error: channels %d and %d have different sampling frequencies\n", FirstChannel, j );
        exit(1);
      }
    }
  }
 
  if (strlen(oname)>1) {
    /* start output */
    fprintf(stderr,"# Open TXT file %s\n",oname);
    if ((fp=fopen(oname,"w"))==NULL) {
      perror(""); return(-1);
    }
    /* print EDF/BDF samples */
    edf_prt_hex_samples(fp, &edf, chn, hex, val, hdr);
    fclose(fp);
  }
 
  /* allocate space quantile of all channels */
  qi=(int32_t *)calloc(12*edf.NrOfSignals,sizeof(int32_t));
 
  /* calculate quantile of selected channels */
  for (j=0; j<edf.NrOfSignals; j++) {
    if (chn&(1LL<<j)) {
      edf_quantile(&edf,j,q,11,&qi[12*j]);
    }
  }
   
  fprintf(stderr," %2s %9s","i","q[i]");
  for (j=0; j<edf.NrOfSignals; j++) {
    if (chn&(1LL<<j)) {
      fprintf(stderr," %8s",edf.signal[j].Label);
    }
  }
  fprintf(stderr,"\n");
  for (i=0; i<11; i++) {
    fprintf(stderr," %2d %9.3f",i,q[i]);
    /* quantile calculations of selected channels */
    for (j=0; j<edf.NrOfSignals; j++) {
      if (chn&(1LL<<j)) {
        /* print results */
        fprintf(stderr," %8d",qi[12*j+i]);
      }
    }
    fprintf(stderr,"\n");
  }
  /* print quantile difference */
  fprintf(stderr," %12s","quantilediff");
  /* quantile calculations of selected channels */
  for (j=0; j<edf.NrOfSignals; j++) {
    if (chn&(1LL<<j)) {
      /* print results */
      fprintf(stderr," %8d",qi[12*j+6]-qi[12*j+4]);
    }
  }
  fprintf(stderr,"\n");
  
  return(0);
} 
