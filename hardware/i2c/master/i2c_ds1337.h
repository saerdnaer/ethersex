/*
 *
 * Copyright (c) 2009 by Dirk Tostmann <tostmann@busware.de>
 * Copyright (c) 2010 Thomas Kaiser
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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

#ifndef _I2C_DS1337_H
#define _I2C_DS1337_H

#define I2C_SLA_DS1337 0x68

uint16_t i2c_ds1337_set(uint8_t reg, uint8_t data);
uint16_t i2c_ds1337_get(uint8_t reg);

uint8_t i2c_ds1337_set_block(uint8_t addr, char *data, uint8_t len);
uint8_t i2c_ds1337_get_block(uint8_t addr, char *data, uint8_t len);

void i2c_ds1337_sync(uint32_t timestamp);

uint32_t  i2c_ds1337_read();

struct ds1337_reg {
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
    uint8_t day;
    uint8_t date;
    uint8_t month;
    uint8_t year;
} __attribute__((__packed__));

typedef struct ds1337_reg ds1337_reg_t;

void i2c_ds1337_init(void);

#endif /* _I2C_DS1337_H */
