/** @file tms_rd.c
 *
 * @ingroup TMSi
 *
 * $Id:  $
 *
 * @brief Decode Poly5/TMS32 sample file to simple text.
 *
 * $Log: $

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
#include "tmsi.h"

#define VERSION "$Revision: 0.1 $"

#define INDEF         "test1.s00"  /**< default input file */
#define TMS32HDRSIZE        (218)  /**< Poly5/TMS32 header size [byte] */
#define TMS32VERSION        (204)  /**< Poly5/TMS32 version nr */
#define TMS32DESCSIZE       (136)  /**< Poly5/TMS32 channel description size [byte] */
#define TMS32BLKHDRSIZE      (86)  /**< Poly5/TMS32 block header size [byte] */

static int32_t dbg = 0x0000;
static int32_t vb  = 0x0000;

/** tms_cfg usage
 * @return number of printed characters.
*/
int32_t tms32_rd_intro(FILE *fp) {
  
  int32_t nc=0;
  
  nc+=fprintf(fp,"Convert Poly5/TMS32 sample file to text: %s\n",VERSION);
  nc+=fprintf(fp,"Usage: tms32_rd [-i <in>] [-o <out>] [-c <chn>] [-v <vb>] [-d <dbg>] [-h]\n");
  nc+=fprintf(fp,"in   : Poly5/TMS32 sample file (default=%s)\n",INDEF);
  nc+=fprintf(fp,"out  : text output file (default=<in>.txt)\n");
  nc+=fprintf(fp,"h    : show this manual page\n");
  nc+=fprintf(fp,"vb   : verbose switch (default=0x%02X)\n",vb);
  nc+=fprintf(fp,"  0x01 : show header info\n");
  nc+=fprintf(fp,"  0x02 : show raw signal info\n");
  nc+=fprintf(fp,"  0x04 : show signal info\n");
  nc+=fprintf(fp,"  0x08 : show block overhead\n");
  nc+=fprintf(fp,"dbg  : debug value (default=0x%02X)\n",dbg);
  return(nc);
}

/** reads the options from the command line */
static void parse_cmd(int32_t argc, char *argv[], char *iname, char *oname) {

  int32_t i;
  char   *cp;          /**< character pointer */
  char   fname[MNCN];  /**< temp file output name */
 
  strcpy(iname, INDEF); strcpy(oname,"");
  
  for (i=1; i<argc; i++) {
    if (argv[i][0]!='-') {
      fprintf(stderr,"missing - in argument %s\n",argv[i]);
    } else {
      switch (argv[i][1]) {
        case 'i': strcpy(iname,argv[++i]); break;
        case 'o': strcpy(oname,argv[++i]); break;
        case 'v': vb=strtol(argv[++i],NULL,0); break;
        case 'd': dbg=strtol(argv[++i],NULL,0); break;
        case 'h': tms32_rd_intro(stderr); exit(0);
        default : fprintf(stderr,"can't understand argument %s\n",argv[i]); 
          exit(0);
      }
    }        
  }
  /* strip '.smp' extension */
  strcpy(fname,iname);
  if ((cp=strstr(fname,".s00"))!=NULL) { *cp='\0'; }
  if ((cp=strstr(fname,".S00"))!=NULL) { *cp='\0'; }

  /* make 'oname' out of 'iname' without extension */
  if (strlen(oname)<1) {
    sprintf(oname,"%s.txt",fname);
  }
} 

