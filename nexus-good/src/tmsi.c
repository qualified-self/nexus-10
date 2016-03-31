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

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#ifdef _MSC_VER
  #include "win32_compat.h"
  #include "stdint_win32.h"
  #include <winsock2.h>
  #pragma comment(lib, "ws2_32.lib")
#else
  #include <stdint.h>
  #include <sys/ioctl.h>
  #include <alloca.h>
  #include <unistd.h>
  #include <sys/time.h>
  #include <termios.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <time.h>
#include <malloc.h>

#include "tmsi.h"

#define VERSION "$Revision: 0.1 $"

#define TMSBLOCKSYNC (0xAAAA)  /**< TMS block sync word */
#define WAIT_FOR_NEXT_BYTE (2000) /**<Wait time in us */
#define MAX_RECEIVED_COUNT (30)
#define RETRY_COUNT (3)

#define TMSACKNOWLEDGE      (0x00)
#define TMSCHANNELDATA      (0x01)
#define TMSFRONTENDINFO     (0x02)
#define TMSFRONTENDINFOREQ  (0x03)
#define TMSRTCREADREQ       (0x06) 
#define TMSRTCDATA          (0x07) 
#define TMSRTCTIMEREADREQ   (0x1E) 
#define TMSRTCTIMEDATA      (0x1F) 
#define TMSIDREADREQ        (0x22)
#define TMSIDDATA           (0x23)
#define TMSKEEPALIVEREQ     (0x27)
#define TMSVLDELTADATA      (0x2F)
#define TMSVLDELTAINFOREQ   (0x30) 
#define TMSVLDELTAINFO      (0x31)

static int32_t saw_len =      5; /**< default saw length:  Nexus10==5, Mark II==14 */
static int32_t saw_chn =     13; /**< default saw channel nr:  Nexus10==13, Mark II==15 */
static int32_t  tms_vb = 0x0000; /**< verbose level */
static FILE       *fpl =   NULL; /**< log file pointer */

const int32_t SWITCH_CHANNEL = 12;      /**< Nexus switch channel nr */
const float   BUTTON_DELTA   = 0.0220f; /**< button delta duration [s] */  

/** set verbose level of module TMS to 'new_vb'.
 * @return old verbose value
*/
int32_t tms_set_vb(int32_t new_vb) {

  int32_t old_vb=tms_vb;

  tms_vb=new_vb;
  return(old_vb);
}
 
/** get verbose variable for module TMS
 * @return current verbose level
*/
int32_t tms_get_vb() {
  return(tms_vb);
}

/** A uint32_t send from PC to the front end: first low uint16_t, second high uint16_t
 *  A uint32_t send from front end to the PC: first low uint16_t, second high uint16_t
 * EXCEPTION: channeldata samples are transmitted as first high uint16_t, second low uint16_t
 * When transmitted as bytes (serial interface): first low-byte, then hi-byte */


/** Get current time in [sec] since 1970-01-01 00:00:00.
 * @note current time has micro-seconds resolution.
 * @return current time in [sec].
*/
double get_time() {
#ifdef _MSC_VER

  double seconds;
  SYSTEMTIME t;
  GetSystemTime( &t );

  seconds = 3600*t.wHour + 60*t.wMinute + t.wSecond + 1e-3*t.wMilliseconds;
  return seconds;

#else
  struct timeval tv;   /**< time value */
  /* struct timezone tz; */  /**< time zone NOT used under linux */
  double ts=0.0;       /**< default time [s] */

  if (gettimeofday(&tv,NULL)!=0) {
    fprintf(stderr,"time_us: Error in gettimeofday\n");
    return(ts);
  }
  ts=1e-6*tv.tv_usec + tv.tv_sec;
  return(ts);
#endif
}

/** Get current date and time as string in 'datetime' 
 *  with maximum 'size'.
 * @note current time has micro-seconds resolution.
 * @return current time in [sec].
*/
double get_date_time(char *datetime, int32_t size) {
#ifdef _MSC_VER

  double seconds;
  SYSTEMTIME t;
  GetSystemTime( &t );

  sprintf(datetime,"%04d%02d%02dT%02d%02d%02d",
      t.wYear, t.wMonth, t.wDay,
      t.wHour, t.wMinute, t.wSecond );

  seconds = 3600*t.wHour + 60*t.wMinute + t.wSecond + 1e-3*t.wMilliseconds;
  (void) size;    // Avoid warning about unused parameter
  return seconds;

#else

  struct timeval tv;         /**< time value */
  /* struct timezone tz; */  /**< time zone NOT used under linux */
  double ts=0.0;             /**< default time [s] */
  time_t now;
  struct tm t0;

  if (gettimeofday(&tv,NULL)!=0) {
    fprintf(stderr,"time_us: Error in gettimeofday\n");
    return(ts);
  }
  ts=1e-6*tv.tv_usec + tv.tv_sec;

  /* use day and current time as file name */
  now=(time_t)(tv.tv_sec);
  t0=*localtime(&now);  
  // strftime lijkt niet te werken !!!
  // strftime(datetime,size,"%Y%m%dT%H%M%S",&t0);
  // Meer precies snprintf lijkt niet te werken sprintf wel!!!
  sprintf(datetime,"%04d%02d%02dT%02d%02d%02d",
      1900+t0.tm_year,t0.tm_mon+1,t0.tm_mday,
      t0.tm_hour,t0.tm_min,t0.tm_sec);

  (void) size;    // Avoid warning about unused parameter
  return(ts);

#endif
}


/** Get integer of 'n' bytes from byte array 'msg' starting at position 's'.
 * @note n<=4 to avoid bit loss
 * @note on return start position 's' is incremented with 'n'.
 * @return integer value.
 */
int32_t tms_get_int(uint8_t *msg, int32_t *s, int32_t n) {

  int32_t i;   /**< general index */
  int32_t b=0; /**< temp result */
  
  /* skip overflow bytes */
  if (n>4) { n=4; }
  /* get MSB byte first */
  for (i=n-1; i>=0; i--) {
    b=(b<<8) | (msg[(*s)+i] & 0xFF);
  }
  /* increment start position 's' */
  (*s)+=n;
  return(b);
}

/** Put 'n' LSB bytes of 'a' into byte array 'msg' 
 *   starting at location 's'.
 * @note n<=4.
 * @note start location is incremented at return.
 * @return number of bytes put.
 */
int32_t tms_put_int(int32_t a, uint8_t *msg, int32_t *s, int32_t n) {

  int32_t i=0;

  if (n>4) { n=4; }

  for (i=0; i<n; i++) {
    msg[(*s)+i]=(uint8_t)(a & 0xFF);
    a=(a>>8);
  }
  /* increment start location */
  (*s)+=n;
  /* return number of byte processed */
  return(n);
}

/* byte reverse */
uint8_t tms_byte_reverse(uint8_t a) {

 uint8_t b=0;
 int32_t i;

 for (i=0; i<8; i++) { 
   b=(b<<1) | (a & 0x01); a=a>>1;
 }
 return b; 
}

/** Grep 'n' bits signed long integer from byte buffer 'buf'
 *  @note most significant byte first. 
 *  @return 'n' bits signed integer
 */
int32_t get_int32_t(uint8_t *buf, int32_t *bip, int32_t n) {

  int32_t i=*bip;   /**< start location */
  int32_t a=0;      /**< wanted integer value */
  int32_t mb;       /**< maximum usefull bits in 'byte[i/8]' */
  int32_t wb;       /**< number of wanted bits in current byte 'buf[i/8]' */

  while (n>0) {
    /* calculate number of usefull bits in this byte */
    mb=8-(i%8);
    /* select maximum needed number of bits */
    if (n>mb) { wb=mb; } else { wb=n; }
    /* grep 'wb' bits out of byte 'buf[i/8]' */
    a=(a<<wb) | ((buf[i/8]>>(mb-wb)) & ((1<<wb)-1));
    /* decrement number of needed bits, and increment bit index */
    n-=wb; i+=wb;
  }

  /* put back resulting bit position */
  (*bip)=i; 
  return(a);
}

/** Grep 'n' bits signed long integer from byte buffer 'buf' 
 *  @note least significant byte first. 
 *  @return 'n' bits signed integer
 */
int32_t get_lsbf_int32_t(uint8_t *buf, int32_t *bip, int32_t n) {

  int32_t i=*bip;   /**< start location */
  int32_t a=0;      /**< wanted integer value */
  int32_t mb;       /**< maximum usefull bits in 'byte[i/8]' */
  int32_t wb;       /**< number of wanted bits in current byte 'buf[i/8]' */
  int32_t m=0;      /**< number of already written bits in 'a' */

  while (n>0) {
    /* calculate number of usefull bits in this byte */
    mb=8-(i%8);
    /* select maximum needed number of bits */
    if (n>mb) { wb=mb; } else { wb=n; }
    /* grep 'wb' bits out of byte 'buf[i/8]' */
    a|=(((buf[i/8]>>(i%8))&((1<<wb)-1)))<<m; 
    /* decrement number of needed bits, and increment bit index */
    n-=wb; i+=wb; m+=wb;
  }
  /* put back resulting bit position */
  (*bip)=i; 

  return(a);
}

/** Grep 'n' bits sign extented long integer from byte buffer 'buf' 
 *  @note least significant byte first. 
 *  @return 'n' bits signed integer
 */
int32_t get_lsbf_int32_t_sign_ext(uint8_t *buf, int32_t *bip, int32_t n)
{
  int32_t a;

  a = get_lsbf_int32_t(buf,bip,n);
  /* check MSB-1 bits */
  if ((1<<(n-1)&a)==0) {
    /* add heading one bits for negative numbers */
    a= (~((1<<n)-1))|a;
  }
  return(a);
}

/** Get 4 byte long float for 'buf' starting at byte 'sb'.
  * @return float
*/
float tms_get_float(uint8_t *msg, int32_t *sb) {

  float   sign;
  float   expo;
  float   mant;
  int32_t manti;
  int32_t bip=0;
  float   a;
  int32_t i;
  uint8_t buf[4];
  
  buf[0]=msg[*sb+3]; 
  buf[1]=msg[*sb+2]; 
  buf[2]=msg[*sb+1]; 
  buf[3]=msg[*sb+0]; 
  
  if ((buf[0]==0) && (buf[1]==0) && (buf[2]==0) && (buf[3]==0)) {
    (*sb)+=4;
    return(0.0);
  }
  /* fetch sign bit */  
  if (get_int32_t(buf,&bip,1)==1) { sign=-1.0; } else { sign=+1.0; }
  /* fetch 8 bits exponent */
  expo=(float)get_int32_t(buf,&bip,8);
  /* fetch 23 bits mantissa */
  manti=(get_int32_t(buf,&bip,23)+(1<<23));
  mant=(float)(manti);
  
  if (tms_vb&0x10) {
    for (i=0; i<4; i++) {
      fprintf(stderr," %02X",buf[i]);
    }
    fprintf(stderr," sign %2.1f expo %6.0f manti 0x%06X\n",sign,expo,manti);
    /* print this only once */
    tms_vb=tms_vb & ~0x10;    
  }
  /* construct float */  
  a=sign*mant*powf(2.0f,expo-23.0f-127);
  
  /* 4 bytes float */
  (*sb)+=4;
  /* return found float 'a' */
  return(a);
}

/** Get 4 byte long float for 'msg' starting at byte 'i'.
  * @return float
*/
float tms_get_float32(uint8_t *msg, int32_t *i) {

  tms_float_t a; /**< union of float and character array */
  
  memcpy(&a.ch[0],&msg[(*i)],4);
  (*i)+=4;
  return(a.fl);
}

/** Get string starting at position 'start' of message 'msg' of 'n' bytes.
 * @note will malloc storage space for the null terminated string.
 * @return pointer to string or NULL on no success.
*/ 
char *tms_get_string(uint8_t *msg, int32_t n, int32_t start) {

  int32_t size;         /**< string size [byte] */
  int32_t i=start;      /**< general index */
  char    *string=NULL; /**< string pointer */
  
  size=2*(tms_get_int(msg,&i,2)-1);
  if (i+size>n) {
    fprintf(stderr,"# Error: tms_get_string: index 0x%04X out of range 0x%04X\n",i+size,n);
  } else {
    /* malloc space for the string */
    string = malloc(size);
    /* copy content */
    strncpy(string,(char *)&msg[i],size);
  }
  return(string);
}

/** Get Pascal string 'pstr' of maximum length 'outsize' from starting 
 * position 'i' of message 'msg' of 'n' bytes.
 * @note will malloc storage space for the null terminated string.
 * @return number of characters in 'out'.
*/ 
int32_t tms_get_pascal_string(uint8_t *msg, int32_t *i, char *out, int32_t outsize) {

  int32_t size=msg[*i];  /**< string size [byte] */
  
  if (size+1>outsize) {
    fprintf(stderr,"# tms_get_pascal_string: outsize %d < size %d\n",outsize,size+1);
    size=outsize-1;
  }
  /* copy content */
  strncpy(out,(char *)&msg[(*i)+1],size);
  /* always end with '\0' */
  out[size]='\0';
  /* advance byte pointer */
  *i+=outsize;
  /* return content of pascal string */
  return(size);
}

/** Calculate checksum of message 'msg' of 'n' bytes.
 * @return checksum.
*/
int16_t tms_cal_chksum(uint8_t *msg, int32_t n) {

  int32_t i;                   /**< general index */
  int16_t sum=0x0000;          /**< checksum */
  
  for (i=0; i<(n/2); i++){
    sum += (msg[2*i+1] << 8) + msg[2*i];
  }
  return(sum);
}
 
/** Check checksum buffer 'msg' of 'n' bytes.
 * @return packet type.
*/
int16_t tms_get_type(uint8_t *msg, int32_t n) {

  int16_t rv =(int16_t)msg[3]; /**< packet type */
  
  if (n<4){
    rv=-1;
  }
  return(rv);
}
 
/** Get payload size of TMS message 'msg' of 'n' bytes.
 *  @note 'i' will return start byte address of payload.
 *  @return payload size.
*/
int32_t tms_msg_size(uint8_t *msg, int32_t n, int32_t *i) {

  int32_t size=0; /**< payload size */

  (void) n;     // Avoid warning about unused parameter
  
  (*i)=2; /* address location */
  size=tms_get_int(msg,i,1);
  (*i)=4; 
  if (size==0xFF) {
    size=tms_get_int(msg,i,4);
  }
  return(size);
}

/** Open TMS log file with name 'fname' and mode 'md'.
 * @return file pointer.
*/
FILE *tms_open_log(char *fname, char *md) {

  /* open log file */
  if ((fpl=fopen(fname,md))==NULL) {
    perror(fname);
  }
  return(fpl);
}

/** Close TMS log file
 * @return 0 in success.
*/
int32_t tms_close_log() {

  /* close log file */
  if (fpl!=NULL) {
    fclose(fpl);
  }
  return(0);
}

/** Log TMS buffer 'msg' of 'n' bytes to log file.
 * @return return number of printed characters.
*/
int32_t tms_write_log_msg(uint8_t *msg, int32_t n, char *comment) {

  static int32_t nr=0;   /**< message counter */
  int32_t nc=0;          /**< number of characters printed */
  int32_t sync;    /**< sync */
  int32_t type;    /**< type */
  int32_t size;          /**< payload size */
  int32_t pls;           /**< payload start adres */
  int32_t calsum;  /**< calculated checksum */
  int32_t i=0;           /**< general index */
  
  if (fpl==NULL) {
    return(nc);
  }
  i=0;
  sync=tms_get_int(msg,&i,2);
  type=tms_get_type(msg,n);
  size=tms_msg_size(msg,n,&pls);
  calsum=tms_cal_chksum(msg,n);
  nc+=fprintf(fpl,"# %s sync 0x%04X type 0x%02X size 0x%02X checksum 0x%04X\n",comment,sync,type,size,calsum);
  nc+=fprintf(fpl,"#%3s %4s %2s %2s %2s\n","nr","ba","wa","d1","d0");
  i=0;
  while (i<n) {
    nc+=fprintf(fpl," %3d %04X %02X %02X %02X %1c %1c\n",
          nr,(i&0xFFFF),((i-pls)/2)&0xFF,msg[i+1],msg[i],
          ((msg[i+1]>=0x20 && msg[i+1]<=0x7F) ? msg[i+1] : '.'),
          ((msg[i  ]>=0x20 && msg[i  ]<=0x7F) ? msg[i  ] : '.')
         );
    i+=2;
  }
  /* increment message counter */
  nr++;
  return(nc);
}

