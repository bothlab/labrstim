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

#include "tasks.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <galdur.h>
#include "defaults.h"
#include "fftw-functions.h"
#include "data-file-si.h"
#include "utils.h"
#include "stimpulse.h"

/**
 * perform_train_stimulation:
 *
 * Do train a stimulation at a given frequency or at random intervals
 */
gboolean
perform_train_stimulation (gboolean random, double trial_duration_sec, double pulse_duration_ms,
                           double minimum_interval_ms, double maximum_interval_ms, double train_frequency_hz)
{
    TimeKeeper tk;

    tk.trial_duration_sec = trial_duration_sec;
    tk.pulse_duration_ms = pulse_duration_ms;

    tk.duration_sleep_when_no_new_data =
        gld_set_timespec_from_ms (SLEEP_WHEN_NO_NEW_DATA_MS);
    tk.duration_pulse = gld_set_timespec_from_ms (tk.pulse_duration_ms);
    tk.interval_duration_between_swr_processing =
        gld_set_timespec_from_ms (INTERVAL_DURATION_BETWEEN_SWR_PROCESSING_MS);

    if (random) {
        // check that the time variables make sense for the smallest possible interval
        tk.inter_pulse_duration_ms = minimum_interval_ms;
        tk.inter_pulse_duration = gld_set_timespec_from_ms (tk.inter_pulse_duration_ms);
        tk.end_to_start_pulse_ms = tk.inter_pulse_duration_ms - tk.pulse_duration_ms;
        if (tk.end_to_start_pulse_ms <= 0) {
            g_printerr ("tk.end_to_start_pulse_ms <= 0, %lf\n",
                        tk.end_to_start_pulse_ms);
            g_printerr ("The length of the pulse is longer than the minimum_interval_ms\n");
            return FALSE;
        }
    } else {
        // set the time variables for train stimulation at a fixed frequency
        tk.inter_pulse_duration_ms = 1000 / train_frequency_hz;
        tk.inter_pulse_duration = gld_set_timespec_from_ms (tk.inter_pulse_duration_ms);
        tk.end_to_start_pulse_ms = tk.inter_pulse_duration_ms - tk.pulse_duration_ms;
        if (tk.end_to_start_pulse_ms <= 0) {
            g_printerr ("tk.end_to_start_pulse_ms <= 0, %lf\n",
                        tk.end_to_start_pulse_ms);
            g_printerr ("Is the length of the pulse longer than the inter pulse duration?\n");
            return FALSE;
        }
    }

    // get time at beginning of trial
    clock_gettime (CLOCK_REALTIME, &tk.time_beginning_trial);
    clock_gettime (CLOCK_REALTIME, &tk.time_now);
    clock_gettime (CLOCK_REALTIME, &tk.time_last_stimulation);

    /* initialize pins for stimulation */
    stimpulse_init ();

    tk.elapsed_beginning_trial = gld_time_diff (&tk.time_beginning_trial, &tk.time_now);
    while (tk.elapsed_beginning_trial.tv_sec < tk.trial_duration_sec) { // loop until the trial is over
        clock_gettime (CLOCK_REALTIME, &tk.time_last_stimulation);

        /* start the pulse */
        stimpulse_set_trigger_high ();
        // wait
        nanosleep (&tk.duration_pulse, &tk.req);

        /* end of the pulse */
        stimpulse_set_trigger_low ();

        // get time now and the actual pulse duration
        clock_gettime (CLOCK_REALTIME, &tk.time_end_last_stimulation);
        tk.actual_pulse_duration =
            gld_time_diff (&tk.time_last_stimulation, &tk.time_end_last_stimulation);
        if (random) {
            // if doing random stimulation, get the next interval
            tk.inter_pulse_duration_ms =
                minimum_interval_ms +
                (rand () % (int) (maximum_interval_ms - minimum_interval_ms));
            tk.inter_pulse_duration =
                gld_set_timespec_from_ms (tk.inter_pulse_duration_ms);
        }
        fprintf (stderr, "interval: %lf ms\n", tk.inter_pulse_duration_ms);
        // calculate how long we need to sleep until the next pulse
        tk.end_to_start_pulse_duration =
            gld_time_diff (&tk.actual_pulse_duration, &tk.inter_pulse_duration);

        // sleep until the next pulse, note that this will overshoot by the time of 4 lines of code! (almost nothing)
        nanosleep (&tk.end_to_start_pulse_duration, &tk.req);

        // get the time since the beginning of the trial
        clock_gettime (CLOCK_REALTIME, &tk.time_now);
        tk.elapsed_beginning_trial =
            gld_time_diff (&tk.time_beginning_trial, &tk.time_now);
    }

    return TRUE;
}

