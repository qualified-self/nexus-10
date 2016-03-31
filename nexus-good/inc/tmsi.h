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
# include "stdint_win32.h"
#else
# include <stdint.h>
#endif

#include <stdio.h>
#include <time.h>

#include "edf.h"

#ifndef TMSI_H
#define TMSI_H

#ifdef __cplusplus
extern "C" {
#endif
/* EDF/BDF definitions are at the tail of this file */

#define MNCN  (1024)  /**< maximum characters in filename */

#define TMSCFGSIZE          (1024)

typedef struct TMS_ACKNOWLEDGE_T {
   uint16_t descriptor;    
     /**< received blockdescriptor (type+size) being acknowledged */
   uint16_t errorcode;
     /**<   numbers 0x0000 - 0x0010 are reserved for system errors
     * 0x00 - no error, positive acknowledge
     * 0x01 - unknown or not implemented blocktype
     * 0x02 - CRC error in received block
     * 0x03 - error in command data (can't do that)
     * 0x04 - wrong blocksize (too large)
     *   numbers 0x0010 - 0xFFFF are reserved for user errors
     * 0x11 - No external power supplied
     * 0x12 - Not possible because the Front is recording
     * 0x13 - Storage medium is busy
     * 0x14 - Flash memory not present
     * 0x15 - nr of words to read from flash memory out of range
     * 0x16 - flash memory is write protected
     * 0x17 - incorrect value for initial inflation pressure
     * 0x18 - wrong size or values in BP cycle list
     * 0x19 - sample frequency divider out of range (<0, >max)
     * 0x1A - wrong nr of user channels (<=0, >maxUSRchan)
     * 0x1B - adress flash memory out of range
     * 0x1C - Erasing not possible because battery low  */
} tms_acknowledge_t, *ptms_acknowledge_t;


/** Frontend system info struct */
typedef struct TMS_FRONTENDINFO_T {
  uint16_t nrofuserchannels;  /**< nr of channels set by host (<=nrofswchannels and >0)
                               * first 'nrofuserchannels' channels of system
                               *  will be sent by frontend (only when supported by frontend software!) */
  uint16_t currentsampleratesetting;
    /**< When imp.mode, then only effect when stopping the impedance mode (changing to other mode)
    * 0 = base sample rate (when supported by hardware)
    * 1 = base sample rate /2 (when supported by hardware)
    * 2 = base sample rate /4 (when supported by hardware)
    * 3 = base sample rate /8 (when supported by hardware)
    * 4 = base sample rate /16 (when supported by hardware) */
  uint16_t mode;
    /**< bit 0.. 7 is status bits active low
    * bit 8..15 is mask bits active low
    * bit 0 = datamode    0 = normal, Channel data send enabled
    *                     1 = nodata, Channel data send disabled
    * bit 1 = storagemode 0 = storage on  (only if supported by frontend hardware/software)
    *                     1 = storage off */
    /** last 13 uint16_t have valid values only from frontend to PC */
  uint16_t maxRS232;          /**< Maximum RS232 send frequentie in Hz */
  uint32_t serialnumber;      /**< System serial number, low uint16_t first */
  uint16_t nrEXG;             /**< nr of EXG (unipolar) channels */
  uint16_t nrAUX;             /**< nr of BIP and AUX channels */
  uint16_t hwversion;         /**< frontend hardware version number
                               * hundreds is major part, ones is minor */
  uint16_t swversion;         /**< frontend software version number
                               * hundreds is major part, ones is minor */
  uint16_t cmdbufsize;        /**< number of uint16_ts in frontend receive buffer */
  uint16_t sendbufsize;       /**< number of uint16_ts in frontend send buffer */
  uint16_t nrofswchannels;    /**< total nr of channels in frontend */
  uint16_t basesamplerate;    /**< base sample frequency (in Hz) */
  /** power and  hardwarecheck not implemented yet, for future use, send 0xFFFF */
  uint16_t power; 
    /**< bit 0 - line power detected
    * bit 1 - battery detected
    * bit 2 - RTC battery detected
    * bit 3 - line power low
    * bit 4 - battery low
    * bit 5 - RTC battery low */
  uint16_t hardwarecheck;
    /**< bit 0 vext_error   = 0x0001   'POSTcode'
    * bit 1 vbat_error   = 0x0002    P ower
    * bit 2 vRTC_error   = 0x0004     O n
    * bit 3 BPM_error    = 0x0008      S elf
    * bit 4 UART_error   = 0x0010       T est
    * bit 5 ADC_error    = 0x0020        code
    * bit 6 PCM_error    = 0x0040
    * bit 7 extmem_error = 0x0080 */
} tms_frontendinfo_t, *ptms_frontendinfo_t;

#define TMSFRONTENDINFOSIZE (sizeof(tms_frontendinfo_t))


typedef struct TMS_IDREADREQ_T {
  uint16_t startaddress;    /**< start adress in de buffer with ID data;
                             * 4 MSB bits Device number */
  uint16_t length;        /**< amount of words requested;
                           * length <= max_length = 0x80 = 128 words */
} tms_idreadreq_t, *ptms_idreadreq_t;


typedef struct TMS_TYPE_DESC_T {
  uint16_t Size;      /**< Size in words of this structure */
  uint16_t Type;      /**< Channel type id code
                       *     0 UNKNOWN
                       *     1 EXG
                       *     2 BIP
                       *     3 AUX
                       *     4 DIG
                       *     5 TIME
                       *     6 LEAK
                       *     7 PRESSURE
                       *     8 ENVELOPE
                       *     9 MARKER
                       *     10 ZAAG
                       *     11 SAO2  */
  uint16_t SubType; /**< Channel subtype
                     * (+256: unipolar reference, +512: impedance reference)
                     *  .0 Unknown
                     *  .1 EEG
                     *  .2 EMG
                     *  .3 ECG
                     *  .4 EOG
                     *  .5 EAG
                     *  .6 EGG
                     *  .257 EEGREF  (for specific unipolar reference)
                     *  .10 resp
                     *  .11 flow
                     *  .12 snore
                     *  .13 position
                     *  .522 resp/impref (impedance reference)
                     *  .20 SaO2
                     *  .21 plethysmogram
                     *  .22 heartrate
                     *  .23 sensor status
                     *  .30 PVES
                     *  .31 PURA
                     *  .32 PABD
                     *  .33 PDET */
  uint16_t Format; /**< Format id
                    *  0x00xx xbit unsigned
                    *  0x01xx xbit signed
                    * examples:
                    *  0x0001 1bit unsigned
                    *  0x0101 1bit signed
                    *  0x0002 2bit unsigned
                    *  0x0102 2bit signed
                    *  0x0008 8bit unsigned
                    *  0x0108 8bit signed */
  float a;  /**< Information for converting bits to units: */
  float b;  /**< Unit  = a * Bits  + b ; */
  uint8_t UnitId; /**< Id identifying the units
                   *  0 bit (no unit) (do not use with front6)
                   *  1 Volt
                   *  2 %
                   *  3 Bpm
                   *  4 Bar
                   *  5 Psi
                   *  6 mH2O
                   *  7 mHg
                   *  8 bit  */
  int8_t Exp;  /**< Unit exponent, 3 for Kilo, -6 for micro, etc. */
} tms_type_desc_t, *ptms_type_desc_t; 


typedef struct TMS_CHANNEL_DESC_T {
  tms_type_desc_t Type;  /**< channel type descriptor */
  char *ChannelDescription;  /**< String pointer identifying the channel */
  float  GainCorrection;  /**< Optional gain correction */
  float  OffsetCorrection; /**< Optional offset correction */
} tms_channel_desc_t, *ptms_channel_desc_t;  /**< Size = 6 words */


typedef struct TMS_INPUT_DEVICE_T {
  uint16_t Size;         /**< Size of this structure in words (device not present: send 2) */
  uint16_t Totalsize;    /**< Total size ID data from this device in words (device not present: send 2) */
  uint32_t SerialNumber; /**< Serial number of this input device */
  uint16_t Id;           /**< Device ID */
  char    *DeviceDescription;  /**< String pointer identifying the device */
  uint16_t NrOfChannels;  /**< Number of channels of this input device */
  uint16_t DataPacketSize;  /**< Size simple PCM data packet over all channels */
  tms_channel_desc_t *Channel;  /**< Pointer to all channel descriptions */
} tms_input_device_t, *ptms_input_device_t;  

typedef struct TMS_VLDELTA_INFO_T {
  uint16_t Config;         /**< Config&0x0001 -> 0: original VLDelta encoding 1: Huffman */
                           /**< Config&0x0100 -> 0: same 1: different */
  uint16_t Length;         /**< Size [bits] of the length block before a delta sample */
  uint16_t TransFreqDiv;   /**< Transmission frequency divisor Transfreq = MainSampFreq / TransFreqDiv */
  uint16_t NrOfChannels;  
  uint16_t *SampDiv;       /**< Real sample frequence divisor per channel*/
} tms_vldelta_info_t, *ptms_vldelta_info_t;   

typedef struct TMS_DATA_T {
  float   sample;    /**< real sample value */
  int32_t isample;   /**< integer representation of sample */
  int32_t flag;      /**< sample status: 0x00: ok 0x01: overflow */
} tms_data_t, *ptms_data_t;   

typedef struct TMS_CHANNEL_DATA_T {
  int32_t      ns;   /**< number of samples in 'data' */
  int32_t      rs;   /**< already received samples in 'data' */
  int32_t      sc;   /**< sample counter */
  double       td;   /**< tick duration [s] */
  tms_data_t *data;  /**< data samples */
} tms_channel_data_t, *ptms_channel_data_t;   

/** TMS storage type struct */
typedef struct TMS_STORAGE_T {
  int8_t  ref;       /**< reference channel nr. 0...63 and -1 none */
  int8_t  deci;      /**< decimation 0,1,3,7,15,63,127 or 255 */
  int8_t  delta;     /**< delta 0:no storage 1: 8 bit delta 2: 16 bit delta 3: 24 bit data */
  int8_t  shift;     /**< shift delta==3 -> 0, delta==2 -> 0..6, delta==1 -> 0..14 */
  int32_t period;    /**< sample period */
  int32_t overflow;  /**< overflow value */
} tms_storage_t, *ptms_storage_t;

/** TMS config struct */
typedef struct TMS_CONFIG_T {
  int16_t version;       /**< PC Card protocol version number 0x0314 */
  int16_t hdrSize;       /**< size of measurement header 0x0200 */
  int16_t fileType;      /**< File Type (0: .ini 1: .smp 2:evt) */
  int32_t cfgSize;       /**< size of config.ini  0x400         */
  int16_t sampleRate;    /**< File Type (0: .ini 1: .smp 2:evt) */
  int16_t nrOfChannels;  /**< number of channels */
  int32_t startCtl;      /**< start control       */
  int32_t endCtl;        /**< end control       */
  int16_t cardStatus;    /**< card status */
  int32_t initId;        /**< Initialisation Identifier */
  int16_t sampleRateDiv; /**< Sample Rate Divider */
  int16_t mindecimation; /**< Minimum Decimantion of all channels */
  tms_storage_t storageType[64]; /**< Storage Type */
  uint8_t fileName[12];  /**< Measurement file name */
  time_t  alarmTime;     /**< alarm time */
  uint8_t info[700];     /**< patient of measurement info */
} tms_config_t, *ptms_config_t;
 
/** TMS measurement header struct */
typedef struct TMS_MEASUREMENT_HDR_T {
  int32_t nsamples;         /**< number of samples in this recording */
  time_t  startTime;        /**< start time */
  time_t  endTime;          /**< end time */
  int32_t frontendSerialNr; /**< frontendSerial Number */
  int16_t frontendHWNr;     /**< frontend Hardware version Number */
  int16_t frontendSWNr;     /**< frontend Software version Number */
} tms_measurement_hdr_t, *ptms_measurement_hdr_t;

#define TMS32SIGNALNAMESIZE   (41)
#define TMS32UNITNAMESIZE     (11)

/** Poly5/TMS32 signal description struct */
typedef struct TMS_SIGNAL_DESC32_T {
  char    signalName[TMS32SIGNALNAMESIZE];    /**< signal name */
  char    unitName[TMS32UNITNAMESIZE];        /**< unit name */
  float   unitLow;          /**< Unit Low */
  float   unitHigh;         /**< Unit High */
  float   ADCLow;           /**< ADC Low */
  float   ADCHigh;          /**< ADC High */
  int32_t index;            /**< index in signal list */
  int32_t cacheOffset;      /**< cache offset */
  int32_t sampleSize;       /**< sample size [Byte] */
  int32_t zeroCnt;          /**< count number of zero samples */
} tms_signal_desc32_t, *ptms_signal_desc32_t;

#define TMS32IDENTIFIERSIZE        (32)
#define TMS32MEASUREMENTNAMESIZE   (81)

/** Poly5/TMS32 sample file header struct */
typedef struct TMS_HDR32_T {
  char    identifier[TMS32IDENTIFIERSIZE];       /**< File identifier */
  int32_t version;                               /**< version number */
  char    measurement[TMS32MEASUREMENTNAMESIZE]; /**< measurement name */
  int32_t sampleRate;       /**< Sample Rate [Hz] */
  int32_t storageRate;      /**< Storage rate: lower or equal to 'SampleFreq' */
  int32_t storageType;      /**< Storage type: 0=Hz 1=Hz^-1 */
  int32_t nrOfSignals;      /**< Number of signals stored in sample file */
  int32_t totalPeriods;     /**< Total number of sample periods */
  time_t  startTime;        /**< start time */
  int32_t nrOfBlocks;       /**< number of sample blocks */
  int32_t periodsPerBlock;  /**< Sample periods per Sample Data Block (multiple of 16) */
  int32_t blockSize;        /**< Size of sample data in one Block [byte] header not include */
  int32_t deltaCompression; /**< Delta compression flag (not used: must be zero) */
} tms_hdr32_t, *ptms_hdr32_t;

/** Poly5/TMS32 sample file block admin struct */
typedef struct TMS_BLOCK_DESC32_T {
  int32_t idx;              /**< block index */
  time_t  then;             /**< block creation time */
} tms_block_desc32_t, *ptms_block_desc32_t;

/** float union for translation to float and visa versa */
typedef union TMS_FLOAT_T {
  uint8_t   ch[4]; /**< byte representation of float 'a' */ 
  float     fl;    /**< float 'a' */
  int32_t   i;     /**< integer representation of float 'a' */
} tms_float_t, *ptms_float_t;

/** set verbose level of module TMS to 'new_vb'.
 * @return old verbose value
*/
int32_t tms_set_vb(int32_t new_vb);

/** get verbose variable for module TMS
 * @return current verbose level
*/
int32_t tms_get_vb();

/** Get integer of 'n' bytes from byte array 'msg' starting at position 's'.
 * @note n<=4 to avoid bit loss
 * @note on return start position 's' is incremented with 'n'.
 * @return integer value.
 */
int32_t tms_get_int(uint8_t *msg, int32_t *s, int32_t n);

/** Get current time in [sec] since 1970-01-01 00:00:00.
 * @note current time has micro-seconds resolution.
 * @return current time in [sec].
*/
double get_time();

/** Get current date and time into 'datetime'
 * @note current time has micro-seconds resolution.
 * @return current time in [sec].
*/
double get_date_time(char *datetime, int32_t size);

/** General check of TMS message 'msg' of 'n' bytes.
 * @return 0 on correct checksum, 1 on failure.
*/
int32_t tms_chk_msg(uint8_t *msg, int32_t n);

/** Check checksum buffer 'msg' of 'n' bytes.
 * @return packet type.
*/
int16_t tms_get_type(uint8_t *msg, int32_t n);

/** Read at max 'n' bytes of TMS message 'msg' for 
 *   bluetooth device descriptor 'fd'.
 * @return number of bytes read.
*/
int32_t tms_rcv_msg(int fd, uint8_t *msg, int32_t n);

/** Convert buffer 'msg' of 'n' bytes into tms_acknowledge_t 'ack'.
 * @return >0 on failure and 0 on success 
*/
int32_t tms_get_ack(uint8_t *msg, int32_t n, tms_acknowledge_t *ack);

/** Print tms_acknowledge_t 'ack' to file 'fp'.
 *  @return number of printed characters.
*/
int32_t tms_prt_ack(FILE *fp, tms_acknowledge_t *ack);

/** Send frontend Info request to 'fd'.
 *  @return bytes send.
*/
int32_t tms_snd_FrontendInfoReq(int32_t fd);

/** Write frontendinfo_t 'fei' into socket 'fd'.
 * @return number of bytes written (<0: failure)
*/
int32_t tms_write_frontendinfo(int32_t fd, tms_frontendinfo_t *fei);

/** Convert buffer 'msg' of 'n' bytes into frontendinfo_t 'fei'
 * @note 'b' needs size of TMSFRONTENDINFOSIZE
 * @return -1 on failure and on succes number of frontendinfo structs
*/
int32_t tms_get_frontendinfo(uint8_t *msg, int32_t n, tms_frontendinfo_t *fei);

/** Print tms_frontendinfo_t 'fei' to file 'fp'.
 *  @return number of printed characters.
*/
int32_t tms_prt_frontendinfo(FILE *fp, tms_frontendinfo_t *fei, int nr, int hdr);

int32_t tms_fetch_iddata(int32_t fd, uint8_t *msg, int32_t n);
int32_t tms_get_iddata(uint8_t *msg, int32_t n, tms_input_device_t *inpdev);
int32_t tms_get_input_device(uint8_t *msg, int32_t n, int32_t start, tms_input_device_t *inpdev);
int32_t tms_prt_iddata(FILE *fp, tms_input_device_t *inpdev);

/** Construct channel data block with frontend info 'fei' and
 *   input device 'dev' with eventually vldelta_info 'vld'.
 * @return pointer to channel_data_t struct, NULL on failure.
*/
tms_channel_data_t *tms_alloc_channel_data();

/** free channel data block previously allocated with tms_alloc_channel_data()
 */
void tms_free_channel_data(tms_channel_data_t *chd);


/** Print channel data block 'chd' of tms device 'dev' to file 'fp'.
 * @param print switch md 0: integer  1: float values
 * @return number of printed characters.
*/
int32_t tms_prt_channel_data(FILE *fp, tms_channel_data_t *chd, int32_t chn_cnt, int32_t md);

/** Get TMS data from message 'msg' of 'n' bytes into floats 'val'.
 * @return number of samples.
 */
int32_t tms_get_data(uint8_t *msg, int32_t n, tms_input_device_t *dev, 
    tms_channel_data_t *chd);

/** Flag all samples in 'channel' with 'flg'.
 * @return always 0
*/
int32_t tms_flag_samples(tms_channel_data_t *chn, int32_t chn_cnt, int32_t flg);

/** Print TMS channel data 'chd' to file 'fp'.
 * @param md: print switch per channel 1: float 0: integer values 
 *            A: 0x01 B: 0x02 C: 0x04 ...
 * @param cs: channel selector A: 0x01 B: 0x02 C: 0x04 ...
 * @param nc: number of channels: i.e. tms_get_number_of_channels()
 * @return number of characters printed.
 */
int32_t tms_prt_samples(FILE *fp, tms_channel_data_t *chd, int32_t cs, int32_t md, int32_t nc);

/** Send VLDeltaInfo request to file descriptor 'fd'
*/
int32_t tms_snd_vldelta_info_request(int32_t fd);

/** Convert buffer 'msg' of 'n' bytes into tms_vldelta_info_t 'vld' for 'nch' channels.
 * @return number of bytes processed.
*/
int32_t tms_get_vldelta_info(uint8_t *msg, int32_t n, int32_t nch, tms_vldelta_info_t *vld);

/** Print tms_prt_vldelta_info 'vld' to file 'fp'.
 *  @return number of printed characters.
*/
int32_t tms_prt_vldelta_info(FILE *fp, tms_vldelta_info_t *vld, int nr, int hdr);

/** Send keepalive request to 'fd'.
 *  @return bytes send.
*/
int32_t tms_snd_keepalive(int32_t fd);

/** Send Real Time Clock (RTC) read request to file descriptor 'fd'
*  @return number of bytes send.
*/
int32_t tms_send_rtc_time_read_req(int32_t fd);

/** Convert buffer 'msg' of 'n' bytes into time_t 'rtc'
 * @return 0 on failure and number of bytes processed
 */
int32_t tms_get_rtc(uint8_t *msg, int32_t n, time_t *rtc);

/** Print tms_rtc_t 'rtc' to file 'fp'.
 *  @return number of printed characters.
*/
int32_t tms_prt_rtc(FILE *fp, time_t *rtc, int nr, int hdr);

/** Write RealTime Clock 'rtc' into socket 'fd'.
 * @return number of bytes written (<0: failure)
*/
int32_t tms_write_rtc(int32_t fd, time_t *rtc);

/** Open TMS log file with name 'fname' and mode 'md'.
 * @return file pointer.
*/
FILE *tms_open_log(char *fname, char *md);

/** Close TMS log file
 * @return 0 in success.
*/
int32_t tms_close_log();


/** Log TMS buffer 'msg' of 'n' bytes to log file.
 * @return return number of printed characters.
*/
int32_t tms_write_log_msg(uint8_t *msg, int32_t n, char *comment);

/** Read TMS log number 'nr' into buffer 'msg' of maximum 'n' bytes from log file.
 * @return return length of message.
*/
int32_t tms_read_log_msg(uint32_t nr, uint8_t *msg, int32_t n);

/** Grep 'n' bits signed long integer from byte buffer 'buf' 
 *  @return 'n' bits signed integer
 */
int32_t get_int32_t(uint8_t *buf, int32_t *bip, int32_t n);

/** Get 4 byte long float for 'msg' starting at byte 'i'.
  * @return float
*/
float tms_get_float32(uint8_t *msg, int32_t *i);


/*********************************************************************/
/* Functions for reading the data from the SD flash cards            */
/*********************************************************************/

/** Get 16 bytes TMS date for 'msg' starting at position 'i'.
 * @note position after parsing in returned in 'i'.
 * @return 0 on success, -1 on failure.
*/
int32_t tms_get_date(uint8_t *msg, int32_t *i, time_t *t);

/** Get 14 bytes DOS date for 'msg' starting at position 'i'.
 * @note position after parsing in returned in 'i'.
 * @return 0 on success, -1 on failure.
*/
int32_t tms_get_dosdate(uint8_t *msg, int32_t *i, time_t *t);
 
/** Put time_t 't' as 16 bytes TMS date into 'msg' starting at position 'i'.
 * @note position after parsing in returned in 'i'.
 * @return 0 always.
*/
int32_t tms_put_date(time_t *t, uint8_t *msg, int32_t *i);

/** Convert buffer 'msg' starting at position 'i' into tms_config_t 'cfg'
 * @note new position byte will be return in 'i'
 * @return number of bytes parsed
 */
int32_t tms_get_cfg(uint8_t *msg, int32_t *i, tms_config_t *cfg);

/** Put tms_config_t 'cfg' into buffer 'msg' starting at position 'i'
 * @note new position byte will be return in 'i'
 * @return number of bytes put
 */
int32_t tms_put_cfg(uint8_t *msg, int32_t *i, tms_config_t *cfg);

/** Print tms_config_t 'cfg' to file 'fp'
 * @param prt_info !=0 -> print measurement/patient info
 * @return number of characters printed.
 */
int32_t tms_prt_cfg(FILE *fp, tms_config_t *cfg, int32_t prt_info);

/** Read tms_config_t 'cfg' from file 'fp'
 * @return number of characters printed.
 */
int32_t tms_rd_cfg(FILE *fp, tms_config_t *cfg);

/** Convert buffer 'msg' starting at position 'i' into tms_measurement_hdr_t 'hdr'
 * @note new position byte will be return in 'i'
 * @return number of bytes parsed
 */
int32_t tms_get_measurement_hdr(uint8_t *msg, int32_t *i, tms_measurement_hdr_t *hdr);

/** Print tms_config_t 'cfg' to file 'fp'
 * @param prt_info 0x01 print measurement/patient info
 * @return number of characters printed.
 */
int32_t tms_prt_measurement_hdr(FILE *fp, tms_measurement_hdr_t *hdr);

/** Convert buffer 'msg' starting at position 'i' into tms32_hdr_t 'hdr'
 * @note new position byte will be return in 'i'
 * @return number of bytes parsed
 */
int32_t tms_get_hdr32(uint8_t *msg, int32_t *i, tms_hdr32_t *hdr);

/** Print tms_hdr_t 'hdr' to file 'fp'
 * @return number of characters printed.
 */
int32_t tms_prt_hdr32(FILE *fp, tms_hdr32_t *hdr);

/** Convert buffer 'msg' starting at position 'i' into tms_signal_desc32_t 'desc'
 * @note new position byte will be return in 'i'
 * @return number of bytes parsed
 */
int32_t tms_get_desc32(uint8_t *msg, int32_t *i, tms_signal_desc32_t *desc);

/** Print tms_desc32_t 'hdr' to file 'fp'
 * @return number of characters printed.
 */
int32_t tms_prt_desc32(FILE *fp, tms_signal_desc32_t *desc, int32_t hdr);

/** Convert buffer 'msg' starting at position 'i' into tms_block_desc32_t 'blk'
 * @note new position byte will be return in 'i'
 * @return number of bytes parsed
 */
int32_t tms_get_blk32(uint8_t *msg, int32_t *i, tms_block_desc32_t *blk);

/** Print tms_block_desc32_t 'blk' to file 'fp'
 * @return number of characters printed.
 */
int32_t tms_prt_blk32(FILE *fp, tms_block_desc32_t *blk);

/*********************************************************************/
/* Functions for bluetooth connection                                */
/*********************************************************************/

/* Convert string 'a' with channel labels 'AB...I' into channel selection
 * @return channel selection integer with bit 'i' set to corresponding channel 'A'+i
*/
int32_t tms_chn_sel(char *a);

/** Initialize TMSi device with Bluetooth file descriptor 'fdd' and
 *   sample rate divider 'sample_rate_div'.
 * @note no timeout implemented yet.
 * @return current sample rate [Hz]
*/
int32_t tms_init(int32_t fdd, int32_t sample_rate_div);

/** Get elapsed time [s] of this tms_channel_data_t 'channel'.
* @return -1 of failure, elapsed seconds in success.
*/
double tms_elapsed_time(tms_channel_data_t *channel);

/** Get one or more samples for all channels
*  @note all samples are returned via 'channel'
* @return total number of samples in 'channel'
*/
int32_t tms_get_samples(tms_channel_data_t *channel);

/** Check battery status on channel 'sw_chn'
* @return 0:ok 1:battery low
*/
int32_t tms_chk_battery(tms_channel_data_t *chd, int32_t sw_chn);

/** Check button status
* @return 0:not pressed >0:button number and 'tsw' [s] of rising edge
*/
int32_t tms_chk_button(tms_channel_data_t *chd, int32_t sw_chn, double *tsw);

/** Get the number of channels of this TMSi device
 * @return number of channels
*/
int32_t tms_get_number_of_channels();

/** Get the TMSi device name
 * @return pointer to TMSi device name
*/
char *tms_get_device_name();

/* Get the current sample frequency.
* @return current sample frequency [Hz]
*/
double tms_get_sample_freq();

/** shutdown sample capturing.
 *  @return 0 always.
*/
int32_t tms_shutdown();


/*********************************************************************/
/*                    EDF/BDF definition                             */
/*********************************************************************/

/** EDF/BDF main header */
typedef struct edfMainHdr {
    char Version            [8];
    char PatientId         [80];
    char RecordingId       [80];
    char StartDate          [8];
    char StartTime          [8];
    char NrOfHeaderBytes    [8];
    char DataFormatVersion [44];
    char NrOfDataRecords    [8];
    char RecordDuration     [8];
    char NrOfSignalsInRecord[4];
} edfMainHdr_t;

typedef struct edfSignalHdr {
    char Label             [16];
    char TransducerType    [80];
    char PhysicalDimension  [8];
    char PhysicalMin        [8];
    char PhysicalMax        [8];
    char DigitalMin         [8];
    char DigitalMax         [8];
    char Prefiltering      [80];
    char NrOfSamples        [8];
    char Reserved          [32];
} edfSignalHdr_t;

/** Fill EDF/BDF main header 
 *  @return 0 always
*/
int32_t edfFillMainHdr(edfMainHdr_t *mHdr, int bdf, const char  *Patient, const char  *RecordId,
 const char *Version, time_t *t, int32_t NrOfDataRecords, double RecordDuration, 
 int32_t NrOfSignalsInRecord);

/** Fill EDF signal header 
  * @return 0 always
*/
int32_t edfFillSignalHdr(edfSignalHdr_t *sHdr, const char *Label, 
 char *TransducerType, char *PhysicalDimension, 
 double PhysicalMin, double PhysicalMax,      
 int32_t DigitalMin,int32_t DigitalMax, 
 char *Prefiltering, int32_t NrOfSamples);    

/** Write EDF main headers and 'NrOfSignals' signal headers to file 'fp'
 *  @return number of characters written
*/
int32_t edfWriteHeaders(FILE *fp, edfMainHdr_t *mHdr, edfSignalHdr_t *sHdr, 
 int32_t NrOfSignals);

/** Write 'n' bytes of integer 'a' into EDF file 'fp'
 */
int32_t edfWriteSample(FILE *fp, int32_t a, int32_t n);

/** write BDF (=EDF with 24 iso 16 bits samples) headers to file 'fp'
 *   for continues ExG recording with maximum of 'nch' channel definitions in 'chd', 
 *   channel selection 'cs', person 'id', message 'record', channel names 'chn_name' 
 *   and record duration 'dt' [s].
 * @return bytes written to 'fp'
*/
int32_t edfWriteHdr(FILE *fp, tms_channel_data_t *chd, int32_t nch, int32_t cs,
 const char *id, const char *record, time_t *t, double dt, const char **chn_name);

/** write samples in 'chd' to EDF (bdf==0) or BDF (bdf==1) file 'fp' with
 *   channel selection switch 'cs' and 'mpc' missed packets before this 'chd'.
 * @return number of bytes written
*/
int32_t edfWriteSamples(FILE *fp, int32_t bdf, tms_channel_data_t *chd,
 int32_t cs, int32_t mpc);

/** Construct tms_channel_data_t out of 'edf' header info.
 * @return pointer to channel_data_t struct, NULL on failure.
 */
tms_channel_data_t *edf_alloc_channel_data(edf_t *edf);

/** Read one block of channel data from EDF/BDF data structure 'edf'
 *   into tms channel data structure 'chn'.
 * @return number of samples read, 0 on failure.
 */
int32_t edf_rd_chn(edf_t *edf, tms_channel_data_t *chn);

extern int32_t BtWriteBytes( int32_t fd, const uint8_t * buf, int32_t count );
extern int32_t BtReadBytes ( int32_t fd,       uint8_t * buf, int32_t count );

#ifdef __cplusplus
}
#endif

#endif

