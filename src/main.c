/*
 * Copyright (C) 2016-2017 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C) 2011 Kevin Allen
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

#include "config.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <float.h>
#include <galdur.h>

#include "defaults.h"
#include "tasks.h"
#include "stimpulse.h"

/* misc */
#define APP_NAME "labrstim"

/* global options valid for (almost) all stimulation methods */

static double opt_minimum_interval_ms = 100;
static double opt_maximum_interval_ms = 200;

static gchar *opt_dat_filename = NULL;
static int    opt_channels_in_dat_file = -1;

static int    opt_offline_channel = -1;

static GOptionEntry generic_option_entries[] =
{
    { "minimum_interval_ms", 'm', 0, G_OPTION_ARG_DOUBLE, &opt_minimum_interval_ms,
        "Set minimum interval for stimulation at random intervals (-R) or swr delay (-s -D)", "min_ms" },

    { "maximum_interval_ms", 'M', 0, G_OPTION_ARG_DOUBLE, &opt_maximum_interval_ms,
        "Set maximum interval for stimulation at random intervals (-R) or swr delay (-s -D)", "max_ms" },

    { "offline", 'o', 0, G_OPTION_ARG_FILENAME, &opt_dat_filename,
        "Use a .dat file as input data. You also need option -c, together with either -s or -t, to work with -o", "dat_file_name" },

    { "channels_in_dat_file", 'c', 0, G_OPTION_ARG_INT, &opt_channels_in_dat_file,
        "The number of channels in the dat file. Use only when working offline from a dat file (-o or --offline)", "number" },

    { "offline_channel", 'x', 0, G_OPTION_ARG_INT, &opt_offline_channel,
        "The channel on which swr detection is done when working offline from a dat file (-o and -s)", "number" },

    { NULL }
};

/**
 * labrstim_print_help_hint:
 */
static void
labrstim_print_help_hint (const gchar *subcommand, const gchar *unknown_option)
{
    if (unknown_option != NULL)
        g_printerr ("Option '%s' is unknown.\n", unknown_option);

    if (subcommand == NULL)
        g_printerr ("Run '%s --help' to see a full list of available command line options.\n", APP_NAME);
    else
        g_printerr ("Run '%s --help' to see a list of available commands and options, and '%s %s --help' to see a list of options specific for this subcommand.\n",
                    APP_NAME, APP_NAME, subcommand);
}

/**
 * labrstim_new_subcommand_option_context:
 *
 * Create a new option context for a Labrstim subcommand.
 */
static GOptionContext*
labrstim_new_subcommand_option_context (const gchar *command, const GOptionEntry *entries)
{
    GOptionContext *opt_context = NULL;
    g_autofree gchar *summary = NULL;

    opt_context = g_option_context_new ("- Labrstim CLI.");
    g_option_context_set_help_enabled (opt_context, TRUE);
    g_option_context_add_main_entries (opt_context, entries, NULL);

    /* set the summary text */
    summary = g_strdup_printf ("The '%s' command.", command);
    g_option_context_set_summary (opt_context, summary);

    return opt_context;
}

/**
 * labrstim_option_context_parse:
 *
 * Parse the options, print errors.
 */
static int
labrstim_option_context_parse (GOptionContext *opt_context, const gchar *subcommand, int *argc, char ***argv)
{
    g_autoptr(GError) error = NULL;

    g_option_context_parse (opt_context, argc, argv, &error);
    if (error != NULL) {
        g_autofree gchar *msg = NULL;
        msg = g_strconcat (error->message, "\n", NULL);
        g_printerr ("%s", msg);

        labrstim_print_help_hint (subcommand, NULL);
        return 1;
    }

    /* global parameter sanity check */
    if (opt_minimum_interval_ms >= opt_maximum_interval_ms) {
        g_printerr ("Minimum_interval_ms needs to be smaller than maximum_intervals_ms\nYou gave %lf and %lf\n",
                    opt_minimum_interval_ms, opt_maximum_interval_ms);
        return 3;
    }

    if (opt_minimum_interval_ms < 0) {
        g_printerr ("Minimum_interval_ms should be larger than 0\nYou gave %lf\n",
                    opt_minimum_interval_ms);
        return 3;
    }

    if (opt_dat_filename != NULL) {
        /* we need to know how many channels we have */
        if (opt_channels_in_dat_file < 0) {
            g_printerr ("Offline .dat file is give (-o), but number of channels in the file is missing (-c argument).\n");
            return 3;
        }

        if (opt_offline_channel < 0) {
            g_printerr ("You need to define a channel to use from the .dat file when running in offline mode (-x argument).\n");
            return 3;
        }

        if (opt_channels_in_dat_file <= 0) {
            g_printerr ("The number of channels in the .dat file must be > 0 (check the -c flag).\n");
            return 3;
        }

        if (opt_offline_channel < 0 || opt_offline_channel >= opt_channels_in_dat_file) {
            g_printerr ("The selected offline channel should be from 0 to %d but is %d.\n",
                        opt_channels_in_dat_file - 1, opt_offline_channel);
            return 3;
        }
    }

    return 0;
}

