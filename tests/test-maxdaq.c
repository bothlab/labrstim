
#include <stdio.h>
#include <time.h>

#include "../src/max1133daq.h"

static gulong SAMPLE_COUNT  = 1000 * 124;
static guint  CHANNEL_COUNT = 16;

static gulong SAMPLE_FREQUENCY  = 6000; /* 6 kHz */

void
test_daq_speed ()
{
    struct timespec start, stop;
    struct timespec diff;
    Max1133Daq *daq;
    guint i;

    g_print ("Reading %lu samples from %u channels at %luHz.\n",
            SAMPLE_COUNT, CHANNEL_COUNT, SAMPLE_FREQUENCY);

    daq = max1133daq_new (CHANNEL_COUNT, SAMPLE_COUNT);

    max1133daq_set_acq_frequency (daq, SAMPLE_FREQUENCY);

    clock_gettime(CLOCK_REALTIME, &start);

    max1133daq_acquire_data (daq, SAMPLE_COUNT);

    while (max1133daq_is_running (daq)) {}

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

        while (max1133daq_get_data (daq, i, &data))
            fprintf (f, "%i\n", data);
        fclose (f);
    }

    g_print ("Required time: %zu(sec) + %zu(nsec)\n", diff.tv_sec, diff.tv_nsec);
    g_print ("Sampled data written to /tmp\n");

    max1133daq_free (daq);
}

void
test_daq_ringbuf ()
{
    Max1133Daq *daq;
    guint i;
    gboolean data_available;
    g_autofree gchar *fname = NULL;

    fname = g_strdup ("/tmp/test_sampled-data-rbtest.csv");

    g_print ("Reading %lu samples from %u channels at %luHz (ringbuffer test).\n",
            SAMPLE_COUNT, CHANNEL_COUNT, SAMPLE_FREQUENCY);

    daq = max1133daq_new (CHANNEL_COUNT, SAMPLE_COUNT / 4);

    max1133daq_set_acq_frequency (daq, SAMPLE_FREQUENCY);

    max1133daq_acquire_data (daq, SAMPLE_COUNT);

    FILE *f = fopen (fname, "w");
    if (f == NULL) {
        g_error ("Error opening %s.", fname);
        return;
    }

    while (max1133daq_is_running (daq) || data_available) {
        data_available = FALSE;
        for (i = 0; i < CHANNEL_COUNT; i++) {
            int16_t data;

            if (max1133daq_get_data (daq, i, &data)) {
                data_available = TRUE;
                fprintf(f, "%i;", data);
            }
        }

        fprintf(f, "\n");
    }

    fclose (f);
    g_print ("Sampled data written to /tmp\n");

    max1133daq_free (daq);
}

int main(int argc, char **argv)
{
    test_daq_speed ();

    test_daq_ringbuf ();

    return 0;
}