/** Read TMS log number 'en' into buffer 'msg' of maximum 'n' bytes from log file.
 * @return return length of message.
*/
int32_t tms_read_log_msg(uint32_t en, uint8_t *msg, int32_t n) {

  int32_t br=0;          /**< number of bytes read */
  char line[0x100];      /**< temp line buffer */
  uint32_t nr;            /**< log event number */
  int32_t ba;            /**< byte address */
  int32_t wa;            /**< word address */
  int32_t d0,d1;         /**< byte data value */
  int32_t sec=0;         /**< sequence error counter */
  int32_t lc=0;          /**< line counter */
  int32_t ni=0;          /**< number of input arguments */
  
  if (fpl==NULL) {
    return(br);
  }
  /* seek to begin of file */
  fseek(fpl,0,SEEK_SET);
  /* read whole ASCII EEG sample file */ 
  while (!feof(fpl)) {
    /* clear line */
    line[0]='\0';
    /* read one line with hist data */
    if (fgets(line,sizeof(line)-1,fpl)==NULL) { continue; } lc++;
    /* skip comment lines or too small lines */
    if ((line[0]=='#') || (strlen(line)<2)) {
      continue;
    } 
    ni=sscanf(line,"%d %X %X %X %X",&nr,&ba,&wa,&d1,&d0);
    if (ni!=5) {
      if (sec==0) {
        fprintf(stderr,"# Error: tms_read_log_msg : wrong argument count %d on line %d. Skip it\n",ni,lc);
      }
      sec++;
      continue;
    }
    if (ba>=n) {
      fprintf(stderr,"# Error: tms_read_log_msg : size array 'msg' %d too small %d\n",n,nr);
    } else 
    if (en==nr) {
      msg[ba+1]=(uint8_t) d1; msg[ba]=(uint8_t) d0; br+=2;
    }
  }
  return(br);
}

/*********************************************************************/
/* Functions for reading the data from the SD flash cards            */
/*********************************************************************/

/** Convert array of 7 integers in 'wrd' to time_t 't'.
 * @note position after parsing in returned in 'i'.
 * @return 0 on success, -1 on failure.
*/
int32_t tms_cvt_date(int32_t *wrd, time_t *t) {
 
  int32_t j;         /**< general index */
  struct tm cal;     /**< broken calendar time */
  int32_t zeros=0;   /**< zero counter */
  int32_t ffcnt=0;   /**< no value counter */

  if (tms_vb&0x01) {
    fprintf(stderr," %04d-%02d-%02d %02d:%02d:%02d\n",
             wrd[0],wrd[1],wrd[2],wrd[4],wrd[5],wrd[6]);
  }
  for (j=0; j<7; j++) {
    if (wrd[j]==0) { zeros++; }
    if ((wrd[j]&0xFF)==0xFF) { ffcnt++; }
  }
  if ((zeros==7) || (ffcnt>0)) {
    /* by definition 1970-01-01 00:00:00 GMT */
    (*t)=(time_t)0;
    return(-1);
  } else {
    /* year since 1900 */
    cal.tm_year=wrd[0]-1900;
    /* months since January [0,11] */
    cal.tm_mon =wrd[1]-1;
    /* day of the month [1,31] */
    cal.tm_mday=wrd[2];
    /* hours since midnight [0,23] */
    cal.tm_hour=wrd[4];
    /* minutes since the hour [0,59] */
    cal.tm_min =wrd[5];
    /* seconds since the minute [0,59] */
    cal.tm_sec =wrd[6];
    /* convert to broken calendar to calendar */
    (*t)=mktime(&cal);
    return(0);
  }
}

/** Get 16 bytes TMS date for 'msg' starting at position 'i'.
 * @note position after parsing in returned in 'i'.
 * @return 0 on success, -1 on failure.
*/
int32_t tms_get_date(uint8_t *msg, int32_t *i, time_t *t) {
 
  int32_t j;         /**< general index */
  int32_t wrd0;      /**< TMS date format */
  int32_t wrd[7];    /**< TMS date format */

  wrd0=tms_get_int(msg,i,2); 
  for (j=0; j<7; j++) {
    wrd[j]=tms_get_int(msg,i,2); 
  }
  /* put year in wrd[0] */
  wrd[0]=wrd0*100+wrd[0];
  /* convert integer array 'wrd' to time 't'. */
  return(tms_cvt_date(wrd,t));
}

/** Get 14 bytes DOS date for 'msg' starting at position 'i'.
 * @note position after parsing in returned in 'i'.
 * @return 0 on success, -1 on failure.
*/
int32_t tms_get_dosdate(uint8_t *msg, int32_t *i, time_t *t) {
 
  int32_t j;         /**< general index */
  int32_t wrd[7];    /**< DOS date/time format */

  for (j=0; j<7; j++) {
    wrd[j]=tms_get_int(msg,i,2); 
  }
  /* convert integer array 'wrd' to time 't'. */
  return(tms_cvt_date(wrd,t));
}

/** Put time_t 't' as 16 bytes TMS date into 'msg' starting at position 'i'.
 * @note position after parsing in returned in 'i'.
 * @return 0 always.
*/
int32_t tms_put_date(time_t *t, uint8_t *msg, int32_t *i) {
 
  int32_t j;         /**< general index */
  int32_t wrd[8];    /**< TMS date format */
  struct tm cal;     /**< broken calendar time */

  if (*t==(time_t)0) {
    /* all zero for t-zero */
    for (j=0; j<8; j++) {
      wrd[j]=0;
    }
  } else {
    cal=*localtime(t);
    /* year since 1900 */
    wrd[0]=(cal.tm_year+1900)/100;
    wrd[1]=(cal.tm_year+1900)%100;
    /* months since January [0,11] */
    wrd[2]=cal.tm_mon+1;
    /* day of the month [1,31] */
    wrd[3]=cal.tm_mday;
    /* day of the week [0,6] ?? sunday = ? */
    wrd[4]=cal.tm_wday; /** !!! checken */
     /* hours since midnight [0,23] */
    wrd[5]=cal.tm_hour;
    /* minutes since the hour [0,59] */
    wrd[6]=cal.tm_min;
    /* seconds since the minute [0,59] */
    wrd[7]=cal.tm_sec;
  }
  /* put 16 bytes */
  for (j=0; j<8; j++) {
    tms_put_int(wrd[j],msg,i,2); 
  }
  if (tms_vb&0x01) {
    fprintf(stderr," %02d%02d-%02d-%02d %02d:%02d:%02d\n",
        wrd[0],wrd[1],wrd[2],wrd[3],wrd[5],wrd[6],wrd[7]);
  }
  return(0);
}

/** Convert buffer 'msg' starting at position 'i' into tms_config_t 'cfg'
 * @note new position byte will be return in 'i'
 * @return number of bytes parsed
 */
int32_t tms_get_cfg(uint8_t *msg, int32_t *i, tms_config_t *cfg) {

  int32_t i0=*i;    /**< start index */
  int32_t j;        /**< general index */

  cfg->version      =(int16_t) tms_get_int(msg,i,2); /**< PC Card protocol version number 0x0314 */
  cfg->hdrSize      =(int16_t) tms_get_int(msg,i,2); /**< size of measurement header 0x0200 */
  cfg->fileType     =(int16_t) tms_get_int(msg,i,2); /**< File Type (0: .ini 1: .smp 2:evt) */
  (*i)+=2; /**< skip 2 reserved bytes */
  cfg->cfgSize      =tms_get_int(msg,i,4); /**< size of config.ini  0x400         */
  (*i)+=4; /**< skip 4 reserved bytes */
  cfg->sampleRate   =(int16_t) tms_get_int(msg,i,2); /**< sample frequency [Hz] */
  cfg->nrOfChannels =(int16_t) tms_get_int(msg,i,2); /**< number of channels */
  cfg->startCtl     =tms_get_int(msg,i,4); /**< start control       */
  cfg->endCtl       =tms_get_int(msg,i,4); /**< end control       */
  cfg->cardStatus   =(int16_t) tms_get_int(msg,i,2); /**< card status */
  cfg->initId       =tms_get_int(msg,i,4); /**< Initialization Identifier */
  cfg->sampleRateDiv=(int16_t) tms_get_int(msg,i,2); /**< Sample Rate Divider */
  (*i)+=2; /**< skip 2 reserved bytes */
  for (j=0; j<64; j++) {
    cfg->storageType[j].shift=(int8_t)tms_get_int(msg,i,1); /**< shift */
    cfg->storageType[j].delta=(int8_t)tms_get_int(msg,i,1); /**< delta */
    cfg->storageType[j].deci =(int8_t)tms_get_int(msg,i,1); /**< decimation */
    cfg->storageType[j].ref  =(int8_t)tms_get_int(msg,i,1); /**< ref */
    cfg->storageType[j].period=0;                           /**< sample period */
    cfg->storageType[j].overflow=(0xFFFFFF80)<<(8*(cfg->storageType[j].delta-1)); /**< overflow */
  }
  for (j=0; j<12; j++) {
    cfg->fileName[j]=(uint8_t) tms_get_int(msg,i,1); /**< Measurement file name */
  }
  /**< alarm time */
  tms_get_date(msg,i,&cfg->alarmTime);
  (*i)+=2; /**< skip 2 or 4 reserved bytes !!! check it */
  for (j=0; j<sizeof(cfg->info); j++) {
    cfg->info[j]=(uint8_t) tms_get_int(msg,i,1);
  }

  /* find minimum decimation */
  cfg->mindecimation=255;
  for (j=0; j<cfg->nrOfChannels; j++) {
    if ((cfg->storageType[j].delta>0) && (cfg->storageType[j].deci<cfg->mindecimation)) {
      cfg->mindecimation=cfg->storageType[j].deci;
    }
  }
  /* calculate channel period */ 
  for (j=0; j<cfg->nrOfChannels; j++) {
    if (cfg->storageType[j].delta>0) {
      cfg->storageType[j].period=(cfg->storageType[j].deci+1)/(cfg->mindecimation+1);
    }
  }

  return((*i)-i0);
}

/** Put tms_config_t 'cfg' into buffer 'msg' starting at position 'i'
 * @note new position byte will be return in 'i'
 * @return number of bytes put
 */
int32_t tms_put_cfg(uint8_t *msg, int32_t *i, tms_config_t *cfg) {

  int32_t i0=*i;    /**< start index */
  int32_t j;        /**< general index */

  tms_put_int(cfg->version,msg,i,2);  /**< PC Card protocol version number 0x0314 */
  tms_put_int(cfg->hdrSize,msg,i,2);  /**< size of measurement header 0x0200 */
  tms_put_int(cfg->fileType,msg,i,2); /**< File Type (0: .ini 1: .smp 2:evt) */
  tms_put_int(0xFFFF,msg,i,2);        /**< 2 reserved bytes */
  tms_put_int(cfg->cfgSize,msg,i,4);  /**< size of config.ini  0x400         */
  tms_put_int(0xFFFFFFFF,msg,i,4);    /**< 4 reserved bytes */
  tms_put_int(cfg->sampleRate   ,msg,i,2); /**< sample frequency [Hz] */
  tms_put_int(cfg->nrOfChannels ,msg,i,2); /**< number of channels */
  tms_put_int(cfg->startCtl     ,msg,i,4); /**< start control       */
  tms_put_int(cfg->endCtl       ,msg,i,4); /**< end control       */
  tms_put_int(cfg->cardStatus   ,msg,i,2); /**< card status */
  tms_put_int(cfg->initId       ,msg,i,4); /**< Initialisation Identifier */
  tms_put_int(cfg->sampleRateDiv,msg,i,2); /**< Sample Rate Divider */
  tms_put_int(0x0000,msg,i,2);        /**< 2 reserved bytes */
  for (j=0; j<64; j++) {
    tms_put_int(cfg->storageType[j].shift,msg,i,1); /**< shift */
    tms_put_int(cfg->storageType[j].delta,msg,i,1); /**< delta */
    tms_put_int(cfg->storageType[j].deci ,msg,i,1); /**< decimation */
    tms_put_int(cfg->storageType[j].ref  ,msg,i,1); /**< ref */
  }
  for (j=0; j<12; j++) {
    tms_put_int(cfg->fileName[j],msg,i,1); /**< Measurement file name */
  }
  tms_put_date(&cfg->alarmTime,msg,i); /**< alarm time */
  tms_put_int(0xFFFFFFFF,msg,i,2);     /**< 2 or 4 reserved bytes. check it!!! */
  /* put info part */
  j=0;
  while ((j<sizeof(cfg->info)) && (*i<TMSCFGSIZE)) {
    tms_put_int(cfg->info[j],msg,i,1); j++;
  }
  /* fprintf(stderr,"tms_put_cfg: i %d j %d\n",*i,j); */
  return((*i)-i0);
}

/** Print tms_config_t 'cfg' to file 'fp'
 * @param prt_info !=0 -> print measurement/patient info
 * @return number of characters printed.
 */
int32_t tms_prt_cfg(FILE *fp, tms_config_t *cfg, int32_t prt_info) {

  int32_t nc=0;        /**< printed characters */
  int32_t i;           /**< index */
  char    atime[MNCN]; /**< alarm time */

  nc+=fprintf(fp,"v 0x%04X ; Version\n",cfg->version);
  nc+=fprintf(fp,"h 0x%04X ; hdrSize \n",cfg->hdrSize);
  nc+=fprintf(fp,"c 0x%04X ; cardStatus\n",cfg->cardStatus   ); 
  nc+=fprintf(fp,"t 0x%04x ; ",cfg->fileType);
  switch (cfg->fileType) {
    case 0:
      nc+=fprintf(fp,"ini");
      break;
    case 1:
      nc+=fprintf(fp,"smp");
      break;
    case 2:
      nc+=fprintf(fp,"evt");
      break;
    default:
      nc+=fprintf(fp,"unknown?");
      break;
  }    
  nc+=fprintf(fp," fileType\n");
  nc+=fprintf(fp,"g 0x%08X ; cfgSize\n",cfg->cfgSize);
  nc+=fprintf(fp,"r   %8d ; sample Rate [Hz]\n",cfg->sampleRate   );
  nc+=fprintf(fp,"n   %8d ; nr of Channels\n",cfg->nrOfChannels );

  nc+=fprintf(fp,"b 0x%08X ; startCtl:",cfg->startCtl);
  if (cfg->startCtl&0x01) {
    nc+=fprintf(fp," RTC_SET");
  }
  if (cfg->startCtl&0x02) {
    nc+=fprintf(fp," RECORD_AUTO_START");
  }
  if (cfg->startCtl&0x04) {
    nc+=fprintf(fp," BUTTON_ENABLE");
  }
  nc+=fprintf(fp,"\n");

  nc+=fprintf(fp,"e 0x%08X ; endCtl\n",cfg->endCtl       ); 
  nc+=fprintf(fp,"i 0x%08X ; initId\n",cfg->initId       ); 
  nc+=fprintf(fp,"d   %8d ; sample Rate Divider\n",cfg->sampleRateDiv); 
  strftime(atime,MNCN,"%Y-%m-%d %H:%M:%S",localtime(&cfg->alarmTime));
  nc+=fprintf(fp,"a   %8d ; alarm time %s\n",(int32_t)cfg->alarmTime,atime);
  nc+=fprintf(fp,"f %12s ; file name\n",cfg->fileName);
  nc+=fprintf(fp,"# nr refer decim delta shift ; ch period\n");
  for (i=0; i<64; i++) {
    if (cfg->storageType[i].delta!=0) {
      nc+=fprintf(fp,"s %2d %5d %5d %5d %5d ;  %1c %6d\n",i,
          cfg->storageType[i].ref,
          cfg->storageType[i].deci,
          cfg->storageType[i].delta,
          cfg->storageType[i].shift,
          'A'+i,
          cfg->storageType[i].period);
    }
  }
  if (prt_info) {
    for (i=0; i<sizeof(cfg->info); i++) {
      if (i%16==0) { 
        if (i>0) {
          nc+=fprintf(fp,"\n");
        } 
        nc+=fprintf(fp,"o 0x%04X",i);
      }
      nc+=fprintf(fp," 0x%02X",cfg->info[i]);
    }
    nc+=fprintf(fp,"\n");
  }
  nc+=fprintf(fp,"q  ; end of configuration\n");
  return(nc);
}

/** Read tms_config_t 'cfg' from file 'fp'
 * @return number of characters printed.
 */
