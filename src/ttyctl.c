/*
 * Copyright (C) 2017 Matthias Klumpp <matthias@tenstral.net>
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

#include <config.h>

#include <glib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/wait.h>
#include <gio/gunixinputstream.h>

#include "defaults.h"

static GPid labrstim_proc_pid = 0;
static GInputStream *labrstim_stderr = NULL;

/* fd of the tty we are connected to to communicate with the master */
static int tty_fd = -1;

/**
 * tty_set_interface_attribs:
 *
 * Configure tty fd.
 */
static int
tty_set_interface_attribs (int fd, int speed)
{
    struct termios tty;

    if (tcgetattr (fd, &tty) < 0) {
        g_printerr ("tcgetattr failed: %s\n", g_strerror (errno));
        return 2;
    }

    cfsetospeed (&tty, (speed_t) speed);
    cfsetispeed (&tty, (speed_t) speed);

    tty.c_cflag |= (CLOCAL | CREAD); /* ignore modem controls */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         /* 8-bit characters */
    tty.c_cflag &= ~PARENB;     /* no parity bit */
    tty.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
    tty.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */

    /* setup for non-canonical mode */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    /* fetch bytes as they become available */
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr (fd, TCSANOW, &tty) != 0) {
        g_printerr ("Error from tcsetattr: %s\n", g_strerror (errno));
        return 2;
    }

    return 0;
}

/**
 * tty_write:
 *
 * Write string output to tty.
 */
static gboolean
tty_write (int fd, const gchar *str)
{
    size_t wlen;
    size_t nbytes;

    nbytes = strlen (str);
    wlen = write (fd, str, nbytes);
    if (wlen != nbytes) {
        g_printerr ("Write failed: %zu, %d\n", wlen, errno);
        return FALSE;
    }

    /* delay for output */
    tcdrain(fd);

    return TRUE;
}

/**
 * tty_writecmd:
 *
 * Write line to tty.
 */
static gboolean
tty_writecmd (int fd, const gchar *cmd, const gchar *val)
{
    g_autofree gchar *strnl;

    if (val == NULL)
        strnl = g_strdup_printf ("%s\n", cmd);
    else
        strnl = g_strdup_printf ("%s %s\n", cmd, val);
    return tty_write (fd, strnl);
}

/**
 * labrstim_command_exited:
 *
 * Triggered if the labrstim command is done.
 */
void
labrstim_command_exited (GPid pid, gint status, gpointer user_data)
{
    g_autofree gchar *tmp = NULL;
    tmp = g_strdup_printf ("%i", status);

    if ((status == 0) || (status == 15)) {
        /* we exited normally or received SIGTERM */
        tty_writecmd (tty_fd, "FINISHED", tmp);
    } else {
        g_autofree gchar *buf = NULL;
        g_autofree gchar *tmp = NULL;
        g_autofree gchar *msg = NULL;
        static const gsize buf_size = 1536;
        gsize bytes_read;
        g_autoptr(GError) error = NULL;

        buf = g_malloc (buf_size); /* this buffer should be big enozgh for the largers errors */
        g_input_stream_read_all (labrstim_stderr,
                                 buf,
                                 buf_size,
                                 &bytes_read,
                                 NULL,
                                 &error);
        if (error == NULL) {
            buf[bytes_read - 1] = '\0';
            tmp = g_strescape (buf, NULL);
        } else {
            tmp = g_strdup_printf ("Unable to read from stderr: %s", error->message);
        }

        msg = g_strdup_printf ("Labrstim exited with an error: %s", tmp);
        tty_writecmd (tty_fd, "ERROR",  msg);
    }

    labrstim_proc_pid = 0;

    /* noop on Linux */
    g_spawn_close_pid (pid);
}

/**
 * process_command:
 *
 * Process a command.
 */
static void
run_labrstim_command (const gchar *lstim_cmd)
{
    gboolean ret;
    g_autoptr(GError) error = NULL;
    g_auto(GStrv) args = NULL;
    g_autofree gchar *cmd_raw = NULL;
    int ls_stderr;

    if (labrstim_proc_pid != 0) {
        tty_writecmd (tty_fd, "ERROR", "Already running.");
        return;
    }

    /* clean up */
    if (labrstim_stderr != NULL)
        g_object_unref (labrstim_stderr);

    /* create the final command */
    cmd_raw = g_strdup_printf ("labrstim %s", lstim_cmd);
    args = g_strsplit (cmd_raw, " ", -1);

    ret = g_spawn_async_with_pipes ("/tmp", /* working directory */
                          args,
                          NULL, /* envp */
                          G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                          NULL, /* child setup */
                          NULL,
                          &labrstim_proc_pid,
                          NULL, /* stdin */
                          NULL, /* we ignore stdout for now */
                          &ls_stderr,
                          &error);
    if (!ret) {
        g_autofree gchar *tmp = NULL;

        tmp = g_strescape (error->message, NULL);
        tty_writecmd (tty_fd, "ERROR", tmp);
    } else {
        g_child_watch_add (labrstim_proc_pid,
                       labrstim_command_exited,
                       NULL);
    }

    labrstim_stderr = g_unix_input_stream_new (ls_stderr, TRUE);
    tty_writecmd (tty_fd, "OK", NULL);
}

/**
 * stop_labrstim_command:
 */