/**
 * perform_theta_stimulation:
 *
 * Do the theta stimulation.
 */
gboolean
perform_theta_stimulation (gboolean random, double trial_duration_sec, double pulse_duration_ms, double stimulation_theta_phase,
                           const gchar *offline_data_file, int channels_in_dat_file, int offline_channel)
{
    TimeKeeper tk;
    GldAdc *daq;
    gboolean ret = FALSE;

    /* variables to work offline from a dat file */
    int new_samples_per_read_operation = 60; /* 3 ms of data */
    data_file_si data_file;
    short int* data_from_file = NULL;
    long int last_sample_no = 0;

    double theta_frequency = (MIN_FREQUENCY_THETA + MAX_FREQUENCY_THETA) / 2;
    double theta_degree_duration_ms = (1000 / theta_frequency) / 360;
    double theta_delta_ratio;

    double current_phase = 0;
    double phase_diff;

    double max_phase_diff = MAX_PHASE_DIFFERENCE;


    tk.trial_duration_sec = trial_duration_sec;
    tk.pulse_duration_ms = pulse_duration_ms;

    tk.duration_sleep_when_no_new_data =
        gld_set_timespec_from_ms (SLEEP_WHEN_NO_NEW_DATA_MS);
    tk.duration_pulse = gld_set_timespec_from_ms (tk.pulse_duration_ms);
    tk.interval_duration_between_swr_processing =
        gld_set_timespec_from_ms (INTERVAL_DURATION_BETWEEN_SWR_PROCESSING_MS);

    /* structure with variables to do the filtering of signal */
    struct fftw_interface_theta fftw_inter;

    /* configure ADC, run DAQ on CPU 0 */
    daq = gld_adc_new (LS_ADC_CHANNEL_COUNT, LS_DATA_BUFFER_SIZE, 0);
    gld_adc_set_acq_frequency (daq, LS_DEFAULT_SAMPLING_RATE);

    if (fftw_interface_theta_init (&fftw_inter) == -1) {
        fprintf (stderr, "Could not initialize fftw_interface_theta\n");
        return FALSE;
    }

    if (offline_data_file != NULL) {
        /* initialize the dat file */
        if (init_data_file_si (&data_file, offline_data_file, channels_in_dat_file) != 0) {
            fprintf (stderr, "Problem in initialisation of dat file\n");
            return FALSE;
        }

        // if get data from dat file, allocate memory to store short integer from dat file
        if ((data_from_file = (short *) malloc (sizeof (short) * fftw_inter.real_data_to_fft_size)) == NULL) {
            fprintf (stderr,
                     "Problem allocating memory for data_from_file\n");
            return FALSE;
        }
    }
    tk.duration_refractory_period =
        gld_set_timespec_from_ms (STIMULATION_REFRACTORY_PERIOD_THETA_MS);

    // start the acquisition thread, which will run in the background until comedi_inter.is_acquiring is set to 0
    ls_debug ("Starting acquisition\n");

#ifdef DEBUG
    // to check the intervals before getting new data.
    clock_gettime (CLOCK_REALTIME, &tk.time_previous_new_data);
    long int counter = 0;
#endif

    // get time at beginning of trial
    clock_gettime (CLOCK_REALTIME, &tk.time_beginning_trial);
    clock_gettime (CLOCK_REALTIME, &tk.time_now);
    clock_gettime (CLOCK_REALTIME, &tk.time_last_stimulation);
    tk.elapsed_beginning_trial =
        gld_time_diff (&tk.time_beginning_trial, &tk.time_now);

    /* initialize the stimulation output */
    stimpulse_init ();

    ls_debug ("Start trial loop\n");

    /* loop until the trial is over */
    while (tk.elapsed_beginning_trial.tv_sec < tk.trial_duration_sec) {
        size_t data_slice_pos = 0;

        /* acquire data */
        if (!gld_adc_acquire_data (daq, fftw_inter.real_data_to_fft_size)) {
            fprintf (stderr,
                     "Unable to acquire samples, swr stimulation not possible\n");
            goto out;
        }

        if (offline_data_file == NULL) {
            /* if no new data is available or not enough data to do a fftw */
            while (data_slice_pos < fftw_inter.real_data_to_fft_size) {
                int16_t data;

                /* get data from MAX1133 ADC chip */
                if (gld_adc_get_data (daq, LS_SCAN_CHAN, &data)) {
                    fftw_inter.signal_data[data_slice_pos] = data;
                    data_slice_pos++;
                } else {
                    /* sleep a bit and check for new data */
                    nanosleep (&tk.duration_sleep_when_no_new_data, &tk.req);
                }
            }

            /* set time when the last sample was acquired */
            clock_gettime(CLOCK_REALTIME, &tk.time_last_acquired_data);

            data_slice_pos = 0;

        } else {
            guint i;

            /* get data from a dat file */
            if (last_sample_no == 0) {
                // fill the buffer with the beginning of the file
                if ((data_file_si_get_data_one_channel (&data_file, offline_channel, data_from_file, 0, fftw_inter.real_data_to_fft_size)) != 0) {
                    fprintf (stderr, "Problem with data_file_si_get_data_one_channel, first index: %d, last index: %zu\n",
                             0, fftw_inter.real_data_to_fft_size);
                    goto out;
                }
                last_sample_no = fftw_inter.real_data_to_fft_size;
            } else {
                // get data further on in the dat file
                if ((data_file_si_get_data_one_channel
                     (&data_file, offline_channel, data_from_file,
                      last_sample_no + new_samples_per_read_operation -
                      fftw_inter.real_data_to_fft_size,
                      last_sample_no + new_samples_per_read_operation)) !=
                    0) {
                    fprintf (stderr,
                             "Problem with data_file_si_get_data_one_channel, first index: %ld, last index: %ld\n",
                             last_sample_no + new_samples_per_read_operation - fftw_inter.real_data_to_fft_size,
                             last_sample_no + new_samples_per_read_operation);
                    goto out;
                }
                last_sample_no =
                    last_sample_no + new_samples_per_read_operation;
            }
            // copy the short int array to double array
            for (i = 0; i < fftw_inter.real_data_to_fft_size; i++) {
                fftw_inter.signal_data[i] = data_from_file[i];
            }
        }

#ifdef DEBUG
        clock_gettime (CLOCK_REALTIME, &tk.time_current_new_data);
        tk.duration_previous_current_new_data =
            gld_time_diff (&tk.time_previous_new_data, &tk.time_current_new_data);
        g_printerr ("%ld, last_sample_no: %ld  with interval %lf(us)\n",
                 counter++, last_sample_no,
                 tk.duration_previous_current_new_data.tv_nsec / 1000.0);
        tk.time_previous_new_data = tk.time_current_new_data;
#endif
        // filter for theta and delta
        fftw_interface_theta_apply_filter_theta_delta (&fftw_inter);

        /* to see real and filtered signal
            for(i=0;i < fftw_inter.real_data_to_fft_size;i++)
            {
            printf("aa %lf, %lf\n",fftw_inter.filtered_signal_theta[i], fftw_inter.signal_data[i]);
            }
            return FALSE;
            */
        // get the theta/delta ratio

        theta_delta_ratio =
            fftw_interface_theta_delta_ratio (&fftw_inter);

        ls_debug ("theta_delta_ratio: %lf\n", theta_delta_ratio);

        if (theta_delta_ratio > THETA_DELTA_RATIO) {
            clock_gettime (CLOCK_REALTIME, &tk.time_now);
            tk.elapsed_last_acquired_data =
                gld_time_diff (&tk.time_last_acquired_data, &tk.time_now);
            // get the phase
            current_phase =
                fftw_interface_theta_get_phase (&fftw_inter,
                                                &tk.
                                                elapsed_last_acquired_data,
                                                theta_frequency);
            // phase difference between wanted and what it is now, from -180 to 180
            phase_diff = phase_difference (current_phase, stimulation_theta_phase);

            ls_debug ("stimulation_theta_phase: %lf current_phase: %lf phase_difference: %lf\n",
                     stimulation_theta_phase, current_phase,
                     phase_diff);

            // if the absolute phase difference is smaller than the max_phase_difference
            if (sqrt (phase_diff * phase_diff) < max_phase_diff) {
                // if we are just before the stimulation phase, we nanosleep to be bang on the correct phase
                if (phase_diff < 0) {
                    tk.duration_sleep_to_right_phase =
                        gld_set_timespec_from_ms ((0 -
                                               phase_diff) *
                                              theta_degree_duration_ms);
                    nanosleep (&tk.duration_sleep_to_right_phase,
                               &tk.req);
                }
                clock_gettime (CLOCK_REALTIME, &tk.time_now);
                tk.elapsed_last_stimulation =
                    gld_time_diff (&tk.time_last_stimulation, &tk.time_now);

                // if the laser refractory period is over
                if (tk.elapsed_last_stimulation.tv_nsec >
                    tk.duration_refractory_period.tv_nsec
                    || tk.elapsed_last_stimulation.tv_sec >
                    tk.duration_refractory_period.tv_sec) {
                    // stimulation time!!
                    clock_gettime (CLOCK_REALTIME,
                                   &tk.time_last_stimulation);

                    /* start the pulse */
                    stimpulse_set_trigger_high ();

                    /* wait */
                    nanosleep (&tk.duration_pulse, &tk.req);

                    /* end of the pulse */
                    stimpulse_set_trigger_low ();

                    ls_debug ("interval from last stimulation: %ld (us)\n",
                             tk.elapsed_last_stimulation.tv_nsec /
                             1000);

                }
            }
        }

        clock_gettime (CLOCK_REALTIME, &tk.time_now);
        tk.elapsed_last_acquired_data =
            gld_time_diff (&tk.time_last_acquired_data, &tk.time_now);

        // will stop the trial
        //            tk.elapsed_beginning_trial.tv_sec = tk.trial_duration_sec;
        clock_gettime (CLOCK_REALTIME, &tk.time_now);
        tk.elapsed_beginning_trial =
            gld_time_diff (&tk.time_beginning_trial, &tk.time_now);
    }

    /* this will stop the acquisition thread */
    if (!gld_adc_reset (daq)) {
        fprintf (stderr, "Could not stop data acquisition\n");
        goto out;
    }

    ret = TRUE; /* success */
out:
    /* free the memory used by fftw_inter */
    fftw_interface_theta_free (&fftw_inter);

    /* free daq interface */
    gld_adc_free (daq);

    /* free the memory for dat file data, if running with offline data */
    if (offline_data_file != NULL) {
        if ((clean_data_file_si (&data_file)) != 0) {
            fprintf (stderr, "Problem with clean_data_file_si\n");
            return FALSE;
        }
        free (data_from_file);
    }

    return ret;
}

