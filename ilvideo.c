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
// ------------------------------------------------------------------------------
//                                            
// ilvideo.c   HP-IL video MC00701A module
//                                            
// Based on ildisp.c work:           
// 2026: created on linux by JM Vansteene
// ------------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ilper.h"

    // HP-IL data and variables */
#define AID  50     // Accessory ID = printer
#define DEFADDR  31  // default address after AAU
static char *did = "MC00701A\r\n";  // Device ID 
static int status;  // HP-IL status (always 0 here)
static int adr;     // bits 0-5 = HP-IL address, bit 7 = 1 means auto address taken 
static int fvideo;  // HP-IL state machine flags: 
    // bit 7, bit 6:
    // 0      0       idle
    // 1      0       addressed listen
    // 0      1       addressed talker 
    // 1      1       active talker
    // ( this choice comes from my original 6502 assembly code [PLTERx and ILPERx applications, 1984-1988]
    //   that used the efficient BIT opcode to test both bit 6 and 7)
static int ptsdi;   // output pointer for device ID 

// Display
static int16_t m_nBufRows, m_nBufCols, m_nVisCols, m_nVisRows;
static int16_t m_xCurPos, m_yCurPos;
static int8_t m_nEsc, m_nBufLine;
static int8_t m_bAutoCrlf;
static char  *m_pbyBuffer;
#define CHR_AT(x,y)(m_pbyBuffer[(m_nBufLine + (y)) * m_nVisCols + (x)])

extern DISP_PANEL panels[];	// panels[1] is wdv (MC00701A)

//
// Init Display (MC00701A)
//
void initDisplay (WINDOW *w)
{
  getmaxyx(w, m_nBufRows, m_nBufCols);
  m_nVisCols = m_nBufCols;
  m_nVisRows = m_nBufRows;
  
  m_xCurPos = m_yCurPos = 0;
  m_nEsc = 0;
  m_nBufLine = 0;
  m_bAutoCrlf = FALSE;

  m_pbyBuffer = (char *) malloc (m_nBufCols * m_nBufRows);
  if (m_pbyBuffer)  memset ((char *)m_pbyBuffer, ' ', m_nBufCols * m_nBufRows);
  else {
    endwin ();
    fprintf(stderr, "Error initialising video buffer.\n");
    exit (-1);
  }
}

//
// Set Cursor Position on Display window
//
void SetCursorPos (WINDOW *w, int x, int y)
{
  m_xCurPos = x;
  m_yCurPos = y;
  
  wmove (w, y, x);
}

//
// move cursor left
//
void CursorLeft (WINDOW *w)
{
  int16_t x = m_xCurPos;
  int16_t y = m_yCurPos;

  // nove cursor left
  if (x-- <= 0)			// already at first column
	{
	  x  = m_nVisCols - 1; 	// goto last column
	  
	  // move cursor up
	  if (y-- <= 0)		// already at first row
	    {
	      x = y = 0;	// set cursur to (0,0)
	    }
	}
  SetCursorPos (w, x, y);
  return;
}

//
// move cursor right
//
void CursorRight(WINDOW *w)
{
  int16_t x = m_xCurPos;
  int16_t y = m_yCurPos;
  
  if (x++ == m_nVisCols - 1)
    {
      x = 0;
      if (y++ == m_nVisRows - 1)
	y = 0;
    }
  SetCursorPos (w, x, y);
  return;
}

//
// clrScr
// Clear current window
void clrScr (WINDOW *w)
{
  werase (w);
  m_xCurPos = m_yCurPos = 0;			// cursor at (0,0)
}

// 
// ClrVideo()
// clear the display box
// 
void ClrVideo( void )
{
  clrScr( panels[1].w );
  wrefresh( panels[1].w );
}

//
// clrScrFrom
// Clear current window from cursor point to end of window
void clrScrFrom (WINDOW *w)
{
  wclrtobot (w);
  //  wrefresh(w);
}

