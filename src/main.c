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
#include <galdur.h>

#include "defaults.h"
#include "tasks.h"
#include "stimpulse.h"

/* misc */
static const gchar *app_name = "labrstim";

/* options */
static double opt_stimulation_theta_phase = 90;
static double opt_swr_power_threshold;                  // as a z score
static double opt_swr_convolution_peak_threshold = 0.5; // as a z score
static double opt_train_frequency_hz = 6;

static gboolean  opt_random = FALSE;
static double    opt_minimum_interval_ms = 100;
static double    opt_maximum_interval_ms = 200;

static gchar *opt_dat_file_name = NULL;
static int    opt_channels_in_dat_file = 32;

static int opt_offline_channel = 0;
static int opt_swr_offline_reference = 1;

static gboolean opt_delay_swr = FALSE;

static double opt_swr_refractory = 0;

static GOptionEntry main_option_entries[] =
{
    { "theta", 't', 0, G_OPTION_ARG_DOUBLE, &opt_stimulation_theta_phase,
        "Stimulation at the given theta phases", "theta_phase" },
    { "swr", 's', 0, G_OPTION_ARG_DOUBLE, &opt_swr_power_threshold,
        "Stimulation during swr, give the power detection threashold (z score) as option argument", "power_threashold" },
    { "swr_convolution_peak_threshold", 'C', 0, G_OPTION_ARG_DOUBLE, &opt_swr_convolution_peak_threshold,
        "Give the detection threshold (z score) as option argument", "convolution_threshold" },
    { "train", 'T', 0, G_OPTION_ARG_DOUBLE, &opt_train_frequency_hz,
        "Train of stimulations at a fix frequency", "frequency_hz" },

    { "random", 'R', 0, G_OPTION_ARG_NONE, &opt_random,
        "Train of stimulations with random intervals, use with -m and -M", NULL },
    { "minimum_interval_ms", 'm', 0, G_OPTION_ARG_DOUBLE, &opt_minimum_interval_ms,
        "Set minimum interval for stimulation at random intervals (-R) or swr delay (-s -D)", "min_ms" },
    { "maximum_interval_ms", 'M', 0, G_OPTION_ARG_DOUBLE, &opt_maximum_interval_ms,
        "Set maximum interval for stimulation at random intervals (-R) or swr delay (-s -D)", "max_ms" },

    { "offline", 'o', 0, G_OPTION_ARG_FILENAME, &opt_dat_file_name,
        "Use a .dat file as input data. You also need option -c, together with either -s or -t, to work with -o", "dat_file_name" },
    { "channels_in_dat_file", 'c', 0, G_OPTION_ARG_INT, &opt_channels_in_dat_file,
        "The number of channels in the dat file. Use only when working offline from a dat file (-o or --offline)", "number" },

    { "offline_channel", 'x', 0, G_OPTION_ARG_INT, &opt_offline_channel,
        "The channel on which swr detection is done when working offline from a dat file (-o and -s)", "number" },
    { "swr_offline_reference", 'y', 0, G_OPTION_ARG_INT, &opt_swr_offline_reference,
        "The reference channel for swr detection when working offline from a dat file (-o and -s)", "number" },

    { "delay_swr", 'D', 0, G_OPTION_ARG_NONE, &opt_delay_swr,
        "Add a delay between swr detection and beginning of stimulation. Use -m and -M to set minimum and maximum delay", NULL },

    { "swr_refractory", 'f', 0, G_OPTION_ARG_DOUBLE, &opt_swr_refractory,
        "Add a refractory period in ms between end of last swr stimulation and beginning of next one", "ms" },
    { "ttl_amplitude_volt", 'a', 0, G_OPTION_ARG_DOUBLE, &opt_swr_refractory,
        "Set the amplitude of the ttl pulse in volt", "V" },

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
        g_printerr ("Run '%s --help' to see a full list of available command line options.\n", app_name);
    else
        g_printerr ("Run '%s --help' to see a list of available commands and options, and '%s %s --help' to see a list of options specific for this subcommand.\n",
                    app_name, app_name, subcommand);
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
        gchar *msg;
        msg = g_strconcat (error->message, "\n", NULL);
        g_print ("%s", msg);
        g_free (msg);

        labrstim_print_help_hint (subcommand, NULL);
        return 1;
    }

    return 0;
}

