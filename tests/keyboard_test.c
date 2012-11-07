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

/* Function to create a keyboard event
 * press = 0 for KeyRelease else KeyPress
 */
XKeyEvent create_key_event(int keycode, int state, int press)
{
  XKeyEvent event;
  Display *display;
  Window winRoot;
  Window winFocus;
  int revert;

  display = XOpenDisplay(0);
  assert(display != NULL);

  winRoot = XDefaultRootWindow(display);
  XGetInputFocus(display, &winFocus, &revert);

  event.display     = display;
  event.window      = winFocus;
  event.root        = winRoot;
  event.subwindow   = None;
  event.time        = CurrentTime;
  event.x           = 1;
  event.y           = 1;
  event.x_root      = 1;
  event.y_root      = 1;
  event.same_screen = True;
  event.keycode     = XKeysymToKeycode(display, keycode);
  event.state       = state;
  event.type        = press ? KeyPress : KeyRelease;
  return event;
}

static void
test_keyboard_clear()
{
  unsigned char expected_keyports[8] = {
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff
  };

  keyboard_clear();
  keyboard_process_keypress_keyports(XK_3);
  keyboard_process_keypress_keyports(XK_7);
  keyboard_process_keypress_keyports(XK_u);
  keyboard_process_keypress_keyports(XK_e);
  keyboard_process_keypress_keyports(XK_f);
  keyboard_process_keypress_keyports(XK_l);
  keyboard_process_keypress_keyports(XK_n);
  keyboard_process_keypress_keyports(XK_z);
  keyboard_clear();

  check_keyports(expected_keyports);
}

static void
test_keyboard_process_keypress_keyports_single_key()
{
  int key_found;
  unsigned char expected_keyports[8] = {
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xfe
  };

  keyboard_clear();
  key_found = keyboard_process_keypress_keyports('\t');
  assert(key_found);
  check_keyports(expected_keyports);
}

static void
test_keyboard_process_keypress_keyports_multiple_keys()
{
  int key_found;
  unsigned char expected_keyports[8] = {
    0xff, 0xf7, 0xfb, 0xff,
    0xf7, 0xf7, 0xff, 0xfb
  };

  keyboard_clear();
  key_found = keyboard_process_keypress_keyports(XK_7);
  assert(key_found);

  key_found = keyboard_process_keypress_keyports(XK_u);
  assert(key_found);

  key_found = keyboard_process_keypress_keyports(XK_e);
  assert(key_found);

  key_found = keyboard_process_keypress_keyports(XK_f);
  assert(key_found);

  key_found = keyboard_process_keypress_keyports(XK_n);
  assert(key_found);

  check_keyports(expected_keyports);
}

static void
test_keyboard_process_keypress_keyports_symbol_on_physical_keyboard()
{
  int key_found;
  unsigned char expected_keyports[8] = {
    0xfd, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xf7
  };

  keyboard_clear();
  key_found = keyboard_process_keypress_keyports(XK_asterisk);
  assert(key_found);

  check_keyports(expected_keyports);
}

static void
test_keyboard_process_keypress_keyports_key_not_found()
{
  int key_found;
  unsigned char expected_keyports[8] = {
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff
  };

  keyboard_clear();
  key_found = keyboard_process_keypress_keyports(XK_Sys_Req);
  assert(!key_found);

  check_keyports(expected_keyports);
}

static void
test_keyboard_keypress_pass_to_non_ace_key_handler()
{
  XKeyEvent kev;

  unsigned char expected_keyports[8] = {
    0xfe, 0xfe, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff
  };

  non_ace_key_handler_init();
  keyboard_clear();
  kev = create_key_event(XK_A, 5, 1);
  keyboard_keypress(&kev, non_ace_key_handler);
  assert(non_ace_key_handler_status.handler_called);
  assert(non_ace_key_handler_status.keySym == XK_A);
  assert(non_ace_key_handler_status.key_state == 5);

  check_keyports(expected_keyports);
}

int main()
{
  test_keyboard_clear();
  test_keyboard_process_keypress_keyports_single_key();
  test_keyboard_process_keypress_keyports_multiple_keys();
  test_keyboard_process_keypress_keyports_symbol_on_physical_keyboard();
  test_keyboard_process_keypress_keyports_key_not_found();
  test_keyboard_keypress_pass_to_non_ace_key_handler();
  exit(0);
}
