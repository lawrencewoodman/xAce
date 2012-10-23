/* xace, an X-based Jupiter ACE emulator (based on xz81)
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

#include <limits.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "z80.h"
#include "tape.h"

#define MAX_DISP_LEN 256

#include "xace_icon.h"

unsigned char keyports[8] = {
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff,
  0xff
};


/* key, keyport_index, and_value, keyport_index, and_value
 * if keyport_index == -1 then no action for that port */
const int keypress_response[] = {
  XK_Escape, 7, 0xfe, 0, 0xfe,     /* Break */
  '\n', 6, 0xfe, -1, 0,
  XK_Return, 6, 0xfe, -1, 0,
  XK_F1, 3, 0xfe, 0, 0xfe,         /* Delete line */
  XK_F4, 3, 0xf7, 0, 0xfe,         /* Inverse video */
  XK_F9, 4, 0xfd, 0, 0xfe,         /* Graphics */
  XK_BackSpace, 0, 0xfe, 4, 0xfe,
  XK_Delete, 0, 0xfe, 4, 0xfe,
  XK_Up, 4, 0xef, 0, 0xfe,
  XK_Down, 4, 0xf7, 0, 0xfe,
  XK_Left, 3, 0xef, 0, 0xfe,
  XK_Right, 4, 0xfb, 0, 0xfe,
  XK_space, 7, 0xfe, -1, 0,
  XK_exclam, 3, 0xfe, 0, 0xfd,
  XK_quotedbl, 5, 0xfe, 0, 0xfd,
  XK_numbersign, 3, 0xfb, 0, 0xfd,
  XK_dollar, 3, 0xf7, 0, 0xfd,
  XK_percent, 3, 0xef, 0, 0xfd,
  XK_ampersand, 4, 0xef, 0, 0xfd,
  XK_apostrophe, 4, 0xf7, 0, 0xfd,
  XK_parenleft, 4, 0xfb, 0, 0xfd,
  XK_parenright, 4, 0xfd, 0, 0xfd,
  XK_asterisk, 7, 0xf7, 0, 0xfd,
  XK_plus, 6, 0xfb, 0, 0xfd,
  XK_comma, 7, 0xfb, 0, 0xfd,
  XK_minus, 6, 0xf7, 0, 0xfd,
  XK_period, 7, 0xfd, 0, 0xfd,
  XK_slash, 7, 0xef, 0, 0xfd,
  XK_0, 4, 0xfe, -1, 0,
  XK_1, 3, 0xfe, -1, 0,
  XK_2, 3, 0xfd, -1, 0,
  XK_3, 3, 0xfb, -1, 0,
  XK_4, 3, 0xf7, -1, 0,
  XK_5, 3, 0xef, -1, 0,
  XK_6, 4, 0xef, -1, 0,
  XK_7, 4, 0xf7, -1, 0,
  XK_8, 4, 0xfb, -1, 0,
  XK_9, 4, 0xfd, -1, 0,
  XK_colon, 0, 0xfb, 0, 0xfd,
  XK_semicolon, 5, 0xfd, 0, 0xfd,
  XK_less, 2, 0xf7, 0, 0xfd,
  XK_equal, 6, 0xfd, 0, 0xfd,
  XK_greater, 2, 0xef, 0, 0xfd,
  XK_question, 0, 0xef, 0, 0xfd,
  XK_at, 3, 0xfd, 0, 0xfd,
  XK_A, 0, 0xfe, 1, 0xfe,
  XK_a, 1, 0xfe, -1, 0,
  XK_B, 0, 0xfe, 7, 0xf7,
  XK_b, 7, 0xf7, -1, 0,
  XK_C, 0, 0xfe, 0, 0xef,
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
  XK_X, 0, 0xfe, 0, 0xf7,
  XK_x, 0, 0xf7, -1, 0,
  XK_Y, 0, 0xfe, 5, 0xef,
  XK_y, 5, 0xef, -1, 0,
  XK_Z, 0, 0xfe, 0, 0xfb,
  XK_z, 0, 0xfb, -1, 0,
  XK_bracketleft, 5, 0xef, 0, 0xfd,
  XK_backslash, 1, 0xfb, 0, 0xfd,
  XK_bracketright, 5, 0xf7, 0, 0xfd,
  XK_asciicircum, 6, 0xef, 0, 0xfd,
  XK_underscore, 4, 0xfe, 0, 0xfd,
  XK_grave, 5, 0xfb, 0, 0xfd,
  XK_braceleft, 1, 0xf7, 0, 0xfd,
  XK_bar, 1, 0xfd, 0, 0xfd,
  XK_braceright, 1, 0xef, 0, 0xfd,
  XK_asciitilde, 1, 0xfe, 0, 0xfd
};