/**
 * labrstim_get_stim_parameters:
 *
 * Obtain additional session parameters.
 */
static gboolean
labrstim_get_stim_parameters (char **argv, int argc, double *trial_duration_sec, double *pulse_duration_ms, double *laser_intensity_volt)
{
    /* check if we have the required number of arguments */
    if (argc != 5) {
        gint i;
        fprintf (stderr, "Usage for %s is \n", app_name);
        fprintf (stderr, "%s trial_duration_sec pulse_duration_ms laser_intensity_volts\n", argv[0]);
        fprintf (stderr, "You need %d arguments but gave %d arguments: \n",
                 3, argc - 2);
        for (i = 1; i < argc; i++) {
            fprintf (stderr, "%s\n", argv[i]);
        }
        return FALSE;
    }

    /* parse the arguments from the command line */
    (*trial_duration_sec)   = atof (argv[2]);
    (*pulse_duration_ms)    = atof (argv[3]);
    (*laser_intensity_volt) = atof (argv[4]);

    return TRUE;
}

/**
 * labrstim_run_train:
 *
 * Do train a stimulation at a given frequency or at random intervals.
 */
static gint
labrstim_run_train (char **argv, int argc)
{
    g_autoptr(GOptionContext) opt_context = NULL;
    gint ret;
    const gchar *command = "train";

    double trial_duration_sec;
    double pulse_duration_ms;
    double laser_intensity_volt;

    opt_context = labrstim_new_subcommand_option_context (command, main_option_entries);

    /* parse all arguments and retrieve settings */
    ret = labrstim_option_context_parse (opt_context, command, &argc, &argv);
    if (ret != 0)
        return ret;
    if (!labrstim_get_stim_parameters (argv, argc, &trial_duration_sec, &pulse_duration_ms, &laser_intensity_volt))
        return 1;

    stimpulse_set_intensity (laser_intensity_volt);
    perform_train_stimulation (opt_random,
                                   trial_duration_sec,
                                   pulse_duration_ms,
                                   opt_minimum_interval_ms,
                                   opt_maximum_interval_ms,
                                   opt_train_frequency_hz);
    return 0;
}

/**
 * labrstim_run_theta:
 *
 * Do the theta stimulation.
 */
static gint
labrstim_run_theta (char **argv, int argc)
{
    g_autoptr(GOptionContext) opt_context = NULL;
    gint ret;
    const gchar *command = "theta";

    double trial_duration_sec;
    double pulse_duration_ms;
    double laser_intensity_volt;

    opt_context = labrstim_new_subcommand_option_context (command, main_option_entries);

    /* parse all arguments and retrieve settings */
    ret = labrstim_option_context_parse (opt_context, command, &argc, &argv);
    if (ret != 0)
        return ret;
    if (!labrstim_get_stim_parameters (argv, argc, &trial_duration_sec, &pulse_duration_ms, &laser_intensity_volt))
        return 1;

    stimpulse_set_intensity (laser_intensity_volt);
    perform_theta_stimulation (opt_random,
                                   trial_duration_sec,
                                   pulse_duration_ms,
                                   opt_stimulation_theta_phase,
                                   opt_dat_file_name,
                                   opt_channels_in_dat_file,
                                   opt_offline_channel);
    return 0;
}

/**
 * labrstim_run_swr:
 *
 * Do SWR stimulation.
 */
