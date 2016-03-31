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

#define _LARGEFILE64_SOURCE /* for ftello64 */

#ifdef _MSC_VER
  #include "../nexus/inc/win32_compat.h"
  #include "../nexus/inc/stdint_win32.h"
#else
  #include <stdint.h>
  #include <inttypes.h>
  #include <alloca.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <malloc.h>

#include "edf.h"


task_def_t task_def[NTASK] = { 
  {0xEA,0xEB,"EO"  ,300.0,  0},
  {0xEC,0xED,"EC"  ,300.0,  0},
  {0xE0,0xE1,"Flk" ,309.6,144},
  {0xE8,0xE9,"NFB1",480.0,  0},
  {0xE2,0xE3,"Stop",288.0,144},
  {0xE8,0xE9,"NFB2",480.0,  0},
  {0xE4,0xE5,"Strp",400.0,168},
  {0xE8,0xE9,"NFB3",480.0,  0},
  {0xE6,0xE7,"Nbck",300.0,144} 
};

static int32_t vb = 0x0000;

/** Set verbose value in module EDF
 * @return previous verbose value
*/
int32_t edf_set_vb(int32_t new_vb) {
  
  int32_t vb_old=vb;
  
  vb=new_vb;
  return(vb_old);
}


/** Get message 'msg' of at most 'n' characters out of 'buf' string.
 * @return number of character in 'msg'
 * @note string 'msg' should have place for at least 'n+1' characters
*/
int32_t edf_get_str(char *buf, int32_t *idx, char *msg, int32_t n) {

  int32_t i; /**< general index */
  
  for (i=0; i<n; i++) {
    msg[i]=buf[(*idx)+i];
  }
  msg[n]='\0';
  /* kill trailing space's */
  i=n-1;
  while ((i>=0) && (msg[i]==' ')) {
    msg[i]='\0'; i--;
  }
  if (vb&0x01) {
    fprintf(stderr,"# edf_get_str: %s\n",msg);
  }
  (*idx)+=n;
  return(i);
}

/** Get integer of at most 'n' characters out of 'buf' string.
 * @return number 
*/
int32_t edf_get_int(char *buf, int32_t *idx, int32_t n) {

  int32_t i;        /**< general index */
  int32_t a;
  char   *msg = alloca(n+1);
  
  for (i=0; i<n; i++) {
    msg[i]=buf[(*idx)+i];
  }
  msg[n]='\0';
  a=atoi(msg);
  if (vb&0x01) {
    fprintf(stderr,"# edf_get_int: msg %s int %d\n",msg,a);
  }
  (*idx)+=n;
  return(a);
}

/** Get integer of at most 'n' characters out of 'buf' string.
 * @return number 
*/
double edf_get_double(char *buf, int32_t *idx, int32_t n) {

  int32_t i;        /**< general index */
  double  a;
  char   *msg = alloca(n+1);
  
  for (i=0; i<n; i++) {
    msg[i]=buf[(*idx)+i];
  }
  msg[n]='\0';
  a=strtod(msg,NULL);
  if (vb&0x01) {
    fprintf(stderr,"# edf_get_double: msg %s double %f\n",msg,a);
  }
  (*idx)+=n;
  return(a);
}

/** Print double 'a' in exactly 'w' characters to file 'fp'
 *   use e-notation to fit too big or small numbers
 * @note 'w' should be >= 7
 * @return number of printed characters
*/
int32_t edf_prt_double(FILE *fp, int32_t w, double a) {

  int32_t i,n;       /**< general index, print length in characters */

#ifdef _MSC_VER
  char    buf[256];  /**< print buffer, should be big enough */
#else
  char    buf[2*w];  /**< print buffer, should be big enough */
#endif

  if (w<7) {
    fprintf(stderr,"# Error: Impossible to print double %f in %d characters\n", a, w);
  }
  sprintf(buf,"%-*g", w, a);
  n = strlen(buf);
  if (n>w) {
    /* shorting the output to 'w' characters: NO rounding !!! */
    if (buf[n-4]=='e') {
      /* shift 4 character of exponent ie 'e-04' forward */
      for (i=0; i<4; i++) {
        buf[w-4+i] = buf[n-4+i];
      }
    }
    buf[w]='\0';
  }  
  fprintf(fp,"%s",buf);
  return(n);
}


/** Get integer sample value in edf struct of channel 'chn' and sample index 'idx'.
 * @return new integer value
*/
int32_t edf_get_integer_value(edf_t *edf, int32_t chn, int32_t idx) {

  int32_t   n; /**< number of bytes (8 bits) per sample */
  int32_t msk; /**< integer value msk */
  int32_t  yi; /**< integer value */
  int32_t shl; /**< bits shift left */
  
  if ((chn<0) || (chn>=edf->NrOfSignals)) {
    fprintf(stderr,"# Error: edf_get_integer_value channel nr %d out of range\n",chn);
    return(-1);
  }
  if ((idx<0) || (idx>=edf->signal[chn].NrOfSamples)) {
    fprintf(stderr,"# Error: edf_get_integer_value sample index %d out of range\n",idx);
    return(-1);
  }

  if (edf->bdf==1) { n=3; } else { n = 2; }
  msk = (1<<(8*n))-1;
  /* get integer value and drop overflow bit */
  yi = edf->signal[chn].data[idx] & msk;
  /* preserve sign bit */
  shl = 8*(4-n);
  yi = (yi<<shl)>>shl;
  return(yi);
} 

/** Get overflow value in edf struct of channel 'chn' and sample index 'idx'.
 * @return 0 on no overflow, OVERFLOWBIT on overflow
*/
int32_t edf_get_overflow_value(edf_t *edf, int32_t chn, int32_t idx) {

  if ((chn<0) || (chn>=edf->NrOfSignals)) {
    fprintf(stderr,"# Error: edf_get_integer_value channel nr %d out of range\n",chn);
    return(-1);
  }
  if ((idx<0) || (idx>=edf->signal[chn].NrOfSamples)) {
    fprintf(stderr,"# Error: edf_get_integer_value sample index %d out of range\n",idx);
    return(-1);
  }

  /* get overflow bit */
  return(edf->signal[chn].data[idx] & OVERFLOWBIT);
} 

/** Get integer sample value and overflow 'ovf' in edf struct of channel 'chn' and sample index 'idx'.
 * @return new integer value
*/
int32_t edf_get_integer_overflow_value(edf_t *edf, int32_t chn, int32_t idx, int32_t *ovf) {

  int32_t   n; /**< number of bytes (8 bits) per sample */
  int32_t msk; /**< integer value msk */
  int32_t  yi; /**< integer value */
  int32_t shl; /**< bits shift left */
  
  if ((chn<0) || (chn>=edf->NrOfSignals)) {
    fprintf(stderr,"# Error: edf_get_integer_value channel nr %d out of range\n",chn);
    return(-1);
  }
  if ((idx<0) || (idx>=edf->signal[chn].NrOfSamples)) {
    fprintf(stderr,"# Error: edf_get_integer_value sample index %d out of range\n",idx);
    return(-1);
  }

  /* get overflow bit */
  (*ovf) = edf->signal[chn].data[idx] & OVERFLOWBIT;

  if (edf->bdf==1) { n=3; } else { n = 2; }
  msk = (1<<(8*n))-1;
  /* get integer value */
  yi = edf->signal[chn].data[idx] & msk;
  /* preserve sign bit */
  shl = 8*(4-n);
  yi = (yi<<shl)>>shl;
  /* return integer value */
  return(yi);
} 

/** Set new integer sample value of 'ynew' and overflow bit 'ovf' in edf struct 
 *   of channel 'chn' and sample index 'idx' with 'gain'.
 * @return new integer value
*/
int32_t edf_set_integer_overflow_value(edf_t *edf, float ynew, int32_t ovf, int32_t chn, int32_t idx, float gain) {

  int32_t msk;  /**< integer value mask */
  int32_t yi;   /**< integer sample value */
  
  if ((chn<0) || (chn>=edf->NrOfSignals)) {
    fprintf(stderr,"# Error: edf_set_new_integer_value channel nr %d out of range\n",chn);
    return(-1);
  }
  if ((idx<0) || (idx>=edf->signal[chn].NrOfSamples)) {
    fprintf(stderr,"# Error: edf_set_new_integer_value sample index %d out of range\n",idx);
    return(-1);
  }
  
  /* clip result and set overflow bit accordingly */
  if (ynew < edf->signal[chn].PhysicalMin) {
    ynew = (float) edf->signal[chn].PhysicalMin; ovf |= OVERFLOWBIT; 
  } else 
  if (ynew > edf->signal[chn].PhysicalMax) {
    ynew = (float) edf->signal[chn].PhysicalMax; ovf |= OVERFLOWBIT; 
  }

  if (edf->bdf==1) { msk = (1<<24)-1; } else { msk = (1<<16)-1; }
  
  /* new integer sample value */
  yi = (int32_t)round(ynew/gain);
  
  /* set new sample and overflow value */
  edf->signal[chn].data[idx]=(ovf | (yi & msk));
  
  return(yi);
}

/** Translate labels in 'a' to selection bitvector 
 * @return selection bitvector as uint64_t
*/
uint64_t edf_chn_selection(char *a, edf_t *edf ) {

  uint64_t chn=0;
  int32_t  i,j;
  char    *token;

  token=strtok(a,","); i=0;
  while (token!=NULL) {
    if (vb&0x04) { fprintf(stderr,"# nr %d label %s at channel",i,token); }
    j=edf_fnd_chn_nr(edf,token);
    if (vb&0x04) { fprintf(stderr," %d\n",j); }
    if (j>=0) {
      chn|=(1LL<<j);
    }
    token=strtok(NULL,","); i++;
  }
  return(chn);
}

/** Get filesize of 'fp'
 * @return file size [byte]
*/
int64_t edf_get_file_size(FILE *fp) {

  int64_t where,file_size;

  /* remember current position */
  where=ftello64(fp);
  /* seek to end of file */
  fseeko64(fp, 0L, SEEK_END);
  file_size=ftello64(fp);
  /* seek to orginal position */
  fseeko64(fp, where, SEEK_SET);
  
  return(file_size);

}

/** Get EDF/BDF record size
 * @return record size [byte]
*/
int32_t edf_get_record_size(const edf_t *edf) {

  int32_t i;
  int32_t sampleCnt=0;
  int32_t recordSize=0;
  edf_signal_t *sig=edf->signal;

  for (i=0; i<edf->NrOfSignals; i++) {
    sampleCnt+=sig[i].NrOfSamplesPerRecord;
  }
  
  if (edf->bdf==0) {
    recordSize=2*sampleCnt;
  } else {
    recordSize=3*sampleCnt;
  }
  return(recordSize);  
}

/** Calculate the number of records in EDF/BDF file 'fp' with main header 'edf'
 *   and signal headers 'sig' by filesize / record size.
 * @return number of records.
*/
int32_t edf_get_record_cnt(FILE *fp, edf_t *edf) {

  int64_t fileLength;
  int64_t numRecords;
  int32_t recordSize=0;

  if (edf->NrOfDataRecords<=0) {

    recordSize=edf_get_record_size(edf);
    fileLength=edf_get_file_size(fp);

    if (recordSize<1) {
      fprintf(stderr,"# Error: edf_fix_data_records recordSize %d < 1 !!!\n",
          recordSize);
      return(-1);
    }

    /* use file size to estimate number of data records */
    numRecords=(fileLength - edf->NrOfHeaderBytes) / recordSize; 
    if (numRecords>INT32_MAX) {
      fprintf(stderr, "Too many records found!\n");
      abort();
    }
    edf->NrOfDataRecords=(uint32_t) numRecords;

  }
  return(edf->NrOfDataRecords);
}


/** Fix first character of version string in EDF/BDF file 'fp'
 * @return 0 on success, <0 on failure
 */
int32_t edf_fix_hdr_version(FILE *fp) {

  int64_t where;    /**< file location */
  char buf[8];      /**< read buffer */
  int64_t br;       /**< bytes read */
  
  if (fp==NULL) { return(-1); }
  
  /* remember current position */
  where=ftello64(fp);
  
  /* seek to 'NrOfDataRecords' position */
  if (fseek(fp, 0L, SEEK_SET)!=0) { return(-2); }
  
  if ((br=fread(buf,1,sizeof(buf),fp))!=sizeof(buf)) { return(-3); }
  
  if (strncmp(&buf[1],"BIOSEMI",7)==0) {
    if (((unsigned char)buf[0])!=0xFF) {
      fprintf(stderr,"# Fix BIOSEMI BDF header version\n");
      buf[0]=0xFF;
      fseek(fp, 0L, SEEK_SET);
      br=fwrite(buf,1,1,fp);
    }
  } else 
  if (strncmp(&buf[1],"       ",7)==0) {
    if (buf[0]!='0') {
      fprintf(stderr,"# Fix EDF header version\n");
      buf[0]='0';
      br=fwrite(buf,1,1,fp);
    }
  }  
    
  /* seek to orginal position */
  if (fseeko64(fp, where, SEEK_SET)!=0) { return(-4); }
  
  return(0);
}

/** Set number of records 'rec_cnt' in EDF/BDF file 'fp'
 * @return 0 on success, <0 on failure
*/
int32_t edf_set_record_cnt(FILE *fp, int32_t rec_cnt) {

  int64_t where;
  
  if (fp==NULL) { return(-1); }
  
  /* remember current position */
  where=ftello64(fp);
  
  /* seek to 'NrOfDataRecords' position */
  if (fseeko64(fp, 236, SEEK_SET)!=0) { return(-2); }

  /* print record count into correct position */
  fprintf(fp,"%-8d",rec_cnt);
   
  /* seek to orginal position */
  if (fseeko64(fp, where, SEEK_SET)!=0) { return(-3); }
  
  return(0);
}

