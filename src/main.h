/****************************************************************
Copyright (C) 2010 Kevin Allen

This file is part of laser_stimulation

laser_stimulation is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

laser_stimulation is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with laser_stimulation.  If not, see <http://www.gnu.org/licenses/>.

date 15.02.2010
************************************************************************/

#include <comedi.h> // for the driver
#include <comedilib.h> // for the driver API
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <sys/time.h> // for the nanosleep
#include <pthread.h> // to be able to create threads
#include <math.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/mman.h>
#include <string.h>
#include <fftw3.h>
#include <config.h>
#include <sys/types.h>
#include <sys/stat.h>


// defines for the comedi interface or comedi device
#define COMEDI_DEVICE_MAX_CHANNELS 64 // could be higher without too much trouble, this is per device
#define COMEDI_INTERFACE_MAX_DEVICES 1 // will only use the first comedi device (/dev/comedi0)
#define NSEC_PER_SEC (1000000000) // The number of nsecs per sec
#define MAX_BUFFER_LENGTH 120000 // buffer length for each comedi_dev
#define COMEDI_INTERFACE_TO_DEVICE_BUFFER_SIZE_RATIO 10 // size of comedi interface buffer, set according to device buffer size
#define DEFAULT_SAMPLING_RATE 20048 // of the device
#define MAX_SAMPLING_RATE 48000 // maximum allowed sampling rate
#define COMEDI_INTERFACE_ACQUISITION_SLEEP_TIME_MS 1 // if too high could lead to buffer overflow
// #define DEBUG_ACQ


//#define DEBUG_FFTW_INTER

// defines for theta detection
#define FFT_SIGNAL_DATA_SIZE_THETA 16384 // length of data segment to FFTW
#define REAL_DATA_IN_SEGMENT_TO_FFT_THETA 10000 // half a second of data in FFTW, the rest is padding of 0 to avoid phase shift at the end due to beginning of signal
#define DATA_IN_SEGMENT_TO_POWER_THETA 10000 // half of second of data to calculate the theta/delta ratio
#define THETA_DELTA_RATIO  1.75
#define STIMULATION_REFRACTORY_PERIOD_THETA_MS 80
#define MIN_FREQUENCY_THETA 6
#define MAX_FREQUENCY_THETA 10
#define MIN_FREQUENCY_DELTA 2
#define MAX_FREQUENCY_DELTA 4
#define MAX_PHASE_DIFFERENCE 10
#define SLEEP_WHEN_NO_NEW_DATA_MS 0.2
#define NUMBER_SAMPLED_CHANNEL_DEVICE_0 64
//#define DEBUG_THETA


// defines for swr detection
#define FFT_SIGNAL_DATA_SIZE_SWR 1024 // number of data points that goes into the fft, needs to be a 2^x number
#define REAL_DATA_IN_SEGMENT_TO_FFT_SWR 400 // the is the number of singal data point that will go in the fft
                                                                                     // the rest will be filled with 0. window size
                                                                                     //  should be a number smaller or equal  FFT_SIGNAL_DATA_SIZE_SWR
#define DATA_IN_SEGMENT_TO_POWER_SWR 200 // this is the length of the segment on which the power detection is based on.
                                                                                 // should not be larger than REAL_DATA_IN_SEGMENT_TO_FFT_SWR

#define MIN_FREQUENCY_SWR 125 // default minimum frequency for ripple detection
#define MAX_FREQUENCY_SWR 250 // default maximum frequency for ripple detection
#define FREQUENCY_WAVELET_FOR_CONVOLUTION 160
#define SLEEP_WHEN_NO_NEW_DATA_MS 0.2  // 0.2 is ok, probably best not to change that
#define NUMBER_SAMPLED_CHANNEL_DEVICE_0 64 // 64 is ok if your card has 64 channels, will return new data more often the larger the number
#define INTERVAL_DURATION_BETWEEN_SWR_PROCESSING_MS 5 // the program will sleep 5 ms between each calculation of ripple power
#define SIZE_ROOT_MEAN_SQUARE_ARRAY 10000 
//#define DEBUG_SWR 

// defines for setting device and channels that control the laser
#define DEVICE_INDEX_FOR_STIMULATION 0 
#define CHANNEL_FOR_PULSE 1 // analog output channel
#define CHANNEL_FOR_LASER_INTENSITY 0 // analog output channel
#define BRAIN_CHANNEL_1 0 // analog input channel for theta and swr stimulation 
#define BRAIN_CHANNEL_2 1 // analog input channel for reference signal for swr stimulation

/// for the real time process
#define MY_PRIORITY (49) /* we use 49 as the PRREMPT_RT use 50
                            as the priority of kernel tasklets
                            and interrupt handler by default */