/* key, keyport_index, or_notvalue, keyport_index, or_notvalue
 * if keyport_index == -1 then no action for that port */
const int keyrelease_response[] = {
  XK_F4, 3, 0xf7, 0, 0xfe,
  XK_F9, 4, 0xfd, 0, 0xfe,
  XK_Control_L, 7, 0xfe, 0, 0xfe,
  XK_Control_R, 7, 0xfe, 0, 0xfe,
  XK_F1, 3, 0xfe, 0, 0xfe,
  XK_Up, 4, 0xef, 0, 0xfe,
  XK_Down, 4, 0xf7, 0, 0xfe,
  XK_Left, 3, 0xef, 0, 0xfe,
  XK_Right, 4, 0xfb, 0, 0xfe,
  XK_space, 7, 0xfe, -1, 0,
  XK_exclam, 3, 0xfe, 0, 0xfd,
  XK_quotedbl, 5, 0xfe, 0, 0xfd,
  XK_numbersign, 3, 0xfb, 0, 0xfd,
  XK_dollar, 3, 0xf7, 0, 0xfd,
  XK_percent, 3, 0xef, 0, 0xfd,
  XK_ampersand, 4, 0xef, 0, 0xfd,
  XK_apostrophe, 4, 0xf7, 0, 0xfd,
  XK_parenleft, 4, 0xfb, 0, 0xfd,
  XK_parenright, 4, 0xfd, 0, 0xfd,
  XK_asterisk, 7, 0xf7, 0, 0xfd,
  XK_plus, 6, 0xfb, 0, 0xfd,
  XK_comma, 7, 0xfb, 0, 0xfd,
  XK_minus, 6, 0xf7, 0, 0xfd,
  XK_period, 7, 0xfd, 0, 0xfd,
  XK_slash, 7, 0xef, 0, 0xfd,
  XK_0, 4, 0xfe, -1, 0,
  XK_1, 3, 0xfe, -1, 0,
  XK_2, 3, 0xfd, -1, 0,
  XK_3, 3, 0xfb, -1, 0,
  XK_4, 3, 0xf7, -1, 0,
  XK_5, 3, 0xef, -1, 0,
  XK_6, 4, 0xef, -1, 0,
  XK_7, 4, 0xf7, -1, 0,
  XK_8, 4, 0xfb, -1, 0,
  XK_9, 4, 0xfd, -1, 0,
  XK_colon, 0, 0xfb, 0, 0xfd,
  XK_semicolon, 5, 0xfd, 0, 0xfd,
  XK_less, 2, 0xf7, 0, 0xfd,
  XK_equal, 6, 0xfd, 0, 0xfd,
  XK_greater, 2, 0xef, 0, 0xfd,
  XK_question, 0, 0xef, 0, 0xfd,
  XK_at, 3, 0xfd, 0, 0xfd,
  XK_A, 0, 0xfe, 1, 0xfe,
  XK_a, 1, 0xfe, -1, 0,
  XK_B, 0, 0xfe, 7, 0xf7,
  XK_b, 7, 0xf7, -1, 0,
  XK_C, 0, 0xfe, 0, 0xef,
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
  XK_X, 0, 0xfe, 0, 0xf7,
  XK_x, 0, 0xf7, -1, 0,
  XK_Y, 0, 0xfe, 5, 0xef,
  XK_y, 5, 0xef, -1, 0,
  XK_Z, 0, 0xfe, 0, 0xfb,
  XK_z, 0, 0xfb, -1, 0,
  XK_bracketleft, 5, 0xef, 0, 0xfd,
  XK_backslash, 1, 0xfb, 0, 0xfd,
  XK_bracketright, 5, 0xf7, 0, 0xfd,
  XK_asciicircum, 6, 0xef, 0, 0xfd,
  XK_underscore, 4, 0xfe, 0, 0xfd,
  XK_grave, 5, 0xfb, 0, 0xfd,
  XK_braceleft, 1, 0xf7, 0, 0xfd,
  XK_bar, 1, 0xfd, 0, 0xfd,
  XK_braceright, 1, 0xef, 0, 0xfd,
  XK_asciitilde, 1, 0xfe, 0, 0xfd,
};


