/* Tests for keyboard emulation
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
#include <stdlib.h>
#include <X11/keysym.h>

#include "keyboard.h"

static void
check_keyports(unsigned char *expected_keyports)
{
  int i;
  for (i = 0; i < 8; i++) {
    assert(keyboard_get_keyport(i) == expected_keyports[i]);
  }
}

static struct {
  int handler_called;
  KeySym keySym;
  int key_state;
} non_ace_key_handler_status;

static void
non_ace_key_handler_init(void)
{
  non_ace_key_handler_status.handler_called = 0;
}

static void
non_ace_key_handler(KeySym ks, int key_state)
{
  non_ace_key_handler_status.handler_called = 1;
  non_ace_key_handler_status.keySym = ks;
  non_ace_key_handler_status.key_state = key_state;
}

static void
test_keyboard_clear()
{
  unsigned char expected_keyports[8] = {
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff
  };

  keyboard_init(non_ace_key_handler);
  keyboard_keypress(XK_3, 0);
  keyboard_keypress(XK_7, 0);
  keyboard_keypress(XK_u, 0);
  keyboard_keypress(XK_e, 0);
  keyboard_keypress(XK_f, 0);
  keyboard_keypress(XK_l, 0);
  keyboard_keypress(XK_n, 0);
  keyboard_keypress(XK_z, 0);
  keyboard_clear();

  check_keyports(expected_keyports);
}

static void
test_keyboard_keypress_single_key()
{
  unsigned char expected_keyports[8] = {
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xfe
  };

  keyboard_init(non_ace_key_handler);
  keyboard_keypress('\t', 0);
  check_keyports(expected_keyports);
}

static void
test_keyboard_keypress_multiple_keys()
{
  unsigned char expected_keyports[8] = {
    0xff, 0xf7, 0xfb, 0xff,
    0xf7, 0xf7, 0xff, 0xfb
  };

  keyboard_init(non_ace_key_handler);
  keyboard_keypress(XK_7, 0);
  keyboard_keypress(XK_u, 0);
  keyboard_keypress(XK_e, 0);
  keyboard_keypress(XK_f, 0);
  keyboard_keypress(XK_n, 0);
  check_keyports(expected_keyports);
}

static void
test_keyboard_keypress_symbol_on_physical_keyboard()
{
  unsigned char expected_keyports[8] = {
    0xfd, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xf7
  };

  keyboard_init(non_ace_key_handler);
  keyboard_keypress(XK_asterisk, 0);
  check_keyports(expected_keyports);
}

static void
test_keyboard_keypress_key_not_found()
{
  unsigned char expected_keyports[8] = {
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff
  };

  keyboard_init(non_ace_key_handler);
  keyboard_keypress(XK_Sys_Req, 0);
  check_keyports(expected_keyports);
}

static void
test_keyboard_keypress_pass_to_non_ace_key_handler()
{
  unsigned char expected_keyports[8] = {
    0xfe, 0xfe, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff
  };

  non_ace_key_handler_init();
  keyboard_init(non_ace_key_handler);
  keyboard_keypress(XK_A, 5);
  assert(non_ace_key_handler_status.handler_called);
  assert(non_ace_key_handler_status.keySym == XK_A);
  assert(non_ace_key_handler_status.key_state == 5);

  check_keyports(expected_keyports);
}

int main()
{
  test_keyboard_clear();
  test_keyboard_keypress_single_key();
  test_keyboard_keypress_multiple_keys();
  test_keyboard_keypress_symbol_on_physical_keyboard();
  test_keyboard_keypress_key_not_found();
  test_keyboard_keypress_pass_to_non_ace_key_handler();
  exit(0);
}
