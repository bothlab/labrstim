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

#include "fftw-functions.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "defaults.h"

int
fftw_interface_theta_init (struct fftw_interface_theta *fftw_int)
{
    unsigned int i;
    // the next 3 variables should be set according to argument pass to this function.
    // that way the size of signal analyzed could be set in main.c according to options
    fftw_int->sampling_rate = DEFAULT_SAMPLING_RATE;
    fftw_int->fft_signal_data_size = FFT_SIGNAL_DATA_SIZE_THETA;  // should be a power of 2
    fftw_int->power_signal_length = DATA_IN_SEGMENT_TO_POWER_THETA;       // last data on which to calculate power, for theta/delta ratio
    fftw_int->real_data_to_fft_size = REAL_DATA_IN_SEGMENT_TO_FFT_THETA;
    if (fftw_int->fft_signal_data_size < fftw_int->real_data_to_fft_size) {
        fprintf (stderr,
                 "fftw_int->fft_signal_data_size<fftw_int->real_data_to_fft_size in fftw_interface_theta_init\n");
        return -1;
    }
    fftw_int->m = fftw_int->fft_signal_data_size / 2 + 1; // length of the fft complex array (diff than for numerical reciepe)
    fftw_int->power_signal_length = fftw_int->fft_signal_data_size / 2;   // portion of the signal on which the power is calculated
    // start from most recent data and go back this number of samples


    // variables for theta detection
    fftw_int->min_frequency_theta = MIN_FREQUENCY_THETA;
    fftw_int->max_frequency_theta = MAX_FREQUENCY_THETA;
    fftw_int->min_frequency_delta = MIN_FREQUENCY_DELTA;
    fftw_int->max_frequency_delta = MAX_FREQUENCY_DELTA;

    // variables to calculate the power
    fftw_int->signal_sum_square = 0;
    fftw_int->signal_mean_square = 0;
    fftw_int->signal_root_mean_square_theta = 0;
    fftw_int->signal_root_mean_square_delta = 0;

    // allocate memory
    if ((fftw_int->signal_data =
             malloc (sizeof (double) * fftw_int->fft_signal_data_size)) == NULL) {
        fprintf (stderr,
                 "problem allocating memory for fftw_int->signal_data\n");
        return -1;
    }
    for (i = 0; i < fftw_int->fft_signal_data_size; i++) {
        // initialise to 0
        fftw_int->signal_data[i] = 0;
    }
    if ((fftw_int->filter_function_theta =
             malloc (sizeof (double) * fftw_int->m)) == NULL) {
        fprintf (stderr,
                 "problem allocating memory for fftw_int->filter_function_theta\n");
        return -1;
    }
    if ((fftw_int->filter_function_delta =
             malloc (sizeof (double) * fftw_int->m)) == NULL) {
        fprintf (stderr,
                 "problem allocating memory for fftw_int->filter_function_delta\n");
        return -1;
    }
    if ((fftw_int->filtered_signal_theta =
             (double *) fftw_malloc (sizeof (double) *
                                     fftw_int->fft_signal_data_size)) == NULL) {
        fprintf (stderr,
                 "problem allocating memory for fftw_int->filtered_signal_theta\n");
        return -1;
    }
    if ((fftw_int->filtered_signal_delta =
             (double *) fftw_malloc (sizeof (double) *
                                     fftw_int->fft_signal_data_size)) == NULL) {
        fprintf (stderr,
                 "problem allocating memory for fftw_int->filtered_signal_delta\n");
        return -1;
    }
    if ((fftw_int->out_theta =
             (fftw_complex *) fftw_malloc (sizeof (fftw_complex) *
                                           fftw_int->fft_signal_data_size)) == NULL) {
        fprintf (stderr, "problem allocating memory for fftw_int->out_theta\n");
        return -1;
    }
    if ((fftw_int->out_delta =
             (fftw_complex *) fftw_malloc (sizeof (fftw_complex) *
                                           fftw_int->fft_signal_data_size)) == NULL) {
        fprintf (stderr, "problem allocating memory for fftw_int->out_delta\n");
        return -1;
    }
    // the output complex of fftw has n/2+1 size , real is in out_theta[i][0] and imaginary is in out_theta [i][1]
    if ((fftw_int->fft_plan_forward_theta =
             fftw_plan_dft_r2c_1d (fftw_int->fft_signal_data_size,
                                   fftw_int->filtered_signal_theta,
                                   fftw_int->out_theta, FFTW_MEASURE)) == NULL) {
        fprintf (stderr,
                 "unable to create a plan for fftw_int->fft_plan_forward_theta\n");
        return -1;
    }
    if ((fftw_int->fft_plan_backward_theta =
             fftw_plan_dft_c2r_1d (fftw_int->fft_signal_data_size,
                                   fftw_int->out_theta,
                                   fftw_int->filtered_signal_theta,
                                   FFTW_MEASURE)) == NULL) {
        fprintf (stderr,
                 "unable to create a plan for fftw_int->fft_plan_backward_theta\n");
        return -1;
    }
    if ((fftw_int->fft_plan_forward_delta =
             fftw_plan_dft_r2c_1d (fftw_int->fft_signal_data_size,
                                   fftw_int->filtered_signal_delta,
                                   fftw_int->out_delta, FFTW_MEASURE)) == NULL) {
        fprintf (stderr,
                 "unable to create a plan for fftw_int->fft_plan_forward_delta\n");
        return -1;
    }
    if ((fftw_int->fft_plan_backward_delta =
             fftw_plan_dft_c2r_1d (fftw_int->fft_signal_data_size,
                                   fftw_int->out_delta,
                                   fftw_int->filtered_signal_delta,
                                   FFTW_MEASURE)) == NULL) {
        fprintf (stderr,
                 "unable to create a plan for fftw_int->fft_plan_backward_delta\n");
        return -1;
    }
    // make the butterworth filter for theta
    make_butterworth_filter (fftw_int->sampling_rate,     // max freq is sr/2
                             fftw_int->m, // m
                             fftw_int->filter_function_theta,     // size m
                             fftw_int->min_frequency_theta,       // lower cut-off frequency
                             fftw_int->max_frequency_theta);      // higher cut-off frequency
    // make the butterworth filter for delta
    make_butterworth_filter (fftw_int->sampling_rate,     // max freq is sr/2
                             fftw_int->m, // m
                             fftw_int->filter_function_delta,     // size m
                             fftw_int->min_frequency_delta,       // lower cut-off frequency
                             fftw_int->max_frequency_delta);      // higher cut-off frequency
    return 0;
}