int32_t tms_rd_cfg(FILE *fp, tms_config_t *cfg) {

  int32_t lc=0;                /**< line counter */
  char line[2*MNCN];           /**< temp line buffer */
  char **endptr=NULL;          /**< end pointer of integer parsing */
  char *token;                 /**< token on the input line */
  int32_t nr;
  int32_t ref,decim,delta,shift;  /**< nr, ref, decim and delta storage */
  int32_t go_on=1; 
  
  /* all zero as default */
  memset(cfg,0,sizeof(tms_config_t));
  /* default no channel reference */
  for (nr=0; nr<64; nr++) {
    cfg->storageType[nr].ref=-1;
  }
 
  while (!feof(fp) && (go_on==1)) {
    /* clear line */
    line[0]='\0';
    /* read one line with hist data */
    if (fgets(line,sizeof(line)-1,fp)==NULL) { continue; } lc++;
    /* skip comment and data lines */
    if ((line[0]=='#') || (line[0]==' ')) { continue; }
    switch (line[0]) {
      case 'v': 
        cfg->version=(int16_t) strtol(&line[2],endptr,0);
        break;
      case 'h': 
        cfg->hdrSize=(int16_t) strtol(&line[2],endptr,0);
        break;
      case 't': 
        cfg->fileType=(int16_t) strtol(&line[2],endptr,0);
        break;
      case 'g': 
        cfg->cfgSize=(int32_t) strtol(&line[2],endptr,0);
        break;
      case 'r': 
        cfg->sampleRate=(int16_t) strtol(&line[2],endptr,0);
        break;
      case 'n': 
        cfg->nrOfChannels=(int16_t) strtol(&line[2],endptr,0);
        break;
      case 'b': 
        cfg->startCtl=(int32_t) strtol(&line[2],endptr,0);
        break;
      case 'e': 
        cfg->endCtl=(int32_t) strtol(&line[2],endptr,0);
        break;
      case 'i': 
        cfg->initId=(int32_t) strtol(&line[2],endptr,0);
        break;
      case 'c': 
        cfg->cardStatus=(int16_t) strtol(&line[2],endptr,0);
        break;
      case 'd': 
        cfg->sampleRateDiv=(int16_t) strtol(&line[2],endptr,0);
        break;
      case 'a': 
        cfg->alarmTime=(int16_t) strtol(&line[2],endptr,0);
        break;
      case 'f':
        sscanf(line,"f %s ;",cfg->fileName); endptr=NULL;
        break;
      case 's': 
        sscanf(line,"s %d %d %d %d %d ;",&nr,&ref,&decim,&delta,&shift); 
        endptr=NULL;
         if ((nr>=0) && (nr<64)) {
          cfg->storageType[nr].ref  =(int8_t) ref;
          cfg->storageType[nr].deci =(int8_t) decim;
          cfg->storageType[nr].delta=(int8_t) delta;
          cfg->storageType[nr].shift=(int8_t) shift;
        }
        break;
      case 'o':
        /* start parsing direct after character 'o' */
        token=strtok(&line[1]," \n");
        if (token!=NULL) {
          /* get start address */
          nr=strtol(token,endptr,0);
          /* parse data bytes */
          while ((token=strtok(NULL," \n"))!=NULL) {
            if ((nr>=0) && (nr<sizeof(cfg->info))) 
            cfg->info[nr]=(uint8_t) strtol(token,endptr,0);
            nr++;
          }
        }
        break; 
      case 'q':
        go_on=0;
        break; 
      default: 
        break;
    } 
    if (endptr!=NULL) {
      fprintf(stderr,"# Warning: line %d has an configuration error!!!\n",lc);
    }       
  }
  return(lc);
}

/** Convert buffer 'msg' starting at position 'i' into tms_measurement_hdr_t 'hdr'
 * @note new position byte will be return in 'i'
 * @return number of bytes parsed
 */
int32_t tms_get_measurement_hdr(uint8_t *msg, int32_t *i, tms_measurement_hdr_t *hdr) {

  int32_t i0=*i;   /**< start byte index */
  int32_t err=0;   /**< error status of tms_get_date */

  (*i)+=4; /**< skip 4 reserved bytes */
  hdr->nsamples=tms_get_int(msg,i,4); /**< number of samples in this recording */
  err=tms_get_date(msg,i,&hdr->startTime);
  if (err!=0) {
    fprintf(stderr,"# Warning: start time incorrect!!!\n");
  }
  err=tms_get_date(msg,i,&hdr->endTime);
  if (err!=0) {
    fprintf(stderr,"# Warning: end time incorrect, unexpected end of recording!!!\n");
  }
  hdr->frontendSerialNr=tms_get_int(msg,i,4); /**< frontendSerial Number */
  hdr->frontendHWNr    =(int16_t) tms_get_int(msg,i,2); /**< frontend Hardware version Number */
  hdr->frontendSWNr    =(int16_t) tms_get_int(msg,i,2); /**< frontend Software version Number */
  return((*i)-i0);
}

/** Print tms_config_t 'cfg' to file 'fp'
 * @param prt_info 0x01 print measurement/patient info
 * @return number of characters printed.
 */
int32_t tms_prt_measurement_hdr(FILE *fp, tms_measurement_hdr_t *hdr) {

  int32_t nc=0;        /**< printed characters */
  char    stime[MNCN]; /**< start time */
  char    etime[MNCN]; /**< end time */

  strftime(stime,MNCN,"%Y-%m-%d %H:%M:%S",localtime(&hdr->startTime));
  strftime(etime,MNCN,"%Y-%m-%d %H:%M:%S",localtime(&hdr->endTime));
  nc+=fprintf(fp,"# time start %s end %s\n",stime,etime);
  nc+=fprintf(fp,"# Frontend SerialNr %d HWNr 0x%04X SWNr 0x%04X\n",
      hdr->frontendSerialNr,
      hdr->frontendHWNr,hdr->frontendSWNr); 
  nc+=fprintf(fp,"# nsamples %9d\n",hdr->nsamples); 

  return(nc);
}

/** Convert buffer 'msg' starting at position 'i' into tms_hdr32_t 'hdr'
 * @note new position byte will be return in 'i'
 * @return number of bytes parsed
 */
int32_t tms_get_hdr32(uint8_t *msg, int32_t *i, tms_hdr32_t *hdr) {

  int32_t i0=*i;    /**< start index */
  int32_t j;        /**< general index */
  char    ch;       /**< temp character */
  
  for (j=0; j<TMS32IDENTIFIERSIZE; j++) {
    ch = (char) tms_get_int(msg,i,1);
    hdr->identifier[j] =((ch>'\0' && ch<' ') ? '.' : ch); /**< identifier */
  }
  hdr->version         =tms_get_int(msg,i,2);    /**< version number 0x00CC */
  /* measurement name */
  tms_get_pascal_string(msg,i,hdr->measurement,TMS32MEASUREMENTNAMESIZE);
  hdr->sampleRate      =tms_get_int(msg,i,2); /**< Sample Frequency [Hz] */
  hdr->storageRate     =tms_get_int(msg,i,2); /**< Storage rate: lower or equal to 'SampleFreq' */
  hdr->storageType     =tms_get_int(msg,i,1); /**< Storage type: 0=Hz 1=Hz^-1 */
  hdr->nrOfSignals     =tms_get_int(msg,i,2); /**< Number of signals stored in sample file */
  hdr->totalPeriods    =tms_get_int(msg,i,4); /**< Total number of sample periods */
  (*i)+=4; /**< skip 4 reserved bytes */      /**< start time */
  tms_get_dosdate(msg,i,&hdr->startTime);        
  hdr->nrOfBlocks      =tms_get_int(msg,i,4); /**< number of sample blocks */
  hdr->periodsPerBlock =tms_get_int(msg,i,2); /**< Sample periods per Sample Data Block (multiple of 16) */
  hdr->blockSize       =tms_get_int(msg,i,2); /**< Size of sample data in one Block [byte] header not include */
  hdr->deltaCompression=tms_get_int(msg,i,2); /**< Delta compression flag (not used: must be zero) */
  return((*i)-i0);
}

/** Print tms_hdr32_t 'hdr' to file 'fp'
 * @return number of characters printed.
 */
int32_t tms_prt_hdr32(FILE *fp, tms_hdr32_t *hdr) {

  int32_t nc=0;        /**< printed characters */
  char    atime[MNCN]; /**< start time */

  nc+=fprintf(fp,"f %s ; File identifier\n",hdr->identifier);
  nc+=fprintf(fp,"v 0x%04X ; version \n",hdr->version);
  nc+=fprintf(fp,"m %s ; measurement name\n",hdr->measurement);
  nc+=fprintf(fp,"s   %8d ; Sample rate\n",hdr->sampleRate);
  nc+=fprintf(fp,"r   %8d ; storage Rate\n",hdr->storageRate);
  nc+=fprintf(fp,"t   %8d ; storage Type\n",hdr->storageType);
  nc+=fprintf(fp,"n   %8d ; nr of signals\n",hdr->nrOfSignals);
  nc+=fprintf(fp,"p   %8d ; total Periods\n",hdr->totalPeriods);
  strftime(atime,MNCN,"%Y-%m-%d %H:%M:%S",localtime(&hdr->startTime));
  nc+=fprintf(fp,"a   %8d ; stArt time %s\n",(int32_t)hdr->startTime,atime);
  nc+=fprintf(fp,"b   %8d ; nr of Blocks\n",hdr->nrOfBlocks);
  nc+=fprintf(fp,"d   %8d ; perioDs per block\n",hdr->periodsPerBlock);
  nc+=fprintf(fp,"z   %8d ; blocksiZe\n",hdr->blockSize);
  nc+=fprintf(fp,"c   %8d ; delta Compression\n",hdr->deltaCompression);
  return(nc);
}

/** Convert buffer 'msg' starting at position 'i' into tms_signal_desc32_t 'desc'
 * @note new position byte will be return in 'i'
 * @return number of bytes parsed
 */
int32_t tms_get_desc32(uint8_t *msg, int32_t *i, tms_signal_desc32_t *desc) {

  int32_t i0=*i;    /**< start index */
  
  tms_get_pascal_string(msg,i,desc->signalName,TMS32SIGNALNAMESIZE); /**< signalName */
  (*i)+=4; /**< skip 4 reserved bytes */      
  tms_get_pascal_string(msg,i,desc->unitName,TMS32UNITNAMESIZE); /**< unitName */
  desc->unitLow     =tms_get_float32(msg,i); /**< Unit Low */
  desc->unitHigh    =tms_get_float32(msg,i); /**< Unit High */
  desc->ADCLow      =tms_get_float32(msg,i); /**< ADC Low */
  desc->ADCHigh     =tms_get_float32(msg,i); /**< ADC High */
  desc->index       =tms_get_int(msg,i,2);   /**< index in signal list */
  desc->cacheOffset =tms_get_int(msg,i,2);   /**< cache offset */
  desc->sampleSize  =2;                      /**< default sample size [byte] */
  return((*i)-i0);
}

/** Print tms_desc32_t 'hdr' to file 'fp'
 * @return number of characters printed.
 */
int32_t tms_prt_desc32(FILE *fp, tms_signal_desc32_t *desc, int32_t hdr) {

  int32_t nc=0;        /**< printed characters */

  if (hdr) {
    nc+=fprintf(fp,"# %15s %6s %9s %9s %9s %9s %3s %3s %4s\n",
         "signal","unit","unitLo","unitHi","ADCLo","ADCHi","idx","cof","size");
    
  }
  nc+=fprintf(fp,"u %15s %6s %9.6f %9.6f %9.6f %9.6f %3d %3d %4d\n",
        desc->signalName,desc->unitName,desc->unitLow,desc->unitHigh,
        desc->ADCLow,desc->ADCHigh,desc->index,desc->cacheOffset,
        desc->sampleSize);
  return(nc);
}

/** Convert buffer 'msg' starting at position 'i' into tms_block_desc32_t 'blk'
 * @note new position byte will be return in 'i'
 * @return number of bytes parsed
 */
int32_t tms_get_blk32(uint8_t *msg, int32_t *i, tms_block_desc32_t *blk) {

  int32_t i0=*i;    /**< start index */
  
  blk->idx=tms_get_int(msg,i,4);      /**< Sample period index */
  (*i)+=4; /**< skip 4 reserved bytes */      
  tms_get_dosdate(msg,i,&blk->then);  /**< time of block creation */        
  return((*i)-i0);
}

/** Print tms_block_desc32_t 'blk' to file 'fp'
 * @return number of characters printed.
 */
int32_t tms_prt_blk32(FILE *fp, tms_block_desc32_t *blk) {

  int32_t nc=0;        /**< printed characters */
  char    atime[MNCN]; /**< time of block creation */

  nc+=fprintf(fp,"# idx %d",blk->idx);
  strftime(atime,MNCN,"%Y-%m-%d %H:%M:%S",localtime(&blk->then));
  nc+=fprintf(fp," time_t %8d time %s\n",(int32_t)blk->then,atime);
  
  return(nc);
}

/*********************************************************************/
/* Functions for reading and setting up the bluetooth connection     */
/*********************************************************************/

/* Convert string 'a' with channel labels 'AB...I' into channel selection
 * @return channel selection integer with bit 'i' set to corresponding channel 'A'+i
*/
int32_t tms_chn_sel(char *a) {

  int32_t i,chn=0;
  int32_t n=(int32_t) strlen(a);

  for (i=0; i<n; i++) {
    switch (toupper(a[i])) {
      case 'A': chn|=0x0001; break;
      case 'B': chn|=0x0002; break;
      case 'C': chn|=0x0004; break;
      case 'D': chn|=0x0008; break;
      case 'E': chn|=0x0010; break;
      case 'F': chn|=0x0020; break;
      case 'G': chn|=0x0040; break;
      case 'H': chn|=0x0080; break;
      case 'I': chn|=0x0100; break;
      case 'J': chn|=0x0200; break;
      case 'K': chn|=0x0400; break; 
      case 'L': chn|=0x0800; break; 
      case 'M': chn|=0x1000; break; 
      default: 
        break;
    }
  }
  //fprintf(stderr,"# tms_chn_sel: %s 0x%04X\n",a,chn);
  return(chn);
}

/** General check of TMS message 'msg' of 'n' bytes.
 * @return 0 on correct checksum, 1 on failure.
*/
int32_t tms_chk_msg(uint8_t *msg, int32_t n) {

  int32_t i;       /**< general index */
  int32_t sync;    /**< TMS block sync */
  int32_t size;    /**< TMS block size */
  int32_t calsum;  /**< calculate checksum of TMS block */
  
  if (n<4){
    return (-1);
  }
  /* check sync */
  i=0;
  sync=tms_get_int(msg,&i,2);
  if (sync!=TMSBLOCKSYNC) {
    fprintf(stderr,"# Warning: found sync %04X != %04X\n",
      sync,TMSBLOCKSYNC);
    return(-1);
  }
  /* size check */
  size=tms_msg_size(msg,n,&i);
  if (n!=(2*size+i+2)) {
    fprintf(stderr,"# Warning: found size %d != expected size %d\n",2*size+i+2,n);
    return(1);
   // exit(-1); /* te streng */
  }  
  /* check checksum and get type */
  calsum=tms_cal_chksum(msg,n);
  if (calsum!=0x0000) {
    fprintf(stderr,"# Warning: checksum 0x%04X != 0x0000\n",calsum);
    return(1);
  } else {
    return(0);
  }
}

/** Put checksum at end of buffer 'msg' of 'n' bytes.
 * @return total size of 'msg' including checksum.
*/
int16_t tms_put_chksum(uint8_t *msg, int32_t n) {

  int32_t i;                   /**< general index */
  int16_t sum=0x0000;          /**< checksum */

  if (n%2==1) {
    fprintf(stderr,"Warning: tms_put_chksum: odd packet length %d\n",n);
  }
  /* calculate checksum */  
  for (i=0; i<(n/2); i++){
    sum += (msg[2*i+1] << 8) + msg[2*i];
  }
  /* checksum should add up to 0x0000 */
  sum=-sum;
  /* put it */
  i=n; tms_put_int(sum,msg,&i,2);
  /* return total size of 'msg' including checksum */
  return((uint16_t) i);
}