/**
 * labrstim_get_stim_parameters:
 *
 * Obtain additional session parameters.
 */
static gboolean
labrstim_get_stim_parameters (char **argv, int argc, int *sampling_rate_hz, double *trial_duration_sec, double *pulse_duration_ms, double *laser_intensity)
{
    double trial_dur_s;
    double pulse_dur_ms;
    double laser_int;
    int    sample_freq;

    /* strip the "--" positional arguments separator */
    for (guint i = 0; i < argc; i++) {
        if (g_strcmp0 (argv[i], "--") == 0) {
            guint j;

            /* shift elements to the left */
            for (j = i; j < argc - 1; j++)
                argv[j] = argv[j + 1];

            argv[j] = NULL;
            argc--;
            break;
        }
    }

    /* check if we have the required number of arguments */
    if (argc != 6) {
        gint i;
        g_printerr ("Usage for %s is \n", APP_NAME);
        g_printerr ("%s %s [sampling frequency (Hz)] [trial duration (sec)] [pulse duration (ms)] [laser intensity]\n", argv[0], argv[1]);
        g_printerr ("You need %d arguments but gave %d arguments:",
                 4, argc - 2);
        for (i = 2; i < argc; i++) {
            g_printerr (" %s", argv[i]);
        }
        g_printerr ("\n");
        return FALSE;
    }

    /* parse the arguments from the command line */
    sample_freq  = g_ascii_strtoll (argv[2], NULL, 10);
    trial_dur_s  = g_strtod (argv[3], NULL);
    pulse_dur_ms = g_strtod (argv[4], NULL);
    laser_int    = g_strtod (argv[5], NULL);

    if (sample_freq <= 1000 || sample_freq > 200000) {
        g_printerr ("Sampling frequency must be between 1000 and 200000.\nYou gave %i\n",
                    sample_freq);
        return FALSE;
    }
    if (trial_dur_s <= -2 || trial_dur_s > 86400) {
        g_printerr ("Trial duration should be between 0 and 86400 sec, or -1 for infinite.\nYou gave %lf\n",
                    trial_dur_s);
        return FALSE;
    }
    if (trial_dur_s < 0)
        trial_dur_s = DBL_MAX;
    if (pulse_dur_ms < 0 || pulse_dur_ms > 10000) {
        g_printerr ("Pulse_duration_ms should be between 0 and 10000 ms.\nYou gave %lf\n",
                    pulse_dur_ms);
        return FALSE;
    }
    if (laser_int <= 0 || laser_int > 65535) {
        g_printerr ("Laser power should be between 0 and 65535.\nYou gave %lf\n",
                    laser_int);
        return FALSE;
    }

    (*sampling_rate_hz)     = sample_freq;
    (*trial_duration_sec)   = trial_dur_s;
    (*pulse_duration_ms)    = pulse_dur_ms;
    (*laser_intensity)      = laser_int;

    return TRUE;
}

/**
 * labrstim_run_train:
 *
 * Do train a stimulation at a given frequency or at random intervals.
 */