int
fftw_interface_theta_free (struct fftw_interface_theta *fftw_int)
{
    free (fftw_int->signal_data);
    free (fftw_int->filter_function_theta);
    free (fftw_int->filter_function_delta);
    fftw_free (fftw_int->filtered_signal_theta);
    fftw_free (fftw_int->filtered_signal_delta);
    fftw_free (fftw_int->out_theta);
    fftw_free (fftw_int->out_delta);
    fftw_destroy_plan (fftw_int->fft_plan_forward_theta);
    fftw_destroy_plan (fftw_int->fft_plan_forward_delta);
    fftw_destroy_plan (fftw_int->fft_plan_backward_theta);
    fftw_destroy_plan (fftw_int->fft_plan_backward_delta);
    return 0;
}

int
fftw_interface_theta_apply_filter_theta_delta (struct fftw_interface_theta
        *fftw_int)
{
    unsigned int i;
    double sum = 0;
    double mean;

    // set the mean of the signal to 0
    for (i = 0; i < fftw_int->real_data_to_fft_size; i++) {
        sum += fftw_int->signal_data[i];
    }
    mean = sum / fftw_int->real_data_to_fft_size;
    for (i = 0; i < fftw_int->real_data_to_fft_size; i++) {
        fftw_int->signal_data[i] = fftw_int->signal_data[i] - mean;
    }

    // copy the signal data into the fitered signal array
    for (i = 0; i < fftw_int->fft_signal_data_size; i++) {
        fftw_int->filtered_signal_theta[i] = fftw_int->signal_data[i];
        fftw_int->filtered_signal_delta[i] = fftw_int->signal_data[i];
    }

    // do the fft forward, will give us an array of complex values
    fftw_execute (fftw_int->fft_plan_forward_theta);
    fftw_execute (fftw_int->fft_plan_forward_delta);


    // do the filtering
    for (i = 0; i < fftw_int->m; i++) {
        fftw_int->out_theta[i][0] =
            fftw_int->out_theta[i][0] * fftw_int->filter_function_theta[i];
        fftw_int->out_theta[i][1] =
            fftw_int->out_theta[i][1] * fftw_int->filter_function_theta[i];
        fftw_int->out_delta[i][0] =
            fftw_int->out_delta[i][0] * fftw_int->filter_function_delta[i];
        fftw_int->out_delta[i][1] =
            fftw_int->out_delta[i][1] * fftw_int->filter_function_delta[i];
    }
    // reverse the fft
    fftw_execute (fftw_int->fft_plan_backward_theta);
    fftw_execute (fftw_int->fft_plan_backward_delta);

    // rescale the results by the factor of fft_signal_data_size
    for (i = 0; i < fftw_int->fft_signal_data_size; i++) {
        fftw_int->filtered_signal_theta[i] =
            fftw_int->filtered_signal_theta[i] / fftw_int->fft_signal_data_size;
        fftw_int->filtered_signal_delta[i] =
            fftw_int->filtered_signal_delta[i] / fftw_int->fft_signal_data_size;
    }
    return 0;
}

