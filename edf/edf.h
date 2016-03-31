#ifndef EDF_H
#define EDF_H

#ifdef _MSC_VER
  #include "../nexus/inc/win32_compat.h"
  #include "../nexus/inc/stdint_win32.h"
#else
  #include <stdint.h>
#endif

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EDFMAXNAMESIZE  (1024)
#define OVERFLOWBIT    (1<<30)

/** EDF/BDF signal header */
typedef struct edf_signal_t {
    char Label             [17];
    char TransducerType    [81];
    char PhysicalDimension  [9];
  double PhysicalMin           ;
  double PhysicalMax           ;
 int32_t DigitalMin            ;
 int32_t DigitalMax            ;
    char Prefiltering      [81];
 int32_t NrOfSamplesPerRecord  ;
    char Reserved          [33];
 int32_t NrOfSamples;
 int32_t *data;
} edf_signal_t;

/** EDF/BDF main header */
typedef struct edf_t {
    char Version           [9];
 int32_t bdf                  ;  /**< 0:EDF, 1:BDF */
    char PatientId        [81];
    char RecordingId      [81];
  time_t StartDateTime        ;
 int32_t NrOfHeaderBytes      ;
    char DataFormatVersion[45];
 int32_t NrOfDataRecords      ;
  double RecordDuration       ;
 int32_t NrOfSignals          ;
edf_signal_t  *signal         ;      
 double *rts;                    /**< record time stamp: available in EDF+C, EDF+D, BDF+C, BDF+D */
} edf_t;

/** Set verbose value in module EDF
 * @return previous verbose value
*/
int32_t edf_set_vb(int32_t new_vb);

/** Get message 'msg' of at most 'n' characters out of 'buf' string.
 * @return number of character in 'msg'
 * @note string 'msg' should have place for at least 'n+1' characters
*/
int32_t edf_get_str(char *buf, int32_t *idx, char *msg, int32_t n);

/** Get integer of at most 'n' characters out of 'buf' string.
 * @return number 
*/
int32_t edf_get_int(char *buf, int32_t *idx, int32_t n);

/** Get integer of at most 'n' characters out of 'buf' string.
 * @return number 
*/
double edf_get_double(char *buf, int32_t *idx, int32_t n);

/** Print double 'a' in exactly 'w' characters to file 'fp'
 *   use e-notation to fit too big or small numbers
 * @note 'w' should be >= 7
 * @return number of printed characters
*/
int32_t edf_prt_double(FILE *fp, int32_t w, double a);

/** Get integer sample value in edf struct of channel 'chn' and sample index 'idx'.
 * @return new integer value
*/
int32_t edf_get_integer_value(edf_t *edf, int32_t chn, int32_t idx);

/** Get overflow value in edf struct of channel 'chn' and sample index 'idx'.
 * @return 0 on no overflow, OVERFLOWBIT on overflow
*/
int32_t edf_get_overflow_value(edf_t *edf, int32_t chn, int32_t idx);

/** Get integer sample value and overflow 'ovf' in edf struct of channel 'chn' and sample index 'idx'.
 * @return new integer value
*/
int32_t edf_get_integer_overflow_value(edf_t *edf, int32_t chn, int32_t idx, int32_t *ovf);

/** Set new integer sample value of 'ynew' and overflow bit 'ovf' in edf struct 
 *   of channel 'chn' and sample index 'idx' with 'gain'.
 * @return new integer value
*/
int32_t edf_set_integer_overflow_value(edf_t *edf, float ynew, int32_t ovf, int32_t chn, int32_t idx, float gain);

/** Translate 'labels' to selection bitvector 
 * @return selection bitvector as uint64_t
*/
uint64_t edf_chn_selection(char *labels, edf_t *edf );

/** Get filesize of 'fp'
 * @return file size [byte]
*/
int64_t edf_get_file_size(FILE *fp);

/** Get EDF/BDF record size
 * @return record size [byte]
*/
int32_t edf_get_record_size(const edf_t *edf);

/** Calculate the number of records in EDF/BDF file 'fp' with main header 'edf'
 *   and signal headers 'sig' by filesize / record size.
 * @return number of records.
*/
int32_t edf_get_record_cnt(FILE *fp, edf_t *edf);

/** Set number of records 'rec_cnt' in EDF/BDF file 'fp'
 * @return 0 on success, <0 on failure
*/
int32_t edf_set_record_cnt(FILE *fp, int32_t rec_cnt);

