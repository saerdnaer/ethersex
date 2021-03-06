/*
* Copyright (c) 2007 by Stefan Siegl <stesie@brokenpipe.de>
* Copyright (c) 2009 by David Gräff <david.graeff@web.de>
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
*
* For more information on the GPL, please go to:
* http://www.gnu.org/copyleft/gpl.html
*/

#include <stdint.h>
#include <string.h>
#include "core/eeprom.h"
#include "core/debug.h"
#include "stella.h"
#include "stella_fading_functions.h"

uint8_t stella_brightness[STELLA_CHANNELS];
uint8_t stella_fade[STELLA_CHANNELS];

uint8_t stella_fade_func = STELLA_FADE_FUNCTION_INIT;
uint8_t stella_fade_step = STELLA_FADE_STEP_INIT;
volatile uint8_t stella_fade_counter = 0;

volatile enum stella_update_sync stella_sync;
uint8_t stella_portmask[STELLA_PORT_COUNT];

struct stella_timetable_struct timetable_1, timetable_2;
struct stella_timetable_struct* int_table;
struct stella_timetable_struct* cal_table;


void stella_sort(void);

/* Initialize stella */
void
stella_init (void)
{
	int_table = &timetable_1;
	cal_table = &timetable_2;
	cal_table->head = 0;
	
	stella_sync = NOTHING_NEW;

	/* set stella port pins to output and save the port mask */
	stella_portmask[0] = ((1 << STELLA_PINS_PORT1) - 1) << STELLA_OFFSET_PORT1;
	STELLA_DDR_PORT1 |= stella_portmask[0];
	cal_table->port[0].port = &STELLA_PORT1;
	cal_table->port[0].mask = 0;
	#ifdef STELLA_PINS_PORT2
	stella_portmask[1] = ((1 << STELLA_PINS_PORT2) - 1) << STELLA_OFFSET_PORT2;
	STELLA_DDR_PORT2 |= stella_portmask[1];
	cal_table->port[0].port = &STELLA_PORT2;
	cal_table->port[1].mask = 0;
	#endif

	/* initialise the fade counter. Fading works like this:
	* -> decrement fade_counter
	* -> on zero, fade if neccessary
	* -> reset counter to fade_step
	*/
	stella_fade_counter = stella_fade_step;

	#if !defined(TEENSY_SUPPORT) && STELLA_START == stella_start_eeprom
	stella_loadFromEEROMFading();
	#endif
	#if STELLA_START == stella_start_all
	memset(stella_fade, 255, sizeof(stella_fade));
	#endif

	stella_sort();

	/* we need at least 64 ticks for the compare interrupt,
	* therefore choose a prescaler of at least 64. */
	
	#ifdef STELLA_HIGHFREQ
	/* High frequency PWM Mode, 64 Prescaler */
	_TCCR2_PRESCALE = _BV(CS22);
	debug_printf("Stella freq: %u Hz\n", F_CPU/64/(256*2));
	#else
	/* Normal PWM Mode, 128 Prescaler */
	_TCCR2_PRESCALE |= _BV(CS20) | _BV(CS22);
	debug_printf("Stella freq: %u Hz\n", F_CPU/128/(256*2));
	#endif

	/* Interrupt on overflow and CompareMatch */
	_TIMSK_TIMER2 |= _BV(TOIE2) | _BV(_OUTPUT_COMPARE_IE2);
}

uint8_t
stella_output_channels(void* target)
{
	struct stella_output_channels_struct *buf = target;
	buf->channel_count = STELLA_CHANNELS;
	memcpy(buf->pwm_channels, stella_brightness, STELLA_CHANNELS);
	return sizeof(struct stella_output_channels_struct);
}

void
stella_dmx(uint8_t* dmx_data, uint8_t len)
{
	// length
	if (len<2) return; // no real data, abort
	--len; // ignore first byte (defines fade function)
	if (STELLA_CHANNELS < len) len = STELLA_CHANNELS;

	for (uint8_t i=0;i<len;++i)
		stella_setValue(dmx_data[0], i, dmx_data[i+1]);
}