double
fftw_interface_theta_delta_ratio (struct fftw_interface_theta *fftw_int)
{
    unsigned int i;
    fftw_int->signal_sum_square = 0;
    fftw_int->signal_mean_square = 0;
    fftw_int->signal_root_mean_square_theta = 0;
    fftw_int->signal_root_mean_square_delta = 0;
    fftw_int->signal_sum_square = 0;

    // the signal is at the beginning of the filtered_signal_array
    for (i = fftw_int->real_data_to_fft_size - fftw_int->power_signal_length;
         i < fftw_int->real_data_to_fft_size; i++) {
        fftw_int->signal_sum_square +=
            (fftw_int->filtered_signal_theta[i] *
             fftw_int->filtered_signal_theta[i]);
    }

    fftw_int->signal_mean_square =
        fftw_int->signal_sum_square / fftw_int->real_data_to_fft_size;
    fftw_int->signal_root_mean_square_theta =
        sqrt (fftw_int->signal_mean_square);

    // for delta
    fftw_int->signal_sum_square = 0;
    for (i = fftw_int->real_data_to_fft_size - fftw_int->power_signal_length;
         i < fftw_int->real_data_to_fft_size; i++) {
        fftw_int->signal_sum_square +=
            (fftw_int->filtered_signal_delta[i] *
             fftw_int->filtered_signal_delta[i]);
    }
    fftw_int->signal_mean_square =
        fftw_int->signal_sum_square / fftw_int->real_data_to_fft_size;
    fftw_int->signal_root_mean_square_delta =
        sqrt (fftw_int->signal_mean_square);
    // check if the theta/delta ration is larger than threshold
    return fftw_int->signal_root_mean_square_theta /
           fftw_int->signal_root_mean_square_delta;
}

double
fftw_interface_theta_get_phase (struct fftw_interface_theta *fftw_int,
                                struct timespec *elapsed_since_acquisition,
                                double frequency)
{
    int i;
    double diff;
    double time_elapsed_acquisition_ms =
        ((double) elapsed_since_acquisition->tv_nsec / 1000000.0) +
        ((double) elapsed_since_acquisition->tv_sec * 1000);
    double time_elapsed_since_transition_ms;
    double total_elapsed_ms;
    double phase;
    double degrees_per_ms = 360 / (1000.0 / 8);
    // after looking at the raw signal and filtered signal, it seems that the way to go is to detect the last neg to pos or pos to neg transition.

    i = fftw_int->real_data_to_fft_size - 1;
    if (fftw_int->filtered_signal_theta[i] >= 0) {
        // detect the last negative to postive  transition, faster to start from end of signal
        while (i >= 0 && fftw_int->filtered_signal_theta[i] >= 0) {
            i--;
        }
        // we got the index of negative to position, which represents phase 180
        diff = fftw_int->real_data_to_fft_size - 1 - i;
        time_elapsed_since_transition_ms =
            diff / fftw_int->sampling_rate * 1000.0;
        total_elapsed_ms =
            time_elapsed_since_transition_ms + time_elapsed_acquisition_ms;
        phase = 180 + (total_elapsed_ms * degrees_per_ms);
        //      fprintf(stderr,"diff: %lf time_elapsed_since_transition_ms: %lf time_elapsed_acquisition_ms: %lf, dpms: %lf, degrees added: %lf\n",diff,time_elapsed_since_transition_ms,time_elapsed_acquisition_ms,degrees_per_ms,degrees_per_ms*total_elapsed_ms);
    } else {
        // detect the last positive to negative
        while (i >= 0 && fftw_int->filtered_signal_theta[i] < 0) {
            i--;
        }
        // we got the index of the positive to negative transition, which represents phase 0
        diff = fftw_int->real_data_to_fft_size - 1 - i;

        time_elapsed_since_transition_ms =
            diff / fftw_int->sampling_rate * 1000.0;
        total_elapsed_ms =
            time_elapsed_since_transition_ms + time_elapsed_acquisition_ms;
        phase = total_elapsed_ms * degrees_per_ms;
        //   fprintf(stderr,"diff: %lf time_elapsed_since_transition_ms: %lf time_elapsed_acquisition_ms: %lf, dpms: %lf, degrees added: %lf\n",diff,time_elapsed_since_transition_ms,time_elapsed_acquisition_ms,degrees_per_ms,degrees_per_ms*total_elapsed_ms);
    }
    while (phase > 360) {
        phase -= 360;
    }
    return phase;
}


