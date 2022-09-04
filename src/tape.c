/* Emulation of the cassette machine
 *
 * Copyright (C) 1994 Ian Collier. 
 * xz81 changes (C) 1995-6 Russell Marks.
 * xace changes (C) 1997 Edward Patel.
 * xace changes (C) 2010-12 Lawrence Woodman.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>

#include "z80.h"
#include "tape.h"

static void tape_notify_observers(TapeMessageType message_type,
  char message[TAPE_MAX_MESSAGE_SIZE]);
static int tape_eof(void);
static void tape_rewind_to_start(void);
static void tape_attach_empty_tape(char load_type);
static void tape_extract_filename(char *filename, char *mem);
static void tape_load_block(char *mem, int block_dest_offset);
static void tape_skip_block(void);
static void tape_load_empty_tape_block(char *mem, int block_dest_offset);
static void tape_truncate(void);
static void tape_save_block(char *block, int block_size);
static char tape_calc_checksum(char *data, int data_size);

static unsigned char low_byte(int word) { return(word & 0xff); }
static unsigned char high_byte(int word) { return(word >> 8); }

/* A small screen will be loaded if wanted file can't be opened */
static unsigned char empty_bytes[799] = {
  0x1a,0x00,0x20,0x6f,
  0x74,0x68,0x65,0x72,
  0x20,0x20,0x20,0x20,
  0x20,0x00,0x03,0x00,
  0x24,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,
  0x20,0x20,0x20,0x20,
  0x01,0x03,0x43,0x6f,
  0x75,0x6c,0x64,0x6e,
  0x27,0x74,0x20,0x6c,
  0x6f,0x61,0x64,0x20,
  0x79,0x6f,0x75,0x72,
  0x20,0x66,0x69,0x6c,
  0x65,0x21,0x20,
};

/* A small forth program will be loaded if wanted file can't be opened */
static unsigned char empty_dict[] = {
  0x1a,0x00,0x00,0x6f,
  0x74,0x68,0x65,0x72,
  0x20,0x20,0x20,0x20,
  0x20,0x2a,0x00,0x51,
  0x3c,0x58,0x3c,0x4c,
  0x3c,0x4c,0x3c,0x4f,
  0x3c,0x7b,0x3c,0x20,
  0x2b,0x00,0x52,0x55,
  0xce,0x27,0x00,0x49,
  0x3c,0x03,0xc3,0x0e,
  0x1d,0x0a,0x96,0x13,
  0x18,0x00,0x43,0x6f,
  0x75,0x6c,0x64,0x6e,
  0x27,0x74,0x20,0x6c,
  0x6f,0x61,0x64,0x20,
  0x79,0x6f,0x75,0x72,
  0x20,0x66,0x69,0x6c,
  0x65,0x21,0xb6,0x04,
  0xff,0x00
};

static FILE *tape_fp = NULL;
static int empty_tape_pos = 0;
static unsigned char *empty_tape;
static char tape_filename[TAPE_MAX_FILENAME_SIZE+1] = "";
static char requested_filename[11];  /* The file requested on the tape */

static int observer_count = 0;
static TapeObserver observers[TAPE_MAX_OBSERVERS] = {NULL};

void
tape_add_observer(TapeObserver tape_observer)
{
  if (observer_count < TAPE_MAX_OBSERVERS)
    observers[observer_count++] = tape_observer;
}

void
tape_clear_observers(void)
{
  observer_count = 0;
}

void
tape_patches(char *mem)
{
  /* patch the ROM here */
  mem[0x18a7]=0xed; /* for tape_load_p */
  mem[0x18a8]=0xfc;
  mem[0x18a9]=0xc9;

  mem[0x1820]=0xed; /* for tape_save_p */
  mem[0x1821]=0xfd;
  mem[0x1822]=0xc9;
}

FILE *
tape_attach(char *filename)
{
  tape_detach();

  if ((tape_fp = fopen(filename, "rb+")) == NULL)
    tape_fp = fopen(filename, "wb+");
  else
    tape_rewind_to_start();

  if (tape_fp) {
    strncpy(tape_filename, filename, TAPE_MAX_FILENAME_SIZE);
    tape_notify_observers(TAPE_MESSAGE, "Tape image attached.");
  } else {
    tape_notify_observers(TAPE_ERROR, "Couldn't create file.");
  }

  return tape_fp;
}

void
tape_detach(void)
{
  if (tape_fp != NULL) {
    fclose(tape_fp);
    tape_fp = NULL;
    tape_notify_observers(TAPE_MESSAGE, "Tape image detached.");
  }
}

void
tape_load_p(char *mem, int block_dest_offset)
{
  char found_filename[11];
  static int load_header = 1;
  char message[TAPE_MAX_MESSAGE_SIZE] = "";

  if (tape_eof()) {
    tape_notify_observers(TAPE_MESSAGE, "End of tape reached.  Rewinding.");
    tape_rewind_to_start();
    load_header = 1;
  }

  if (load_header) {
    tape_attach_empty_tape(mem[9985]);
    tape_extract_filename(requested_filename, mem+9985+1);
    sprintf(message, "Searching for file: %s", requested_filename);
    tape_notify_observers(TAPE_MESSAGE, message);

    tape_load_block(mem, block_dest_offset);
    tape_extract_filename(found_filename, mem+block_dest_offset+1);
    if (strcmp(requested_filename, found_filename) != 0) {
      sprintf(message, "Skipping file: %s", found_filename);
      tape_skip_block();
    } else {
      sprintf(message, "Found file: %s", found_filename);
      load_header = 0;
    }
  } else {
    tape_load_block(mem, block_dest_offset);
    sprintf(message, "Load complete.");
    load_header = 1;
  }

  tape_notify_observers(TAPE_MESSAGE, message);
}

