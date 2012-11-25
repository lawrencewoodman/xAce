/* Declarations for the spooler controller
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

#ifndef SPOOLER_H
#define SPOOLER_H

typedef enum SpoolerMessage {
  SPOOLER_NO_MESSAGE,
  SPOOLER_OPENED,
  SPOOLER_CLOSED,
  SPOOLER_OPEN_ERROR
} SpoolerMessage;

typedef void (*SpoolerObserver)(SpoolerMessage message);
typedef void (*ClearKeyboardFunc)(void);
typedef void (*KeypressFunc)(KeySym ks, int key_state);

extern void spooler_init(SpoolerObserver spooler_observer_func,
                         ClearKeyboardFunc clear_keyboard_func,
                         KeypressFunc keypress_func);
extern void spooler_open(char *filename);
extern void spooler_close(void);
extern void spooler_read(void);
extern int spooler_active(void);

#endif