int
make_butterworth_filter (int sampling_rate,     // max freq is sr/2
                         int filter_length,     // m
                         double *filter_function,       // size m
                         double low_pass,       // lower cut-off frequency
                         double high_pass)      // higher cut-off frequency
{
    int n_low;                    // order of filter
    int n_high;                   // order of filter
    double *function_low_pass;
    double *function_high_pass;
    double frequency_steps =
        (double) sampling_rate / 2 / (double) filter_length;
    double frequency;
    int i = 0;

    if ((function_low_pass = malloc (sizeof (double) * filter_length)) == NULL) {
        fprintf (stderr,
                 "unable to allocate memory for function_low_pass in fftw_interface_theta_make_butterworth_filter\n");
        return -1;
    }
    if ((function_high_pass = malloc (sizeof (double) * filter_length)) == NULL) {
        fprintf (stderr,
                 "unable to allocate memory for function_high_pass in fftw_interface_theta_make_butterworth_filter\n");
        return -1;
    }

    // set the order of the filters
    if (low_pass >= 0 && low_pass < 15) {
        n_low = 15;
    }
    if (low_pass >= 15 && low_pass < 85) {
        n_low = 19;
    }
    if (low_pass >= 85) {
        n_low = 19;
    }
    if (high_pass >= 0 && high_pass < 15) {
        n_high = 15;
    }
    if (high_pass >= 15 && high_pass < 85) {
        n_high = 19;
    }
    if (high_pass >= 85) {
        n_high = 19;
    }

    // make the function for low pass
    frequency = 0;
    for (i = 0; i < filter_length; i++) {
        function_low_pass[i] =
            1 / sqrt (1 + pow ((frequency / high_pass), 2 * n_low));
        frequency = frequency + frequency_steps;
    }
    // make the function for high pass
    frequency = 0;
    for (i = 0; i < filter_length; i++) {
        function_high_pass[i] =
            1 - (1 / sqrt (1 + pow ((frequency / low_pass), 2 * n_low)));
        frequency = frequency + frequency_steps;
    }
    // make the product of the two function
    for (i = 0; i < filter_length; i++) {
        filter_function[i] = function_high_pass[i] * function_low_pass[i];
    }
    free (function_low_pass);
    free (function_high_pass);

    /* FIXME: we don't use this variable (yet?) */
    (void) n_high;
    return 0;
}

double
phase_difference (double phase1, double phase2)
{
    // will output a phase from -180 to 180
    double phase_difference;
    if (phase1 <= phase2) {
        phase_difference = phase2 - phase1;
        if (phase_difference <= 180) {
            phase_difference = 0 - phase_difference;
        }
        if (phase_difference > 180) {
            // that means that
            phase_difference = 360 - phase_difference;
        }
    } else {
        phase_difference = phase1 - phase2;
        if (phase_difference > 180) {
            phase_difference = 0 - (360 - phase_difference);
        }

    }
    return phase_difference;
}