#define BORDER_WIDTH  (20*SCALE)
int rrnoshm=4;
unsigned char mem[65536];
unsigned char *memptr[8] = {
  mem,
  mem+0x2000,
  mem+0x4000,
  mem+0x6000,
  mem+0x8000,
  mem+0xa000,
  mem+0xc000,
  mem+0xe000
};

unsigned long tstates=0,tsmax=62500;

int memattr[8]={0,1,1,1,1,1,1,1}; /* 8K RAM Banks */

int hsize=256*SCALE,vsize=192*SCALE;
volatile int interrupted=0;
int scrn_freq=2;

/* Used to see if image needs refreshing on X display */
unsigned char video_ram_old[24*32];

int refresh_screen=1;

/* The following are used for spooling */
static FILE *spoolFile=NULL;
static int flipFlop;

/* Prototypes */
void clear_keyboard(void);
void loadrom(unsigned char *x);
void startup(int *argc, char **argv);
void process_keypress_keyports(KeySym ks);
void process_keypress(XKeyEvent *kev);
void check_events(void);
void refresh(void);
void closedown(void);

void
sighandler(int a)
{
  if(interrupted<2) interrupted=1;
}


void
dontpanic(int signum)
{
  tape_detach();
  closedown();
  exit(1);
}

static void
normal_speed(void)
{
  scrn_freq = 4;
  tsmax = 62500;
}

void
fast_speed(void)
{
  scrn_freq = 2;
  tsmax = ULONG_MAX;
}


static void
tape_observer(int tape_attached, int tape_pos,
  const char tape_filename[TAPE_MAX_FILENAME_SIZE],
  MessageType message_type, const char message[TAPE_MAX_MESSAGE_SIZE])
{
  switch (message_type) {
    case NO_MESSAGE:
      if (tape_attached)
        printf("TAPE: %s Pos: %04d\n", tape_filename, tape_pos);
      break;

    case MESSAGE:
      if (tape_attached)
        printf("TAPE: %s Pos: %04d - %s\n", tape_filename, tape_pos, message);
      else
        printf("TAPE: empty tape Pos: %04d - %s\n", tape_pos, message);
      break;

    case ERROR:
      if (tape_attached) {
        fprintf(stderr, "TAPE: %s Pos: %04d - Error: %s\n",
                tape_filename, tape_pos, message);
      } else {
        fprintf(stderr, "TAPE: empty tape Pos: %04d - Error: %s\n",
                tape_pos, message);
      }
      break;
  }
}

void
handle_cli_args(int argc, char **argv)
{
  int arg_pos = 0;
  char *cli_switch;
  char *spool_filename;

  while (arg_pos < argc) {
    cli_switch = argv[arg_pos];
    if (strcasecmp("-s", cli_switch) == 0) {
      if (strcmp("-S", cli_switch) == 0) {
        fast_speed();
      }

      if (++arg_pos < argc) {
        spool_filename = argv[arg_pos];
        spoolFile=fopen(spool_filename, "rt");
        if (!spoolFile)
          fprintf(stderr, "Error: Couldn't open file: %s\n", spool_filename);
        flipFlop=2;
      } else {
        fprintf(stderr, "Error: Missing filename for %s arg\n", cli_switch);
      }
    }
    arg_pos++;
  }
}

