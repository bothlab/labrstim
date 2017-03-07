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
    volatile gboolean running;

    guint acq_frequency;
    size_t sample_max_count;
} GldAdc;

GldAdc      *gld_adc_new (guint channel_count,
                                 size_t buffer_capacity);
void            gld_adc_free (GldAdc *daq);

void            gld_adc_set_acq_frequency (GldAdc *daq,
                                              guint hz);

gboolean        gld_adc_acquire_data (GldAdc *daq,
                                         size_t sample_count);
gboolean        gld_adc_reset (GldAdc *daq);
gboolean        gld_adc_is_running (GldAdc *daq);

gboolean        gld_adc_get_data (GldAdc *daq,
                                     guint channel,
                                     int16_t *data);


#endif /* __GLD_ADC_H */