int
fftw_interface_swr_init (struct fftw_interface_swr *fftw_int)
{
    unsigned int i;
    // the next 3 variables should be set according to argument pass to this function.
    // that way the size of signal analyzed could be set in main.c according to options
    fftw_int->sampling_rate = DEFAULT_SAMPLING_RATE;
    fftw_int->fft_signal_data_size = FFT_SIGNAL_DATA_SIZE_SWR;    // should be a power of 2
    fftw_int->power_signal_length = DATA_IN_SEGMENT_TO_POWER_SWR;
    fftw_int->real_data_to_fft_size = REAL_DATA_IN_SEGMENT_TO_FFT_SWR;
    fftw_int->fft_scale = 1.0 / (double) fftw_int->fft_signal_data_size;
    if (fftw_int->fft_signal_data_size < fftw_int->real_data_to_fft_size) {
        fprintf (stderr,
                 "fftw_int->fft_signal_data_size<fftw_int->real_data_to_fft_size in fftw_interface_swr_init\n");
        return -1;
    }
    fftw_int->m = fftw_int->fft_signal_data_size / 2 + 1; // length of the fft complex array (diff than for numerical reciepe)

    // start from most recent data and go back this number of samples


    // variables for swr detection
    fftw_int->min_frequency_swr = MIN_FREQUENCY_SWR;
    fftw_int->max_frequency_swr = MAX_FREQUENCY_SWR;
    fftw_int->frequency_wavelet_for_convolution =
        FREQUENCY_WAVELET_FOR_CONVOLUTION;

    // variables to calculate the power
    fftw_int->size_root_mean_square_array = SIZE_ROOT_MEAN_SQUARE_ARRAY;
    fftw_int->number_segments_analysed = 0;
    fftw_int->number_convolution_peaks_analysed = 0;
    fftw_int->signal_sum_square = 0;
    fftw_int->signal_mean_square = 0;
    fftw_int->signal_root_mean_square = 0;
    fftw_int->mean_power = 0;
    fftw_int->std_power = 0;

    // allocate memory
    if ((fftw_int->signal_data =
             malloc (sizeof (double) * fftw_int->fft_signal_data_size)) == NULL) {
        fprintf (stderr,
                 "problem allocating memory for fftw_int->signal_data\n");
        return -1;
    }
    if ((fftw_int->ref_signal_data =
             malloc (sizeof (double) * fftw_int->fft_signal_data_size)) == NULL) {
        fprintf (stderr,
                 "problem allocating memory for fftw_int->ref_signal_data\n");
        return -1;
    }
    if ((fftw_int->wavelet_for_convolution =
             malloc (sizeof (double) * fftw_int->fft_signal_data_size)) == NULL) {
        fprintf (stderr,
                 "problem allocating memory for fftw_int->wavelet_for_convolution\n");
        return -1;
    }
    for (i = 0; i < fftw_int->fft_signal_data_size; i++) {
        // initialise to 0
        fftw_int->signal_data[i] = 0;
        fftw_int->ref_signal_data[i] = 0;
    }
    if ((fftw_int->root_mean_square_array =
             malloc (sizeof (double) * fftw_int->size_root_mean_square_array)) ==
        NULL) {
        fprintf (stderr,
                 "problem allocating memory for fftw_int->root_mean_square_array\n");
        return -1;
    }
    if ((fftw_int->convolution_peak_array =
             malloc (sizeof (double) * fftw_int->size_root_mean_square_array)) ==
        NULL) {
        fprintf (stderr,
                 "problem allocating memory for fftw_int->convolution_peak_array\n");
        return -1;
    }

    if ((fftw_int->filter_function_swr =
             malloc (sizeof (double) * fftw_int->m)) == NULL) {
        fprintf (stderr,
                 "problem allocating memory for fftw_int->filter_function_swr\n");
        return -1;
    }
    if ((fftw_int->filtered_signal_swr =
             (double *) fftw_malloc (sizeof (double) *
                                     fftw_int->fft_signal_data_size)) == NULL) {
        fprintf (stderr,
                 "problem allocating memory for fftw_int->filtered_signal_swr\n");
        return -1;
    }
    for (i = 0; i < fftw_int->fft_signal_data_size; i++) {
        fftw_int->filtered_signal_swr[i] = 0;     // assigned to value of 0
    }

    if ((fftw_int->out_swr =
             (fftw_complex *) fftw_malloc (sizeof (fftw_complex) *
                                           fftw_int->fft_signal_data_size)) == NULL) {
        fprintf (stderr, "problem allocating memory for fftw_int->out_swr\n");
        return -1;
    }
    if ((fftw_int->out_wavelet =
             (fftw_complex *) fftw_malloc (sizeof (fftw_complex) *
                                           fftw_int->fft_signal_data_size)) == NULL) {
        fprintf (stderr,
                 "problem allocating memory for fftw_int->out_wavelet\n");
        return -1;
    }
    if ((fftw_int->out_convoluted =
             (fftw_complex *) fftw_malloc (sizeof (fftw_complex) *
                                           fftw_int->fft_signal_data_size)) == NULL) {
        fprintf (stderr,
                 "problem allocating memory for fftw_int->out_convoluted\n");
        return -1;
    }
    if ((fftw_int->convoluted_signal =
             (double *) fftw_malloc (sizeof (double) *
                                     fftw_int->fft_signal_data_size)) == NULL) {
        fprintf (stderr,
                 "problem allocating memory for fftw_int->convoluted_signal\n");
        return -1;
    }


    // to fft the signal before filtering, the output complex of fftw has n/2+1 size , real is in out_swr[i][0] and imaginary is in out_swr [i][1]
    if ((fftw_int->fft_plan_forward_swr =
             fftw_plan_dft_r2c_1d (fftw_int->fft_signal_data_size,
                                   fftw_int->filtered_signal_swr, fftw_int->out_swr,
                                   FFTW_MEASURE)) == NULL) {
        fprintf (stderr,
                 "unable to create a plan for fftw_int->fft_plan_forward_swr\n");
        return -1;
    }
    // to fft the wavelet, the output complex of fftw has n/2+1 size , real is in out_swr[i][0] and imaginary is in out_swr [i][1]
    if ((fftw_int->fft_plan_forward_wavelet =
             fftw_plan_dft_r2c_1d (fftw_int->fft_signal_data_size,
                                   fftw_int->wavelet_for_convolution,
                                   fftw_int->out_wavelet, FFTW_MEASURE)) == NULL) {
        fprintf (stderr,
                 "unable to create a plan for fftw_int->fft_plan_forward_wavelet\n");
        return -1;
    }
    // the get back the filtered signal
    if ((fftw_int->fft_plan_backward_swr =
             fftw_plan_dft_c2r_1d (fftw_int->fft_signal_data_size,
                                   fftw_int->out_swr, fftw_int->filtered_signal_swr,
                                   FFTW_MEASURE)) == NULL) {
        fprintf (stderr,
                 "unable to create a plan for fftw_int->fft_plan_backward_swr\n");
        return -1;
    }

    // the get back the filtered signal
    if ((fftw_int->fft_plan_backward_wavelet_convolution =
             fftw_plan_dft_c2r_1d (fftw_int->fft_signal_data_size,
                                   fftw_int->out_convoluted,
                                   fftw_int->convoluted_signal,
                                   FFTW_MEASURE)) == NULL) {
        fprintf (stderr,
                 "unable to create a plan for fftw_int->fft_plan_backward_swr\n");
        return -1;
    }


    // make the butterworth filter for swr
    make_butterworth_filter (fftw_int->sampling_rate,     // max freq is sr/2
                             fftw_int->m, // m
                             fftw_int->filter_function_swr,       // size m
                             fftw_int->min_frequency_swr, // lower cut-off frequency
                             fftw_int->max_frequency_swr);        // higher cut-off frequency

    // make the wavelet for convolution
    make_wavelet_for_convolution (fftw_int->sampling_rate,
                                  fftw_int->fft_signal_data_size,
                                  fftw_int->wavelet_for_convolution,
                                  fftw_int->frequency_wavelet_for_convolution);

    // fft the wavelet for convolution as we only need it in the frequency domain for convolution
    fftw_execute (fftw_int->fft_plan_forward_wavelet);


    return 0;
}

