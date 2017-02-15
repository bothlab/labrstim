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

#include "max1133daq.h"
#include "config.h"

#include <glib.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include <fcntl.h>
#include <bcm2835.h>

#include "timespec-utils.h"

#define BIT(x) (1UL << x)


#define MAX1133_START       BIT(7) /* start byte */
#define MAX1133_UNIPOLAR    BIT(6) /* unipolar mode if set, bipolar if not set */
#define MAX1133_INT_CLOCK   BIT(5) /* use internat clock if set, use external if not set */

#define MAX1133_M1       BIT(4)
#define MAX1133_M0       BIT(3)

#define MAX1133_P2       BIT(2)
#define MAX1133_P1       BIT(1)
#define MAX1133_P0       BIT(0)

static uint8_t max1133_mux_channel_map[8] = {
    0,
    MAX1133_P0,
    MAX1133_P1,
    MAX1133_P2,
    MAX1133_P0 | MAX1133_P1,
    MAX1133_P1 | MAX1133_P2,
    MAX1133_P0 | MAX1133_P2,
    MAX1133_P0 | MAX1133_P1 | MAX1133_P2
};


typedef struct _DataBuffer DataBuffer;

struct _DataBuffer
{
    int16_t *buffer;
    size_t capacity; /* maximum number of items in the buffer */

    size_t wr_pos;   /* write position */
    size_t rd_pos;   /* read position */

    size_t len;      /* length of the data */
};

/**
 * data_buffer_init:
 *
 * Initialize a data ring buffer.
 */
static void
data_buffer_init (DataBuffer *dbuf, size_t capacity)
{
    g_assert (capacity > 0);

    dbuf->buffer = malloc (capacity * sizeof(int16_t));
    if (dbuf->buffer == NULL) {
        g_printerr ("Could not allocate memory for data buffer.");
        exit (8);
        return;
    }

    dbuf->capacity = capacity;
    dbuf->wr_pos = 0;
    dbuf->rd_pos = 0;
}

/**
 * data_buffer_new:
 *
 * Initialize a data ring buffer.
 */
static DataBuffer*
data_buffer_new (size_t capacity)
{
    DataBuffer *dbuf;

    dbuf = g_slice_new0 (DataBuffer);
    data_buffer_init (dbuf, capacity);

    return dbuf;
}

/**
 * data_buffer_free:
 */
static void
data_buffer_free (DataBuffer *dbuf)
{
    g_free (dbuf->buffer);
    g_slice_free (DataBuffer, dbuf);
}

/**
 * data_buffer_reset:
 */
static void
data_buffer_reset (DataBuffer *dbuf)
{
    dbuf->wr_pos = 0;
    dbuf->rd_pos = 0;
    dbuf->len = 0;
}

/**
 * data_buffer_get_size:
 */
LBS_UNUSED static size_t
data_buffer_get_size (DataBuffer *dbuf)
{
    return dbuf->wr_pos;
}

/**
 * data_buffer_push:
 */
static void
data_buffer_push_data (DataBuffer *dbuf, const int16_t value)
{
    static pthread_mutex_t mutex;

    if (dbuf->wr_pos >= dbuf->capacity) {
        /* we reached the end of our buffer slice, start from the beginning */
        dbuf->wr_pos = 0;
    }

    if ((dbuf->wr_pos == dbuf->rd_pos) && (dbuf->len == dbuf->capacity - 1)) {
        g_warning ("Acquiring data faster than we use it, skipped %zu entries.", dbuf->capacity);
        pthread_mutex_lock (&mutex);
        data_buffer_reset (dbuf);
        pthread_mutex_unlock (&mutex);
    }

    dbuf->buffer[dbuf->wr_pos] = value;
    dbuf->wr_pos++;
    dbuf->len++;
}

/**
 * data_buffer_pull_data:
 */
static gboolean
data_buffer_pull_data (DataBuffer *dbuf, int16_t *data)
{
    if (dbuf->rd_pos >= dbuf->capacity) {
        /* we reached the end of our buffer slice, start from the beginning */
        dbuf->rd_pos = 0;
    }

    /* fail if we don't have enough data yet */
    if (dbuf->len == 0)
        return FALSE;
    if (dbuf->rd_pos >= dbuf->wr_pos)
        return FALSE;

    (*data) = dbuf->buffer[dbuf->rd_pos];
    dbuf->len--;
    dbuf->rd_pos++;

    return TRUE;
}

/**
 * daq_thread_main:
 */