//
// Soft reset
// empty display & screen buffer and show replace cursor at (0,0)
//
void SoftReset(WINDOW *w)
{
  // empty display buffer
  memset ((char *)m_pbyBuffer, ' ', m_nBufCols * m_nBufRows);
  m_nBufLine = 0;							// view display buffer from top

  clrScr (w);
  return;
}

///
// Device Clear
// empty display & screen buffer and show replace cursor at (0,0)
//
void ClearDevice(WINDOW *w)
{
  SoftReset(w);				// then do the soft reset
  return;
}

//
// eval line feed
//
void EvalLF(WINDOW *w)
{
  if (++m_yCurPos == m_nVisRows)	// end of screen
    {
      m_yCurPos = m_nVisRows - 1; 	// set cursor to last line
      scroll(w);   	          	//
    }
  return;
}

//
// synchronize screen with display memory buffer
//
void ScrollScreen()
{
  //  vertScroll ();
  if (m_nBufLine++ == (m_nBufRows- m_nVisRows)) {
    m_nBufLine = 0;
    memset ((char *)m_pbyBuffer, ' ', m_nBufCols * m_nBufRows);
  }
}

// ******************************************
// printChar (w, ch)
//
// Print char on Screen and writes in file
// and internal Buffer
// ******************************************
void printChar (WINDOW *w, char ch)
{
  if( (160 <= ((int)ch & 0xFF)) && (254 >= ((int)ch & 0xFF)) )
    wattron( w, A_REVERSE );
  wprintw( w, "%c", (ch & 0x7F) );
  if( (160 <= ((int)ch & 0xFF)) && (254 >= ((int)ch & 0xFF)) )
    wattroff( w, A_REVERSE );
  
  CHR_AT (m_xCurPos, m_yCurPos) = ch;	
}

