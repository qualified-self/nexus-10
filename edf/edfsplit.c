/**
  @brief Split an EDF/BDF file into multiple pieces

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>
#include <alloca.h>
#include <math.h>

#include "edf.h"

#define VERSION "edfsplit 0.1 2010/01/20 22:30"

#define  INDEF    "pp07_01_flanker_01_20090203T114938.bdf"
#define  STARTDEF ( 0.0)
#define  STOPDEF  (60.0)
#define  MNCN     (1024)

/* maximum number of segments supported */
#define MAX_SEGMENTS 100

/* structure to specify time intervals */
typedef struct _interval_s_ {
  double wcs,wce;    /**< wall clock start and end */
  double start,stop; /**< start and stop time [s] relative to beginning of file */ 
  char   ext[256];   /**< file extension */
} interval_t;


int32_t  vb       = 0x00;
bool     quiet    = false;
bool     simulate = false;

/** edfsplit usage
 * @return number of printed characters.
 */
int32_t edfsplit_intro(FILE *fp_in) {

  int32_t nc=0;

  nc+=fprintf(fp_in,"%s\n",VERSION);
  nc+=fprintf(fp_in,"Usage: edfsplit [-i <in>] [-o <out>] [-q] [-v <vb>] [-u] [-h]\n");
  nc+=fprintf(fp_in,"  [-s <start,stop<,ext>>] [-S <wcs,wce<,ext>>]\n");
  nc+=fprintf(fp_in,"in   : input EDF/BDF file (default=%s)\n",INDEF);
  nc+=fprintf(fp_in,"out  : output EDF/BDF file (default=<in>.part.bdf)\n");
  nc+=fprintf(fp_in,"start: start of section to output (in seconds) (default=%8.4f)\n",STARTDEF);
  nc+=fprintf(fp_in,"stop : end of section to output (in seconds) (default=%8.4f)\n",STOPDEF);
  nc+=fprintf(fp_in,"wcs  : wall clock start section time format hh:mm:ss start (default=not used)\n");
  nc+=fprintf(fp_in,"wce  : wall clock end section time format hh:mm:ss start (default=not used)\n");
  nc+=fprintf(fp_in,"q    : be quiet\n");
  nc+=fprintf(fp_in,"u    : simUlate, but don't actually write files\n");
  nc+=fprintf(fp_in,"h    : show this manual page\n");
  nc+=fprintf(fp_in,"vb   : verbose switch (default=0x%02X)\n",vb);
  nc+=fprintf(fp_in," 0x01: show segment details\n");
  return(nc);
}

/** convert time string hh:mm:ss.sss or mm:ss.sss or ss.sss to [s]
 * @return time [s]
*/
double time2sec(char *a) {
  
  char   tmp[MNCN];   /**< temp string */
  double  t=0.0;      /**< current time [s] */
  char   *cp = NULL;  /**< character pointer */
  
  if (a!=NULL) {
    strcpy(tmp,a); 
    cp = strtok(tmp,":"); 
    while (cp!=NULL) {
      //fprintf(stderr,"# cp %s\n",cp);
      t=60.0*t + strtod(cp, NULL);
      cp = strtok(NULL,":");
    }
    //fprintf(stderr,"# time %s t %.3f [s]\n",a,t);
  }
  return(t);
}

/** reads the options from the command line */
static void parse_cmd(int32_t argc, char *argv[], char *iname, char *oname,
    interval_t segment[], uint32_t *ns, char *fext ) {

  int32_t  i;
  char    *cp = NULL;          /**< character pointer */
  char     tmp[MNCN];          /**< temp string */
  uint32_t num_segments = 0;
  char     ts[MNCN];
  char     te[MNCN];
  
  /* set defaults */
  strcpy(iname, INDEF); strcpy(oname,""); strcpy(fext,"bdf");
  segment[0].start = STARTDEF;
  segment[0].stop  = STOPDEF;
  sprintf(segment[0].ext,"part");


  for (i=1; i<argc; i++) {
    if (argv[i][0]!='-') {
      fprintf(stderr,"missing - in argument %s\n",argv[i]);
    } else {
      switch (argv[i][1]) {
        case 'i': strcpy(iname,argv[++i]); break;
        case 'o': strcpy(oname,argv[++i]); break;
        case 'q': quiet = true; break;
        case 'u': simulate = true; break;
        case 's':
                  cp = argv[++i];
                  segment[num_segments].start = strtod(   cp, &cp );
                  segment[num_segments].stop  = strtod( ++cp, &cp );
                  //fprintf(stderr,"%s\n",cp);
                  if ((cp++)[0]==',') {
                    strcpy(segment[num_segments].ext, cp);
                  } else {
                    sprintf(segment[num_segments].ext,"part.%d",num_segments);
                  } 
                  if ( ++num_segments > MAX_SEGMENTS ) {
                    fprintf(stderr,"Too many segments!  Maximum is %u\n", MAX_SEGMENTS);
                    exit(1);
                  }
                  break;
        case 'S': 
                  segment[num_segments].start =  0.0; 
                  segment[num_segments].stop  = -1.0; 
                  strncpy(tmp, argv[++i], MNCN); 
                  //fprintf(stderr,"# tmp %s\n",tmp);
                  cp = strtok(tmp,","); strcpy(ts,cp);
                  cp = strtok(NULL,","); strcpy(te,cp);
                  cp = strtok(NULL,","); 
                  if (cp!=NULL) {
                    strcpy(segment[num_segments].ext, cp);
                  } else {
                    sprintf(segment[num_segments].ext,"part.%d",num_segments);
                  } 
                  segment[num_segments].wcs=time2sec(ts);
                  segment[num_segments].wce=time2sec(te);
                  if ( ++num_segments > MAX_SEGMENTS ) {
                    fprintf(stderr,"Too many segments!  Maximum is %u\n", MAX_SEGMENTS);
                    exit(1);
                  }
                  break;
        case 'v': vb=strtol(argv[++i],NULL,0); break;
        case 'h': edfsplit_intro(stderr); exit(0);
        default : fprintf(stderr,"can't understand argument %s\n",argv[i]);
                  exit(0);
      }
    }
  }
  /* strip '.edf' or '.bdf' extension of 'iname' */
  strcpy(tmp,iname);
  if      ((cp=strstr(tmp,".edf"))!=NULL) { *cp='\0'; strcpy(fext,"edf"); }
  else if ((cp=strstr(tmp,".bdf"))!=NULL) { *cp='\0'; strcpy(fext,"bdf"); }
 
  /* make 'oname' out of 'iname' */
  if (strlen(oname)<1) {
    sprintf(oname,"%s",tmp);
  }

  /* make sure the default segment is returned correctly */
  if ( num_segments == 0 ) num_segments = 1;
  *ns = num_segments;

}


