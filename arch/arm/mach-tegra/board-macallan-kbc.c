/*
 * arch/arm/mach-tegra/board-macallan-kbc.c
 * Keys configuration for Nvidia t114 macallan platform.
 *
 * Copyright (C) 2013 NVIDIA, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <mach/io.h>
#include <linux/io.h>
#include <mach/iomap.h>
#include <mach/kbc.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/mfd/palmas.h>
#include "wakeups-t11x.h"

#include "tegra-board-id.h"
#include "board.h"
#include "board-macallan.h"
#include "devices.h"
#include <asm/mach-types.h>

#define GPIO_KEY(_id, _gpio, _iswake)           \
	{                                       \
		.code = _id,                    \
		.gpio = TEGRA_GPIO_##_gpio,     \
		.active_low = 1,                \
		.desc = #_id,                   \
		.type = EV_KEY,                 \
		.wakeup = _iswake,              \
		.debounce_interval = 10,        \
	}

static struct gpio_keys_button macallan_e1545_keys[] = {
	[0] = GPIO_KEY(KEY_POWER, PQ0, 1),
	[1] = GPIO_KEY(KEY_VOLUMEUP, PR2, 0),
	[2] = GPIO_KEY(KEY_VOLUMEDOWN, PR1, 0),
};


static struct gpio_keys_button macall_haydn_keys[] = {
	[0] = GPIO_KEY(KEY_MODE, PK2, 1),
};

static struct gpio_keys_platform_data macallan_e1545_keys_pdata = {
	.buttons	= macallan_e1545_keys,
	.nbuttons	= ARRAY_SIZE(macallan_e1545_keys),
};

static struct gpio_keys_platform_data macallan_haydn_keys_pdata = {
	.buttons        = macall_haydn_keys,
	.nbuttons       = ARRAY_SIZE(macall_haydn_keys),
};

static struct platform_device macallan_e1545_keys_device = {
	.name	= "gpio-keys",
	.id	= 0,
	.dev	= {
		.platform_data  = &macallan_e1545_keys_pdata,
	},
};

static struct platform_device macallan_haydn_keys_device = {
	.name   = "gpio-keys",
	.id     = 1,
	.dev    = {
		.platform_data  = &macallan_haydn_keys_pdata,
	},
};


int __init macallan_kbc_init(void)
{
	platform_device_register(&macallan_e1545_keys_device);
	if(machine_is_haydn()){
		platform_device_register(&macallan_haydn_keys_device);
	}
	return 0;
}