/** Read EDF/BDF signal headers from file 'fp' into 'sig' 
 *   by using info of 'edf'.
 * @return number of characters parsed.
*/
int32_t edf_rd_signal_hdr(FILE *fp, edf_t *edf) {

  const size_t buf_size = sizeof(char) * 256*edf->NrOfSignals;
  char *buf = alloca( buf_size ); /**< character buffer */
  size_t br;                      /**< character read */
  int32_t idx=0;        /**< character pointer in 'buf' */
  int32_t i,j;          /**< signal and item counter */
  edf_signal_t *sig=edf->signal;

  assert(sizeof(char)==1);
  
  if ((br=fread(buf,1,buf_size,fp))!=buf_size) {
    fprintf(stderr,"# EDF/BDF signal header too small %zd\n",br);
    return(-1);
  }
  
  for (j=0; j<10; j++) {
    for (i=0; i<edf->NrOfSignals; i++) {
      switch (j) {
        case 0:
          edf_get_str(buf,&idx,sig[i].Label,16);
          break;
        case 1:
          edf_get_str(buf,&idx,sig[i].TransducerType,80);
          break;
        case 2:
          edf_get_str(buf,&idx,sig[i].PhysicalDimension,8);
          break;
        case 3:
          sig[i].PhysicalMin=edf_get_double(buf,&idx,8);
          break;
        case 4:
          sig[i].PhysicalMax=edf_get_double(buf,&idx,8);
          break;
        case 5:
          sig[i].DigitalMin=edf_get_int(buf,&idx,8);
          break;
        case 6:
          sig[i].DigitalMax=edf_get_int(buf,&idx,8);
          break;
        case 7:
          edf_get_str(buf,&idx,sig[i].Prefiltering,80);
          break;
        case 8:
          sig[i].NrOfSamplesPerRecord=edf_get_int(buf,&idx,8);
          break;
        case 9:
          edf_get_str(buf,&idx,sig[i].Reserved,32);
          sig[i].NrOfSamples=0;
          break;
      }
    }
  }

  if (edf->NrOfDataRecords<=0) {
    fprintf(stderr,"# Warning: NrOfDataRecord %d <= 0\n",edf->NrOfDataRecords);
    edf_get_record_cnt(fp,edf);
  }
  
  return(idx);
}

/** Read EDF/BDF main + signal headers from file 'fp' into 'edf'
 * @return number of characters parsed.
*/
int32_t edf_rd_hdr(FILE *fp, edf_t *edf) {

  char    buf[256];  /**< character buffer */
  int64_t br=-1;     /**< bytes read */
  int32_t idx=1;     /**< character index, skip first character */
  struct  tm cal;    /**< broken calendar time */
  
  if ((br=fread(buf,1,sizeof(buf),fp))!=sizeof(buf)) {
    fprintf(stderr,"# EDF/BDF header too small %"PRIu64"d\n",br);
    return(-1);
  }
  
//  edf_get_str(buf,&idx,edf->Version,7); /**< will kill leading spaces */
  strncpy(edf->Version,&buf[idx],7); edf->Version[7]='\0'; idx+=7;

  if (strncmp("BIOSEMI",edf->Version,7)==0) {
    edf->bdf=1; /* BDF */
  } else 
  if (strncmp("       ",edf->Version,7)==0) {
    edf->bdf=0; /* EDF */
  } else {
    fprintf(stderr,"# Error: edf_rd_hdr Unknown EDF/BDF %s version found\n",
      edf->Version); 
    edf->bdf=-1; /* Unknown EDF/BDF version */
  }
  
  edf_get_str(buf,&idx,edf->PatientId,80);
  edf_get_str(buf,&idx,edf->RecordingId,80);
  
  /* get start date and time */
  cal.tm_mday=edf_get_int(buf,&idx,2);     /* day of the month [1,31] */ 
  idx++; /* skip dot */ 
  cal.tm_mon =edf_get_int(buf,&idx,2)-1;   /* months since January [0,11] */
  idx++; /* skip dot */
  cal.tm_year=edf_get_int(buf,&idx,2)+100; /* year since 1900 */  
  cal.tm_hour=edf_get_int(buf,&idx,2);     /* hours since midnight [0,23] */
  idx++; /* skip dot */ 
  cal.tm_min =edf_get_int(buf,&idx,2);     /* minutes since the hour [0,59] */
  idx++; /* skip dot */
  cal.tm_sec =edf_get_int(buf,&idx,2);     /* seconds since the minute [0,59] */  
  cal.tm_isdst = -1;
  /* convert from broken calendar to calendar */
  edf->StartDateTime=mktime(&cal);

  edf->NrOfHeaderBytes=edf_get_int(buf,&idx,8);
  edf_get_str(buf,&idx,edf->DataFormatVersion,44);
  edf->NrOfDataRecords=edf_get_int(buf,&idx,8);
  edf->RecordDuration=edf_get_double(buf,&idx,8);
  edf->NrOfSignals=edf_get_int(buf,&idx,4);

  /* allocate space for all signals */
  edf->signal=(edf_signal_t *)calloc(edf->NrOfSignals,sizeof(edf_signal_t));

  /* read all signal headers */
  idx+=edf_rd_signal_hdr(fp,edf);
  
  return(idx);
}

/** Free the edf_t data structure that was allocated by edf_rd_hdr()
*/
void edf_free( edf_t * edf ) 
{
  int j;

  if ( edf == NULL )  return;
  for (j=0; j<edf->NrOfSignals; j++) {
    if ( edf->signal[j].data != NULL ) free( edf->signal[j].data );
  }
  if ( edf->signal != NULL )  free( edf->signal );
}

/** Write EDF/BDF main header 'edf' to file 'fp' 
 * @return number of characters printed.
*/
int32_t edf_wr_hdr(FILE *fp, edf_t *edf) {

  int32_t nc=0;
  struct  tm t0;
  edf_signal_t *sig=edf->signal;
  int32_t i,j;

  /* main header */
  if (edf->bdf==1) {
    nc+=fprintf(fp,"%1c",0xFF);
  } else {
    nc+=fprintf(fp,"%1c",'0');
  }
  nc+=fprintf(fp,"%-7s",edf->Version);
  nc+=fprintf(fp,"%-80s",edf->PatientId);
  nc+=fprintf(fp,"%-80s",edf->RecordingId);  
  t0=*localtime(&edf->StartDateTime);  
  nc+=fprintf(fp,"%02d.%02d.%02d%02d.%02d.%02d",
    t0.tm_mday,t0.tm_mon+1,(t0.tm_year%100),t0.tm_hour,t0.tm_min,t0.tm_sec);
  nc+=fprintf(fp,"%-8d",edf->NrOfHeaderBytes);
  nc+=fprintf(fp,"%-44s",edf->DataFormatVersion);
  nc+=fprintf(fp,"%-8d",edf->NrOfDataRecords);
  nc+=edf_prt_double(fp,8,edf->RecordDuration);
  nc+=fprintf(fp,"%-4d",edf->NrOfSignals);
  /* signal headers */
  for (j=0; j<10; j++) {
    for (i=0; i<edf->NrOfSignals; i++) {
      switch (j) {
        case 0: nc+=fprintf(fp,"%-16s",sig[i].Label); break;
        case 1: nc+=fprintf(fp,"%-80s",sig[i].TransducerType); break;
        case 2: nc+=fprintf(fp,"%-8s",sig[i].PhysicalDimension); break;
        case 3: nc+=edf_prt_double(fp,8,sig[i].PhysicalMin); break;
        case 4: nc+=edf_prt_double(fp,8,sig[i].PhysicalMax); break;
        case 5: nc+=fprintf(fp,"%-8d",sig[i].DigitalMin); break;
        case 6: nc+=fprintf(fp,"%-8d",sig[i].DigitalMax); break;
        case 7: nc+=fprintf(fp,"%-80s",sig[i].Prefiltering); break;
        case 8: nc+=fprintf(fp,"%-8d",sig[i].NrOfSamplesPerRecord); break;
        case 9: nc+=fprintf(fp,"%-32s",sig[i].Reserved); break;
        default:
          break;
      }
    }
  }     
  return(nc);
}

/** Print EDF/BDF main header 'edf' to file 'fp' 
 * @return number of characters printed.
*/
int32_t edf_prt_hdr(FILE *fp, edf_t *edf) {

  int32_t nc=0;
  struct tm t0;
  edf_signal_t *sig=edf->signal;
  int32_t i;

  nc+=fprintf(fp,"Version          : %s\n",edf->Version);
  nc+=fprintf(fp,"bdf              : %d\n",edf->bdf);
  nc+=fprintf(fp,"PatientId        : %s\n",edf->PatientId);
  nc+=fprintf(fp,"RecordingId      : %s\n",edf->RecordingId);  
  t0=*localtime(&edf->StartDateTime);  
  nc+=fprintf(fp,"Start Date Time  : %04d-%02d-%02d %02d:%02d:%02d\n",
    1900+t0.tm_year,t0.tm_mon+1,t0.tm_mday,t0.tm_hour,t0.tm_min,t0.tm_sec);
  nc+=fprintf(fp,"NrOfHeaderBytes  : %d\n",edf->NrOfHeaderBytes);
  nc+=fprintf(fp,"DataFormatVersion: %s\n",edf->DataFormatVersion);
  nc+=fprintf(fp,"NrOfDataRecords  : %d\n",edf->NrOfDataRecords);
  nc+=fprintf(fp,"RecordDuration   : %f\n",edf->RecordDuration);
  nc+=fprintf(fp,"NrOfSignals      : %d\n",edf->NrOfSignals);

  nc+=fprintf(fp,"#%3s %16s %8s %10s %10s %8s %8s %8s %10s %10s %10s\n",
    "nr","Label","Dim","Phy_Min","Phy_Max","Dig_Min","Dig_Max","NSamples",
    "Transducer","Prefilter","Reserved");
  for (i=0; i<edf->NrOfSignals; i++) {
    nc+=fprintf(fp," %3d %16s %8s %10.1f %10.1f %8d %8d %8d %10s %10s %10s\n",
      i,
      sig[i].Label,            
      sig[i].PhysicalDimension,
      sig[i].PhysicalMin,      
      sig[i].PhysicalMax,      
      sig[i].DigitalMin,       
      sig[i].DigitalMax,       
      sig[i].NrOfSamplesPerRecord,      
      sig[i].TransducerType,   
      sig[i].Prefiltering,     
      sig[i].Reserved);
  }  
  return(nc);
}

/** Read sample of 'n' bytes long from file 'fp'
*/
int32_t edf_rd_int(FILE *fp, int32_t n) {

  uint8_t *buf = alloca(n);
  int32_t a=0;
  size_t br=0;
  int32_t i;
  
  if ((br=fread(buf,1,n,fp))!=(size_t) n) {
    fprintf(stderr,"# edf_rd_int: missing bytes %zd\n",br);
    return(-1);
  }
  /* convert to integer */
  for (i=n-1; i>=0; i--) {
    a=(a<<8) + buf[i];
  }
  return(a);
}
 
/** Write sample 'a' of 'n' bytes long to file 'fp'
*/
int32_t edf_wr_int(FILE *fp, int32_t a, int32_t n) {

  uint8_t *buf = alloca(n);
  size_t br=0;
  int32_t i;
  
  /* convert integer to char array */
  for (i=0; i<n; i++) {
    buf[i]=a & 0xFF; a=(a>>8);
  }
  if ((br=fwrite(buf,1,n,fp))!=(size_t) n) {
    fprintf(stderr,"# edf_wr_int: missing bytes %zd\n",br);
    return(-1);
  }
  return(a);
}
 
/** Read EDF/BDF samples from file 'fp' into 'edf'
 * @return number of bytes read
*/
int64_t edf_rd_samples(FILE *fp, edf_t *edf) {
 
  int32_t  k;  /**< block counter */
  int32_t  j;  /**< sample counter per block */
  int32_t  i;
  int32_t  idx;
  int32_t  sampleSize=2; /**< default size of 16 bits EDF samples */
  int64_t  tsc=0;        /**< total sample counter */
  char     aname[32];    /**< EDF/BDF Annotation name */
  int32_t *achn;         /**< EDF/BDF Annotation channel array */
  int32_t  acnt=0;       /**< number of annotation channels */

  if (edf->bdf==1) { 
    strcpy(aname,"BDF Annotations");
    sampleSize=3; 
  } else {
    strcpy(aname,"EDF Annotations");
    sampleSize=2; 
  }
  achn=(int32_t *)calloc(edf->NrOfSignals, sizeof(int32_t));

  /* allocate space for all samples */
  for (j=0; j<edf->NrOfSignals; j++) {
    if ( edf->signal[j].data != NULL ) free( edf->signal[j].data );
    /* allocate space for 'sc' samples of signal[j] */
    edf->signal[j].NrOfSamples=edf->NrOfDataRecords * edf->signal[j].NrOfSamplesPerRecord;
    /* is this an annotation channel */
    if (strcmp(edf->signal[j].Label,aname)==0) { 
      edf->signal[j].data=(int32_t *)calloc(edf->NrOfDataRecords * ANNOTRECSIZE/4, sizeof(int32_t));
      achn[j]=1; 
      acnt++;
    } else {
      achn[j]=0;
      edf->signal[j].data=(int32_t *)calloc(edf->signal[j].NrOfSamples, sizeof(int32_t));
    }
  }
  if (acnt>0) {
    fprintf(stderr,"# Info: Found %d EDF/BDF Annotations channels\n",acnt);
  }
  
  /* read all samples */
  for (k=0; k<edf->NrOfDataRecords; k++) {
    for (j=0; j<edf->NrOfSignals; j++) {
      if (achn[j]==1) {
        //idx=k*edf->signal[j].NrOfSamplesPerRecord;
        idx=k*ANNOTRECSIZE/4;
        tsc+=((fread(&(edf->signal[j].data[idx]),1,
               edf->signal[j].NrOfSamplesPerRecord*sampleSize,fp))/sampleSize);
      } else {
        idx=k*edf->signal[j].NrOfSamplesPerRecord;
        for (i=0; i<edf->signal[j].NrOfSamplesPerRecord; i++) {
          edf->signal[j].data[idx+i]=edf_rd_int(fp,sampleSize);
          tsc+=sampleSize;
        }
      }
    }
  }
  
  /* set NrOfSamplesPerRecord according ANNOTRECSIZE for correct writing edf files */
  for (j=0; j<edf->NrOfSignals; j++) {
    if (achn[j]==1) {
      edf->signal[j].NrOfSamplesPerRecord = ANNOTRECSIZE/sampleSize; 
    }
  }

  free(achn);
  return(tsc);
}