// ******************************************
// VideoStr(ch)
//
// display a character in the MC00701A window
// convert some HP41 character
// ******************************************
void VideoStr( char ch )
{
  int i, j;

  if (panels[1].hide)
    return;

  if( m_nEsc == 0 )
{
    switch( (int)ch & 0xFF )
      {
      // convert special HP41 characters to regular ASCII
      case 0:
	ch = '*';
	break;
	
      case 8:
	CursorLeft(panels[1].w);
	m_bAutoCrlf = FALSE;
	break;

      case 10:
	ch = '\0';
	if (!m_bAutoCrlf)		// not evaluated automatic CR LF
	  {
	    EvalLF(panels[1].w);	// eval LF
	  }
	m_bAutoCrlf = FALSE;		// reset automatic CR LF flag
	break;
      case 12: ch = 'u'; break;   // micron
      case 13:
	m_xCurPos = 0;
	ch = '\0';
	break;
      case 28: ch = 's'; break;   // sigma
      case 27: m_nEsc = 1; break;   // escape sequences
      case 29: ch = '#'; break;   // different
      case 124: ch = 'a'; break;  // angle sign
      case 127: ch = '`'; break;  // append
      default:
	if( (((int)ch & 0xFF) > 127) &&
	    ((((int)ch & 0xFF) < 160) || (((int)ch & 0xFF) == 255)) )
	  {
	    ch = '.';  // non-printable characters
	  }
	break;
      }
    if( ( m_nEsc == 0 ) && ch )
      {
	printChar (panels[1].w, ch);

	// move cursor forward : end of column?
	m_bAutoCrlf = FALSE;
	if (++m_xCurPos == m_nVisCols)
	  {
	    m_xCurPos = 0;       // eval CR
	    EvalLF(panels[1].w); // eval LF
	    m_bAutoCrlf = TRUE;  // evaluated automatic CR LF
	  }
      }
    }
  else
    {
      if (m_nEsc > 0)						// escape sequence
	{
	  --m_nEsc;						// handled ESC sequence

	  switch (m_nEsc)
	    {
	    case 0: 						// single ESC
	      switch ( (int)ch & 0xFF)
		{
		case 0:
		  break;

		case '<': // 0x3C
		  curs_set (0);
		  break;
		  
		case '>':  // 0x3E
		  curs_set (1);
		  break;
		  
		case 'A': // Cursor Up
		  if (m_yCurPos-- <= 0)		// move cursor up
		    m_yCurPos = 0;
		  break;
		  
		case 'B': // Cursor Down
		  if (m_yCurPos++ == m_nVisRows - 1)
		    m_yCurPos = m_nVisRows - 1;
		  break;

		case 'C': // Cursor Right
		  CursorRight (panels[1].w);
		  break;
		  break;
		  
		case 'D': // 0x44 ; Cursor Left
		  CursorLeft(panels[1].w);
		  break;

		case 'E': // 0x45 ; Device Clear
		  SoftReset(panels[1].w);
		  break;

		case 'H': // 0x48 ; Cursor Home
		  m_xCurPos = m_yCurPos = 0;
		  break;
		  
		case 'J': // Clear screen memory from cursor to end of page
		  clrScrFrom(panels[1].w);
		  // then clear the lines after current position
		  for (j = m_yCurPos; j < m_nVisRows; ++j)
		    {
		      for (i = m_xCurPos; i < m_nVisCols; ++i)
			{
			  CHR_AT(i,j) = ' ';
			}
		      i = 0;			// clear all other lines from beginning
		    }
		  break;

		case 'K':	// Clear screen memory from cursor to end of line
		  for (i = m_xCurPos; i < m_nVisCols; ++i)
		    {
		      CHR_AT(i,m_yCurPos) = ' ';
		    }
		  wclrtoeol (panels[1].w);
		  break;

		case 'N': // Enable insert character mode
//		  m_eCurState = CUR_INSERT;
//		  m_bInsertMode = true;// enable insert character mode
		  curs_set (1);
		  break;

		case 'O': //Delete Character
		  for (i = m_xCurPos; i < m_nBufCols - 1; ++i)
		    {
		      printChar (panels[1].w, CHR_AT (i+1,m_yCurPos));
		    }
		  printChar (panels[1].w, ' ');
		  break;
		  
		case 'Q': // Switch to insert cursor, but do not enable insert mode
//		  m_eCurState = CUR_INSERT;
		  curs_set (1);
		  break;
		  
		case 'R': // 0x52 ; Disable Insert Character Mode (Default.)
//		  m_eCurState = CUR_REPLACE;
//		  m_bInsertMode = false;   // disable insert character mode
		  curs_set (2);
		  break;

		case 'S': // Scroll up (move window down)
		  break;

		case 'T': // Scroll down (move window up)
		  break;
		  
		case '%': // Cursor To Address
		  m_nEsc = 3;					// still need two arguments, fetch column next
		  break;

		case 'e': // Hard reset
		  ClearDevice(panels[1].w);
		  break;

		default: // illegal ESC sequence
		  break;					// ignore sequence
		}
	      break;

	    case 2: // Cursor To Address, m column argument
	      m_xCurPos = ch % m_nVisCols;
	      m_nEsc = 2;						// still need one argument, fetch row next
	      break;
	    case 1:  // Cursor To Address, n row argument
	      m_yCurPos = ch % m_nVisRows;
	      m_nEsc = 0;						// handled ESC sequence
	      break;

	    default:
	      break;		
	    }
	  m_bAutoCrlf = FALSE;
	}
    }
  
  SetCursorPos (panels[1].w, m_xCurPos, m_yCurPos);
  wrefresh( panels[1].w );
}

// ****************************************** 
// init_ilvideo()                           
//                                            
// init the virtual HPIL display device       
// ****************************************** 
void init_ilvideo( void )
{
  status = 0;
  adr = 0;
  fvideo = 0;
  ptsdi = 0;
}

