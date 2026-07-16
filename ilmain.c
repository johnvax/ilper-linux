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
// ilmain.c   ILPER main module
//
// Based on previous personal work:
// 1986: ILPER4 (6502 assembler)
// 1988: ILPER5 modular version (6502 assembler)
// 1993: ILPER6 ported on PC (8086 assembler)
// 1997: rewriten in C and included in Emu41
// 2008: rewriten in VB for the standalone ILPER Windows version using the PILBox!
// 2009: released as free software
// 2011: ported on Linux by Ch. Gottheimer with NCURSES
// 2026: windows improvement: see ilper(1) man page
// ---------------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/fcntl.h>
#include <limits.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <locale.h>
#include <signal.h>
#include <ncurses.h>
#include <libgen.h>

#include "ilper.h"

/*  This signal is not defined by POSIX, but should be
   present on all systems that have resizable terminals. */
#ifndef SIGWINCH
#define SIGWINCH  28
#endif

#define CON   0x496
#define COFF  0x497
#define TDIS  0x494

#define USE8BITS 1

#ifndef ILDEVICE
# define ILDEVICE  "/dev/ttyUSB0"
#endif

#ifndef ILDISK
# define ILDISK  "HDRIVE1.DAT"
#endif

       char   strwd[_POSIX_PATH_MAX]; // working directory (PATH where strca)
       char   strca[_POSIX_PATH_MAX];
static char   strpo[_POSIX_PATH_MAX];
static char   strvers[20];
static char  *strst[ 2 ] = { "STOPPED", "RUNNING", };
static char  *stryn[ 2 ] = { "YES", "NO", };
static char  *strsc = "scope.out";
static char  *strdp = "display.out";
static int    ilst = FALSE, issc = FALSE;
static int    ilfd = -1, fdsc = -1;
       int    fddp = -1;
static int    lasth = 0;
//                          LFile, Scope, Display, Video,  Start/Stop, Pilboxlink
static WINDOW *main_window,*wca,   *wsc,  *wdp,    *wdv,  *wst,        *wpo,       *wcur;
static WINDOW *wrwin = NULL, *wdrwin = NULL;
static char   *wstr;
static int    x, y;
                       //  ls   rs   ts   bs   tl   tr   bl   br   lt   rt   tt   bt
static chtype gboxc[] = {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, };
static chtype aboxc[] = { '|', '|', '-', '-', '+', '+', '+', '+', '|', '|', '-', '-', };
#ifdef USEASCII
       chtype *boxc = aboxc;
#else
       chtype *boxc = gboxc;
#endif

static const char *const copyright = "Copyright (c) 2008-2026 J-F Garnier & Ch Gottheimer & J-M Vansteene";
static const char *const footer = "d: toggle display - h: Help - q: quit";

DISP_PANEL panels[3] = { {NULL, NULL, FALSE} ,{NULL, NULL, FALSE} ,{NULL, NULL, FALSE} };

// Help
typedef struct _RICHTEXT {
  char *key;
  int kcolor;
  char *str;
  int strcolor;
} RICHTEXT;
static RICHTEXT help [] = {
  { "h", A_BOLD | COLOR_PAIR(YELLOW), " - Display a help" , COLOR_PAIR(DEFAULT)},
  { "v", A_BOLD | COLOR_PAIR(YELLOW), " - Display the software version", COLOR_PAIR(0) },
  { "a", A_BOLD | COLOR_PAIR(YELLOW), " - Start the emulation", COLOR_PAIR(0) },
  { "o", A_BOLD | COLOR_PAIR(YELLOW), " - Stop the emulation", COLOR_PAIR(0) },
  { "l", A_BOLD | COLOR_PAIR(YELLOW), " - Change the LIF File (Edit)", COLOR_PAIR(0) },
  { "L", A_BOLD | COLOR_PAIR(YELLOW), " - Select the LIF File (Browser)", COLOR_PAIR(0) },
  { "p", A_BOLD | COLOR_PAIR(YELLOW), " - Change the device for the PILBox Link", COLOR_PAIR(0) },
  { "s", A_BOLD | COLOR_PAIR(YELLOW), " - Enable/Disable the HP-IL scope mode", COLOR_PAIR(0) },
  { "S", A_BOLD | COLOR_PAIR(YELLOW), " - Enable/Disable the HP-IL scope recording into the file scope.out", COLOR_PAIR(0) },
  { "d", A_BOLD | COLOR_PAIR(YELLOW), " - Toggle between default Display (PRINTER) and Video interface MC00701A", COLOR_PAIR(0) },
  { "D", A_BOLD | COLOR_PAIR(YELLOW), " - Enable/Disable the HP-IL display recording into the file display.out", COLOR_PAIR(0) },
  { "q or Q", A_BOLD | COLOR_PAIR(YELLOW), " - Close the PILBox and exit", COLOR_PAIR(0) },
  { NULL, 0, NULL, 0 }
};

volatile sig_atomic_t pending_usr1;
volatile sig_atomic_t pending_winch;
void init_window ();

