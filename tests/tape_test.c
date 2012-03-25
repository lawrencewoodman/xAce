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

#include "../tape.h"

/* Generate a block of data to save and compare */
static void
generate_block(char *block, int num_bytes, int first_num)
{
  int i;
  for (i = 0; i < num_bytes; i++) {
    block[i] = (first_num+i) % 256;
  }
}

/* Return true/false if block read is as expected */
static bool
check_block(FILE *fp, unsigned char *expected_block, int num_bytes)
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


static void
test_tape_attach_file_exists()
{
  char *filename = "fixtures/test.tap";  
  FILE *fp = tape_attach(filename);

  assert(fp != NULL);
  assert(ftell(fp) == 0);
  assert(fgetc(fp) == 0x1A);
  tape_detach();
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
test_tape_load_p_first_dict_on_tape()
{
  char *filename = "fixtures/test.tap";  
  FILE *fp = tape_attach(filename);
  int i;
  unsigned char mem[65536];

  mem[9985] = 0;
  mem[9986] = 't';
  mem[9987] = 'e';
  mem[9988] = 's';
  mem[9989] = 't';
  mem[9990] = ' ';
  mem[9991] = ' ';
  mem[9992] = ' ';
  mem[9993] = ' ';
  mem[9994] = ' ';
  mem[9995] = ' ';

  tape_load_p(mem, 10);
  /* Check correct name is in memory */
  for (i = 0; i < 10; i++)
    assert(mem[10+1+i] == mem[9986+i]);

  tape_load_p(mem, 10000);
  /* Check data block is loaded */
  assert(mem[10000] == 0x54);
  assert(mem[10001] == 0x45);
  assert(mem[10027] == 0x73);
  assert(mem[10029] == 0xB6);
  assert(mem[10030] == 0x04);

  tape_detach();
}

static void
test_tape_load_p_second_dict_on_tape()
{
  char *filename = "fixtures/test.tap";  
  FILE *fp = tape_attach(filename);
  int i;
  unsigned char mem[65536];

  mem[9985] = 0;
  mem[9986] = 't';
  mem[9987] = 'e';
  mem[9988] = 's';
  mem[9989] = 't';
  mem[9990] = '2';
  mem[9991] = ' ';
  mem[9992] = ' ';
  mem[9993] = ' ';
  mem[9994] = ' ';
  mem[9995] = ' ';

  /* Skip first dictionary */
  tape_load_p(mem, 10);

  /* Load the header block */
  tape_load_p(mem, 10);
  /* Check correct name is in memory */
  for (i = 0; i < 10; i++)
    assert(mem[10+1+i] == mem[9986+i]);

  /* Load the data block */
  tape_load_p(mem, 10000);
  /* Check data block is loaded */
  assert(mem[10000] == 0x54);
  assert(mem[10001] == 0x45);
  assert(mem[10029] == 0x32);
  assert(mem[10030] == 0xB6);
  assert(mem[10031] == 0x04);

  tape_detach();
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
 
  /* Save the blocks */
  tape_attach(filename);
  tape_save_p(mem, 25);
  tape_save_p(mem+50, 300);
  tape_detach();

  /* Check the header and data blocks */
  fp = fopen(filename, "rb");
  assert(fp != NULL);
 
  assert(check_block(fp, mem, 25));
  assert(check_block(fp, mem+50, 300));
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

  assert(check_block(fp, mem, 25));
  assert(check_block(fp, mem+50, 30));
  
  assert(check_block(fp, mem+2000, 28));
  assert(check_block(fp, mem+2050, 20));

  assert(fgetc(fp) == EOF);
  fclose(fp);
}

int main()
{
  test_tape_attach_file_exists();  
  test_tape_attach_file_doesnt_exist();
  test_tape_load_p_first_dict_on_tape();
  test_tape_load_p_second_dict_on_tape();
  test_tape_save_p();
  test_tape_save_p_truncate();
}
