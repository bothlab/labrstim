/*
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef __LS_FFTW_FUNCTIONS_H
#define __LS_FFTW_FUNCTIONS_H

#include <time.h>
#include <fftw3.h>

/**
 * fftw_interface_swr:
 *
 * structure to hold the variables to do the theta analysis
 */
struct fftw_interface_swr
{
    int sampling_rate;
    size_t real_data_to_fft_size; // is smaller than fft_signal_data_size, rest will be padded with zero
    size_t fft_signal_data_size; // should be pow of 2
    float fft_scale;
    float* signal_data; // array of fft_signal_data_size size
    float* ref_signal_data;
    float* wavelet_for_convolution;
    size_t m; // length of the fft complex array (diff than for numerical reciepe)
    int power_signal_length; // portion of the signal on which the power is calculated
    // start from most recent data and go back this number of samples
    // limits for the band-pass filter
    float min_frequency_swr;
    float max_frequency_swr;
    float frequency_wavelet_for_convolution;
    // power calculation
    float signal_sum_square;
    float signal_mean_square;
    float signal_root_mean_square;
    float* root_mean_square_array; // to establish mean and sd of power
    float* convolution_peak_array; // to establish the mean and sd of peak
    int size_root_mean_square_array;
    long int number_segments_analysed;
    long int number_convolution_peaks_analysed;
    float mean_power; // mean of all signal root mean square so far
    float std_power; // standard deviation of all root mean square so far
    float z_power;
    float mean_convolution_peak;
    float std_convolution_peak;
    // filter function to do the filtering of the signal
    float* filter_function_swr; // kernel to do the filtering after the fft
    fftwf_complex *out_swr; // complex array return by fft forward
    fftwf_complex *out_wavelet; // complex array return by fft forward
    fftwf_complex *out_convoluted; // to put the convoluted complex signal
    float* filtered_signal_swr; // will go in and out of the fft
    float* convoluted_signal; // will get the convolution results of signal*wavelet
    fftwf_plan fft_plan_forward_swr; // plan to do fft forward
    fftwf_plan fft_plan_forward_wavelet; // plan to do fft forward
    fftwf_plan fft_plan_backward_swr; // plan to do fft backward
    fftwf_plan fft_plan_backward_wavelet_convolution; // plan to do fft backward
};

/**
 * fftw_interface_theta:
 *
 * structure to hold the variables to do the theta analysis
 */
struct fftw_interface_theta
{
    int sampling_rate;
    size_t real_data_to_fft_size; // is smaller than fft_signal_data_size, rest will be padded with zero
    size_t fft_signal_data_size; // should be pow of 2
    float* signal_data; // array of fft_signal_data_size size
    size_t m; // length of the fft complex array (diff than for numerical reciepe)
    int power_signal_length; // portion of the signal on which the power is calculated
                             // start from most recent data and go back this number of samples
    // limits for the different filters
    float min_frequency_theta;
    float max_frequency_theta;
    float min_frequency_delta;
    float max_frequency_delta;
    // theta_delta_ration to know if this is a theta epochs
    float signal_sum_square;
    float signal_mean_square;
    float signal_root_mean_square_theta;
    float signal_root_mean_square_delta;
    // filter function to do the filtering of the signal
    float* filter_function_theta; // kernel to do the filtering after the fft
    float* filter_function_delta; // kernel to do the filtering after the fft
    fftwf_complex *out_theta; // complex array return by fft forward
    fftwf_complex *out_delta; // complex array return by fft forward
    float* filtered_signal_theta; // will go in and out of the fft
    float* filtered_signal_delta; // will go in and out of the fft
    fftwf_plan fft_plan_forward_theta; // plan to do fft forward
    fftwf_plan fft_plan_backward_theta; // plan to do fft forward
    fftwf_plan fft_plan_forward_delta; // plan to do fft backward
    fftwf_plan fft_plan_backward_delta; // plan to do fft backward
};

int fftw_interface_theta_init (struct fftw_interface_theta* fftw_int); // should have parameters to allow signal of different length to be treated
int fftw_interface_theta_free (struct fftw_interface_theta* fftw_int);
int fftw_interface_theta_apply_filter_theta_delta (struct fftw_interface_theta* fftw_int);
float fftw_interface_theta_delta_ratio (struct fftw_interface_theta* fftw_int);

float fftw_interface_theta_get_phase (struct fftw_interface_theta* fftw_int,
                                      struct timespec* elapsed_since_acquisition,
                                      float frequency);

int fftw_interface_swr_init (struct fftw_interface_swr* fftw_int); // should have parameters to allow signal of different length to be treated
int fftw_interface_swr_free (struct fftw_interface_swr* fftw_int);

int fftw_interface_swr_differential_and_filter (struct fftw_interface_swr* fftw_int);
float fftw_interface_swr_get_power (struct fftw_interface_swr* fftw_int);
float fftw_interface_swr_get_convolution_peak (struct fftw_interface_swr* fftw_int);

float phase_difference (float phase1,
                        float phase2);
int make_butterworth_filter (int sampling_rate, // max freq is sr/2
                             int filter_length, // m
                             float* filter_function, // size m
                             float low_pass, // lower cut-off frequency
                             float high_pass); // higher cut-off frequency

float mean (int num_data,
            float* data,
            float invalid);
float standard_deviation (int num_data,
                          float* data,
                          float invalid);
int make_wavelet_for_convolution (int sampling_rate,
                                  int size,
                                  float* data,
                                  float frequency);

#endif /* __LS_FFTW_FUNCTIONS_H */
