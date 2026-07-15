// ------------------------------------------------------------------------------
// ILPER 1.40 for Linux
// Copyright (c) 2008-2009  J-F Garnier
// Copyright (c) 2011-2012  Ch. Gottheimer
// Copyright (c) 2026       J.M. Vansteene
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// ----------------------------------------------------------------------------------
//
// ilper.h   ILPER prototypes
//
// 2011: ported on Linux by Ch. Gottheimer
// 2026: updated by J-M Vansteene
// ---------------------------------------------------------------------------------

#ifndef __ILPER_H__
#define __ILPER_H__

#include <panel.h>

// From ilmain.c
extern char strca[];
extern char strwd[];
extern void DisplayMnemo( char * );
extern void VideoStr( char );
extern void DisplayStr( char );
extern void initDisplay (WINDOW *);

// From ilper7.c
extern void init_hpil( void );
extern int hpil_transmit( int );

// From ildisp.c
extern int ILdisplay( int );
extern void ClrDisplayP ( void );
extern void init_ildisplay( void );

// From ilvideo.c
extern int ILvideo( int );
extern void ClrDisplayV ( void );
extern void init_ilvideo( void );

// From ildrive.c
extern int ILhdisc( int );
extern void init_ilhdisc( char * );

// From version.c
extern char ilper_vers[];
extern char ilper_date[];
extern char ilper_os[];

// PANEL structure
typedef struct DISP_PANEL {
  PANEL  *panel;
  WINDOW *w;
  int     hide;	/* TRUE if panel is hidden */
}DISP_PANEL;

extern DISP_PANEL panels[];

typedef enum Color {DEFAULT, RED, GREEN, BLUE, CYAN, YELLOW, MAGENTA, WHITE, BLACK} Color;

extern chtype *boxc;

void init_term ();
void init_window ();
void create_subwindows ();
void delete_subwindows ();
void select_file (WINDOW *);

#endif //  __ILPER_H__