/** Read at max 'n' bytes of TMS message 'msg' for 
 *   bluetooth device descriptor 'fd'.
 * @return number of bytes read.
*/
int32_t tms_rcv_msg(int fd, uint8_t *msg, int32_t n) {
  
  int32_t i=0;         /**< byte index */
  int32_t br=0;        /**< bytes read */
  int32_t tbr=0;       /**< total bytes read */
  int32_t sync=0x0000; /**< sync block */
  int32_t rtc=0;       /**< retry counter */
  int32_t size=0;      /**< payload size [uint16_t] */
  
  /* clear receive buffer */
  memset(msg,0x00,n);

  /* wait (not too long) for sync block */
  br=0;
  while ((rtc<1000) && (sync!=TMSBLOCKSYNC)) {
    if (br>0) {
      msg[0]=msg[1];
    }
    br=BtReadBytes(fd,&msg[1],1);
    if (br>0) {
      tbr+=br;
      i=0; sync=tms_get_int(msg,&i,2);
    } else {
      rtc++;
      usleep(WAIT_FOR_NEXT_BYTE);
    }
  }
  if (rtc>=1000) {
    fprintf(stderr,"# Error: timeout on waiting for block sync\n");
    return(-1);    
  }
  /* read 2 byte description */
  while ((rtc<1000) && (i<4)) {
    br=BtReadBytes(fd,&msg[i],1); 
    if (br>0){
      i++; tbr+=br;
    } else {
      rtc++;
      usleep(WAIT_FOR_NEXT_BYTE);
    }
  }
  if (rtc>=1000) {
    fprintf(stderr,"# Error: timeout on waiting description\n");
    return(-2);    
  }
  size=msg[2]; 
  /* read rest of message */
  while ((rtc<1000) && (i<(2*size+6)) && (i<n)) {
    br=BtReadBytes(fd,&msg[i],(2*size+6-i)); 
    if (br>0){
      i+=br; tbr+=br;
    } else {
      rtc++;
      usleep(WAIT_FOR_NEXT_BYTE);
    }
  }
  if (rtc>=1000) {
    fprintf(stderr,"# Error: timeout on rest of message\n");
    return(-3);    
  }
  if (i>n) {
    fprintf(stderr,"# Warning: message buffer size %d too small %d !\n",n,i);
  }
  if (tms_vb&0x01) {
    /* log response */
    tms_write_log_msg(msg,tbr,"receive message");
  }
  
  return(tbr);
}
  

/** Convert buffer 'msg' of 'n' bytes into tms_acknowledge_t 'ack'.
 * @return >0 on failure and 0 on success 
*/
int32_t tms_get_ack(uint8_t *msg, int32_t n, tms_acknowledge_t *ack) {

  int32_t i;     /**< general index */
  int32_t type;  /**< message type */
  int32_t size;  /**< payload size */
 
  /* get message type */ 
  type=tms_get_type(msg,n);
  if (type!=TMSACKNOWLEDGE) {
    fprintf(stderr,"# Warning: type %02X != %02X\n",
              type,TMSFRONTENDINFO);
    return(-1);
  }
  /* get payload size and payload pointer 'i' */
  size=tms_msg_size(msg,n,&i);
  ack->descriptor=(uint16_t) tms_get_int(msg,&i,2);
  ack->errorcode=(uint16_t) tms_get_int(msg,&i,2);
  /* number of found Frontend system info structs */  
  return(ack->errorcode);
}


/** Print tms_acknowledge_t 'ack' to file 'fp'.
 *  @return number of printed characters.
*/
int32_t tms_prt_ack(FILE *fp, tms_acknowledge_t *ack) {
 
  int32_t nc=0;
  
  if (fp==NULL) {
    return(nc);
  }
  nc+=fprintf(fp,"# Ack: desc 0x%04X err 0x%04X",
         ack->descriptor,ack->errorcode);
  switch (ack->errorcode) {
    case 0x01: 
      nc+=fprintf(fp," unknown or not implemented blocktype"); break;
    case 0x02: 
      nc+=fprintf(fp," CRC error in received block"); break;
    case 0x03: 
      nc+=fprintf(fp," error in command data (can't do that)"); break;
    case 0x04: 
      nc+=fprintf(fp," wrong blocksize (too large)"); break;
    /** 0x0010 - 0xFFFF are reserved for user errors */
    case 0x11: 
      nc+=fprintf(fp," No external power supplied"); break;
    case 0x12: 
      nc+=fprintf(fp," Not possible because the Front is recording"); break;
    case 0x13: 
      nc+=fprintf(fp," Storage medium is busy"); break;
    case 0x14: 
      nc+=fprintf(fp," Flash memory not present"); break;
    case 0x15: 
      nc+=fprintf(fp," nr of words to read from flash memory out of range"); break;
    case 0x16: 
      nc+=fprintf(fp," flash memory is write protected"); break;
    case 0x17: 
      nc+=fprintf(fp," incorrect value for initial inflation pressure"); break;
    case 0x18: 
      nc+=fprintf(fp," wrong size or values in BP cycle list"); break;
    case 0x19: 
      nc+=fprintf(fp," sample frequency divider out of range (<0, >max)"); break;
    case 0x1A: 
      nc+=fprintf(fp," wrong nr of user channels (<=0, >maxUSRchan)"); break;
    case 0x1B:
      nc+=fprintf(fp," adress flash memory out of range"); break;
    case 0x1C:
      nc+=fprintf(fp," Erasing not possible because battery low"); break;
    case 0x1D:
      nc+=fprintf(fp," 0x1D Unknown error of Mark II"); break;
    default:
     /** 0x00 - no error, positive acknowledge */
     break;
  }
  nc+=fprintf(fp,"\n");
  return(nc);
}


/** Send frontend Info request to 'fd'.
 *  @return bytes send.
*/
int32_t tms_snd_FrontendInfoReq(int32_t fd) {

  int32_t i;       /**< general index */
  uint8_t msg[6];  /**< message buffer */
  int32_t bw;      /**< byte written */
  
  i=0;
  /* construct frontendinfo req message */ 
  /* block sync */ 
  tms_put_int(TMSBLOCKSYNC,msg,&i,2);
  /* length 0, no data */
  msg[2] = 0x00;
  /* FrontendInfoReq type */
  msg[3] = TMSFRONTENDINFOREQ;
  /* add checksum */
  bw=tms_put_chksum(msg,4);

  if (tms_vb&0x01) {
    tms_write_log_msg(msg,bw,"send frontendinfo request");
  }
  /* send request */
  bw = BtWriteBytes(fd, msg, bw);
  /* return number of byte actually written */ 
  return(bw);
}


/** Send keepalive request to 'fd'.
 *  @return bytes send.
*/
int32_t tms_snd_keepalive(int32_t fd) {

  int32_t i;       /**< general index */
  uint8_t msg[6];  /**< message buffer */
  int32_t bw;      /**< byte written */
  
  i=0;
  /* construct frontendinfo req message */ 
  /* block sync */ 
  tms_put_int(TMSBLOCKSYNC,msg,&i,2);
  /* length 0, no data */
  msg[2] = 0x00;
  /* FrontendInfoReq type */
  msg[3] = TMSKEEPALIVEREQ;
  /* add checksum */
  bw=tms_put_chksum(msg,4);

  if (tms_vb&0x01) {
    tms_write_log_msg(msg,bw,"send keepalive");
  }
  /* send request */
  bw=BtWriteBytes(fd,msg,bw);
  /* return number of byte actually written */ 
  return(bw);
}

/** Convert buffer 'msg' of 'n' bytes into frontendinfo_t 'fei'
 * @note 'b' needs size of TMSFRONTENDINFOSIZE
 * @return -1 on failure and on succes number of frontendinfo structs
*/
int32_t tms_get_frontendinfo(uint8_t *msg, int32_t n, tms_frontendinfo_t *fei) {

  int32_t i;     /**< general index */
  int32_t type;  /**< message type */
  int32_t size;  /**< payload size */
  int32_t nfei;  /**< number of frontendinfo_t structs */
 
  /* get message type */ 
  type=tms_get_type(msg,n);
  if (type!=TMSFRONTENDINFO) {
    fprintf(stderr,"# Warning: tms_get_frontendinfo type %02X != %02X\n",
              type,TMSFRONTENDINFO);
    return(-3);
  }
  /* get payload size and start pointer */
  size=tms_msg_size(msg,n,&i);
  /* number of available frontendinfo_t structs */
  nfei=(2*size)/TMSFRONTENDINFOSIZE;
  if (nfei>1) {
    fprintf(stderr,"# Error: tms_get_frontendinfo found %d struct > 1\n",nfei);
  }
  fei->nrofuserchannels=(uint16_t) tms_get_int(msg,&i,2);
  fei->currentsampleratesetting=(uint16_t) tms_get_int(msg,&i,2);
  fei->mode=(uint16_t) tms_get_int(msg,&i,2);
  fei->maxRS232=(uint16_t) tms_get_int(msg,&i,2);
  fei->serialnumber=tms_get_int(msg,&i,4);
  fei->nrEXG=(uint16_t) tms_get_int(msg,&i,2);
  fei->nrAUX=(uint16_t) tms_get_int(msg,&i,2);
  fei->hwversion=(uint16_t) tms_get_int(msg,&i,2);
  fei->swversion=(uint16_t) tms_get_int(msg,&i,2);
  fei->cmdbufsize=(uint16_t) tms_get_int(msg,&i,2);
  fei->sendbufsize=(uint16_t) tms_get_int(msg,&i,2);
  fei->nrofswchannels=(uint16_t) tms_get_int(msg,&i,2);
  fei->basesamplerate=(uint16_t) tms_get_int(msg,&i,2);
  fei->power=(uint16_t) tms_get_int(msg,&i,2);
  fei->hardwarecheck=(uint16_t) tms_get_int(msg,&i,2);
  /* number of found Frontend system info structs */  
  return(nfei);
}


/** Write frontendinfo_t 'fei' into socket 'fd'.
 * @return number of bytes written (<0: failure)
*/
int32_t tms_write_frontendinfo(int32_t fd, tms_frontendinfo_t *fei) {

  int32_t i;         /**< general index */
  uint8_t msg[0x40]; /**< message buffer */
  int32_t bw;        /**< byte written */
  
  i=0;
  /* construct frontendinfo req message */ 
  /* block sync */ 
  tms_put_int(TMSBLOCKSYNC,msg,&i,2);
  /* length */
  tms_put_int(TMSFRONTENDINFOSIZE/2,msg,&i,1);
  /* FrontendInfoReq type */
  tms_put_int(TMSFRONTENDINFO,msg,&i,1);

  /* readonly !!! */
  tms_put_int(fei->nrofuserchannels,msg,&i,2);

  /* writable*/
  tms_put_int(fei->currentsampleratesetting,msg,&i,2);
  tms_put_int(fei->mode,msg,&i,2);
 
  /* readonly !!! */
  tms_put_int(fei->maxRS232,msg,&i,2);
  tms_put_int(fei->serialnumber,msg,&i,4);
  tms_put_int(fei->nrEXG,msg,&i,2);
  tms_put_int(fei->nrAUX,msg,&i,2);
  tms_put_int(fei->hwversion,msg,&i,2);
  tms_put_int(fei->swversion,msg,&i,2);
  tms_put_int(fei->cmdbufsize,msg,&i,2);
  tms_put_int(fei->sendbufsize,msg,&i,2);
  tms_put_int(fei->nrofswchannels,msg,&i,2);
  tms_put_int(fei->basesamplerate,msg,&i,2);
  tms_put_int(fei->power,msg,&i,2);
  tms_put_int(fei->hardwarecheck,msg,&i,2);
  /* add checksum */
  bw=tms_put_chksum(msg,i);

  if (tms_vb&0x01) {
    tms_write_log_msg(msg,bw,"write frontendinfo");
  }
  /* send request */
  bw=BtWriteBytes(fd,msg,bw);

  /* return number of byte actually written */ 
  return(bw);
}

/** Print tms_frontendinfo_t 'fei' to file 'fp'.
 *  @return number of printed characters.
*/
int32_t tms_prt_frontendinfo(FILE *fp, tms_frontendinfo_t *fei, int nr, int hdr) {

  int32_t nc=0;  /**< number of printed characters */
  
  (void) nr;    // Avoid warning about unused parameter

  if (fp==NULL) {
    return(nc);
  }
  if (hdr) {
    nc+=fprintf(fp,"# TMSi frontend info\n");
    nc+=fprintf(fp,"# %3s %3s %2s %4s %9s %4s %4s %4s %4s %4s %4s %4s %4s %4s %4s\n",
         "uch","css","md","mxfs","serialnr","nEXG","nAUX","hwv","swv","cmds","snds","nc","bfs","pw","hwck");
  }
  nc+=fprintf(fp," %4d %3d %02X %4d %9d %4d %4d %04X %04X %4d %4d %4d %4d %04X %04X\n",
    fei->nrofuserchannels,
    fei->currentsampleratesetting,
    fei->mode,
    fei->maxRS232,
    fei->serialnumber,
    fei->nrEXG,
    fei->nrAUX,
    fei->hwversion,
    fei->swversion,
    fei->cmdbufsize,
    fei->sendbufsize,
    fei->nrofswchannels,
    fei->basesamplerate,
    fei->power,
    fei->hardwarecheck);
  return(nc);
}

#define TMSIDREADREQ (0x22) 
#define TMSIDDATA    (0x23) 

/** Send IDData request to file descriptor 'fd'
*/

int32_t tms_send_iddata_request(int32_t fd, int32_t adr, int32_t len)
{
  uint8_t req[10];    /**< id request message */
  int32_t i=0;
  int32_t bw;         /**< byte written */

  /* block sync */ 
  tms_put_int(TMSBLOCKSYNC,req,&i,2);
  /* length 2 */
  tms_put_int(0x02,req,&i,1);
  /* IDReadReq type */
  tms_put_int(TMSIDREADREQ,req,&i,1);
  /* start address */
  tms_put_int(adr,req,&i,2);
  /* maximum length */
  tms_put_int(len,req,&i,2);
  /* add checksum */
  bw=tms_put_chksum(req,i);

  if (tms_vb&0x01) {
    tms_write_log_msg(req,bw, "send IDData request");
  }
  /* send request */
  bw=BtWriteBytes(fd,req,bw);
  if(bw < 0)
  {
      perror("# Warning: tms_send_iddata_request write problem");
  }
  return bw;
}

/** Get IDData from device descriptor 'fd' into byte array 'msg'
 *   with maximum size 'n'.
 * @return bytes in 'msg'.
*/
int32_t tms_fetch_iddata(int32_t fd, uint8_t *msg, int32_t n) {

  int32_t i,j;        /**< general index */
  int16_t adr=0x0000; /**< start address of buffer ID data */
  int16_t len=0x80;   /**< amount of words requested */
  int32_t br=0;       /**< bytes read */
  int32_t tbw=0;      /**< total bytes written in 'msg' */
  uint8_t rcv[512];   /**<  buffer */
  int32_t type;       /**< received IDData type */
  int32_t size;       /**< received IDData size */
  int32_t tsize=0;    /**< total received IDData size */
  int32_t start=0;    /**< start address in receive ID Data packet */
  int32_t length=0;   /**< length in receive ID Data packet */
  int32_t rtc=0;      /**< retry counter */
  
  /* prepare response header */
  tbw=0;
  /* block sync */ 
  tms_put_int(TMSBLOCKSYNC,msg,&tbw,2);
  /* length 0xFF */
  tms_put_int(0xFF,msg,&tbw,1);
  /* IDData type */
  tms_put_int(TMSIDDATA,msg,&tbw,1);
  /* temp zero length, final will be put at the end */
  tms_put_int(0,msg,&tbw,4);
  
  /* start address and maximum length */
  adr=0x0000; 
  len=0x80;
  
  rtc=0;
  /* keep on requesting id data until all data is read */
  while ((rtc<10) && (len>0) && (tbw<n)) {
    rtc++;
    if (tms_send_iddata_request(fd,adr,len) < 0) {
      continue;
    }
    /* get response */
    br=tms_rcv_msg(fd,rcv,sizeof(rcv)); 
    
    /* check checksum and get type of response */
    type=tms_get_type(rcv,br);
    if (type!=TMSIDDATA) {
      return -1;
      //fprintf(stderr,"# Warning: tms_get_iddata: unexpected type 0x%02X\n",type);
    } else {
      /* get payload of 'rcv' */
      size=tms_msg_size(rcv,sizeof(rcv),&i);
      /* get start address */
      start=tms_get_int(rcv,&i,2);
      /* get length */
      length=tms_get_int(rcv,&i,2);
      /* copy response to final result */
      if (tbw+2*length>n) {
        fprintf(stderr,"# Error: tms_get_iddata: msg too small %d\n",tbw+2*length);
      } else { 
        for (j=0; j<2*length; j++) {
          msg[tbw+j]=rcv[i+j];
        }
        tbw+=2*length;
        tsize+=length; 
      }
      /* update address admin */
      adr+= (int16_t) length;
      /* if block ends with 0xFFFF, then this one was the last one */ 
      if ((rcv[2*size-2]==0xFF) && (rcv[2*size-1]==0xFF)) { len=0; }
    }
  }
  /* put final total size */
  i=4; tms_put_int(tsize,msg,&i,4);
  /* add checksum */
  tbw=tms_put_chksum(msg,tbw);
  
  /* return number of byte actualy written */ 
  return(tbw);
}

