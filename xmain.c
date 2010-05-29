/* xace, an X-based Jupiter ACE emulator (based on xz81)
 *
 * Copyright (C) 1994 Ian Collier. 
 * xz81 changes (C) 1995-6 Russell Marks.
 * xace changes (C) 1997 Edward Patel.
 * xace changes (C) 2010 Lawrence Woodman.
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
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#ifdef MITSHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif

#ifdef HAS_UNAME
#include <sys/utsname.h>
#endif

#include "z80.h"

#define MAX_DISP_LEN 256

/* #define SPOOLING_HOOK */

#include "xace.icon"

#if SCALE>1
static unsigned long scaleup[256]; /* to hold table of scaled up bytes,
                                 e.g. 00110100B -> 0000111100110000B */
#endif

unsigned char keyports[8]={0xff,
			   0xff,
			   0xff,
			   0xff, 
			   0xff,
			   0xff,
			   0xff,
			   0xff};

#define BORDER_WIDTH	(20*SCALE)
int rrshm=2,rrnoshm=4,mitshm=1;
unsigned char mem[65536];
unsigned char *memptr[8]={mem,
			  mem+0x2000,
			  mem+0x4000,
			  mem+0x6000,
			  mem+0x8000,
			  mem+0xa000,
			  mem+0xc000,
			  mem+0xe000};
unsigned long tstates=0,tsmax=(1<<30);
int topspeed=0,lowref=0;

int memattr[8]={0,1,1,1,1,1,1,1}; /* 8K RAM Banks */

int screen_dirty;
int hsize=256*SCALE,vsize=192*SCALE;
volatile int interrupted=0;
int scrn_freq=2;

unsigned char chrmap_old[24*32],chrmap[24*32];
unsigned char himap_old[192*32],himap[192*32];

int refresh_screen=1;



void sighandler(a)
int a;
{
  if(interrupted<2) interrupted=1;
}


void dontpanic()
{
  closedown();
  exit(1);
}


main(argc,argv)
int argc;
char **argv;
{
  struct sigaction sa;
  struct itimerval itv;
  int tmp=1000/50;	/* 50 ints/sec */
  
  printf("xace: Jupiter ACE emulator v%s (by Edward Patel)\n", XACE_VERSION);
  printf("Keys:\n");
  printf("\tESC - Quit xace\n");
  printf("\tF1  - Delete Line\n");
  printf("\tF4  - Inverse Video\n");
  printf("\tF9  - Graphics\n");
  printf("\tCtl - Break\n");
  printf("\tF12 - Reset\n");
      
  loadrom(mem);
  patches();
  memset(mem+8192,0xff,57344);
  memset(chrmap_old,0xff,768);
  
  startup(&argc,argv);
  
  if(argc==2) scrn_freq=atoi(argv[1]);
  if(scrn_freq<1) scrn_freq=1;
  if(scrn_freq>50) scrn_freq=50;

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
  
#ifndef SPOOLING_HOOK
  tsmax=62500;
#endif  

  mainloop();
  
}


loadrom(x)
unsigned char *x;
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


patches()
{
  /* patch the ROM here */
  mem[0x18a7]=0xed; /* for load_p */
  mem[0x18a8]=0xfc;
  mem[0x18a9]=0xc9;

  mem[0x1820]=0xed; /* for save_p */
  mem[0x1821]=0xfd;
  mem[0x1822]=0xc9;
}


unsigned int in(h,l)
     int h,l;
{
  if(l==0xfe)	/* keyboard */
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
      default:	return(255);
      }
  return(255);
}


unsigned int out(h,l,a)
int h,l,a;
{
  return(0);
}


save_p(c,_de,_hl,cf)
int c;
int _de;
int _hl;
int cf;
{
  char filename[32];
  int i;
  static int firstTime=1;
  static FILE *fp;

  /* printf("Save! c=%d de=%d hl=0x%04x cf=%d\n",c,_de,_hl,cf); */
  /* printf("[%d]\n",mem[8961]); */

  if (firstTime) {
    i=0;
    while (!isspace(mem[_hl+1+i])&&i<10) {
      filename[i]=mem[_hl+1+i]; i++;
    }
    filename[i++]='.';
    filename[i++]='t';
    filename[i++]='a';
    filename[i++]='p';
    filename[i++]='\0';
    printf("Saving to file '%s'\n",filename);
    fp = fopen(filename,"wb");
    if (!fp) {
      printf("Couldn't open file\n");
    } else {
      _de++;
      fputc(_de&0xff,fp);
      fputc((_de>>8)&0xff,fp);
      fwrite(&mem[_hl],1,_de,fp);
      firstTime = 0;
    }
  } else {
    _de++;
    fputc(_de&0xff,fp);
    fputc((_de>>8)&0xff,fp);
    fwrite(&mem[_hl],1,_de,fp);
    fclose(fp);
    fp=NULL;
    firstTime = 1;
  }
}


