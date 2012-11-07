/* Emulation of the keyboard
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
 * --------------------------------------------------------------------------
 *
 * For information on how the Ace's keyboard works look at:
 *   http://www.jupiter-ace.co.uk/prog_keyboardread.html
 *
 * Certain compromises had to be made in the emulation of the keyboard.
 * It is not possible to detect the shift key separately for example,
 * because that would mean that it wouldn't be possible to use the symbols
 * on the physical keyboard such as $ or ^, as on the Ace these are accessed
 * via the symbol symbol shift key not the letter shift.  However, I don't
 * think this should be an issue very often and in turn it creates a much
 * nicer experience for the user of the emulator.
 */
#include <stdio.h>
#include <X11/keysym.h>

#include "keyboard.h"

static unsigned char keyboard_ports[8] = {
  0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff
};


/* key, keyport_index, and_value, keyport_index, and_value
 * if keyport_index == -1 then no action for that port */
static const int keypress_response[] = {
  XK_F1, 3, 0xfe, 0, 0xfe,         /* Delete line */
  XK_F4, 3, 0xf7, 0, 0xfe,         /* Inverse video */
  XK_F9, 4, 0xfd, 0, 0xfe,         /* Graphics */
  XK_Left, 3, 0xef, 0, 0xfe,
  XK_Down, 4, 0xf7, 0, 0xfe,
  XK_Up, 4, 0xef, 0, 0xfe,
  XK_Right, 4, 0xfb, 0, 0xfe,
  XK_BackSpace, 0, 0xfe, 4, 0xfe,
  XK_Delete, 0, 0xfe, 4, 0xfe,
  XK_1, 3, 0xfe, -1, 0,
  XK_2, 3, 0xfd, -1, 0,
  XK_3, 3, 0xfb, -1, 0,
  XK_4, 3, 0xf7, -1, 0,
  XK_5, 3, 0xef, -1, 0,
  XK_6, 4, 0xef, -1, 0,
  XK_7, 4, 0xf7, -1, 0,
  XK_8, 4, 0xfb, -1, 0,
  XK_9, 4, 0xfd, -1, 0,
  XK_0, 4, 0xfe, -1, 0,
  XK_exclam, 3, 0xfe, 0, 0xfd,
  XK_at, 3, 0xfd, 0, 0xfd,
  XK_numbersign, 3, 0xfb, 0, 0xfd,
  XK_dollar, 3, 0xf7, 0, 0xfd,
  XK_percent, 3, 0xef, 0, 0xfd,
  XK_Escape, 7, 0xfe, 0, 0xfe,     /* Break */
  XK_ampersand, 4, 0xef, 0, 0xfd,
  XK_apostrophe, 4, 0xf7, 0, 0xfd,
  XK_parenleft, 4, 0xfb, 0, 0xfd,
  XK_parenright, 4, 0xfd, 0, 0xfd,
  XK_underscore, 4, 0xfe, 0, 0xfd,
  XK_A, 0, 0xfe, 1, 0xfe,
  XK_a, 1, 0xfe, -1, 0,
  XK_B, 0, 0xfe, 7, 0xf7,
  XK_b, 7, 0xf7, -1, 0,
  XK_C, 0, 0xee, -1, 0,
  XK_c, 0, 0xef, -1, 0,
  XK_D, 0, 0xfe, 1, 0xfb,
  XK_d, 1, 0xfb, -1, 0,
  XK_E, 0, 0xfe, 2, 0xfb,
  XK_e, 2, 0xfb, -1, 0,
  XK_F, 0, 0xfe, 1, 0xf7,
  XK_f, 1, 0xf7, -1, 0,
  XK_G, 0, 0xfe, 1, 0xef,
  XK_g, 1, 0xef, -1, 0,
  XK_H, 0, 0xfe, 6, 0xef,
  XK_h, 6, 0xef, -1, 0,
  XK_I, 0, 0xfe, 5, 0xfb,
  XK_i, 5, 0xfb, -1, 0,
  XK_J, 0, 0xfe, 6, 0xf7,
  XK_j, 6, 0xf7, -1, 0,
  XK_K, 0, 0xfe, 6, 0xfb,
  XK_k, 6, 0xfb, -1, 0,
  XK_L, 0, 0xfe, 6, 0xfd,
  XK_l, 6, 0xfd, -1, 0,
  XK_M, 0, 0xfe, 7, 0xfd,
  XK_m, 7, 0xfd, -1, 0,
  XK_N, 0, 0xfe, 7, 0xfb,
  XK_n, 7, 0xfb, -1, 0,
  XK_O, 0, 0xfe, 5, 0xfd,
  XK_o, 5, 0xfd, -1, 0,
  XK_P, 0, 0xfe, 5, 0xfe,
  XK_p, 5, 0xfe, -1, 0,
  XK_Q, 0, 0xfe, 2, 0xfe,
  XK_q, 2, 0xfe, -1, 0,
  XK_R, 0, 0xfe, 2, 0xf7,
  XK_r, 2, 0xf7, -1, 0,
  XK_S, 0, 0xfe, 1, 0xfd,
  XK_s, 1, 0xfd, -1, 0,
  XK_T, 0, 0xfe, 2, 0xef,
  XK_t, 2, 0xef, -1, 0,
  XK_U, 0, 0xfe, 5, 0xf7,
  XK_u, 5, 0xf7, -1, 0,
  XK_V, 0, 0xfe, 7, 0xef,
  XK_v, 7, 0xef, -1, 0,
  XK_W, 0, 0xfe, 2, 0xfd,
  XK_w, 2, 0xfd, -1, 0,
  XK_X, 0, 0xf6, -1, 0,
  XK_x, 0, 0xf7, -1, 0,
  XK_Y, 0, 0xfe, 5, 0xef,
  XK_y, 5, 0xef, -1, 0,
  XK_Z, 0, 0xfa, -1, 0,
  XK_z, 0, 0xfb, -1, 0,
  XK_less, 2, 0xf7, 0, 0xfd,
  XK_greater, 2, 0xef, 0, 0xfd,
  XK_bracketleft, 5, 0xef, 0, 0xfd,
  XK_bracketright, 5, 0xf7, 0, 0xfd,
  /* FIX: Copyright symbol: 5, 0xfb, 0, 0xfd*/
  XK_semicolon, 5, 0xfd, 0, 0xfd,
  XK_quotedbl, 5, 0xfe, 0, 0xfd,
  XK_asciitilde, 1, 0xfe, 0, 0xfd,
  XK_bar, 1, 0xfd, 0, 0xfd,
  XK_backslash, 1, 0xfb, 0, 0xfd,
  XK_braceleft, 1, 0xf7, 0, 0xfd,
  XK_braceright, 1, 0xef, 0, 0xfd,
  XK_asciicircum, 6, 0xef, 0, 0xfd,
  XK_minus, 6, 0xf7, 0, 0xfd,
  XK_plus, 6, 0xfb, 0, 0xfd,
  XK_equal, 6, 0xfd, 0, 0xfd,
  '\n', 6, 0xfe, -1, 0,
  XK_Return, 6, 0xfe, -1, 0,
  XK_colon, 0, 0xf9, -1, 0,
  XK_sterling, 0, 0xf5, -1, 0,
  XK_question, 0, 0xed, -1, 0,
  XK_slash, 7, 0xef, 0, 0xfd,
  XK_asterisk, 7, 0xf7, 0, 0xfd,
  XK_comma, 7, 0xfb, 0, 0xfd,
  XK_period, 7, 0xfd, 0, 0xfd,
  XK_space, 7, 0xfe, -1, 0,
  XK_Tab, 7, 0xfe, -1, 0,
  '\t', 7, 0xfe, -1, 0
};

