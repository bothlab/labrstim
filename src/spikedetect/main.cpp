/*
 * Copyright (C) 2024 Matthias Klumpp <matthias@tenstral.net>
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

#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <kfr/base.hpp>
#include <kfr/dsp.hpp>
#include <kfr/io.hpp>

extern "C" {
#include <galdur.h>
#include "../utils.h"
#include "../defaults.h"
#include "../stimpulse.h"
#include "../data-file-si.h"
}

class SlidingWindowCounter
{
private:
    std::vector<bool> m_buffer;
    uint m_windowSizeMs;
    uint m_currentIndex;
    uint m_eventCount;
    uint64_t m_lastTimestampMs;

    int64_t m_pinnedTimestampMs;

    uint64_t currentTimestampMs() const
    {
        if (m_pinnedTimestampMs >= 0)
            return m_pinnedTimestampMs;
        else
            return std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now())
                .time_since_epoch()
                .count();
    }

    void updateWindow()
    {
        const auto currentTimeMs = currentTimestampMs();
        const int64_t elapsedTimeMs = currentTimeMs - m_lastTimestampMs;

        if (elapsedTimeMs <= 0)
            return;

        if (elapsedTimeMs >= m_windowSizeMs) {
            // if more time has passed than the window size, reset everything
            std::fill(m_buffer.begin(), m_buffer.end(), false);
            m_eventCount = 0;
        } else {
            // update the buffer for the elapsed time
            for (int i = 0; i < elapsedTimeMs; ++i) {
                m_currentIndex = (m_currentIndex + 1) % m_windowSizeMs;
                if (m_buffer[m_currentIndex]) {
                    m_buffer[m_currentIndex] = false;
                    m_eventCount--;
                }
            }
        }

        m_lastTimestampMs = currentTimeMs;
    }

public:
    explicit SlidingWindowCounter(uint milliseconds)
        : m_windowSizeMs(milliseconds),
          m_currentIndex(0),
          m_eventCount(0),
          m_lastTimestampMs(0),
          m_pinnedTimestampMs(-1)
    {
        m_buffer.resize(m_windowSizeMs, false);
    }

    void setPinnedTimestampMs(int64_t timestampMs)
    {
        m_pinnedTimestampMs = timestampMs;
    }

    void addEvent()
    {
        updateWindow();

        if (!m_buffer[m_currentIndex]) {
            m_buffer[m_currentIndex] = true;
            m_eventCount++;
        }
    }

    uint currentEventCount()
    {
        updateWindow();
        return m_eventCount;
    }

    uint currentFrequencyHz()
    {
        updateWindow();
        return (m_eventCount * 1000) / m_windowSizeMs;
    }
};

static bool run_spikedetect(
    int samplingRateHz,
    double trialDurationSec,
    double pulseDurationMs,
    int triggerFrequencyHz,
    int timeWindowMsec,
    int cooldownTimeMsec,
    int spikeThreshold = -2500,
    const gchar *offlineDataFile = nullptr,
    int channelsInDatFile = 1,
    int offlineChannel = 0)
{
    TimeKeeper tk;
    GldAdc *daq;
    bool ret = false;

    if (samplingRateHz <= 0)
        samplingRateHz = LS_DEFAULT_SAMPLING_RATE;
    if (timeWindowMsec <= 0)
        timeWindowMsec = 1000;
    if (cooldownTimeMsec <= 0)
        cooldownTimeMsec = 20;
    if (triggerFrequencyHz <= 0) {
        fprintf(stderr, "Trigger frequency can not be negative!\n");
        return false;
    }

    tk.trial_duration_sec = trialDurationSec;
    tk.pulse_duration_ms = pulseDurationMs;

    tk.duration_pulse = gld_set_timespec_from_ms(tk.pulse_duration_ms);
    tk.duration_refractory_period = gld_set_timespec_from_ms(cooldownTimeMsec);

    std::vector<double> offlineData;
    size_t offlineDataIndex = 0;
    if (offlineDataFile != nullptr) {
        data_file_si data_file;

        // initialize the dat file
        if (init_data_file_si(&data_file, offlineDataFile, 1) != 0) {
            fprintf(stderr, "Problem in initialisation of dat file\n");
            return false;
        }

        for (size_t i = 1; i < data_file.num_samples_in_file; i++) {
            uint16_t buffer[1];
            if (data_file_si_get_data_one_channel(&data_file, offlineChannel, (short int *)&buffer, i - 1, i) != 0) {
                fprintf(
                    stderr,
                    "Problem with data_file_si_get_data_one_channel, first index: %zu, last index: %zu\n",
                    i - 1,
                    i);
                return false;
            }

            offlineData.push_back((float)buffer[0] - std::pow(2, 15));
        }

        if ((clean_data_file_si(&data_file)) != 0) {
            fprintf(stderr, "Problem with clean_data_file_si\n");
            return false;
        }
    }

    // configure ADC, run DAQ on CPU 0
    daq = gld_adc_new(LS_ADC_CHANNEL_COUNT, LS_DATA_BUFFER_SIZE, 0);
    gld_adc_set_acq_frequency(daq, samplingRateHz);
    gld_adc_set_nodata_sleep_time(daq, gld_set_timespec_from_ms(SLEEP_WHEN_NO_NEW_DATA_MS));

    // get time at beginning of trial
    clock_gettime(CLOCK_REALTIME, &tk.time_beginning_trial);
    clock_gettime(CLOCK_REALTIME, &tk.time_now);
    clock_gettime(CLOCK_REALTIME, &tk.time_last_stimulation);
    tk.elapsed_beginning_trial = gld_time_diff(&tk.time_beginning_trial, &tk.time_now);

    if (!offlineDataFile) {
        // initialize the stimulation output
        stimpulse_init();

        // acquire data
        if (!gld_adc_acquire_samples(daq, -1)) {
            fprintf(stderr, "Unable to acquire samples, spike stimulation not possible\n");
            gld_adc_free(daq);
            return false;
        }
    }

    ls_debug("Start trial loop\n");

    // create a FIR bandpass filter with 127 taps
    kfr::univector<double, 127> taps127;
    kfr::expression_handle<double> kaiser = kfr::to_handle(kfr::window_kaiser(taps127.size(), 3.0));

    kfr::fir_bandpass(taps127, 700.0 / samplingRateHz, 5000.0 / samplingRateHz, kaiser, true);
    kfr::filter_fir<double> filter(taps127);

    SlidingWindowCounter peakCounter(timeWindowMsec);
    static const int CHUNK_SIZE = 3;

    // loop until the trial is over
    double filterBuffer[CHUNK_SIZE * 2] = {0};
    while (tk.elapsed_beginning_trial.tv_sec < tk.trial_duration_sec) {
        double samples[CHUNK_SIZE];

        // jump to front and start sampling new data
        gld_adc_skip_to_front(daq, LS_SCAN_CHAN);

        if (offlineDataFile == nullptr) {
            // get fixed size of samples from the data buffer
            gld_adc_get_samples_double(daq, LS_SCAN_CHAN, samples, CHUNK_SIZE);

            // set time when the last sample was acquired
            clock_gettime(CLOCK_REALTIME, &tk.time_last_acquired_data);
            peakCounter.setPinnedTimestampMs(gld_milliseconds_from_timespec(&tk.time_last_acquired_data));
        } else {
            if (offlineDataIndex + CHUNK_SIZE >= offlineData.size())
                break;
            for (double &sample : samples)
                sample = offlineData[offlineDataIndex++];

            peakCounter.setPinnedTimestampMs((offlineDataIndex * 1000) / samplingRateHz);
        }

        // apply filter in place
        filter.apply(samples);

        // check if any value in our buffer is sub-threshold, null everything else for simplicity
        bool peakThresholdReached = false;
        for (double &sample : samples) {
            if (sample < spikeThreshold)
                peakThresholdReached = true;
            else
                sample = 0;
        }

        // update our buffer of filtered samples
        std::memmove(filterBuffer, filterBuffer + CHUNK_SIZE, CHUNK_SIZE * sizeof(double));
        // copy new samples to the end of the buffer
        std::memcpy(filterBuffer + CHUNK_SIZE, samples, CHUNK_SIZE * sizeof(double));

        // check the old values for the peak threshold as well, just in case
        if (!peakThresholdReached) {
            for (int i = CHUNK_SIZE; i < CHUNK_SIZE * 2; ++i) {
                if (filterBuffer[i] < spikeThreshold) {
                    peakThresholdReached = true;
                    break;
                }
            }
        }

        // if we are above threshold, check if we saw a peak, and if so, register it
        if (peakThresholdReached) {
            for (int i = CHUNK_SIZE - 1; i < (CHUNK_SIZE * 2) - 1; ++i) {
                // check if the next value indicates an increase
                if (filterBuffer[i - 1] > filterBuffer[i] && filterBuffer[i + 1] >= filterBuffer[i]) {
                    peakCounter.addEvent();
                    break;
                }
            }
        }

        const auto currentPeakFrequencyHz = peakCounter.currentFrequencyHz();
        if (currentPeakFrequencyHz >= (uint)triggerFrequencyHz) {
            clock_gettime(CLOCK_REALTIME, &tk.time_now);
            tk.elapsed_last_stimulation = gld_time_diff(&tk.time_last_stimulation, &tk.time_now);

            if (tk.elapsed_last_stimulation.tv_nsec > tk.duration_refractory_period.tv_nsec
                && tk.elapsed_last_stimulation.tv_sec > tk.duration_refractory_period.tv_sec) {

                // stimulation time!!
                clock_gettime(CLOCK_REALTIME, &tk.time_last_stimulation);

                if (offlineDataFile == nullptr) {
                    /* start the pulse */
                    stimpulse_set_trigger_high();

                    /* wait */
                    nanosleep(&tk.duration_pulse, &tk.req);

                    /* end of the pulse */
                    stimpulse_set_trigger_low();
                }
            }
        }

        clock_gettime(CLOCK_REALTIME, &tk.time_now);
        tk.elapsed_beginning_trial = gld_time_diff(&tk.time_beginning_trial, &tk.time_now);
    }

    // stop data acquisition
    if (!gld_adc_reset(daq)) {
        fprintf(stderr, "Could not stop data acquisition\n");
        goto out;
    }

    // success
    ret = true;
