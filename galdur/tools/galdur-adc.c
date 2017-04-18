/*
 * Copyright (C) 2016-2017 Matthias Klumpp <matthias@tenstral.net>
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

#include <stdio.h>
#include <time.h>

#include "galdur.h"

static gint64 opt_sample_count      = 1000 * 124;
static gint   opt_channel_count     = 16;

static gint64 opt_sample_frequency  = 6000; /* 6 kHz */

static gchar  *opt_base_filename = NULL;

void
run_galdur_adc_daq ()
{
    struct timespec start, stop;
    struct timespec diff;
    GldAdc *daq;
    guint i;

    g_print ("Reading %"G_GINT64_FORMAT" samples from %i channels at %"G_GINT64_FORMAT"Hz.\n",
            opt_sample_count, opt_channel_count, opt_sample_frequency);

    daq = gld_adc_new (opt_channel_count, opt_sample_count, -1);

    gld_adc_set_acq_frequency (daq, opt_sample_frequency);

    clock_gettime(CLOCK_REALTIME, &start);

    gld_adc_acquire_samples (daq, opt_sample_count);

    while (gld_adc_is_running (daq)) {}

    clock_gettime(CLOCK_REALTIME, &stop);

    diff.tv_sec = stop.tv_sec - start.tv_sec;
    diff.tv_nsec = stop.tv_nsec - start.tv_nsec;


    g_debug ("Writing results file");

    for (i = 0; i < opt_channel_count; i++) {
        int16_t data;
        g_autofree gchar *fname = NULL;

        fname = g_strdup_printf ("%s%i.csv", opt_base_filename, i);
        FILE *f = fopen (fname, "w");
        if (f == NULL) {
            g_error ("Error opening %s.", fname);
            break;
        }

        while (gld_adc_get_sample (daq, i, &data))
            fprintf (f, "%i\n", data);
        fclose (f);
    }

    g_print ("Required time: %lld(sec) + %lld(nsec)\n", (long long) diff.tv_sec, (long long) diff.tv_nsec);
    g_print ("Sampled data written to /tmp\n");

    gld_adc_free (daq);
}

static GOptionEntry option_entries[] =
{
    { "samples", 'n', 0, G_OPTION_ARG_INT64, &opt_sample_count,
        "Number of samples to be taken", "Sample count" },
    { "channels", 'c', 0, G_OPTION_ARG_INT, &opt_channel_count,
        "Number of channels to record from", "Channel count" },
    { "frequency", 'f', 0, G_OPTION_ARG_INT64, &opt_sample_frequency,
        "Sampling frequency", "Sampling frequency" },
    { "basefile", 'o', 0, G_OPTION_ARG_FILENAME, &opt_base_filename,
        "Base filename for resulting data", "Base filename" },

    { NULL }
};

int main(int argc, char **argv)
{
    GError *error = NULL;
    GOptionContext *octx;

    /* parse our options */
    octx = g_option_context_new ("- Galdur ADC Test Tool");
    g_option_context_add_main_entries (octx, option_entries, NULL);
    g_option_context_set_help_enabled (octx, TRUE);
    if (!g_option_context_parse (octx, &argc, &argv, &error)) {
        g_printerr ("Option parsing failed: %s\n", error->message);
        return 1;
    }

    if (opt_base_filename == NULL) {
        g_print ("No base filename given, storing data in '/tmp/galdur-adc-<chan>.csv'.\n");
        g_print ("\n");
        opt_base_filename = g_strdup ("/tmp/galdur-adc-");
    }

    if (opt_channel_count < 0) {
        g_error ("A negative channel count is not allowed.");
        return 2;
    }
    if (opt_sample_count < 0) {
        g_error ("A negative amount of samples is not only fairly useless but also impossible.");
        return 2;
    }
    if (opt_sample_frequency < 0) {
        g_error ("A negative sampling frequency is an interesting idea, but not something that makes much sense and that we can do.");
        return 2;
    }

    if (!gld_board_initialize ())
        return 1;

    gld_board_set_spi_clock_divider (48); /* fast SPI connection */

    run_galdur_adc_daq ();

    gld_board_shutdown ();
    return 0;
}
