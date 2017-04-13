
#include <stdio.h>
#include <time.h>

#include "galdur.h"

static gulong SAMPLE_COUNT  = 1000 * 124;
static guint  CHANNEL_COUNT = 16;

static gulong SAMPLE_FREQUENCY  = 6000; /* 6 kHz */

void
test_daq_speed ()
{
    struct timespec start, stop;
    struct timespec diff;
    GldAdc *daq;
    guint i;

    g_print ("Reading %lu samples from %u channels at %luHz.\n",
            SAMPLE_COUNT, CHANNEL_COUNT, SAMPLE_FREQUENCY);

    daq = gld_adc_new (CHANNEL_COUNT, SAMPLE_COUNT, -1);

    gld_adc_set_acq_frequency (daq, SAMPLE_FREQUENCY);

    clock_gettime(CLOCK_REALTIME, &start);

    gld_adc_acquire_data (daq, SAMPLE_COUNT);

    while (gld_adc_is_running (daq)) {}

    clock_gettime(CLOCK_REALTIME, &stop);

    diff.tv_sec = stop.tv_sec - start.tv_sec;
    diff.tv_nsec = stop.tv_nsec - start.tv_nsec;


    g_debug ("Writing results file");

    for (i = 0; i < CHANNEL_COUNT; i++) {
        int16_t data;
        g_autofree gchar *fname = NULL;

        fname = g_strdup_printf ("/tmp/test_sampled-data-chan%i.csv", i);
        FILE *f = fopen (fname, "w");
        if (f == NULL) {
            g_error ("Error opening %s.", fname);
            break;
        }

        while (gld_adc_get_data (daq, i, &data))
            fprintf (f, "%i\n", data);
        fclose (f);
    }

    g_print ("Required time: %lld(sec) + %lld(nsec)\n", (long long) diff.tv_sec, (long long) diff.tv_nsec);
    g_print ("Sampled data written to /tmp\n");

    gld_adc_free (daq);
}

void
test_daq_ringbuf ()
{
    GldAdc *daq;
    guint i;
    gboolean data_available = TRUE;
    g_autofree gchar *fname = NULL;

    fname = g_strdup ("/tmp/test_sampled-data-rbtest.csv");

    g_print ("Reading %lu samples from %u channels at %luHz (ringbuffer test).\n",
            SAMPLE_COUNT, CHANNEL_COUNT, SAMPLE_FREQUENCY);

    daq = gld_adc_new (CHANNEL_COUNT, SAMPLE_COUNT / 4, -1);

    gld_adc_set_acq_frequency (daq, SAMPLE_FREQUENCY);

    gld_adc_acquire_data (daq, SAMPLE_COUNT);

    FILE *f = fopen (fname, "w");
    if (f == NULL) {
        g_error ("Error opening %s.", fname);
        return;
    }

    while (gld_adc_is_running (daq) || data_available) {
        data_available = FALSE;
        for (i = 0; i < CHANNEL_COUNT; i++) {
            int16_t data;

            if (gld_adc_get_data (daq, i, &data)) {
                data_available = TRUE;
                fprintf(f, "%i;", data);
            }
        }

        fprintf(f, "\n");
    }

    fclose (f);
    g_print ("Sampled data written to /tmp\n");

    gld_adc_free (daq);
}

int main(int argc, char **argv)
{
    if (!gld_board_initialize ())
        return 1;

    test_daq_speed ();

    test_daq_ringbuf ();

    gld_board_shutdown ();
    return 0;
}
