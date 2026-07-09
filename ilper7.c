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
// ilper7.c   ILPER main module
//
// Based on previous personal work:           
// 1986: ILPER4 (6502 assembler)  
// 1988: ILPER5 modular version (6502 assembler)  
// 1993: ILPER6 ported on PC (8086 assembler)
// 1997: rewriten in C and included in Emu41
// 2008: rewriten in VB for the standalone ILPER Windows version using the PILBox!
// 2009: released as free software
// 2011: ported on Linux by Ch. Gottheimer
// 2026: improved by JM. Vansteene
// ---------------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <limits.h>     /* PATH_MAX */
#include "ilper.h"

/* String buffers. */
#define BUFLEN  PATH_MAX
static char  buf0[BUFLEN];  // buffer 0 

// ******************************************
// ILMnemo()
//
// returns the frame mnemonic
// ******************************************
static char *ILMnemo( int frame )
{
  static char stmp[9];
         char s[7];
         int  n;

  char *mnemo[] = { "DAB", "DSR", "END", "ESR", "CMD", "RDY", "IDY", "ISR", };
  char *scmd0[] = { "NUL", "GTL", "???", "???", "SDC", "PPD", "???", "???",
                    "GET", "???", "???", "???", "???", "???", "???", "ELN",
                    "NOP", "LLO", "???", "???", "DCL", "PPU", "???", "???",
                    "EAR", "???", "???", "???", "???", "???", "???", "???", };
  char *scmd9[] = { "IFC", "???", "REN", "NRE", "???", "???", "???", "???",
                    "???", "???", "AAU", "LPD", "???", "???", "???", "???", };

  n = frame & 255;
  snprintf( s, sizeof(s), "%s %02X", mnemo[frame / 256], n );
  if( (frame & 0x700) == 0x400 )
    {
      // CMD 
      switch( n / 32 )
	{
	case 0: snprintf( s, sizeof(s), "%s", scmd0[(n & 31)] );
	  break;
	case 1: if( (n & 31) == 31)
	    {
	      snprintf( s, sizeof(s), "%s", "UNL" );
	    }
	  else
	    {
	      snprintf( s, sizeof(s), "%s %02X", "LAD", (n & 31) );
	    }
	  break;
	case 2: if( (n & 31) == 31 )
	    {
	      snprintf( s, sizeof(s), "%s", "UNT" );
	    }
	  else
	    {
	      snprintf( s, sizeof(s), "%s %02X", "TAD", (n & 31) );
	    }
	  break;
	case 3: snprintf( s, sizeof(s), "%s %02X", "SAD", (n & 31) );
	  break;
	case 4: if( (n & 31) < 16 )
	    {
	      snprintf( s, sizeof(s), "%s %02X", "PPE", (n & 31) );
	    }
	  else
	    {
	      snprintf(s, sizeof(s), "%s", scmd9[(n & 15)] );
	    }
	  break;
	case 5: snprintf(s, sizeof(s), "%s %02X", "DDL", (n & 31) );
	  break;
	case 6: snprintf(s, sizeof(s), "%s %02X", "DDT", (n & 31) );
	  break;
	}
      if( s[0] == '?' )
	{
	  snprintf(s, sizeof(s), "%s %02X", "CMD", n );
	}
    }
  else if( (frame & 0x700) == 0x500 )
    {
      // RDY 
      if( n < 128 )
	{
	  switch( n )
	    {
	    case 0: snprintf( s, sizeof(s), "%s", "RFC"); break;
	    case 64: snprintf( s, sizeof(s), "%s", "ETO"); break;
	    case 65: snprintf( s, sizeof(s), "%s", "ETE"); break;
	    case 66: snprintf( s, sizeof(s), "%s", "NRD"); break;
	    case 96: snprintf( s, sizeof(s), "%s", "SDA"); break;
	    case 97: snprintf( s, sizeof(s), "%s", "SST"); break;
	    case 98: snprintf( s, sizeof(s), "%s", "SDI"); break;
	    case 99: snprintf( s, sizeof(s), "%s", "SAI"); break;
	    case 100: snprintf( s, sizeof(s), "%s", "TCT"); break;
	    default: snprintf( s, sizeof(s), "%s %02X", "RDY", n ); break;
	    }
	}
      else
	{
	  switch( n / 32 )
	    {
	    case 4: snprintf( s, sizeof(s), "%s %02X", "AAD", (n & 31) ); break;
	    case 5: snprintf( s, sizeof(s), "%s %02X", "AEP", (n & 31) ); break;
	    case 6: snprintf( s, sizeof(s), "%s %02X", "AES", (n & 31) ); break;
	    case 7: snprintf( s, sizeof(s), "%s %02X", "AMP", (n & 31) ); break;
	    default: snprintf( s, sizeof(s), "%s %02X", "RDY", (n) ); break;
	    }
	}
    }

  snprintf( stmp, sizeof(stmp), "%-8s", s );
  return stmp;
}

// ******************************************
// hpil_transmit()
//
// transmit the frame to all the
// internal virtual devices
// ******************************************
int hpil_transmit( int frame )
{
  char *s;

  // scope - can be inserted between any devices
  s = ILMnemo( frame );
  DisplayMnemo(s);

  // devices - in sequencial order
  frame = ILdisplay ( frame );
  frame = ILvideo ( frame );
  frame = ILhdisc ( frame );

  return frame;
}

// ******************************************
// init_hpil()
//
// init the virtual devices at appl starup
// ******************************************
void init_hpil( void )
{
  init_ildisplay();
  init_ilvideo();
  strcpy (buf0, strwd); // Copy "PATH/"
  strcat (buf0, strca); // Copy Filename
  init_ilhdisc( buf0 );
}

