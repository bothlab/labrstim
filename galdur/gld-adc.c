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

#include "gld-adc.h"
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

#include "bcm2835.h"
#include "gld-utils.h"

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
    MAX1133_P0 | MAX1133_P1,
    MAX1133_P2,
    MAX1133_P0 | MAX1133_P2,
    MAX1133_P1 | MAX1133_P2,
    MAX1133_P0 | MAX1133_P1 | MAX1133_P2
};

static void*    daq_thread_main (void *daq_ptr);

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
static inline gboolean
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
 * gld_adc_new:
 */
GldAdc*
gld_adc_new (guint channel_count, size_t buffer_capacity, int cpu_affinity)
{
    GldAdc *daq;
    guint i;
    int rc;

    /* we don't support more than 16 channels */
    g_return_val_if_fail (channel_count <= 16, NULL);

    if (buffer_capacity <= 0)
        buffer_capacity = 65536;

    daq = g_slice_new0 (GldAdc);
    daq->acq_frequency = 20000; /* default to 20 kHz data acquisition speed */

    /* allocate buffer space */
    daq->buffer = g_malloc_n (channel_count, sizeof(DataBuffer*));
    for (i = 0; i < channel_count; i++)
        daq->buffer[i] = data_buffer_new (buffer_capacity);
    daq->channel_count = channel_count;

    /* initialize RNG if we are faking data */
#ifdef SIMULATE_DATA
    srand (time(NULL));
#endif

    daq->cpu_affinity = cpu_affinity;

    /* default duration we sleep when we could not fetch new data */
    gld_adc_set_nodata_sleep_time (daq, gld_set_timespec_from_ms (0.2));

    /* create DAQ thread */
    daq->shutdown = FALSE;
    daq->running = FALSE;
    rc = pthread_create (&daq->tid, NULL, &daq_thread_main, daq);
    if (rc) {
        g_error ("Return code from pthread_create() is %d\n", rc);
        return FALSE;
    }

	return daq;
}

/**
 * gld_adc_free:
 */
void
gld_adc_free (GldAdc *daq)
{
    guint i;
    g_return_if_fail (daq != NULL);

    /* make DAQ thread terminate */
    daq->running  = FALSE;
    daq->shutdown = TRUE;

    /* dispose of buffers */
    for (i = 0; i < daq->channel_count; i++)
        data_buffer_free (daq->buffer[i]);
    g_free (daq->buffer);

    g_slice_free (GldAdc, daq);
}

/**
 * gld_adc_set_acq_frequency:
 */
void
gld_adc_set_acq_frequency (GldAdc *daq, guint hz)
{
    daq->acq_frequency = hz;
}

/**
 * gld_adc_set_nodata_sleep_time:
 *
 * Set time we sleep when we want to acquire a sample of a specified
 * length but do not acquire data quickly enough.
 */
void
gld_adc_set_nodata_sleep_time (GldAdc *daq, struct timespec time)
{
    daq->nodata_sleep_time = time;
}

/**
 * gld_adc_acquire_oneshot:
 */
static inline void
gld_adc_acquire_oneshot (GldAdc *daq)
{
    int16_t rxval = 0;
    guint i;

    /* retrieve data from all channels */
    for (i = 0; i < daq->channel_count; i++) {
        uint8_t txbuf = MAX1133_START;
        int chan;

        if (i < 8) {
            if (i == 0) {
                if (daq->channel_count >= 8)
                    chan = 7;
                else
                    chan = daq->channel_count - 1;
            } else {
                chan = i - 1;
            }
            txbuf |= max1133_mux_channel_map[i];

            /* select MAX 1133 1 */
            bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
        } else {
            if ((i - 8) == 0) {
                chan = daq->channel_count - 1;
            } else {
                chan = i - 1;
            }
            txbuf |= max1133_mux_channel_map[i - 8];

            /* select MAX 1133 2 */
            bcm2835_spi_chipSelect(BCM2835_SPI_CS1);
        }

#ifndef SIMULATE_DATA
        /* synchronous SPI query, two bytes received and stored in int16_t */
        bcm2835_spi_transfernb ((char*) &txbuf, (char*) &rxval, sizeof(int16_t));
#else
        if ((chan % 2) == 0)
            rxval = rand () % (600 * (chan + 1));
        else
            rxval = (rand () % (600 * (chan + 1))) * -1;
#endif
        data_buffer_push_data (daq->buffer[chan], rxval);
    }
}

/**
 * daq_thread_main:
 */
