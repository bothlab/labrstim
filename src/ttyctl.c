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

#include "defaults.h"

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
 * tty_writeln:
 *
 * Write line to tty.
 */
static gboolean
tty_writeln (int fd, const gchar *str)
{
    g_autofree gchar *strnl;

    strnl = g_strdup_printf ("%s\n", str);
    return tty_write (fd, strnl);
}

/**
 * process_command:
 *
 * Process a command.
 */
static void
process_command (int fd, const gchar *cmd)
{
    if (g_strcmp0 (cmd, "VERSION") == 0) {
        tty_write (fd, "VERSION ");
        tty_write (fd, PACKAGE_NAME);
        tty_write (fd, " ");
        tty_write (fd, PACKAGE_VERSION);
        tty_write (fd, "\n");
    } else if (g_str_has_prefix (cmd, "RUN ")) {
        printf ("Yeah! Run!: %s\n", cmd);
    } else if (g_strcmp0 (cmd, "PING") == 0) {
        tty_writeln (fd, "PONG");
    } else if (g_strcmp0 (cmd, "NOOP") == 0) {
        /* we never greet back */
    } else {
        tty_writeln (fd, "UNKNOWN_CMD");
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
    GString *strbuf = (GString*) data;
    int fd = g_io_channel_unix_get_fd (io);

    do {
        guint i;
        rdlen = read(fd, buf, sizeof(buf) - 1);

        if (rdlen < 0) {
            g_printerr ("Read Error: %d: %s\n", rdlen, g_strerror (errno));
            tty_writeln (fd, "READ_ERROR");
            break;
        }

        for (i = 0; i < rdlen; i++) {
            if (buf[i] == '\n') {
                process_command (fd, strbuf->str);
                g_string_truncate (strbuf, 0);
            } else {
                g_string_append_c (strbuf, buf[i]);
            }
        }
    } while (rdlen > 0);

    return TRUE;
}

int main()
{
    int fd;
    g_autoptr(GMainLoop) loop = NULL;
    g_autoptr(GString) strbuf = NULL;
    GIOChannel *iochan;

    fd = open (PI_TTY_CTLPORT, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        g_printerr ("Unable to open tty at %s: %s\n", PI_TTY_CTLPORT, g_strerror (errno));
        return 2;
    }

    /* baudrate 115200, 8 bits, no parity, 1 stop bit */
    tty_set_interface_attribs (fd, B115200);

    /* new buffer string to store the data we receive */
    strbuf = g_string_sized_new (48);

    /* tell the world that we started up */
    tty_writeln (fd, "STARTUP");

    loop = g_main_loop_new (NULL, FALSE);

    iochan = g_io_channel_unix_new (fd);
    g_io_add_watch (iochan, G_IO_IN, receive_data, strbuf);
    g_io_channel_unref (iochan);

    g_main_loop_run (loop);

    return 0;
}