/** Write EDF/BDF samples in 'edf' to file 'fp'
 * starting at record index 'first' up to 'last'
 * @return number of bytes written
*/
int32_t edf_wr_samples(FILE *fp, edf_t *edf, int32_t first, int32_t last) {
 
  int32_t k;  /**< block counter */
  int32_t j;  /**< sample counter per block */
  int32_t i;
  int32_t idx;
  int32_t sampleSize=2; /**< default size of 16 bits EDF samples */
  int32_t tsc=0;        /**< total sample counter */
  int32_t acn;          /**< channel number of EDF Annotations */
 
  if (edf->bdf==1) { 
    sampleSize=3; 
    /* get annotation channel number */
    acn=edf_fnd_chn_nr(edf,"BDF Annotations");
  } else {
    sampleSize=2; 
    /* get annotation channel number */
    acn=edf_fnd_chn_nr(edf,"EDF Annotations");
  }
  if (acn>=0) {
    fprintf(stderr,"# Info: EDF/BDF Annotations channel %d\n",acn);
  }
  
  /* write samples for record 'first' up to 'last' */
  for (k=first; k<=last; k++) {
    for (j=0; j<edf->NrOfSignals; j++) {
      if (j==acn) {
        idx=k*ANNOTRECSIZE/4;
        tsc+=fwrite(&(edf->signal[j].data[idx]),1,
               edf->signal[j].NrOfSamplesPerRecord*sampleSize,fp);
      } else {
        idx=k*edf->signal[j].NrOfSamplesPerRecord;
        for (i=0; i<edf->signal[j].NrOfSamplesPerRecord; i++) {
          edf_wr_int(fp,edf->signal[j].data[idx+i],sampleSize);
          tsc+=sampleSize;
        }
      }
    }
  }
  return(tsc);
}


/** Write EDF/BDF struct in 'edf' to file 'fname'
 * starting at record index 'first' up to 'last'
 * @return number of bytes written
*/
int32_t edf_wr_file(char *fname, edf_t *edf, int32_t first, int32_t last) {

  FILE *fp;       /**< file pointer */
  int32_t nc=0;   /**< number of bytes written */
  int32_t nrec=0; /**< orginal NrOfDataRecords */
  
  if (edf==NULL) { return(-2); }
  
  /* open output file */
  if ((fp=fopen(fname,"w"))==NULL) {
    perror(fname); return(-1);
  }
  
  /* preserve orginal NrOfDataRecords */
  nrec=edf->NrOfDataRecords;
  /* set actual NrOfDataRecords */
  edf->NrOfDataRecords=last-first+1;
  
  /* write edf header */
  nc+=edf_wr_hdr(fp,edf);
  /* write edf data samples */
  nc+=edf_wr_samples(fp,edf,first,last);
  /* close output file */
  fclose(fp);

  /* restore orginal NrOfDataRecords */
  edf->NrOfDataRecords=nrec;
 
  return(nc);
}

/** Print EDF/BDF integer samples of selected channels 'chn'
 *   hexadecimal mode 'hex' and float printing mode 'val'
 *   print header hdr==1, no header hdr==0
 * @return number of printed characters
*/
int32_t edf_prt_hex_samples(FILE *fp, edf_t *edf, uint64_t chn, uint64_t hex, uint64_t val, int32_t hdr) {
 
  int32_t j;            /**< sample counter per block */
  int32_t i;
  int32_t nc=0;
  uint8_t FirstChannel;
  int32_t yi;           /**< integer representation of sample 'y' */
  int32_t ovf;          /**< sample overflow bit */
  float   gain[64];
  
  /* print all signal labels */
  if (hdr) { nc+=fprintf(fp," %8s","sc"); }
  for (j=0; j<edf->NrOfSignals; j++) {
    gain[j] = (float) ((edf->signal[j].PhysicalMax - edf->signal[j].PhysicalMin) /
                       (edf->signal[j].DigitalMax  - edf->signal[j].DigitalMin));
    if (chn&(1LL<<j)) {
      if (hdr) { nc+=fprintf(fp," %8s",edf->signal[j].Label); }
    }
  }
  if (hdr) { nc+=fprintf(fp,"\n"); }

  /* find the first channel to be output */
  FirstChannel = 0;
  while ( ( chn & (1LL<<FirstChannel) ) == 0 ) { FirstChannel++; }

  /* print all samples */
  for (i=0; i<edf->signal[FirstChannel].NrOfSamples; i++) {
    nc+=fprintf(fp," %8d",i);
    for (j=0; j<edf->NrOfSignals; j++) {
      if (chn&(1LL<<j)) {
        /* get integer value and overflow bit of sample 'i' in channel 'j' */
        yi = edf_get_integer_overflow_value(edf, j, i, &ovf);
        if (val&(1LL<<j)) {
          nc+=fprintf(fp," %8.2f",gain[j]*yi);
        } else
        if (hex&(1LL<<j)) {
          nc+=fprintf(fp," %08X",yi);
        } else {
          nc+=fprintf(fp," %8d",yi);
        }
      }
    }
    nc+=fprintf(fp,"\n");
  }

  return(nc);
}

/** Print EDF/BDF interger samples of selected channels 'chn'
*/
int32_t edf_prt_samples(FILE *fp, edf_t *edf, uint64_t chn) {
 
  return(edf_prt_hex_samples(fp, edf, chn, 0LL, 0LL, 1));
} 
  
/** Find channel number of EDF channel with signal name 'name'
 * @return channel number 0...n-1 or -1 on failure.
*/
int32_t edf_fnd_chn_nr(edf_t *edf, const char *name) {

  int32_t j;          /**< signal counter */
  int32_t nr=-1;
   
  for (j=0; j<edf->NrOfSignals; j++) {
    if (vb&0x04) { fprintf(stderr,"# %d %s\n",j,edf->signal[j].Label); }
    if (strcmp(edf->signal[j].Label,name)==0) { 
      nr=j; break; 
    }
  }
  return(nr);
}

/* swap channel numbers 'i' and 'j' in 'edf'
 * @return always zero
*/
int32_t edf_swap_chn(edf_t *edf, int32_t i, int32_t j) {

  edf_signal_t tmp;

  memcpy(&tmp           , &edf->signal[j], sizeof(edf_signal_t));
  memcpy(&edf->signal[j], &edf->signal[i], sizeof(edf_signal_t));
  memcpy(&edf->signal[i], &tmp           , sizeof(edf_signal_t));
  
  return(0);
} 

/** Add copy of EDF channel 'name' as last channel with 'newname' in 'edf'
 * @return new channel number: <0 on failure
*/
int32_t edf_cpy_chn(edf_t *edf, char *name, char *newname) {

  int32_t chn,flt;      /**< channel number of 'name' and new channel */
  
  /* get channel number of 'name' signal */
  if ((chn = edf_fnd_chn_nr(edf, name))<0) {
    fprintf(stderr,"# Error: missing '%s' channel\n", name); 
    return(-1);
  }

  /* check if it already exists */
  if ((flt = edf_fnd_chn_nr(edf, newname))>=0) {
    fprintf(stderr,"# Error: channel '%s' already exists\n", newname); 
    return(-1);
  }

  /* add filtered respiration signal as last channel */
  flt=edf->NrOfSignals;

  /* increase storage space for (n+1) signals */
  edf->signal=(edf_signal_t *)realloc(edf->signal, (edf->NrOfSignals+1)*sizeof(edf_signal_t));
  edf->NrOfHeaderBytes += 256;

  /* copy all channel properties */
  memcpy(&edf->signal[flt], &edf->signal[chn], sizeof(edf_signal_t));
  strncpy(edf->signal[flt].Label, newname, 16);

  /* allocate space for new signal values */
  edf->signal[flt].data = (int32_t *)calloc(edf->signal[flt].NrOfSamples, sizeof(int32_t));

  /* copy all samples as default data */
  memcpy(edf->signal[flt].data, edf->signal[chn].data, edf->signal[flt].NrOfSamples * sizeof(int32_t));

  /* increment number of signals */
  edf->NrOfSignals++;

  fprintf(stderr, "# Info: Found '%s' signal at channel nr %d new name %s nr %d\n", name, chn, newname, flt);

  /* return new channel number */
  return(flt);
}

/* Check range of all 'EEG' channels of a nexus recording
 * @return number of channels with more than 5% overflow problems
 * @note 'edf' struct appended with overflow bit per sample at bit position 30 = 1<<30
*/
int32_t edf_range_chk(edf_t *edf) {
  
  int32_t  i,j;       /**< channel numbers */
  float    gain;      /**< channel gain */
  int32_t  yi;        /**< integer representation of sample 'y' */
  float    y;         /**< sample */
  float    ymin;      /**< minimum range per channel [uV] */
  float    ymax;      /**< maximum range per channel [uV] */
  int32_t  cnt;       /**< overflow problem counter per channel */
  float    err;
  int32_t  ccnt=0;    /**< count of channels with problems */
    
  for (j=0; j<edf->NrOfSignals; j++) {
    if (strstr("uV", edf->signal[j].PhysicalDimension) != NULL) {
      //fprintf(stderr,"# Checking channel %s\n",edf->signal[j].Label);
      /* range for channel 'j' */
      ymin = (float) (0.995 * edf->signal[j].PhysicalMin);
      ymax = (float) (0.995 * edf->signal[j].PhysicalMax);
       /* gain for channel 'j' */
      gain = (float) ((edf->signal[j].PhysicalMax - edf->signal[j].PhysicalMin) /
                      (edf->signal[j].DigitalMax  - edf->signal[j].DigitalMin));
      cnt=0;
      for (i=0; i < edf->signal[j].NrOfSamples; i++) {
        /* get integer sample 'i' of channel 'j' */
        yi = edf_get_integer_value(edf, j, i);
        /* convert to float value */
        y = gain * yi;
        if ((y < ymin) || (y > ymax)) { 
          edf->signal[j].data[i] |= OVERFLOWBIT;
          cnt++;
        }
      }
      err = (float) ((100.0*cnt)/edf->signal[j].NrOfSamples);

      if (err > 5.0) { 
        fprintf(stderr,"# Warning: channel %s has %.1f %% overflow problems\n", edf->signal[j].Label, err);
        ccnt++;
      }
    }
  }
  return(ccnt);
}
  
/* Count all samples with overflow problems
 * @return number total number of samples in overflow
*/
int32_t edf_range_cnt(edf_t *edf) {
  
  int32_t  i,j;       /**< channel numbers */
  int32_t  cnt;       /**< overflow problem counter per channel */
  float    err;       /**< percentage sample in overflow */
  int32_t  ccnt=0;    /**< count of channels with problems */
 
  fprintf(stderr,"# %9s %9s %9s\n","Label","Overflow","%");   
  for (j=0; j<edf->NrOfSignals; j++) {
    if (strstr("uV", edf->signal[j].PhysicalDimension) != NULL) {
      cnt=0;
      for (i=0; i < edf->signal[j].NrOfSamples; i++) {
        /* check overflow bit of sample 'i' in channel 'j' */
        if (edf_get_overflow_value(edf, j, i) == OVERFLOWBIT) { cnt ++; } 
      }
      if (cnt>0) {
        err = (float) ((100.0*cnt)/edf->signal[j].NrOfSamples);
        fprintf(stderr,"# %9s %9d %9.1f\n", edf->signal[j].Label, cnt, err);
        ccnt += cnt;
      }
    }
  }
  return(ccnt);
}


/** ##### stimuli pulse functions ###### */

/** Compare two integers (passed by reference); used by qsort() below
 * @return 0 if a and b are equal, -1 if a is smaller, 1 if a is larger than b
 */
static inline int32_t comp_func(const void* ap, const void* bp) {

  int32_t a = *((int32_t *) ap);
  int32_t b = *((int32_t *) bp);

  if (a<b) { return(-1); } else
  if (a>b) { return( 1); } else { return(0); }
}

/** Find corresponding amplitude 'a' for 'n' quantile points 'q' 
 *   of signal 'k' in 'edf' data.
 * @return number of quantiles found 
*/
int32_t edf_quantile(edf_t *edf, int32_t k, float *q, int32_t n, int32_t *a) {

  int32_t i,j;  /**< sample counters */
  int32_t ns;   /**< number of samples */
  int32_t *sv;  /**< sample values */
  int32_t fnd=0; /**< number of found quantiles */
  
  /* signal number range check */
  if ((k<0) || (k>=edf->NrOfSignals)) {
    return(fnd);
  }
  ns=edf->signal[k].NrOfSamples;

  /* allocate space for 'ns' samples */
  sv=(int32_t *)calloc(ns, sizeof(int32_t));

  /* copy data */
  memcpy(sv, edf->signal[k].data, sizeof(int32_t)*ns);

  /* sort data */
  qsort( sv, ns, sizeof(sv[0]), comp_func);

  /* make integer quantiles */
  for (i=0; i<n; i++) { 
    j=(int32_t)round(q[i]*ns); 
    if (j<0) {
      a[i]=sv[0];
    } else
    if (j>=ns) {
      a[i]=sv[ns-1];
    } else {
      a[i]=sv[j]; fnd++;
    }
  }
  if (vb&0x04) {
    fprintf(stderr,"#%6s %8s\n","i",edf->signal[k].Label);
    for (i=0; i<ns; i++) {
      fprintf(stderr," %6d %8d\n",i,sv[i]);
    }
  }

  /* free temp sample storage space */
  free(sv);
 
  return(fnd);
}