static void*
daq_thread_main (void *daq_ptr)
{
    Max1133Daq *daq = (Max1133Daq*) daq_ptr;
    struct timespec start, stop;
    struct timespec daq_time;

    struct timespec delay_time;
    struct timespec max_delay_time;

    size_t sample_count = 0;

    max_delay_time = set_timespec_from_ms (1000 / daq->acq_frequency);
    while (daq->running) {
        int16_t rxval = 0;
        guint i;

        if (clock_gettime (CLOCK_REALTIME, &start) == -1 ) {
            g_critical ("Unable to get realtime DAQ start time.");
            continue;
        }

        /* retrieve data from all channels */
        for (i = 0; i < daq->channel_count; i++) {
            uint8_t txbuf = MAX1133_START;

            if (i < 8) {
                txbuf |= max1133_mux_channel_map[i];
                /* TODO: Set CS */
            } else {
                txbuf |= max1133_mux_channel_map[i - 8];
                /* TODO: Set CS */
            }

            /* synchronous SPI query, two bytes received and stored in int16_t */
            bcm2835_spi_transfernb ((char*) &txbuf, (char*) &rxval, sizeof(int16_t));
            data_buffer_push_data (daq->buffer[i], rxval);
        }

        if (clock_gettime (CLOCK_REALTIME, &stop) == -1 ) {
            g_critical ("Unable to get realtime DAQ stop time.");
            daq->running = FALSE;
            continue;
        }

        daq_time.tv_sec = stop.tv_sec - start.tv_sec;
        daq_time.tv_nsec = stop.tv_nsec - start.tv_nsec;

        sample_count++;
        daq->running = daq->sample_max_count > sample_count;

        if ((daq_time.tv_sec > max_delay_time.tv_sec) && (daq_time.tv_nsec > max_delay_time.tv_nsec)) {
            /* we took too long, get new sample immediately */
            continue;
        }

        delay_time.tv_sec = max_delay_time.tv_sec - daq_time.tv_sec;
        delay_time.tv_nsec = max_delay_time.tv_nsec - daq_time.tv_nsec;

        /* wait the remaining time */
        nanosleep (&delay_time, NULL);
    }

    return NULL;
}

/**
 * max1133daq_new:
 */
Max1133Daq*
max1133daq_new (guint channel_count, size_t buffer_capacity)
{
    Max1133Daq *daq;
    guint i;

    /* we don't support more than 16 channels */
    g_return_val_if_fail (channel_count <= 16, NULL);

    if (buffer_capacity <= 0)
        buffer_capacity = 65536;

    daq = g_slice_new0 (Max1133Daq);
    daq->acq_frequency = 20000; /* default to 20 kHz data acquisition speed */

    /* allocate buffer space */
    daq->buffer = g_malloc_n (channel_count, sizeof(DataBuffer*));
    for (i = 0; i < channel_count; i++)
        daq->buffer[i] = data_buffer_new (buffer_capacity);
    daq->channel_count = channel_count;

    /* initialize bcm2835 interface */
    if (!bcm2835_init ()) {
      g_error ("bcm2835_init failed. Is the software running on the right CPU?");
      max1133daq_free (daq);
      return NULL;
    }

    if (!bcm2835_spi_begin ()) {
      g_error ("bcm2835_spi_begin failed. Is the software running on the right CPU?");
      max1133daq_free (daq);
      return NULL;
    }

	return daq;
}

/**
 * max1133daq_free:
 */
void
max1133daq_free (Max1133Daq *daq)
{
    guint i;
    g_return_if_fail (daq != NULL);

    /* dispose of buffers */
    for (i = 0; i < daq->channel_count; i++)
        data_buffer_free (daq->buffer[i]);
    g_free (daq->buffer);

    g_slice_free (Max1133Daq, daq);

    bcm2835_spi_end ();
    bcm2835_close ();
}

/**
 * max1133daq_set_acq_frequency:
 */
void
max1133daq_set_acq_frequency (Max1133Daq *daq, guint hz)
{
    daq->acq_frequency = hz;
}

/**
 * max1133daq_acquire_data:
 * @sample_count: Number of samples to acquire
 *
 * acquire a fixed amount of samples. Data acquisition happens
 * in a background thread, the function will return immediately.
 */
gboolean
max1133daq_acquire_data (Max1133Daq *daq, size_t sample_count)
{
    int rc;
    g_assert (!daq->running);

    /* start with a fresh buffer, never append data */
    max1133daq_reset (daq);

    /* set up BCM2835 */
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_128); /* the right clock divider for RasPi 3 B+ */
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0); /* chip-select 0 */
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);

    daq->sample_max_count = sample_count;
    daq->running = TRUE;
    rc = pthread_create (&daq->tid, NULL, &daq_thread_main, daq);
    if (rc) {
        daq->running = FALSE;
        g_error ("Return code from pthread_create() is %d\n", rc);
        return FALSE;
    }

    return TRUE;
}

/**
 * max1133daq_reset:
 */
gboolean
max1133daq_reset (Max1133Daq *daq)
{
    guint i;
    daq->running = FALSE;
    daq->tid = 0;

    for (i = 0; i < daq->channel_count; i++)
        data_buffer_reset (daq->buffer[i]);

    return TRUE;
}

/**
 * max1133daq_is_running:
 */
gboolean
max1133daq_is_running (Max1133Daq *daq)
{
    return daq->running;
}

/**
 * max1133daq_get_data:
 */
gboolean
max1133daq_get_data (Max1133Daq *daq, guint channel, int16_t *data)
{
    g_assert (channel < daq->channel_count);
    return data_buffer_pull_data (daq->buffer[channel], data);
}