int
fftw_interface_swr_free (struct fftw_interface_swr *fftw_int)
{
    free (fftw_int->signal_data);
    free (fftw_int->ref_signal_data);
    free (fftw_int->wavelet_for_convolution);
    free (fftw_int->root_mean_square_array);
    free (fftw_int->convolution_peak_array);
    free (fftw_int->filter_function_swr);
    fftw_free (fftw_int->filtered_signal_swr);
    fftw_free (fftw_int->convoluted_signal);
    fftw_free (fftw_int->out_swr);
    fftw_free (fftw_int->out_wavelet);
    fftw_free (fftw_int->out_convoluted);
    fftw_destroy_plan (fftw_int->fft_plan_forward_swr);
    fftw_destroy_plan (fftw_int->fft_plan_forward_wavelet);
    fftw_destroy_plan (fftw_int->fft_plan_backward_swr);
    return 0;
}

int
fftw_interface_swr_differential_and_filter (struct fftw_interface_swr
        *fftw_int)
{
    unsigned int i;
    double sum = 0;
    double mean;
    // do the differential between signal and ref_signal, one signal minus the other
    for (i = 0; i < fftw_int->real_data_to_fft_size; i++) {
        fftw_int->signal_data[i] =
            fftw_int->signal_data[i] - fftw_int->ref_signal_data[i];
        sum += fftw_int->signal_data[i];
    }
    mean = sum / fftw_int->real_data_to_fft_size;

    // correct signal so that the mean is 0, and copy the signal data into the fitered signal array, signal will be at beginning of filtered_signal_swr array
    for (i = 0; i < fftw_int->real_data_to_fft_size; i++) {
        fftw_int->signal_data[i] = fftw_int->signal_data[i] - mean;
        fftw_int->filtered_signal_swr[i] = fftw_int->signal_data[i];
    }

    fftw_execute (fftw_int->fft_plan_forward_swr);

    // do the convolution in the frequency domain, out_swr and out_wavelet
    for (i = 0; i < fftw_int->m; i++) {
        // // pointwise product of the Fourier transforms
        fftw_int->out_convoluted[i][0] = (fftw_int->out_swr[i][0] * fftw_int->out_wavelet[i][0] - fftw_int->out_swr[i][1] * fftw_int->out_wavelet[i][1]) * fftw_int->fft_scale;   // A.re * B.re - A.im * B.im
        fftw_int->out_convoluted[i][1] = (fftw_int->out_swr[i][0] * fftw_int->out_wavelet[i][1] + fftw_int->out_swr[i][1] * fftw_int->out_wavelet[i][0]) * fftw_int->fft_scale;   // A.re * B.im + A.im * B.re
    }

    // do the filtering
    for (i = 0; i < fftw_int->m; i++) {
        fftw_int->out_swr[i][0] =
            fftw_int->out_swr[i][0] * fftw_int->filter_function_swr[i] *
            fftw_int->fft_scale;
        fftw_int->out_swr[i][1] =
            fftw_int->out_swr[i][1] * fftw_int->filter_function_swr[i] *
            fftw_int->fft_scale;
    }

    // get the convoluted signal in time domain, in ftw_int->convoluted_signal
    fftw_execute (fftw_int->fft_plan_backward_wavelet_convolution);
    // get the filtered signal in time domain, in filtered_signal_swr
    fftw_execute (fftw_int->fft_plan_backward_swr);
    return 0;
}