/** Find corresponding threshold for "binair" signal 'k' in 'edf' data
 *   by dividing whole recording period in 'nb' blocks
 * @return number of epochs analyzed 
*/
int32_t edf_fnd_thr(edf_t *edf, int32_t k, int32_t nb) {

  int32_t ns;         /**< number of samples per block */
  float   gain;       /**< gain of channel 'k' */
  int32_t p2p_ok;     /**< ok threshold for peak to peak value */
  int32_t mini;       /**< minimum per block */
  int32_t maxi;       /**< maximum per block */
  int32_t p2p;        /**< peak to peak per block */
  int32_t cnt;        /**< usefull block counter */
  int32_t mean;       /**< mean per block */
  int32_t i,i0;       /**< sample index */
  int32_t j;          /**< block index */
  int32_t sample;     /**< integer data sample */
  int32_t thr=0;      /**< threshold value */
  int32_t diff,pdiff; /**< intersample difference */
  int32_t overload=0; /**< overload counter */
  
  /* gain for channel 'k' */
  gain=(float)(edf->signal[k].PhysicalMax - edf->signal[k].PhysicalMin) /
              (edf->signal[k].DigitalMax  - edf->signal[k].DigitalMin);
              
  p2p_ok=(int32_t)round(MINDIFF_ON_OFF/gain);

  ns=edf->signal[k].NrOfSamples/nb;
  if (vb&0x04) {
    fprintf(stderr," %3s %3s %9s %9s %9s %9s %9s %9s\n",
      "j","cnt","mini","maxi","mean","p2p","c_mean","c_p2p");
  }

  i=0; j=0; cnt=0; mean=0; p2p=0;
  while (i<edf->signal[k].NrOfSamples) {
    /* get integer value and overflow bit of sample 'i' in channel 'k' */
    mini = edf_get_integer_value(edf, k, i);
    maxi = mini;
    while ((i<(j+1)*ns) && (i<edf->signal[k].NrOfSamples)) {
      sample = edf_get_integer_value(edf, k, i);
      if (sample < mini) { mini=sample; } else
      if (sample > maxi) { maxi=sample; }
      i++;
    }
    if (((maxi-mini)>p2p_ok) && ((maxi-mini)<10*p2p_ok)) {
      /* this block has light on and off states */
      mean+=(maxi+mini)/2;
      p2p +=(maxi-mini);
      cnt++;
    }
    if (vb&0x04) {
      fprintf(stderr," %3d %3d %9d %9d %9d %9d %9d %9d\n",
        j,cnt,mini,maxi,(maxi+mini)/2,(maxi-mini),
        (cnt>0 ? mean/cnt : 0),(cnt>0 ? p2p/cnt : 0));
    }
    j++;
  }
  
  if (cnt>0) {
    thr=mean/cnt; p2p=p2p/cnt;
    if (vb&0x04) {
      fprintf(stderr,"# p2p_ok %d ns %d cnt %d threshold %d p2p %d\n",p2p_ok,ns,cnt,thr,p2p);
    }
  } else {
    fprintf(stderr,"# Error: missing on/off light states in stimuli signal!!!\n");
  }

  fprintf(stderr,"# chn %d threshold %9.3f [%s] p2p %9.2f [%s]\n",
           k,(thr*gain),edf->signal[k].PhysicalDimension,
             (p2p*gain),edf->signal[k].PhysicalDimension);
  
  /* number of samples around rising edge */
  ns=(int32_t)round(edf->signal[k].NrOfSamplesPerRecord * EDGE_DURATION / edf->RecordDuration);

  /* check rising edge of stimuli (display) signal on overflow */
  i=1; j=0; 
  while (i<edf->signal[k].NrOfSamples) {
    if ((edf_get_integer_value(edf, k, i-1) < thr) && (edf_get_integer_value(edf, k, i) >= thr)) {
      /* found rising edge */
      diff=0;
      for (i0=-ns; i0<ns; i0++) {
        pdiff=diff; 
        diff = edf_get_integer_value(edf, k, i+i0) - edf_get_integer_value(edf, k, i+i0-1);
        if ((diff==32767) && (pdiff==32767)) { overload++; }
        if (vb&0x08) {
          fprintf(stderr," %1d %2d %4d %9d %9d\n",
            k, j, i0, edf_get_integer_value(edf, k, i+i0), diff);
        }
      }
      j++;
    }
    i++;
  }
  
  if (overload>0) {
    fprintf(stderr,"# Error: overload occurred %d in channel %d\n",overload,k);
  }
  return(thr);
}

/** Check battery level in channel 'chn'
 * @return time [s] when first time battery low occurred (0.0 == never) 
*/
double edf_chk_battery(edf_t *edf, int32_t chn) {

  int32_t  i;         /**< sample index */
  int32_t  idx=0;     /**< first index of battery low */
  double   fs;        /**< sampling frequency */
  
  fs=edf->signal[chn].NrOfSamplesPerRecord / edf->RecordDuration;

  for (i=0; i<edf->signal[chn].NrOfSamples; i++) {
    if (edf->signal[chn].data[i]&0x02) {
      /* found battery low */
      if (idx==0) { idx=i; }
    }
    /* clear all bits except switch bit */
    edf->signal[chn].data[i]&=0x01;
  }
  return(idx/fs);
}

/** Print 't' [s] as hh:mm:ss.ms to file 'fp'
 * @return number of printed characters
*/
int32_t edf_t2hhmmss(FILE *fp, double t) {

  int32_t ts,hh,mm,ss,ms;
 
  ts=(int32_t)floor(t);
  ms=(int32_t)round(1000*(t-ts));
  ss=ts%60; ts/=60;
  mm=ts%60; ts/=60;
  hh=ts;
  return(fprintf(fp," %02d:%02d:%02d.%03d",hh,mm,ss,ms));
}
  
/** Check if duration 'a' fall in range [dur-tol, dur+tol]
 * @return 1 on in range else 0
*/
int32_t edf_match_dur(double a, double dur, double tol) {

  if ((a>=(dur-tol)) && (a<=(dur+tol))) { return(1); } else { return(0); }
}

/** Print serie of pulses 'psr' to file 'fp'
 * @return number of printed characters
*/
int32_t edf_prt_serie(FILE *fp, serie_t *psr, int32_t stp) {

  int32_t i;
  int32_t nc=0; 
  int32_t cnt=0;
  
  /* print header */
  nc+=fprintf(fp," %3s %7s %7s","nr","tr","dur");
  if (stp) { nc+=fprintf(fp," %7s %3s","dly","cnt"); }
  nc+=fprintf(fp,"\n"); 
  /* print data */
  for (i=0; i<psr->np; i++) {
    nc+=fprintf(fp," %3d %7.4f %7.4f",i,psr->ps[i].tr,psr->ps[i].dur);
    if (psr->ps[i].dly>0.0) { cnt++; }
    if (stp) { nc+=fprintf(fp," %7.4f %3d",psr->ps[i].dly,cnt); }
    nc+=fprintf(fp,"\n"); 
  }
  return(nc);
}

/** Codeword to pp, sn and tasknumber mapping
 *  scl        pp        sn        tn
 *   24  0x0FFF00  0x0000F0  0x00000F
 *   28  0xFFF000  0x000F00  0x0000FF
*/

/** Convert codeword to 'pp'.
 *  @return >0 on success
 */
int32_t cw2pp(int32_t cw, int32_t scl) {

  int32_t rv = -1;

  switch (scl) {
    case 24: rv = (cw>> 8)&0xFFF; break;
    case 28: rv = (cw>>12)&0xFFF; break;
    default: rv = -1; break;
  } 
  return(rv);
}

/** Convert codeword to 'sn'.
 *  @return >0 on success
 */
int32_t cw2sn(int32_t cw, int32_t scl) {
  
  int32_t rv = -1;

  switch (scl) {
    case 24: rv = (cw>> 4)&0xF; break;
    case 28: rv = (cw>> 8)&0xF; break;
    default: rv = -1; break;
  } 
  return(rv);
}

/** Convert codeword to 'tn'.
 *  @return >0 on success
 */
int32_t cw2tn(int32_t cw, int32_t scl) {
  
  int32_t rv = -1;

  switch (scl) {
    case  8: rv = (cw)&0x0F; break;
    case 24: rv = (cw)&0x0F; break;
    case 28: rv = (cw)&0xFF; break;
    default: rv = -1; break;
  } 
  return(rv);
}

/** Fetch start code of 'n' bits from serie 'psr' starting at index 'idx'
 * @return startcode on success, <0 on failure
*/
int32_t edf_fetch_start_code(int32_t *idx, serie_t *psr, int32_t n) {

  int32_t i,j,k,bl; /**< serie index, bit counter, bit length */
  //int32_t bt[2*n];
  int32_t *bt = alloca(2*n*sizeof(int32_t));
  int32_t cw,ok;
  double  thigh,tlow;
  
  i=*idx; 
  if (vb&0x20) {
    fprintf(stderr,"# Found start code word begin at idx %d time ",i);
    edf_t2hhmmss(stderr,psr->ps[i].tr); fprintf(stderr,"\n");
  }
  /* quantize bits lentghs of HALFBITDURATION's */
  if (vb&0x20) { 
    fprintf(stderr,"#%5s %6s %2s %6s %2s\n","idx","dur","bl","tlow","bl");
  }
  j=0;
  while ((j < 2*n) && (i < psr->np)) {
     thigh = psr->ps[i].dur;
     if ((i+1) < psr->np) {
       tlow=psr->ps[i+1].tr - (psr->ps[i].tr + psr->ps[i].dur);
     } else {
       tlow = 2.0*HALFBITDURATION;
     }
     /* check for time quantization problems */
     if (thigh < 0.5*HALFBITDURATION) {
       /* thigh too short, lengthing to HALFBITDURATION */
       fprintf(stderr,"# Info: fixing small tlow %6.3f thigh %6.3f problem at %6.3f [s]\n",
                 thigh,tlow,psr->ps[i].tr);
       tlow = thigh + tlow-HALFBITDURATION; thigh = HALFBITDURATION;
     }
     if (tlow < 0.5*HALFBITDURATION) {
       /* tlow too short, lengthing to HALFBITDURATION */
       fprintf(stderr,"# Info: fixing small thigh %6.3f tlow %6.3f problem at %6.3f [s]\n",
                 tlow,thigh,psr->ps[i].tr);
       thigh = thigh + tlow - HALFBITDURATION; tlow=HALFBITDURATION;
     }
     /* high pulse (1) duration */
     bl=(int32_t)round(thigh/HALFBITDURATION);
     if (vb&0x20) { fprintf(stderr," %5d %6.3f %2d",i,thigh,bl); }
     for (k=0; k<bl && j<2*n; k++) { bt[j]=1; j++; }
     /* low pulse (0) to next pulse */
     bl=(int32_t)round(tlow/HALFBITDURATION);
     if (vb&0x20) { fprintf(stderr," %6.3f %2d\n",tlow,bl); }
     for (k=0; k<bl && j<2*n; k++) { bt[j]=0; j++; }
     /* next pulse */
     i++;
  }

  /* return current index-1 to avoid skipping to next startcode */
  *idx=i-1;
  
  /* all bits received */
  if (j<2*n) {
    return(-1);
  }
  
  if (vb&0x20) {
    fprintf(stderr,"# %2s %2s %2s %2s %6s\n","j","b0","b1","ok","cw");
  }
  /* check bi-phase and value */
  cw=0; ok=1;
  for (j=0; j<2*n; j+=2) {
    /* bit value */
    cw=cw<<1 | bt[j];
    /* bi-phase check */
    if (bt[j]==bt[j+1]) { ok=0; }
    if (vb&0x20) {
      fprintf(stderr,"# %2d %2d %2d %2d 0x%06X\n",j,bt[j],bt[j+1],ok,cw);
    }
  }
  if (!ok) { 
    fprintf(stderr,"# codeword decoding phase problems: second try half bit shift\n"); 
    cw=0; ok=1;
    for (j=1; j<2*n-1; j+=2) {
      /* bit value */
      cw=cw<<1 | bt[j];
      /* bi-phase check */
      if (bt[j]==bt[j+1]) { ok=0; }
      if (vb&0x20) {
        fprintf(stderr,"# %2d %2d %2d %2d 0x%06X\n",j,bt[j],bt[j+1],ok,cw);
      }
    }
  }
  if ((ok) & (vb&0x20)) { 
    fprintf(stderr, "# found start code 0x%06X at ",cw);
    edf_t2hhmmss(stderr,psr->ps[*idx].tr); fprintf(stderr," [s]\n");
  }
   
  if (ok) { return(cw); } else { return(-cw); }
}

/* find index of start code 'fcw' in task definition array 'task_def',
 * @return index 0..NTASK-1 or -1 on failure
*/
int32_t edf_fnd_start_codes(int32_t fcw) {

  int32_t j;
  
  /* all negative startcode are not used */
  if (fcw<=0) { return(-1); }
  
  /* find right task definition */
  j=0; while ((j<NTASK) && ((fcw&0xFE)!=task_def[j].scw)) { j++; }

  /* found failure returns -1 */
  if (j>=NTASK) { j=-1; }
  
  return(j);
}