void
main(int argc, char **argv)
{
  struct sigaction sa;
  struct itimerval itv;
  int tmp=1000/50;  /* 50 ints/sec */

  printf("xace: Jupiter ACE emulator v%s (by Edward Patel)\n", XACE_VERSION);
  printf("Keys:\n");
  printf("\tF1     - Delete Line\n");
  printf("\tF3     - Attach a tape image\n");
  printf("\tF4     - Inverse Video\n");
  printf("\tF9     - Graphics\n");
  printf("\tF11    - Spool from a file\n");
  printf("\tF12    - Reset\n");
  printf("\tEsc    - Break\n");
  printf("\tCtrl-Q - Quit xAce\n");

  loadrom(mem);
  tape_patches(mem);
  memset(mem+8192,0xff,57344);
  memset(video_ram_old,0xff,768);

  startup(&argc,argv);
  handle_cli_args(argc, argv);

  memset(&sa,0,sizeof(sa));

  sa.sa_handler=sighandler;
  sa.sa_flags=SA_RESTART;

  sigaction(SIGALRM,&sa,NULL);

  sa.sa_handler=dontpanic;
  sa.sa_flags=0;

  sigaction(SIGINT, &sa,NULL);
  sigaction(SIGHUP, &sa,NULL);
  sigaction(SIGILL, &sa,NULL);
  sigaction(SIGTERM,&sa,NULL);
  sigaction(SIGQUIT,&sa,NULL);
  sigaction(SIGSEGV,&sa,NULL);

  itv.it_interval.tv_sec=tmp/1000;
  itv.it_interval.tv_usec=(tmp%1000)*1000;
  itv.it_value.tv_sec=itv.it_interval.tv_sec;
  itv.it_value.tv_usec=itv.it_interval.tv_usec;
  setitimer(ITIMER_REAL,&itv,NULL);

  tape_add_observer(tape_observer);
  mainloop();
}

void
loadrom(unsigned char *x)
{
  FILE *in;

  if((in=fopen("ace.rom", "rb"))!=NULL)
  {
    if (fread(x,1,8192,in) != 8192) {
      printf("Couldn't load ROM.\n");
      fclose(in);
      exit(1);
    }
    fclose(in);
  }
  else
  {
    printf("Couldn't load ROM.\n");
    exit(1);
  }
}


unsigned int
in(int h, int l)
{
  if(l==0xfe) /* keyboard */
    switch(h)
    {
      case 0xfe: return(keyports[0]);
      case 0xfd: return(keyports[1]);
      case 0xfb: return(keyports[2]);
      case 0xf7: return(keyports[3]);
      case 0xef: return(keyports[4]);
      case 0xdf: return(keyports[5]);
      case 0xbf: return(keyports[6]);
      case 0x7f: return(keyports[7]);
      default:  return(255);
    }
  return(255);
}


unsigned int
out(int h, int l, int a)
{
  return(0);
}


void
fix_tstates(void)
{
  tstates=0;
  pause();
}

void
readch_from_spool_file(void)
{
  KeySym ks;

  if ((ks=fgetc(spoolFile))) {
    if (ks == EOF) {
      fclose(spoolFile);
      spoolFile = NULL;
      normal_speed();
    } else {
      process_keypress_keyports(ks);
    }
  }
}

void
read_from_spool_file(void)
{
  if (!spoolFile) {return;}

  if (flipFlop == 1) {
    readch_from_spool_file();
  } else if (flipFlop == 3) {
    flipFlop = 0;
    clear_keyboard();
  }
  flipFlop++;
}

void
do_interrupt(void)
{
  static int count=0;

  /* only do refresh() every 1/Nth */
  count++;
  if (count >= scrn_freq) {
    count=0;
    read_from_spool_file();
    refresh();
  }

  check_events();

  /* be careful not to screw up any pending reset... */

  if (interrupted == 1)
    interrupted = 0;
}

/* the remainder of xmain.c is based on xz80's xspectrum.c. */
static Display *display;
static Screen *scrptr;
static int screen;
static Window root;
static Colormap cmap;
static GC maingc;
static GC fggc, bggc;
static Window borderwin, mainwin;
static XImage *ximage;
static unsigned char *image;
static int linelen;
static int bytes_per_pixel;
static int black,white;
static int invert=0;
static int borderchange=1;

static Display *
open_display(int *argc, char **argv)
{
  char *ptr;

  char dispname[MAX_DISP_LEN];
  Display *display;

  if((ptr=getenv("DISPLAY")))
    strcpy(dispname,ptr);
  else
    strcpy(dispname,":0.0");

  if(!(display=XOpenDisplay(dispname))){
    fprintf(stderr,"Unable to open display %s\n",dispname);
    exit(1);
  }

  return display;
}


