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

#include <glib.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include <fcntl.h>

#define MAX1133_CONTROL_BYTE 0b10000000;

typedef struct _DataRingBuffer DataRingBuffer;

struct _DataRingBuffer
{
    int16_t *buffer;
    size_t capacity; /* maximum number of items in the buffer */
    size_t len;      /* number of items in the buffer */
    volatile size_t wr_pos;   /* position of the reader */
    volatile size_t rd_pos;   /* position of the writer */
};

/**
 * rb_init:
 *
 * Initialize data ring buffer.
 */
static void
rb_init (DataRingBuffer *rb, size_t capacity)
{
    g_assert (capacity > 0);

    rb->buffer = malloc (capacity * sizeof(int16_t));
    if (rb->buffer == NULL) {
        g_printerr("Could not allocate memory for data buffer.");
        exit (8);
        return;
    }

    rb->capacity = capacity;
    rb->len = 0;
    rb->wr_pos = 0;
    rb->rd_pos = 0;
}

/**
 * rb_free:
 */
static void
rb_free (DataRingBuffer *rb)
{
    free (rb->buffer);
}

/**
 * rb_reset:
 */
static void
rb_reset (DataRingBuffer *rb)
{
    rb->wr_pos = 0;
    rb->rd_pos = 0;
    rb->len = 0;
}

/**
 * rb_push:
 */
static void
rb_push_data (DataRingBuffer *rb, const int16_t value)
{
    static pthread_mutex_t mutex;

    if (rb->wr_pos >= rb->capacity) {
        /* we reached the end of our buffer slice, start from the beginning */
        rb->wr_pos = 0;
    }

    if ((rb->wr_pos == rb->rd_pos) && (rb->len == rb->capacity - 1)) {
        g_warning ("Acquiring data faster than we use it, skipped %zu entries.", rb->capacity);
        pthread_mutex_lock (&mutex);
        rb_reset (rb);
        pthread_mutex_unlock (&mutex);
    }

    rb->buffer[rb->wr_pos] = value;
    rb->len++;
    rb->wr_pos++;
}

/**
 * rb_pull_data:
 */
static gboolean
rb_pull_data (DataRingBuffer *rb, int16_t *data)
{
    if (rb->rd_pos >= rb->capacity) {
        /* we reached the end of our buffer slice, start from the beginning */
        rb->rd_pos = 0;
    }

    /* fail if we don't have enough data yet */
    if (rb->len == 0)
        return FALSE;
    if (rb->rd_pos >= rb->wr_pos)
        return FALSE;

    *data = rb->buffer[rb->rd_pos];
    rb->len--;
    rb->rd_pos++;

    return TRUE;
}

/**
 * spi_transfer:
 */
static void
spi_transfer (int fd, uint8_t const *txbuf, uint8_t const *rxbuf, size_t len)
{
    int status;
    uint16_t delay = 0;
    uint32_t speed = 0;
    uint8_t bits_per_word = 0;

    struct spi_ioc_transfer xfer = {
        .tx_buf = (unsigned long) txbuf,
        .rx_buf = (unsigned long) rxbuf,
        .len = len,
        .delay_usecs = delay,
        .speed_hz = speed,
        .bits_per_word = bits_per_word,
    };

    status = ioctl (fd, SPI_IOC_MESSAGE (1), &xfer);
    if (status < 0) {
        g_printerr ("Unable to send API message: %s\n", g_strerror (errno));
        return;
    }
}

/**
 * adc_read:
 */
static int16_t
adc_read (int fd)
{
    uint8_t *tx;
    uint8_t *rx;
    union {
        uint8_t b[2];
        int16_t i;
    } res;

    tx = malloc (sizeof(uint8_t) * 2);
    tx[0] = MAX1133_CONTROL_BYTE;
    tx[1] = 0;

    rx = malloc (sizeof(uint8_t) * 2);

    // get first part of our 16bit integer
    spi_transfer (fd, tx, rx, sizeof(uint8_t));

    res.b[0] = rx[0];
    res.b[1] = rx[1];

    free (tx);
    free (rx);

    return res.i;
}

/**
 * daq_thread_main:
 */
static void*
daq_thread_main (void *daq_ptr)
{
    Max1133Daq *daq = (Max1133Daq*) daq_ptr;

    while (daq->running) {
        rb_push_data (daq->rb, adc_read (daq->spi_fd));
        usleep (2);
    }

    return NULL;
}

/**
 * max1133daq_new:
 */
Max1133Daq*
max1133daq_new (const gchar *spi_device)
{
    Max1133Daq *daq;

    daq = g_slice_new0 (Max1133Daq);
    rb_init (daq->rb, 4096);

    daq->spi_fd = open (spi_device, O_RDWR);

	return daq;
}

/**
 * max1133daq_free:
 */
void
max1133daq_free (Max1133Daq *daq)
{
    g_return_if_fail (daq != NULL);

    rb_free (daq->rb);
    close (daq->spi_fd);

    g_slice_free (Max1133Daq, daq);
}

/**
 * max1133daq_start:
 *
 * Start threaded data acquisition.
 */
gboolean
max1133daq_start (Max1133Daq *daq)
{
    int rc;
    g_assert (!daq->running);

    daq->running = TRUE;
    rc = pthread_create (&daq->tid, NULL, &daq_thread_main, daq);
    if (rc) {
        daq->running = FALSE;
        g_printerr ("ERROR; return code from pthread_create() is %d\n", rc);
        return FALSE;
    }

    return TRUE;
}

/**
 * max1133daq_stop:
 */
gboolean
max1133daq_stop (Max1133Daq *daq)
{
    if (!daq->running)
        return FALSE;

    daq->running = FALSE;
    daq->tid = 0;
    rb_reset (daq->rb);

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
max1133daq_get_data (Max1133Daq *daq, uint8_t channel, int16_t *data)
{
    /* TODO: Implement multichannel recording */
    return rb_pull_data (daq->rb, data);
}
