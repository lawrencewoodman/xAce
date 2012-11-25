/* Tests for cassette machine emulation
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
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tape.h"

/* Generate a block of data to save and compare */
static void
generate_block(char *block, int num_bytes, int first_num)
{
  int i;
  for (i = 0; i < num_bytes; i++) {
    block[i] = (first_num+i) % 256;
  }
}

/* Return if block read is as expected */
static bool
block_correct(FILE *fp, unsigned char *expected_block, int num_bytes)
{
  int i;
  unsigned char checksum = 0;
  int lo_byte = fgetc(fp);
  int hi_byte = fgetc(fp);

  if (num_bytes != (lo_byte + (hi_byte << 8))-1)
    return false;
  for (i = 0; i < num_bytes; i++) {
    if (fgetc(fp) != expected_block[i]) {
      return false;
    }
    checksum ^= expected_block[i];
  }

 if (fgetc(fp) != checksum)
   return false;

 return true;
}

static struct {
  int observer_called;
  int tape_attached;
  int tape_pos;
  char tape_filename[TAPE_MAX_FILENAME_SIZE];
  TapeMessageType message_type;
  char message[TAPE_MAX_MESSAGE_SIZE];
} observer_status;

static void
observer_status_init(void)
{
  observer_status.observer_called = 0;
  observer_status.tape_attached = 0;
  observer_status.tape_pos = 0;
  observer_status.tape_filename[0] = 0;
  observer_status.message_type = TAPE_NO_MESSAGE;
  observer_status.message[0] = 0;
}

static void
observer(int tape_attached, int tape_pos,
 const char tape_filename[TAPE_MAX_FILENAME_SIZE],
 TapeMessageType message_type, const char message[TAPE_MAX_MESSAGE_SIZE])
{
  observer_status.observer_called = 1;
  observer_status.tape_attached = tape_attached;
  observer_status.tape_pos = tape_pos;
  observer_status.message_type = message_type;
  strncpy(observer_status.tape_filename, tape_filename,
          TAPE_MAX_FILENAME_SIZE);
  strncpy(observer_status.message, message,
          TAPE_MAX_MESSAGE_SIZE);
}

static void
test_tape_add_observer()
{
  char *filename = "fixtures/test.tap";

  observer_status_init();
  tape_add_observer(observer);

  tape_attach(filename);
  assert(observer_status.observer_called == 1);
  assert(observer_status.tape_attached == 1);
  assert(observer_status.tape_pos == 0);
  assert(strcmp(filename, observer_status.tape_filename) == 0);
  assert(observer_status.message_type == TAPE_MESSAGE);
  assert(strcmp(observer_status.message, "Tape image attached.") == 0);
  tape_detach();

  tape_clear_observers();
}

static void
test_tape_attach_file_exists()
{
  FILE *fp;
  char *filename = "fixtures/test.tap";  

  observer_status_init();
  tape_add_observer(observer);

  fp = tape_attach(filename);

  assert(fp != NULL);
  assert(ftell(fp) == 0);
  assert(fgetc(fp) == 0x1A);

  assert(observer_status.observer_called == 1);
  assert(observer_status.tape_attached == 1);
  assert(observer_status.tape_pos == 0);
  assert(strcmp(filename, observer_status.tape_filename) == 0);
  assert(observer_status.message_type == TAPE_MESSAGE);
  assert(strcmp(observer_status.message, "Tape image attached.") == 0);

  tape_detach();
  tape_clear_observers();
}

static void
test_tape_attach_file_doesnt_exist()
{
  char *filename = tmpnam(NULL);  
  FILE *fp = tape_attach(filename);

  assert(fp != NULL);
  assert(ftell(fp) == 0);
  assert(fputc(65, fp) == 65);
  tape_detach();
}