static gint
labrstim_run_swr (char **argv, int argc)
{
    g_autoptr(GOptionContext) opt_context = NULL;
    gint ret;
    const gchar *command = "swr";

    double trial_duration_sec;
    double pulse_duration_ms;
    double laser_intensity_volt;

    opt_context = labrstim_new_subcommand_option_context (command, main_option_entries);

    /* parse all arguments and retrieve settings */
    ret = labrstim_option_context_parse (opt_context, command, &argc, &argv);
    if (ret != 0)
        return ret;
    if (!labrstim_get_stim_parameters (argv, argc, &trial_duration_sec, &pulse_duration_ms, &laser_intensity_volt))
        return 1;

    stimpulse_set_intensity (laser_intensity_volt);
    perform_swr_stimulation (trial_duration_sec,
                                 pulse_duration_ms,
                                 opt_swr_refractory,
                                 opt_swr_power_threshold,
                                 opt_swr_convolution_peak_threshold,
                                 opt_delay_swr,
                                 opt_minimum_interval_ms,
                                 opt_maximum_interval_ms,
                                 opt_dat_file_name,
                                 opt_channels_in_dat_file,
                                 opt_offline_channel,
                                 opt_swr_offline_reference);
    return 0;
}

/**
 * init_random_seed:
 *
 * Intializes the random number generator.
 */