static gint
labrstim_run_train (const gchar *command, char **argv, int argc)
{
    g_autoptr(GOptionContext) opt_context = NULL;
    gint ret;

    int sampling_rate_hz;
    double trial_duration_sec;
    double pulse_duration_ms;
    double laser_intensity_volt;
    gboolean success;

    static double   opt_train_frequency_hz = 6;
    static gboolean opt_random = FALSE;
    const GOptionEntry train_stim_options[] = {
        { "train", 'T', 0, G_OPTION_ARG_DOUBLE, &opt_train_frequency_hz,
          "Train of stimulations at a fix frequency", "frequency_hz" },

        { "random", 'R', 0, G_OPTION_ARG_NONE, &opt_random,
          "Train of stimulations with random intervals, use with -m and -M", NULL },
        { NULL }
    };

    /* NOTE: if both -T and -R parameters are specified, -T is ignored if -R is present */

    opt_context = labrstim_new_subcommand_option_context (command, train_stim_options);

    /* parse all arguments and retrieve settings */
    ret = labrstim_option_context_parse (opt_context, command, &argc, &argv);
    if (ret != 0)
        return ret;
    if (!labrstim_get_stim_parameters (argv, argc, &sampling_rate_hz, &trial_duration_sec, &pulse_duration_ms, &laser_intensity_volt))
        return 1;

    /* train stimulation from an offline file doesn't make sense and is not supported */
    if (opt_dat_filename != NULL) {
        g_printerr ("The 'train' stimulation mode can not run from an offline .dat file.\n");
        return 3;
    }

    if (opt_dat_filename == NULL)
        stimpulse_set_intensity (laser_intensity_volt);
    success = perform_train_stimulation (opt_random,
                                         sampling_rate_hz,
                                         trial_duration_sec,
                                         pulse_duration_ms,
                                         opt_minimum_interval_ms,
                                         opt_maximum_interval_ms,
                                         opt_train_frequency_hz);
    if (!success)
        return 5;

    return 0;
}

/**
 * labrstim_run_theta:
 *
 * Do the theta stimulation.
 */
static gint
labrstim_run_theta (const gchar *command, char **argv, int argc)
{
    g_autoptr(GOptionContext) opt_context = NULL;
    gint ret;
    gboolean success;

    int sampling_rate_hz;
    double trial_duration_sec;
    double pulse_duration_ms;
    double laser_intensity_volt;

    static double   opt_stimulation_theta_phase = 90;
    static gboolean opt_random = FALSE;
    const GOptionEntry theta_stim_options[] = {
        { "theta", 't', 0, G_OPTION_ARG_DOUBLE, &opt_stimulation_theta_phase,
          "Stimulation at the given theta phases", "theta_phase" },

        { "random", 'R', 0, G_OPTION_ARG_NONE, &opt_random,
          "Train of stimulations with random intervals, use with -m and -M", NULL },
        { NULL }
    };

    opt_context = labrstim_new_subcommand_option_context (command, theta_stim_options);

    /* parse all arguments and retrieve settings */
    ret = labrstim_option_context_parse (opt_context, command, &argc, &argv);
    if (ret != 0)
        return ret;
    if (!labrstim_get_stim_parameters (argv, argc, &sampling_rate_hz, &trial_duration_sec, &pulse_duration_ms, &laser_intensity_volt))
        return 1;

    /* verify if parameters make sense */
    if (opt_stimulation_theta_phase < 0 || opt_stimulation_theta_phase >= 360) {
        g_printerr ("Stimulation phase should be from 0 to 360\nYou gave %lf\n",
                    opt_stimulation_theta_phase);
        return 1;
    }

    if (opt_dat_filename == NULL)
        stimpulse_set_intensity (laser_intensity_volt);
    success = perform_theta_stimulation (opt_random,
                                         sampling_rate_hz,
                                         trial_duration_sec,
                                         pulse_duration_ms,
                                         opt_stimulation_theta_phase,
                                         opt_dat_filename,
                                         opt_channels_in_dat_file,
                                         opt_offline_channel);
    if (!success)
        return 5;

    return 0;
}

/**
 * labrstim_run_swr:
 *
 * Do SWR stimulation.
 */