static int image_init()
{
  ximage=XCreateImage(display,DefaultVisual(display,screen),
         DefaultDepth(display,screen),ZPixmap,0,NULL,hsize,vsize,
         8,0);
  if(!ximage){
    perror("XCreateImage failed");
    return 1;
  }
  ximage->data=malloc(ximage->bytes_per_line*(ximage->height+1));
  if(!ximage->data){
    perror("Couldn't get memory for XImage data");
    return 1;
  }
  scrn_freq=rrnoshm;
  linelen=ximage->bytes_per_line/SCALE;
  bytes_per_pixel = linelen / 256;

  /* The following represent 4, 8, 16 or 32 bpp repectively */
  if(linelen!=32 && linelen!=256 && linelen!=512 && linelen!=1024)
    fprintf(stderr,"Line length=%d; expect strange results!\n",linelen);

  image=ximage->data;
  return 0;
}


static void
notify(int *argc, char **argv)
{
  Pixmap icon;
  XWMHints xwmh;
  XSizeHints xsh;
  XClassHint xch;
  XTextProperty appname, iconname;
  char *apptext = "xAce";
  char *icontext = apptext;

#ifdef WHITE_ON_BLACK
  icon=XCreatePixmapFromBitmapData(display,root,icon_bits,
       icon_width,icon_height,white,black,DefaultDepth(display,screen));
#else
  icon=XCreatePixmapFromBitmapData(display,root,icon_bits,
       icon_width,icon_height,black,white,DefaultDepth(display,screen));
#endif

  xsh.flags=PSize|PMinSize|PMaxSize;
  xsh.min_width=hsize;
  xsh.min_height=vsize;
  xsh.max_width=hsize+BORDER_WIDTH*2;
  xsh.max_height=vsize+BORDER_WIDTH*2;
  if(!XStringListToTextProperty(&apptext,1,&appname)){
    fputs("Can't create a TextProperty!",stderr);
    return;
  }
  if(!XStringListToTextProperty(&icontext,1,&iconname)){
    fputs("Can't create a TextProperty!",stderr);
    return;
  }
  xwmh.initial_state=NormalState;
  xwmh.input=1;
  xwmh.icon_pixmap=icon;
  xwmh.flags=StateHint|IconPixmapHint|InputHint;
  xch.res_name = apptext;
  xch.res_class = apptext;
  XSetWMProperties(display,borderwin,&appname,&iconname,argv,
      *argc,&xsh,&xwmh,&xch);

  XFree(appname.value);
  XFree(iconname.value);
}


void
startup(int *argc, char **argv)
{
  display=open_display(argc,argv);
  if(!display){
    fputs("Failed to open X display\n",stderr);
    exit(1);
  }
  invert=0;
  screen=DefaultScreen(display);
  scrptr=DefaultScreenOfDisplay(display);
  root=DefaultRootWindow(display);
#ifdef WHITE_ON_BLACK
  black=WhitePixel(display,screen);
  white=BlackPixel(display,screen);
#else
  white=WhitePixel(display,screen);
  black=BlackPixel(display,screen);
#endif /* WHITE_ON_BLACK */
  maingc=XCreateGC(display,root,0,NULL);
  XCopyGC(display,DefaultGC(display,screen),~0,maingc);
  XSetGraphicsExposures(display,maingc,0);
  fggc=XCreateGC(display,root,0,NULL);
  XCopyGC(display,DefaultGC(display,screen),~0,fggc);
  XSetGraphicsExposures(display,fggc,0);
  bggc=XCreateGC(display,root,0,NULL);
  XCopyGC(display,DefaultGC(display,screen),~0,bggc);
  XSetGraphicsExposures(display,bggc,0);
  cmap=DefaultColormap(display,screen);
  if(image_init()) {
    exit(1);
  }

  borderwin=XCreateSimpleWindow(display,root,0,0,
            hsize+BORDER_WIDTH*2,vsize+BORDER_WIDTH*2,0,0,0);
  mainwin=XCreateSimpleWindow(display,borderwin,BORDER_WIDTH,
            BORDER_WIDTH,hsize,vsize,0,0,0);
  notify(argc,argv);
  XSelectInput(display,borderwin,KeyPressMask|KeyReleaseMask|
     ExposureMask|EnterWindowMask|LeaveWindowMask|
     StructureNotifyMask);
  XMapRaised(display,borderwin);
  XMapRaised(display,mainwin);
  XFlush(display);

  refresh_screen=1;
}

