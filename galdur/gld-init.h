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

#ifndef __GLD_INIT_H
#define __GLD_INIT_H

#include <glib.h>
#include <stdint.h>

gboolean    gld_board_initialize (void);
void        gld_board_shutdown (void);

void        gld_board_set_spi_clock_divider (const uint16_t divider);
void        gld_board_set_aux_spi_clock_divider (const uint16_t divider);

#endif /* __GLD_INIT_H */