/** Fix first character of version string in EDF/BDF file 'fp'
 * @return 0 on success, <0 on failure
*/
int32_t edf_fix_hdr_version(FILE *fp);

/** Read EDF/BDF signal headers from file 'fp' into 'sig' 
 *   by using info of 'edf'.
 * @return number of characters parsed.
*/
int32_t edf_rd_signal_hdr(FILE *fp, edf_t *edf);

/** Read EDF/BDF main + signal headers from file 'fp' into 'edf'
 * @return number of characters parsed.
*/
int32_t edf_rd_hdr(FILE *fp, edf_t *edf);

/** Write EDF/BDF main header 'edf' to file 'fp' 
 * @return number of characters printed.
*/
int32_t edf_wr_hdr(FILE *fp, edf_t *edf);

/** Print EDF/BDF main header 'edf' to file 'fp' 
 * @return number of characters printed.
*/
int32_t edf_prt_hdr(FILE *fp, edf_t *edf);

/** Read sample of 'n' bytes long from file 'fp'
*/
int32_t edf_rd_int(FILE *fp, int32_t n);

/** Write sample 'a' of 'n' bytes long to file 'fp'
*/
int32_t edf_wr_int(FILE *fp, int32_t a, int32_t n);

/** Read EDF/BDF samples from file 'fp' into 'edf'
 * @return number of bytes read
*/
int64_t edf_rd_samples(FILE *fp, edf_t *edf);

/** Write EDF/BDF samples in 'edf' to file 'fp'
 * starting at record index 'first' up to 'last'
 * @return number of bytes written
*/
int32_t edf_wr_samples(FILE *fp, edf_t *edf, int32_t first, int32_t last);
 
/** Write EDF/BDF struct in 'edf' to file 'fname'
 * starting at record index 'first' up to 'last'
 * @return number of bytes written
*/
int32_t edf_wr_file(char *fname, edf_t *edf, int32_t first, int32_t last);

/** Print EDF/BDF integer samples of selected channels in 'chn'.
*/
int32_t edf_prt_samples(FILE *fp, edf_t *edf, uint64_t chn);

/** Print EDF/BDF integer samples of selected channels 'chn'
 *   hexadecimal mode 'hex' and float printing mode 'val'
 *   print header hdr==1, no header hdr==0
 * @return number of printed characters
*/
int32_t edf_prt_hex_samples(FILE *fp, edf_t *edf, uint64_t chn, uint64_t hex, uint64_t val, int32_t hdr);
  
/* swap channel numbers 'i' and 'j' in 'edf'
 * @return always zero
*/
int32_t edf_swap_chn(edf_t *edf, int32_t i, int32_t j);

/** Add copy of EDF channel 'name' as last channel with 'newname' in 'edf'
 * @return new channel number: <0 on failure
*/
int32_t edf_cpy_chn(edf_t *edf, char *name, char *newname);

/** Find channel number of EDF channel with signal name 'name'
 * @return channel number 0...n-1 or -1 on failure.
*/
int32_t edf_fnd_chn_nr(edf_t *edf, const char *name);

/* Check range of all 'EEG' channels of a nexus recording
 * @return number of channels with more than 5% overflow problems
 * @note 'edf' struct appended with overflow bit per sample at bit position 30 = 1<<30
*/
int32_t edf_range_chk(edf_t *edf);
  
/* Count all samples with overflow problems
 * @return number total number of samples in overflow
*/
int32_t edf_range_cnt(edf_t *edf);

/** Free the edf_t data structure that was allocated by edf_rd_hdr()
*/
void edf_free(edf_t *edf);

/** stimuli pulse functions */

#define MINDIFF_ON_OFF     (500.0)  /**< minimum difference [uV] in on/off state of stimuli */
#define EDGE_DURATION      (0.050)  /**< edge duration [s] */ 
#define HALFBITDURATION    (0.100)  /**< duration of half bit in bi-phase modulation */
#define TOLERANCE          (0.035)  /**< [s] = 1.8/frame_rate of display ~ 60 Hz */
#define CHUNKSIZE        (16*1024)  /**< chunk size of pulse_t array */
#define STOP_DURATION      (0.100)  /**< [s] */
#define NTASK                  (9)  /**< number of tasks per session */

/** pulse struct */
typedef struct PULSE_T {
  double  tr;   /**< rising edge time [s] */
  double  dur;  /**< pulse duration [s] */
  double  dly;  /**< delay time [s] of stop signal */
} pulse_t;

