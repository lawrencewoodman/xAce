/* Tests for the spooler
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "spooler.h"

static struct {
  int observer_called;
  int clear_keyboard_called;
  int keypress_called;
  KeySym keypress_key;
  int keypress_key_state;
  SpoolerMessage message;
} observer_status;

static void
observer_status_init(void)
{
  observer_status.observer_called = 0;
  observer_status.clear_keyboard_called = 0;
  observer_status.keypress_called = 0;
  observer_status.keypress_key = 0;
  observer_status.keypress_key_state = 0;
  observer_status.message = SPOOLER_NO_MESSAGE;
}

static void
spooler_observer(SpoolerMessage message)
{
  observer_status.observer_called = 1;
  observer_status.message = message;
}

static void
clear_keyboard(void)
{
  observer_status.clear_keyboard_called = 1;
}

static void
keypress(KeySym ks, int key_state)
{
  observer_status.keypress_called = 1;
  observer_status.keypress_key = ks;
  observer_status.keypress_key_state = key_state;
}


static void
test_spooler_open_successful()
{
  char *filename = "fixtures/star.spool";

  observer_status_init();
  spooler_init(spooler_observer, clear_keyboard, NULL);
  spooler_open(filename);

  assert(observer_status.observer_called);
  assert(observer_status.message == SPOOLER_OPENED);
  assert(spooler_active());

  spooler_close();
}

static void
test_spooler_open_unsuccessful()
{
  char *filename = tmpnam(NULL);

  observer_status_init();
  spooler_init(spooler_observer, NULL, NULL);
  spooler_open(filename);

  assert(observer_status.observer_called);
  assert(observer_status.message == SPOOLER_OPEN_ERROR);
  assert(!spooler_active());
}

static void
test_spooler_close()
{
  char *filename = "fixtures/star.spool";

  observer_status_init();
  spooler_init(spooler_observer, clear_keyboard, NULL);
  spooler_open(filename);
  spooler_close();

  assert(observer_status.observer_called);
  assert(observer_status.clear_keyboard_called);
  assert(observer_status.message == SPOOLER_CLOSED);
  assert(!spooler_active());
}

static void
test_spooler_read()
{
  int text_pos;
  char *filename = "fixtures/star.spool";
  char *comparison_text = ": star 42 emit ;\n";

  spooler_init(spooler_observer, clear_keyboard, keypress);
  spooler_open(filename);

  for (text_pos = 0; text_pos < strlen(comparison_text); text_pos++) {
    /* Test key pressed */
    observer_status_init();
    spooler_read();
    assert(observer_status.keypress_called);
    assert(observer_status.keypress_key == comparison_text[text_pos]);
    assert(observer_status.keypress_key_state == 0);

    /* Test key cleared */
    observer_status_init();
    spooler_read();
    assert(observer_status.clear_keyboard_called);
  }

  observer_status_init();
  spooler_read();
  assert(observer_status.observer_called);
  assert(observer_status.message == SPOOLER_CLOSED);
}

static void
test_spooler_read_after_close_no_action()
{
  int text_pos;
  char *filename = "fixtures/star.spool";
  char *comparison_text = ": star 42 emit ;\n";

  spooler_init(spooler_observer, clear_keyboard, keypress);
  spooler_open(filename);

  for (text_pos = 0; text_pos < strlen(comparison_text); text_pos++) {
    spooler_read();
    spooler_read();
  }

  spooler_read();
  assert(!spooler_active());

  spooler_read();
  observer_status_init();
  assert(!observer_status.observer_called);
  assert(!observer_status.keypress_called);
  assert(!observer_status.clear_keyboard_called);
}

int main()
{
  test_spooler_open_successful();
  test_spooler_open_unsuccessful();
  test_spooler_close();
  test_spooler_read();
  test_spooler_read_after_close_no_action();
  exit(0);
}