void
clear_keyboard(void)
{
  keyports[0]=0xff;
  keyports[1]=0xff;
  keyports[2]=0xff;
  keyports[3]=0xff;
  keyports[4]=0xff;
  keyports[5]=0xff;
  keyports[6]=0xff;
  keyports[7]=0xff;
}

void
process_keypress_keyports(KeySym ks)
{
  int i;
  int keyport1, keyport2;
  int keyport1_and_value, keyport2_and_value;

  for (i = 0; i < sizeof(keypress_response); i+= 5) {
    if (keypress_response[i] == ks) {
      keyport1 = keypress_response[i+1];
      keyport2 = keypress_response[i+3];
      keyport1_and_value = keypress_response[i+2];
      keyport2_and_value = keypress_response[i+4];

      keyports[keyport1] &= keyport1_and_value;
      if (keyport2 != -1)
        keyports[keyport2] &= keyport2_and_value;
      break;
    }
  }
}

int
process_keyrelease_keyports(KeySym ks)
{
  int i;
  int keyfound = 0;
  int keyport1, keyport2;
  int keyport1_or_value, keyport2_or_value;

  for (i = 0; i < sizeof(keyrelease_response); i+= 5) {
    if (keyrelease_response[i] == ks) {
      keyfound = 1;
      keyport1 = keyrelease_response[i+1];
      keyport2 = keyrelease_response[i+3];
      keyport1_or_value = keyrelease_response[i+2];
      keyport2_or_value = keyrelease_response[i+4];

      keyports[keyport1] |= ~keyport1_or_value;
      if (keyport2 != -1)
        keyports[keyport2] |= ~keyport2_or_value;
      break;
    }
  }
  return keyfound;
}

void
process_keypress_emu_commands(KeySym ks, XKeyEvent *kev)
{
  char spool_filename[257];
  char tape_filename[257];

  switch (ks) {
    case XK_q:
      /* If Ctrl-q then Quit xAce */
      if (spoolFile == NULL && kev->state & ControlMask) {
        dontpanic(SIGQUIT);
        /* doesn't return */
      }
      break;

    case XK_F3:
      printf("Enter tape image file:");
      scanf("%256s", tape_filename);
      tape_attach(tape_filename);
      break;

    case XK_F11:
      printf("Enter spool file:");
      scanf("%256s", spool_filename);
      spoolFile = fopen(spool_filename, "rt");
      if (spoolFile) {
        flipFlop=2;
      } {
        fprintf(stderr, "Error: Couldn't open file: %s\n", spool_filename);
      }
      break;

    case XK_F12:
      interrupted = 2; /* will cause a reset */
      memset(mem+8192, 0xff, 57344);
      refresh_screen = 1;
      clear_keyboard();
      break;
  }
}

void
process_keypress(XKeyEvent *kev)
{
  char buf[3];
  KeySym ks;

  XLookupString(kev, buf, 2, &ks, NULL);
  process_keypress_emu_commands(ks, kev);
  process_keypress_keyports(ks);
}


void
process_keyrelease(XKeyEvent *kev)
{
  char buf[3];
  KeySym ks;

  XLookupString(kev,buf,2,&ks,NULL);

  if (!process_keyrelease_keyports(ks)) {
    /* key not in keyrelease_response list */
    switch (ks) {
      case XK_Return:
        keyports[6] |= 1;
        break;

      case XK_BackSpace:
      case XK_Delete:
        keyports[0] |= 1;
        keyports[4] |= 1;
        break;

      default:
        clear_keyboard();
    }
  }
}

void
check_events(void)
{
  static XEvent xev;
  XConfigureEvent *conf_ev;
  XCrossingEvent *cev;

  while (XEventsQueued(display,QueuedAfterReading)){
    XNextEvent(display,&xev);
    switch(xev.type){
      case Expose: refresh_screen=1;
        break;
      case ConfigureNotify:
        conf_ev=(XConfigureEvent *)&xev;
        XMoveWindow(display,mainwin,(conf_ev->width-hsize)/2,
                    (conf_ev->height-vsize)/2);
        break;
      case MapNotify:
      case UnmapNotify:
      case ReparentNotify:
        break;
      case EnterNotify:
        cev=(XCrossingEvent *)&xev;
        if(cev->detail!=NotifyInferior)
          XAutoRepeatOff(display),XFlush(display);
        break;
      case LeaveNotify:
        cev=(XCrossingEvent *)&xev;
        if(cev->detail!=NotifyInferior)
          XAutoRepeatOn(display),XFlush(display);
        break;
      case KeyPress:
        process_keypress((XKeyEvent *)&xev);
        break;
      case KeyRelease:
        process_keyrelease((XKeyEvent *)&xev);
        break;
      default:
        fprintf(stderr,"unhandled X event, type %d\n",xev.type);
    }
  }
}