#define MAX_SAFE_STACK (8*1024) /* The maximum stack size which is
                                   guranteed safe to access without
                                   faulting */
#define NSEC_PER_SEC    (1000000000) /* The number of nsecs per sec. */

#define NANOSLEEP_OVERSHOOT 0.0080000 

/* mutex to prevent thread to read from a buffer that is being modified
 all parts of code reading or writing to comedi_inter.buffer_data should
 be surrounded by 
 pthread_mutex_lock( &mutex1 ); and   pthread_mutex_unlock( &mutex1 );
 this is a global variable for now
*/
pthread_mutex_t mutex_comedi_interface_buffer;

/******************************************************
 structure to hold the information regarding an AD card
Notes:
Acquisition always reads from all available channels

The selection of which channels are displayed or
recorded will not affect comedi_dev code

Each device has its own buffer to read data, but 
the data should always be accessed via the 
comedi_interface buffer
*******************************************************/
struct comedi_dev
{
  comedi_t *comedi_dev; // device itself
  char *file_name; // file name for access to device
  const char *name; // name of the card
  const char *driver; // name of the comedi driver
  int number_of_subdevices;
  int subdevice_analog_input; // id of analog input subdev
  int subdevice_analog_output; // id of analog output subdev
  int number_channels_analog_input;
  int number_channels_analog_output;
  int maxdata_input;
  int maxdata_output;
  int range; // index of the selected range
  int number_ranges;
  comedi_range ** range_input; // pointer to all the possible ranges on the card
  int number_ranges_output;
  comedi_range ** range_output_array;
  int range_output;
  double voltage_max_input;
  double voltage_max_output;
  int aref;
  int buffer_size;
  sampl_t buffer_data[MAX_BUFFER_LENGTH];
  sampl_t* pointer_buffer_data; // to accomodate for incomplete read sample
  int read_bytes;
  int samples_read;
  int data_point_out_of_samples; // because read operation returns incomplete samples
  long int cumulative_samples_read;
  comedi_cmd command; 
  unsigned int channel_list[COMEDI_DEVICE_MAX_CHANNELS]; // channel number for the comedi side
  int number_sampled_channels; // variable to be able to sample twice same channel on each sampling
  int is_acquiring;
};


/*********************************************************
structure to hold the information about all cards together

comed_interface has a buffer with the data from all
devices in it. The recording and gui part of the program
should only interface with comedi_interface and never directly 
with the comedi_dev.

*********************************************************/
struct comedi_interface
{
  int is_acquiring; // to send signal to the comedi thread
  int acquisition_thread_running; // set by the comedi thread when enter and exit
  int command_running;
  pthread_t acquisition_thread;
  int acquisition_thread_id;
  int number_devices; // counter of valid devices
  int sampling_rate; // sampling rate of recording
  int number_channels; // aggregate of two cards, calculate from dev[]->number_sampled_channels
  int max_number_samples_in_buffer;
  int min_samples_read_from_devices; // if compare the different devices
  unsigned long int number_samples_read; // total samples read
  int sample_no_to_add; // sample no of the newest sample in the buffer
  int samples_copied_at_end_of_buffer;

  int data_offset_to_dat; // to correct the data so that 0 is midpoint
  struct comedi_dev dev[COMEDI_INTERFACE_MAX_DEVICES]; // hold the info about each device
  unsigned int channel_list[COMEDI_DEVICE_MAX_CHANNELS*COMEDI_INTERFACE_MAX_DEVICES]; // hold the channel number user side
  int buffer_size;
  short int* buffer_data;// buffer to get data from comedi devices
  int data_points_move_back;
  int offset_move_back;

  struct timespec time_last_sample_acquired_timespec; 

  struct timespec inter_acquisition_sleep_timespec; // between read operations to driver
  double inter_acquisition_sleep_ms;
  struct timespec timespec_pause_restat_acquisition_thread; // allow acquisition to complete
  double pause_restart_acquisition_thread_ms;
  struct timespec req;
  };

/********************************************************
structure to hold the variables to do the theta analysis
********************************************************/
struct fftw_interface_theta
{
  int sampling_rate;
  int real_data_to_fft_size; // is smaller than fft_signal_data_size, rest will be padded with zero 
  int fft_signal_data_size; // should be pow of 2
  double* signal_data; // array of fft_signal_data_size size
  int m; // length of the fft complex array (diff than for numerical reciepe)
  int power_signal_length; // portion of the signal on which the power is calculated
                                         // start from most recent data and go back this number of samples
  // limits for the different filters
  double min_frequency_theta; 
  double max_frequency_theta;
  double min_frequency_delta;
  double max_frequency_delta;
  // theta_delta_ration to know if this is a theta epochs
  double signal_sum_square;
  double signal_mean_square;
  double signal_root_mean_square_theta;
  double signal_root_mean_square_delta;
  // filter function to do the filtering of the signal
  double* filter_function_theta; // kernel to do the filtering after the fft
  double* filter_function_delta; // kernel to do the filtering after the fft
  fftw_complex *out_theta; // complex array return by fft forward
  fftw_complex *out_delta; // complex array return by fft forward
   double* filtered_signal_theta; // will go in and out of the fft
  double* filtered_signal_delta; // will go in and out of the fft
  fftw_plan fft_plan_forward_theta; // plan to do fft forward
  fftw_plan fft_plan_backward_theta; // plan to do fft forward
  fftw_plan fft_plan_forward_delta; // plan to do fft backward
  fftw_plan fft_plan_backward_delta; // plan to do fft backward
};