unsigned char empty_bytes[799] = { /* a small screen       */
  0x1a,0x00,0x20,0x6f,             /* will be loaded if    */
  0x74,0x68,0x65,0x72,             /* wanted file can't be */
  0x20,0x20,0x20,0x20,             /* opened */
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


unsigned char empty_dict[] = { /* a small forth program */
  0x1a,0x00,0x00,0x6f,         /* will be loaded if     */
  0x74,0x68,0x65,0x72,         /* wanted file can't be  */
  0x20,0x20,0x20,0x20,         /* opened */
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

load_p(c,_de,_hl,cf)
int c;
int _de;
int _hl;
int cf;
{
  char filename[32];
  int i;
  static unsigned char *empty_tape;
  static int efp;
  static int firstTime=1;
  static FILE *fp;

  /* printf("Load! c=%d de=%d hl=0x%04x cf=%d\n",c,_de,_hl,cf); */
  /* printf("[%d]\n",mem[9985]); */

  if (firstTime) {
    i=0;
    while (!isspace(mem[9985+1+i])&&i<10) {
      filename[i]=mem[9985+1+i]; i++;
    }
    filename[i++]='.';
    filename[i++]='t';
    filename[i++]='a';
    filename[i++]='p';
    
    if (mem[9985]) { /* dict or bytes load */
      empty_tape = empty_bytes;
    } else {
      empty_tape = empty_dict;
    }
    filename[i++]='\0';
    fp = fopen(filename,"rb");
    if (!fp) {
      printf("Couldn't open file '%s'\n",filename);
      efp=0;
      _de=empty_tape[efp++];
      _de+=256*empty_tape[efp++];
      memcpy(&mem[_hl],&empty_tape[efp],_de-1); /* -1 -> skip last byte */
      for (i=0;i<_de;i++) /* get memory OK */
	store(_hl+i,fetch(_hl+i));
      efp+=_de;
      for (i=0;i<10;i++)               /* let this file be it! */
	mem[_hl+1+i]=mem[9985+1+i];
      firstTime = 0;
    } else {
      printf("Reading from file '%s'\n",filename);
      _de=fgetc(fp);
      _de+=256*fgetc(fp);
      fread(&mem[_hl],1,_de-1,fp); /* -1 -> skip last byte */
      fgetc(fp); /* skip last byte */
      for (i=0;i<_de;i++) /* get memory OK */
	store(_hl+i,fetch(_hl+i));
      for (i=0;i<10;i++)               /* let this file be it! */
	mem[_hl+1+i]=mem[9985+1+i];
      firstTime = 0;
    }
  } else {
    if (fp) {
      _de=fgetc(fp);
      _de+=256*fgetc(fp);
      fread(&mem[_hl],1,_de-1,fp); /* -1 -> skip last byte */
      fgetc(fp); /* skip last byte */
      for (i=0;i<_de;i++) /* get memory OK */
	store(_hl+i,fetch(_hl+i));
      fclose(fp);
      fp=NULL;
    } else {
      _de=empty_tape[efp++];
      _de+=256*empty_tape[efp++];
      memcpy(&mem[_hl],&empty_tape[efp],_de-1); /* -1 -> skip last byte */
      for (i=0;i<_de;i++) /* get memory OK */
	store(_hl+i,fetch(_hl+i));
    }
    firstTime = 1;
  }
}


fix_tstates()
{
  tstates=0;
  pause();
}


do_interrupt()
{
  static int count=0;

  /* only do refresh() every 1/Nth */
  count++;
  if(count>=scrn_freq)
    count=0,refresh();

  check_events();

  /* be careful not to screw up any pending reset... */
  
  if(interrupted==1)
    interrupted=0;
}

/* the remainder of xmain.c is based on xz80's xspectrum.c. */

#ifdef SunKludge
char *shmat();
char *getenv();
#endif

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
static int black,white;
#ifdef MITSHM
static XShmSegmentInfo xshminfo;
#endif
static int invert=0;
static int borderchange=1;

static int is_local_server(name)
char *name;
{
#ifdef HAS_UNAME
   struct utsname un;
#else
   char sysname[MAX_DISP_LEN];
#endif

   if(name[0]==':')return 1;
   if(!strncmp(name,"unix",4))return 1;
   if(!strncmp(name,"localhost",9))return 1;

#ifdef HAS_UNAME
   uname(&un);
   if(!strncmp(name,un.sysname,strlen(un.sysname)))return 1;
   if(!strncmp(name,un.nodename,strlen(un.nodename)))return 1;
#else
   gethostname(sysname,MAX_DISP_LEN);
   if(!strncmp(name,sysname,strlen(sysname)))return 1;
#endif
   return 0;
}


static Display *open_display(argc,argv)
int *argc;
char **argv;
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

#ifdef MITSHM   
   mitshm=1;
#else
   mitshm=0;
#endif
   
   /* XXX deal with args here */

   if(mitshm && !is_local_server(dispname)){
      fputs("Disabling MIT-SHM on remote X server\n",stderr);
      mitshm=0;
   }
   return display;
}


static int image_init()
{
#ifdef MITSHM
   if(mitshm){
      ximage=XShmCreateImage(display,DefaultVisual(display,screen),
             DefaultDepth(display,screen),ZPixmap,NULL,&xshminfo,
             hsize,vsize);
      if(!ximage){
         fputs("Couldn't create X image\n",stderr);
         return 1;
      }
      xshminfo.shmid=shmget(IPC_PRIVATE,
               ximage->bytes_per_line*(ximage->height+1),IPC_CREAT|0777);
      if(xshminfo.shmid == -1){
         perror("Couldn't perform shmget");
         return 1;
      }
      xshminfo.shmaddr=ximage->data=shmat(xshminfo.shmid,0,0);
      if(!xshminfo.shmaddr){
         perror("Couldn't perform shmat");
         return 1;
      }
      xshminfo.readOnly=0;
      if(!XShmAttach(display,&xshminfo)){
         perror("Couldn't perform XShmAttach");
         return 1;
      }
      scrn_freq=rrshm;
   } else
#endif
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
   }
   linelen=ximage->bytes_per_line/SCALE;
   
   /* The following represent 4, 8, 16 or 32 bpp repectively */
   if(linelen!=32 && linelen!=256 && linelen!=512 && linelen!=1024)
      fprintf(stderr,"Line length=%d; expect strange results!\n",linelen);
#if 0
   if(linelen==32 &&
         (BitmapBitOrder(display)!=MSBFirst || ImageByteOrder(display)!=MSBFirst))
      fprintf(stderr,"BitmapBitOrder=%s and ImageByteOrder=%s.\n",
         BitmapBitOrder(display)==MSBFirst?"MSBFirst":"LSBFirst",
         ImageByteOrder(display)==MSBFirst?"MSBFirst":"LSBFirst"),
      fputs("If the display is mixed up, please mail me these values\n",stderr),
      fputs("and describe the display as accurately as possible.\n",stderr);
#endif
   
   image=ximage->data;
   return 0;
}