/** Convert buffer 'msg' of 'n' bytes into tms_type_desc_t 'td'
 * @return 0 on success, -1 on failure
*/
int32_t tms_get_type_desc(uint8_t *msg, int32_t n, int32_t start, tms_type_desc_t *td) {

  int32_t i=start;   /**< general index */

  td->Size=(uint16_t) tms_get_int(msg,&i,2);
  td->Type=(uint16_t) tms_get_int(msg,&i,2);
  td->SubType=(uint16_t) tms_get_int(msg,&i,2);
  td->Format=(uint16_t) tms_get_int(msg,&i,2);
  td->a=tms_get_float(msg,&i);
  td->b=tms_get_float(msg,&i);
  td->UnitId=(uint8_t) tms_get_int(msg,&i,1);
  td->Exp=(int8_t) tms_get_int(msg,&i,1);
  if (i<=n) { return(0); } else { return(-1); }
}


/** Get input device struct 'inpdev' at position 'start' of message 'msg' of 'n' bytes
 * @return always on success, -1 on failure
*/
int32_t tms_get_input_device(uint8_t *msg, int32_t n, int32_t start, tms_input_device_t *inpdev) {

  int32_t i=start,j;          /**< general index */
  int32_t idx;                /**< location */
  int32_t ChannelDescriptionSize;
  int32_t nb;                 /**< number of bits */
  int32_t tnb;                /**< total number of bits */
  
  inpdev->Size=(uint16_t) tms_get_int(msg,&i,2);
  inpdev->Totalsize=(uint16_t) tms_get_int(msg,&i,2);
  inpdev->SerialNumber=tms_get_int(msg,&i,4);
  inpdev->Id=(uint16_t) tms_get_int(msg,&i,2);
  idx=2*tms_get_int(msg,&i,2)+start;
  inpdev->DeviceDescription = tms_get_string(msg,n,idx);
  inpdev->NrOfChannels=(uint16_t) tms_get_int(msg,&i,2);
  ChannelDescriptionSize=tms_get_int(msg,&i,2);
  /* allocate space for all channel descriptions */
  inpdev->Channel=(tms_channel_desc_t *)calloc(inpdev->NrOfChannels,sizeof(tms_channel_desc_t));
  /* get pointer to first channel description */
  idx=2*tms_get_int(msg,&i,2)+start; 
  /* goto first channel descriptor */
  i=idx;
  /* get all channel descriptions */
  for (j=0; j<inpdev->NrOfChannels; j++) {
    idx=2*tms_get_int(msg,&i,2)+start;
    tms_get_type_desc(msg,n,idx,&inpdev->Channel[j].Type);
    idx=2*tms_get_int(msg,&i,2)+start;
    inpdev->Channel[j].ChannelDescription=tms_get_string(msg,n,idx);
    inpdev->Channel[j].GainCorrection    =tms_get_float(msg,&i);
    inpdev->Channel[j].OffsetCorrection  =tms_get_float(msg,&i);
  }
  /* count total number of bits needed */
  tnb=0;
  for (j=0; j<inpdev->NrOfChannels; j++) {
    nb=inpdev->Channel[j].Type.Format & 0xFF;
    if (nb%8!=0) {
      fprintf(stderr,"# Warning: tms_get_input_device: channel %d has %d bits\n",j,nb);
    } 
    tnb+=nb;
  }
  if (tnb%16!=0) {
    fprintf(stderr,"# Warning: tms_get_input_device: total bits count %d %% 16 !=0\n",tnb);
  }
  inpdev->DataPacketSize = (uint16_t) ((tnb+15)/16);

  if (i<=n) { return(0); } else { return(-1); }
}


/** Get input device struct 'inpdev' at position 'start' of message 'msg' of 'n' bytes
 * @return always on success, -1 on failure
 */
int32_t tms_get_iddata(uint8_t *msg, int32_t n, tms_input_device_t *inpdev) {

  int32_t type;    /**< message type */
  int32_t i=0;     /**< general index */
  int32_t size;    /**< payload size */

  /* get message type */ 
  type=tms_get_type(msg,n);
  if (type!=TMSIDDATA) {
    fprintf(stderr,"# Warning: type %02X != %02X\n",type,TMSIDDATA);
    return(-1);
  }
  size=tms_msg_size(msg,n,&i);
  return(tms_get_input_device(msg,n,i,inpdev));
}

/** Print tms_type_desc_t 'td' to file 'fp'.
 *  @return number of printed characters.
 */
int32_t tms_prt_type_desc(FILE *fp, tms_type_desc_t *td, int nr, int hdr) {

  int32_t nc=0;  /**< number of printed characters */

  (void) nr;    // Avoid warning about unused parameter

  if (fp==NULL) {
    return(nc);
  }
  if (hdr) {
    nc+=fprintf(fp," %6s %4s %4s %4s %4s %2s %3s %9s %9s %3s %4s %3s\n",
        "size","type","typd","sty","styd","sg","bit","a","b","uid","uidd","exp");
  }
  if (td==NULL) {
    return(nc);
  }

  /* print struct info */
  nc+=fprintf(fp," %6d %4d",td->Size,td->Type);
  switch (td->Type) {
    case  0: nc+=fprintf(fp," %4s","UNKN"); break;
    case  1: nc+=fprintf(fp," %4s","EXG"); break;
    case  2: nc+=fprintf(fp," %4s","BIP"); break;
    case  3: nc+=fprintf(fp," %4s","AUX"); break;
    case  4: nc+=fprintf(fp," %4s","DIG "); break;
    case  5: nc+=fprintf(fp," %4s","TIME"); break;
    case  6: nc+=fprintf(fp," %4s","LEAK"); break;
    case  7: nc+=fprintf(fp," %4s","PRES"); break;
    case  8: nc+=fprintf(fp," %4s","ENVE"); break;
    case  9: nc+=fprintf(fp," %4s","MARK"); break;
    case 10: nc+=fprintf(fp," %4s","ZAAG"); break;
    case 11: nc+=fprintf(fp," %4s","SAO2"); break;
    default: break;
  }    
  /** (+256: unipolar reference, +512: impedance reference) */
  nc+=fprintf(fp," %4d",td->SubType);
  /* SybType description */
  switch (td->SubType) {
    case   0: nc+=fprintf(fp," %4s","Unkn"); break;
    case   1: nc+=fprintf(fp," %4s","EEG"); break;
    case   2: nc+=fprintf(fp," %4s","EMG"); break;
    case   3: nc+=fprintf(fp," %4s","ECG"); break;
    case   4: nc+=fprintf(fp," %4s","EOG"); break;
    case   5: nc+=fprintf(fp," %4s","EAG"); break;
    case   6: nc+=fprintf(fp," %4s","EGG"); break;
    case 257: nc+=fprintf(fp," %4s","EEGR"); break;
    case  10: nc+=fprintf(fp," %4s","resp"); break;
    case  11: nc+=fprintf(fp," %4s","flow"); break;
    case  12: nc+=fprintf(fp," %4s","snor"); break;
    case  13: nc+=fprintf(fp," %4s","posi"); break;
    case 522: nc+=fprintf(fp," %4s","impr"); break;
    case  20: nc+=fprintf(fp," %4s","SaO2"); break;
    case  21: nc+=fprintf(fp," %4s","plet"); break;
    case  22: nc+=fprintf(fp," %4s","hear"); break;
    case  23: nc+=fprintf(fp," %4s","sens"); break;
    case  30: nc+=fprintf(fp," %4s","PVES"); break;
    case  31: nc+=fprintf(fp," %4s","PURA"); break;
    case  32: nc+=fprintf(fp," %4s","PABD"); break;
    case  33: nc+=fprintf(fp," %4s","PDET"); break;
    default: break;
  }    

  nc+=fprintf(fp," %2s %3d %9e %9e %3d",
      (td->Format&0x0100 ? "S" : "U"),(td->Format&0xFF),
      td->a,td->b,td->UnitId);
  /* UnitId description */
  switch (td->UnitId) {
    case 0: nc+=fprintf(fp," %4s","bit"); break;
    case 1: nc+=fprintf(fp," %4s","Volt"); break;
    case 2: nc+=fprintf(fp," %4s","%"); break;
    case 3: nc+=fprintf(fp," %4s","Bpm"); break;
    case 4: nc+=fprintf(fp," %4s","Bar"); break;
    case 5: nc+=fprintf(fp," %4s","Psi"); break;
    case 6: nc+=fprintf(fp," %4s","mH2O"); break;
    case 7: nc+=fprintf(fp," %4s","mHg"); break;
    case 8: nc+=fprintf(fp," %4s","bit"); break;
    default: break;
  }
  nc+=fprintf(fp," %3d\n",td->Exp);
  return(nc);
}


/** Print input device struct 'inpdev' to file 'fp'
 * @return number of characters printed.
 */
int32_t tms_prt_iddata(FILE *fp, tms_input_device_t *inpdev) {

  int32_t i=0;     /**< general index */
  int32_t nc=0;    /**< number of printed characters */

  if (fp==NULL) {
    return(nc);
  } 
  nc+=fprintf(fp,"# Input Device %s Serialnr %d\n",
      inpdev->DeviceDescription,
      inpdev->SerialNumber);
  /* ChannelDescriptions */
  nc+=fprintf(fp,"#%3s %7s %12s %12s","nr","Channel","Gain","Offset");
  nc+=tms_prt_type_desc(fp,NULL,0,(0==0));

  /* print all channel descriptors */
  for (i=0; i<inpdev->NrOfChannels; i++) {
    nc+=fprintf(fp," %3d %7s %12e %12e",
        i,
        inpdev->Channel[i].ChannelDescription,
        inpdev->Channel[i].GainCorrection,
        inpdev->Channel[i].OffsetCorrection);
    /* print type description */  
    nc+=tms_prt_type_desc(fp,&inpdev->Channel[i].Type,i,(1==0));
  }
  return(nc);
}

/** Print channel data block 'chd' of tms device 'dev' to file 'fp'.
 * @param print switch md 0: integer  1: float values
 * @return number of printed characters.
 */
int32_t tms_prt_channel_data(FILE *fp, tms_channel_data_t *chd, int32_t chn_cnt, int32_t md)
{
  int32_t i,j;  /**< general index */
  int32_t nc=0;

  nc+=fprintf(fp,"# Channel data\n");   
  for (j=0; j<chn_cnt; j++) {
    nc+=fprintf(fp,"%2d %2d %2d |",j,chd[j].ns,chd[j].rs);
    for (i=0; i<chd[j].rs; i++) {
      if (md==0) {
        nc+=fprintf(fp," %08X%1C",chd[j].data[i].isample,
            (chd[j].data[i].flag&0x01 ? '*' : ' '));
      } else {
        nc+=fprintf(fp," %9g%1C",chd[j].data[i].sample,
            (chd[j].data[i].flag&0x01 ? '*' : ' '));
      }
    }
    nc+=fprintf(fp,"\n");
  }  
  return(nc);
}

/* Print bit string of 'msg' */
int32_t tms_prt_bits(FILE *fp, uint8_t *msg, int32_t n, int32_t idx)
{
  int32_t nc=0;
  int32_t i,j,a;

  /* hex values */
  nc+=fprintf(fp,"Hex MSB");
  for (i=n-1; i>=idx; i--) {
    nc+=fprintf(fp,"       %02X",msg[i]);
  }
  nc+=fprintf(fp," LSB\n");

  /* bin values */
  nc+=fprintf(fp,"Bin MSB");
  for (i=n-1; i>=idx; i--) {
    fprintf(fp," ");
    a=msg[i];
    for (j=0; j<8; j++) {
      nc+=fprintf(fp,"%1d",(a&0x80 ? 1 : 0));
      a=a<<1;
    }
  }
  nc+=fprintf(fp," LSB\n");
  return(nc);
}

/** Get TMS data from message 'msg' of 'n' bytes into floats 'val'.
 * @return number of samples.
 */
int32_t tms_get_data(uint8_t *msg, int32_t n, tms_input_device_t *dev, 
    tms_channel_data_t *chd)
{
  int32_t nbps;             /**< number of bytes per sample */ 
  int32_t type,size;        /**< TMS type and packet size */
  int32_t i,j;              /**< general index */
  int32_t bip;              /**< bit index pointer */
  int32_t cnt=0;            /**< sample counter */
  int32_t len,dv,overflow;  /**< delta sample: length, value and overflow flag */
  static int32_t *srp=NULL; /**< sample receiving period */
  static int32_t  nrch=0;   /**< current number of channels */
  int32_t maxns;            /**< maximum number of samples */
  int32_t totns;            /**< total number of samples in this block */
  int32_t pc;               /**< period counter */
  float   gain;             /**< for each channel so that all value are in [uV] */

  /* get message type */ 
  type=tms_get_type(msg,n);
  /* parse packet data */
  size=tms_msg_size(msg,n,&i);

  for (j=0; j<dev->NrOfChannels; j++) {
    /* only 1, 2 or 3 bytes width expected !!! */
    nbps=(dev->Channel[j].Type.Format & 0xFF)/8;
    /* get integer sample values */
    chd[j].data[0].isample=tms_get_int(msg,&i,nbps);
    /* sign extension for signed samples */    
    if (dev->Channel[j].Type.Format & 0x0100) {
      chd[j].data[0].isample=(chd[j].data[0].isample<<(32-8*nbps))>>(32-8*nbps);
    }
    /* check for overflow or underflow */
    chd[j].data[0].flag=0x00;
    if (chd[j].data[0].isample ==(int32_t) ((0xFFFFFF80<<(8*(nbps-1))))) {
      chd[j].data[0].flag|=0x01;
    }
    /* increment receive counter */
    chd[j].rs=1;
  }

  /* continue with packets with VL Delta samples */
  if (type==TMSVLDELTADATA) {
    /* check of new space is needed for sample receiving period admin */
    if (nrch != dev->NrOfChannels) {
      /* free previous allocation */
      if (srp != NULL) { free(srp); }
      /* allocate space once for sample receive period */
      srp = (int32_t *)calloc(dev->NrOfChannels, sizeof(int32_t));
      nrch = dev->NrOfChannels;
    }
    /* find maximum period and count total number of samples */
    maxns=0; totns=0;
    for (j=0; j<dev->NrOfChannels; j++) {
      if (!chd[j].data[0].flag&0x01) {
        if (maxns<chd[j].ns) { maxns=chd[j].ns; }
        totns+=chd[j].ns;
      } else {
        totns++;
      }
    } 
    /* calculate sample receive period per channel */
    for (j=0; j<dev->NrOfChannels; j++) {
      srp[j]=maxns/chd[j].ns;
    } 
    if (tms_vb&0x04) {
      /* print delta block */
      fprintf(stderr,"\nDelta block of %d bytes totns %d\n",n-2-i,totns);
      tms_prt_bits(stderr,msg,n-2,i);
      /* Delta block */
      fprintf(stderr,"Delta block:");
    }
    bip=8*i; cnt=dev->NrOfChannels; j=0; pc=1;
    while ((cnt<totns) && (bip<8*n-16)) {
      len=get_lsbf_int32_t(msg,&bip,4);
      if (len==0) {
        dv=get_lsbf_int32_t(msg,&bip,2); overflow=0;
        switch (dv) {
          case 0: dv= 0; overflow=0; break; /* delta sample = 0 */ 
          case 1: dv= 0; overflow=0; break; /* not used */ 
          case 2: dv= 0; overflow=1; break; /* overflow */ 
          case 3: dv=-1; overflow=0; break; /* delta sample =-1 */ 
          default: break;
        }
      } else {
        dv=get_lsbf_int32_t_sign_ext(msg,&bip,len);
        overflow=0;
      }
      if (tms_vb&0x04) { fprintf(stderr," %d:%d",len,dv); }
      /* find channel not in overflow and needs this sample */
      while ((chd[j].data[0].flag&0x01) || (chd[j].rs>=chd[j].ns) || ((pc % srp[j])!=0)) {
        /* next channel nr */
        j++; if (j==dev->NrOfChannels) { j=0; pc++; }
        if (pc>maxns) break;
      }
      chd[j].data[chd[j].rs].isample=dv;
      chd[j].data[chd[j].rs].flag=overflow;
      if (len==15) {
        if (abs(dv)>=((1<<len)-1)) {
          chd[j].data[chd[j].rs].flag|=0x02; 
        }
      }
      chd[j].rs++;
      /* delta sample counter */
      cnt++;
      /* next channel nr */
      j++; if (j==dev->NrOfChannels) { j=0; pc++; }
    }
    if (tms_vb&0x04) { fprintf(stderr," cnt %d\n",cnt); }
  }

  /* convert integer samples to real floats */
  for (j=0; j<dev->NrOfChannels; j++) {
    /* integrate delta value to actual values or fill skipped overflow channels */
    for (i=1; i<chd[j].ns; i++) {
      /* check overflow */
      if (chd[j].data[0].flag&0x01) {
        chd[j].data[i].isample =chd[j].data[0].isample;
        chd[j].data[i].flag    =chd[j].data[0].flag;
      } else {
        chd[j].data[i].isample+=chd[j].data[i-1].isample;
      }
    }
    /* convert all values into [uV] */
    gain=powf(10,dev->Channel[j].Type.Exp+6);

    /* convert to real float values */
    for (i=0; i<chd[j].ns; i++) {
      chd[j].data[i].sample=((dev->Channel[j].Type.a)*chd[j].data[i].isample + dev->Channel[j].Type.b)*gain;
    }

    /* update sample counter */
    chd[j].sc += chd[j].ns;
  }
  return(cnt);
}