static gint
labrstim_run_swr (const gchar *command, char **argv, int argc)
{
    g_autoptr(GOptionContext) opt_context = NULL;
    gint ret;
    gboolean success;

    int sampling_rate_hz;
    double trial_duration_sec;
    double pulse_duration_ms;
    double laser_intensity_volt;

    static double   opt_swr_refractory = 0;
    static double   opt_swr_power_threshold = 0;              /* as a z score */
    static double   opt_swr_convolution_peak_threshold = 0.5; /* as a z score */
    static gboolean opt_delay_swr = FALSE;
    static int      opt_swr_offline_reference = -1;

    const GOptionEntry swr_stim_options[] = {
        { "swr_refractory", 'f', 0, G_OPTION_ARG_DOUBLE, &opt_swr_refractory,
          "Add a refractory period in ms between end of last swr stimulation and beginning of next one", "ms" },

        { "swr", 's', 0, G_OPTION_ARG_DOUBLE, &opt_swr_power_threshold,
          "Stimulation during swr, give the power detection threashold (z score) as option argument", "power_threshold" },

        { "swr_convolution_peak_threshold", 'C', 0, G_OPTION_ARG_DOUBLE, &opt_swr_convolution_peak_threshold,
          "Give the detection threshold (z score) as option argument", "convolution_threshold" },

        { "delay_swr", 'D', 0, G_OPTION_ARG_NONE, &opt_delay_swr,
          "Add a delay between swr detection and beginning of stimulation. Use -m and -M to set minimum and maximum delay", NULL },

        { "swr_offline_reference", 'y', 0, G_OPTION_ARG_INT, &opt_swr_offline_reference,
          "The reference channel for swr detection when working offline from a dat file (-o and -s)", "number" },
        { NULL }
    };

    opt_context = labrstim_new_subcommand_option_context (command, swr_stim_options);

    /* parse all arguments and retrieve settings */
    ret = labrstim_option_context_parse (opt_context, command, &argc, &argv);
    if (ret != 0)
        return ret;
    if (!labrstim_get_stim_parameters (argv, argc, &sampling_rate_hz, &trial_duration_sec, &pulse_duration_ms, &laser_intensity_volt))
        return 3;

    /* check if parameters make sense */
    if (opt_swr_power_threshold < 0 || opt_swr_power_threshold > 20) {
        g_printerr ("SWR power threshold should be between 0 and 20\n. You gave %lf\n",
                    opt_swr_power_threshold);
        return 3;
    }

    if (opt_swr_refractory < 0) {
        g_printerr ("SWR refractory should be larger or equal to 0\n. You gave %lf\n",
                    opt_swr_refractory);
        return 3;
    }

    if (opt_dat_filename != NULL) {
        if (opt_swr_refractory > 0) {
            g_printerr ("Option -f should not be used when running from an offline file.\n");
            return 3;
        }

        if (opt_swr_offline_reference < 0) {
            g_printerr ("No reference channel is defined from the .dat file. Please select one (-y argument).\n");
            return 3;
        }

        if (opt_swr_offline_reference < 0 || opt_swr_offline_reference >= opt_channels_in_dat_file) {
            g_printerr ("Offline reference channel (-y) should be from 0 to %d but is %d.\n",
                        opt_channels_in_dat_file - 1, opt_swr_offline_reference);
            return 3;
        }
    }

    if (opt_swr_convolution_peak_threshold > 20 || opt_swr_convolution_peak_threshold < 0) {
        g_printerr ("The SWR convolution peak threshold should be between 0 and 20 but is %lf.\n",
                    opt_swr_convolution_peak_threshold);
        return 3;
    }

    if (opt_dat_filename == NULL)
        stimpulse_set_intensity (laser_intensity_volt);
    success = perform_swr_stimulation (sampling_rate_hz,
                                       trial_duration_sec,
                                       pulse_duration_ms,
                                       opt_swr_refractory,
                                       opt_swr_power_threshold,
                                       opt_swr_convolution_peak_threshold,
                                       opt_delay_swr,
                                       opt_minimum_interval_ms,
                                       opt_maximum_interval_ms,
                                       opt_dat_filename,
                                       opt_channels_in_dat_file,
                                       opt_offline_channel,
                                       opt_swr_offline_reference);
    if (!success)
        return 5;

    return 0;
}

/**
 * labrstim_init_random_seed:
 *
 * Intializes the random number generator.
 */
static void
labrstim_init_random_seed (void)
{
    time_t t;
    srand ((unsigned) time (&t));
}

/**
 * labrstim_make_realtime:
 *
 * Declare the program as a reat time program
 * (the user will need to be root to do this)
 */
static gboolean
labrstim_make_realtime (void)
{
    /* set scheduler policy */
    struct sched_param param;
    param.sched_priority = LS_PRIORITY;
    if (sched_setscheduler (0, SCHED_FIFO, &param) == -1) {
        g_printerr ("%s : sched_setscheduler failed\n", APP_NAME);
        g_printerr ("Do you have permission to run real-time applications? You might need to be root or use sudo\n");
        return FALSE;
    }

    /* ensure the main thread does never run on the DAQ core */
    if (gld_set_thread_no_cpu_affinity (0) < 0) {
        g_printerr ("Unable to set CPU core affinity.");
        return FALSE;
    }

    /* lock memory */
    if (mlockall (MCL_CURRENT | MCL_FUTURE) == -1) {
        g_printerr ("%s: mlockall failed", APP_NAME);
        return FALSE;
    }
    /* pre-fault our stack */
    unsigned char dummy[MAX_SAFE_STACK];
    memset (&dummy, 0, MAX_SAFE_STACK);

    return TRUE;
}