/** serie of pulses */
typedef struct SERIE_T {
  int32_t  np;  /**< number of pulses in 'ps' */
  pulse_t *ps;  /**< array of pulses */
} serie_t;

/** codeword struct */
typedef struct CODEWORD_T {
  int32_t cw;      /**< codeword */
  int32_t is,ie;   /**< index of codeword start and end */
  double  ts,te;   /**< timestamp [s] of start and end of this codeword */
} codeword_t;

/** task struct */
typedef struct TASK_DEF_T {
  int32_t scw,ecw;
  char    name[8];
  double  dur;    /**< default duration [s] */
  int32_t nst;    /**< number of stimuli */
} task_def_t;

/** task struct */
typedef struct TASK_T {
  int32_t pn;     /**< person nr */
  int32_t sn;     /**< session nr */
  int32_t tn;     /**< task nr */
  int32_t tr;     /**< try number counts backward number 1 is last try */
  int32_t cw;     /**< task end code */
  char    name[8];/**< task name */
  int32_t is,ise; /**< start and start-end index of task */
  int32_t ie,ies; /**< end and end-start index of task */
  double  ts,te;  /**< when [s] */
  int32_t cs,ce;  /**< occurrence counter */
  double  dur;    /**< default duration [s] */
  int32_t cnt;    /**< stimuli counter */
} task_t;

/** Convert codeword to ident 'pp'.
 *  @return >0 on success
 */
int32_t cw2pp(int32_t cw, int32_t scl);

/** Convert codeword to sessionnr 'sn'.
 *  @return >0 on success
 */
int32_t cw2sn(int32_t cw, int32_t scl);

/** Convert codeword to tasknr 'tn'.
 *  @return >0 on success
 */
int32_t cw2tn(int32_t cw, int32_t scl);

/** Print 't' [s] as hh:mm:ss.ms to file 'fp'
 * @return number of printed characters
*/
int32_t edf_t2hhmmss(FILE *fp, double t);

/** Find corresponding amplitude 'a' for 'n' quantile points 'q' 
 *   of signal 'k' in 'edf' data.
 * @return number of quantiles found 
*/
int32_t edf_quantile(edf_t *edf, int32_t k, float *q, int32_t n, int32_t *a);

/** Find corresponding threshold for "binair" signal 'k' in 'edf' data
 *   by dividing whole recording period in 'nb' blocks
 * @return number of epochs analyzed 
*/
int32_t edf_fnd_thr(edf_t *edf, int32_t k, int32_t nb);

/** Check battery level in channel 'chn'
 * @return time [s] when first time battery low occurred (0.0 == never) 
*/
double edf_chk_battery(edf_t *edf, int32_t chn);

/** Check if duration 'a' fall in range [dur-tol, dur+tol]
 * @return 1 on in range else 0
*/
int32_t edf_match_dur(double a, double dur, double tol);

/** Print serie of pulses 'psr' to file 'fp'
 * @return number of printed characters
*/
int32_t edf_prt_serie(FILE *fp, serie_t *psr, int32_t stop);

/* find index of start code 'fcw' in task definition array 'task_def',
 * @return index 0..NTASK-1 or -1 on failure
*/
int32_t edf_fnd_start_codes(int32_t fcw);

/** Look for all bi-phase modulated startcodes of 'n' bits starting at index 'idx'
 *  Insert all start codes in 'task' maximum 'nt'
 * @return number of completed tasks
*/
int32_t edf_get_start_codes(int32_t idx, serie_t *psr, int32_t n, task_t *task, int32_t nt);

/** Look for all bi-phase modulated startcodes starting at index 'idx'
 *  Insert first 'ncw' startcodes in 'cw'
 * @return number of found startcodes
*/
int32_t edf_fnd_startcodes(int32_t idx, serie_t *psr, int32_t n, codeword_t *cwa, int32_t ncw);

/** Find task index in task definition array 'tasks' with 'nt' tasks
 *   which start with task number 'tns' and ends with 'tne'
 * @return task index, <0 if no match found
*/
int32_t edf_fnd_task_idx(int32_t tns, int32_t tne, task_def_t *tasks, int32_t nt);