static void
stop_labrstim_command ()
{
     if (labrstim_proc_pid != 0) {
         if (kill (labrstim_proc_pid, SIGTERM) != 0)
             tty_writecmd (tty_fd, "ERROR", g_strerror (errno));
         usleep (200);

         /* test if the process is really dead, kill it if necessary */
         if (kill (labrstim_proc_pid, 0) == 0) {
             if (kill (labrstim_proc_pid, SIGKILL) != 0)
                tty_writecmd (tty_fd, "ERROR", g_strerror (errno));
        }
    }

    tty_writecmd (tty_fd, "OK", NULL);
}

/**
 * tty_replace_with_agetty:
 *
 * Replace the current process with agetty for maintenance purposes.
 */
static void
tty_replace_with_agetty ()
{
    gchar *args[] = {"--autologin", "root",
                     "-s",
                     "ttyAMA0",
                     "115200,38400,9600",
                     "vt102",
                     NULL };

    execv ("/sbin/agetty", args);
    /* we do not send an OK as this process won't exist anymore if the command succeeded */
    tty_writecmd (tty_fd, "ERROR", g_strerror (errno));
}

/**
 * shutdown_device:
 *
 * Shut down the whole device until it is manually
 * restarted.
 */
static void
shutdown_device ()
{
    int r;
    r = system ("systemctl poweroff");
    /* we do not send an OK as this process won't exist anymore if the command succeeded */
    if (r != 0) {
        tty_writecmd (tty_fd, "ERROR", g_strerror (errno));
    }
}

/**
 * reboot_device:
 *
 * Reboot the device.
 */
static void
reboot_device ()
{
    int r;
    r = system ("systemctl reboot");
    /* we do not send an OK as this process won't exist anymore if the command succeeded */
    if (r != 0) {
        tty_writecmd (tty_fd, "ERROR", g_strerror (errno));
    }
}

/**
 * process_command:
 *
 * Process a command.
 */
static void
process_command (const gchar *cmd)
{
    if (g_strcmp0 (cmd, "VERSION") == 0) {
        tty_write (tty_fd, "VERSION ");
        tty_write (tty_fd, PACKAGE_NAME);
        tty_write (tty_fd, " ");
        tty_write (tty_fd, PACKAGE_VERSION);
        tty_write (tty_fd, "\n");
    } else if (g_str_has_prefix (cmd, "RUN ")) {
        run_labrstim_command (cmd + 4);
    } else if (g_strcmp0 (cmd, "STOP") == 0) {
        stop_labrstim_command (tty_fd);
    } else if (g_strcmp0 (cmd, "PING") == 0) {
        tty_writecmd (tty_fd, "PONG", NULL);
    } else if (g_strcmp0 (cmd, "NOOP") == 0) {
        /* we never greet back */
    } else if (g_strcmp0 (cmd, "I_SOLEMNLY_SWEAR_THAT_IM_UP_TO_NO_GOOD") == 0) {
        tty_replace_with_agetty ();
    } else if (g_strcmp0 (cmd, "SHUTDOWN") == 0) {
        shutdown_device ();
    } else if (g_strcmp0 (cmd, "REBOOT") == 0) {
        reboot_device ();
    } else {
        tty_writecmd (tty_fd, "UNKNOWN_CMD", NULL);
    }
}

/**
 * receive_data:
 */
static gboolean
receive_data (GIOChannel *io, GIOCondition condition, gpointer data)
{
    unsigned char buf[128];
    int rdlen;
    guint i;
    GString *strbuf = (GString*) data;
    int fd = g_io_channel_unix_get_fd (io);

    rdlen = read (fd, buf, sizeof(buf) - 1);

    if (rdlen < 0) {
        g_printerr ("Read Error: %d: %s\n", rdlen, g_strerror (errno));
        tty_writecmd (fd, "READ_ERROR", NULL);
        return TRUE;
    }

    for (i = 0; i < rdlen; i++) {
        if (buf[i] == '\n') {
            process_command (strbuf->str);
            g_string_truncate (strbuf, 0);
        } else {
            g_string_append_c (strbuf, buf[i]);
        }
    }

    return TRUE;
}

static gboolean
lsttyctl_make_realtime (void)
{
    /* set scheduler policy */
    struct sched_param param;
    param.sched_priority = LSTTYCTL_PRIORITY;
    if (sched_setscheduler (0, SCHED_FIFO, &param) == -1) {
        g_printerr ("Do you have permission to run real-time applications? You might need to be root or use sudo\n");
        return FALSE;
    }

    return TRUE;
}

int main()
{
    g_autoptr(GMainLoop) loop = NULL;
    g_autoptr(GString) strbuf = NULL;
    GIOChannel *iochan;

    if (!lsttyctl_make_realtime ())
        return 2;

    tty_fd = open (PI_TTY_CTLPORT, O_RDWR | O_NOCTTY | O_SYNC);
    if (tty_fd < 0) {
        g_printerr ("Unable to open tty at %s: %s\n", PI_TTY_CTLPORT, g_strerror (errno));
        return 2;
    }

    /* baudrate 115200, 8 bits, no parity, 1 stop bit */
    tty_set_interface_attribs (tty_fd, B115200);

    /* new buffer string to store the data we receive */
    strbuf = g_string_sized_new (48);

    /* tell the world that we started up */
    tty_writecmd (tty_fd, "STARTUP", NULL);

    loop = g_main_loop_new (NULL, FALSE);

    iochan = g_io_channel_unix_new (tty_fd);
    g_io_add_watch (iochan, G_IO_IN, receive_data, strbuf);
    g_io_channel_unref (iochan);

    g_main_loop_run (loop);

    return 0;
}