/**
 * perform_swr_stimulation:
 *
 * Do swr stimulation
 */
gboolean
perform_swr_stimulation (double trial_duration_sec, double pulse_duration_ms, double swr_refractory, double swr_power_threshold, double swr_convolution_peak_threshold,
                         gboolean delay_swr, double minimum_interval_ms, double maximum_interval_ms,
                         const gchar *offline_data_file, int channels_in_dat_file, int offline_channel, int offline_reference_channel)
{
    TimeKeeper tk;
    GldAdc *daq;
    gboolean ret = FALSE;

    /* variables to work offline from a dat file */
    data_file_si data_file;
    int new_samples_per_read_operation = 60; /* 3 ms of data */
    short int *data_from_file = NULL;
    short int *ref_from_file = NULL;
    size_t last_sample_no = 0;

    guint i;

    /* fftw SWR filtering structure */
    struct fftw_interface_swr fftw_inter_swr;


    /* set up timekeeper */
    tk.trial_duration_sec = trial_duration_sec;
    tk.pulse_duration_ms = pulse_duration_ms;

    tk.duration_sleep_when_no_new_data =
        gld_set_timespec_from_ms (SLEEP_WHEN_NO_NEW_DATA_MS);
    tk.duration_pulse = gld_set_timespec_from_ms (tk.pulse_duration_ms);
    tk.interval_duration_between_swr_processing =
        gld_set_timespec_from_ms (INTERVAL_DURATION_BETWEEN_SWR_PROCESSING_MS);

    /* create ADC interface and configure it, run DAQ on CPU 0 */
    daq = gld_adc_new (LS_ADC_CHANNEL_COUNT, LS_DATA_BUFFER_SIZE, 0);
    gld_adc_set_acq_frequency (daq, LS_DEFAULT_SAMPLING_RATE);

    /* initialize fftw interface */
    if (fftw_interface_swr_init (&fftw_inter_swr) == -1) {
        fprintf (stderr, "Could not initialize fftw_interface_swr\n");
        return FALSE;
    }

    if (offline_data_file == NULL) {
        /* initialize the stimulation output */
        stimpulse_init ();
    } else {
        /* the program is running from a data file. allocate memory for 2 arrays. */

        /* initialize the dat file */
        if (init_data_file_si (&data_file, offline_data_file, channels_in_dat_file) != 0) {
            fprintf (stderr, "Problem in initialisation of dat file\n");
            return FALSE;
        }

        if ((data_from_file = (short *) malloc (sizeof (short) * fftw_inter_swr.real_data_to_fft_size)) == NULL) {
            fprintf (stderr,
                     "Problem allocating memory for data_from_file\n");
            return FALSE;
        }
        if ((ref_from_file = (short *) malloc (sizeof (short) * fftw_inter_swr.real_data_to_fft_size)) == NULL) {
            fprintf (stderr,
                     "Problem allocating memory for ref_from_file\n");
            return FALSE;
        }
    }

#ifdef DEBUG
    // to check the intervals before getting new data.
    clock_gettime (CLOCK_REALTIME, &tk.time_previous_new_data);
    long int counter = 0;
#endif

    // get time at beginning of trial
    clock_gettime (CLOCK_REALTIME, &tk.time_beginning_trial);
    clock_gettime (CLOCK_REALTIME, &tk.time_now);
    clock_gettime (CLOCK_REALTIME, &tk.time_last_stimulation);
    tk.elapsed_beginning_trial =
        gld_time_diff (&tk.time_beginning_trial, &tk.time_now);
    tk.duration_refractory_period = gld_set_timespec_from_ms (swr_refractory);    // set the refractory period for stimulation

    // loop until the time is over

    // to check the intervals before getting new data.
    ls_debug ("Starting trial loop for swr\n");

    /* loop while the trial is running */
    while (tk.elapsed_beginning_trial.tv_sec < tk.trial_duration_sec) {
        size_t data_slice_pos = 0;
        size_t data_ref_slice_pos = 0;
        gboolean acquire_data = TRUE;

        double swr_power = 0;
        double swr_convolution_peak = 0;

        if (offline_data_file == NULL) {
            /* get data from our ADC chip */

            /* acquire data */
            if (!gld_adc_acquire_data (daq, fftw_inter_swr.real_data_to_fft_size * 2)) {
                fprintf (stderr,
                         "Unable to acquire samples, swr stimulation not possible\n");
                goto out;
            }

            while (acquire_data) {
                int16_t data;
                int16_t data_ref;
                gboolean data_acquired = FALSE;

                /* get data from ADC chip, data channel */
                if (data_slice_pos < fftw_inter_swr.real_data_to_fft_size) {
                    if (gld_adc_get_data (daq, LS_SCAN_CHAN, &data)) {
                        fftw_inter_swr.signal_data[data_slice_pos] = data;
                        data_slice_pos++;
                        last_sample_no++;
                        data_acquired = TRUE;
                    }
                } else {
                    acquire_data = FALSE;
                }

                /* get data from ADC chip, reference channel */
                if (data_ref_slice_pos < fftw_inter_swr.real_data_to_fft_size) {
                    if (gld_adc_get_data (daq, LS_REF_CHAN, &data_ref)) {
                        fftw_inter_swr.ref_signal_data[data_ref_slice_pos] = data_ref;
                        data_ref_slice_pos++;
                        data_acquired = TRUE;
                    }
                    acquire_data = TRUE;
                } else {
                    acquire_data = FALSE;
                }

                if (data_acquired) {
                    /* sleep a bit, then check for new data */
                    nanosleep (&tk.duration_sleep_when_no_new_data, &tk.req);
                }

                /* set time when the last sample was acquired */
                clock_gettime(CLOCK_REALTIME, &tk.time_last_acquired_data);
            }

            data_slice_pos = 0;
            data_ref_slice_pos = 0;
        } else {
            /* get data from a dat file */

            if (last_sample_no == 0) {
                // fill the buffer with the beginning of the file
                if ((data_file_si_get_data_one_channel
                     (&data_file, offline_channel, data_from_file, 0,
                      fftw_inter_swr.real_data_to_fft_size - 1)) != 0) {
                    fprintf (stderr,
                             "Problem with data_file_si_get_data_one_channel, first index: %d, last index: %zu\n", 0,
                             fftw_inter_swr.real_data_to_fft_size - 1);
                    goto out;
                }
                if ((data_file_si_get_data_one_channel
                     (&data_file, offline_reference_channel, ref_from_file, 0,
                      fftw_inter_swr.real_data_to_fft_size - 1)) != 0) {
                    fprintf (stderr,
                             "Problem with data_file_si_get_data_one_channel, first index: %d, last index: %zu\n", 0,
                             fftw_inter_swr.real_data_to_fft_size - 1);
                    goto out;
                }
                last_sample_no = fftw_inter_swr.real_data_to_fft_size - 1;
            } else {
                /* check if we still have data in the file */
                if (data_file.num_samples_in_file <
                    last_sample_no + new_samples_per_read_operation - 1) {
                    // no more data in file, exit the trial loop
                    break;
                }
                // update the buffer by adding new data in it
                if ((data_file_si_get_data_one_channel
                     (&data_file, offline_channel, data_from_file,
                      last_sample_no + new_samples_per_read_operation -  fftw_inter_swr.real_data_to_fft_size,
                      last_sample_no + new_samples_per_read_operation)) !=  0) {
                    g_printerr ("Problem with data_file_si_get_data_one_channel, first index: %zu, last index: %zu\n",
                                last_sample_no + new_samples_per_read_operation - fftw_inter_swr.real_data_to_fft_size,
                                last_sample_no + new_samples_per_read_operation - 1);
                    goto out;
                }
                if ((data_file_si_get_data_one_channel
                     (&data_file, offline_reference_channel, ref_from_file,
                      last_sample_no + new_samples_per_read_operation - fftw_inter_swr.real_data_to_fft_size,
                      last_sample_no + new_samples_per_read_operation)) != 0) {
                    g_printerr ("Problem with data_file_si_get_data_one_channel, first index: %zu, last index: %zu\n",
                             last_sample_no + new_samples_per_read_operation - fftw_inter_swr.real_data_to_fft_size,
                             last_sample_no + new_samples_per_read_operation - 1);
                    goto out;
                }

                last_sample_no = last_sample_no + new_samples_per_read_operation - 1;
            }

            // copy the short int array to double array
            for (i = 0; i < fftw_inter_swr.real_data_to_fft_size; i++) {
                fftw_inter_swr.signal_data[i] = data_from_file[i];
                fftw_inter_swr.ref_signal_data[i] = ref_from_file[i];
            }
        } /* end dat file */

        if (last_sample_no >= fftw_inter_swr.real_data_to_fft_size) {
            // do differential, filtering, and convolution
            fftw_interface_swr_differential_and_filter (&fftw_inter_swr);

            // get the power
            swr_power = fftw_interface_swr_get_power (&fftw_inter_swr);

            // get the peak in the convoluted signal
            swr_convolution_peak = fftw_interface_swr_get_convolution_peak (&fftw_inter_swr);

            // get the current time for refractory period
            clock_gettime (CLOCK_REALTIME, &tk.time_now);
            tk.elapsed_last_stimulation =
                gld_time_diff (&tk.time_last_stimulation, &tk.time_now);

            if ((swr_power > swr_power_threshold && swr_convolution_peak > swr_convolution_peak_threshold) && /* if power is large enough and refractory over */
                (tk.elapsed_last_stimulation.tv_nsec >
                 tk.duration_refractory_period.tv_nsec
                 || tk.elapsed_last_stimulation.tv_sec >
                 tk.duration_refractory_period.tv_sec)) {

#ifdef DEBUG
                printf ("%ld\n", last_sample_no);
                for (i = 0; i < fftw_inter_swr.real_data_to_fft_size; i++) {
                    g_print ("%lf %lf %lf\n", fftw_inter_swr.signal_data[i],
                            fftw_inter_swr.filtered_signal_swr[i],
                            fftw_inter_swr.convoluted_signal[i]);
                }
                fprintf (stderr,
                         "ps check no: %ld, last_sample_no: %ld  sleep time: %.2lf (us), processing time: %.2lf, diff_between_two_get_data: %.2lf power: %.2lf threshold: %.2lf convolution_peak: %.2lf\n",
                         counter++, last_sample_no,
                         tk.duration_sleep_between_swr_processing.tv_nsec /
                         1000.0,
                         tk.elapsed_from_acquisition.tv_nsec / 1000.0,
                         tk.duration_previous_current_new_data.tv_nsec /
                         1000.0, swr_power, swr_power_threshold,
                         swr_convolution_peak);
                //! goto out;
#endif

                /* stimulation time!! */
                if (offline_data_file == NULL) { /* not working with a data file */
                    /* do we need a delay before swr stimulation */
                    if (delay_swr) {
                        tk.swr_delay_ms = minimum_interval_ms + (rand () % (int) (maximum_interval_ms - minimum_interval_ms));
                        tk.swr_delay = gld_set_timespec_from_ms (tk.swr_delay_ms);
                        g_print ("SWR delay: %lf ms\n", tk.swr_delay_ms);
                        // sleep until the next pulse, note that this will overshoot by the time of 1 lines of code! (almost nothing)
                        nanosleep (&tk.swr_delay, &tk.req);
                    }

                    /* start the pulse */
                    stimpulse_set_trigger_high ();

                    /* wait */
                    nanosleep (&tk.duration_pulse, &tk.req);

                    /* end of the pulse */
                    stimpulse_set_trigger_low ();

                    /* get the time of last stimulation */
                    clock_gettime (CLOCK_REALTIME,
                                   &tk.time_last_stimulation);

                    /* sleep so that the interval between two calculation of power is approximately tk.interval_duration_between_swr_processing */
                    clock_gettime (CLOCK_REALTIME, &tk.time_now);
                    tk.elapsed_from_acquisition = gld_time_diff (&tk.time_last_acquired_data, &tk.time_now);
                    tk.duration_sleep_between_swr_processing = gld_time_diff (&tk.elapsed_from_acquisition, &tk.interval_duration_between_swr_processing);
                    nanosleep (&tk.duration_sleep_between_swr_processing, &tk.req);
                    tk.elapsed_beginning_trial = gld_time_diff (&tk.time_beginning_trial, &tk.time_now);
                } else {
                    /* working with data file */

                    //printf("%ld %lf %lf %lf %ld\n",last_sample_no,swr_power,fftw_inter_swr.mean_power,fftw_inter_swr.std_power,fftw_inter_swr.number_segments_analysed);
                    // print the res value of the stimulation time
                    g_print ("%zu\n", last_sample_no);

                    // move forward in file by the duration of the pulse
                    last_sample_no = last_sample_no + (tk.pulse_duration_ms * LS_DEFAULT_SAMPLING_RATE / 1000);
                }
            }
        }

#ifdef DEBUG
        tk.duration_previous_current_new_data =
            gld_time_diff (&tk.time_previous_new_data, &tk.time_last_acquired_data);
        tk.time_previous_new_data = tk.time_last_acquired_data;
        fprintf (stderr,
                 "ns check no: %ld, last_sample_no: %ld  sleep time: %.2lf (us), processing time: %.2lf, diff_between_two_get_data: %.2lf power: %.2lf threshold: %.2lf convolution_peak: %.2lf\n",
                 counter++, last_sample_no,
                 tk.duration_sleep_between_swr_processing.tv_nsec / 1000.0,
                 tk.elapsed_from_acquisition.tv_nsec / 1000.0,
                 tk.duration_previous_current_new_data.tv_nsec / 1000.0,
                 swr_power, swr_power_threshold, swr_convolution_peak);
#endif

    } /* stimulation trial is over */

    fftw_interface_swr_free (&fftw_inter_swr);
    if (offline_data_file == NULL) {
        if (!gld_adc_reset (daq)) {
            fprintf (stderr, "Could not stop data acquisition\n");
            goto out;
        }
    }

    ret = TRUE;
out:
    /* free daq interface */
    gld_adc_free (daq);

    /* free the memory for dat file data, if running with offline data */
    if (offline_data_file != NULL) {
        if ((clean_data_file_si (&data_file)) != 0) {
            fprintf (stderr, "Problem with clean_data_file_si\n");
            return FALSE;
        }
        free (data_from_file);
        free (ref_from_file);
    }

    return ret;
}