/* Process recurring actions for stella */
void
stella_process (void)
{

	/* the main loop is too fast, slow down */
	if (stella_fade_counter == 0)
	{
		uint8_t i;
		/* Fade channels. stella_fade_counter is 0 currently. Set to 1
		if fading changed a channel brigthness value */
		for (i = 0; i < STELLA_CHANNELS; ++i)
		{
			if (stella_brightness[i] == stella_fade[i])
				continue;

			stella_fade_funcs[stella_fade_func].p (i);

			stella_fade_counter = 1;
		}

		if (stella_fade_counter) stella_sync = UPDATE_VALUES;

		/* reset counter */
		stella_fade_counter = stella_fade_step;
	}

	/* sort if new values are available */
	if (stella_sync == UPDATE_VALUES)
		stella_sort();
}

void
stella_setValue(const enum stella_set_function func, const uint8_t channel, const uint8_t value)
{
	if (channel >= STELLA_CHANNELS) return;

	switch (func)
	{
		case STELLA_SET_IMMEDIATELY:
			stella_brightness[channel] = value;
			stella_fade[channel] = value;
			stella_sync = UPDATE_VALUES;
			break;
		case STELLA_SET_FADE:
			stella_fade[channel] = value;
			break;
		case STELLA_SET_FLASHY:
			stella_brightness[channel] = value;
			stella_fade[channel] = 0;
			stella_sync = UPDATE_VALUES;
			break;
		case STELLA_SET_IMMEDIATELY_RELATIVE:
			stella_brightness[channel] += (int8_t)value;
			stella_fade[channel] += (int8_t)value;
			stella_sync = UPDATE_VALUES;
			break;
	}
}

void stella_setFadestep(const uint8_t fadestep) {
  stella_fade_step = fadestep;
}

uint8_t stella_getFadestep() {
  return stella_fade_step;
}

/* Get a channel value.
 * Only call this function with a channel<STELLA_CHANNELS ! */
inline uint8_t
stella_getValue(const uint8_t channel)
{
	return stella_brightness[channel];
}

#ifndef TEENSY_SUPPORT
void
stella_loadFromEEROMFading()
{
	eeprom_restore(stella_channel_values, stella_fade, STELLA_CHANNELS);
}
#endif

#ifndef TEENSY_SUPPORT
void
stella_loadFromEEROM()
{
	eeprom_restore(stella_channel_values, stella_fade, STELLA_CHANNELS);
	memcpy(stella_brightness, stella_fade, STELLA_CHANNELS);
	stella_sync = UPDATE_VALUES;
}
#endif

#ifndef TEENSY_SUPPORT
void
stella_storeToEEROM()
{
	eeprom_save(stella_channel_values, stella_brightness, STELLA_CHANNELS);
}
#endif

/* How to use:
 * Do not call this directly, but use "stella_sync = UPDATE_VALUES" instead.
 * Purpose:
 * Sort channels' brightness values from high to low (and the
 * interrupt time points from low to high), to be able to switch on
 * channels one after the other depending on their brightness level
 * and point in time.
 * Implementation details:
 * Use a "linked list" to avoid expensive memory copies. Main difference
 * to a real linked list is, that all elements are already preallocated
 * on the stack and are not allocated on demand.
 * The function directly writes to a "just calculated"-structure and if we
 * want new values in the pwm interrupt, we just have to swap pointers from
 * the "interrupt save"-structure to the "just calculated"-structure. (The
 * meaning of both structures changes, too, of course.)
 * Although we provide each channel in the structure with its neccessary
 * information such as portmask and brightness level, we will actually
 * ignore brightness levels of 0% and 100% due to not linking them to the linked list.
 * 100%-level channels are switched on at the beginning of each
 * pwm cycle and not touched afterwards. Channels with same brightness
 * levels are merged together (their portmask at least).
 * */