/** main */
int32_t main(int32_t argc, char *argv[]) {

  char iname[MNCN];         /**< input file name */
  FILE   *fpi=NULL;         /**< input file pointer */
  char oname[MNCN];         /**< output ECG file name */
  FILE   *fpo=NULL;         /**< ecg file pointer */
  uint8_t hdr_buf[TMS32HDRSIZE];   /**< header byte buffer */
  tms_hdr32_t hdr;          /**< Poly5/TMS32 sample file header */
  int64_t br,bp;            /**< number of bytes read/parsed */
  int32_t bidx=0;           /**< byte index */
  int32_t i,j,k;            /**< general index */
  uint8_t *desc_buf=NULL;   /**< Poly5/TMS32 channel description buffer */
  tms_signal_desc32_t *desc=NULL;   /**< Poly5/TMS32 channel descriptions */
  float   sample;              /**< sample value as float */  
  float   value;            /**< sample value after calibration */
  uint8_t blk_hdr[TMS32BLKHDRSIZE]; /**< Poly5/TMS32 block header */
  uint8_t *blk;             /**< Poly5/TMS32 block */
  int32_t cnt=0;            /**< sample counter */
  char    *cpl,*cph;        /**< (Lo) and (Hi) character pointers */
  int32_t nrc=0;            /**< number of real channels */
  tms_signal_desc32_t *chn; /**< signal channel info */
  tms_block_desc32_t blk_desc;      /**< block creation info */
  
  parse_cmd(argc,argv,iname,oname);
    
  /* open config file */
  fprintf(stderr,"# Read Poly5/TMS32 TMSi samples from file %s\n",iname); 
  if ((fpi=fopen(iname,"rb"))==NULL) {
    perror(""); exit(1);
  }
  
  /* read Poly5/TMS32 sample file header */
  br=fread(hdr_buf,1,TMS32HDRSIZE,fpi);
  if (br<TMS32HDRSIZE) {
    fprintf(stderr,"# Error: config size %lld < %d\n",br,TMS32HDRSIZE);
    return(-1);
  }

  /* parse buffer 'hdr_buf' to tms_hdr32_t struct 'hdr' */
  bidx=0;
  bp=tms_get_hdr32(hdr_buf,&bidx,&hdr);

  /* version check */
  if (hdr.version!=TMS32VERSION) {
    fprintf(stderr,"# Error missing version number %d\n",TMS32VERSION);
    return(-1);
  }
  
  /* allocate space for all channel descriptions */
  desc=(tms_signal_desc32_t *)calloc(hdr.nrOfSignals,sizeof(tms_signal_desc32_t));
  /* allocate channel description buffer */
  desc_buf=(uint8_t *)calloc(hdr.nrOfSignals*TMS32DESCSIZE,sizeof(uint8_t));

  /* read Poly5/TMS32 signal description headers */
  br=fread(desc_buf,1,hdr.nrOfSignals*TMS32DESCSIZE,fpi);
  /* parse all channel descriptions and count number of real channels 'nrc' */
  nrc=0;
  for (i=0; i<hdr.nrOfSignals; i++) {
    bidx=i*TMS32DESCSIZE;
    tms_get_desc32(desc_buf,&bidx,&desc[i]);
    cpl=strstr(desc[i].signalName,"(Lo)");
    if (cpl==NULL) {
      nrc++; /* 16 bits integer sample or (Hi) part of 32 bits double sample*/
    }
  }

  /* allocate space for all real channels */
  chn=(tms_signal_desc32_t *)calloc(nrc,sizeof(tms_signal_desc32_t));

  /* find real channel info */  
  j=0;
  for (i=0; i<hdr.nrOfSignals; i++) {
    cpl=strstr(desc[i].signalName,"(Lo)");
    cph=strstr(desc[i].signalName,"(Hi)");
    if ((cpl==NULL) && (cph==NULL)) {
      memcpy(&chn[j],&desc[i],sizeof(tms_signal_desc32_t));
      /* 2 byte integer sample */
      chn[j].sampleSize=2;
      /* next signal */
      j++; 
    } else
    if (cph!=NULL) {
      memcpy(&chn[j],&desc[i],sizeof(tms_signal_desc32_t));
      /* 32 bits double sample */
      chn[j].sampleSize=4;
      /* patch signal name */
      k=4;
      while (chn[j].signalName[k]!='\0') {
        chn[j].signalName[k-4]=chn[j].signalName[k];
        k++;
      }
      chn[j].signalName[k-4]='\0';
      /* next signal */
      j++; 
    }    
  }
  
  if (vb&0x01) {
    /* show Poly5/TMS32 header */
    tms_prt_hdr32(stderr,&hdr);
  }
  
  if (vb&0x02) {
    /* show all Poly5/TMS32 raw channel descriptions */
    for (i=0; i<hdr.nrOfSignals; i++) {
      tms_prt_desc32(stderr,&desc[i],(i==0));
    }
  }

  if (vb&0x04) {
    /* show all Poly5/TMS32 channel descriptions */
    for (i=0; i<nrc; i++) {
      tms_prt_desc32(stderr,&chn[i],(i==0));
    }
  }
 /* open output text file */  
  fprintf(stderr,"# Write TMSi measurement samples to text file %s\n",oname);
  if (strcmp(oname,"stdout")==0) { fpo=stdout; } else { fpo=fopen(oname,"w"); }
  if (fpo==NULL) {
    perror(""); exit(1);
  }
 
  /* allocate block buffer */
  blk=(uint8_t *)calloc(hdr.blockSize,sizeof(uint8_t));

  /* print output header */
  fprintf(fpo,"# %9s ","t[s]");
  for (i=0; i<nrc; i++) {
    fprintf(fpo," %7s[%s]",chn[i].signalName,chn[i].unitName);
  }
  fprintf(fpo,"\n");
  
  j=0; cnt=0;
  while ((!feof(fpi)) && (j<hdr.nrOfBlocks)) {
    /* read Poly5/TMS32 block header */
    br=fread(blk_hdr,1,TMS32BLKHDRSIZE,fpi);
    bidx=0;
    tms_get_blk32(blk_hdr,&bidx,&blk_desc);

    if (vb&0x08) {
      tms_prt_blk32(stderr,&blk_desc);
    }
    
    /* read Poly5/TMS32 block data */
    br=fread(blk,1,hdr.blockSize,fpi);
    bidx=0; 
    while (bidx<hdr.blockSize) {
      fprintf(fpo," %f",1.0*cnt/hdr.sampleRate);
      for (i=0; i<nrc; i++) {
        if (chn[i].sampleSize==4) {
          sample=tms_get_float32(blk,&bidx);
        } else {
          sample=(int16_t)tms_get_int(blk,&bidx,chn[i].sampleSize);
        }          
        if (sample==0.0) { 
          chn[i].zeroCnt++;
        }
        value=((sample-chn[i].ADCLow)/(chn[i].ADCHigh-chn[i].ADCLow))*(chn[i].unitHigh-chn[i].unitLow)+chn[i].unitLow;
        fprintf(fpo," %f",value);
      }
      fprintf(fpo,"\n");
      cnt++;
    }
    j++;
  }
  
  /* show channel zero sample count to stderr */
  fprintf(stderr,"# %9s ","Cnt");
  for (i=0; i<nrc; i++) {
    fprintf(stderr," %6s[%s]",chn[i].signalName,chn[i].unitName);
  }
  fprintf(stderr,"\n");

  fprintf(stderr,"# %9s ","zero");
  for (i=0; i<nrc; i++) {
    fprintf(stderr," %10d",chn[i].zeroCnt);
  }
  fprintf(stderr,"\n");
  
  fprintf(stderr,"# %9s ","r.zero[%]");
  for (i=0; i<nrc; i++) {
    fprintf(stderr," %10.2f",100.0*chn[i].zeroCnt/cnt);
  }
  fprintf(stderr,"\n");
         
  /* close input file */
  fclose(fpi);
  /* close output file */
  fclose(fpo); 

  return(0);
}