static void
init_random_seed ()
{
    time_t t;
    srand ((unsigned) time (&t));
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
    if (!g_option_context_parse (opt_context, &argc, &argv, &error)) {
        g_printerr ("option parsing failed: %s\n", error->message);
        return 1;
    }

    if (opt_show_version) {
        printf ("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
        printf ("%s\n", PACKAGE_COPYRIGHT);
        printf
        ("License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n"
         "This is free software: you are free to change and redistribute it.\n"
         "There is NO WARRANTY, to the extent permitted by law.\n");
        return 0;
    }

    /*
     * declare the program as a reat time program
     * the user will need to be root to do this
     */
    if (opt_dat_file_name == NULL) {        /* don't do it if we work offline */
        struct sched_param param;
        param.sched_priority = MY_PRIORITY;
        if (sched_setscheduler (0, SCHED_FIFO, &param) == -1) {
            fprintf (stderr, "%s : sched_setscheduler failed\n", app_name);
            fprintf (stderr,
                     "Do you have permission to run real-time applications? You might need to be root or use sudo\n");
            return 1;
        }
        /* lock memory */
        if (mlockall (MCL_CURRENT | MCL_FUTURE) == -1) {
            fprintf (stderr, "%s: mlockall failed", app_name);
            return 1;
        }
        /* pre-fault our stack */
        unsigned char dummy[MAX_SAFE_STACK];
        memset (&dummy, 0, MAX_SAFE_STACK);
    }

    /* initialize DAQ board */
    if (!gld_board_initialize ())
        return 2;

    /* set a random seed based on the time we launched */
    init_random_seed ();

    /* init GPIO */
    stimpulse_gpio_init ();

    /* check which task we should execute */
    if (g_strcmp0 (command, "theta") == 0) {
        ret = labrstim_run_theta (argv, argc);
    } else if (g_strcmp0 (command, "swr") == 0) {
        ret = labrstim_run_swr (argv, argc);
    } else if (g_strcmp0 (command, "train") == 0) {
        ret = labrstim_run_train (argv, argc);
    } else {
        g_printerr ("You need to specify a valid command, '%s' is unknown.\n", command);
        ret = 1;
    }

    /* clear Galdur board state */
    gld_board_shutdown ();

    return ret;
}


#if 0
    /* validate parameters */
    // FIXME: both -t and -T are not permitted


    // FIXME: both -R and -t are not permitted

    // FIXME: -t and -s are not permitted together


    // FIXME: -R and -T are not permitted

    // FIXME: -R and -s are not permitted

    // FIXME: -R and -m are not permitted

    // FIXME: -s and -T are not permitted

     if (trial_duration_sec <= 0 || trial_duration_sec > 10000) {
        fprintf (stderr,
                 "%s: trial duration should be between 0 and 10000 sec\nYou gave %lf\n",
                 app_name, trial_duration_sec);
        return FALSE;
    }
    if (pulse_duration_ms < 0 || pulse_duration_ms > 10000) {
        fprintf (stderr,
                 "%s: pulse_duration_ms should be between 0 and 10000 ms\nYou gave %lf\n",
                 app_name, pulse_duration_ms);
        return FALSE;
    }
    if (laser_intensity_volt <= 0 || laser_intensity_volt > 4) {
        fprintf (stderr,
                 "%s: voltage to control laser power should be between 0 and 4 volt\nYou gave %lf\n",
                 app_name, laser_intensity_volt);
        return FALSE;
    }
    if (opt_stimulation_theta_phase < 0 || opt_stimulation_theta_phase >= 360) {
        fprintf (stderr,
                 "%s: stimulation phase should be from 0 to 360\nYou gave %lf\n",
                 app_name, opt_stimulation_theta_phase);
        return FALSE;
    }


    if (opt_minimum_interval_ms >= opt_maximum_interval_ms) {
        fprintf (stderr,
                 "%s: minimum_interval_ms needs to be smaller than maximum_intervals_ms\nYou gave %lf and %lf\n",
                 app_name, opt_minimum_interval_ms, opt_maximum_interval_ms);
        return FALSE;
    }
    if (opt_minimum_interval_ms < 0) {
        fprintf (stderr,
                 "%s: minimum_interval_ms should be larger than 0\nYou gave %lf\n",
                 app_name, opt_minimum_interval_ms);
        return FALSE;
    }

    if (opt_swr_power_threshold < 0 || opt_swr_power_threshold > 20) {
        fprintf (stderr,
                 "%s: swr_power_threashold should be between 0 and 20\n. You gave %lf\n",
                 app_name, opt_swr_power_threshold);
        return FALSE;
    }

    /*
    if (with_s_opt) {
        if (swr_refractory < 0) {
            fprintf (stderr,
                     "%s: swr_refractory should be larger or equal to 0\n. You gave %lf\n",
                     app_name, swr_refractory);
            return FALSE;
        }
    }

    if (with_o_opt == 1) {
        // we then need -c
        if (with_c_opt == 0) {
            fprintf (stderr, "%s: option -o requires the use of -c.\n",
                     app_name);
            return 1;
        }
        // we need either -s or -t
        if (with_s_opt == 0 && with_t_opt == 0) {
            fprintf (stderr,
                     "%s: option -o requires the use of either -s or -t.\n",
                     app_name);
            return 1;
        }
        if (with_s_opt == 1) {
            if (with_x_opt == 0 || with_y_opt == 0) {
                fprintf (stderr,
                         "%s: option -o used with -s requires the use of both -x and -y.\n",
                         app_name);
                return 1;
            }
        }
        if (with_t_opt == 1) {
            if (with_x_opt == 0) {
                fprintf (stderr,
                         "%s: option -o used with -t requires the use of -x.\n",
                         app_name);
                return 1;
            }
        }
        if (with_f_opt == 1) {
            fprintf (stderr, "%s: option -f should not be used with -o.\n",
                     app_name);
            return 1;
        }
    }
    if (with_c_opt) {
        if (opt_channels_in_dat_file < 0) {
            fprintf (stderr,
                     "%s: opt_channels_in_dat_file should be larger than 0 but is %d.\n",
                     app_name, opt_channels_in_dat_file);
            return 1;
        }
    }
    if (with_x_opt) {
        if (offline_channel < 0 || offline_channel >= opt_channels_in_dat_file) {
            fprintf (stderr,
                     "%s: offline_channel should be from 0 to %d but is %d.\n",
                     app_name, opt_channels_in_dat_file - 1, offline_channel);
            return 1;
        }
    }
    if (with_y_opt) {
        if (swr_offline_reference < 0
            || swr_offline_reference >= opt_channels_in_dat_file) {
            fprintf (stderr,
                     "%s: swr_offline_reference should be from 0 to %d but is %d.\n",
                     app_name, opt_channels_in_dat_file - 1,
                     swr_offline_reference);
            return 1;
        }
    }
    if (with_C_opt) {
        if (swr_convolution_peak_threshold > 20
            || swr_convolution_peak_threshold < 0) {
            fprintf (stderr,
                     "%s: swr_convolution_peak_threshold should be between 0 and 20 but is %lf.\n",
                     app_name, swr_convolution_peak_threshold);
            return 1;
        }
    }
    if (with_d_opt) {
        if (device_index_for_stimulation < 0) {
            fprintf (stderr,
                     "%s: device_index_for_stimulation should not be negative but is %d.\n",
                     app_name, device_index_for_stimulation);
            return 1;
        }
    }
    */
#endif
