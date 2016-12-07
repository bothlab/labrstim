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

#ifndef __LS_MAX1133_DAQ_H
#define __LS_MAX1133_DAQ_H

#include <glib.h>
#include <stdint.h>

typedef struct
{
    struct _DataRingBuffer *rb;
    pthread_t tid;
    volatile gboolean running;

    int spi_fd;
} Max1133Daq;

Max1133Daq      *max1133daq_new (const gchar *spi_device);
void            max1133daq_free (Max1133Daq *daq);

gboolean        max1133daq_start (Max1133Daq *daq);
gboolean        max1133daq_stop (Max1133Daq *daq);
gboolean        max1133daq_is_running (Max1133Daq *daq);

gboolean        max1133daq_get_data (Max1133Daq *daq,
                                     int16_t *data);


#endif /* __LS_MAX1133_DAQ_H */