/**
 * labrstim_get_summary:
 **/
static gchar*
labrstim_get_summary ()
{
    GString *string;
    string = g_string_new ("");

    g_string_append_printf (string, "%s\n\n%s\n", "Laser Brain Stimulator",
                /* these are commands we can use with labrstim */
                "Subcommands:");

    g_string_append_printf (string, "  %s - %s\n", "theta", "Theta stimulation.");
    g_string_append_printf (string, "  %s - %s\n", "swr  ", "Sharp-wave-ripple detection and stimulation.");
    g_string_append_printf (string, "  %s - %s\n", "train", "Train stimulation.");

    g_string_append (string, "\n");
    g_string_append (string, "You can find information about subcommand-specific options by passing \"--help\" to the subcommand.");

    return g_string_free (string, FALSE);
}

/**
 * main:
 */
int
main (int argc, char *argv[])
{
    GError *error = NULL;
    GOptionContext *opt_context;
    const gchar *command = NULL;
    gchar *summary = NULL;
    gint ret = 0;

    static gboolean opt_show_version = FALSE;
    static gboolean opt_verbose_mode = FALSE;
    const GOptionEntry base_options[] = {
        { "version", 0, 0,
            G_OPTION_ARG_NONE,
            &opt_show_version,
            "Show the program version.",
            NULL },
        { "verbose", (gchar) 0, 0,
            G_OPTION_ARG_NONE,
            &opt_verbose_mode,
            "Show extra debugging information.",
            NULL },
        { NULL }
    };

    if (argc < 2) {
        g_printerr ("%s\n", "You need to specify a command.");
        g_printerr ("Run '%s --help' to see a full list of available command line options.\n", argv[0]);
        return 1;
    }
    command = argv[1];

    opt_context = g_option_context_new ("- Laser Brain Stimulator");

    /* set help text */
    summary = labrstim_get_summary ();
    g_option_context_set_summary (opt_context, summary) ;
    g_free (summary);

    /* only attempt to show global help if we don't have a subcommand as first parameter (subcommands are never prefixed with "-") */
    if (g_str_has_prefix (command, "-"))
        g_option_context_set_help_enabled (opt_context, TRUE);
    else
        g_option_context_set_help_enabled (opt_context, FALSE);
    g_option_context_set_ignore_unknown_options (opt_context, TRUE);

    /* parse our options */
    g_option_context_add_main_entries (opt_context, base_options, NULL);
    g_option_context_add_main_entries (opt_context, generic_option_entries, NULL);

    if (!g_option_context_parse (opt_context, &argc, &argv, &error)) {
        g_printerr ("option parsing failed: %s\n", error->message);
        return 1;
    }

    if (opt_show_version) {
        g_print ("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
        g_print ("%s\n", PACKAGE_COPYRIGHT);
        g_print
        ("License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n"
         "This is free software: you are free to change and redistribute it.\n"
         "There is NO WARRANTY, to the extent permitted by law.\n");
        return 0;
    }

    if (opt_dat_filename == NULL) {
        /* give the program realtime priority if we are not running from an offline file */
        if (!labrstim_make_realtime ())
            return 5;

        /* initialize DAQ board */
        if (!gld_board_initialize ())
            return 5;
    }



    /* set a random seed based on the time we launched */
    labrstim_init_random_seed ();

    /* check which task we should execute */
    if (g_strcmp0 (command, "theta") == 0) {
        ret = labrstim_run_theta (command, argv, argc);
    } else if (g_strcmp0 (command, "swr") == 0) {
        ret = labrstim_run_swr (command, argv, argc);
    } else if (g_strcmp0 (command, "train") == 0) {
        ret = labrstim_run_train (command, argv, argc);
    } else {
        g_printerr ("You need to specify a valid command, '%s' is unknown.\n", command);
        ret = 1;
    }

    /* clear Galdur board state */
    if (opt_dat_filename == NULL)
        gld_board_shutdown ();

    return ret;
}
