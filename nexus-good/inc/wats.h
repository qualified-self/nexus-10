/** @file wats.h
 *
 * @ingroup HUMAN++
 *
 * $Id:  $
 *
 * @brief WATS constant and struct definitions.
 *
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

#ifndef WATS_H
#define WATS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "time.h"
#include "tmsi.h"


#define TDMA_FREQ               (33)

#define NR_OF_SENSOR_NODES      (3)
#define NR_OF_SENSOR_CHANNELS   (2)
#define NR_OF_CHANNEL_SAMPLES   (6)

#define NODE_DOMAIN             (0xAA00)

#define TH_FRAME_SYNC           (0x53)   
#define TH_BROADCAST_MASK       (0xFF)
#define TH_MASTER_ADDR          (NODE_DOMAIN)
#define TH_SENSOR_1_ADDR        (NODE_DOMAIN + 1)
#define TH_SENSOR_2_ADDR        (NODE_DOMAIN + 2)
#define TH_SENSOR_3_ADDR        (NODE_DOMAIN + 3)
#define TH_BROADCAST_ADDR       (NODE_DOMAIN + TH_BROADCAST_MASK)

#define MAX_PAYLOAD_LENGTH      (18)

#define EXG8_PACKET_LENGTH      (25)

#define TH_SIZE                 (sizeof (tp_hdr_t))
#define MAX_FRAME_LENGTH        (1+ TH_SIZE + MAX_PAYLOAD_LENGTH)


typedef struct tp_hdr
{
    uint16_t  dAddr;     // destination address (address of the receiver)
    uint16_t  sAddr;     // source address (address of the sender)
    uint8_t   type;
    uint8_t   group;
    uint8_t   length;
    uint8_t   seqNo;
} tp_hdr_t;

typedef struct packet
{
    tp_hdr_t  header;
    time_t    mStartTime;
    clock_t   mTimeStamp;     //  Nr of clock ticks since program is launched
    int16_t   pData[MAX_PAYLOAD_LENGTH];
    int16_t   pChnl[NR_OF_SENSOR_CHANNELS][NR_OF_CHANNEL_SAMPLES];
} packet_t;

typedef enum master_command
{
    cmdReset        = 0x00,
    cmdTdmaBeacon,
    cmdInquiry,
    cmdGetSwVersion,            //
    cmdGetHwVersion,            //  Returns [Major]         [Minor]
    cmdGetId,                   //  Returns [ID]
    cmdGetServices,             //  Returns [Major]         [Minor]
    cmdSetHeartBeat,            //
    cmdSetNrOfTimeSlots,        //
    cmdGetChannels,             //  Returns [NrOfChannels]
    cmdGetResolution,           //  Returns [Resolution]
    cmdSetMode,                 //          [Mode]
    cmdGetMode,                 //  Returns [Mode]
    cmdStart,
    cmdGetBatteryLevel,
    cmdSetGain,                 //          [ChannelNr]     [Gain]
    cmdGetGain,                 //  Returns [ChannelNr]     [Gain]
    cmdSetBandWidth,            //          [ChannelNr]     [Bandwidth]
    cmdGetBandWidth,            //  Returns [ChannelNr]     [Bandwidth]
    cmdGetRange,                //  Returns [UpperLimit]    [LowerLimit]
    cmdSetLed,                   //          [Enable/Disable]
    cmdSetChannel
} master_command_t;

typedef struct cmdFormat
{
    master_command_t    cmd;
    unsigned char       size;
}cmdFormat_t;

typedef enum group_members
{
    grpNone     = 0x33,
    grpMaster,
    grpSlave,
} group_members_t;

static cmdFormat_t cmdList[] __attribute__ ((unused)) = {
    {cmdReset            , 0},
    {cmdTdmaBeacon       , 0},
    {cmdInquiry          , 0},
    {cmdGetSwVersion     , 0},
    {cmdGetHwVersion     , 0},
    {cmdGetId            , 0},
    {cmdGetServices      , 0},
    {cmdSetHeartBeat     , 1},
    {cmdSetNrOfTimeSlots , 1},
    {cmdGetChannels      , 0},
    {cmdGetResolution    , 0},
    {cmdSetMode          , 1},
    {cmdGetMode          , 0},
    {cmdStart            , 1},
    {cmdGetBatteryLevel  , 0},
    {cmdSetGain          , 2},
    {cmdGetGain          , 1},
    {cmdSetBandWidth     , 2},
    {cmdGetBandWidth     , 1},
    {cmdGetRange         , 1},
    {cmdSetLed           , 1},
    {cmdSetChannel       , 1}
  };


static const char *spCmdDescription[] __attribute__ ((unused)) = {
    "Reset",
    "TdmaBeacon",
    "Inquiry",
    "Get Sw Version",
    "Get Hw Version",
    "Get Id",
    "Get Services",
    "Set Heart Beat",
    "Set Nr Of Time Slots",
    "Get Chnls",
    "Get Resolution",
    "Set Mode",
    "Get Mode",
    "Start DAQ",
    "Get Battery Level",
    "Set Gain",
    "Get Gain",
    "Set Band Width",
    "Get Band Width",
    "Get Range",
    "Set Led",
    "Set Chnl"
};

#define MAX_GAIN_IDX   (3)

static double gain2ChnExg[MAX_GAIN_IDX+1] = {390.0,800.0,1450.0,2500.0};

/** Set debug level of module wats to 'dbg'
 * @return previous debug level 
*/
int32_t wats_set_dbg(int32_t dbg);

