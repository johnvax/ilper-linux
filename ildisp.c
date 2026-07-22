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
// ildisp.c   HP-IL display module           
// (used in Emu41/Emu71/ILper)                
//                                            
// Based on previous personal work:           
// 1986: ILPER4 video+disc, (6502 assembler)  
// 1988: ILPER5 videoil module                
// 1993: ILPER ported on PC (8086 assembler)  
// 1997: rewriten in C and included in Emu41  
// 2008: rewriten in VB for the standalone ILPER Windows version using the PILBox!
// oct 2009: changed the AID to 46 (for general 'dumb// printer)
// 2011: ported on Linux by Ch. Gottheimer
// 2026: updated on linux by JM Vansteene
// ------------------------------------------------------------------------------

#include <stdio.h>
#include <unistd.h>
#include "ilper.h"

    // HP-IL data and variables */
#define AID  46		// Accessory ID = printer
#define DEFADDR  3	// default address after AAU
static char *did = "DISPLAY\r\n";  // Device ID 
static int status;	// HP-IL status (always 0 here)
static int addr;	// bits 0-5 = HP-IL address AAD or AEP, bit 7 = 1 means auto address taken 
static int fvideo;	// HP-IL state machine flags: 
    // bit 7, bit 6:
    // 0      0       idle
    // 1      0       addressed listen
    // 0      1       addressed talker 
    // 1      1       active talker
    // ( this choice comes from my original 6502 assembly code [PLTERx and ILPERx applications, 1984-1988]
    //   that used the efficient BIT opcode to test both bit 6 and 7)
static int ptsdi;	// output pointer for device ID
static int ptssi;	// output pointer for HP-IL status
static int talkerFrame;	// last talker frame

extern int fddp;

// ******************************************
// ClrDisplayP()
//
// clear the display box
// ******************************************
void ClrDisplayP( void )
{
  werase ( panels[0].w );
  wrefresh ( panels[0].w );
}

// ******************************************
// DisplayStr(ch)
//
// display a character in the DISPLAY window
// convert some HP41 character
// ******************************************
void DisplayStr( char ch )
{
  static int fesc = 0;

  if (panels[0].hide)
    return;
  
  if( fesc == 0 )
    {
    switch( (int)ch & 0xFF )
      {
      // convert special HP41 characters to regular ASCII
      case 0: ch = '*'; break;
      case 12: ch = 'u'; break;   // micron
      case 13: ch = '\0'; break;
      case 28: ch = 's'; break;   // sigma
      case 27: fesc = 1; break;   // escape sequences
      case 29: ch = '#'; break;   // different
      case 124: ch = 'a'; break;  // angle sign
      case 127: ch = '+'; break;  // append
      default: if( (((int)ch & 0xFF) > 127) &&
                    ((((int)ch & 0xFF) < 160) || (((int)ch & 0xFF) == 255)) )
	  {
	    ch = '.';  // non-printable characters
	  }
	break;
      }
    if( fesc == 0 )
      {
	if( ch )
	  {
	    if( -1 != fddp )
	      {
		char dp = (ch & 0x7F);
		write( fddp, &dp, sizeof(dp) );
	      }
	    if( (160 <= ((int)ch & 0xFF)) && (254 >= ((int)ch & 0xFF)) )
	      wattron( panels[0].w, A_REVERSE );
	    wprintw( panels[0].w, "%c", (ch & 0x7F) );
	    if( (160 <= ((int)ch & 0xFF)) && (254 >= ((int)ch & 0xFF)) )
	      wattroff( panels[0].w, A_REVERSE );
	    wrefresh( panels[0].w );
	  }
      }
    }
  else
    {
      fesc = 0; // ignore escape sequences (for the HP71)
    }
}

// ****************************************** 
// init_ildisplay()                           
//                                            
// init the virtual HPIL display device       
// ****************************************** 
void init_ildisplay( void )
{
  status = 0;
  addr = 0;
  fvideo = 0;
  ptsdi = 0;
  ptssi = 0;
  talkerFrame = 0;		// last talker frame
}

