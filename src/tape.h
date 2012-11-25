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

#define TAPE_MAX_FILENAME_SIZE 256
#define TAPE_MAX_MESSAGE_SIZE 256
#define TAPE_MAX_OBSERVERS 10

typedef enum TapeMessageType {
  TAPE_NO_MESSAGE,
  TAPE_MESSAGE,
  TAPE_ERROR,
} TapeMessageType;

/**
 * Observer for the status of the tape emulation
 * tape_attached - True/False
 * tape_pos - Position on the tape
 * _tape_filename - Name of the attached tape file.  This is shared
 *                  between all the observers notified, so don't overwrite.
 * message_type - NO_MESSAGE, MESSAGE or ERROR
 * message - The message to pass on
 */
typedef void (*TapeObserver)(int tape_attached, int tape_pos,
  const char _tape_filename[TAPE_MAX_FILENAME_SIZE],
  TapeMessageType message_type,
  const char message[TAPE_MAX_MESSAGE_SIZE]);

void tape_clear_observers(void);
void tape_add_observer(TapeObserver tape_observer);
extern void tape_patches(char *mem);
extern FILE* tape_attach(char *filename);
extern void tape_detach();
extern void tape_load_p(char *mem, int block_dest_offset);
extern void tape_save_p(char *mem, int block_size);

#endif