/** Look for all bi-phase modulated startcodes starting at index 'idx'
 *  Insert first 'ncw' startcodes in 'cw'
 * @return number of found startcodes
*/
int32_t edf_fnd_startcodes(int32_t idx, serie_t *psr, int32_t n, codeword_t *cwa, int32_t ncw) {

  int32_t i,i0,j; /**< general index */
  double  tlow;       /**< low signal duration */
  int32_t bc;         /**< bit counter */
  int32_t cw;         /**< current codeword */
    
  /* loop over all pulses */
  i=idx; j=0;
  while (i<(psr->np-1)) {
    /* look for 2 bi-phase ones */
    bc=0; 
    while ((i<(psr->np-1)) && (bc<2)) {
      if (edf_match_dur(psr->ps[i].dur,HALFBITDURATION,1.5*TOLERANCE)) {
        tlow=psr->ps[i+1].tr - (psr->ps[i].tr+psr->ps[i].dur);
        if (edf_match_dur(tlow,HALFBITDURATION,1.5*TOLERANCE)) {
          bc++;
        } else {
          /* restart again */
          bc=0; 
        }
      } else {
        /* restart again */
        bc=0; 
      }
      i++;
    }
    /* start moment */
    i0 = i-bc; i = i0;
    cw=edf_fetch_start_code(&i, psr, n);
    if (cw > 0) {
      if (vb&0x80) {
        fprintf(stderr,"# found 0x%06X ts %.3f te %.3f at", cw, psr->ps[i0].tr, psr->ps[i].tr); 
        edf_t2hhmmss(stderr,psr->ps[i0].tr); fprintf(stderr,"\n");
      }
      if (j < ncw) {
        cwa[j].cw = cw;
        cwa[j].is = i0; cwa[j].ts = psr->ps[i0].tr;
        cwa[j].ie = i;  cwa[j].te = psr->ps[ i].tr;
      }
      j++;
    }
  }
  fprintf(stderr,"# Info: found %d startcode or endcodes\n",j);
  if (j > ncw) {
    fprintf(stderr,"# Error: missing storage space for %d startcodes\n",j-ncw);
    j = ncw;
  }
  return(j);  
}

/** Find task index in task definition array 'tasks' with 'nt' tasks
 *   which start with task number 'tns' and ends with 'tne'
 * @return task index, <0 if no match found
*/
int32_t edf_fnd_task_idx(int32_t tns, int32_t tne, task_def_t *tasks, int32_t nt) {

  int32_t i,j; /**< task index */
  
  i=0; j=-1;
  while ((i<nt) && (j<0)) {
    if ((tns==tasks[i].scw) && (tne==tasks[i].ecw)) { j=i; }
    i++;
  }
  return(j);
}

/** Put all tasks in codeword array 'cwa' with 'ncw' codewords according
 *   task definition array 'tasks' with 'nt' tasks into 'task' with maximum
 *   number of storage places
 * @return number of tasks found
*/
int32_t edf_fnd_def_task(codeword_t *cwa, int32_t ncw, task_def_t *tasks, int32_t nt,
 task_t *task, int32_t mtsk, int32_t scl) {
 
  int32_t i,j,k; /**< general index */
 
  /* find all tasks in startcode array */
  i=0; j=0;
  while (i<(ncw-1)) {
    /* look for matching task with index 'k' */
    if ((k = edf_fnd_task_idx(cw2tn(cwa[i].cw,scl), cw2tn(cwa[i+1].cw, scl), tasks, nt))>=0) {
      if (j < mtsk) {
        task[j].cw  = cwa[i  ].cw;
        /* start task */
        task[j].is  = cwa[i  ].is; task[j].ise = cwa[i  ].ie; task[j].ts = cwa[i  ].ts;  
        /* start end */
        task[j].ies = cwa[i+1].is; task[j].ie  = cwa[i+1].ie; task[j].te = cwa[i+1].te;
        /* task admin */
        task[j].cs = 1; task[j].ce = 1; task[j].tn = k;
      }  
      i+=2; j++;
    } else {
      fprintf(stderr,"# Warning: Can't match codeword %d 0x%06X at ",i,cwa[i].cw);
      edf_t2hhmmss(stderr,cwa[i].ts); 
      fprintf(stderr," with task nr 0x%02X to task index\n",cw2tn(cwa[i].cw,scl));
      i++;
    }
  }
  
  if (j > mtsk) {
    fprintf(stderr,"# Error: Find %d tasks and only %d storage places in edf_fnd_def_task\n", j, mtsk);
    j = mtsk;
  }
  return(j);
}

/** Look for all bi-phase modulated startcodes starting at index 'idx'
 *  Insert all start codes in 'task' maximum 'nt'
 * @return number of completed tasks
*/
int32_t edf_get_start_codes(int32_t idx, serie_t *psr, int32_t n, task_t *task, int32_t nt) {

  int32_t i,i0,j,k,m; /**< general index */
  double  tlow;       /**< low signal duration */
  int32_t bc;         /**< bit counter */
  int32_t cw;         /**< current codeword */
  int32_t cnt=0;      /**< number of complete tasks */
  char    ch;         /**< first character of task name */
  double  t[4*NTASK]; /**< found timestamp of 'fcw' */
  int32_t seq[2*NTASK];
  int32_t mcnt=0;     /**< max count for sequence match */
  int32_t idx0[4*NTASK];
  int32_t idx1[4*NTASK];
  int32_t match;
  int32_t *fcw = alloca( sizeof(int32_t)*nt ); /**< found codewords */
  
  /* make task codeword sequence */
  for (j=0; j<2*NTASK; j++) {
    seq[j]=task_def[j/2].scw | (j%2);
  }
  
  /* clean task memory */
  memset(task,0,nt*sizeof(task_t));
  
  /* loop over all pulses */
  i=idx; j=0; k=0;
  while (i<(psr->np-1)) {
    /* look for 2 bi-phase ones */
    bc=0; 
    while ((i<(psr->np-1)) && (bc<2)) {
      if (edf_match_dur(psr->ps[i].dur,HALFBITDURATION,1.5*TOLERANCE)) {
        tlow=psr->ps[i+1].tr - (psr->ps[i].tr+psr->ps[i].dur);
        if (edf_match_dur(tlow,HALFBITDURATION,1.5*TOLERANCE)) {
          bc++;
        } else {
          /* restart again */
          bc=0; 
        }
      } else {
        /* restart again */
        bc=0; 
      }
      i++;
    }
    /* start moment */
    i0=i-bc; i=i0;
    cw=edf_fetch_start_code(&i, psr, n);
    if ((cw>0) && (j<nt)) {
      if (vb&0x80) {
        fprintf(stderr,"# found 0x%02X i0 %d i %d at",cw,i0,i); 
        edf_t2hhmmss(stderr,psr->ps[i0].tr); fprintf(stderr,"\n");
      }
      if (j==0) { 
        k=2*edf_fnd_start_codes(cw); 
      } else {
        if ((cw!=seq[k]) && (cw==seq[(k+1)%(2*NTASK)])) {
          /* detect missing codeword */
          fcw[j]=seq[k]; t[j]=-1;
          idx0[j]=idx0[j-1]; idx1[j]=idx1[j-1];
          fprintf(stderr,"# Warning: inserted missing codeword 0x%02X at %f [s]\n",
            seq[k],psr->ps[i0].tr);
          j++; k++;
        }
      }
      
      fcw[j]=cw; 
      if (cw&0x01) { t[j]=psr->ps[i0].tr; } else { t[j]=psr->ps[i].tr; }
      idx0[j]=i0; idx1[j]=i;
      
      j++; k=(k+1)%(2*NTASK);
    }
  }

  /* estimate missing start/end time of task */
  for (i=0; i<j; i++) {
    if (t[i]<0.0) {
      if (fcw[i]&0x01) {
        if (i>0) {
          /* missing start time */
          m=edf_fnd_start_codes(fcw[i-1]);
          t[i]=t[i-1]+task_def[m].dur;
        }
      } else {
        if ((i+1)<j) {
          m=edf_fnd_start_codes(fcw[i]); 
          t[i]=t[i+1]-task_def[m].dur;
        }
      }
    }
    if (t[i]<0.0) {
      fprintf(stderr,"# Error: unknown start time at seq %d\n",i);
    }
  }
  
  /* search for optimal start point */
  mcnt=0; m=0;
  for (k=0; k<NTASK; k++) {  
    cnt=0;
    for (i=0; i<j; i++) {
      if (fcw[i]==seq[i+k]) { cnt++; }
    }
    if (vb&0x80) {
      fprintf(stderr,"# k %d cnt %d\n",k,cnt);
    }
    if (cnt>mcnt) {
      mcnt=cnt; m=k;
    }
  }
  if (vb&0x80) {
    fprintf(stderr,"# >>> k %d mcnt %d\n",m,mcnt);
  }
  
  /* check and correct codeword sequence */
  for (i=0; i<j; i++) {
    k=(i+m)%(2*NTASK);
    if (fcw[i]!=seq[k]) { 
      if (fcw[i]&0x01) {
        match=((fcw[i-1]&0xFE) == (fcw[i  ]&0xFE));
      } else {
        match=((fcw[i  ]&0xFE) == (fcw[i+1]&0xFE));
      } 
      if (!match) {
        /* only correct when start or end codeword don't match */
        fprintf(stderr,"# correct task seq %d 0x%02X in %d 0x%02X\n",i,fcw[i],k,seq[k]);
        fcw[i]=seq[k];
      } 
    }
  }
    
  k=0;
  if (fcw[0]&0x01) { i0=1; } else { i0=0; }
  for (i=i0; i<j; i++) {
    if (fcw[i]&0x01) { 
      /* end task */ 
      task[k].ie=idx0[i]; task[k].te=t[i]; task[k].ce++;
      k++;
    } else {
      task[k].cw=fcw[i];
      /* start task */ 
      task[k].is=idx1[i]; task[k].ts=t[i]; task[k].cs++;
    }
  }
  
  /* copy from definitions */
  for (j=0; j<k; j++) {
    i=edf_fnd_start_codes(task[j].cw);
    if (i>=0) {
      strcpy(task[j].name,task_def[i].name);
      task[j].dur=task_def[i].dur;
      task[j].cnt=task_def[i].nst;
    }
  }
  
  /* check all tasks */
  cnt=0;
  for (j=0; j<k; j++) {
    i=edf_fnd_start_codes(task[j].cw);
    if (i>=0) {
      /* quick fix of missing codes */
      if ((task[j].cs>0) && (task[j].ce==0)) {
        /* missing end time */
        task[j].te=task[j].ts+task[j].dur;
        fprintf(stderr,"# Warning: missing end time of task %s\n",task[j].name);
      } else
      if ((task[j].cs==0) && (task[j].ce>0)) {
        /* missing start time */
        task[j].ts=task[j].te-task[j].dur;
        fprintf(stderr,"# Warning: missing start time of task %s\n",task[j].name);
      }
      
      if (vb&0x80) {
        fprintf(stderr," %2d 0x%02X %4s %9.3f %2d %9.3f %2d\n",
           j,task[j].cw,task[j].name,task[j].ts,task[j].cs,task[j].te,task[j].ce);
      }
      if ((task[j].cs>0) && (task[j].ce>0)) {
        cnt++;
      }

      if ((task[j].cs>1) || (task[j].ce>1)) {
        fprintf(stderr,"# Warning: found task %s cs %d ce %d more than once!!!\n",
          task[j].name,task[j].cs,task[j].ce);
      }
    }
  }
  
  /* check nfb task numbering */
  for (i=0; i<cnt; i++) { 
    if (task[i].name[2]=='B') {
      /* found nfb task */
      if (i>0) { 
        switch (task[i-1].name[1]) {
          case 'l': ch='1'; break;
          case 't': if (task[i-1].name[2]=='o') { ch='2'; } else { ch='3'; } break;
          default: ch='1';
        }
        task[i].name[3]=ch;
      } else {
        if (i<cnt-1) {
          switch (task[i+1].name[1]) {
            case 'b': ch='3'; break;
            case 't': if (task[i+1].name[2]=='o') { ch='1'; } else { ch='2'; } break;
            default: ch='1';
          }
          task[i].name[3]=ch;
        }
      }
    }
  } 
  
  /* check if previous NFB task was too short */
  for (i=1; i<cnt; i++) { 
    if ((task[i-1].name[2]=='B') && (task[i].name[2]=='B')) { 
      fprintf(stderr,"# Correction: found task %d %s renamed to %s\n",i,task[i].name,task[i-1].name);
      if ((task[i-1].te-task[i-1].ts) < 0.90*task[i-1].dur) {
        task[i].name[3]=task[i-1].name[3];
      }
    }
  } 
   
  return(cnt);
}

#define TSKCHUNKSIZE  (16)