static void notify(argc,argv)
int *argc;
char **argv;
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


#if SCALE>1
static void scaleup_init(){
   int j, k, l, m;
   unsigned long bigval; /* SCALING must be <= 4! */
   for(l=0;l<256;scaleup[l++]=bigval)
      for(m=l,bigval=j=0;j<8;j++) {
         for(k=0;k<SCALE;k++)
            bigval=(bigval>>1)|((m&1)<<31);
         m>>=1;
      }
}
#endif


startup(argc,argv)
int *argc;
char **argv;
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
   if(image_init()){
      if(mitshm){
         fputs("Failed to create X image - trying again with mitshm off\n",stderr);
         mitshm=0;
         if(image_init())exit(1);
      }
      else exit(1);
   }

#if SCALE>1
   if(linelen==32) scaleup_init();
#endif

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


clear_keyboard() 
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


#ifdef SPOOLING_HOOK
static FILE *spoolFile=NULL;
static int flipFlop;
#endif

int process_keypress(kev)
XKeyEvent *kev;
{
   char buf[3];
   KeySym ks;

#ifdef SPOOLING_HOOK
   if (spoolFile && (ks=fgetc(spoolFile))) {
     if (ks==EOF) {
       fclose(spoolFile);
       spoolFile=NULL;
       return 0;
     }
   } else
#endif
     XLookupString(kev,buf,2,&ks,NULL);

   switch(ks){
   case XK_Escape:
     dontpanic();
     /* doesn't return */
     break;  
#ifdef SPOOLING_HOOK
   case XK_F11: 
     {
       char str[80];
       printf("Enter spool file:");
       scanf("%s",str);
       spoolFile=fopen(str,"rt");
       flipFlop=0;
     }
     break;
#endif
   case XK_F12:
     interrupted=2;	/* will cause a reset */
     memset(mem+8192,0xff,57344);
     refresh_screen=1;
     clear_keyboard();
     break;
   case '\n':
   case XK_Return: 
     keyports[6]&=0xfe; 
     break;
   case XK_Shift_R:
   case XK_Shift_L: 
   case XK_Alt_L: 
   case XK_Alt_R: 
   case XK_Meta_L: 
   case XK_Meta_R:
     break;
   case XK_F4:
     keyports[3]&=0xf7; 
     keyports[0]&=0xfe; 
     break;
   case XK_F9:
     keyports[4]&=0xfd; 
     keyports[0]&=0xfe; 
     break;
   case XK_Control_L: 
   case XK_Control_R: 
     keyports[7]&=0xfe; 
     keyports[0]&=0xfe; 
     break;
   case XK_F1:
     keyports[3]&=0xfe; 
     keyports[0]&=0xfe; 
     break;
   case XK_BackSpace: 
   case XK_Delete:
     keyports[0]&=0xfe; 
     keyports[4]&=0xfe; 
     break;
   case XK_Up:
     keyports[4]&=0xef;
     keyports[0]&=0xfe;
     break;
   case XK_Down:
     keyports[4]&=0xf7;
     keyports[0]&=0xfe;
     break;
   case XK_Left:
     keyports[3]&=0xef;
     keyports[0]&=0xfe;
     break;
   case XK_Right:
     keyports[4]&=0xfb;
     keyports[0]&=0xfe;
     break;
     break;
   case XK_space:
     keyports[7]&=0xfe;
     break;
   case XK_exclam:
     keyports[3]&=0xfe;
     keyports[0]&=0xfd;
     break;
   case XK_quotedbl:
     keyports[5]&=0xfe;
     keyports[0]&=0xfd;
     break;
   case XK_numbersign:
     keyports[3]&=0xfb;
     keyports[0]&=0xfd;
     break;
   case XK_dollar:
     keyports[3]&=0xf7;
     keyports[0]&=0xfd;
     break;
   case XK_percent:
     keyports[3]&=0xef;
     keyports[0]&=0xfd;
     break;
   case XK_ampersand:
     keyports[4]&=0xef;
     keyports[0]&=0xfd;
     break;
   case XK_apostrophe:
     keyports[4]&=0xf7;
     keyports[0]&=0xfd;
     break;
   case XK_parenleft:
     keyports[4]&=0xfb;
     keyports[0]&=0xfd;
     break;
   case XK_parenright:
     keyports[4]&=0xfd;
     keyports[0]&=0xfd;
     break;
   case XK_asterisk:
     keyports[7]&=0xf7;
     keyports[0]&=0xfd;
     break;
   case XK_plus:
     keyports[6]&=0xfb;
     keyports[0]&=0xfd;
     break;
   case XK_comma:
     keyports[7]&=0xfb;
     keyports[0]&=0xfd;
     break;
   case XK_minus:
     keyports[6]&=0xf7;
     keyports[0]&=0xfd;
     break;
   case XK_period:
     keyports[7]&=0xfd;
     keyports[0]&=0xfd;
     break;
   case XK_slash:
     keyports[7]&=0xef;
     keyports[0]&=0xfd;
     break;
   case XK_0:
     keyports[4]&=0xfe;
     break;
   case XK_1:
     keyports[3]&=0xfe;
     break;
   case XK_2:
     keyports[3]&=0xfd;
     break;
   case XK_3:
     keyports[3]&=0xfb;
     break;
   case XK_4:
     keyports[3]&=0xf7;
     break;
   case XK_5:
     keyports[3]&=0xef;
     break;
   case XK_6:
     keyports[4]&=0xef;
     break;
   case XK_7:
     keyports[4]&=0xf7;
     break;
   case XK_8:
     keyports[4]&=0xfb;
     break;
   case XK_9:
     keyports[4]&=0xfd;
     break;
   case XK_colon:
     keyports[0]&=0xfb;
     keyports[0]&=0xfd;
     break;
   case XK_semicolon:
     keyports[5]&=0xfd;
     keyports[0]&=0xfd;
     break;
   case XK_less:
     keyports[2]&=0xf7;
     keyports[0]&=0xfd;
     break;
   case XK_equal:
     keyports[6]&=0xfd;
     keyports[0]&=0xfd;
     break;
   case XK_greater:
     keyports[2]&=0xef;
     keyports[0]&=0xfd;
     break;
   case XK_question:
     keyports[0]&=0xef;
     keyports[0]&=0xfd;
     break;
   case XK_at:
     keyports[3]&=0xfd;
     keyports[0]&=0xfd;
     break;
   case XK_A:
     keyports[0]&=0xfe;
   case XK_a:
     keyports[1]&=0xfe;
     break;
   case XK_B:
     keyports[0]&=0xfe;
   case XK_b:
     keyports[7]&=0xf7;
     break;
   case XK_C:
     keyports[0]&=0xfe;
   case XK_c:
     keyports[0]&=0xef;
     break;
   case XK_D:
     keyports[0]&=0xfe;
   case XK_d:
     keyports[1]&=0xfb;
     break;
   case XK_E:
     keyports[0]&=0xfe;
   case XK_e:
     keyports[2]&=0xfb;
     break;
   case XK_F:
     keyports[0]&=0xfe;
   case XK_f:
     keyports[1]&=0xf7;
     break;
   case XK_G:
     keyports[0]&=0xfe;
   case XK_g:
     keyports[1]&=0xef;
     break;
   case XK_H:
     keyports[0]&=0xfe;
   case XK_h:
     keyports[6]&=0xef;
     break;
   case XK_I:
     keyports[0]&=0xfe;
   case XK_i:
     keyports[5]&=0xfb;
     break;
   case XK_J:
     keyports[0]&=0xfe;
   case XK_j:
     keyports[6]&=0xf7;
     break;
   case XK_K:
     keyports[0]&=0xfe;
   case XK_k:
     keyports[6]&=0xfb;
     break;
   case XK_L:
     keyports[0]&=0xfe;
   case XK_l:
     keyports[6]&=0xfd;
     break;
   case XK_M:
     keyports[0]&=0xfe;
   case XK_m:
     keyports[7]&=0xfd;
     break;
   case XK_N:
     keyports[0]&=0xfe;
   case XK_n:
     keyports[7]&=0xfb;
     break;
   case XK_O:
     keyports[0]&=0xfe;
   case XK_o:
     keyports[5]&=0xfd;
     break;
   case XK_P:
     keyports[0]&=0xfe;
   case XK_p:
     keyports[5]&=0xfe;
     break;
   case XK_Q:
     keyports[0]&=0xfe;
   case XK_q:
     keyports[2]&=0xfe;
     break;
   case XK_R:
     keyports[0]&=0xfe;
   case XK_r:
     keyports[2]&=0xf7;
     break;
   case XK_S:
     keyports[0]&=0xfe;
   case XK_s:
     keyports[1]&=0xfd;
     break;
   case XK_T:
     keyports[0]&=0xfe;
   case XK_t:
     keyports[2]&=0xef;
     break;
   case XK_U:
     keyports[0]&=0xfe;
   case XK_u:
     keyports[5]&=0xf7;
     break;
   case XK_V:
     keyports[0]&=0xfe;
   case XK_v:
     keyports[7]&=0xef;
     break;
   case XK_W:
     keyports[0]&=0xfe;
   case XK_w:
     keyports[2]&=0xfd;
     break;
   case XK_X:
     keyports[0]&=0xfe;
   case XK_x:
     keyports[0]&=0xf7;
     break;
   case XK_Y:
     keyports[0]&=0xfe;
   case XK_y:
     keyports[5]&=0xef;
     break;
   case XK_Z:
     keyports[0]&=0xfe;
   case XK_z:
     keyports[0]&=0xfb;
     break;
   case XK_bracketleft:
     keyports[5]&=0xef;
     keyports[0]&=0xfd;
     break;
   case XK_backslash:
     keyports[1]&=0xfb;
     keyports[0]&=0xfd;
     break;
   case XK_bracketright:
     keyports[5]&=0xf7;
     keyports[0]&=0xfd;
     break;
   case XK_asciicircum:
     keyports[6]&=0xef;
     keyports[0]&=0xfd;
     break;
   case XK_underscore:
     keyports[4]&=0xfe;
     keyports[0]&=0xfd;
     break;
   case XK_grave:
     keyports[5]&=0xfb;
     keyports[0]&=0xfd;
     break;
   case XK_braceleft:
     keyports[1]&=0xf7;
     keyports[0]&=0xfd;
     break;
   case XK_bar:
     keyports[1]&=0xfd;
     keyports[0]&=0xfd;
     break;
   case XK_braceright:
     keyports[1]&=0xef;
     keyports[0]&=0xfd;
     break;
   case XK_asciitilde:
     keyports[1]&=0xfe;
     keyports[0]&=0xfd;
     break;
   default:
     break;
   }
   return 0;
}