// ****************************************** 
// traite_doe(frame)                          
//                                            
// manage the HPIL data frames                
// returns the returned frame                 
// ****************************************** 
static int traite_doe( int frame )
{
  int  n;
  char c;

  n = frame & 255;
  if( fvideo & 0x80 )
    {
    // addressed 
    if( fvideo & 0x40 )
      {
      // talker 
      if( ptsdi > 0 )
        {
	  c = did[ptsdi - 1];
	  frame = (int)c & 0xFF;
	  if( c == '\r' )
	    {
	      ptsdi = 0;
	    }
	  else
	    {
	      ptsdi = ptsdi + 1;
	    }
        }
      if( ptsdi == 0 )
        {
	  frame = 0x540;  // EOT 
	  fvideo = 0x40;
        }
      }
    else
      {
	// listener 
	VideoStr( (char)n );
      }
    }
  return frame;
}

// ****************************************** 
// traite_cmd(frame)                          
//                                            
// manage the HPIL command frames             
// returns the returned frame                 
// ****************************************** 
static int traite_cmd( int frame )
{
  int n;

  n = frame & 255;
  switch( n >> 5 )
    {
    case 0:
      switch( n )
	{
	case 4: // SDC 
	  if( fvideo & 0x80 )
	    {
	      ClrVideo();
	    }
	  break;
	case 20: // DCL 
	  ClrVideo();
	  break;
	}
    case 1: // LAD 
      n = n & 31;
      if( n == 31 )
	{
	  // UNL, if not talker then go to idle state 
	  if( (fvideo & 0x40) == 0 )
	    {
	      fvideo = 0;
	    }
	}
      else
	{
	  // else, if MLA go to listen state 
	  if( n == (adr & 31) )
	    {
	      fvideo = 0x80;
	    }
	}
      break;
    case 2: // TAD 
      n = n & 31;
      if( n == (adr & 31) )
	{
	  // if MTA go to talker state 
	  fvideo = 0x40;
	}
      else
	{
	  // else if addressed talker, go to idle state 
	  if( (fvideo & 0x40) != 0 )
	    {
	      fvideo = 0;
	    }
	}
      break;
    case 4:
      n = n & 31;
      switch( n )
	{
	case 16: // IFC 
	  fvideo = 0;
	  break;
	case 26: // AAU 
	  adr = DEFADDR;
	  break;
	}
    }
  
    return frame;
}

// ******************************************
// traite_rdy(frame)                          
//                                            
// manage the HPIL ready frames               
// returns the returned frame                 
// ****************************************** 
static int traite_rdy( int frame )
{
  int n;

  n = frame & 255;
  if( n <= 127 )
    {
    // sot 
    if( fvideo & 0x40 )
      {
      // if addressed talker 
      if(n == 66)
        {
        // NRD 
        ptsdi = 0; // abort transfer 
        }
      else if( n == 96 )
        {
        // SDA 
        // no response 
        }
      else if( n == 97 )
        {
        // SST 
        frame = status;
        fvideo = 0xC0; // active talker
        }
      else if( n == 98 )
        {
        // SDI 
        frame = (int)did[0] & 0xFF;
        ptsdi = 2;
        fvideo = 0xC0;
        }
      else if( n == 99 )
        {
        // SAI 
        frame = AID;
        fvideo = 0xC0;
        }
      }
    }
  else if( n < 0x9E )
    {
    // AAD, if not already an assigned address, take it 
    if( (adr & 0x80) == 0 )
      {
      adr = n;
      frame = frame + 1;
      }
    }

  return frame;
}

// ****************************************** 
// ildisplay(frame)                           
//                                            
// manage a HPIL frame                       
// returns the returned frame                 
// ****************************************** 
int ILvideo( int frame )
{
  if( (frame & 0x400) == 0 )
    {
    frame = traite_doe( frame );
    }
  else if( (frame & 0x700) == 0x400 )
    {
    frame = traite_cmd( frame );
    }
  else if( (frame & 0x700) == 0x500 )
    {
    frame = traite_rdy(frame);
    }
  return frame;
}

