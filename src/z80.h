/* Miscellaneous definitions for xz80, copyright (C) 1994 Ian Collier.
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

#define Z80_quit  1
#define Z80_NMI   2
#define Z80_reset 3
#define Z80_load  4
#define Z80_save  5
#define Z80_log   6

extern unsigned char mem[65536];
extern unsigned char *memptr[];
extern int memattr[];
extern int hsize,vsize;
extern volatile int interrupted;

extern unsigned int in(int h, int l);
extern unsigned int out(int h,int l, int a);
extern void do_interrupt(void);
extern int mainloop();
extern void fix_tstates(void);

#define fetch(x) (memptr[(unsigned short)(x&0xe000)>>13][(x)&0x1fff])
#define fetch2(x) ((fetch((x)+1)<<8)|fetch(x))

#define store(x,y) do {\
          unsigned short off=(x)&0x1fff;\
          unsigned char page=(unsigned short)(x&0xe000)>>13;\
          int attr=memattr[page];\
          if(attr){\
             memptr[page][off]=(y); \
	     if ((x>=0x2000&&x<=0x23ff)||(x>=0x2800&&x<=0x2bff)) \
	       memptr[page][off+0x400]=(y); \
	     else if ((x>=0x2400&&x<=0x27ff)||(x>=0x2c00&&x<=0x2fff)) \
	       memptr[page][off-0x400]=(y); \
	     else if (x>=0x3000&&x<=0x3fff) { \
	       memptr[page][(x&0x03ff)+0x1000]=(y); \
	       memptr[page][(x&0x03ff)+0x1400]=(y); \
	       memptr[page][(x&0x03ff)+0x1800]=(y); \
	       memptr[page][(x&0x03ff)+0x1c00]=(y); \
	       } \
             } \
           } while(0)

#define store2b(x,hi,lo) do {\
          unsigned short off=(x)&0x1fff;\
          unsigned char page=(unsigned short)(x&0xe000)>>13;\
          int attr=memattr[page];\
          if(attr) { \
             memptr[page][off]=(lo);\
             memptr[page][off+1]=(hi);\
	     if ((x>=0x2000&&x<=0x23ff)||(x>=0x2800&&x<=0x2bff)) { \
	       memptr[page][off+0x400]=(lo); \
	       memptr[page][off+0x401]=(hi); \
	     } else if ((x>=0x2400&&x<=0x27ff)||(x>=0x2c00&&x<=0x2fff)) { \
	       memptr[page][off-0x400]=(lo); \
	       memptr[page][off-0x3ff]=(hi); \
	     } else if (x>=0x3000&&x<=0x3fff) { \
	       memptr[page][(x&0x03ff)+0x1000]=(lo); \
	       memptr[page][(x&0x03ff)+0x1001]=(hi); \
	       memptr[page][(x&0x03ff)+0x1400]=(lo); \
	       memptr[page][(x&0x03ff)+0x1401]=(hi); \
	       memptr[page][(x&0x03ff)+0x1800]=(lo); \
	       memptr[page][(x&0x03ff)+0x1801]=(hi); \
	       memptr[page][(x&0x03ff)+0x1c00]=(lo); \
	       memptr[page][(x&0x03ff)+0x1c01]=(hi); \
             } \
	     } \
          } while(0)

#define store2(x,y) store2b(x,(y)>>8,(y)&255)

#ifdef __GNUC__
static void inline storefunc(unsigned short ad,unsigned char b){
   store(ad,b);
}
#undef store
#define store(x,y) storefunc(x,y)

static void inline store2func(unsigned short ad,unsigned char b1,unsigned char b2){
   store2b(ad,b1,b2);
}
#undef store2b
#define store2b(x,hi,lo) store2func(x,hi,lo)
#endif

#define bc ((b<<8)|c)
#define de ((d<<8)|e)
#define hl ((h<<8)|l)