/********************************************************
structure to hold the variables to do the theta analysis
********************************************************/
struct fftw_interface_swr
{
  int sampling_rate;
  int real_data_to_fft_size; // is smaller than fft_signal_data_size, rest will be padded with zero 
  int fft_signal_data_size; // should be pow of 2
  double fft_scale;
  double* signal_data; // array of fft_signal_data_size size
  double* ref_signal_data;
  double* wavelet_for_convolution;
  int m; // length of the fft complex array (diff than for numerical reciepe)
  int power_signal_length; // portion of the signal on which the power is calculated
  // start from most recent data and go back this number of samples
  // limits for the band-pass filter
  double min_frequency_swr; 
  double max_frequency_swr;
  double frequency_wavelet_for_convolution;
  // power calculation
  double signal_sum_square;
  double signal_mean_square;
  double signal_root_mean_square;
  double* root_mean_square_array; // to establish mean and sd of power
  double* convolution_peak_array; // to establish the mean and sd of peak
  int size_root_mean_square_array;
  long int number_segments_analysed;
  long int number_convolution_peaks_analysed;
  double mean_power; // mean of all signal root mean square so far
  double std_power; // standard deviation of all root mean square so far
  double z_power;
  double mean_convolution_peak;
  double std_convolution_peak;
  // filter function to do the filtering of the signal
  double* filter_function_swr; // kernel to do the filtering after the fft
  fftw_complex *out_swr; // complex array return by fft forward
  fftw_complex *out_wavelet; // complex array return by fft forward
  fftw_complex *out_convoluted; // to put the convoluted complex signal
  double* filtered_signal_swr; // will go in and out of the fft
  double* convoluted_signal; // will get the convolution results of signal*wavelet
  fftw_plan fft_plan_forward_swr; // plan to do fft forward
  fftw_plan fft_plan_forward_wavelet; // plan to do fft forward
  fftw_plan fft_plan_backward_swr; // plan to do fft backward
  fftw_plan fft_plan_backward_wavelet_convolution; // plan to do fft backward
};


/********************************************************
structure to hold the variables to do the time keeping
********************************************************/
struct time_keeper
{
  // timespec variables store time with high accuracy
  //  time_ = time point
  //  elapsed_ = duration

  double trial_duration_sec;
  double pulse_duration_ms;

  double inter_pulse_duration_ms; // for the train stimulation
  struct timespec inter_pulse_duration; // for train stimulation
  double end_to_start_pulse_ms; // for the train stimulation
  struct timespec swr_delay;
  double swr_delay_ms;
  struct timespec end_to_start_pulse_duration;
  struct timespec time_now;
  struct timespec time_beginning_trial;
  struct timespec elapsed_beginning_trial;
  struct timespec time_data_arrive_from_acquisition;
  struct timespec elapsed_from_acquisition;
  struct timespec time_last_stimulation;
  struct timespec time_end_last_stimulation;
  struct timespec elapsed_last_stimulation;
  struct timespec time_last_acquired_data;
  struct timespec elapsed_last_acquired_data;
  struct timespec time_processing;
  struct timespec elapsed_processing;
  struct timespec duration_pulse;
  struct timespec duration_refractory_period;
  struct timespec duration_sleep_when_no_new_data;
  struct timespec duration_sleep_to_right_phase;
  struct timespec actual_pulse_duration; // the duration of the last pulse, including nanosleep jitter
  struct timespec time_previous_new_data; // next 3 used in theta stimultion, to check the time to get new data
  struct timespec time_current_new_data;
  struct timespec duration_previous_current_new_data;
  struct timespec duration_sleep_between_swr_processing;
  struct timespec interval_duration_between_swr_processing;

  struct timespec req;
  //  double nano_comp_ms=NANOSLEEP_OVERSHOOT;
};


// structure to read dat files
struct data_file_si // data_file_short_integer
{
  char *file_name;
  int file_descriptor;
  int num_channels; 
  off_t file_size; 	                // length of the file in bytes
  long num_samples_in_file; 	// file_length/byte_per_sample
  short int* data_block;      // pointer to store the data from file
  int num_samples_in_complete_block; // number of samples in the complete blocks
  int block_size; // in bytes
};