/** Flag all samples in 'channel' with 'flg'.
 * @return always 0
*/
int32_t tms_flag_samples(tms_channel_data_t *chn, int32_t chn_cnt, int32_t flg) {

  int32_t i,j;                      /**< general index */

  for (i=0; i<chn_cnt; i++) {
    for (j=0; j<chn[i].ns; j++) {
      chn[i].data[j].flag=flg;
    }
  }
  return(0);
}

/** Print TMS channel data 'chd' to file 'fp'.
 * @param md: print switch per channel 1: float 0: integer values 
 *            A: 0x01 B: 0x02 C: 0x04 ...
 * @param cs: channel selector A: 0x01 B: 0x02 C: 0x04 ...
 * @param nchn: number of channels: i.e. tms_get_number_of_channels()
 * @return number of characters printed.
 */
int32_t tms_prt_samples(FILE *fp, tms_channel_data_t *chd, int32_t cs, int32_t md, int32_t nchn)
{
  int32_t nc=0;                 /**< number of characters printed */ 
  static int32_t maxns=0;       /**< maximum number of samples over all channels */
  int32_t mns;                  /**< current maxns */
  int32_t cnt;                  /**< number of channels with equal number of samples */
  int32_t i,j,jm,idx;           /**< general index */
  int32_t ssf;                  /**< sub-sample factor */
  static tms_data_t *prt=NULL;  /**< printer storage for all samples over all channels */

  /* print output file header */
  if (chd[0].sc==0) {
    nc+=fprintf(fp,"#%9s","t[s]");
    for (j=0; j<nchn; j++) {
      if (cs&(1<<j)) {
        nc+=fprintf(fp," %8s%1c","",'A'+j);
      }
    }
    nc+=fprintf(fp," %8s\n","sc");
  }

  /* search for current maximum number of samples over all channels */
  mns=chd[0].ns; cnt=1;
  for (j=1; j<nchn; j++) {
    if (chd[j].ns==chd[0].ns) { cnt++; }
    if (mns<chd[j].ns)  { mns=chd[j].ns; }
  }
  
  /* trival print when all channels have equal number of samples */
  if (cnt==nchn) {
    for (i=0; i<chd[0].ns; i++) {
      nc+=fprintf(fp," %9.4f",(chd[0].sc+i)*chd[0].td);
      for (j=0; j<nchn; j++) {
        if (cs&(1<<j)) {
          if (chd[j].data[i].flag > 0) {
             nc+=fprintf(fp," %9s","NaN");
          } else {
            if (md&(1<<j)) {
              nc+=fprintf(fp," %9.3f",chd[j].data[i].sample);
            } else {
              nc+=fprintf(fp," %9d",chd[j].data[i].isample);
            }
          }
        }
      }
      nc+=fprintf(fp," %d\n",(chd[0].sc+i));
    }
    return(nc); 
  }
  
  if (mns>maxns) {
    maxns=mns;
    if (prt!=NULL) { 
      /* free previous printer storage space */
      free(prt); 
    }
    /* malloc storage space for rectangle print data array */
    prt=(tms_data_t *)calloc(maxns*nchn,sizeof(tms_data_t));
  }

  /* search for channel 'jm' with maximum number of samples over all wanted channels */
  mns=0; jm=0;
  for (j=0; j<nchn; j++) {
    if (cs&(1<<j)) {
      if (mns<chd[j].ns) {
        mns=chd[j].ns;
        jm=j;
      }
    }
  }

  /* fill all wanted channels */  
  for (j=0; j<nchn; j++) {
    if (cs&(1<<j)) {
      ssf=mns/chd[j].ns;
      for (i=0; i<mns; i++) {
        idx=maxns*j+i;
        if ((i % ssf)==0) {
          /* copy samples into rectanglar array 'ptr' */
          prt[idx]=chd[j].data[i/ssf];
        } else {
          /* fill all unavailable samples with "NaN" -> flag=0x04 */
          prt[idx].isample=0; prt[idx].sample=0.0; prt[idx].flag=0x04;
        }
      }
    }
  }

  /* print all wanted channels */
  for (i=0; i<mns; i++) {
    nc+=fprintf(fp," %9.4f",(chd[jm].sc+i)*chd[jm].td);
    for (j=0; j<nchn; j++) {
      if (cs&(1<<j)) {
        idx=maxns*j+i;
        if (prt[idx].flag!=0) {
          /* all overflows and not availables are mapped to "NaN" */
          nc+=fprintf(fp," %9s","NaN");
        } else {
          if (md&(1<<j)) {
            nc+=fprintf(fp," %9.3f",prt[idx].sample);
          } else {
              nc+=fprintf(fp," %9d",prt[idx].isample);
          } 
        }
      }
    }
    nc+=fprintf(fp," %8d\n",chd[jm].sc+i);
  }
  return(nc);
}

/** Send VLDeltaInfo request to file descriptor 'fd'
*/
int32_t tms_snd_vldelta_info_request(int32_t fd)
{
  uint8_t req[10];    /**< id request message */
  int32_t i=0;
  int32_t bw;         /**< byte written */

  /* block sync */ 
  tms_put_int(TMSBLOCKSYNC,req,&i,2);
  /* length 0 */
  tms_put_int(0x00,req,&i,1);
  /* IDReadReq type */
  tms_put_int(TMSVLDELTAINFOREQ,req,&i,1);
  /* add checksum */
  bw=tms_put_chksum(req,i);

  if (tms_vb&0x01) {
    tms_write_log_msg(req,bw,"send VLDeltaInfo request");
  }
  /* send request */
  bw=BtWriteBytes(fd,req,bw);
  if (bw < 0) {
    perror("# Warning: tms_send_vl_delta_info_request write problem");
  }
  return bw;
}

/** Convert buffer 'msg' of 'n' bytes into tms_vldelta_info_t 'vld' for 'nch' channels.
 * @return number of bytes processed.
 */
int32_t tms_get_vldelta_info(uint8_t *msg, int32_t n, int32_t nch, tms_vldelta_info_t *vld) {

  int32_t i,j;   /**< general index */
  int32_t type;  /**< message type */
  int32_t size;  /**< payload size */

  /* get message type */ 
  type=tms_get_type(msg,n);
  if (type!=TMSVLDELTAINFO) {
    fprintf(stderr,"# Warning: tms_get_vldelta_info type %02X != %02X\n",
        type,TMSVLDELTAINFO);
    return(-1);
  }
  /* get payload size and payload pointer 'i' */
  size=tms_msg_size(msg,n,&i);
  vld->Config=(uint16_t) tms_get_int(msg,&i,2);
  vld->Length=(uint16_t) tms_get_int(msg,&i,2);
  vld->TransFreqDiv=(uint16_t) tms_get_int(msg,&i,2);
  vld->NrOfChannels=(uint16_t) nch;
  vld->SampDiv=(uint16_t *)calloc(nch,sizeof(int16_t));
  for (j=0; j<nch; j++) {
    vld->SampDiv[j]=(uint16_t) tms_get_int(msg,&i,2);
  }
  /* number of found Frontend system info structs */  
  return(i);
}

/** Print tms_prt_vldelta_info 'vld' to file 'fp'.
 *  @return number of printed characters.
 */
int32_t tms_prt_vldelta_info(FILE *fp, tms_vldelta_info_t *vld, int nr, int hdr) {

  int32_t nc=0;  /**< number of printed characters */
  int32_t j;     /**< general index */

  if (fp==NULL) {
    return(nc);
  }
  if (hdr) {
    nc+=fprintf(fp,"# VL Delta Info\n");
    nc+=fprintf(fp,"# %5s %6s %6s %6s","nr","Config","Length","TransFS");
    for (j=0; j<vld->NrOfChannels; j++) {
      nc+=fprintf(fp," SampDiv%02d",j);
    }    
    nc+=fprintf(fp,"\n");

  }
  nc+=fprintf(fp," %6d %6d %6d %6d",nr,vld->Config,vld->Length,vld->TransFreqDiv);
  for (j=0; j<vld->NrOfChannels; j++) {
    nc+=fprintf(fp," %9d",vld->SampDiv[j]);
  }
  nc+=fprintf(fp,"\n");
  return(nc);
}

/** Send Real Time Clock (RTC) read request to file descriptor 'fd'
 *  @return number of bytes send.
 */
int32_t tms_send_rtc_time_read_req(int32_t fd)
{
  uint8_t req[10];    /**< id request message */
  int32_t i=0;
  int32_t bw;         /**< byte written */

  /* block sync */ 
  tms_put_int(TMSBLOCKSYNC,req,&i,2);
  /* length 0 */
  tms_put_int(0x00,req,&i,1);
  /* IDReadReq type */
  tms_put_int(TMSRTCTIMEREADREQ,req,&i,1);
  /* add checksum */
  bw=tms_put_chksum(req,i);

  if (tms_vb&0x01) {
    tms_write_log_msg(req,bw,"send rtc read request");
  }
  /* send request */
  bw=BtWriteBytes(fd,req,bw);
  if(bw < 0) {
    perror("# Warning: tms_rtc_time_read_request write problem");
  }
  return(bw);
}

/** Convert buffer 'msg' of 'n' bytes into time_t 'rtc'
 * @return 0 on failure and number of bytes processed
 */
int32_t tms_get_rtc(uint8_t *msg, int32_t n, time_t *rtc) {

  int32_t i;       /**< general index */
  int32_t type;    /**< message type */
  int32_t size;    /**< payload size */
  struct tm when;  /**< broken time struct */

  /* get message type */ 
  type=tms_get_type(msg,n);
  if (type!=TMSRTCTIMEDATA) {
    fprintf(stderr,"# Warning: type %02X != %02X\n",
        type,TMSRTCTIMEDATA);
    return(-3);
  }
  /* get payload size and start pointer */
  size=tms_msg_size(msg,n,&i);
  if (size!=8) {
    fprintf(stderr,"# Warning: tms_get_rtc: unexpected size %d iso %d\n",size,8);
  }
  /* parse message 'msg' */
  when.tm_sec =tms_get_int(msg,&i,2);
  when.tm_min =tms_get_int(msg,&i,2);
  when.tm_hour=tms_get_int(msg,&i,2);
  when.tm_mday=tms_get_int(msg,&i,2);
  when.tm_mon =tms_get_int(msg,&i,2)-1;    /* months since January [0,11] */
  when.tm_year=tms_get_int(msg,&i,2)
          +100*tms_get_int(msg,&i,2)-1900; /* years since 1900 */
  when.tm_wday=tms_get_int(msg,&i,2);
  when.tm_isdst= -1;  
  /* -1: timezone information and system databases is used to determine
       whether DST is in effect at the specified time */

  /* make time */
  *rtc=mktime(&when);

  return(i);
}


/** Write RealTime Clock 'rtc' into socket 'fd'.
 * @return number of bytes written (<0: failure)
*/
int32_t tms_write_rtc(int32_t fd, time_t *rtc) {

  int32_t i;         /**< general index */
  uint8_t msg[0x40]; /**< message buffer */
  int32_t bw;        /**< byte written */
  struct  tm when;
  
  when = *localtime(rtc);
  
  i=0;
  /* construct set RTC message */ 
  /* block sync */ 
  tms_put_int(TMSBLOCKSYNC,msg,&i,2);
  /* length */
  tms_put_int(8,msg,&i,1);
  /* Set type to RTC */
  tms_put_int(TMSRTCTIMEDATA,msg,&i,1);
  /* put 'rtc' into  message 'msg' */
  tms_put_int(when.tm_sec ,msg,&i,2);
  
  tms_put_int(when.tm_min     ,msg,&i,2);
  tms_put_int(when.tm_hour    ,msg,&i,2);
  tms_put_int(when.tm_mday    ,msg,&i,2);
  tms_put_int(when.tm_mon+1   ,msg,&i,2);
  tms_put_int(when.tm_year%100,msg,&i,2);
  tms_put_int(when.tm_year/100+19,msg,&i,2);
  tms_put_int(when.tm_wday    ,msg,&i,2);
  
  /* add checksum */
  bw=tms_put_chksum(msg,i);
  if (tms_vb&0x01) {
    tms_write_log_msg(msg,bw,"write RTC");
  }
  /* send request */
  bw=BtWriteBytes(fd,msg,bw);
  /* return number of byte actually written */ 
  return(bw);
}


/** Print tms_rtc_t 'rtc' to file 'fp'.
 *  @return number of printed characters.
 */
int32_t tms_prt_rtc(FILE *fp, time_t *rtc, int nr, int hdr) {

  int32_t nc=0;  /**< number of printed characters */
  struct  tm t0;
  char    when[MNCN];

  if (fp==NULL) {
    return(nc);
  }
  if (hdr) {
    nc+=fprintf(fp,"# Real time clock\n");
    nc+=fprintf(fp,"#    nr yyyy-mm-dd hh:mm:ss\n");
  }
  t0=*localtime(rtc);
 
  strftime(when,MNCN,"%Y-%m-%d %H:%M:%S",&t0);

  nc+=fprintf(fp," %6d %s\n",nr,when);
  return(nc);
}

static tms_frontendinfo_t *fei=NULL;        /**< storage for all frontend info structs */
static tms_vldelta_info_t *vld=NULL;
static tms_input_device_t *in_dev=NULL;     /**< TMSi input device */

/** Get the number of channels of this TMSi device
 * @return number of channels
*/
int32_t tms_get_number_of_channels()
{
  if (in_dev==NULL) {
    return(14);
  }
  return in_dev->NrOfChannels;
}

/** Get the TMSi device name
  * @return pointer to TMSi device name
*/
char *tms_get_device_name() {
 
  if (in_dev==NULL) {
    return(NULL);
  }
  return(in_dev->DeviceDescription);
}

/* Get the current sample frequency.
* @return current sample frequency [Hz]
*/
double tms_get_sample_freq()
{
  if (fei==NULL) {
    return(2048.0);
  }
  return (double)(fei->basesamplerate/(1<<fei->currentsampleratesetting));
}

/** Construct channel data block with frontend info 'fei' and
 *   input device 'dev' with eventually vldelta_info 'vld'.
 * @return pointer to channel_data_t struct, NULL on failure.
 */
tms_channel_data_t *tms_alloc_channel_data()
{
  int32_t i;                 /**< general index */
  tms_channel_data_t *chd;   /**< channel data block pointer */
  int32_t ns_max=1;          /**< maximum number of samples of all channels */
    
  /* allocate storage space for all channels */
  chd = (tms_channel_data_t *)calloc(in_dev->NrOfChannels, sizeof(tms_channel_data_t));
  for (i=0; i < in_dev->NrOfChannels; i++) {
    if ((fei->currentsampleratesetting == 0) && (vld->TransFreqDiv == 1)) {
      /* MiniMobi with srd==0 sends to 2 samples per packet */
      chd[i].ns = 2;
    } else 
    if ((fei->currentsampleratesetting > 2) || (vld->TransFreqDiv == 1)) {
      chd[i].ns = 1;
    } else {
      chd[i].ns = (vld->TransFreqDiv + 1)/(vld->SampDiv[i] + 1);
    }
    /* reset sample counter so that it will start after first packet with '0' */
    chd[i].sc = -chd[i].ns;
    if (chd[i].ns>ns_max) { ns_max = chd[i].ns; }
    chd[i].data = (tms_data_t *)calloc(chd[i].ns,sizeof(tms_data_t));
  }  
  for (i=0; i < in_dev->NrOfChannels; i++) {
    chd[i].td = ns_max/(chd[i].ns*tms_get_sample_freq());
    if (tms_vb & 0x02) {
      fprintf(stderr,"chn %d ns %d td %.4f\n", i, chd[i].ns, chd[i].td);
    }
  }
  return(chd);
}