inline void
stella_sort()
{
	struct stella_timetable_entry* current, *last;
	uint8_t i;

	cal_table->head = 0;
	cal_table->port[0].mask = 0;
	cal_table->port[0].port = &STELLA_PORT1;
	#ifdef STELLA_PINS_PORT2
	cal_table->port[1].mask = 0;
	cal_table->port[1].port = &STELLA_PORT2;
	#endif

	for (i=0;i<STELLA_CHANNELS;++i)
	{
		/* set data of channel i */
		cal_table->channel[i].port.mask = _BV(i+STELLA_OFFSET_PORT1);
		cal_table->channel[i].port.port = &STELLA_PORT1;
		#ifdef STELLA_PINS_PORT2
		if (i>=STELLA_PINS_PORT1) {
			cal_table->channel[i].port.mask = _BV( (i-STELLA_PINS_PORT1) +STELLA_OFFSET_PORT2);
			cal_table->channel[i].port.port = &STELLA_PORT2;
		}
		#endif
		cal_table->channel[i].value = 255 - stella_brightness[i];
		cal_table->channel[i].next = 0;

		/* Special case: 0% brightness (Don't include this channel!) */
		if (stella_brightness[i] == 0) continue;

		//cal_table->portmask |= _BV(i+STELLA_OFFSET);

		/* Special case: 100% brightness (Merge pwm cycle start masks! Don't include this channel!) */
		if (stella_brightness[i] == 255)
		{
			#ifdef STELLA_PINS_PORT2
			if (i>=STELLA_PINS_PORT1)
				cal_table->port[1].mask |= _BV( (i-STELLA_PINS_PORT1) +STELLA_OFFSET_PORT2);
			else
				cal_table->port[0].mask |= _BV(i+STELLA_OFFSET_PORT1);
			#else
			cal_table->port[0].mask |= _BV(i+STELLA_OFFSET_PORT1);
			#endif
			continue;
		}

		/* first item in linked list */
		if (!cal_table->head)
		{
			cal_table->head = &(cal_table->channel[i]);
			continue;
		}
		/* add to linked list with >=1 entries */
		current = cal_table->head; last = 0;
		while (current)
		{
			// same value as current item: do not add to linked list
			// but just update the portmask (DO THIS ONLY IF BOTH CHANNELS OPERATE ON THE SAME PORT)
			if (current->value == cal_table->channel[i].value && current->port.port == cal_table->channel[i].port.port)
			{
				#ifdef STELLA_PINS_PORT2
				if (i>=STELLA_PINS_PORT1)
					current->port.mask |= _BV( (i-STELLA_PINS_PORT1) +STELLA_OFFSET_PORT2);
				else
					current->port.mask |= _BV(i+STELLA_OFFSET_PORT1);
				#else
				current->port.mask |= _BV(i+STELLA_OFFSET_PORT1);
				#endif
				break;
			}
			// insert our new value at the head of the list
			else if (!last && current->value > cal_table->channel[i].value)
			{
				cal_table->channel[i].next = cal_table->head;
				cal_table->head = &(cal_table->channel[i]);
				break;
			}
			// insert our new value somewhere in betweem
			else if (current->value > cal_table->channel[i].value)
			{
				cal_table->channel[i].next = last->next;
				last->next = &(cal_table->channel[i]);
				break;
			}
			// reached the end of the linked list: just append our new entry
			else if (!current->next)
			{
				current->next = &(cal_table->channel[i]);
				break;
			}
			// else go to the next item in the linked list
			else
			{
				last = current;
				current = current->next;
			}
		}
	}

	#ifdef DEBUG_STELLA
	// debug out
	current = cal_table->head;
	i = 0;
	while (current)
	{
		i++;
		debug_printf("%u %s\n", current->value, debug_binary(current->portmask));
		current = current->next;
	}
	debug_printf("Mask1: %s %u\n", debug_binary(stella_portmask[0]), stella_portmask[0]);
	#ifdef STELLA_PINS_PORT2
	debug_printf("Mask2: %s %u\n", debug_binary(stella_portmask[1]), stella_portmask[1]);
	#endif
	#endif

	/* Allow the interrupt to actually apply the calculated values */
	stella_sync = NEW_VALUES;
}

/*
  -- Ethersex META --
  header(services/stella/stella.h)
  mainloop(stella_process)
  init(stella_init)
*/
