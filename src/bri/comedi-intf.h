/*
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C) 2010 Kevin Allen
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LS_COMEDI_INTF_H
#define __LS_COMEDI_INTF_H

#include <comedi.h>
#include <comedilib.h>
#include <pthread.h>

#include "../defaults.h"

// defines for the comedi interface or comedi device
#define COMEDI_DEVICE_MAX_CHANNELS 64 // could be higher without too much trouble, this is per device
#define COMEDI_INTERFACE_MAX_DEVICES 1 // will only use the first comedi device (/dev/comedi0)
#define NSEC_PER_SEC (1000000000) // The number of nsecs per sec
#define MAX_BUFFER_LENGTH 120000 // buffer length for each comedi_dev
#define COMEDI_INTERFACE_TO_DEVICE_BUFFER_SIZE_RATIO 10 // size of comedi interface buffer, set according to device buffer size
#define MAX_SAMPLING_RATE 48000 // maximum allowed sampling rate
#define COMEDI_INTERFACE_ACQUISITION_SLEEP_TIME_MS 1 // if too high could lead to buffer overflow
// #define DEBUG_ACQ

/**
 * comedi_dev:
 *
 * structure to hold the information regarding an AD card
 * Notes:
 * Acquisition always reads from all available channels
 *
 * The selection of which channels are displayed or
 * recorded will not affect comedi_dev code
 *
 * Each device has its own buffer to read data, but
 * the data should always be accessed via the
 * comedi_interface buffer
 */
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

/**
 * comedi_interface:
 *
 * structure to hold the information about all cards together
 *
 * comed_interface has a buffer with the data from all
 * devices in it. The recording and gui part of the program
 * should only interface with comedi_interface and never directly
 * with the comedi_dev.
 */
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

int comedi_interface_init(); // get the information on the AD cards installed
int comedi_interface_free(struct comedi_interface* com);
int comedi_interface_set_inter_acquisition_sleeping_time(struct comedi_interface* com,
                                                         double ms);
int comedi_interface_set_channel_list();
int comedi_interface_set_sampled_channels(struct comedi_interface* com,
                                          int dev_no,
                                          int number_channels_to_sample,
                                          int* list);
int comedi_interface_start_acquisition(struct comedi_interface* com);
long int comedi_interface_get_last_data_one_channel(struct comedi_interface* com,
                                                    int channel_no,
                                                    int number_samples,
                                                    double* data,
                                                    struct timespec* time_acquisition);
int comedi_interface_stop_acquisition(struct comedi_interface* com);
long int comedi_interface_get_last_data_two_channels(struct comedi_interface* com,
                                                     int channel_no_1,
                                                     int channel_no_2,
                                                     int number_samples,
                                                     double* data_1,
                                                     double* data_2,
                                                     struct timespec* time_acquisition);

int comedi_dev_init(struct comedi_dev *dev,
                    char* file_name);
int comedi_dev_free(struct comedi_dev *dev);
int comedi_dev_read_data(struct comedi_dev *dev);
int comedi_interface_set_sampling_rate (struct comedi_interface* com,
                                        int sampling_rate);
int comedi_interface_build_command();
int comedi_interface_run_command();
int comedi_interface_stop_command();

int comedi_t_enable_master(comedi_t *dev);
int comedi_t_enable_slave(comedi_t *dev);

int comedi_dev_adjust_data_point_out_of_samples(struct comedi_dev *dev,
                                                int number_samples_transfered_to_inter_buffer);

int comedi_dev_clear_current_acquisition_variables(struct comedi_dev *dev);

int comedi_device_print_info(struct comedi_dev* dev);

#endif /* __LS_COMEDI_INTF_H */