/** Read task definition file 'fname'
* @return pointer to task array and number of tasks in 'nt'
*/
task_def_t *edf_rd_tasks(char *fname, int32_t *nt) {

  FILE   *fp;
  char    line[1024];     /**< temp line buffer */
  int32_t lc=0;           /**< line counter */
  int32_t ni;             /**< number of arguments read */
  int32_t sec=0;          /**< sample error count */
  int32_t i=0;            /**< annotation count */
  task_def_t *p=NULL;     /**< annotation array pointer */
  int32_t size=TSKCHUNKSIZE; /**< allocation chuck size counter */
  int32_t nr;             /**< task index */
  
  fprintf(stderr,"# Open task definition file %s\n",fname);
  if ((fp=fopen(fname,"r"))==NULL) {
    perror(""); (*nt)=i; return(p);
  }
  
  /* allocate space for annotations */
  if ((p=(task_def_t *)calloc(size,sizeof(task_def_t)))==NULL) {
    fprintf(stderr,"# Error not enough memory to allocate %d ecg events!!!\n",
              size);
  }

  //#nr name  scw ecw    dur  nst
  // 0   EC  0x0 0x1  216.0    0
  // 1  EO1  0x2 0x3  216.0    0

  /* read whole ASCII EEG sample file */ 
  while (!feof(fp)) {
    /* clear line */
    line[0]='\0';
    /* read one line with hist data */
    if (fgets(line,sizeof(line)-1,fp)==NULL) { continue; }
    lc++;
    /* skip comment lines or too small lines */
    if ((line[0]=='#') || (strlen(line)<2)) {
      continue;
    } 
    ni=sscanf(line,"%d %s %x %x %lf %d", &nr, p[i].name, &p[i].scw, &p[i].ecw, &p[i].dur, &p[i].nst);
    if (ni<6) { 
      if (sec==0) {
        fprintf(stderr,"# Error: argument count %d !=6 on line %d. Skip it\n",ni,lc);
      }
      sec++; 
      continue;
    } else {
      i++;
    }
    
    /* check available storage space */
    if (i>=size) {
      /* increment chuck size counter */
      size+=TSKCHUNKSIZE;
      /* allocate one chuck extra */
      p=(task_def_t *)realloc(p, size*sizeof(task_def_t));
    }
  }
  fclose(fp);

  if (sec>1) { fprintf(stderr,"# Error: skipped %d task definitions!!!\n",sec); }

  fprintf(stderr,"# Info: found %d tasks definitions\n",i);
  (*nt)=i;

  return(p);
}

/** Print 'nt' tasks of 'task' to file 'fp' with 'ppw' digits volunteer nr
* found in file 'fname'.
* @ return number of printed characters
*/
int32_t edf_show_tasks(FILE *fp, task_t *task, int32_t nt, int32_t ppw, char *fname) {

  int32_t i;        /**< general index */
  int32_t nc=0;     /**< number of characters printed to file 'fp' */
 
  /* print all tasks in chronologica order and skip missed ones */
  nc+=fprintf(fp," %*s %2s %2s %2s %6s %12s %9s %12s %9s %8s %3s %5s\n",(ppw+2),
               "pp","sn","tn","tr","name","start","start.s","end","end.s","dur","cnt","fname");
  for (i=0; i<nt; i++) {

    nc+=fprintf(fp," dn%0*d %2d %2d %2d %6s", ppw, task[i].pn, task[i].sn, task[i].tn, 
     task[i].tr, task[i].name);
    edf_t2hhmmss(fp,task[i].ts); nc+=fprintf(fp," %9.3f",task[i].ts);
    edf_t2hhmmss(fp,task[i].te); nc+=fprintf(fp," %9.3f",task[i].te);
    nc+=fprintf(fp," %8.3f %3d %s\n",task[i].te-task[i].ts,task[i].cnt,fname);
  }
  return(nc);
}

/** Find index of time 't' in serie_t 'psr'
* @return found index else -1
*/
int32_t edf_fnd_time_serie(double t, serie_t *psr) {
 
  int32_t i=0;         /**< general index */
  
  if (vb&0x100) {
    fprintf(stderr,"# edf_fnd_time_serie t %10.3f t0 %10.3f np %4d tn %10.3f\n",
      t,psr->ps[0].tr,psr->np,psr->ps[psr->np].tr);
  }
  i=0;
  while ((i < psr->np) && (t > psr->ps[i].tr)) { 
    if (vb&0x100) {
      fprintf(stderr," # %4d %10.3f\n",i,psr->ps[i].tr);
    }
    i++; 
  }
  if (i==psr->np) { i=-1; }
  return(i);
}


/** Find stop delay time in pulse serie 'psr' from index 'i0' up to 'i1'
 *   with 'tgo' as stimulus duration of go signal 
* @return number of found stop signals
*/
int32_t edf_fnd_stop_delay(int32_t i0, int32_t i1, serie_t *psr, double tgo) {

  int32_t i=i0,j;         /**< general index */
  double  tlow=0.0;       /**< low signal duration */
  int32_t cnt =0;         /**< stop pulse counter */
  int32_t skip=0;         /**< skip counter */

  if (vb&0x40) {
    fprintf(stderr,"# edf_fnd_stop_delay i0 %d i1 %d\n",i0,i1);
    fprintf(stderr," %3s %10s %7s %7s %7s %7s %3s\n","nr","t","dur","tlow","stop","delay","cnt");
  }
  i=i0;
  while ((i<i1-skip) && (i<psr->np-1)) {
    tlow=psr->ps[i+1].tr - (psr->ps[i].tr+psr->ps[i].dur);
    if (vb&0x40) {
      fprintf(stderr," %3d %10.4f %7.4f %7.4f",i,psr->ps[i].tr,psr->ps[i].dur,tlow);
    }
    /* !! AD increased tolerance with factor 1.5 to better cope with bluetooth packet loss */
    if (edf_match_dur(tlow,STOP_DURATION,1.5*TOLERANCE)) {
      /* stop stimuli found */
      cnt++;
      if (vb&0x40) {
        fprintf(stderr," %7.4f %7.4f %3d",tlow,psr->ps[i].dur,cnt);
      }
      psr->ps[i].dly  = psr->ps[i].dur;
      psr->ps[i].dur += tlow + psr->ps[i+1].dur;
      /* kill pulse 'i+1' */
      psr->np--; 
      for (j=i+1; j< psr->np; j++) {
        psr->ps[j]=psr->ps[j+1];
      }
      skip++;
    } else
    if (edf_match_dur(psr->ps[i].dur, tgo-STOP_DURATION, TOLERANCE )) {
      /* stop stimulus found with missing of too small tail */
      psr->ps[i].dly  = psr->ps[i].dur;
      cnt++;
      if (vb&0x40) {
        fprintf(stderr," small tail %7.4f %7.4f %3d",tlow,psr->ps[i].dur,cnt);
      }
    }
    if (vb&0x40) { fprintf(stderr,"\n"); }
    i++;
  }
  return(cnt); 
}

/** Find all pulse channel 'chn' in EDF struct 'edf' with help of 
 *  threshold 'thr' and reject all pulse shorter than 'tol' [s].
 *  @note serie of pulses returned 'psr'
 *  @return number of pulses
*/
int32_t edf_fnd_pulse(edf_t *edf, int32_t chn, int32_t thr, double tol, serie_t *psr) {

  int32_t i,j;             /**< general index */
  int32_t size=CHUNKSIZE;  /**< pulse chunk size */
  double  fs;              /**< sampling frequency */
  double  tlow;            /**< low signal duration */
  
  fs=edf->signal[chn].NrOfSamplesPerRecord / edf->RecordDuration;

  if (psr->ps!=NULL) {
    /* remove old serie of pulses */
    free(psr->ps);
  }
  /* allocate space for 'CHUNKSIZE' pulses */
  psr->ps=(pulse_t *)calloc(size, sizeof(pulse_t));
  
  if (vb&0x10) {
    fprintf(stderr,"# edf_fnd_pulse chn %d thr %d tol %7.4f\n",chn,thr,tol);
    fprintf(stderr," %3s %10s %7s %7s\n","nr","t","dur","tlow");
  }

  /* check rising edge of stimuli (display) signal on overflow */
  j=0; tlow=0.0;
  for (i=1; i<edf->signal[chn].NrOfSamples; i++) {
    if ((edf_get_integer_value(edf, chn, i-1) < thr) && (edf_get_integer_value(edf, chn, i) >= thr)) {
      /* found rising edge */
      psr->ps[j].tr=i/fs;
    } else
    if ((edf_get_integer_value(edf, chn, i-1) >= thr) && (edf_get_integer_value(edf, chn, i) < thr)) {
      /* found falling edge */
      psr->ps[j].dur=(i/fs) - psr->ps[j].tr;

      /* duration check */
      if (psr->ps[j].dur < tol) { continue; }

      if (j>0) { 
        tlow=psr->ps[j].tr - (psr->ps[j-1].tr+psr->ps[j-1].dur);
      }
      if (vb&0x10) {
        fprintf(stderr," %3d %10.4f %7.4f %7.4f\n",j,psr->ps[j].tr,psr->ps[j].dur,tlow);
      }
      /* next pulse */
      j++;
      /* check size of result array */
      if (j==size) {
        /* increment size */
        size+=CHUNKSIZE; 
        psr->ps=(pulse_t *)realloc(psr->ps,size*sizeof(pulse_t));
      }
    }
  }
  psr->np=j;
  return(j);
}

/** Find start and end code of 'n' bits of all tasks with range check window 
 *   length of 'tp' [s] put all found tasks in 'task' and 'tc' at most
 * @return number of found tasks
*/
int32_t edf_fnd_tasks(edf_t *edf, int32_t n, double tp, task_t *task, int32_t tc) {

  int32_t stc = 5;     /**< number of stimuli channel */
  int32_t nb;          /**< number of blocks of 'tp' [s] */
  int32_t thr;         /**< threshold value for channel 'the_stc' */
  serie_t sts;         /**< serie of stimuli pulses */
  int32_t tcnt=0;      /**< task counter */

  /* find number of display channel */
  stc=edf_fnd_chn_nr(edf,"Disp");
  
  /* number of blocks of 'tp' [s] */
  nb=(int32_t)round((edf->signal[stc].NrOfSamples * edf->RecordDuration)/
                (edf->signal[stc].NrOfSamplesPerRecord * tp));
  /* use minimum of 3 blocks */
  if (nb<3) { nb=3; }
  
  /* find nice threshold for channel 'stc' */
  thr=edf_fnd_thr(edf,stc,nb);
  
  /* find all stimuli pulses */ 
  memset(&sts,0,sizeof(serie_t));
  if (edf_fnd_pulse(edf, stc, thr, TOLERANCE, &sts)<=0) {
    fprintf(stderr,"# Warning: no pulses found in channel %d\n",stc);
  }
  
  /* check all task codes */
  tcnt=edf_get_start_codes(0,&sts, n, task, tc);
  
  /* free pulse */
  free(sts.ps);

  return(tcnt);
} 

/** Try to remove the switching backlight in the display signal
 *  with a periode smaller than 'dt' [s].
 *  @return total number of fixed periods
*/
int32_t edf_remove_switching_backlight(edf_t *edf, double dt) {

  int32_t  chn;         /**< display channel */
  int32_t  i,j,i0,i1;   /**< sample index */
  int32_t  mxs;         /**< maximum number of samples between two succeeding maxima */
  int32_t  nr=0;        /**< iteration counter */
  int32_t  cnt,tcnt=0;  /**< count backlight off switch states */
  double   slope;       /**< slope */
  int32_t  nv;          /**< new value */
  int32_t  auc,suc;     /**< area under curve, samples under curve */
  
  if ((chn=edf_fnd_chn_nr(edf,"Disp"))<0) {
    return(-1);
  }
  fprintf(stderr,"# Info: Remove switching backlight in channel 'Disp' %d\n",chn);
  
  /* estimate the backlight switching duration in samples */
  mxs=(int32_t)round(dt*edf->signal[chn].NrOfSamplesPerRecord / edf->RecordDuration);

  fprintf(stderr,"# Info: %5s %9s %9s\n","cycle","removed","mean auc");
  nr=0;
  do {
    i0=0; i1=0; cnt=0; auc=0; suc=0;
    /* find all peaks */
    for (i=1; i<edf->signal[chn].NrOfSamples-1; i++) {
      if ((edf_get_integer_value(edf, chn, i-1) < edf_get_integer_value(edf, chn, i  )) && 
          (edf_get_integer_value(edf, chn, i  ) > edf_get_integer_value(edf, chn, i+1))) {
        /* peak found */
        i0=i1; i1=i;
        /* check distance */
        if ((i1-i0)<=mxs) {
          slope=(edf_get_integer_value(edf, chn, i1) - edf_get_integer_value(edf, chn, i0))/(i1-i0);
          /* possible backlight off switch state detected */
          for (j=i0+1; j<i1; j++) {
            nv = (int32_t)round(edf->signal[chn].data[i0]+(j-i0)*slope);
            auc += (nv - edf->signal[chn].data[j]); suc++;
            edf->signal[chn].data[j] = nv;
          }
          cnt++;
        }
      }
    }
    fprintf(stderr,"# Info: %5d %9d %9.3f\n",nr,cnt,((1.0*auc)/suc));
    tcnt+=cnt; nr++;
  } while (cnt>1);
  
  return(tcnt);
}