/************************************************
functions defined in timespec_functions.c
************************************************/
struct timespec set_timespec_from_ms(double milisec);
struct timespec diff(struct timespec* start, struct timespec* end);
int microsecond_from_timespec(struct timespec* duration);

/*************************
 define in comedi_code.c
**************************/
int comedi_interface_init(); // get the information on the AD cards installed
int comedi_interface_free(struct comedi_interface* com); 
int comedi_interface_set_channel_list();
int comedi_interface_build_command();
int comedi_interface_run_command();
int comedi_interface_stop_command();
int comedi_interface_get_data_from_devices();
int comedi_interface_clear_current_acquisition_variables();
int comedi_interface_start_acquisition(struct comedi_interface* com);
int comedi_interface_stop_acquisition(struct comedi_interface* com);
int comedi_interface_set_sampling_rate (struct comedi_interface* com, int sampling_rate);
int comedi_interface_set_inter_acquisition_sleeping_time(struct comedi_interface* com, double ms);
int comedi_interface_set_sampled_channels(struct comedi_interface* com,int dev_no,int number_channels_to_sample,int* list);
int comedi_interface_print_info(struct comedi_interface* com);
long int comedi_interface_get_last_data_one_channel(struct comedi_interface* com,int channel_no,int number_samples,double* data,struct timespec* time_acquisition);
long int comedi_interface_get_last_data_two_channels(struct comedi_interface* com,int channel_no_1, int channel_no_2,int number_samples,double* data_1, double* data_2, struct timespec* time_acquisition);
int comedi_dev_init(struct comedi_dev *dev, char* file_name);
int comedi_dev_free(struct comedi_dev *dev);
int comedi_dev_read_data(struct comedi_dev *dev);
int comedi_dev_adjust_data_point_out_of_samples(struct comedi_dev *dev,int number_samples_transfered_to_inter_buffer);
int comedi_dev_clear_current_acquisition_variables(struct comedi_dev *dev);
int comedi_t_enable_master(comedi_t *dev);
int comedi_t_enable_slave(comedi_t *dev);
int comedi_device_print_info(struct comedi_dev* dev);
void * acquisition(void* comedi_inter); // thread that does the acquisition, the comedi interface needs to be passed to this function

/***************************************************
functions run at by the thread starter
the thread is killed when function exits
body of the function is in comedi_code.c
***************************************************/
void *acquisition(void* thread_id);

/****************************
defined in fftw_functions.c
****************************/
int fftw_interface_theta_init(struct fftw_interface_theta* fftw_int); // should have parameters to allow signal of different length to be treated
int fftw_interface_theta_free(struct fftw_interface_theta* fftw_int);
int fftw_interface_theta_apply_filter_theta_delta(struct fftw_interface_theta* fftw_int);
double fftw_interface_theta_delta_ratio(struct fftw_interface_theta* fftw_int);
double fftw_interface_theta_get_phase(struct fftw_interface_theta* fftw_int, struct timespec* elapsed_since_acquisition, double frequency);


int fftw_interface_swr_init(struct fftw_interface_swr* fftw_int); // should have parameters to allow signal of different length to be treated
int fftw_interface_swr_free(struct fftw_interface_swr* fftw_int);
int fftw_interface_swr_differential_and_filter(struct fftw_interface_swr* fftw_int);
double fftw_interface_swr_get_power(struct fftw_interface_swr* fftw_int);
double fftw_interface_swr_get_convolution_peak(struct fftw_interface_swr* fftw_int);            


double phase_difference(double phase1, double phase2);
int make_butterworth_filter(int sampling_rate, // max freq is sr/2
					   int filter_length, // m
					   double* filter_function, // size m
					   double low_pass, // lower cut-off frequency
					   double high_pass); // higher cut-off frequency
double mean(int num_data, double* data, double invalid);
double standard_deviation(int num_data, double* data, double invalid);
int make_wavelet_for_convolution(int sampling_rate,
				 int size,
				 double* data,
				 double frequency);


/*************************
defined in data_file_si.c 
**************************/
int init_data_file_si(struct data_file_si* df,const char *file_name,int num_channels);
int clean_data_file_si(struct data_file_si* df);
int data_file_si_load_block(struct data_file_si* df, long int start_index, long int size);
int data_file_si_get_data_one_channel(struct data_file_si* df, int channel_no, short int* one_channel, long int start_index, long int end_index);
int data_file_si_get_data_all_channels(struct data_file_si* df, short int* data, long int start_index, long int end_index);
int data_file_si_cut_data_file(struct data_file_si* df, char* new_name,long int start_index, long int end_index);
