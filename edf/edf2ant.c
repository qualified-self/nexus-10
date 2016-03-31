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

#define VERSION "edf2ant 0.1 23/03/2010 14:00"

#define  INDEF "pp31_20090918T122951_00000004.bdf"
#define ANTDEF "pp31_20090918T122951_00000004.ant"
#define   MNCN     (1024)
#define  MDDEF        (1)

int32_t  vb =  0x00;
int32_t dbg =  0x00;

/** edffix usage
 * @return number of printed characters.
*/
int32_t edf2ant_intro(FILE *fp) {
  
  int32_t nc=0;
  
  nc+=fprintf(fp,"%s\n",VERSION);
  nc+=fprintf(fp,"Usage: edf2ant [-i <in>] [-o <ant>] [-m <md>] [-v <vb>] [-d <dbg>] [-h]\n");
  nc+=fprintf(fp,"in   : input EDF/BDF file (default=%s)\n",INDEF);
  nc+=fprintf(fp,"ant  : output annotation file (default=%s)\n",ANTDEF);
  nc+=fprintf(fp,"md   : report mode 0: t,ibi,sc,txt 1: t,ibi (kubios) 2: sc,txt (default=%d)\n",MDDEF);
  nc+=fprintf(fp,"h    : show this manual page\n");
  nc+=fprintf(fp,"vb   : verbose switch (default=0x%02X)\n",vb);
  nc+=fprintf(fp," 0x01: show EDF header info\n");
  nc+=fprintf(fp," 0x02: show EDF data\n");
  nc+=fprintf(fp,"dbg  : debug value (default=0x%02X)\n",dbg);
  return(nc);
}

/** reads the options from the command line */
static void parse_cmd(int32_t argc, char *argv[], char *iname, char *oname, int32_t *md) {

  int32_t i;
  char   *cp;          /**< character pointer */
  char    fname[MNCN]; /**< temp file output name */
 
  strcpy(iname, INDEF); strcpy(oname,""); *md=MDDEF;
  
  for (i=1; i<argc; i++) {
    if (argv[i][0]!='-') {
      fprintf(stderr,"missing - in argument %s\n",argv[i]);
    } else {
      switch (argv[i][1]) {
        case 'i': strcpy(iname,argv[++i]); break;
        case 'o': strcpy(oname,argv[++i]); break;
        case 'm': *md=strtol(argv[++i],NULL,0); break;
        case 'v': vb=strtol(argv[++i],NULL,0); break;
        case 'd': dbg=strtol(argv[++i],NULL,0); break;
        case 'h': edf2ant_intro(stderr); exit(0);
        default : fprintf(stderr,"can't understand argument %s\n",argv[i]); 
          exit(0);
      }
    }        
  }

  /* strip '.edf' or '.bdf' extension of 'iname' */
  strcpy(fname,iname);
  if ((cp=strstr(fname,".edf"))!=NULL) {*cp='\0'; } else
  if ((cp=strstr(fname,".bdf"))!=NULL) {*cp='\0'; }

  /* make 'oname' out of 'fname' */
  if (strlen(oname)<1) {
    sprintf(oname,"%s.ant",fname);
  }
} 

/** main */
int32_t main(int32_t argc, char *argv[]) {

  char    iname[MNCN];         /**< input file name */
  char    oname[MNCN];         /**< output ECG file name */
  FILE   *fp;                  /**< file pointer */
  edf_t   edf;                 /**< EDF/BDF struct */
  edf_annot_t *p;              /**< pointer to annotations struct */
  int32_t n;                   /**< number of annotations */
  int32_t i;                   /**< general index */
  double  ibi;                 /**< inter beat interval [s] */
  int32_t md;                  /**< report mode */
  
  parse_cmd(argc,argv,iname,oname,&md);
  
  fprintf(stderr,"# Open EDF/BDF file %s\n",iname);
  if ((fp=fopen(iname,"r"))==NULL) {
    perror(""); return(-1);
  }
  
  edf_set_vb(vb>>4);
  
  /* read EDF/BDF main and signal headers */
  edf_rd_hdr(fp,&edf);

  /* read EDF/BDF samples */
  edf_rd_samples(fp,&edf);
  
  fclose(fp);

  if (vb&0x01) {
    edf_prt_hdr(stderr,&edf);
  }
  
  if (vb&0x02) {
    /* print EDF/BDF samples */
    edf_prt_samples(stderr,&edf,0xFFFFFFFF);
  }
  
  p=edf_get_annot(&edf,&n);
  
  fprintf(stderr,"# found %d EDF/BDF annotations\n",n);
  
  fprintf(stderr,"# Open annotation file %s\n",oname);
  if ((fp=fopen(oname,"w"))==NULL) {
    perror(""); return(-1);
  }
  
  for (i=0; i<n; i++) {
    if (i==0) {
      ibi=p[1].sec - p[  0].sec;
    } else {
      ibi=p[i].sec - p[i-1].sec;
    }
    if (md<2) {
      fprintf(fp,"%f %f",p[i].sec,ibi);
    }
    if ((md==0) | (md==2)) {
      fprintf(fp," %" PRIu64 " %s",p[i].sc,p[i].txt);
    }
    fprintf(fp,"\n");
  }
  
  fclose(fp);

  return(0);
} 