void process_keyrelease(kev)
XKeyEvent *kev;
{
   char buf[3];
   KeySym ks;

   XLookupString(kev,buf,2,&ks,NULL);

   switch(ks){
   case XK_Return: 
     keyports[6]|=1; 
     break;
   case XK_Shift_L: 
   case XK_Shift_R:
   case XK_Alt_L: 
   case XK_Alt_R: 
   case XK_Meta_L: 
   case XK_Meta_R:
   case XK_Escape:
   case XK_Tab:
     clear_keyboard();
     break;
   case XK_F4:
     keyports[3]|=~0xf7; 
     keyports[0]|=~0xfe; 
     break;
   case XK_F9:
     keyports[4]|=~0xfd; 
     keyports[0]|=~0xfe; 
     break;
   case XK_Control_L: 
   case XK_Control_R: 
     keyports[7]|=~0xfe; 
     keyports[0]|=~0xfe; 
     break;
   case XK_F1:
     keyports[3]|=~0xfe; 
     keyports[0]|=~0xfe; 
     break;
   case XK_BackSpace: 
   case XK_Delete:
     keyports[0]|=1; 
     keyports[4]|=1; 
     break;
   case XK_Up:
     keyports[4]|=~0xef;
     keyports[0]|=~0xfe;
     break;
   case XK_Down:
     keyports[4]|=~0xf7;
     keyports[0]|=~0xfe;
     break;
   case XK_Left:
     keyports[3]|=~0xef;
     keyports[0]|=~0xfe;
     break;
   case XK_Right:
     keyports[4]|=~0xfb;
     keyports[0]|=~0xfe;
     break;
     break;
   case XK_space:
     keyports[7]|=~0xfe;
     break;
   case XK_exclam:
     keyports[3]|=~0xfe;
     keyports[0]|=~0xfd;
     break;
   case XK_quotedbl:
     keyports[5]|=~0xfe;
     keyports[0]|=~0xfd;
     break;
   case XK_numbersign:
     keyports[3]|=~0xfb;
     keyports[0]|=~0xfd;
     break;
   case XK_dollar:
     keyports[3]|=~0xf7;
     keyports[0]|=~0xfd;
     break;
   case XK_percent:
     keyports[3]|=~0xef;
     keyports[0]|=~0xfd;
     break;
   case XK_ampersand:
     keyports[4]|=~0xef;
     keyports[0]|=~0xfd;
     break;
   case XK_apostrophe:
     keyports[4]|=~0xf7;
     keyports[0]|=~0xfd;
     break;
   case XK_parenleft:
     keyports[4]|=~0xfb;
     keyports[0]|=~0xfd;
     break;
   case XK_parenright:
     keyports[4]|=~0xfd;
     keyports[0]|=~0xfd;
     break;
   case XK_asterisk:
     keyports[7]|=~0xf7;
     keyports[0]|=~0xfd;
     break;
   case XK_plus:
     keyports[6]|=~0xfb;
     keyports[0]|=~0xfd;
     break;
   case XK_comma:
     keyports[7]|=~0xfb;
     keyports[0]|=~0xfd;
     break;
   case XK_minus:
     keyports[6]|=~0xf7;
     keyports[0]|=~0xfd;
     break;
   case XK_period:
     keyports[7]|=~0xfd;
     keyports[0]|=~0xfd;
     break;
   case XK_slash:
     keyports[7]|=~0xef;
     keyports[0]|=~0xfd;
     break;
   case XK_0:
     keyports[4]|=~0xfe;
     break;
   case XK_1:
     keyports[3]|=~0xfe;
     break;
   case XK_2:
     keyports[3]|=~0xfd;
     break;
   case XK_3:
     keyports[3]|=~0xfb;
     break;
   case XK_4:
     keyports[3]|=~0xf7;
     break;
   case XK_5:
     keyports[3]|=~0xef;
     break;
   case XK_6:
     keyports[4]|=~0xef;
     break;
   case XK_7:
     keyports[4]|=~0xf7;
     break;
   case XK_8:
     keyports[4]|=~0xfb;
     break;
   case XK_9:
     keyports[4]|=~0xfd;
     break;
   case XK_colon:
     keyports[0]|=~0xfb;
     keyports[0]|=~0xfd;
     break;
   case XK_semicolon:
     keyports[5]|=~0xfd;
     keyports[0]|=~0xfd;
     break;
   case XK_less:
     keyports[2]|=~0xf7;
     keyports[0]|=~0xfd;
     break;
   case XK_equal:
     keyports[6]|=~0xfd;
     keyports[0]|=~0xfd;
     break;
   case XK_greater:
     keyports[2]|=~0xef;
     keyports[0]|=~0xfd;
     break;
   case XK_question:
     keyports[0]|=~0xef;
     keyports[0]|=~0xfd;
     break;
   case XK_at:
     keyports[3]|=~0xfd;
     keyports[0]|=~0xfd;
     break;
   case XK_A:
     keyports[0]|=~0xfe;
   case XK_a:
     keyports[1]|=~0xfe;
     break;
   case XK_B:
     keyports[0]|=~0xfe;
   case XK_b:
     keyports[7]|=~0xf7;
     break;
   case XK_C:
     keyports[0]|=~0xfe;
   case XK_c:
     keyports[0]|=~0xef;
     break;
   case XK_D:
     keyports[0]|=~0xfe;
   case XK_d:
     keyports[1]|=~0xfb;
     break;
   case XK_E:
     keyports[0]|=~0xfe;
   case XK_e:
     keyports[2]|=~0xfb;
     break;
   case XK_F:
     keyports[0]|=~0xfe;
   case XK_f:
     keyports[1]|=~0xf7;
     break;
   case XK_G:
     keyports[0]|=~0xfe;
   case XK_g:
     keyports[1]|=~0xef;
     break;
   case XK_H:
     keyports[0]|=~0xfe;
   case XK_h:
     keyports[6]|=~0xef;
     break;
   case XK_I:
     keyports[0]|=~0xfe;
   case XK_i:
     keyports[5]|=~0xfb;
     break;
   case XK_J:
     keyports[0]|=~0xfe;
   case XK_j:
     keyports[6]|=~0xf7;
     break;
   case XK_K:
     keyports[0]|=~0xfe;
   case XK_k:
     keyports[6]|=~0xfb;
     break;
   case XK_L:
     keyports[0]|=~0xfe;
   case XK_l:
     keyports[6]|=~0xfd;
     break;
   case XK_M:
     keyports[0]|=~0xfe;
   case XK_m:
     keyports[7]|=~0xfd;
     break;
   case XK_N:
     keyports[0]|=~0xfe;
   case XK_n:
     keyports[7]|=~0xfb;
     break;
   case XK_O:
     keyports[0]|=~0xfe;
   case XK_o:
     keyports[5]|=~0xfd;
     break;
   case XK_P:
     keyports[0]|=~0xfe;
   case XK_p:
     keyports[5]|=~0xfe;
     break;
   case XK_Q:
     keyports[0]|=~0xfe;
   case XK_q:
     keyports[2]|=~0xfe;
     break;
   case XK_R:
     keyports[0]|=~0xfe;
   case XK_r:
     keyports[2]|=~0xf7;
     break;
   case XK_S:
     keyports[0]|=~0xfe;
   case XK_s:
     keyports[1]|=~0xfd;
     break;
   case XK_T:
     keyports[0]|=~0xfe;
   case XK_t:
     keyports[2]|=~0xef;
     break;
   case XK_U:
     keyports[0]|=~0xfe;
   case XK_u:
     keyports[5]|=~0xf7;
     break;
   case XK_V:
     keyports[0]|=~0xfe;
   case XK_v:
     keyports[7]|=~0xef;
     break;
   case XK_W:
     keyports[0]|=~0xfe;
   case XK_w:
     keyports[2]|=~0xfd;
     break;
   case XK_X:
     keyports[0]|=~0xfe;
   case XK_x:
     keyports[0]|=~0xf7;
     break;
   case XK_Y:
     keyports[0]|=~0xfe;
   case XK_y:
     keyports[5]|=~0xef;
     break;
   case XK_Z:
     keyports[0]|=~0xfe;
   case XK_z:
     keyports[0]|=~0xfb;
     break;
   case XK_bracketleft:
     keyports[5]|=~0xef;
     keyports[0]|=~0xfd;
     break;
   case XK_backslash:
     keyports[1]|=~0xfb;
     keyports[0]|=~0xfd;
     break;
   case XK_bracketright:
     keyports[5]|=~0xf7;
     keyports[0]|=~0xfd;
     break;
   case XK_asciicircum:
     keyports[6]|=~0xef;
     keyports[0]|=~0xfd;
     break;
   case XK_underscore:
     keyports[4]|=~0xfe;
     keyports[0]|=~0xfd;
     break;
   case XK_grave:
     keyports[5]|=~0xfb;
     keyports[0]|=~0xfd;
     break;
   case XK_braceleft:
     keyports[1]|=~0xf7;
     keyports[0]|=~0xfd;
     break;
   case XK_bar:
     keyports[1]|=~0xfd;
     keyports[0]|=~0xfd;
     break;
   case XK_braceright:
     keyports[1]|=~0xef;
     keyports[0]|=~0xfd;
     break;
   case XK_asciitilde:
     keyports[1]|=~0xfe;
     keyports[0]|=~0xfd;
     break;
   default:
     clear_keyboard();
     break;
   }
   return;
}