/* Set a pixel in the image */
void
set_pixel(int x, int y, int colour)
{
  int sx, sy;
  int imageIndex = (y*hsize+x)*SCALE*bytes_per_pixel;

  for(sy = 0; sy < SCALE; sy++) {
    for(sx = 0; sx < SCALE*bytes_per_pixel; sx++) {
      image[imageIndex+(sy*hsize*bytes_per_pixel+sx)] = colour;
    }
  }
}

/* Set a character in the image
 * x              Column in image to draw character
 * y              Row in image to draw character
 * inv            Whether an inverted character
 * charbmap       Ptr to the character bit map
 */
void
set_image_character(int x, int y, int inv, unsigned char *charbmap)
{
  int colour;
  int charbmap_x, charbmap_y;
  unsigned char charbmap_row;
  unsigned char charbmap_row_mask;

  for (charbmap_y = 0; charbmap_y < 8; charbmap_y++) {
    charbmap_row = charbmap[charbmap_y];
    if (inv) charbmap_row ^= 255;

    if (linelen == 32) {
      /* 1-bit mono */
      /* doesn't support SCALE>1 */
      image[(y*8+charbmap_y)*linelen+x]=~charbmap_row;
    } else {
      charbmap_row_mask = 128;
      for (charbmap_x = 0; charbmap_x < 8; charbmap_x++) {
        colour = (charbmap_row & charbmap_row_mask) ? black : white;
        set_pixel(x*8+charbmap_x, y*8+charbmap_y, colour);
        charbmap_row_mask >>= 1;
      }
    }
  }
}


/* To redraw the screen, we translate it into a char. map, then find
 * the smallest rectangle which covers all the changes, and update that.
 */
void
refresh(void)
{
  unsigned char *video_ram,*charset;
  int x,y,c,inv;
  int xmin,ymin,xmax,ymax;
  int video_ram_old_ofs;
  int chrmap_changed = 0;

  if (borderchange > 0) {
    /* FIX: what about expose events? need to set borderchange... */
    XSetWindowBackground(display,borderwin,white);
    XClearWindow(display,borderwin);
    XFlush(display);
    borderchange=0;
  }

  charset = mem+0x2c00;
  video_ram = mem+0x2400;
  xmin = 31; ymin = 23; xmax = 0; ymax = 0;

  /* since we can't just do "don't bother if it's junk" as we always
   * need to draw a screen, just draw *valid* junk that won't result
   * in a segfault or anything. :-)
   */
  if (video_ram-mem > 0xf000) video_ram = mem+0xf000;

  video_ram_old_ofs=0;
  for (y = 0; y < 24; y++) {
    for (x = 0; x < 32; x++, video_ram++, video_ram_old_ofs++) {
      c = *video_ram;

      if (c != video_ram_old[video_ram_old_ofs] || refresh_screen) {
        video_ram_old[video_ram_old_ofs] = c;

        /* update size of area to be drawn */
        if (x < xmin) xmin=x;
        if (y < ymin) ymin=y;
        if (x > xmax) xmax=x;
        if (y > ymax) ymax=y;

        inv = c&128;
        c &= 127;

        set_image_character(x, y, inv, charset+c*8);
      }
    }
  }

  if (refresh_screen)
    xmin=0; ymin=0; xmax=31; ymax=23;

  if (xmax >= xmin && ymax >= ymin) {
    XPutImage(display, mainwin, maingc, ximage,
              xmin*8*SCALE, ymin*8*SCALE, xmin*8*SCALE, ymin*8*SCALE,
              (xmax-xmin+1)*8*SCALE, (ymax-ymin+1)*8*SCALE);
    XFlush(display);
  }

  refresh_screen = 0;
}

void
closedown(void)
{
  tape_clear_observers();
  free(ximage->data);
  XAutoRepeatOn(display);
  XCloseDisplay(display);
}
