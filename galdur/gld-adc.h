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

#ifndef __GLD_ADC_H
#define __GLD_ADC_H

#include <glib.h>
#include <stdint.h>

typedef struct
{
    struct _DataBuffer **buffer;
    guint       channel_count;
    pthread_t   tid;

    guint acq_frequency;
    ssize_t sample_max_count;

    int cpu_affinity;
    struct timespec nodata_sleep_time;
    volatile gboolean running;
    volatile gboolean shutdown;
} GldAdc;

GldAdc          *gld_adc_new (guint channel_count,
                              size_t buffer_capacity,
                              int cpu_affinity);
void            gld_adc_free (GldAdc *daq);

void            gld_adc_set_acq_frequency (GldAdc *daq,
                                              guint hz);
void            gld_adc_set_nodata_sleep_time (GldAdc *daq,
                                               struct timespec time);

void            gld_adc_acquire_single_dataset (GldAdc *daq);
gboolean        gld_adc_acquire_samples (GldAdc *daq,
                                         ssize_t sample_count);

gboolean        gld_adc_reset (GldAdc *daq);
gboolean        gld_adc_is_running (GldAdc *daq);

gboolean        gld_adc_get_sample (GldAdc *daq,
                                     guint channel,
                                     int16_t *data);

void            gld_adc_get_samples (GldAdc *daq,
                                     guint channel,
                                     int *samples,
                                     size_t samples_len);
void            gld_adc_get_samples_double (GldAdc *daq,
                                            guint channel,
                                            double *samples,
                                            size_t samples_len);
void            gld_adc_get_samples_float (GldAdc *daq,
                                           guint channel,
                                           float *samples,
                                           size_t samples_len);

void            gld_adc_skip_to_front (GldAdc *daq,
                                       guint channel);


#endif /* __GLD_ADC_H */
