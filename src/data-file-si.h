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

#ifndef __LS_DATA_FILE_SI_H
#define __LS_DATA_FILE_SI_H

#include <sys/types.h>

/**
 *data_file_si:
 *
 * structure to read dat files
 */
typedef struct data_file_si {
    // data_file_short_integer
    char *file_name;
    int file_descriptor;
    int num_channels;
    off_t file_size;              // length of the file in bytes
    size_t num_samples_in_file;   // file_length/byte_per_sample
    short int *data_block;        // pointer to store the data from file
    int num_samples_in_complete_block;    // number of samples in the complete blocks
    int block_size;               // in bytes
} data_file_si;

int init_data_file_si (data_file_si * df,
                       const char *file_name, int num_channels);
int clean_data_file_si (data_file_si * df);
int data_file_si_load_block (data_file_si * df,
                             long int start_index, long int size);
int data_file_si_get_data_one_channel (data_file_si * df,
                                       int channel_no,
                                       short int *one_channel,
                                       long int start_index,
                                       long int end_index);
int data_file_si_get_data_all_channels (data_file_si * df,
                                        short int *data,
                                        long int start_index,
                                        long int end_index);
int data_file_si_cut_data_file (data_file_si * df,
                                char *new_name,
                                long int start_index,
                                long int end_index);

#endif /* __LS_DATA_FILE_SI_H */
