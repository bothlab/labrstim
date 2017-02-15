
#include <stdio.h>
#include <time.h>

#include "../src/max1133daq.h"

static gulong SAMPLE_COUNT  = 100000;
static guint  CHANNEL_COUNT = 16;

static gulong SAMPLE_FREQUENCY  = 3000; /* 3 kHz */

int main(int argc, char **argv)
{
    struct timespec start, stop;
    struct timespec diff;
    Max1133Daq *daq;
    guint i;

    daq = max1133daq_new (CHANNEL_COUNT, 65536);

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
            fprintf(f, "%i\n", data);
        fclose (f);
    }

    printf ("Read %lu samples from %u channels at %luHz.\n Required time: %zu(sec) + %zu(nsec)\n",
            SAMPLE_COUNT, CHANNEL_COUNT, SAMPLE_FREQUENCY,
            diff.tv_sec, diff.tv_nsec);

    max1133daq_free (daq);

    return 0;
}
