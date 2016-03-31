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

#define VERSION "$Revision: edf2txt 0.1 21/09/2010 12:30 $"

#define  INDEF "pp07_01_flanker_01_20090203T114938.bdf"
#define  MNCN     (1024)

int  vb = 0x00;
int dbg = 0x00;

/** edf2txt usage
 * @return number of printed characters.
*/
int32_t edf2hdr_intro(FILE *fp) {
  
  int32_t nc=0;
  
  nc+=fprintf(fp,": %s\n",VERSION);
  nc+=fprintf(fp,"Usage: edf2hdr [-i <in>] [-o <out>] [-v <vb>] [-d <dbg>] [-h]\n");
  nc+=fprintf(fp,"in   : input EDF/BDF file (default=%s)\n",INDEF);
  nc+=fprintf(fp,"out  : output EDF/BDF file (default=<in>.hdr)\n");
  nc+=fprintf(fp,"h    : show this manual page\n");
  nc+=fprintf(fp,"vb   : verbose switch (default=0x%02X)\n",vb);
  nc+=fprintf(fp,"  0x01: show EDF header info\n");
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
        case 'v': vb=strtol(argv[++i],NULL,0); break;
        case 'd': dbg=strtol(argv[++i],NULL,0); break;
        case 'h': edf2hdr_intro(stderr); exit(0);
        default : fprintf(stderr,"can't understand argument %s\n",argv[i]); 
          exit(0);
      }
    }        
  }

  /* make 'oname' out of 'iname' */
  if (strlen(oname)<1) {
    sprintf(oname,"%s.hdr",iname);
  }
} 

/** main */
int32_t main(int32_t argc, char *argv[]) {

  char  iname[MNCN];         /**< input file name */
  char  oname[MNCN];         /**< output ECG file name */
  FILE *fp;
  edf_t edf;
  struct tm t0;
  
  parse_cmd(argc,argv,iname,oname);
  
  fprintf(stderr,"# Open EDF/BDF file %s\n",iname);
  if ((fp=fopen(iname,"r"))==NULL) {
    perror(""); return(-1);
  }
  
  /* read EDF/BDF main and signal headers */
  edf_rd_hdr(fp,&edf);

  fclose(fp);


  t0=*localtime(&edf.StartDateTime);  
  
  /* start output */
  fprintf(stderr,"# Open TXT file %s\n",oname);
  if ((fp=fopen(oname,"w"))==NULL) {
    perror(""); return(-1);
  }  
  
  fprintf(fp," %s %s %04d-%02d-%02d %02d:%02d:%02d\n",iname,edf.PatientId,
    1900+t0.tm_year,t0.tm_mon+1,t0.tm_mday,t0.tm_hour,t0.tm_min,t0.tm_sec);

  /* print headers */
  if (vb&0x01) { edf_prt_hdr(fp,&edf); }

  fclose(fp);
  
  fprintf(stdout," %s %s %04d-%02d-%02d %02d:%02d:%02d\n",iname,edf.PatientId,
    1900+t0.tm_year,t0.tm_mon+1,t0.tm_mday,t0.tm_hour,t0.tm_min,t0.tm_sec);
  
  return(0);
} 