/** Free channel data block */
void tms_free_channel_data(tms_channel_data_t *chd)
{
  int32_t i;                 /**< general index */

  if ( chd == NULL ) return;
    
  /* free storage space for all channels */
  for (i=0; i<tms_get_number_of_channels(); i++) {
      if (chd[i].data!=NULL) free(chd[i].data);
  }
  free(chd);
}

/** Get saw length [bits] for input device description
 * @return saw length
 * @note local variables saw_chn and saw_len will be set
*/
int32_t get_saw_len_from_input_device(tms_input_device_t *dev) {

  int32_t i; /**< channel index */
  char devname[256];
  int32_t devnr;
  
  /* check all channels for saw type */
  for (i=0; i < dev->NrOfChannels; i++) {
    if (dev->Channel[i].Type.Type==10) { saw_chn=i; }
  }
  
  // # Input Device NeXus-10 Serialnr 928080086926 Mobi6
  //  938 NX10MkII
  //  934 NX4          check saw_len
  //  931 Mobimini
  //  928 Mobi8/NX10
  //  710 Mobita       check saw_len
  //  207 Porti7       check saw_len
  //  208 NX16/NX32    check saw_len 
  devnr = dev->SerialNumber/1000000;
  switch (devnr) { 
    case 938: saw_len=14; strcpy(devname,"NX10MkII"); break;
    case 934: saw_len=8;  strcpy(devname,"NX4");  break; 
    case 926: 
    case 931: saw_len=8;  strcpy(devname,"MobiMini"); break;
    case 928: saw_len=5;  strcpy(devname,"NeXus-10/Mobi8"); break;
    case 710: saw_len=8;  strcpy(devname,"Mobita"); break;
    case 207: saw_len=8;  strcpy(devname,"Porti7"); break;
    case 208: saw_len=8;  strcpy(devname,"NX16/NX32"); break;
    default: fprintf(stderr,"# Error: unknown TMSi hardware version %d\n",devnr);
  }
  
  if (tms_vb&0x02) {
    fprintf(stderr,"# Info: dev %s saw_chn %d saw_len %d\n", devname, saw_chn, saw_len);
  }

  return(saw_len);
}

static int32_t state=0;   /**< State machine */
static int32_t fd=0;      /**< File descriptor of bluetooth socket */

/** Initialize TMSi device with Bluetooth address 'fname' and
 *   sample rate divider 'sample_rate_div'.
 * @note no timeout implemented yet.
 * @return current sample rate [Hz]  or -1 on failure
*/
int32_t tms_init(int32_t fdd, int32_t sample_rate_div) {

  int bw = 0;                    /**< bytes written */
  int br = 0;                    /**< bytes read */
  int32_t fs=0;                  /**< sample frequence */
  uint8_t resp[0x10000];         /**< TMS response to challenge */
  int32_t type;                  /**< TMS message type */
  int32_t send=0;                /**< number of send attempts */
  int32_t received=0;            /**< number of received messages */ 
  int32_t transmit=1;            /**< retransmit last request */
  
  tms_acknowledge_t  ack;        /**< TMS acknowlegde */
  
  fei = (tms_frontendinfo_t *)malloc(sizeof(tms_frontendinfo_t));
  vld = (tms_vldelta_info_t *)malloc(sizeof(tms_vldelta_info_t));
  in_dev = (tms_input_device_t *)malloc(sizeof(tms_input_device_t));

  if (fdd<0) {
    return(-1);
  }
  fd=fdd;

  while (state < 4) {

    switch (state) {
      case 0: 
        /* send frontend Info request */
        if(transmit) {
          bw=tms_snd_FrontendInfoReq(fd);
          if (bw<0) {
            fprintf(stderr,"# Error: Can't send Frontend Info Request to nexus\n");
            abort();
          }
          send++;
          transmit=0;
        }
        /* receive response to frontend Info request */
        br=tms_rcv_msg(fd,resp,sizeof(resp));
        break;
      case 1:  
        if (transmit) {
          /* switch off data capture when it is on */
          /* stop capture */
          fei->mode|=0x01;         
          /* set sample rate divider */
          fs=fei->basesamplerate/(1<<sample_rate_div);
          fei->currentsampleratesetting=(uint16_t) sample_rate_div;
          /* send it */
          tms_write_frontendinfo(fd,fei);
          send++;
          transmit=0;
        }
        /* receive ack */
        br=tms_rcv_msg(fd,resp,sizeof(resp));
        break;
      case 2:
        /* receive ID Data */
        br=tms_fetch_iddata(fd,resp,sizeof(resp));
        if (br < 0) {
          return -1;
        }
        break;
      case 3:
        /* send vldelta info request */
        bw=tms_snd_vldelta_info_request(fd);
        /* receive response to vldelta info request */
        br=tms_rcv_msg(fd,resp,sizeof(resp));
        break;
    }
    
    if (tms_vb&0x01) {
      fprintf(stderr,"# State %d\n", state);
    }
    /* process response */
    if (br<0) {
      fprintf(stderr,"# Error: no valid response in state %d\n",state);
      transmit=1;
    } else {
      received++;
    }

    if (received>MAX_RECEIVED_COUNT) {
      transmit=1;
    }
    if (send>RETRY_COUNT) {
      fprintf(stderr,"# Error: exceeded retry count!!!\n");
      return(-1);
    }

    /* check checksum and get type of response */
    if (tms_chk_msg(resp,br)!=0) {
      fprintf(stderr,"# checksum error !!!\n");
    } else {
      
      type=tms_get_type(resp,br);
      fprintf(stderr,"# Info: State is %d received msg with type 0x%02X\n",state,type);
      
      switch (type) {
      
        case TMSVLDELTADATA:
        case TMSCHANNELDATA:
        case TMSRTCTIMEDATA:
          break;
        case TMSFRONTENDINFO:
          if (state==0) {
            /* decode packet to struct */
            tms_get_frontendinfo(resp,br,fei);
            if (tms_vb&0x02) {
              tms_prt_frontendinfo(stderr,fei,0,(0==0));
            }
            state++;
            send=0; received=0; transmit=1;
          }
          break;
        case TMSACKNOWLEDGE:
          if (state==1) {
            tms_get_ack(resp,br,&ack);
            if (tms_vb&0x02) {
              tms_prt_ack(stderr,&ack);
            }
            if (ack.errorcode>0) { 
              tms_prt_ack(stderr,&ack);
              return(-1);
            }
            state++;
            send=0; received=0; transmit=1;
          }
          break;
        case TMSVLDELTAINFO:
          tms_get_vldelta_info(resp,br,in_dev->NrOfChannels,vld);
          if (tms_vb&0x02) {
            tms_prt_vldelta_info(stderr,vld,0,0==0);
          }
          state++;
          break;
        case TMSIDDATA:
          tms_get_iddata(resp,br,in_dev);
          if (tms_vb&0x02) {
            tms_prt_iddata(stderr,in_dev);
          }
          get_saw_len_from_input_device(in_dev);
          state++;
          break;
          
        default:
          fprintf(stderr,"# don't understand type %02X\n",type);
          break;
      }
    }
  }
  return(fs);
}

/** Get elapsed time [s] of this tms_channel_data_t 'channel'.
* @return -1 of failure, elapsed seconds in success.
*/
double tms_elapsed_time(tms_channel_data_t *channel) {

  if (channel==NULL) {
    return(-1.0);
  }
  /* elapsed time = previous sample counter * tick duration */
  return(channel[0].sc*channel[0].td);
}

/** Get one or more samples for all channels
*  @note all samples are returned via 'channel'
* @return lost packet(s) before this packet (should be zero)
*/
int32_t tms_get_samples(tms_channel_data_t *channel) {

  int32_t br = 0;                /**< bytes read */
  int32_t i,j;                   /**< general index */
  uint8_t resp[0x10000];         /**< TMS response to challenge */
  int32_t type;                  /**< TMS message type */
  static int32_t dpc=0;          /**< data packet counter */
  double t;                      /**< current and start time */
  static double tze;             /**< time of previous zaag error */
  static double t0;              /**< start time */
  static double tka;             /**< keep-alive time */
  static int32_t pzaag=-1;       /**< previous zaag value */
  int32_t zaag;                  /**< current zaag value */
  static int32_t tzerr=0;        /**< total zaag error counter */
  int32_t zerr=0;                /**< zaag error value */
  int32_t cnt=0;                 /**< sample counter */
  tms_acknowledge_t  ack;        /**< TMS acknowlegde */
  int32_t dcnt=0;                /**< delta packet count */
 
  if (state < 4 || state > 5) {
    return -1;
  }
    
  if  (state==4) {
    /* switch to data capture 0x01 active low */
    fei->mode=fei->mode & 0xFFFC;
    /* switch to data capture 0x01 and flash storage 0x02: active low */
    //fei->mode=0x00;
    /* start data capturing */
    tms_write_frontendinfo(fd,fei);
    /* receive ack */
    br=tms_rcv_msg(fd,resp,sizeof(resp));
    type=tms_get_type(resp,br);
    fprintf(stderr,"# Info: State is %d received msg with type %d\n",state,type);
    if (type != TMSACKNOWLEDGE) {
      return -1;
    } else {
      tms_get_ack(resp,br,&ack);
      if (tms_vb&0x02) { tms_prt_ack(fpl,&ack); }
      if (ack.errorcode>0) { 
        tms_prt_ack(stderr,&ack); 
        if (ack.errorcode!=0x12) {
          return(-1);
        }
      }
      state++; t0=get_time(); t=t0; tze=t;
    }
  }
  if (state==5) {
    br=tms_rcv_msg(fd,resp,sizeof(resp));
    if (tms_chk_msg(resp,br)!=0) {
      fprintf(stderr,"# checksum error !!!\n");
      //state=4;
      return(-1);
    } else {
      
      type=tms_get_type(resp,br);
      
      switch (type) {
      
        case TMSVLDELTADATA:
        case TMSCHANNELDATA:
          /* get current time */
          t=get_time();
          /* first sample */
          if (dpc==0) { 
            /* start keep alive timer */
            tka=t;
          }
          /* convert channel data to float's */
          cnt=tms_get_data(resp,br,in_dev,channel);
          
          /* repeat sample for channels with missing sample */
          for (j=0; j<in_dev->NrOfChannels; j++) {
            if (channel[j].rs < channel[j].ns) {
              for (i=channel[j].rs; i<channel[j].ns; i++) {
                channel[j].data[i] = channel[j].data[channel[j].rs-1];
              }
              channel[j].rs = channel[j].ns;
            }
          }
          
          if (tms_vb&0x04) {
            /* print wanted channels !!! */
            tms_prt_channel_data(stderr,channel,in_dev->NrOfChannels,1);
          }
          
          /* check zaag in channel number 'saw_chn'  */
          for (i=0; i<channel[saw_chn].rs; i++) {
            //fprintf(stderr,"# i %d Zaag %d\n", i, channel[saw_chn].data[i].isample);
            if ((saw_len==5) && ((saw_chn==13) || (saw_chn==9))) {
              zaag=channel[saw_chn].data[i].isample/2;
            } else { /* Nexus10 Mark II or MobiMini */
              zaag=channel[saw_chn].data[i].isample;
            }
            if (pzaag==-1) { dcnt=1; } else { dcnt=(zaag-pzaag+(1<<saw_len)) % (1<<saw_len); }
            if (dcnt!=1) {
              fprintf(stderr,"# TMSi continuity counter problem: %2d previous: %2d dcnt %2d t %7.3f dt %6.3f\n",
                zaag, pzaag, dcnt, t-t0, t-tze);
              /* !!! 5 bits for saw is too small -> firmware fix in Mobi-8 */
              /* correct data packet counter with saw jump */
              dpc+=dcnt;
              /* saw error */
              zerr=1;
              tzerr++;
              tze=t;
            }
            pzaag=zaag; 
          }
          
          /* check if keep alive is needed */
          if (t-tka>10.0) {
            tms_snd_keepalive(fd);
            tka=t;
          }  
          /* increment data packet counter */
          dpc++;
          break;
        default:
          break;
       
      }
    }
  }
  return(dcnt-1);
}

/** shutdown sample capturing.
 *  @return 0 always.
*/
int32_t tms_shutdown()
{
  int br = 0;                    /**< bytes read */
  uint8_t resp[0x10000];         /**< TMS response to challenge */
  int32_t type;                  /**< TMS message type */
  int32_t got_ack=0;
  int32_t retry=0;

  if (fd>0) {
    /* stop capturing data */
    fei->mode=fei->mode | 0x01;
    tms_write_frontendinfo(fd,fei);

    /* wait for ack is received */
    do
    {
      br=tms_rcv_msg(fd,resp,sizeof(resp));
      if (tms_chk_msg(resp,br)!=0) {
        fprintf(stderr,"# checksum error !!!\n");
        retry++;
      } else {
        type=tms_get_type(resp,br);
        if (type==TMSACKNOWLEDGE) {
          got_ack=1;
        }
      }
    } while (!got_ack && retry<3);
  }

  state=0;

  free(fei);
  free(vld);
  free(in_dev);
  return(state);
}

/** Check battery status
* @return 0:ok 1:battery low
*/
int32_t tms_chk_battery(tms_channel_data_t *chd, int32_t sw_chn) {

  int32_t i;
  int32_t sw;
  int32_t bat_low=0;
  
  for (i=0; i<chd[sw_chn].rs; i++) {
    sw=chd[sw_chn].data[i].isample;
    if (sw&0x02) { bat_low=1; }
  }
  return(bat_low);
}


/** Check button status
* @return 0:not pressed >0:button number and 'tsw' [s] of rising edge
*/
int32_t tms_chk_button(tms_channel_data_t *chd, int32_t sw_chn, double *tsw) {

  int32_t i;
  static int32_t sw,tr,tf;
  int32_t psw;
  int32_t button=0;
  
  for (i=0; i<chd[sw_chn].rs; i++) {
    psw=sw; sw=chd[sw_chn].data[i].isample & 0x01;
    if ((psw==0) && (sw==1)) {
      /* rising edge */
      tr=chd[sw_chn].sc+i;
    }
    if ((psw==1) && (sw==0)) {
      /* falling edge */
      tf=chd[sw_chn].sc+i;
      button= (int32_t)round( (double)(tf-tr) / (BUTTON_DELTA * 0.5 * tms_get_sample_freq() ) );
      (*tsw)=tr/(0.5 * tms_get_sample_freq());
      if (tms_vb&0x08) {
        fprintf(stderr,"# button t %9.3f tc %d nr %d\n",(*tsw),tf-tr,button);
      }
    }
  }
  return(button);
}

/*********************************************************************/
/*                    EDF/BDF functions                              */
/*********************************************************************/

/** Print double 'a' in exactly 'w' characters to string 'msg'
 *   use e-notation to fit too big or small numbers
 * @note 'w' should be >= 7
 * @return number of printed characters
*/
int32_t tms_prt_double(char *msg, int32_t w, double a) {

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
  strncpy(msg,buf,w);
  return(n);
}