// ****************************************** 
// traite_doe(frame)                          
//                                            
// manage the HPIL data frames                
// returns the returned frame                 
// ******************************************
static int traite_doe( int frame )
{
  if (( fvideo & 0xC0) == 0x40)
    {// addressed 
      int talkerError = 0;		// no talker error
      
      if ((fvideo & 0x03) != 0)		// active talker
	{
	  // compare last talker frame with actual frame without SRQ bit
	  talkerError = (int) ((frame & 0x6FF) != (talkerFrame & 0x6FF));
	  
	  if (!talkerError && (fvideo & 0x02) != 0)
	    {
	      int srqBit = frame & 0x100; // actual SRQ state
	      
	      // status (SST) or accessory ID (SDI)
	      if ((fvideo & 0x01) != 0)
		{
		  // 0x43: active talker (multiple byte status)
		  if (ptssi > 0)		// SST
		    {
		      if (--ptssi > 0)
			{
			  frame = (status >> ((ptssi - 1) * 8)) & 0xFF;
			}
		    }
		  // talker 
		  if( ptsdi > 0 )
		    {
		      frame = (int) did[ptsdi++];
//		      frame = (int)c & 0xFF;
		      if( frame == 0 ) // end of string
			ptsdi = 0;
		    }
		  
		  if (ptssi == 0 && ptsdi == 0)
		    {
		      // EOT for SST and SDI
		      frame = 0x540;		// EOT
		    }
		}
	      else // 0x42: active talker (data)
		{
		  // SDA
		  //return frame;
		}
	      
	      // a set SRQ bit doesn't matter on ready class frames
	      frame |= srqBit;			// restore SRQ bit
	    }
	  else // 0x41: active talker (single byte status)
	    {
	      // end of SAI, NRD or talker error
	      frame = 0x540;				// EOT
	    }
	}

      if (frame == 0x540)				// EOT type
	{
	  frame += talkerError;			// check for error and set ETO/ETE frame
	  fvideo &= ~0x03;			// delete active talker
	}
      
      talkerFrame = frame;			// last talker frame
    }

  if (( fvideo & 0xC0) == 0x80)  // listener
    {
      // listener 
      DisplayStr( frame );
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
  int n = frame & 0xFF;

  switch( n >> 5 )
    {
    case 0:
      switch( n )
	{
	case 4: // SDC 
	  if( fvideo & 0x80 )
	      ClrDisplayP();
	  break;
	  
	case 20: // DCL 
	  ClrDisplayP();
	  break;
	}
      break;
      
    case 1: // LAD 
      n = n & 31;
      if( n == 31 )
	{
	  // UNL, if not talker then go to idle state 
	  if( (fvideo & 0xA0) == 0 )
	      fvideo &= 0x50;
	}
      else
	{
	  // else, if MLA go to listen state 
	  if( (fvideo & 0x80) == 0 && n == (addr & 31) )
	      fvideo = 0x80;
	}
      break;

    case 2: // TAD 
      n = n & 31;
      if( n == (addr & 31) )
	  // if MTA go to talker state 
	  fvideo = 0x40;
      else // UNT
	// else if addressed talker, go to idle state 
	if( (fvideo & 0x50) != 0 )
	      fvideo &= 0xA0;
      break;

    case 3:
      if ((fvideo & 0x30) != 0) // LAD or TAD address matched
	{
	  n = n & 31;
	  fvideo = 0;
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
	  addr = DEFADDR;
	  break;
	}
      break;

    default:
      break;
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
    // SOT
      if( (fvideo & 0xC0) == 0x40 )
	{
	  // if addressed talker 
	  if(n == 66) // NRD
	    {
	      // NRD 
	      ptsdi = 0; // abort transfer
	      ptssi = 0;
	      fvideo = 0x41;
	    }
	  else if( n == 96 )
	    {
	      // SDA 
	      // no response
//	      wFrame = OutData(wFrame);
	      if (frame != 0x560)	// not SDA, received data
		{
		  fvideo = 0x42;	// active talker (data)
		  talkerFrame = frame;  // last talker frame
		}
	    }
	  else if( n == 97 )
	    {
	      ptssi = status;
	      // SST
	      frame = (status >> ((ptssi - 1) * 8)) & 0xFF;
	      fvideo = 0x43; // active talker
	      talkerFrame = frame; // last talker frame
	    }
	  else if( n == 98 )
	    {
	      // SDI 
	      frame = (int)(did[0] & 0xFF);
	      ptsdi = 1;
	      fvideo = 0x43;       // active talker (multiple byte status)
	      talkerFrame = frame; // last talker frame
        }
	  else if( n == 99 )
	    {
	      // SAI 
	      frame = AID;
	      fvideo = 0x41;	   // active talker (single byte status)
	      talkerFrame = frame; // last talker frame
	    }
	}
    }
  else if( n < 0x80 + 31 )
    {
      // AAD, if not already an assigned address, take it 
      if( (addr & 0x80) == 0 )
	{
	  addr = n;
	  frame++;
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
int ILdisplay( int frame )
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