out:
    /* free daq interface */
    gld_adc_free(daq);

    return ret;
}

/**
 * labrstim_get_stim_parameters:
 *
 * Obtain additional session parameters.
 */
static gboolean labrstim_get_stim_parameters(
    char **argv,
    int argc,
    int *sampling_rate_hz,
    double *trial_duration_sec,
    double *pulse_duration_ms,
    double *laser_intensity)
{
    double trial_dur_s;
    double pulse_dur_ms;
    double laser_int;
    int sample_freq;

    /* strip the "--" positional arguments separator */
    for (gint i = 0; i < argc; i++) {
        if (g_strcmp0(argv[i], "--") == 0) {
            gint j;

            /* shift elements to the left */
            for (j = i; j < argc - 1; j++)
                argv[j] = argv[j + 1];

            argv[j] = NULL;
            argc--;
            break;
        }
    }

    /* check if we have the required number of arguments */
    if (argc != 5) {
        gint i;
        g_printerr("Usage for %s is \n", "labrstim-spikedetect");
        g_printerr(
            "%s %s [sampling frequency (Hz)] [trial duration (sec)] [pulse duration (ms)] [laser intensity]\n",
            argv[0],
            argv[1]);
        g_printerr("You need %d arguments but gave %d arguments:", 4, argc - 1);
        for (i = 1; i < argc; i++) {
            g_printerr(" %s", argv[i]);
        }
        g_printerr("\n");
        return FALSE;
    }

    /* parse the arguments from the command line */
    sample_freq = g_ascii_strtoll(argv[1], NULL, 10);
    trial_dur_s = g_strtod(argv[2], NULL);
    pulse_dur_ms = g_strtod(argv[3], NULL);
    laser_int = g_strtod(argv[4], NULL);

    if (sample_freq <= 1000 || sample_freq > 200000) {
        g_printerr("Sampling frequency must be between 1000 and 200000.\nYou gave %i\n", sample_freq);
        return FALSE;
    }
    if (trial_dur_s <= -2 || trial_dur_s > 86400) {
        g_printerr(
            "Trial duration should be between 0 and 86400 sec, or -1 for infinite.\nYou gave %lf\n", trial_dur_s);
        return FALSE;
    }
    if (trial_dur_s < 0)
        trial_dur_s = DBL_MAX;
    if (pulse_dur_ms < 0 || pulse_dur_ms > 10000) {
        g_printerr("Pulse_duration_ms should be between 0 and 10000 ms.\nYou gave %lf\n", pulse_dur_ms);
        return FALSE;
    }
    if (laser_int <= 0 || laser_int > 65535) {
        g_printerr("Laser power should be between 0 and 65535.\nYou gave %lf\n", laser_int);
        return FALSE;
    }

    (*sampling_rate_hz) = sample_freq;
    (*trial_duration_sec) = trial_dur_s;
    (*pulse_duration_ms) = pulse_dur_ms;
    (*laser_intensity) = laser_int;

    return TRUE;
}

