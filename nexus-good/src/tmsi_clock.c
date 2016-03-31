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

#ifdef _MSC_VER
  #include "win32_compat.h"
  #include "stdint_win32.h"
#else
  #include <stdint.h>
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <signal.h>

#include "tmsi.h"
#include "tmsi_bluez.h"
#include "edf.h"

#define MNCN        (1024)  /**< Maximum number of characters in file name */
#define SECDEF         (0)  /**< Default clock offset */
#define RSTDEF         (0)  /**< Default clock reset 0:off 1:on */

#define VERSION "$Revision: 0.1 $ $Date: 2010/07/10 18:30:00 $"

int32_t dbg=0x0000;  /**< debug level */
int32_t  vb=0x0000;  /**< verbose level */

/** Evaluate shell variables in path name 'a' into 'b'
 * @return path depth before evaluation
*/
int32_t get_path(char *b, const char *a) {

  char tmp[MNCN];
  char *dir;
  char *chk;
  int32_t i=0;
  
  strcpy(tmp,a); dir=strtok(tmp,"/"); b[0]='\0';
  while (dir!=NULL) {
    if (dir[0]=='$') {
      dir++; /* skip first character */
      chk=getenv(dir);
      if (chk!=NULL) {
        strcat(b,getenv(dir));
      }
    } else {
      strcat(b,"/"); strcat(b,dir);
    }
    dir=strtok(NULL,"/"); i++;
  }
  return(i);  
}

/** Print usage to file 'fp'.
 *  @return number of printed characters.
*/
int32_t tmsi_clock_intro(FILE *fp) {
  
  int32_t nc=0;
  int32_t i;
  
  nc+=fprintf(fp,"tmsi_clock: %s\n",VERSION); 
  nc+=fprintf(fp,"Usage: tmsi_clock [-a <bta>] [-s <sec>] [-R <rst>] [-v <vb>] [-d <dbg>] [-h]\n");
  nc+=fprintf(fp,"  Press CTRL+c to stop capturing bio-data\n");
  nc+=fprintf(fp,"bta  : bluetooth address (default=%s)\n","$NEXUS_BADR");
  nc+=fprintf(fp,"sec  : clock offset [s] (default=%d)\n",SECDEF);
  nc+=fprintf(fp,"rst  : clock reset (0:off 1:on) (default=%d)\n",RSTDEF);
  nc+=fprintf(fp,"vb   : verbose switch (default=0x%02X)\n",vb);
  nc+=fprintf(fp,"dbg  : debug value (default=0x%02X)\n",dbg);
  return(nc);
}

void parse_cmd(int32_t argc, char *argv[], char *iname, int32_t *sec, int32_t *rst) {

  int32_t i;          /**< general index */

  /* use TMSI as default input device */
  get_path(iname,"$NEXUS_BADR");
  if (strlen(iname)<2) {
    fprintf(stderr,"# Warning: missing shell variable $NEXUS_BADR !!!\n");
  }
  fprintf(stderr,"# NEXUS_BADR %s\n",iname);
  
  *sec=SECDEF; *rst=RSTDEF;
  
  for (i=1; i<argc; i++) {
    if (argv[i][0]!='-') {
      printf("missing - in argument %s\n",argv[i]);
    } else {
      switch (argv[i][1]) {
        case 'a': strcpy(iname,argv[++i]); break;
        case 's': *sec=strtol(argv[++i],NULL,0); break;
        case 'R': *rst=strtol(argv[++i],NULL,0); break;
        case 'v': vb=strtol(argv[++i],NULL,0); break;
        case 'd': dbg=strtol(argv[++i],NULL,0); break;
        case 'h': tmsi_clock_intro(stderr); exit(0); break;
        default : fprintf(stderr,"can't understand argument %s\n",argv[i]);
      }
    }
  }
}

/** Read RealTime Clock 'rtc' of Nexus device 'fd'
 * @return number of bytes read
*/
int32_t get_rtc(int32_t fd, time_t *rtc) {
 
  char    msg[MNCN];   /**< message buffer */ 
  int32_t bw,br;       /**< byte written, read */

  /* get Nexus/Mobi internal clock */
  bw=tms_send_rtc_time_read_req(fd);
  /* get response */
  br=tms_rcv_msg(fd,msg,MNCN); 
  /* convert to struct */
  tms_get_rtc(msg,br,rtc);

  return(br);
}


/** Put RealTime Clock 'rtc' to Nexus device 'fd'
 * @return acknowlegde status 0 == success
*/
int32_t put_rtc(int32_t fd, time_t *rtc) {
 
  char    msg[MNCN];   /**< message buffer */ 
  int32_t bw,br;       /**< byte written, read */
  int32_t type;        /**< response type */

  /* send 'rtc' packet */ 
  bw=tms_write_rtc(fd,rtc);
  /* get response */
  br=tms_rcv_msg(fd,msg,MNCN); 
  /* check checksum and get type of response */
  if (tms_chk_msg(msg,br)!=0) {
    fprintf(stderr,"# Error: put_rtc checksum error !!!\n");
  } else {
    type=tms_get_type(msg,br);
    if (type!=0) {
      fprintf(stderr,"# Error: put_rtc setting Real Time Clock 0x%02X\n",type);
    }
  }

  return(type);
}



int32_t main(int32_t argc, char *argv[]) {

  int32_t fd;            /**< bluetooth socket file descriptor */
  char    iname[MNCN];   /**< bluetooth address */
  int32_t sec;           /**< clock shift [s] */
  int32_t rst;           /**< clock reset 0:off 1:on */
  double  tshift;           
  time_t  rtc,now,rtc1;  /**< TMS RealTime Clock, now and new */
  int32_t br;            /**< bytes read */
  int32_t rv;            /**< response value */
   
  /* parse command line arguments */
  parse_cmd(argc,argv,iname,&sec,&rst);
  
  /* open bluetooth socket */
  if ((fd=tms_open_port(iname))<0) {
    fprintf(stderr,"# Error: Can't connect to Nexus %s\n",iname);
    exit(-1);
  }
  
  time(&now);

  /* get RealTime Clock */
  br=get_rtc(fd,&rtc);
  if (br>0) {
    /* print rtc struct */
    tms_prt_rtc(stderr,&rtc,1,1);
    /* print now struct */
    tms_prt_rtc(stderr,&now,2,0);
    tshift = difftime(now, rtc);
    fprintf(stderr,"# Info: time difference local PC - Nexus time: %.1f\n",  tshift);
    if (rst==1) {
      sec = (int32_t)round(tshift);
    }

    if (sec!=0) {
      /* shift real time clocks 'sec' [s] */
      rtc += sec;
      
      /* put new RealTime Clock */
      rv=put_rtc(fd,&rtc);
      
      /* get current RealTime Clock */
      br=get_rtc(fd,&rtc1);
      /* print current rtc struct */
      tms_prt_rtc(stderr,&rtc1,3,0);
    }
  }

  /* shutdown bluetooth capture */
  tms_shutdown();  
  /* close bluetooth socket */
  tms_close_port(fd);

  return(0);
}
