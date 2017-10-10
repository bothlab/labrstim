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

#ifndef __LS_DEFAULTS_H
#define __LS_DEFAULTS_H

/* size of the Galdur ADC ring buffer (2 bytes per int16 -> ~128MB buffer) */
#define LS_DATA_BUFFER_SIZE 1024 * 1024 * 64

/* ADC sampling speed per channel */
#define LS_DEFAULT_SAMPLING_RATE 20000 /* 20kHz */

/* channel defaults */
#define LS_ADC_CHANNEL_COUNT 2
#define LS_SCAN_CHAN 0 /* analyze channel */
#define LS_REF_CHAN  1 /* reference channel */

/* defaults for theta detection */
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

/* defaults for SWR detection */
#define FFT_SIGNAL_DATA_SIZE_SWR 1024 // number of data points that goes into the fft, needs to be a 2^x number
#define REAL_DATA_IN_SEGMENT_TO_FFT_SWR 512 // the is the number of singal data point that will go in the fft
                                            // the rest will be filled with 0. window size
                                            // should be a number smaller or equal  FFT_SIGNAL_DATA_SIZE_SWR
#define DATA_IN_SEGMENT_TO_POWER_SWR 256 // this is the length of the segment on which the power detection is based on.
                                         // should not be larger than REAL_DATA_IN_SEGMENT_TO_FFT_SWR

#define MIN_FREQUENCY_SWR 125 // default minimum frequency for ripple detection
#define MAX_FREQUENCY_SWR 250 // default maximum frequency for ripple detection
#define FREQUENCY_WAVELET_FOR_CONVOLUTION 160
#define SLEEP_WHEN_NO_NEW_DATA_MS 0.2  // 0.2 is ok, probably best not to change that
#define INTERVAL_DURATION_BETWEEN_SWR_PROCESSING_MS 4 // the program will sleep 4 ms between each calculation of ripple power
#define SIZE_ROOT_MEAN_SQUARE_ARRAY 10000

/* RT process defaults */
#define LS_PRIORITY     49      /* we use 49 as the PRREMPT_RT use 50
                                   as the priority of kernel tasklets
                                   and interrupt handler by default */
#define LSTTYCTL_PRIORITY 28   /* priority of the tty communication bridge */

#define MAX_SAFE_STACK (8*1024) /* The maximum stack size which is
                                   guranteed safe to access without
                                   faulting */

#define NSEC_PER_SEC    (1000000000) /* The number of nsecs per sec. */
#define NANOSLEEP_OVERSHOOT 0.0080000

#define PI_TTY_CTLPORT "/dev/ttyAMA0"

#endif /* __LS_DEFAULTS_H */