// ******************************************
// raise_window_r()
//
// raise a pop-up window and print the given
// richtext structure
// justify: 0=left, 1=middle, 2=right
// ******************************************
static void raise_window_r ( int nlin, int ncol, const char *title, RICHTEXT text [] )
{
  int i;
  
  wrwin = newwin( nlin, ncol, (y - nlin) / 2, (x - ncol) / 2 );
  wborder( wrwin, boxc[0], boxc[1], boxc[2], boxc[3], boxc[4], boxc[5], boxc[6], boxc[7] );
  wattron (wrwin, COLOR_PAIR(CYAN));
  mvwprintw( wrwin, 0, 2, "[ %s ]", title );
  wattroff (wrwin, COLOR_PAIR(CYAN));
  wrefresh( wrwin );
  wdrwin = derwin( wrwin, nlin - 2, ncol - 2, 1, 1);

  i=0;
  while (text[i].key)
    {
      wattron  (wrwin, text[i].kcolor);
      mvwprintw  (wrwin, i+1, 2, "%s", text[i].key);
      wattroff (wrwin, text[i].kcolor);
      wrefresh (wrwin);
      wattron  (wrwin, text[i].strcolor);
      mvwprintw  (wrwin, i+1, 2 + strlen (text[i].key) , "%s", text[i].str);
      wattroff (wrwin, text[i].strcolor);
      i++;
    }
  wrefresh( wdrwin );
}

// ******************************************
// raise_window()
//
// raise a pop-up window and print the given
// format and arguments
// justify: 0=left, 1=middle, 2=right
// ******************************************
static void raise_window( int nlin, int ncol, const char *title, const int color, int justify, const char *fmt, ... )
{
  va_list  ap;
  int i, l;
  char c, str[1024];
  char *lastp, *p;

  wrwin = newwin( nlin, ncol, (y - nlin) / 2, (x - ncol) / 2 );
  wborder( wrwin, boxc[0], boxc[1], boxc[2], boxc[3], boxc[4], boxc[5], boxc[6], boxc[7] );
  wattron (wrwin, color);
  mvwprintw( wrwin, 0, 2, "[ %s ]", title );
  wattroff (wrwin, color);
  wrefresh( wrwin );
  wdrwin = derwin( wrwin, nlin - 2, ncol - 2, 1, 1);
  va_start( ap, fmt );

  switch (justify)
    {
    case 1:
      vsnprintf (str, sizeof (str), fmt, ap);
      i = 1;
      p = lastp = (char *)str;
      //      c = *p;
      while ( ( c = *p ) != '\0' )
	{
	  if (c == '\n') {
	    *p = '\0';
	    l = (p - lastp);
	    mvwprintw(wrwin, i++, (ncol - l - 1) / 2, "%s", lastp);
	    *p = c;
	    lastp = p+1;
	  }
	  p++;
	}
      if (p > lastp) {
	l = (p - lastp);
	mvwprintw(wrwin, i++, (ncol - l - 1) / 2, "%s", lastp);
      }
      break;
      
    case 2:
      vsnprintf (str, sizeof(str), fmt, ap);
      i = 1;
      p = lastp = (char *)str;
      //      c = *p;
      while ( ( c = *p ) != '\0' )
	{
	  if (c == '\n') {
	    *p = '\0';
	    l = (p - lastp);
	    mvwprintw(wrwin, i++, (ncol - l - 1), "%s", lastp);
	    *p = c;
	    lastp = p+1;
	  }
	  p++;
	}
      if (p > lastp) {
	l = (p - lastp);
	mvwprintw(wrwin, i++, (ncol - l - 1), "%s", lastp);
      }
      break;
    
    case 0:
    default:
      wmove( wrwin, 1, 1 );
      vw_printw( wrwin, fmt, ap );
      break;
    }
  va_end ( ap );
  wrefresh( wdrwin );
}

//
// ******************************************
// refresh_windows()
//
// refresh all visible windows
// ******************************************
static void refresh_windows()
{
  redrawwin( stdscr );
  redrawwin (main_window);
  refresh();
  redrawwin( wca );
  redrawwin( wst );
  redrawwin( wpo );
  if (issc) redrawwin( wsc );
  if (wrwin) redrawwin (wrwin);
  if (wdrwin) redrawwin (wdrwin);
  if ( ! panels[0].hide ) redrawwin( panels[0].w );
  if ( ! panels[1].hide ) redrawwin( panels[1].w );
  if ( ! panels[2].hide ) redrawwin( panels[2].w );
  wrefresh( wca );
  wrefresh( wst );
  wrefresh( wpo );
  if (issc) wrefresh( wsc );
  if ( ! panels[0].hide ) wrefresh( panels[0].w );
  if ( ! panels[1].hide ) wrefresh( panels[1].w );
  if ( ! panels[2].hide ) wrefresh( panels[2].w );
  update_panels();
  if (wrwin) wrefresh (wrwin);
  if (wdrwin) wrefresh (wdrwin);
}

// ******************************************
// unraise_window()
//
// close and delete the pop-up window and
// redraw the others
// ******************************************
static void unraise_window( void )
{
  delwin( wdrwin );
  werase( wrwin );
  wrefresh( wrwin );
  delwin( wrwin );
  wdrwin = wrwin = NULL;

  refresh_windows();
}