/** main */
int32_t main(int32_t argc, char *argv[]) {

  char    iname[MNCN];      /**< input file name */
  char    oname[MNCN];      /**< output file name */
  char    fname[MNCN];      /**< output part file name */
  char    fext[MNCN];       /**< output part file extension */
  FILE   *fp_in, *fp_out;
  edf_t   edf_in, edf_out;
  double record_len;
  interval_t segments[MAX_SEGMENTS];
  uint32_t num_segments;
  uint32_t  i;

  parse_cmd( argc, argv, iname, oname, segments, &num_segments, fext );

  if (!quiet) fprintf(stderr,"# Open EDF/BDF file %s\n",iname);
  if ( (fp_in=fopen(iname,"rb")) == NULL ) {
    perror("");
    return(-1);
  }
  
  /* read EDF/BDF main and signal headers */
  edf_rd_hdr( fp_in, &edf_in );
  record_len = edf_in.RecordDuration;

  int32_t t0 = (int32_t) (edf_in.StartDateTime % (24*60*60));
  /* convert clock wall timing to relative number of seconds from the start */
  for (i=0; i<num_segments; i++) {
    if (segments[i].stop<=0.0) {
      segments[i].start = segments[i].wcs - t0;
      segments[i].stop  = segments[i].wce - t0;
    }  
  }
  
  if (vb&0x01) {
    fprintf(stderr,"# %2s %9s %9s %6s\n", "nr", "start", "stop", "ext");
    for (i=0; i<num_segments; i++) {
      fprintf(stderr," %3d %9.3f %9.3f %6s\n", i, segments[i].start, segments[i].stop, segments[i].ext);
    }
  }

  /* find the channel wiht the largest sample rate/number of samples per record */
  int32_t max_samples_per_record = 0;
  for (int32_t ch=0; ch<edf_in.NrOfSignals; ch++) {
    int32_t sampperrec = edf_in.signal[ch].NrOfSamplesPerRecord;
    if (sampperrec > max_samples_per_record) {
      max_samples_per_record = sampperrec;
    }
  }
  
  time_t tstart = edf_in.StartDateTime;
  
  /* for each of the segments, prepare a separate output file */
  for ( i = 0; i < num_segments; i++ ) {
    /* calculate parameters for the output file */
    uint64_t out_rec_first   = (uint64_t) ( segments[i].start / record_len );
    uint64_t out_rec_last    = (uint64_t) ( segments[i].stop  / record_len );
    uint64_t out_rec_num     = out_rec_last - out_rec_first + 1;

    double real_start = out_rec_first    * record_len;
    double real_end   = (out_rec_last+1) * record_len;

    assert( out_rec_last >= out_rec_first );

    /* print the start and duration in sample numbers and seconds for each channel */
    printf("%u  ", i);
    printf("%.6f %.6f %.6f  ", real_start, real_end, real_end-real_start);
    printf("%8"PRIu64" %8"PRIu64" %8"PRIu64"\n", out_rec_first*max_samples_per_record,
        (out_rec_last+1)*max_samples_per_record-1, out_rec_num*max_samples_per_record);

    if (simulate) { continue; }

    /* create a new filename for outputting this segment */
    if (num_segments>1) {   
      sprintf(fname, "%s%s.%s", oname, segments[i].ext, fext);
    } else {
      sprintf(fname, "%s", oname);
    }

    edf_out = edf_in;
    edf_out.NrOfDataRecords = out_rec_num;
    snprintf(edf_out.PatientId,80,"%s %s",edf_in.PatientId,segments[i].ext);
    
    if (!quiet) fprintf(stderr,"# Open EDF/BDF output file %s\n",fname);
    fp_out = fopen(fname, "wb");
    if ( fp_out == NULL ) {
      perror(""); return(-1);
    }

    if (vb&0x01) {
      fprintf(stderr,"# Segment %"PRIu32": outputting %"PRIu64" - %"PRIu64"\n", 
          i, out_rec_first, out_rec_last);
    }

    /* shift start timestamp */
    edf_out.StartDateTime = tstart + (time_t)round(segments[i].start);
    
    /* now actually write the header and copy the records */
    edf_wr_hdr(fp_out,&edf_out);
    edf_copy_samples( &edf_in, fp_in, &edf_out, fp_out, out_rec_first );

    /* clean up */
    fclose(fp_out);
  }

  fclose(fp_in);

  if (!quiet) fprintf(stderr,"# Done\n");
  return(0);
}
