/*
 * Copyright (c) 2009 by Stefan Riepenhausen <rhn@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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
 *
 * For more information on the GPL, please go to:
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <avr/pgmspace.h>
#include <util/delay.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "../clock/clock.h"
#include "appsample.h"


#include "protocols/ecmd/ecmd-base.h"

const uint8_t table[] PROGMEM = { 
	0b10111011, // 0
	0b00101000, // 1
	0b01110011, // 2
	0b01111001, // 3
	0b11101000, // 4
	0b11011001, // 5
	0b11011011, // 6
	0b00111000, // 7
	0b11111011, // 8
	0b11111001, // 9
	0b11111010, // A
	0b11001011, // b
	0b01000011, // c
	0b01101011, // d
	0b11010011, // E
	0b11010010, // F
	0b01000000, // -
	0b10010011, // C
	0
};

const uint32_t countdowns[] PROGMEM = {
1238763600,  1238766300, 
1238767200,  1238771700, 
1238772600,  1238777100, 
1238781600,  1238786100, 
1238787000,  1238791500, 
1238792400,  1238796900, 
1238842800,  1238847300, 
1238848200,  1238852700, 
1238853600,  1238862600, 
1238868000,  1238878800, 
1238879700,  1238884200, 
1238929200,  1238933700, 
1238934600,  1238939100, 
1238940000,  1238944500, 
1238945400,  1238949900, 
1238954400,  1238958900, 
1238959800,  1238964300, 
1238965200,  1238969700, 
1239015600,  1239020100, 
1239021000,  1239025500, 
1239026400,  1239030900, 
1239031800,  1239035400,
0}; 

uint8_t current_countdown = 0;



// current display value
uint8_t display[] = {0, 0, 0};

int16_t
app_sample_init(void)
{
  DDRA = 0xff;
  PORTA = 0b00001111;
  DDRC = 0xff;
  PORTC = 0b01000000;

  current_countdown = 0;

  return ECMD_FINAL_OK;
}

// function is called from the mainloop
// see mainloop(app_sample_process) at the bottom
int16_t
app_sample_process(void)
{
  static int8_t j=0;
  
  PORTC = 0;
  PORTA = 0;
  _delay_us(5);
  PORTC = display[j];
  PORTA = 0x01<<j;
  
  j = (j+1)%3;

  return ECMD_FINAL_OK;
}


/*
  This function is periodically called
  change "timer(100,app_sample_periodic)" if needed
*/
int16_t
app_sample_periodic(void)
{
  uint32_t current_time = clock_get_time();
  
  while ( (pgm_read_dword(&countdowns[current_countdown]) < current_time) && pgm_read_dword(&countdowns[current_countdown]) != 0 ) 
  {
    current_countdown++;
  }
  
  if ( pgm_read_dword(&countdowns[current_countdown]) == 0 )
  {
    set_display_via_table(14, 0, 17); //  EOC
    return ECMD_FINAL_OK;
  }

  uint16_t rest = (uint16_t) (pgm_read_dword(&countdowns[current_countdown]) - current_time) / 60;
  // ungerade eintraege in der countdowntable sind startzeiten
  // anzeige mit minus
  if ( current_countdown % 2 == 0 )
  {
    if ( rest > 99 )
    {
      clear_display();
    }
    else 
    {
      set_display_via_table(16, rest/10, rest%10);
    }
  }
  else
  {
    if ( rest > 999 )
    {
      clear_display();
    }
    else 
    {
      set_display_via_table(rest/100, (rest/10)%10, rest%10);
    }
  }
  return ECMD_FINAL_OK;
}

void
clear_display(void)
{
  set_display(0b01000000, 0b01000000, 0b10000000);
}


void
set_display_via_table(uint8_t a, uint8_t b, uint8_t c)
{
  set_display(pgm_read_byte(&table[a]), pgm_read_byte(&table[b]), pgm_read_byte(&table[c]));
}

void
set_display(uint8_t a, uint8_t b, uint8_t c)
{
  display[0] = a;
  display[1] = b;
  display[2] = c;
}

/*
  This function will be called on request by menuconfig, if wanted...
  You need to enable ECMD_SUPPORT for this.
  Otherwise you can use this function for anything you like 
*/
int16_t
app_sample_onrequest(char *cmd, char *output, uint16_t len)
{
  APPSAMPLEDEBUG ("main\n");
  // enter your code here

  return ECMD_FINAL(snprintf_P(output, len, PSTR("current_countdown %u"), current_countdown));
}

/*
  -- Ethersex META --
  header(services/appsample/appsample.h)
  init(app_sample_init)
  mainloop(app_sample_process)
  timer(100,app_sample_periodic())
*/
