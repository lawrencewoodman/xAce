/* Declarations for handling the emulated keyboard
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
#include <X11/Xlib.h>

#ifndef KEYBOARD_H
#define KEYBOARD_H

typedef void (*NonAceKeyHandler)(KeySym ks, int key_state);

extern unsigned char keyboard_get_keyport(int port);
extern void keyboard_keyboard(void);
extern int keyboard_process_keypress_keyports(KeySym ks);
extern void keyboard_keypress(XKeyEvent *kev,
                              NonAceKeyHandler non_ace_key_handler);
extern void keyboard_keyrelease(XKeyEvent *kev);

#endif