/** Add extra signal 'RejectedEpochs' for missing packets in 'edf'
 *   or delta overflow of more than 2 succeeding samples 
 * @return percentage of Rejected Epochs or delta overflow samples
*/
double edf_chk_miss(edf_t *edf) {
  
  int32_t  i,j,k;   /**< general index */
  double   fs;      /**< sampling frequency */
  int32_t  rl;      /**< run length */
  int32_t  dy;      /**< delta between 2 succeeding samples */
  int32_t  rep;     /**< channel number of rejected epochs */
  
  fprintf(stderr,"# Info: edf check for missing packets 1\n");
  
  /* check battery level of Nexus-10 */
  rep=edf_fnd_chn_nr(edf,"RejectedEpochs");
  if (rep<0) {
    rep=edf->NrOfSignals;
    /* add extra signal 'RejectedEpochs' */
    edf->signal=(edf_signal_t *)realloc(edf->signal,(edf->NrOfSignals+1)*sizeof(edf_signal_t));
    edf->NrOfHeaderBytes+=256;
    strcpy(edf->signal[rep].Label,"RejectedEpochs");
    strcpy(edf->signal[rep].PhysicalDimension,"-");
    strcpy(edf->signal[rep].TransducerType,"Virtual");
    edf->signal[rep].PhysicalMin=0.0;
    edf->signal[rep].PhysicalMax=1.0;
    edf->signal[rep].DigitalMin=edf->signal[0].DigitalMin;
    edf->signal[rep].DigitalMax=edf->signal[0].DigitalMax;
    /* look for channel with the most samples per record */
    i=0; k=edf->signal[0].NrOfSamplesPerRecord;
    for (j=1; j<edf->NrOfSignals; j++) {
      if (edf->signal[j].NrOfSamplesPerRecord > k) {
        k=edf->signal[j].NrOfSamplesPerRecord; i=j;
      }
    }
    /* allocate space for RejectedEpochs signal values */
    edf->signal[rep].NrOfSamplesPerRecord=edf->signal[i].NrOfSamplesPerRecord;
    edf->signal[rep].data=(int32_t *)calloc(edf->signal[i].NrOfSamples,sizeof(int32_t));
    edf->signal[rep].NrOfSamples=edf->signal[i].NrOfSamples;
    /* increment number of signals */
    edf->NrOfSignals++;
  } 
  
  /* reset all overflow samples to 0: 'no problem' */
  //for (i=0; i<edf->signal[rep].NrOfSamples; i++) { edf->signal[rep].data[i] = 0; }  
  
  /* check for missed packets in first 4 EEG signals */
  for (j=0; j<4; j++) {
    rl=0;
    for (i=1; i<edf->signal[j].NrOfSamples; i++) {
      if (edf->signal[j].data[i] == edf->signal[j].data[i-1]) {
        rl++;
      } else {
        if (rl>7) {
          for (k=0; k<rl; k++) {
            edf->signal[rep].data[i-k-1]++;
          }
        }
        rl=0;
      }
    }
  }

  /* check for missing data in first 4 channels */
  for (i=0; i<edf->signal[rep].NrOfSamples; i++) {
    if (edf->signal[rep].data[i]>0) {
      edf->signal[rep].data[i]=-1;
    } else {
      edf->signal[rep].data[i]=0;
    }
  }
   
  if (vb&0x200) {
    /* show missing packet info */
    fs=edf->signal[rep].NrOfSamplesPerRecord / edf->RecordDuration;
    rl=0; k=0; 
    for (i=0; i<edf->signal[rep].NrOfSamples; i++) {
      if (edf->signal[rep].data[i] == -1) {
        rl++;
      } else {
        if (rl>7) {
          if (k==0) {
            fprintf(stderr,"# missing packets info\n");
            fprintf(stderr,"# %3s %9s %9s\n","nr","t","dt");
          } 
          fprintf(stderr,"# %3d %9.3f %9.3f\n",k,(i-rl)/fs,rl/fs);
          k++;
        }
        rl=0;
      }
    }
  }

  /* Mark all delta overflows in first 4 EEG signals */
  for (j=0; j<4; j++) {
    rl=0;
    for (i=1; i<edf->signal[j].NrOfSamples; i++) {
      dy=abs(edf->signal[j].data[i] - edf->signal[j].data[i-1]);
      if (dy==((1<<15)-1)) {
        rl++;
      } else {
        if (rl>1) {
          for (k=0; k<rl; k++) {
//            edf->signal[rep].data[i-k-1]=-2;
          }
        }
        rl=0;
      }
    }
  }
  
  /* remove first 16 samples of recording to kill connection problems */
  for (i=0; i<16 && i<edf->signal[rep].NrOfSamples; i++) {
    edf->signal[rep].data[i]=-1;
  }
  
  /* remove last 16 samples of recording to kill disconnection problems */
  for (i=edf->signal[rep].NrOfSamples-16; i<edf->signal[rep].NrOfSamples; i++) {
    edf->signal[rep].data[i]=-1;
  }

  k=0;
  for (i=0; i<edf->signal[rep].NrOfSamples; i++) {
    if (edf->signal[rep].data[i]<0) { k++; }
  }
    
  /* return number of missed packets */
  return((100.0*k)/edf->signal[rep].NrOfSamples);
}

/** Mark all samples from 't0' to 't1' [s] as useless
 * @return number of marked samples
*/
int32_t edf_mark(edf_t *edf, int32_t mrk, double t0, double t1) {

  int32_t i,i0,i1;
  int32_t err_chn;
  double  fs;
  
  err_chn=edf_fnd_chn_nr(edf,"RejectedEpochs");
  if (err_chn<0) {
    fprintf(stderr,"# Error: missing Rejected Epochs channel\n");
    return(0);
  }
  fs=edf->signal[0].NrOfSamplesPerRecord / edf->RecordDuration;
  i0=(int32_t)round(t0*fs); i1=(int32_t)round(t1*fs);
  for (i=i0; i<edf->signal[err_chn].NrOfSamples && i<i1; i++) {
    edf->signal[err_chn].data[i]=mrk;
  }
  return(i-i0);
}

/* copies the number of records specified in the edf_out header from fp_in to
 * fp_out, starting with record number first-record of fp_in 
 */ 
void edf_copy_samples( const edf_t * const edf_in, FILE * const fp_in, 
    const edf_t * edf_out, FILE * const fp_out, const uint32_t first_record )
{
#define EDF_COPY_BUFF_SIZE 1024*1024

  uint64_t in_start_byte, out_start_byte;
  int32_t record_size;
  uint64_t bytes_to_copy;
  char buff[EDF_COPY_BUFF_SIZE];

  assert( edf_in != NULL );
  assert( fp_in  != NULL );
  assert( fp_out != NULL );

  record_size = edf_get_record_size( edf_in );
  if ( record_size != edf_get_record_size( edf_out ) )
  {
    fprintf(stderr, "Record sizes of input and output files are different!\n");
    abort();
  }

  /* how many bytes are we copying? */
  bytes_to_copy = edf_out->NrOfDataRecords * record_size;

  /* first byte where to start in the input file */
  in_start_byte = edf_in->NrOfHeaderBytes + record_size * first_record;
  if ( fseeko64( fp_in, in_start_byte, SEEK_SET ) == -1 )
  {
    fprintf( stderr, "Error while seeking in input file: %s\n", strerror(errno) );
    exit(-1);
  }

  /* move output file to first byte beyond the header */
  out_start_byte = edf_out->NrOfHeaderBytes;
  if ( fseeko64( fp_out, out_start_byte, SEEK_SET ) == -1 )
  {
    fprintf( stderr, "Error while seeking in output file: %s\n", strerror(errno) );
    exit(-1);
  }

  if ( vb & 0x01 )
  {
    fprintf(stderr, "# Copying %"PRIu64"d bytes\n", bytes_to_copy );
    fprintf(stderr, "# Input starts at  %"PRIu64"d\n", in_start_byte );
    fprintf(stderr, "# Output starts at %"PRIu64"d\n", out_start_byte );
  }

  /* copy all bytes */
  while ( bytes_to_copy > 0 )
  {
    size_t num_to_read = (size_t) (bytes_to_copy < EDF_COPY_BUFF_SIZE ? bytes_to_copy : EDF_COPY_BUFF_SIZE);
    size_t num_read    = fread ( buff, 1, num_to_read, fp_in  );
    size_t num_written = fwrite( buff, 1, num_read,    fp_out );

    if (vb&0x04) 
    {
      fprintf(stderr, "# %zu bytes read, %zu bytes written, %"PRIu64"d left\n", 
          num_read, num_written, (size_t) (bytes_to_copy - num_read) );
    }

    /* check for errors while writing */
    if ( num_written != num_read )
    {
      fprintf(stderr, "Could write only %zu of %zu bytes to output file: %s\n", 
          num_written, num_read, strerror(ferror(fp_out)) );
      exit(-1);
    }

    bytes_to_copy -= num_read;

    /* check for errors while reading */
    if ( bytes_to_copy != 0  &&  num_read < num_to_read )
    {
      if ( feof(fp_in) )
        fprintf(stderr, "Input file is too short!\n" );
      else if ( ferror(fp_in) )
        fprintf(stderr, "Error while reading from file: %s\n", strerror(ferror(fp_out)) );
      else
        fprintf(stderr, "Unknown error weirdness!\n"); /* should never happen */

      exit(-1);

    }
  }

#undef EDF_COPY_BUFF_SIZE

}

/** EDF/BDF annotation functions */

/** Read annotation sample count positions from file 'fname'
 *   into EDF annotations array.
 * @note the number of annotation is returned in 'n'.
 * @return array pointer to annotation array
*/
edf_annot_t *edf_read_annot(char *fname, char *txt, int32_t *n, uint8_t format) {

  FILE   *fp;
  char    line[2*EDFMAXNAMESIZE]; /**< temp line buffer */
  int32_t lc=0;           /**< line counter */
  int32_t ni;             /**< number of arguments read */
  int32_t sec=0;          /**< sample error count */
  int32_t i=0;            /**< annotation count */
  edf_annot_t *p=NULL;    /**< annotation array pointer */
  int32_t size=CHUNKSIZE; /**< allocation chuck size counter */
  double  value;          /**< annotation number read from file */
  char    atxt[EDFMAXNAMESIZE]; /**< annotation text */
  
  fprintf(stderr,"# Open R-wave annotation file %s\n",fname);
  if ((fp=fopen(fname,"r"))==NULL) {
    perror(""); (*n)=i; return(p);
  }
  
  /* allocate space for annotations */
  if ((p=(edf_annot_t *)calloc(size,sizeof(edf_annot_t)))==NULL) {
    fprintf(stderr,"# Error not enough memory to allocate %d ecg events!!!\n",
              size);
  }
 
  /* read whole ASCII EEG sample file */ 
  while (!feof(fp)) {
    /* clear line */
    line[0]='\0';
    /* read one line with hist data */
    if (fgets(line,sizeof(line)-1,fp)==NULL) { continue; }
    lc++;
    /* skip comment lines or too small lines */
    if ((line[0]=='#') || (strlen(line)<2)) {
      continue;
    } 
    ni=sscanf(line,"%lf %s", &value, atxt);
    if (ni<1) { 
      if (sec==0) {
        fprintf(stderr,"# Error: argument count %d !=1 on line %d. Skip it\n",ni,lc);
      }
      sec++; 
      continue;
    } else {
      if (format==FORMAT_SAMP) {
        p[i].sc  = (int32_t) round(value); 
        p[i].sec = -1;
      } else if ((format==FORMAT_SEC) || (format==FORMAT_SSLN)) {
        p[i].sc  = -1;
        p[i].sec = value;
      } else if (format==FORMAT_MSEC) {
        p[i].sc  = -1;
        p[i].sec = value/1000.0;
      } else {
        fprintf(stderr, "Unsupported format type\n");
        exit(-1);
      }
      if (vb&0x01) {
        fprintf(stderr," %6d %f %8"PRIi64"\n",i,p[i].sec,p[i].sc);
      }

      if ((ni>1) && (strlen(txt)<2)) {
        strncpy(p[i].txt,atxt,ANNOTTXTLEN);
      } else {      
        strncpy(p[i].txt,txt,ANNOTTXTLEN);
      }
      /* be sure that annotation text is ended with a character '\0' */
      p[i].txt[ANNOTTXTLEN-1]='\0';
      /* next annotation */
      i++;
    }
    
    /* check available storage space */
    if (i>=size) {
      /* increment chuck size counter */
      size+=CHUNKSIZE;
      /* allocate one chuck extra */
      p=(edf_annot_t *)realloc(p,size*sizeof(edf_annot_t));
    }
  }
  fclose(fp);

  if (sec>1) { fprintf(stderr,"# Error: skipped %d annotations!!!\n",sec); }

  fprintf(stderr,"# Info: found %d annotations\n",i);
  (*n)=i;

  return(p);
}
/** Print character buffer 'buf' of 'n' characters to file 'fp'
 * @return number of printed characters 
*/
int32_t prt_char_buf(FILE *fp, char *msg, char *buf, int32_t n) {

  int32_t i;
  int32_t nc=0;
  
  fprintf(fp,"%s :",msg);
  for (i=0; i<n; i++) {
    if (buf[i] < 0x20) { 
      switch (buf[i]) {
        case  20: nc+=fprintf(fp,"%1c",'*'); break;
        case  21: nc+=fprintf(fp,"%1c",'&'); break;
        case   0: nc+=fprintf(fp,"%1c",'|'); break;
        default:  
         nc+=fprintf(fp,"<%02X>",buf[i]&0xFF); break;
      }
    } else {
      nc+=fprintf(fp,"%1c",buf[i]);
    }
  }
  nc+=fprintf(fp,"\n");
  return(nc);
}

