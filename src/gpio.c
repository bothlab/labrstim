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

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "gpio.h"

/**
 * gpio_export:
 */
int
gpio_export (int pin)
{
#define BUFFER_MAX 3
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open ("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) {
        fprintf (stderr, "Failed to open export for writing!\n");
        return -1;
    }

    bytes_written = snprintf (buffer, BUFFER_MAX, "%d", pin);
    write (fd, buffer, bytes_written);
    close (fd);
    return 0;
}

/**
 * gpio_unexport:
 */
int
gpio_unexport (int pin)
{
    char buffer[BUFFER_MAX];
    ssize_t bytes_written;
    int fd;

    fd = open ("/sys/class/gpio/unexport", O_WRONLY);
    if (fd < 0) {
        fprintf (stderr, "Failed to open unexport for writing!\n");
        return -1;
    }

    bytes_written = snprintf (buffer, BUFFER_MAX, "%d", pin);
    write (fd, buffer, bytes_written);
    close (fd);
    return 0;
}

/**
 * gpio_set_direction:
 */
int
gpio_set_direction (int pin, int dir)
{
    static const char s_directions_str[] = "in\0out";

#define DIRECTION_MAX 35
    char path[DIRECTION_MAX];
    int fd;

    snprintf (path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);
    fd = open (path, O_WRONLY);
    if (fd < 0) {
        fprintf (stderr, "Failed to open gpio direction for writing!\n");
        return -1;
    }

    if (-1 ==
        write (fd, &s_directions_str[GPIO_IN == dir ? 0 : 3], GPIO_IN == dir ? 2 : 3)) {
        fprintf (stderr, "Failed to set direction!\n");
        return -1;
    }

    close (fd);
    return 0;
}

/**
 * gpio_read:
 */
int
gpio_read (int pin)
{
#define VALUE_MAX 30
    char path[VALUE_MAX];
    char value_str[3];
    int fd;

    snprintf (path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open (path, O_RDONLY);
    if (fd < 0) {
        fprintf (stderr, "Failed to open gpio value for reading!\n");
        return -1;
    }

    if (-1 == read (fd, value_str, 3)) {
        fprintf (stderr, "Failed to read value!\n");
        return -1;
    }

    close (fd);

    return (atoi (value_str));
}

/**
 * gpio_write:
 */
int
gpio_write (int pin, double value)
{
    static const char s_values_str[] = "01";

    char path[VALUE_MAX];
    int fd;

    snprintf (path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
    fd = open (path, O_WRONLY);
    if (fd < 0) {
        fprintf (stderr, "Failed to open gpio value for writing!\n");
        return -1;
    }

    if (1 != write (fd, &s_values_str[GPIO_LOW == value ? 0 : 1], 1)) {
        fprintf (stderr, "Failed to write value!\n");
        return -1;
    }

    close (fd);
    return 0;
}