/**
 * main:
 */
int main(int argc, char *argv[])
{
    GError *error = NULL;
    GOptionContext *opt_context;

    int sampling_rate_hz;
    double trial_duration_sec;
    double pulse_duration_ms;
    double laser_intensity_volt;

    static gboolean opt_show_version = FALSE;
    static gboolean opt_verbose_mode = FALSE;
    static gchar *opt_dat_filename = NULL;
    static int opt_channels_in_dat_file = -1;
    static int opt_offline_channel = -1;

    static int opt_trigger_frequency_hz = -1;
    static int opt_time_window_msec = -1;
    static int opt_cooldown_time_msec = 10;
    static int opt_spike_threshold = -2500;

    const GOptionEntry base_options[] = {
        {"version", 0, 0, G_OPTION_ARG_NONE, &opt_show_version, "Show the program version.", NULL},
        {"verbose", (gchar)0, 0, G_OPTION_ARG_NONE, &opt_verbose_mode, "Show extra debugging information.", NULL},
        {"offline",
         'o', 0,
         G_OPTION_ARG_FILENAME, &opt_dat_filename,
         "Use a .dat file as input data. You also need option -c, together with either -s or -t, to work with -o", "dat_file_name"},
        {"channels_in_dat_file",
         'c', 0,
         G_OPTION_ARG_INT, &opt_channels_in_dat_file,
         "The number of channels in the dat file. Use only when working offline from a dat file (-o or --offline)", "number"},
        {"offline_channel",
         'x', 0,
         G_OPTION_ARG_INT, &opt_offline_channel,
         "The channel on which swr detection is done when working offline from a dat file (-o and -s)", "number"},

        {"trigger-freq-hz",
         't', 0,
         G_OPTION_ARG_INT, &opt_trigger_frequency_hz,
         "The frequency at which stimulation should be triggered.", "number"},
        {"time-window-msec",
         'w', 0,
         G_OPTION_ARG_INT, &opt_time_window_msec,
         "Time window in milliseconds in which the given spike frequency must be reached.", "number"},
        {"cooldown-time-msec",
         'd', 0,
         G_OPTION_ARG_INT, &opt_cooldown_time_msec,
         "Wait time between stimulations in msec.", "number"},
        {"spike-threshold",
         's', 0,
         G_OPTION_ARG_INT, &opt_spike_threshold,
         "Threshold value for a spike to be detected.", "number"},

        {NULL}
    };

    opt_context = g_option_context_new("- Laser Brain Stimulator, Spike Detection");

    /* parse our options */
    g_option_context_add_main_entries(opt_context, base_options, NULL);

    if (!g_option_context_parse(opt_context, &argc, &argv, &error)) {
        g_printerr("option parsing failed: %s\n", error->message);
        return 1;
    }
    if (!labrstim_get_stim_parameters(
            argv, argc, &sampling_rate_hz, &trial_duration_sec, &pulse_duration_ms, &laser_intensity_volt))
        return 1;

    if (opt_show_version) {
        g_print("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
        g_print("%s\n", PACKAGE_COPYRIGHT);
        g_print(
            "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n"
            "This is free software: you are free to change and redistribute it.\n"
            "There is NO WARRANTY, to the extent permitted by law.\n");
        return 0;
    }

    if (opt_dat_filename == NULL) {
        /* give the program realtime priority if we are not running from an offline file */
        if (!labrstim_make_realtime("labrstim-spikedetect"))
            return 5;

        /* initialize DAQ board */
        if (!gld_board_initialize())
            return 5;

        stimpulse_set_intensity(laser_intensity_volt);
    }

    bool success = run_spikedetect(
        sampling_rate_hz,
        trial_duration_sec,
        pulse_duration_ms,
        opt_trigger_frequency_hz,
        opt_time_window_msec,
        opt_cooldown_time_msec,
        opt_spike_threshold,
        opt_dat_filename,
        opt_channels_in_dat_file,
        opt_offline_channel);

    // clear Galdur board state
    if (opt_dat_filename == NULL)
        gld_board_shutdown();

    return success ? 0 : 1;
}