unsigned char
keyboard_get_keyport(int port)
{
  return keyboard_ports[port];
}

void
keyboard_clear(void)
{
  int i;
  for (i = 0; i < 8; i++)
    keyboard_ports[i] = 0xff;
}

static int
keyboard_get_key_response(KeySym ks, int *keyport1, int *keyport2,
                          int *keyport1_response, int *keyport2_response)
{
  int i;
  int num_keys = sizeof(keypress_response)/sizeof(keypress_response[0]);

  for (i = 0; i < num_keys; i+= 5) {
    if (keypress_response[i] == ks) {
      *keyport1 = keypress_response[i+1];
      *keyport2 = keypress_response[i+3];
      *keyport1_response = keypress_response[i+2];
      *keyport2_response = keypress_response[i+4];
      return 1;
    }
  }
  return 0;
}

int
keyboard_process_keypress_keyports(KeySym ks)
{
  int key_found;
  int keyport1, keyport2;
  int keyport1_and_value, keyport2_and_value;

  key_found = keyboard_get_key_response(ks, &keyport1, &keyport2,
                &keyport1_and_value, &keyport2_and_value);
  if (key_found) {
    keyboard_ports[keyport1] &= keyport1_and_value;
    if (keyport2 != -1)
      keyboard_ports[keyport2] &= keyport2_and_value;
  }
  return key_found;
}

int
keyboard_process_keyrelease_keyports(KeySym ks)
{
  int key_found;
  int keyport1, keyport2;
  int keyport1_or_value, keyport2_or_value;

  key_found = keyboard_get_key_response(ks, &keyport1, &keyport2,
                &keyport1_or_value, &keyport2_or_value);
  if (key_found) {
    keyboard_ports[keyport1] |= ~keyport1_or_value;
    if (keyport2 != -1)
      keyboard_ports[keyport2] |= ~keyport2_or_value;
  }
  return key_found;
}


void
keyboard_keypress(XKeyEvent *kev, NonAceKeyHandler non_ace_key_handler)
{
  char buf[20];
  KeySym ks;

  XLookupString(kev, buf, 20, &ks, NULL);
  keyboard_process_keypress_keyports(ks);
  non_ace_key_handler(ks, kev->state);
}


void
keyboard_keyrelease(XKeyEvent *kev)
{
  char buf[20];
  KeySym ks;

  XLookupString(kev, buf, 20, &ks, NULL);
  if (!keyboard_process_keyrelease_keyports(ks)) {
    keyboard_clear();
  }
}