// ******************************************
// yesno_window (int color, char *str)
//
// yes-no popup
// ******************************************
static bool yesno_window (char *str)
{
  int ch;
  int nlin = 7, ncol = 40;
  int8_t yes = FALSE;
  int8_t leave = FALSE;
  int x1 = (x - ncol) / 2;
  //  int x2 = x1 + ncol;
  int y1 = (y - nlin) / 2;
  //  int y2 = y1 + nlin;

  wrwin = newwin( nlin, ncol, y1, x1 );
  wborder( wrwin, boxc[0], boxc[1], boxc[2], boxc[3], boxc[4], boxc[5], boxc[6], boxc[7] );
  mvwhline( wrwin, nlin - 3, 1, boxc[2], ncol - 2 );
  mvwaddch( wrwin, nlin - 3, 0, boxc[8] );
  mvwaddch( wrwin, nlin - 3, ncol -1 , boxc[9] );
  refresh();

  wattron (wrwin, COLOR_PAIR(CYAN));
  mvwprintw( wrwin, 0, 2, "[ Yes-No ]");
  wattroff (wrwin, COLOR_PAIR(CYAN));

  mvwprintw( wrwin, 1, (ncol - strlen (str)) / 2, "%s", str);
  wrefresh( wrwin );

  //  mvwprintw( wrwin, 0, 2, "[ Yes-No ]");
  mvwprintw( wrwin, nlin - 2, ( ncol / 2 ) - 8, "[     ]");
  mvwprintw( wrwin, nlin - 2, ( ncol / 2 ) + 4, "[     ]");
  wdrwin = derwin( wrwin, nlin - 2, ncol - 2, 1, 1);
  wrefresh( wdrwin );  

  ch = 0;
  do
    {
      switch (ch)
	{
	case 'Y':
	case 'y':
	  yes = TRUE;
	  leave = TRUE;
	  break;

	case 0x1b:
	case 'N':
	case 'n':
	  yes = FALSE;
	  leave = TRUE;
	  break;
	
	case KEY_LEFT:
	case KEY_RIGHT:
	case 0x09:
	  yes = !yes;
	  break;

	case 0x0A:
	  leave = TRUE;
	  break;

	default:
	  leave = FALSE;
	  break;
	}

      if (yes) wattron (wrwin, A_REVERSE);
      mvwprintw( wrwin, nlin - 2, ( ncol / 2 ) - 6, "%s", stryn[0]);  
      if (yes) wattroff (wrwin, A_REVERSE);
      if (!yes) wattron (wrwin, A_REVERSE);
      mvwprintw( wrwin, nlin - 2, ( ncol / 2 ) + 6, "%s", stryn[1]);  
      if (!yes) wattroff (wrwin, A_REVERSE);
      wrefresh( wrwin );

      if (! leave) ch = getch();
  
    } while(! leave);

  unraise_window();
  return yes;
}

// ******************************************
// add_display_hdr ()
//
// Add [ Display * ] header
// ******************************************
void add_display_hdr ()
{
  DISP_PANEL *temp0 = (DISP_PANEL *)panel_userptr(panels[0].panel);

  // removes header [Display ]
  mvwhline( main_window, 4, 1, boxc[2], (x / 2 ) - 2 );
  wattron ( main_window, COLOR_PAIR(YELLOW));
  if (temp0 == NULL)
    {
      mvwprintw ( main_window, 4, 2, "[ Display ]" );
      wattroff ( main_window, COLOR_PAIR(YELLOW));
      return;
    }

  if (temp0->hide) {
    mvwprintw ( main_window, 4, 2, "[ Display:%s ]", "MC00701A" );
    wattroff ( main_window, COLOR_PAIR(YELLOW));
  } else {
    if( -1 != fddp )
      mvwprintw ( main_window, 4, 2, "[ Display:%s ]", strdp );
    else
      mvwprintw ( main_window, 4, 2, "[ Display ]" );
    wattroff ( main_window, COLOR_PAIR(YELLOW));
  }
}

// ******************************************
// add_scope_hdr ()
//
// Add [ Scope ] or [Scope:file ] header
// ******************************************
void add_scope_hdr ()
{
  if( -1 == fdsc ) {
    wattron ( main_window, COLOR_PAIR(YELLOW));
    mvwprintw ( main_window, 4, (x / 2) + 2, "[ Scope ]" );
    wattroff ( main_window, COLOR_PAIR(YELLOW));
  } else {
    wattron ( main_window, COLOR_PAIR(YELLOW));
    mvwprintw ( main_window, 4, (x / 2) + 2, "[ Scope:%s ]", strsc );
    wattroff ( main_window, COLOR_PAIR(YELLOW));
  }
}