/** Add 'n' annotations in 'p' into edf structure 'edf'
* @return number inserted annotations
*/
int32_t edf_add_annot(edf_t *edf, edf_annot_t *p, int32_t n) {

  int32_t  i,j;     /**< general index */
  int32_t  imsr;    /**< index of channel with most samples per record */
  int32_t  acn;     /**< channel number of EDF Annotations */
  int32_t  bps=2;   /**< bytes per sample */
  int32_t *dp;      /**< data pointer of EDF Annotations channel */
  char    *cp,*cp0; /**< character pointer of EDF Annotations channel */
  char     buf[2*ANNOTRECSIZE]; /**< temp string buffer */
  int32_t  go;      /**< keep on writing in this annotation record */
  double   ts;      /**< timestamp [s] from start of recording */
  int64_t  sc;      /**< sample counter */
  int32_t  nc;      /**< number of character in current annotation */
  int32_t  tnc=0;   /**< number of annotation character in current record */
  int32_t  rv=0;    /**< return value: number of inserted annotations */
  char     tmp[EDFMAXNAMESIZE];
  struct tm t0;     /**< start date + time in tm struct */
  char     when[EDFMAXNAMESIZE];
  char     id[12];   /**< patient id */
  
  /* channel with the most samples per record determines the samplerate */
  imsr=0; 
  for (j=1; j<edf->NrOfSignals; j++) {
    if (edf->signal[j].NrOfSamplesPerRecord > edf->signal[imsr].NrOfSamplesPerRecord) {
      imsr=j;
    }
  }

  /* get annotation channel number */
  if (edf->bdf==1) {
    acn=edf_fnd_chn_nr(edf,"BDF Annotations");
  } else {
    acn=edf_fnd_chn_nr(edf,"EDF Annotations");
  }

  if (acn<0) {
    acn=edf->NrOfSignals;
    /* add extra signal 'EDF Annotations' */
    edf->signal=(edf_signal_t *)realloc(edf->signal,(edf->NrOfSignals+1)*sizeof(edf_signal_t));
    edf->NrOfHeaderBytes+=256;
    if (edf->bdf==1) { 
      strcpy(edf->signal[acn].Label, "BDF Annotations");
      strcpy(edf->DataFormatVersion, "BDF+C");
      bps=3;
    } else {
      strcpy(edf->signal[acn].Label, "EDF Annotations");
      strcpy(edf->DataFormatVersion, "EDF+C");
      bps=2;
    }
    strcpy(edf->signal[acn].TransducerType, "");
    strcpy(edf->signal[acn].PhysicalDimension, "-");
    
    edf->signal[acn].DigitalMin=-(1<<(8*bps-1));
    edf->signal[acn].DigitalMax= (1<<(8*bps-1))-1;
    
    edf->signal[acn].PhysicalMin=edf->signal[acn].DigitalMin;
    edf->signal[acn].PhysicalMax=edf->signal[acn].DigitalMax;
    
    strcpy(edf->signal[acn].Prefiltering, "");
    strcpy(edf->signal[acn].Reserved, "Reserved");
    /* make PatientId EDF+/BDF+ compliant: use X for anonymous */
    /* format: patient_code sex (F/M) birthdate (dd-MMM-yyyy) patient_name */
    /* example 'MCH-0234567 F 02-MAY-1951 Haagse_Harry' */ 
    strncpy(id,edf->PatientId,12);
    sprintf(tmp,"X X X X %s",id); 
    //fprintf(stderr,"tmp: %s\n",tmp);
    strncpy(edf->PatientId, tmp, sizeof(edf->PatientId)-1);
    //fprintf(stderr,"PatientId: %s\n",edf->PatientId);
    /* make RecordingId EDF+/BDF+ compliant: use X for anonymous */
    /* format: 'Startdate' date (dd-MMM-yyyy) investigation_code investigator used_equipment */
    /* example: 'Startdate 02-MAR-2002 PSG-1234/2002 NN Telemetry03' */
    t0=*localtime(&edf->StartDateTime);  
    strftime(when,EDFMAXNAMESIZE,"%d-%b-%Y",&t0);
    /* convert month name into uppercase letters */
    for (i=0; i<(signed) strlen(when); i++) { when[i] = (char) (toupper(when[i])); }
    sprintf(tmp,"Startdate %s X X X %s",when,edf->RecordingId); 
    strncpy(edf->RecordingId, tmp, sizeof(edf->RecordingId)-1);
    
    /* allocate space for EDF Annotations */
    edf->signal[acn].NrOfSamplesPerRecord=ANNOTRECSIZE/bps;
    edf->signal[acn].NrOfSamples=edf->NrOfDataRecords * ANNOTRECSIZE/4;
    edf->signal[acn].data=(int32_t *)calloc(edf->NrOfDataRecords*ANNOTRECSIZE/4,sizeof(int32_t));
    /* increment number of signals */
    edf->NrOfSignals++;
  } 
  
  dp=edf->signal[acn].data;
  ts=0.0; sc=0; i=0;
  for (j=0; j < edf->NrOfDataRecords; j++) {
    /* convert data pointer of EDF Annotations channel to character pointer */
    cp=(char *)dp; cp0=cp;
    /* write timestamp of this record */
    nc=sprintf(cp,"+%.6f%1c%1c%1c",ts,20,20,'\0'); cp+=nc; tnc=nc;
    /* insert as many as possible annotations in this record until go==0 */
    go=1;
    while ((go==1) && (i<n) && (sc>p[i].sc)) {
      nc=sprintf(buf, "+%.6f%1c%s%1c%1c", p[i].sec, 20, p[i].txt, 20, '\0');
      if (tnc+nc < ANNOTRECSIZE) {
        /* this annotation still fits */
        nc=sprintf(cp,"+%.6f%1c%s%1c%1c",p[i].sec,20,p[i].txt,20,'\0');
        if (vb&0x400) {
          fprintf(stderr,"# Info: insert annotation %d in record %d ts %.4f at %.4f (sample %"PRIi64")\n",
            i,j,ts,p[i].sec,p[i].sc);
          prt_char_buf(stderr,"# annot",buf,nc);
        }
        i++; cp+=nc; tnc+=nc;
      } else {
        go=0;
      }
    }
    if (vb&0x400) {
      prt_char_buf(stderr,"# record", cp0, tnc);
    }
    /* data pointer and timestamp of next record */
    dp+=ANNOTRECSIZE/4;
    ts+=edf->RecordDuration; sc+=edf->signal[imsr].NrOfSamplesPerRecord;
  }
  /* santiy check */
  if (i<n) {
    fprintf(stderr,"# Warning: %d annotations didn't fit\n",n-i);
  }
  rv=i;

  if (vb&0x800) {
    cp=(char *)edf->signal[acn].data;
    for (j=0; j<edf->NrOfDataRecords*ANNOTRECSIZE; j++) {
      if (j%(2*n)==0) { fprintf(stderr,"\n%6d |",j); }
      if (cp[j]<0x20) { fprintf(stderr," %02X",cp[j]); } else { fprintf(stderr," %2c",cp[j]); }
    }
  }

  if (vb&0x1000) {
    cp=(char *)edf->signal[acn].data;
    for (j=0; j<edf->NrOfDataRecords; j++) {
      prt_char_buf(stderr, "# buf", cp, ANNOTRECSIZE);
      cp += ANNOTRECSIZE;
    }
  }
  return(rv);
}

/** Get annotation from edf structure 'edf' 
 * @return array pointer to annotation array
 * @note the number of annotation is returned in 'n'.
 * @note record timestamps are returned in 'edf->rts' (one per record)
*/
edf_annot_t *edf_get_annot(edf_t *edf, int32_t *n) {

  int32_t i=0,j,k,chn;    /**< annotation count */
  int32_t  acn;           /**< channel number of EDF Annotations */
  edf_annot_t *p=NULL;    /**< annotation array pointer */
  int32_t size=CHUNKSIZE; /**< allocation chuck size counter */
  double   fs=1024.0;     /**< default sample frequency */
  int32_t *dp;            /**< data pointer of EDF Annotations channel */
  char     buf[ANNOTRECSIZE];
  double   ts;          /**< timestamp [s] from start of recording */
  double   dt;          /**< annotation duration [s] (default=0.0) */
  int32_t  sc;          /**< sample counter */
  char     txt[ANNOTRECSIZE];
  int32_t  tnc=0;       /**< number of annotation character in current record */
  int32_t  bs=0;        /**< block size */
  int32_t  idx;         /**< block index */
  int32_t  imsr;        /**< index of channel with most samples per record */
  char     token[ANNOTRECSIZE];       /**< token for parsing line */
  int32_t  st,tc,cnt;   /**< parse state, token counter, annotation counter */
  char     aname[32];   /**< EDF/BDF Annotation name */
  edf_annot_t tmp;
  
  fprintf(stderr,"# Info: get EDF/BDF annotations\n");
  
  /* get annotation channel number */
  if (edf->bdf==1) {
    strcpy(aname,"BDF Annotations");
  } else {
    strcpy(aname,"EDF Annotations");
  } 

  chn=0; acn=-1;
  while ((chn < edf->NrOfSignals) && (acn < 0)) {
    if (strcmp(edf->signal[chn].Label,aname)==0) { 
      acn=chn;  
    } else {
      chn++;
    }
  }
  
  if (acn>=0) {
    /* byte per annotation block in internal edf data structure */
    //bs=edf->signal[acn].NrOfSamplesPerRecord*bps;
    bs=ANNOTRECSIZE;
    /* channel with the most samples per record determines the samplerate */
    imsr=0; 
    for (j=1; j<edf->NrOfSignals; j++) {
      if (j!=acn) {
        if (edf->signal[j].NrOfSamplesPerRecord > edf->signal[imsr].NrOfSamplesPerRecord) {
          imsr=j;
        }
      }
    }
    fs=edf->signal[imsr].NrOfSamplesPerRecord/edf->RecordDuration;
    
    if (vb&0x2000) {
      fprintf(stderr,"imsr %d fs %f [Hz]\n",imsr,fs);
    }
    
    /* allocate space for annotations */
    if ((p=(edf_annot_t *)calloc(size,sizeof(edf_annot_t)))==NULL) {
      fprintf(stderr,"# Error not enough memory to allocate %d ecg events!!!\n",
                size);
    }
    
    /* allocate space for timestamp of all records */
    if ((edf->rts=(double *)calloc(edf->NrOfDataRecords,sizeof(double)))==NULL) {
      fprintf(stderr,"# Error not enough memory to allocate %d record timestamp!!!\n",
                edf->NrOfDataRecords);
    }
  }
    
  while ((chn < edf->NrOfSignals) && (acn>=0)) {
    
    fprintf(stderr,"# Info: annotation channel nr %d\n", acn);
     
    dp=edf->signal[acn].data; idx=0;
    for (k=0; k<edf->NrOfDataRecords; k++) {
      /* get characters */
      memcpy(buf, (char *)&dp[idx], bs);
      
      if (vb&0x2000) {
        prt_char_buf(stderr, "# buf", buf, bs);
      }
      
      /* replace '\0' in the middle of a serie of annotation with '|' */
      for (i=0; i<bs; i++)  {
        if (buf[i]==  20) { buf[i]='*'; }
        if (buf[i]==  21) { buf[i]='&'; }
        if (buf[i]=='\0') { buf[i]='|'; }
      }
    
      /* parse rest of annotation record */    
      i=0; j=0; strcpy(token,""); st=0; tc=0; cnt=0; ts=0.0; dt=0.0; strcpy(txt,"");
      while (i<bs) {
        if ((buf[i]=='*') || (buf[i]=='|') || (buf[i]=='&')) { 
          token[j]='\0';
          switch (st) {
            case 0: 
              ts=strtod(token,NULL); 
              if (buf[i]=='&') { st=1; }
              break;
            case 1: 
              strcpy(txt,token); 
              break;
            case 2: 
              dt=0.0; 
              if (j>0) { 
                dt=strtod(token,NULL); 
              } 
              break;
            default: 
              break;
          }
          if (vb&0x2000) {
            fprintf(stderr,"cnt %d st %d token '%s'\n",cnt,st,token);
          }
          if (st==2) {
            sc=(int32_t)round(ts*fs); if (tc==2) { edf->rts[k]=ts; }
            if (vb&0x2000) {
              fprintf(stderr," %d %d %d: %f %f %s %d %d %.1f\n",k,cnt,tnc,ts,dt,txt,sc,tc,edf->rts[k]);
            }
            /* skip record timestamp */
            if ((strlen(txt)>0) & (ts>0.0)) { 
              //fprintf(stderr," %d\n",sc);
              /* check available storage space */
              if (tnc>=size) {
                /* increment chuck size counter */
                size+=CHUNKSIZE;
                /* allocate one chuck extra */
                p=(edf_annot_t *)realloc(p,size*sizeof(edf_annot_t));
              }
              /* insert annotation data */
              p[tnc].sc  = sc; 
              p[tnc].sec = ts;
              strncpy(p[tnc].txt,txt,ANNOTTXTLEN-1);
              /* next annotation */ 
              tnc++;
            }
            st=0; cnt++; dt=0.0; strcpy(txt,"");
          } else {
            st++;
          }
          j=0; strcpy(token,""); tc++;
        } else { 
          token[j]=buf[i]; j++; 
        }
        i++;
      }
      
      //idx+=edf->signal[acn].NrOfSamplesPerRecord;
      idx += bs/4;
    } 

    fprintf(stderr,"# Info: found %d annotations\n", tnc);

    /* find next annotation channel */
    acn=-1; chn++;
    while ((chn < edf->NrOfSignals) && (acn < 0)) {
      if (strcmp(edf->signal[chn].Label,aname)==0) { 
        acn=chn;  
      } else {
        chn++;
      }
    }
      
  }
  
  /* check timestamps of all records */
  tc = 0;
  for (i=1; i < edf->NrOfDataRecords; i++) {
    if (fabs((edf->rts[i] - edf->rts[i-1] - edf->RecordDuration)/edf->RecordDuration) > 0.0001) {
      if (vb&0x2000) {
        fprintf(stderr,"#Warning: time axis gap at record %d on t %.1f [s] of %.1f [s] long\n",
          i, edf->rts[i], edf->rts[i] - edf->rts[i-1]);
      }
      if (tnc>=size) {
         /* increment chuck size counter */
         size+=CHUNKSIZE;
         /* allocate one chuck extra */
         p=(edf_annot_t *)realloc(p,size*sizeof(edf_annot_t));
      }
      /* insert annotation data */
      p[tnc].sc  = (int32_t)round(fs * edf->rts[i]); 
      p[tnc].sec = edf->rts[i];
      snprintf(p[tnc].txt, ANNOTTXTLEN-1, "g%.1f", edf->rts[i] - edf->rts[i-1]);
      /* next annotation */ 
      tnc++; tc++;
    }
  }
  if (tc>0) {
    fprintf(stderr,"# Warning: found %d gaps in the time-axis\n",tc);
  }

  /* sort all annotations */
  for (i=0; i<tnc-1; i++) {
    for (j=i+1; j<tnc; j++) {
      if (p[i].sec > p[j].sec) {
        tmp = p[i]; p[i]=p[j]; p[j]=tmp;
      }
    }
  }
  (*n)=tnc;
  return(p);
}