static void
test_tape_attach_file_can_not_create()
{
  char *filename = "";
  FILE *fp;

  observer_status_init();
  tape_add_observer(observer);
  fp = tape_attach(filename);

  assert(fp == NULL);
  assert(observer_status.observer_called == 1);
  assert(observer_status.tape_attached == 0);
  assert(observer_status.tape_pos == 0);
  assert(strcmp(filename, observer_status.tape_filename) == 0);
  assert(observer_status.message_type == TAPE_ERROR);
  assert(strcmp(observer_status.message, "Couldn't create file.") == 0);

  tape_detach();
  tape_clear_observers();
}

static void
test_tape_detach_notifies_observers()
{
  char *filename = "fixtures/test.tap";  

  observer_status_init();
  tape_add_observer(observer);
  tape_attach(filename);
  tape_detach();

  assert(observer_status.observer_called == 1);
  assert(observer_status.tape_attached == 0);
  assert(observer_status.tape_pos == 0);
  assert(observer_status.tape_filename[0] == 0);
  assert(observer_status.message_type == TAPE_MESSAGE);
  assert(strcmp(observer_status.message, "Tape image detached.") == 0);

  tape_clear_observers();
}

static void
test_tape_load_p_first_dict_on_tape()
{
  int i;
  unsigned char mem[65536];
  char *filename = "fixtures/test.tap";
  char *filename_on_tape = "test      ";

  tape_attach(filename);
  mem[9985] = 0;
  strncpy(mem+9986, filename_on_tape, 10);

  observer_status_init();
  tape_add_observer(observer);

  tape_load_p(mem, 10);
  /* Check correct name is in memory */
  assert(memcmp(mem+10+1, mem+9986, 10) == 0);

  assert(observer_status.observer_called == 1);
  assert(observer_status.tape_attached == 1);
  assert(observer_status.tape_pos == 28);
  assert(strcmp(observer_status.tape_filename, filename) == 0);
  assert(observer_status.message_type == TAPE_MESSAGE);
  assert(strcmp(observer_status.message, "Found file: test") == 0);

  tape_load_p(mem, 10000);
  /* Check data block is loaded */
  assert(mem[10000] == 0x54);
  assert(mem[10001] == 0x45);
  assert(mem[10027] == 0x73);
  assert(mem[10029] == 0xB6);
  assert(mem[10030] == 0x04);

  assert(observer_status.observer_called == 1);
  assert(observer_status.tape_attached == 1);
  assert(observer_status.tape_pos == 62);
  assert(strcmp(observer_status.tape_filename, filename) == 0);
  assert(observer_status.message_type == TAPE_MESSAGE);
  assert(strcmp(observer_status.message, "Load complete.") == 0);

  tape_detach();
  tape_clear_observers();
}

static void
test_tape_load_p_second_dict_on_tape()
{
  int i;
  unsigned char mem[65536];
  char *filename = "fixtures/test.tap";
  char *filename_on_tape = "test2     ";

  tape_attach(filename);
  observer_status_init();
  tape_add_observer(observer);

  mem[9985] = 0;
  strncpy(mem+9986, filename_on_tape, 10);

  /* Skip first dictionary */
  tape_load_p(mem, 10);

  assert(observer_status.observer_called == 1);
  assert(observer_status.tape_attached == 1);
  assert(observer_status.tape_pos == 62);
  assert(strcmp(observer_status.tape_filename, filename) == 0);
  assert(observer_status.message_type == TAPE_MESSAGE);
  assert(strcmp(observer_status.message, "Skipping file: test") == 0);

  /* Load the header block */
  tape_load_p(mem, 10);
  /* Check correct name is in memory */
  assert(memcmp(mem+10+1, mem+9986, 10) == 0);

  assert(observer_status.observer_called == 1);
  assert(observer_status.tape_attached == 1);
  assert(observer_status.tape_pos == 90);
  assert(strcmp(observer_status.tape_filename, filename) == 0);
  assert(observer_status.message_type == TAPE_MESSAGE);
  assert(strcmp(observer_status.message, "Found file: test2") == 0);

  /* Load the data block */
  tape_load_p(mem, 10000);
  /* Check data block is loaded */
  assert(mem[10000] == 0x54);
  assert(mem[10001] == 0x45);
  assert(mem[10029] == 0x32);
  assert(mem[10030] == 0xB6);
  assert(mem[10031] == 0x04);

  assert(observer_status.observer_called == 1);
  assert(observer_status.tape_attached == 1);
  assert(observer_status.tape_pos == 125);
  assert(strcmp(observer_status.tape_filename, filename) == 0);
  assert(observer_status.message_type == TAPE_MESSAGE);
  assert(strcmp(observer_status.message, "Load complete.") == 0);

  tape_detach();
  tape_clear_observers();
}