// ******************************************
// resize_panels ()
//
// resize panels for Display
// ******************************************
void resize_panels ()
{
  int x = COLS;
  int y = LINES;
  int left = (x / 2) - 1;
  int right = (x / 2) - 2 + (x % 2);

  DISP_PANEL *temp0 = (DISP_PANEL *)panel_userptr(panels[0].panel);
  DISP_PANEL *temp1 = (DISP_PANEL *)panel_userptr(panels[1].panel);
  DISP_PANEL *temp2 = (DISP_PANEL *)panel_userptr(panels[2].panel);
  werase (temp0->w);
  werase (temp1->w);
  werase (temp2->w);

if (!issc) {
  mvwaddch ( main_window, y - 1, left + 1, boxc[2] );
  mvwvline ( main_window, 5, x / 2, ' ', y - 6 );
  mvwaddch ( main_window, 4, left + 1, boxc[2] );

  wresize (temp0->w,  y - 6, x - 2 );
  wresize (temp1->w,  y - 6, x - 2 );
  //  wresize (temp2->w,  y - 6, right - 1 );
  
 } else {
  mvwaddch ( main_window, y - 1, left + 1, boxc[11] );
  mvwvline ( main_window, 5, left + 1, boxc[0], y - 6 );
  mvwaddch ( main_window, 4, left + 1, boxc[10] );

  wresize ( temp0->w, y - 6, left);
  wresize ( temp1->w, y - 6, left);
  mvwin   ( temp2->w, 5, left + 2);
  wresize ( temp2->w, y - 6, right);

  // Add Scope header
  if (issc) add_scope_hdr ();
 }
 
 replace_panel ( temp0->panel, temp0->w);
 replace_panel ( temp1->panel, temp1->w);
 replace_panel ( temp2->panel, temp2->w);

  refresh_windows();
}

// ******************************************
// sync_signals
//
// signal when windows resized
// ******************************************
/* Handle any signals received since last call. */
static void sync_signals()
{
  if (pending_usr1) {
    /* SIGUSR1 received: RFU. */
    pending_usr1 = 0;
  }
  if (pending_winch) {
    delete_subwindows();
    werase (main_window);
    delwin(main_window);
    endwin();
    clear();
    refresh();

    init_window();
    create_subwindows();
    resize_panels();
    refresh_windows();
    pending_winch = 0;
  }
}

static void handle_usr1(int sig)
{
    pending_usr1 = 1;
}

static void handle_winch(int sig)
{
    pending_winch = 1;
}

static void enable_handlers()
{
    struct sigaction sa;

    memset(&sa, 0, sizeof (struct sigaction));
    sa.sa_handler = handle_usr1;
    sigaction(SIGUSR1, &sa, NULL);
    sa.sa_handler = handle_winch;
    sigaction(SIGWINCH, &sa, NULL);
}

# if 0
static void disable_handlers()
{
    struct sigaction sa;

    memset(&sa, 0, sizeof (struct sigaction));
    sa.sa_handler = SIG_DFL;
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGWINCH, &sa, NULL);
}
#endif

// ******************************************
// DisplayMnemo(str)
//
// display a IL mnemo in the scope window
// ******************************************
void DisplayMnemo( char *mnemo )
{
  static int cpt = 0;

  if( issc )
    {
      if( -1 != fdsc )
	{
	  write( fdsc, mnemo, strlen( mnemo ) );
	  write( fdsc, "\n", 1 );
	}
      wprintw( wsc, " %s", mnemo );
      if( cpt++ == 3 )
	{
	  wprintw( wsc, "\n" );
	  cpt = 0;
	}
      wrefresh( wsc );
    }
}

// ******************************************
// ReadPILBox(&byt)
//
// try to read one byte from PILBox
// return 1 if read and byte read to &byt
// return 0 if timeout
// ******************************************
int ReadPILBox( char *byt )
{
  struct timeval tv = { 0, 50000 }; // 5ms
  fd_set         pilset;
  int            rc;

  FD_ZERO( &pilset );
  FD_SET( ilfd, &pilset );
  if( 1 == (rc = select( ilfd + 1, &pilset, NULL, NULL, &tv )) )
    read( ilfd, byt, 1 );
  else
    *byt = 0;

  return rc;
}

// ******************************************
// SendFrame(frame)
//
// send a IL frame to the PILBox
// ******************************************
static void SendFrame( int frame )
{
  int lbyt, hbyt;  // low/high bytes to serial
  int n;
  char buf[2];

  // build the low and high parts
  if( ! USE8BITS )
    {
    // use 7-bit characters for maximum compatibility
    hbyt = ((frame >> 6) & 0x1F) | 0x20;
    lbyt = (frame & 0x3F) | 0x40;
    }
  else
    {
    // 8-bit format for optimum speed
    hbyt = ((frame >> 6) & 0x1E) | 0x20;
    lbyt = (frame & 0x7F) | 0x80;
    }
  if( hbyt != lasth )
    {
    // send high part if different from last one
    lasth = hbyt;
    buf[0] = hbyt; buf[1] = lbyt; n = 2;
    }
  else
    {
    // otherwise send only low part
    buf[0] = lbyt; n = 1;
    }
  write( ilfd, buf, n );
}


// ******************************************
// PILBox(byt)
//
// manage the PILBox
// (based on Emu41 ilext2 module)
// called at each received byte
// ******************************************
static void PILBox( int byt )
{
  int frame; // IL frame

  if( (byt & 0xC0) == 0 )
    {
    if( byt & 0x20 )
      {
      // high byte, save it
      lasth = (int)byt & 0xFF;
      write( ilfd, "\r", 1 ); // acknowledge
      }
    }
  else
    {
    // low byte, build frame according to format
    if( byt & 0x80 )
      {
      frame = ((lasth & 0x1E) << 6) + (byt & 0x7F);
      }
    else
      {
      frame = ((lasth & 0x1F) << 6) + (byt & 0x3F);
      }

    frame = hpil_transmit( frame ); // transmit IL frame to internal virtual devices

    // send returned frame to PILBox
    SendFrame( frame );
    }
}

