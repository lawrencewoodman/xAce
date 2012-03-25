/* Declarations for handling the emulated cassette machine
 *
 * Copyright (C) 2012 Lawrence Woodman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef TAPE_H
#define TAPE_H

extern void tape_patches(char *mem);
extern FILE* tape_attach(char *filename);
extern void tape_detach();
extern void tape_load_p(char *mem, int block_dest_offset);
extern void tape_save_p(char *mem, int block_size);

#endif