/** Put all tasks in codeword array 'cwa' with 'ncw' codewords according
 *   task definition array 'tasks' with 'nt' tasks into 'task' with maximum
 *   number of storage places
 * @return number of tasks found
*/
int32_t edf_fnd_def_task(codeword_t *cwa, int32_t ncw, task_def_t *tasks, int32_t nt,
 task_t *task, int32_t mtsk, int32_t scl);

/** Find index of time 't' in serie_t 'psr'
* @return found index else -1
*/
int32_t edf_fnd_time_serie(double t, serie_t *psr);

/** Find stop delay time in pulse serie 'psr' from index 'i0' up to 'i1'
 *   with 'tgo' as stimulus duration of go signal 
* @return number of found stop signals
*/
int32_t edf_fnd_stop_delay(int32_t i0, int32_t i1, serie_t *psr, double tgo);

/** Find all pulse channel 'chn' in EDF struct 'edf' with help of 
 *  threshold 'thr' and reject all pulse shorter than 'tol' [s].
 *  @note serie of pulses returned 'psr'
 *  @return number of pulses
*/
int32_t edf_fnd_pulse(edf_t *edf, int32_t chn, int32_t thr, double tol, serie_t *psr);

/** Find start and end code of 'n' bits of all tasks with range check window length of 'tp' [s]
 *   put all found tasks in 'task' and 'tc' at most
 * @return number of found tasks
*/
int32_t edf_fnd_tasks(edf_t *edf, int32_t n, double tp, task_t *task, int32_t tc);

/** Read task definition file 'fname'
* @return pointer to task array and number of tasks in 'nt'
*/
task_def_t *edf_rd_tasks(char *fname, int32_t *nt);

/** Print 'nt' tasks of 'task' to file 'fp' with 'ppw' digits volunteer nr
 @ return number of printed characters
*/
int32_t edf_show_tasks(FILE *fp, task_t *task, int32_t nt, int32_t ppw, char *fname);

/** Try to remove the switching backlight in the display signal
 *  with a periode smaller than 'dt' [s].
 *  @return total number of fixed periods
*/
int32_t edf_remove_switching_backlight(edf_t *edf, double dt);

/** Add extra signal 'RejectedEpochs' for missing packets in 'edf'
 *   or delta overflow of more than 2 succeeding samples 
 * @return percentage of Rejected Epochs or delta overflow samples
*/
double edf_chk_miss(edf_t *edf);

/** Mark all samples from 't0' to 't1' [s] as useless
 * @return number of marked samples
*/
int32_t edf_mark(edf_t *edf, int32_t mrk, double t0, double t1);

/** Copy a subset of records from the input fiel to the output file
 * the output file should be open, and the headers to this output file should be written already
 * the number of records to copy is taken from the output header of edf_out
 * the first record to copy from the input file is specified by first_record
*/
void edf_copy_samples( const edf_t * const edf_in, FILE * const fp_in, 
                       const edf_t * edf_out, FILE * const fp_out, const uint32_t first_record );


/** EDF/BDF annotation functions */

#define FORMAT_SAMP     (0)
#define FORMAT_SEC      (1)
#define FORMAT_MSEC     (2)
#define FORMAT_SSLN     (3)   /**< Seconds Since Last Night 00:00:00.000 */

#define ANNOTTXTLEN     (8)   /**< maximum string length of an annotation */
#define ANNOTRECSIZE (8*12)   /**< annotation record size [byte] should be multiple of 12 */

/** EDF/BDF signal header */
typedef struct EDF_ANNOT_T {
 int64_t sc;                /**< position of annotation in sample number */
 double  sec;               /**< position of annotation in seconds */
 char    txt[ANNOTTXTLEN];  /**< annotation text */
} edf_annot_t;

/** Read annotation sample count positions from file 'fname'
 *   into EDF annotations array.
 * @note the number of annotation is returned in 'n'.
 * @return array pointer to annotation array
*/
edf_annot_t *edf_read_annot(char *fname, char *txt, int32_t *n, uint8_t format);

/** Add 'n' annotations in 'p' into edf structure 'edf'
* @return number inserted annotations
*/
int32_t edf_add_annot(edf_t *edf, edf_annot_t *p, int32_t n);

/** Get annotation from edf structure 'edf' 
 * @return array pointer to annotation array
 * @note the number of annotation is returned in 'n'.
 * @note record timestamps are returned in 'edf->rts' (one per record)
*/
edf_annot_t *edf_get_annot(edf_t *edf, int32_t *n);

#ifdef __cplusplus
}
#endif
 
#endif 