/** Fill EDF/BDF main header 
 *  @return 0 always
*/
int32_t edfFillMainHdr(edfMainHdr_t *mHdr, int bdf, const char  *Patient, const char  *RecordId,
 const char *Version, time_t *t, int32_t NrOfDataRecords, double RecordDuration, 
 int32_t NrOfSignalsInRecord) {

  struct tm t0;
  char   tmp[128];
  
  /* Fill it with spaces */
  memset(mHdr, ' ',sizeof(edfMainHdr_t));
  if (bdf==1) {
    /* Biosemi Data File: BDF = EDF with 24 bits samples iso 16 bits */
    mHdr->Version[0]=0xFF;
    //sprintf(&mHdr->Version[1],"%-7s","BIOSEMI"); 
    sprintf(tmp,"%-7s","BIOSEMI"); memcpy(&mHdr->Version[1],tmp,7);
  } else {
    /* standard EDF file with 16 bits samples */
    mHdr->Version[0]='0';
    sprintf(tmp,"%-7s",""); memcpy(&mHdr->Version[1],tmp,7);    
  }  
  sprintf(tmp,"%-80s",Patient); memcpy(mHdr->PatientId,tmp,80);
  sprintf(tmp,"%-80s",RecordId); memcpy(mHdr->RecordingId,tmp,80);
  t0=*localtime(t);
  strftime(tmp,9,"%d.%m.%y",&t0); memcpy(mHdr->StartDate,tmp,9);
  strftime(tmp,9,"%H.%M.%S",&t0); memcpy(mHdr->StartTime,tmp,9);
  sprintf(tmp,"%-8d",sizeof(edfMainHdr_t)+sizeof(edfSignalHdr_t)*NrOfSignalsInRecord);
  memcpy(mHdr->NrOfHeaderBytes,tmp,8);
  if (bdf==1) {
    sprintf(tmp,"%-44s","24BIT");
  } else {
    sprintf(tmp,"%-44s",Version);
  } 
  memcpy(mHdr->DataFormatVersion,tmp,44); 
  sprintf(tmp,"%-8d",NrOfDataRecords); memcpy(mHdr->NrOfDataRecords,tmp,8);
  tms_prt_double(mHdr->RecordDuration, 8, RecordDuration);
  /* to avoid '\0' overwrite in succedding data struct */
  sprintf(tmp,"%-4d",NrOfSignalsInRecord);
  memcpy(mHdr->NrOfSignalsInRecord,tmp,4);
        
  return(0);
}

/** Fill EDF signal header 
 *  @return 0 always
*/
int32_t edfFillSignalHdr(edfSignalHdr_t *sHdr, const char *Label, 
 char *TransducerType, char *PhysicalDimension, 
 double PhysicalMin, double PhysicalMax,      
 int32_t DigitalMin,int32_t DigitalMax, 
 char *Prefiltering, int32_t NrOfSamples) {     

  char   tmp[128];
 
  /* Fill it with spaces */
  memset(sHdr, ' ',sizeof(edfSignalHdr_t));
  sprintf(tmp, "%-16s",Label);             memcpy(sHdr->Label            ,tmp,16);
  sprintf(tmp, "%-80s",TransducerType);    memcpy(sHdr->TransducerType   ,tmp,80);
  sprintf(tmp,  "%-8s",PhysicalDimension); memcpy(sHdr->PhysicalDimension,tmp,8);
  tms_prt_double(sHdr->PhysicalMin, 8, PhysicalMin);
  tms_prt_double(sHdr->PhysicalMax, 8, PhysicalMax);
  sprintf(tmp, "%-8d",DigitalMin);         memcpy(sHdr->DigitalMin       ,tmp,8);
  sprintf(tmp, "%-8d",DigitalMax);         memcpy(sHdr->DigitalMax       ,tmp,8);
  sprintf(tmp,"%-80s",Prefiltering);       memcpy(sHdr->Prefiltering     ,tmp,80);
  sprintf(tmp, "%-8d",NrOfSamples);        memcpy(sHdr->NrOfSamples      ,tmp,8);
  memcpy(tmp,"Reserved",8);                memcpy(sHdr->Reserved         ,tmp,8);
 
  return(0);
}

/** Write 'n' bytes of integer 'a' into EDF file 'fp'
 */
int32_t edfWriteSample(FILE *fp, int32_t a, int32_t n) {
  
  int32_t i;  /**< general index */
  //char b[n];  /**< little endian represenation of 'a' */
  char *b = alloca(n);
  
  for (i=0; i<n; i++) {
    b[i]=(char)(a & 0xFF); a=a>>8;
  }
  return (int32_t) fwrite(b,sizeof(char),n,fp);
}

/** Write EDF main headers and 'NrOfSignals' signal headers to file 'fp'
 *  @return number of characters written
*/
int32_t edfWriteHeaders(FILE *fp, edfMainHdr_t *mHdr, edfSignalHdr_t *sHdr, 
 int32_t NrOfSignals) {
 
  size_t nc=0;
  int32_t i,j;
  
  /* write main header */ 
  nc+=fwrite(mHdr,sizeof(char),sizeof(edfMainHdr_t),fp);
  for (j=0; j<10; j++) {
    /* write all signals headers item by item */
    for (i=0; i<NrOfSignals; i++) {
      switch (j) {
        case 0: nc+=fwrite(sHdr[i].Label            ,1,16,fp); break;
        case 1: nc+=fwrite(sHdr[i].TransducerType   ,1,80,fp); break;
        case 2: nc+=fwrite(sHdr[i].PhysicalDimension,1, 8,fp); break;
        case 3: nc+=fwrite(sHdr[i].PhysicalMin      ,1, 8,fp); break;
        case 4: nc+=fwrite(sHdr[i].PhysicalMax      ,1, 8,fp); break;
        case 5: nc+=fwrite(sHdr[i].DigitalMin       ,1, 8,fp); break;
        case 6: nc+=fwrite(sHdr[i].DigitalMax       ,1, 8,fp); break;
        case 7: nc+=fwrite(sHdr[i].Prefiltering     ,1,80,fp); break;
        case 8: nc+=fwrite(sHdr[i].NrOfSamples      ,1, 8,fp); break;
        case 9: nc+=fwrite(sHdr[i].Reserved         ,1,32,fp); break;
        default:
          break;
      }
    }
  }
  return (int32_t) nc;
}
 
/** write BDF (=EDF with 24 iso 16 bits samples) headers to file 'fp'
 *   for continues ExG recording with maximum of 'nch' channel definitions in 'chd', 
 *   channel selection 'cs', person 'id', message 'record', channel names 'chn_name' 
 *   and record duration 'dt' [s].
 * @return bytes written to 'fp'
*/
int32_t edfWriteHdr(FILE *fp, tms_channel_data_t *chd, int32_t nch, int32_t cs,
 const char *id, const char *record, time_t *t, double dt, const char **chn_name) {

  edfMainHdr_t   mHdr;
  edfSignalHdr_t sHdr[9];    /**< maximum of channels + switch channel */
  int32_t  ach=0;            /**< number of active channels */ 
  int32_t  j;                /**< channel index */
  double   uVpb =0.0122150;  /**< uV per bit channel A,B,C and D */
  double   uVaux=0.2384186;  /**< uV per bit channel E,F,H,G */
  int32_t  digMax=(1<<23);   /**< digital value -digMax ... digMax-1 */

  ach=0;
  for (j=0; j<nch; j++) {
    if (cs&(1<<j)) { ach++; }
  }
  
  edfFillMainHdr(&mHdr,1,id,record,"TMSi",t,-1,dt,ach);
  /* make all signal header for all active channels */
  j=0;
  if (cs&0x01) {
    edfFillSignalHdr(&sHdr[j],chn_name[0],"passive wet AgCl electrode","uV",
     -uVpb*digMax,uVpb*(digMax-1),-digMax,digMax-1,"None",chd[0].ns);
    j++;
  }
  if (cs&0x02) {
    edfFillSignalHdr(&sHdr[j],chn_name[1],"passive wet AgCl electrode","uV",
     -uVpb*digMax,uVpb*(digMax-1),-digMax,digMax-1,"None",chd[1].ns);
    j++;
  }
  if (cs&0x04) {
    edfFillSignalHdr(&sHdr[j],chn_name[2],"passive disposable AgCl electrode","uV",
     -uVpb*digMax,uVpb*(digMax-1),-digMax,digMax-1,"None",chd[2].ns);
    j++;
  }
  if (cs&0x08) {
    edfFillSignalHdr(&sHdr[j],chn_name[3],"passive disposable AgCl electrode","uV",
     -uVpb*digMax,uVpb*(digMax-1),-digMax,digMax-1,"None",chd[3].ns);
    j++;
  }
  if (cs&0x10) {
    edfFillSignalHdr(&sHdr[j],chn_name[4],"passive AgCl electrode","uV",
     -uVaux*digMax,uVaux*(digMax-1),-digMax,digMax-1,"None",chd[4].ns);
    j++;
  }
  if (cs&0x20) {
    edfFillSignalHdr(&sHdr[j],chn_name[5],"termokoppel","uV",
     -uVaux*digMax,uVaux*(digMax-1),-digMax,digMax-1,"None",chd[5].ns);
    j++;
  }
  if (cs&0x40) {
    edfFillSignalHdr(&sHdr[j],chn_name[6],"resistor belt","uV",
     -uVaux*digMax,uVaux*(digMax-1),-digMax,digMax-1,"None",chd[6].ns);
    j++;
  }
  if (cs&0x80) {
    edfFillSignalHdr(&sHdr[j],chn_name[7],"Light Dependant Resistor","uV",
     -uVaux*digMax,uVaux*(digMax-1),-digMax,digMax-1,"None",chd[7].ns);
    j++;
  }
  if (cs&0x100) {
    edfFillSignalHdr(&sHdr[j],chn_name[8],"virtual","-",
     -uVpb*digMax,uVpb*(digMax-1),-digMax,digMax-1,"None",chd[8].ns);
    j++;
  }
  if (cs&0x200) {
    edfFillSignalHdr(&sHdr[j],chn_name[9],"virtual","-",
     -uVpb*digMax,uVpb*(digMax-1),-digMax,digMax-1,"None",chd[9].ns);
    j++;
  }
  if (cs&0x400) {
    edfFillSignalHdr(&sHdr[j],chn_name[10],"virtual","-",
     -uVpb*digMax,uVpb*(digMax-1),-digMax,digMax-1,"None",chd[10].ns);
    j++;
  }
  if (cs&0x800) {
    edfFillSignalHdr(&sHdr[j],chn_name[11],"virtual","-",
     -uVpb*digMax,uVpb*(digMax-1),-digMax,digMax-1,"None",chd[11].ns);
    j++;
  }
  if (cs&0x1000) {
    edfFillSignalHdr(&sHdr[j],chn_name[12],"button","-",
     -uVpb*digMax,uVpb*(digMax-1),-digMax,digMax-1,"None",chd[12].ns);
    j++;
  }

  /* write EDF/BDF main and 'j' signal headers */
  return(edfWriteHeaders(fp,&mHdr,sHdr,j));
}


/** write samples in 'chd' to EDF (bdf==0) or BDF (bdf==1) file 'fp' with
 *   channel selection switch 'cs' and 'mpc' missed packets before this 'chd'.
 * @return number of bytes written
*/
int32_t edfWriteSamples(FILE *fp, int32_t bdf, tms_channel_data_t *chd,
 int32_t cs, int32_t mpc) {

  int32_t i,j,k;
  int32_t nc=0;
  int32_t sample=0;
  int32_t sampleSize=2;
  
  if (bdf==1) { sampleSize=3; } /* BDF files has 3 bytes samples */
  
  for (k=mpc; k>0; k--) {
    /* Check all active channels (0x00FF) and switch channel (0x1000) */
    for (j=0; j<tms_get_number_of_channels(); j++) {
      /* is channel 'j' active? */
      if (cs&(1<<j)) {
        sample=chd[j].data[0].isample;
        for (i=0; i<chd[j].ns; i++) {
          nc+=edfWriteSample(fp,sample,sampleSize);
        }
      }
    }
  }
  
  /* Check all active channels (0x00FF) and switch channel (0x1000) */
  for (j=0; j<tms_get_number_of_channels(); j++) {
    /* is channel 'j' active? */
    if (cs&(1<<j)) {
      /* !!! how to handle sample overflow */
      for (i=0; i<chd[j].ns; i++) {
        if (i<chd[j].rs) { sample=chd[j].data[i].isample; }
        nc+=edfWriteSample(fp,sample,sampleSize);
      }
    }
  }
  return(nc);
}

/** Construct tms_channel_data_t out of 'edf' header info.
 * @return pointer to channel_data_t struct, NULL on failure.
 */
tms_channel_data_t *edf_alloc_channel_data(edf_t *edf) {

  int32_t i;                 /**< general index */
  tms_channel_data_t *chd;   /**< channel data block pointer */
  int32_t mspr=0;            /**< maximum samples per record */
  int32_t scale=1;           /**< scale factor for samples per record */

  /* allocate storage space for all channels */
  chd=(tms_channel_data_t *)calloc(edf->NrOfSignals,sizeof(tms_channel_data_t));
  for (i=0; i<edf->NrOfSignals; i++) {
    if (mspr < edf->signal[i].NrOfSamplesPerRecord) {
      mspr = edf->signal[i].NrOfSamplesPerRecord;
    }
  }
  scale = (mspr+15)/16;
  for (i=0; i<edf->NrOfSignals; i++) {
    chd[i].ns= edf->signal[i].NrOfSamplesPerRecord/scale;
    /* reset sample counter */
    chd[i].sc=0;
    /* allocate space for data samples */
    chd[i].data=(tms_data_t *)calloc(chd[i].ns,sizeof(tms_data_t));
    /* tick duration of data sample of channel 'i' */
    chd[i].td= edf->RecordDuration / edf->signal[i].NrOfSamplesPerRecord;
  }
  return(chd);
}

/** Read one block of channel data from EDF/BDF data structure 'edf'
 *   into tms channel data structure 'chn'.
 * @return number of samples read, 0 on failure.
 */
int32_t edf_rd_chn(edf_t *edf, tms_channel_data_t *chn) {

  int32_t i,j;               /**< general index */
  int32_t cnt=0;             /**< total sample counter */
  int32_t isample;           /**< integer sample value */
  int32_t *dc_cnt;           /**< DC counter per channel */
  int32_t missing=0;         /**< missing packet detected */
  static int32_t pre_miss=0; /**< previous packet was missing */
  static int32_t miss_cnt=0; /**< missing packet counter */
  static double ts=0.0;      /**< start time of missing packets */
  static double dt=0.0;      /**< duration of missing packets */

  dc_cnt = alloca( edf->NrOfSignals * sizeof(dc_cnt[0]) );

  /* read edf samples for all signals */
  for (i=0; i<edf->NrOfSignals; i++) {
    chn[i].rs=0;
    if (chn[i].sc < edf->signal[i].NrOfSamples) {
      /* reset DC counter of signal 'i' */
      dc_cnt[i]=0;
      /* read all samples of signal 'i' */
      for (j=0; j<chn[i].ns; j++) {
        /* get current integer sample value of signal 'i'*/
        isample=edf->signal[i].data[chn[i].sc+j];
        chn[i].data[j].isample=isample;
        /* convert to real sample value */
        chn[i].data[j].sample=(float) ( isample *
          (edf->signal[i].PhysicalMax - edf->signal[i].PhysicalMin) /
          (edf->signal[i].DigitalMax - edf->signal[i].DigitalMin)
		  );
        /* count number of times first sample equals all other samples */
        if (isample==chn[i].data[0].isample) {
          dc_cnt[i]++;
        }
      }
      /* admin converted isamples */
      chn[i].rs=chn[i].ns;
      cnt+=chn[i].ns;
      chn[i].sc+=chn[i].ns;
    }
  }

  /* check if all channels have equal samples */
  if (dc_cnt[0]>1) { missing=0x02; } else { missing=0x00; }
  for (i=0; i<edf->NrOfSignals; i++) {
    /* check channel 'i' */
    if (dc_cnt[i]!=chn[i].ns) { missing=0; }
  }

  /* print timing of missing packets */
  if (pre_miss) {
    if (missing==0x02) {
      dt+=chn[0].ns*chn[0].td;
    } else {
      miss_cnt++;
      fprintf(stderr,"# Warning %d : missing packet(s) at %9.3f [s] duration %6.3f [s]\n",
               miss_cnt,ts,dt);
    }
  } else {
    if (missing==0x02) {
      ts=chn[0].sc*chn[0].td; dt=chn[0].ns*chn[0].td;
    }
  }

  /* flag all samples of this packet */
  for (i=0; i<edf->NrOfSignals; i++) {
    /* flag all samples with 0x02 'delta overflow' in case of missing */
    for (j=0; j<chn[i].ns; j++) { chn[i].data[j].flag=missing; }
  }

  /* current missing is previous misssing at next call */
  pre_miss=(missing==0x02);

  return(cnt);
}

int32_t BtWriteBytes( int32_t fd, const uint8_t * buf, int32_t count )
{
#ifdef _MSC_VER
  count=send(fd,(const char *)buf,count,0);
#else
  count=write(fd,buf,count);
#endif
  return count;
}

int32_t BtReadBytes( int fd, uint8_t * buf, int count )
{
#ifdef _MSC_VER
  count=recv(fd,(char *)buf,count,0);
#else
  count=read(fd,buf,count); 
#endif
  return count;
}