double
fftw_interface_swr_get_convolution_peak (struct fftw_interface_swr *fftw_int)
{

    unsigned int i;
    double max = fftw_int->convoluted_signal[0];

    for (i = fftw_int->real_data_to_fft_size - fftw_int->power_signal_length;
         i < fftw_int->real_data_to_fft_size; i++) {
        if (fftw_int->convoluted_signal[i] > max) {
            max = fftw_int->convoluted_signal[i];
        }
    }
    // store the convolution peak in the array if we haven't yet finish establishing the mean and sd of convolution peak
    if (fftw_int->number_convolution_peaks_analysed <
        fftw_int->size_root_mean_square_array) {
        fftw_int->convolution_peak_array[fftw_int->
                                         number_convolution_peaks_analysed] =
                                             max;

        // calculate the mean and sd with the data we have so far
        fftw_int->mean_convolution_peak =
            mean (fftw_int->number_convolution_peaks_analysed + 1,
                  fftw_int->convolution_peak_array, -1.0);
        fftw_int->std_convolution_peak =
            standard_deviation (fftw_int->number_convolution_peaks_analysed + 1,
                                fftw_int->convolution_peak_array, -1.0);
        fftw_int->number_convolution_peaks_analysed++;
    }
    return (max -
            fftw_int->mean_convolution_peak) / fftw_int->std_convolution_peak;
}