check_events()
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
         case MapNotify: case UnmapNotify: case ReparentNotify:
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
         case KeyPress: process_keypress((XKeyEvent *)&xev);
            break;
         case KeyRelease: process_keyrelease((XKeyEvent *)&xev);
            break;
         default:
            fprintf(stderr,"unhandled X event, type %d\n",xev.type);
      }
   }
}



/* XXX tryout */

/* to redraw the (low-res, at least) screen, we translate it into
 * a more normal char. map, then find the smallest rectangle which
 * covers all the changes, and update that.
 */

refresh(){
	int j,k,m;
	unsigned char *ptr,*cptr;
	int x,y,b,c,d,inv,mask;
	int pasteol;
	int xmin,ymin,xmax,ymax;
	int ofs;
	int bytesPerPixel;
	int imageIndex;
     
#ifdef SPOOLING_HOOK
	if (spoolFile) {     
		if (flipFlop==1) {
			process_keypress(NULL);
		}
		if (flipFlop==3) {
			flipFlop=0;
			clear_keyboard();
		}
		flipFlop++;
	}
#endif

	if(borderchange>0)
	{
		/* XXX what about expose events? need to set borderchange... */
		XSetWindowBackground(display,borderwin,white);
		XClearWindow(display,borderwin);
		XFlush(display);
		borderchange=0;
	}

	/* draw normal lo-res screen */
   
	/* translate to char map, comparing against previous */
   
	ptr=mem+0x2400;	/* D_FILE */
   
	/* since we can't just do "don't bother if it's junk" as we always
	 * need to draw a screen, just draw *valid* junk that won't result
	 * in a segfault or anything. :-)
	 */
	if(ptr-mem<0 || ptr-mem>0xf000) ptr=mem+0xf000;
	/*     ptr++; 	*/ /* skip first HALT */
   
	cptr=mem+0x2c00;	/* char. set */
   
	xmin=31; ymin=23; xmax=0; ymax=0;
   
	bytesPerPixel = linelen / 256;
   
   
	ofs=0;
	for(y=0;y<24;y++)
	{
		pasteol=0;
		for(x=0;x<32;x++,ptr++,ofs++)
		{
			c=*ptr;
	   
			if((chrmap[ofs]=c)!=chrmap_old[ofs] || refresh_screen)
			{
				/* update size of area to be drawn */
				if(x<xmin) xmin=x;
				if(y<ymin) ymin=y;
				if(x>xmax) xmax=x;
				if(y>ymax) ymax=y;
	       
				/* draw character into image */
				inv=(c&128); c&=127;
	       
				for(b=0;b<8;b++)
				{
					d=cptr[c*8+b]; if(inv) d^=255;
		   
					if(linelen==32)
					{
						/* 1-bit mono */
						/* XXX doesn't support SCALE>1 */
						image[(y*8+b)*linelen+x]=~d;
					}
					else
					{
						imageIndex = ((y*8+b)*hsize+x*8)*SCALE*bytesPerPixel;
						mask=256;
						while (mask>>=1) 
						{
							m=((d&mask)?black:white);
							
							for(j=0;j<SCALE;j++) {
								for(k=0;k<SCALE*bytesPerPixel;k++) {
									image[imageIndex+(j*hsize*bytesPerPixel+k)] = m;
								}
							}
							imageIndex += SCALE*bytesPerPixel;
						}
					}
				}		     
		     }
		}
	}
   
	/* now, copy new to old for next time */
	memcpy(chrmap_old,chrmap,768);
   
	/* next bit happens for both hi and lo-res */
   
	if(refresh_screen) xmin=0,ymin=0,xmax=31,ymax=23;
   
	if(xmax>=xmin && ymax>=ymin)
	{     
#ifdef MITSHM
		if(mitshm)
			XShmPutImage(display,mainwin,maingc,ximage,
						 xmin*8*SCALE,ymin*8*SCALE,xmin*8*SCALE,ymin*8*SCALE,
						 (xmax-xmin+1)*8*SCALE,(ymax-ymin+1)*8*SCALE,0);
		else
#endif
			XPutImage(display,mainwin,maingc,ximage,
					  xmin*8*SCALE,ymin*8*SCALE,xmin*8*SCALE,ymin*8*SCALE,
					  (xmax-xmin+1)*8*SCALE,(ymax-ymin+1)*8*SCALE);
	}
   
	XFlush(display);
	refresh_screen=0;
}


closedown(){
#ifdef MITSHM
   if(mitshm){
      XShmDetach(display,&xshminfo);
      XDestroyImage(ximage);
      shmdt(xshminfo.shmaddr);
      shmctl(xshminfo.shmid,IPC_RMID,0);
   }
#else
   free(ximage->data);
#endif
   XAutoRepeatOn(display);
   XCloseDisplay(display);
}