void
tape_save_p(char *mem, int block_size)
{
  char filename[32];
  static int save_header = 1;
  char message[TAPE_MAX_MESSAGE_SIZE] = "";

  if (!tape_fp) {
    if (save_header) {
      tape_notify_observers(TAPE_MESSAGE, "No tape file attached.");
      save_header = 0;
    }
    return;
  }

  if (save_header) {
    tape_extract_filename(filename, mem+1);
    sprintf(message, "Saving to file: %s", filename);
    tape_truncate();
  } else {
    sprintf(message, "Save complete.");
  }
  save_header = !save_header;
  tape_save_block(mem, block_size);
  tape_notify_observers(TAPE_MESSAGE, message);
}

static void
tape_notify_observers(TapeMessageType message_type,
  char message[TAPE_MAX_MESSAGE_SIZE])
{
  int i;
  int tape_attached;
  int tape_pos;

  /* Copies of the filename and message to mitigate risk of internal
   * representation being overwritten by a badly behaved observer */
  char _tape_filename[TAPE_MAX_FILENAME_SIZE];
  char _message[TAPE_MAX_MESSAGE_SIZE];

  tape_attached = !!tape_fp;
  if (tape_fp) {
    tape_pos = ftell(tape_fp);
    strncpy(_tape_filename, tape_filename, TAPE_MAX_FILENAME_SIZE);
  } else {
    tape_pos = empty_tape_pos;
    _tape_filename[0] = 0;
  }

  strncpy(_message, message, TAPE_MAX_MESSAGE_SIZE);
  for (i = 0; i < observer_count; i++) {
    observers[i](tape_attached, tape_pos, _tape_filename,
      message_type, _message);
  }
}

/* load_type - If 0 indicates dictionary, else bytes */
static void
tape_attach_empty_tape(char load_type)
{
  if (load_type == 0) {
    empty_tape = empty_dict;
  } else {
    empty_tape = empty_bytes;
  }
  empty_tape_pos = 0;
}

static int
tape_eof(void)
{
  return ((tape_fp && feof(tape_fp)) || (!tape_fp && empty_tape_pos > 28));
}

static void
tape_rewind_to_start(void)
{
  if (tape_fp)
    fseek(tape_fp, 0, SEEK_SET);
  else
    empty_tape_pos = 0;
}

/**
 *  Extract filename from memory
 *  filename must be at least 11 bytes in size
 */
static void
tape_extract_filename(char *filename, char *mem)
{
  int i;
  for (i = 0; i < 10; i++) {
    filename[i] = isspace(mem[i]) ? 0 : mem[i];
  }
  filename[i] = 0;
}

static void
tape_load_empty_tape_block(char *mem, int block_dest_offset)
{
  int block_size;

  block_size = empty_tape[empty_tape_pos++];
  block_size += empty_tape[empty_tape_pos++] << 8;
  memcpy(mem+block_dest_offset, &empty_tape[empty_tape_pos], block_size);
  empty_tape_pos += block_size;
}

static void
tape_load_block(char *mem, int block_dest_offset)
{
  int block_size;

  if (tape_fp) {
    block_size = fgetc(tape_fp);
    if (!feof(tape_fp)) {
      block_size += fgetc(tape_fp) << 8;
      /* Read block less the checksum */
      fread(mem+block_dest_offset, 1, block_size-1, tape_fp);
      fgetc(tape_fp); /* skip checksum */
    }
  } else {
    tape_load_empty_tape_block(mem, block_dest_offset);
  }
}

static void
tape_skip_block(void)
{
  int block_size;

  if (tape_fp) {
    block_size = fgetc(tape_fp);
    block_size += fgetc(tape_fp) << 8;
    fseek(tape_fp, block_size, SEEK_CUR);
  } else {
    block_size = empty_tape[empty_tape_pos++];
    block_size += empty_tape[empty_tape_pos++] << 8;
    empty_tape_pos += block_size;
  }
}

static void
tape_save_block(char *block, int block_size)
{
  fputc(low_byte(block_size+1), tape_fp);
  fputc(high_byte(block_size+1), tape_fp);
  fwrite(block, 1, block_size, tape_fp);
  fputc(tape_calc_checksum(block, block_size), tape_fp);
  fflush(tape_fp);
}

static void
tape_truncate(void)
{
  if (ftruncate(fileno(tape_fp), ftell(tape_fp)) != 0) {
    tape_notify_observers(TAPE_ERROR, "Couldn't truncate file.");
  }
}

/* Returns a checksum calculated by XORing each value in data */
static char
tape_calc_checksum(char *data, int data_size)
{
  int i;
  char checksum = 0;

  for (i = 0; i < data_size; i++) {
    checksum ^= data[i];
  }

  return checksum;
}