static void
test_tape_save_p()
{
  char *filename = tmpnam(NULL);  
  FILE *fp;
  unsigned char mem[65536];

  /* Create header and data block */
  generate_block(mem, 25, 'A');
  generate_block(mem+50, 300, 6);
 
  tape_attach(filename);

  /* Save the blocks */
  observer_status_init();
  tape_add_observer(observer);

  tape_save_p(mem, 25);
  assert(observer_status.observer_called == 1);
  assert(observer_status.tape_attached == 1);
  assert(observer_status.tape_pos == 28);
  assert(strcmp(filename, observer_status.tape_filename) == 0);
  assert(observer_status.message_type == TAPE_MESSAGE);
  assert(strcmp(observer_status.message, "Saving to file: BCDEFGHIJK") == 0);

  tape_save_p(mem+50, 300);
  assert(observer_status.observer_called == 1);
  assert(observer_status.tape_attached == 1);
  assert(observer_status.tape_pos == 331);
  assert(strcmp(filename, observer_status.tape_filename) == 0);
  assert(observer_status.message_type == TAPE_MESSAGE);
  assert(strcmp(observer_status.message, "Save complete.") == 0);

  tape_detach();
  tape_clear_observers();

  /* Check the header and data blocks */
  fp = fopen(filename, "rb");
  assert(fp != NULL);
 
  assert(block_correct(fp, mem, 25));
  assert(block_correct(fp, mem+50, 300));
  assert(fgetc(fp) == EOF);
  fclose(fp);
}

/* Tests that tape_save_p() truncates the tape properly */
static void
test_tape_save_p_truncate()
{
  char *filename = tmpnam(NULL);  
  FILE *fp;
  unsigned char mem[65536];
  int i;

  /* Save 3 lots of header and data blocks */
  tape_attach(filename);
  for (i = 0; i < 3; i++) {
    generate_block(mem+(100*i), 25+i, 'A'+i);
    generate_block(mem+(100*i)+50, 30+i, 6+i);
    tape_save_p(mem+(100*i), 25+i);
    tape_save_p(mem+(100*i)+50, 30+i);
  }
  tape_detach();

  tape_attach(filename);

  /* Load first header and data blocks */
  for(i = 0; i < 10; i++) {
    mem[9986+i] = mem[100+i];
  }
  tape_load_p(mem, 1000);
  tape_load_p(mem, 1150);

  /* Save 1 set of header and data blocks after the first 2 blocks */
  generate_block(mem+2000, 28, 'N');
  generate_block(mem+2050, 40, 20);
  tape_save_p(mem+2000, 28);
  tape_save_p(mem+2050, 20);
  tape_detach();

  /* Check that tape only contains first header and data blocks followed by
   * the last saved header and data blocks */
  fp = fopen(filename, "rb");
  assert(fp != NULL);

  assert(block_correct(fp, mem, 25));
  assert(block_correct(fp, mem+50, 30));
  
  assert(block_correct(fp, mem+2000, 28));
  assert(block_correct(fp, mem+2050, 20));

  assert(fgetc(fp) == EOF);
  fclose(fp);
}

int main()
{
  test_tape_add_observer();
  test_tape_attach_file_exists();  
  test_tape_attach_file_doesnt_exist();
  test_tape_attach_file_can_not_create();
  test_tape_detach_notifies_observers();
  test_tape_load_p_first_dict_on_tape();
  test_tape_load_p_second_dict_on_tape();
  test_tape_save_p();
  test_tape_save_p_truncate();
  exit(0);
}
