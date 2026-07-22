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
  const static struct
  {
    int wOpc;							// opcode
    int wMask;							// opcode mask
    char cMne[4];						// mnemonic
  } sCodes[] =
    {
      { 0x000, 0x700, "DAB" },
      { 0x100, 0x700, "DSR" },
      { 0x200, 0x700, "END" },
      { 0x300, 0x700, "ESR" },
      { 0x400, 0x7FF, "NUL" },
      { 0x401, 0x7FF, "GTL" },
      { 0x404, 0x7FF, "SDC" },
      { 0x405, 0x7FF, "PPD" },
      { 0x408, 0x7FF, "GET" },
      { 0x40F, 0x7FF, "ELN" },
      { 0x410, 0x7FF, "NOP" },
      { 0x411, 0x7FF, "LLO" },
      { 0x414, 0x7FF, "DCL" },
      { 0x415, 0x7FF, "PPU" },
      { 0x418, 0x7FF, "EAR" },
      { 0x43F, 0x7FF, "UNL" },
      { 0x420, 0x7E0, "LAD" },
      { 0x45F, 0x7FF, "UNT" },
      { 0x440, 0x7E0, "TAD" },
      { 0x460, 0x7E0, "SAD" },
      { 0x480, 0x7F0, "PPE" },
      { 0x490, 0x7FF, "IFC" },
      { 0x492, 0x7FF, "REN" },
      { 0x493, 0x7FF, "NRE" },
      { 0x49A, 0x7FF, "AAU" },
      { 0x49B, 0x7FF, "LPD" },
      { 0x4A0, 0x7E0, "DDL" },
      { 0x4C0, 0x7E0, "DDT" },
      { 0x400, 0x700, "CMD" },
      { 0x500, 0x7FF, "RFC" },
      { 0x540, 0x7FF, "ETO" },
      { 0x541, 0x7FF, "ETE" },
      { 0x542, 0x7FF, "NRD" },
      { 0x560, 0x7FF, "SDA" },
      { 0x561, 0x7FF, "SST" },
      { 0x562, 0x7FF, "SDI" },
      { 0x563, 0x7FF, "SAI" },
      { 0x564, 0x7FF, "TCT" },
      { 0x580, 0x7E0, "AAD" },
      { 0x5A0, 0x7E0, "AEP" },
      { 0x5C0, 0x7E0, "AES" },
      { 0x5E0, 0x7E0, "AMP" },
      { 0x500, 0x700, "RDY" },
      { 0x600, 0x700, "IDY" },
      { 0x700, 0x700, "ISR" }
    };

  // go through HP-IL opcode table
  for (unsigned int i = 0; i < sizeof (sCodes) / sizeof (sCodes[0]); ++i)
    {	// found opcode in table
      if ((frame & sCodes[i].wMask) == sCodes[i].wOpc)
	{
	  // get argument from mask
	  const int wArg = (~sCodes[i].wMask) & 0xFF;
	  
	  strcpy(s,sCodes[i].cMne);		// copy name
	  if (wArg != 0)			// opcode has an argument
	    {
	      // OxA0 is unbreakable space (not for curses, retore normal space)
		sprintf(&s[3],"%c%02X", 0x20, (frame & wArg));
	    }
	  break;
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