double
fftw_interface_swr_get_power (struct fftw_interface_swr *fftw_int)
{
    // this function returns the power as a z score, and also calculate the mean and std of power
    unsigned int i;
    fftw_int->signal_sum_square = 0;
    fftw_int->signal_mean_square = 0;
    fftw_int->signal_root_mean_square = 0;

    // the signal is at the beginning of the filtered_signal_array, go to end of signal minus the power_signal_length
    for (i = fftw_int->real_data_to_fft_size - fftw_int->power_signal_length;
         i < fftw_int->real_data_to_fft_size; i++) {
        fftw_int->signal_sum_square +=
            (fftw_int->filtered_signal_swr[i] * fftw_int->filtered_signal_swr[i]);
    }

    fftw_int->signal_mean_square = fftw_int->signal_sum_square / fftw_int->power_signal_length;   // the denominator was corrected on 03.02.12
    fftw_int->signal_root_mean_square = sqrt (fftw_int->signal_mean_square);

    // store the first power in the array to get the mean and sd
    if (fftw_int->number_segments_analysed <
        fftw_int->size_root_mean_square_array) {
        fftw_int->root_mean_square_array[fftw_int->number_segments_analysed] =
            fftw_int->signal_root_mean_square;
        fftw_int->mean_power =
            mean (fftw_int->number_segments_analysed + 1,
                  fftw_int->root_mean_square_array, -9999999.0);
        fftw_int->std_power =
            standard_deviation (fftw_int->number_segments_analysed + 1,
                                fftw_int->root_mean_square_array, -1.0);
        fftw_int->number_segments_analysed++;
    }
    fftw_int->z_power =
        (fftw_int->signal_root_mean_square -
         fftw_int->mean_power) / fftw_int->std_power;
    //  printf("num_seg: %ld, power_signal_length: %d, power: %.3lf, mean: %.3lf, sd: %.3lf, z_power: %.3lf, index start power window: %d\n",fftw_int->number_segments_analysed,fftw_int->power_signal_length,fftw_int->signal_root_mean_square,fftw_int->mean_power,fftw_int->std_power, fftw_int->z_power,fftw_int->real_data_to_fft_size-fftw_int->power_signal_length);
    return fftw_int->z_power;
}

double
mean (int num_data, double *data, double invalid)
{
    /*
       calculate the mean of array of size num_data
       return mean
     */
    double sum = 0;
    int valid = 0;
    int i = 0;
    for (i = 0; i < num_data; i++) {
        if (data[i] != invalid) {
            sum = sum + data[i];
            valid++;
        }
    }
    if (valid > 0) {
        return sum / valid;
    } else {
        return 0;
    }
}

double
standard_deviation (int num_data, double *data, double invalid)
{
    /* return the standard deviation of array */
    double sum_sq = 0;
    double sum = 0;
    int i = 0;
    int valid = 0;
    for (i = 0; i < num_data; i++) {
        if (data[i] != invalid) {
            sum += data[i];
            sum_sq += (data[i] * data[i]);
            valid++;
        }
    }
    return sqrt ((sum_sq - ((sum * sum) / valid)) / (valid - 1));
}

int
make_wavelet_for_convolution (int sampling_rate,
                              int size, double *data, double frequency)
{

    /* create a morlet wavelet with a given frequency
       w=e-(at^2) * cos(2*pi*fo*t)
       the standard deviation in time is set so that 2SD is 2 period long
     */
    double sd_t;
    double exp_1;
    double exp_2;
    double num_1;
    double den_1;
    double wavelet_standard_deviation_t = 2;      // the largest the number, the wider the wavelet
    double *wavelet;
    sd_t = wavelet_standard_deviation_t / frequency;
    double time_per_bin = (1 / (double) sampling_rate);
    double start_time_sec = 0 - size * time_per_bin / 2;
    double t;
    int i;

    if ((wavelet = malloc (sizeof (double) * size)) == NULL) {
        fprintf (stderr,
                 "problem allocating memory for wavelet in function make_wavelet_for_convolution\n");
        return -1;
    }

    for (i = 0; i < size; i++) {
        t = start_time_sec + i * time_per_bin;
        num_1 = pow (t, 2);
        den_1 = 2 * pow (sd_t, 2);
        exp_1 = exp (-(num_1 / den_1));   // for the gaussian kernal
        exp_2 = cos (2 * M_PI * frequency * t);   // sine wave at right frequency
        wavelet[i] = exp_1 * exp_2;
    }

    // rearrange the wavelet for fast fourier transform
    for (i = 0; i < size / 2; i++) {
        // place the second half of wavelet at beginning
        data[i] = wavelet[i + size / 2];
        // place the first half of the function at the end
        data[i + size / 2] = wavelet[i];
    }
    free (wavelet);

    return 0;
}
