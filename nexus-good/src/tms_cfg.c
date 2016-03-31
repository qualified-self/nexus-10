/** @file tms_cfg.c
 *
 * @ingroup ECG
 *
 * $Id:  $
 *
 * @brief decode/encode TMSi config file to/from text.
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "tmsi.h"

#define VERSION "$Revision: 0.2 2008-01-04 $"

#define INDEF       "config.ini"  /**< default input file */
#define CVTDEF               (0)  /**< default conversion bin->txt */

static int32_t dbg = 0x0000;
static int32_t vb  = 0x0000;

/** tms_cfg usage
 * @return number of printed characters.
*/
int32_t tms_cfg_intro(FILE *fp) {
  
  int32_t nc=0;
  
  nc+=fprintf(fp,"Convert TMSi config file to/from text: %s\n",VERSION);
  nc+=fprintf(fp,"Usage: tms_cfg [-i <in>] [-o <out>] [-c <cvt>] [-v <vb>] [-d <dbg>] [-h]\n");
  nc+=fprintf(fp,"in   : TMSi file (default=%s)\n",INDEF);
  nc+=fprintf(fp,"out  : TMSi text output file (default=<in>.txt)\n");
  nc+=fprintf(fp,"cvt  : conversion 0: bin->txt  1: txt->bin (default=0) %d\n",CVTDEF);
  nc+=fprintf(fp,"h    : show this manual page\n");
  nc+=fprintf(fp,"vb   : verbose switch (default=0x%02X)\n",vb);
  nc+=fprintf(fp,"  0x01 : show configuration\n");
  nc+=fprintf(fp,"  0x02 : show patient/measurement info of configuration\n");
  nc+=fprintf(fp,"dbg  : debug value (default=0x%02X)\n",dbg);
  return(nc);
}

/** reads the options from the command line */
static void parse_cmd(int32_t argc, char *argv[], char *iname, char *oname, int32_t *cvt) {

  int32_t i;
  char   *cp;          /**< character pointer */
  char   fname[MNCN];  /**< temp file output name */
 
  strcpy(iname, INDEF); strcpy(oname,""); *cvt=CVTDEF;
  
  for (i=1; i<argc; i++) {
    if (argv[i][0]!='-') {
      fprintf(stderr,"missing - in argument %s\n",argv[i]);
    } else {
      switch (argv[i][1]) {
        case 'i': strcpy(iname,argv[++i]); break;
        case 'o': strcpy(oname,argv[++i]); break;
        case 'c': *cvt=strtol(argv[++i],NULL,0); break;
        case 'v': vb=strtol(argv[++i],NULL,0); break;
        case 'd': dbg=strtol(argv[++i],NULL,0); break;
        case 'h': tms_cfg_intro(stderr); exit(0);
        default : fprintf(stderr,"can't understand argument %s\n",argv[i]); 
          exit(0);
      }
    }        
  }
  /* strip known file extension */
  strcpy(fname,iname);
  if ((cp=strstr(fname,".ini"))!=NULL) { 
    *cvt=0; *cp='\0';
  }
  if ((cp=strstr(fname,".INI"))!=NULL) { 
    *cvt=0; *cp='\0';
  }
  if ((cp=strstr(fname,".smp"))!=NULL) { 
    *cvt=0; *cp='\0';
  }
  if ((cp=strstr(fname,".SMP"))!=NULL) { 
    *cvt=0; *cp='\0';
  }
  if ((cp=strstr(fname,".txt"))!=NULL) { 
    *cvt=1; *cp='\0';
  }
  if ((cp=strstr(fname,".TXT"))!=NULL) { 
    *cvt=1; *cp='\0';
  }

  /* make 'oname' out of 'iname' without extension */
  if (strlen(oname)<1) {
    if (*cvt==0) {
      sprintf(oname,"%s.txt",fname);
    } else {
      sprintf(oname,"%s.ini",fname);
    }
  }
} 

/** main */
int32_t main(int32_t argc, char *argv[]) {

  char iname[MNCN];      /**< input file name */
  FILE   *fpi=NULL;      /**< input file pointer */
  char oname[MNCN];      /**< output ECG file name */
  FILE   *fpo=NULL;      /**< ecg file pointer */
  int32_t cvt;           /**< conversion selector */
  uint8_t header[0x600]; /**< config */
  int32_t br,bp;         /**< number of bytes read/parsed */
  int32_t bidx=0;        /**< byte index */
  tms_config_t cfg;      /**< TMSi config */
 
  parse_cmd(argc,argv,iname,oname,&cvt);
  
  tms_set_vb(vb);
 
  if (cvt==0) { 
    /* config bin -> txt */
    
    /* open config file */
    fprintf(stderr,"# Read TMSi configuration from binary file %s\n",iname); 
    if ((fpi=fopen(iname,"rb"))==NULL) {
      perror(""); exit(1);
    }
    /* read config file */
    br=fread(header,1,sizeof(header),fpi);

    /* close input file */
    fclose(fpi);

    if (br<TMSCFGSIZE) {
      fprintf(stderr,"# Error: config size %d < %d\n",br,TMSCFGSIZE);
      return(-1);
    }

    /* parse buffer 'hdr' to TMSi config struct 'cfg' */
    bidx=0;
    bp=tms_get_cfg(header,&bidx,&cfg);

    /* version check */
    if (cfg.version!=0x0314) {
      fprintf(stderr,"# Error missing version number 0x0314\n");
      return(-1);
    }

    /* open output text file */  
    fprintf(stderr,"# Write TMSi configuration to text file %s\n",oname);
    if (strcmp(oname,"stdout")==0) { fpo=stdout; } else { fpo=fopen(oname,"w"); }
    if (fpo==NULL) {
      perror(""); exit(1);
    }
    
    /* print TMSi config struct 'cfg' at stderr */
    tms_prt_cfg(fpo,&cfg,1);
    
    /* close output file */
    fclose(fpo);
     
  } else {
    /* config txt -> bin */
    
    /* open config file */
    fprintf(stderr,"# Read TMSi configuration from text file %s\n",iname); 
    if ((fpi=fopen(iname,"r"))==NULL) {
      perror(""); exit(1);
    }
    
    /* read config text file */
    br=tms_rd_cfg(fpi,&cfg);
    
    /* close input file */
    fclose(fpi);

    /* version check */
    if (cfg.version!=0x0314) {
      fprintf(stderr,"# Error missing version number 0x0314\n");
      return(-1);
    }
    
    bidx=0;
    tms_put_cfg(header,&bidx,&cfg);

    /* open output binary file */  
    fprintf(stderr,"# Write TMSi configuration to binary file %s\n",oname);
    if (strcmp(oname,"stdout")==0) { fpo=stdout; } else { fpo=fopen(oname,"wb"); }
    if (fpo==NULL) {
      perror(""); exit(1);
    }
    
    br=fwrite(header,1,TMSCFGSIZE,fpo);
    
    /* close output file */
    fclose(fpo);
    
  }
  return(0);
}