static void*
daq_thread_main (void *daq_ptr)
{
    GldAdc *daq = (GldAdc*) daq_ptr;
    struct timespec start, stop;
    struct timespec daq_time;

    struct timespec delay_time;
    struct timespec max_delay_time;

    size_t sample_count = 0;
    gboolean continuous_sampling;

    /* make the DAQ thread use the selected core */
    if (daq->cpu_affinity >= 0) {
        if (gld_set_thread_cpu_affinity (0) < 0) {
            g_critical ("Unable to set CPU core affinity for DAQ thread.");
        }
    }

    max_delay_time = gld_set_timespec_from_ms (1000 / daq->acq_frequency);

    continuous_sampling = daq->sample_max_count < 0;
    while (TRUE) {
        if (!daq->running) {
            /* we are not acquiring data - check if we should terminate */
            if (daq->shutdown)
                break;
            sample_count = 0;
            continuous_sampling = daq->sample_max_count < 0;
            continue;
        }

        if (clock_gettime (CLOCK_REALTIME, &start) == -1 ) {
            g_critical ("Unable to get realtime DAQ start time.");
            continue;
        }

        /* acquire a single set of data */
        gld_adc_acquire_oneshot (daq);

        if (clock_gettime (CLOCK_REALTIME, &stop) == -1 ) {
            g_critical ("Unable to get realtime DAQ stop time.");
            daq->running = FALSE;
            continue;
        }

        daq_time.tv_sec = stop.tv_sec - start.tv_sec;
        daq_time.tv_nsec = stop.tv_nsec - start.tv_nsec;

        if (!continuous_sampling) {
            sample_count++;
            daq->running = daq->sample_max_count > sample_count;
        }

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
 * gld_adc_acquire_samples:
 * @sample_count: Number of samples to acquire, -1 for continuous sampling
 *
 * Acquire a fixed amount of samples. Data acquisition happens
 * in a background thread, the function will return immediately.
 */
gboolean
gld_adc_acquire_samples (GldAdc *daq, ssize_t sample_count)
{
    g_assert (!daq->running);

    /* start with a fresh buffer, never append data */
    gld_adc_reset (daq);

    daq->sample_max_count = sample_count;
    daq->running = TRUE;

    return TRUE;
}

/**
 * gld_adc_acquire_single_dataset:
 *
 * Immediately acquire a single dataset.
 */
void
gld_adc_acquire_single_dataset (GldAdc *daq)
{
    g_assert (!daq->running);
    gld_adc_acquire_oneshot (daq);
}

/**
 * gld_adc_reset:
 */
gboolean
gld_adc_reset (GldAdc *daq)
{
    guint i;
    daq->running = FALSE;
    daq->tid = 0;

    for (i = 0; i < daq->channel_count; i++)
        data_buffer_reset (daq->buffer[i]);

    return TRUE;
}

/**
 * gld_adc_is_running:
 */
gboolean
gld_adc_is_running (GldAdc *daq)
{
    return daq->running;
}

/**
 * gld_adc_get_sample:
 *
 * Get a single samle in @data from channel @channel
 *
 * Returns: %TRUE if we could successfully fetch data, %FALSE otherwise.
 */
gboolean
gld_adc_get_sample (GldAdc *daq, guint channel, int16_t *data)
{
    g_assert (channel < daq->channel_count);
    return data_buffer_pull_data (daq->buffer[channel], data);
}


/**
 * gld_adc_get_samples:
 *
 * Get @samples_len samples in @samples for channel @channel
 * This function will block until the requested number of samples has
 * been added to the buffer.
 */
void
gld_adc_get_samples (GldAdc *daq, guint channel, int16_t *samples, size_t samples_len)
{
    size_t sample_no = 0;

    while (sample_no < samples_len) {
        int16_t data;
        if (data_buffer_pull_data (daq->buffer[channel], &data)) {
            samples[sample_no] = data;
            sample_no++;
        } else {
            /* sleep a little, as we have no data in the buffer */
            nanosleep (&daq->nodata_sleep_time, NULL);
        }
    }
}

/**
 * gld_adc_get_samples_double:
 *
 * Same as %gld_adc_get_samples, but using an array of doubles.
 */
void
gld_adc_get_samples_double (GldAdc *daq, guint channel, double *samples, size_t samples_len)
{
    size_t sample_no = 0;

    while (sample_no < samples_len) {
        int16_t data;
        if (data_buffer_pull_data (daq->buffer[channel], &data)) {
            samples[sample_no] = data;
            sample_no++;
        } else {
            /* sleep a little, as we have no data in the buffer */
            nanosleep (&daq->nodata_sleep_time, NULL);
        }
    }
}

/**
 * gld_adc_get_samples_float:
 *
 * Same as %gld_adc_get_samples, but using an array of floats.
 */
void
gld_adc_get_samples_float (GldAdc *daq, guint channel, float *samples, size_t samples_len)
{
    size_t sample_no = 0;

    while (sample_no < samples_len) {
        int16_t data;
        if (data_buffer_pull_data (daq->buffer[channel], &data)) {
            samples[sample_no] = data;
            sample_no++;
        } else {
            /* sleep a little, as we have no data in the buffer */
            nanosleep (&daq->nodata_sleep_time, NULL);
        }
    }
}

/**
 * gld_adc_skip_to_front:
 *
 * Skip to the front of the selected buffer, ignoring all previously
 * recorded data.
 */
void
gld_adc_skip_to_front (GldAdc *daq, guint channel)
{
    static pthread_mutex_t mutex;

    pthread_mutex_lock (&mutex);
    data_buffer_reset (daq->buffer[channel]);
    pthread_mutex_unlock (&mutex);
}