/* Get the current uV per Bit.
* @return uV per Bit.
*/
double wats_get_uVperBit();

/* Get the current sample frequency.
* @return current sample frequency [Hz]
*/
double wats_get_sample_freq();

/** Open USB port with name 'fname' and set all serial parameters
 *  @return file descriptor >0 on success
*/
int32_t wats_open_port(char *fname);

/** Initialize WATS device with file descriptor 'fd'
 * @param sm 0:nothing 1:set mode 
 * @param gi: gain index 
 * @note no timeout implemented yet.
 * @return always exit state (3)
*/
int32_t wats_init(int32_t sm, int32_t gi);

/** Construct channel data block.
 * @return pointer to channel_data_t struct, NULL on failure.
 */
tms_channel_data_t *wats_alloc_channel_data();

/** Free channel data block.
 */
void wats_free_channel_data(tms_channel_data_t *chd);

/** Get one or more samples for all channels
*  @note all samples are returned via 'channel'
* @return lost packet(s) before this packet (should be zero)
*/
int32_t wats_get_samples(tms_channel_data_t *chd);

/** Print wats channel data 'chd' to file 'fp'.
 * @param mpc missed packet counter (should be zero most of the time)
 * @return number of characters printed.
 */
int32_t wats_prt_samples(FILE *fp, tms_channel_data_t *chd, int mpc);

/** Construct channel data block for ExG8 module.
 * @return pointer to channel_data_t struct, NULL on failure.
 */
tms_channel_data_t *exg8_alloc_channel_data();

/** Free channel ExG8 data block.
 */
void exg8_free_channel_data(tms_channel_data_t *chd);

/** Get one sampe for all 8 + mode + trigger channels
*  @note all samples are returned via 'channel'
* @return lost packet(s) before this packet (should be zero)
*/
int32_t exg8_get_samples(tms_channel_data_t *chd);

/** Print 10 ExG module channel data 'chd' to file 'fp'.
 * @param mpc missed packet counter (should be zero most of the time)
 * @return number of characters printed.
 */
int32_t exg8_prt_samples(FILE *fp, tms_channel_data_t *chd, int mpc);

/** Final stop grabbing
*/
int32_t wats_final_stop(int32_t sig);

/** write EDF with 16 bits samples headers to file 'fp'
 *   for continues ExG recording with channel definition 'chn' and selection 'cs', 
 *   person 'id', message 'record' and channel names 'chn_name'.
 * @return bytes written to 'fp'
*/
int32_t wats_edf_wr_hdr(FILE *fp, tms_channel_data_t *chd, const char *id, const char *record,
 time_t *t, double fs, const char **chn_name);

/** write EDF headers for 16 bits samples of ExG8 Module to file 'fp' with
 *   person 'id', message 'record' and channel names 'chn_name'.
 * @return bytes written to 'fp'
*/
int32_t exg8_edf_wr_hdr(FILE *fp, tms_channel_data_t *chd, const char *id, 
 const char *record, time_t *t, double fs, const char **chn_name);

#ifdef __cplusplus
}
#endif

#endif  //  WATS_H