// ******************************************
// InitPILBox(cmd)
//
// init the PIL Box
// ******************************************
static int InitPILBox( int cmd )
{
  char byt;

  lasth = 0;
  SendFrame(cmd);
  if( 1 == ReadPILBox( &byt ) )
    if( (byt & 0x3F) == (cmd & 0x3F) )
      return 0;

  return -1;
}

// ******************************************
// init_term()
//
// ilper init_terminal
// ******************************************
void init_term ()
{
  setlocale(LC_ALL, "");

  if (!initscr()) {
    fprintf(stderr, "Error initialising ncurses.\n");
    exit(1);
  }
  clear();
  
  getmaxyx( stdscr, y, x );
  if( (24 > y) || (80 > x) )
    {
    endwin();
    fprintf( stderr,
             "Sorry, I need 24 lines and 80 row to run. %d lines, %d row\n", y, x );
    exit(1);
    }

  cbreak(); /* Get one character at a time. */
  timeout(100); /* For getch(). */
  noecho();
  //  nodelay (stdscr, TRUE);
  keypad( stdscr, TRUE );
  curs_set (0); /* Hide cursor */

  if (has_colors ()) {
    start_color();

    use_default_colors();

    /* Initialize all the colors */
    init_pair(RED, COLOR_RED, COLOR_BLACK);
    init_pair(GREEN, COLOR_GREEN, COLOR_BLACK);
    init_pair(BLUE, COLOR_BLUE, COLOR_BLACK);
    init_pair(CYAN, COLOR_CYAN, COLOR_BLACK);
    init_pair(YELLOW, COLOR_YELLOW, COLOR_BLACK);
    init_pair(MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(WHITE, COLOR_WHITE, COLOR_BLACK);
    init_pair(BLACK, COLOR_BLACK, COLOR_BLACK);
  }

#ifndef USEASCII
  (void)aboxc;
  boxc[0] = (chtype)ACS_VLINE;
  boxc[1] = (chtype)ACS_VLINE;
  boxc[2] = (chtype)ACS_HLINE;
  boxc[3] = (chtype)ACS_HLINE;
  boxc[4] = (chtype)ACS_ULCORNER;
  boxc[5] = (chtype)ACS_URCORNER;
  boxc[6] = (chtype)ACS_LLCORNER;
  boxc[7] = (chtype)ACS_LRCORNER;
  boxc[8] = (chtype)ACS_LTEE;
  boxc[9] = (chtype)ACS_RTEE;
  boxc[10] = (chtype)ACS_TTEE;
  boxc[11] = (chtype)ACS_BTEE;
#else
  (void)gboxc;
#endif

  atexit ((void (*)(void)) endwin);
  enable_handlers();
}

// ******************************************
// delete_subwindows()
//
// ilper delete sub windows (Disk, Device, Start/Stop)
// ******************************************
void delete_subwindows ()
{
  delwin ( wca );
  delwin ( wst );
  delwin ( wpo );
}

// ******************************************
// create_subwindows()
//
// ilper create sub windows (Disk, Device, Start/Stop)
// ******************************************
void create_subwindows ()
{
  // Mass Storage (nlines, ncols, begin_y, begin_x)
  wca = subwin(main_window, 1, x - (x / 2) - 12, 2, 7 );
  scrollok( wca, TRUE );
  mvwprintw( wca, 0, 0, "%s", strca ); // Disk
  wrefresh( wca );
  redrawwin( wca );

  // Pilbox link
  wpo = subwin (main_window, 1, x - (x / 2) - 27, 2, x - (x / 2) + 9 );
  scrollok ( wpo, TRUE );
  mvwprintw ( wpo, 0, 0, "%s", strpo ); // Device
  wrefresh ( wpo );
  redrawwin ( wpo );

  // Start-Stop
  wcur = wst = subwin (main_window, 1, 7, 2, x - 11 );
  wattron ( wst, A_REVERSE + (ilst ? COLOR_PAIR(GREEN) : COLOR_PAIR(RED)));
  mvwprintw ( wst, 0, 0, "%s", wstr = strst[ilst] );
  wrefresh ( wst );
  redrawwin ( wst );
}

// ******************************************
// init_window()
//
// ilper create main window
// ******************************************
void init_window ()
{
  y = LINES;
  x = COLS;
  main_window = subwin(stdscr, LINES, COLS, 0, 0);

    // main border
  wborder ( main_window, boxc[0], boxc[1], boxc[2], boxc[3], boxc[4], boxc[5], boxc[6], boxc[7] );
  mvwprintw ( main_window, 0, (x - 10 - strlen( strvers )) / 2, "[ ILPER %s ]", strvers );

  // main footer
  wattron ( main_window, COLOR_PAIR(CYAN));
  mvwprintw ( main_window, y - 1, 2, "[ %s ]", footer );
  if ( ( y < 24 ) || ( x < 80 ) ) {
    wattroff ( main_window, COLOR_PAIR(CYAN));
    wattron ( main_window, A_REVERSE + COLOR_PAIR(RED));
    mvwprintw ( main_window, y - 1, x - 19, "[ Term: %dx%d ]", x, y );
    wattroff ( main_window, A_REVERSE + COLOR_PAIR(RED));
  } else {
    mvwprintw ( main_window, y - 1, x - 19, "[ Term: %dx%d ]", x, y );
    wattroff ( main_window, COLOR_PAIR(CYAN));
  }
  // Display and Scope line
  mvwhline ( main_window, 4, 1, boxc[2], x - 2 );
  mvwaddch ( main_window, 4, 0, boxc[8] );
  mvwaddch ( main_window, 4, x - 1, boxc[9] );

  /* LIF rectangle */
  wattron ( main_window, COLOR_PAIR(YELLOW) );
  mvwhline ( main_window, 1, 3, boxc[2], x - (x / 2) - 6 );
  mvwhline ( main_window, 3, 3, boxc[2], x - (x / 2) - 6 );
  mvwvline ( main_window, 2, 2, boxc[0], 1 );
  mvwvline ( main_window, 2, x - (x / 2) - 3, boxc[0], 1 );
  mvwaddch ( main_window, 1, x - (x / 2) - 3, boxc[5] );
  mvwaddch ( main_window, 3, x - (x / 2) - 3, boxc[7] );
  mvwaddch ( main_window, 1, 2, boxc[4] );
  mvwaddch ( main_window, 3, 2, boxc[6] );
  mvwprintw( main_window, 1, 4, "[ Mass Storage LIF File ]" );

  /* PILBOX link rectangle */
  mvwhline ( main_window, 1, x - (x / 2) + 2, boxc[2], (x / 2) - 16 );
  mvwhline ( main_window, 3, x - (x / 2) + 2, boxc[2], (x / 2) - 16 );
  mvwvline ( main_window, 2, x - (x / 2) + 1, boxc[0], 1 );
  mvwvline ( main_window, 2, x - 15, boxc[0], 1 );
  mvwaddch ( main_window, 1, x - 15, boxc[5] );
  mvwaddch ( main_window, 3, x - 15, boxc[7] );
  mvwaddch ( main_window, 1, x - (x / 2) + 1, boxc[4] );
  mvwaddch ( main_window, 3, x - (x / 2) + 1, boxc[6] );
  mvwprintw (main_window, 1, x - (x / 2) + 3, "[ PILBox Link ]" );
  wattroff ( main_window, COLOR_PAIR(YELLOW));

  add_display_hdr();
  
  // LIF, Device ans Start/stop window header
  mvwprintw ( main_window, 2, 3, "LIF: " );
  mvwprintw ( main_window, 2, x - (x / 2) + 2, "Device: " );
  mvwprintw ( main_window, 2, x - 13, "[         ]" );

  refresh ();
}

// ******************************************
// create_panels ()
//
// creates panels for Display
// ******************************************
void create_panels ()
{
  int x = COLS;
  int y = LINES;
  int left = (x / 2) - 1;
  int right = (x / 2) - 2 + (x % 2);
  
  // ==============
  // Display panels
  // MC00701A
  wdv = newwin( y - 6, x - 2, 5, 1 );
  scrollok( wdv, TRUE );
  initDisplay(wdv); // init MC00701A display

  //DISPLAY
  wdp = newwin( y - 6, x - 2, 5, 1 );
  scrollok( wdp, TRUE );

    // Scope
  wsc = newwin ( y - 6, right, 5, left + 2 );
  scrollok ( wsc, TRUE );

   /* Attach a panel to each window */ 	/* Order is bottom up */
  panels[0].panel = new_panel(wdp); 	/* Push 0, order: stdscr-0 */
  panels[0].w = wdp;
  panels[1].panel = new_panel(wdv); 	/* Push 1, order: stdscr-0-1 */
  panels[1].w = wdv;
  panels[2].panel = new_panel(wsc); 	/* Push 2, order: stdscr-0-1-2 */
  panels[2].w = wsc;

  /* Initialize panel datas saying that nothing is hidden */
  panels[0].hide = FALSE;
  panels[1].hide = TRUE;
  panels[2].hide = TRUE;
  hide_panel (panels[1].panel);
  hide_panel (panels[2].panel);
  
  set_panel_userptr(panels[0].panel, &panels[0]);
  set_panel_userptr(panels[1].panel, &panels[1]);
  set_panel_userptr(panels[2].panel, &panels[2]);
  
  /* Update the stacking order. 2nd panel will be on top */
  update_panels();
}

// ******************************************
// main()
//
// ilper main
// ******************************************
int main(int argc, char **argv)
{
  bool quit = FALSE;
  int  c;

  (void)argc; (void)argv;

  if (argc >= 2) {
    char *progname = basename (argv[0]);

    if (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version")) {
      printf("%s %s\n", progname, ilper_vers);
      return 0;
    } else if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
      printf(
	     "Usage: %s\n"
	     "       Launches the program.\n\n"
	     "  or:  %s -h|--help\n"
	     "       Print this help message and exit.\n\n"
	     "  or:  %s -v|--version\n"
	     "       Print program version and exit.\n\n"
	     "See %s(1) for more information.\n"
	     "Ilper homepage: <https://github.com/johnvax/ilper-linux>.\n",
	     basename(argv[0]),
	     basename(argv[0]),
	     basename(argv[0]),
	     basename(argv[0])
	     );
      return 0;
    }
  }
  
  strcpy( strwd, getenv("HOME"));	// Defaut is Current Directory
  if (strwd[strlen(strwd) - 1] != '/')
    strcat(strwd, "/");
  strcpy( strca, ILDISK ); 		// PILBOX Disk
  strcpy( strpo, ILDEVICE ); 		// PILBOX device

  char *p;

  strncpy( strvers, ilper_vers, sizeof(strvers) );
  if( NULL != (p = strchr( strvers, ' ' )) )
    *p = '\0';

  init_term ();        // init terminal 
  init_window ();      // create window content
  create_subwindows(); // Create Disk, Device and Button windows

  // Panels
  create_panels ();
  refresh_windows();

  while( !quit && (c = getch()))
    {
      if( (ERR != c) && (NULL != wrwin) )
	unraise_window();
      else if ( c == ERR)
	sync_signals ();
      else if ( (c == 'q') || (c == 'Q') )
      {
	quit = yesno_window ("Do you want to exit ?");
      }
      else if  ( !ilst && (0x161 == c) )	// SHIFT + TAB
	{
	  wattroff( wcur, A_REVERSE );
	  mvwprintw( wcur, 0, 0, "%s", wstr );
	  wrefresh( wcur );
	  wcur = ( wcur == wca ? wst : wcur == wpo ? wca : wpo );
	  wstr = ( wstr == strca ? strst[ilst] : wstr == strpo ? strca : strpo );
	  wattron( wcur, A_REVERSE );
	  mvwprintw( wcur, 0,0,"%s", wstr );
	  wrefresh( wcur );
	}
      else if ( !ilst && (0x09 == c) ) 		// TAB
	{
	  wattroff( wcur, A_REVERSE );
	  mvwprintw( wcur, 0, 0, "%s", wstr );
	  wrefresh( wcur );
	  wcur = ( wcur == wca ? wpo : wcur == wpo ? wst : wca );
	  wstr = ( wstr == strca ? strpo : wstr == strpo ? strst[ilst] : strca );
	  wattron( wcur, A_REVERSE );
	  mvwprintw( wcur, 0,0,"%s", wstr );
	  wrefresh( wcur );
	}
      else if ( ! ilst && ( 'p' == c ))
	{
	  wattroff( wcur, A_REVERSE );
	  mvwprintw( wcur, 0, 0, "%s", wstr );
	  wrefresh( wcur );
	  wcur = wpo;
	  wstr = strpo;
	  wattron( wcur, A_REVERSE );
	  mvwprintw( wcur, 0,0,"%s", wstr );
	  wrefresh( wcur );
	}
      else if (! ilst && (( 'l' == c ) || ( 'L' == c )))
	{
	  wattroff( wcur, A_REVERSE );
	  mvwprintw( wcur, 0, 0, "%s", wstr );
	  wrefresh( wcur );
	  wcur = wca;
	  wstr = strca;
	  wattron( wcur, A_REVERSE );
	  if (! ilst && ( 'L' == c ) )
	    {
	      werase (wcur);
	      select_file (main_window);
	      refresh_windows ();
	    }
	  mvwprintw( wcur, 0,0,"%s", wstr );
	  wrefresh( wcur );
	}
      else if ( 'd' == c )
	{
	  DISP_PANEL *temp0 = (DISP_PANEL *)panel_userptr(panels[0].panel);
	  DISP_PANEL *temp1 = (DISP_PANEL *)panel_userptr(panels[1].panel);

	  if (temp0->hide) {		// temp0 is Printer View
    	    temp0->hide = FALSE;
	    show_panel (temp0->panel);
	    temp1->hide = TRUE;		// temp1 is MC00701A view
	    hide_panel (temp1->panel);
	  } else {
	    temp0->hide = TRUE;
	    hide_panel (temp0->panel);
	    temp1->hide = FALSE;
	    show_panel (temp1->panel);
	  }

	  add_display_hdr ();
	  refresh_windows ();
	}
      else if( ('s' == c) || ('S' == c) )
	{
	  DISP_PANEL *temp2 = (DISP_PANEL *)panel_userptr(panels[2].panel);
	  // Remove the "[ Scope ]" header
	  mvwhline ( main_window, 4, (x / 2) + 1, boxc[2], (x / 2 ) - 2 );
	  if( 's' == c )
	    {
	      if ( issc ) {
		panels[2].hide = TRUE;
		hide_panel (temp2->panel);
	      } else {
		werase( wsc );
		panels[2].hide = FALSE;
		show_panel (temp2->panel);
	      }
	      issc = issc ? FALSE : TRUE;
	    }

	  if( !issc )
	    {
	      if( -1 != fdsc )
		close( fdsc );
	      fdsc = -1;
	    }
	  else
	    {
	      if( ('S' == c) )
		{
		  if( -1 != fdsc )
		    {
		      close( fdsc );
		      fdsc = -1;
		    }
		  else
		    {
		      fdsc = open( strsc, O_CREAT | O_TRUNC | O_WRONLY, 0644 );
		      if( -1 == fdsc )
			{
			  raise_window( 4, 76, "Error", COLOR_PAIR(RED) + A_BLINK, 1,
					"Can not open '%s': %m\n Log is disabled", strsc );
			}
		    }
		}
	    }
	  // create scope header
	  if (issc) add_scope_hdr ();
	  /* resize display */
	  resize_panels();
	  refresh_windows();
	  wrefresh( wcur );
	}
      else if( 'D' == c )
	{
	  if ( ! panels[0].hide ) // Default (PRINTER) Display
	    {
	      if( -1 != fddp )
		{
		  close( fddp );
		  fddp = -1;

		  mvwhline ( main_window, 4, 1, boxc[2], (x / 2 ) - 2 );
		  wattron ( main_window, COLOR_PAIR(YELLOW));
		  mvwprintw ( main_window, 4, 2, "[ Display ]" );
		  wattroff ( main_window, COLOR_PAIR(YELLOW));
		}
	      else
		{
		  fddp = open( strdp, O_CREAT | O_TRUNC | O_WRONLY, 0644 );
		  if( -1 == fddp )
		    {
		      raise_window( 4, 76, "Error", COLOR_PAIR(RED) + A_BLINK, 1,
				    "Can not open '%s': %m\n Log is disabled", strdp );
		    }
		  else {
		    wattron ( main_window, COLOR_PAIR(YELLOW));
		    mvwprintw ( main_window, 4, 2, "[ Display:%s ]", strdp );
		    wattroff ( main_window, COLOR_PAIR(YELLOW));
		  }
		}
	    }
	  refresh_windows ();
	  wrefresh( wcur );
	}
      else if( 0x0A == c || ('o' == c || 'a' == c) )
	{
	  if( ('o' == c || 'a' == c) && (wcur != wst) )
	    {
	      wattroff( wcur, A_REVERSE );
	      mvwprintw( wcur, 0, 0, "%s", wstr );
	      wrefresh( wcur );
	      wcur = wst;
	    }
	  if( wcur == wst )
	    {
	      ilst = ( 0x0A == c ? ( ilst ? FALSE : TRUE )
		       : ( 'o' == c ? FALSE : TRUE ) );
	      if( ilst ) // START
		{
		  if( -1 == ilfd )
		    {
		      ilfd = open( strpo, O_RDWR );
		      if( 0 <= ilfd )
			{
			  struct termios tp;
			
			  memset( &tp, 0, sizeof(tp) );
			  tp.c_cflag = B115200 | CS8 | CREAD;
			  tcsetattr( ilfd, TCSANOW, &tp );
			  if( -1 == InitPILBox( COFF ) )
			    {
			      raise_window( 3, 25, "Error", COLOR_PAIR(RED) + A_BLINK, 1,
					    "No response from PILBOX" );
			      ilst = 0;
			      close( ilfd );
			    }
			}
		      if( -1 == ilfd )
			{
			  raise_window( 3, 76, "Error", COLOR_PAIR(RED) + A_BLINK, 1,
					"Can not open %s: %m", strpo );
			  ilst = 0;
			}
		      if( !ilst )
			{
			  if( -1 != ilfd )
			    close( ilfd );
			}
		      else
			{
			  nodelay( stdscr, TRUE );
			  werase( wsc );
			  ClrDisplayP();
			  ClrDisplayV();
			  refresh_windows ();
			  init_hpil();
			}
		    }
		}
	      else // STOP
		{
		  nodelay( stdscr, FALSE ); 	// 
		  timeout (100);		// non-blocking with 100ms

  //		  refresh_windows();
		  if( -1 != ilfd )
		    {
		      InitPILBox( TDIS );
		      close( ilfd );
		      ilfd = -1;
		    }
		}
	      curs_set( (ilst ? 2 : 0)); /* Hide or show blinking cursor. */
	      werase( wcur );
	      wattron( wcur, A_REVERSE + (ilst ? COLOR_PAIR(GREEN) : COLOR_PAIR(RED)));
	      mvwprintw( wcur, 0, 0, "%s", wstr = strst[ilst] );
	      refresh_windows();
	    }
	  else
	    {
	      echo();
	      wattroff( wcur, A_REVERSE );
	      wattron( wcur, A_BOLD | A_UNDERLINE );
	      werase( wcur );
	      wrefresh( wcur );
	      mvwscanw( wcur, 0, 0, "%s", wstr );
	      wattron( wcur, A_REVERSE );
	      wattroff( wcur, A_BOLD | A_UNDERLINE );
	      mvwprintw(wcur, 0, 0, "%s", wstr );
	      wrefresh( wcur );
	      noecho();
	    }
	}
      else if( 'v' == c )
	{
	  raise_window( 5, 76, "Version", COLOR_PAIR(CYAN), 1,
			"ILPer Version %s for %s\nCompiled %s\n %s\n",
			ilper_vers, ilper_os, ilper_date, copyright );
	
	}
      else if( 'h' == c )
	{
	  raise_window_r( 14, 77, "Help", help);
	}
      if( ilst )
	{
	  char byt;
	
	  if( 1 == ReadPILBox( &byt ) )
	    PILBox( (int)byt & 0xFF );
	}
      c = 0;
    }

  if( -1 != ilfd )
    {
      InitPILBox( TDIS );
      close( ilfd );
    }
  if( -1 != fdsc )
    close( fdsc );
  if( -1 != fddp )
    close( fddp );
  
  erase();
  refresh();
  endwin();
  
  return 0;
}

