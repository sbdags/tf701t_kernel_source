/*
 * arch/arm/mach-tegra/board-macallan-sdhci.c
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/resource.h>
#include <linux/platform_device.h>
#include <linux/wlan_plat.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/mmc/host.h>

#include <asm/mach-types.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/sdhci.h>
#include <mach/gpio-tegra.h>
#include <mach/io_dpd.h>
#include <linux/wl12xx.h>

#include "gpio-names.h"
#include "board.h"
#include "board-macallan.h"
#include "dvfs.h"
#include "fuse.h"

#define FUSE_CORE_SPEEDO_0	0x134
#define MACALLAN_SD_CD	TEGRA_GPIO_PV2
#define MACALLAN_WLAN_PWR	TEGRA_GPIO_PCC5
#define MACALLAN_WLAN_RST	TEGRA_GPIO_PX7
#define MACALLAN_WLAN_WOW	TEGRA_GPIO_PU5
#define MACALLAN_EN_3V3_COM	TEGRA_GPIO_PU1
#if defined(CONFIG_BCM43XX_EDP_SUPPORT)
/* Wifi power levels */
#define ON  1050 /* 1050 mW */
#define OFF 0
static unsigned int wifi_states[] = {ON, OFF};
#endif



static void (*wifi_status_cb)(int card_present, void *dev_id);
static void *wifi_status_cb_devid;
static int macallan_wifi_status_register(void (*callback)(int , void *), void *);

static int macallan_wifi_reset(int on);
static int macallan_wifi_power(int on);
static int macallan_wifi_set_carddetect(int val);

static struct wifi_platform_data macallan_wifi_control = {
	.set_power = macallan_wifi_power,
	.set_reset = macallan_wifi_reset,
	.set_carddetect = macallan_wifi_set_carddetect,
#if defined(CONFIG_BCM43XX_EDP_SUPPORT)
	/* set the wifi edp client information here */
	.client_info    = {
		.name       = "wifi_edp_client",
		.states     = wifi_states,
		.num_states = ARRAY_SIZE(wifi_states),
		.e0_index   = 0,
		.priority   = EDP_MAX_PRIO,
	},
#endif

};

static struct resource wifi_resource[] = {
	[0] = {
		.name	= "bcmdhd_wlan_irq",
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL
				| IORESOURCE_IRQ_SHAREABLE,
	},
};

static struct platform_device macallan_wifi_device = {
	.name		= "bcmdhd_wlan",
	.id		= 1,
	.num_resources	= 1,
	.resource       = wifi_resource,
	.dev		= {
		.platform_data = &macallan_wifi_control,
	},
};

static struct resource sdhci_resource0[] = {
	[0] = {
		.start  = INT_SDMMC1,
		.end    = INT_SDMMC1,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SDMMC1_BASE,
		.end	= TEGRA_SDMMC1_BASE + TEGRA_SDMMC1_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource sdhci_resource2[] = {
	[0] = {
		.start  = INT_SDMMC3,
		.end    = INT_SDMMC3,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SDMMC3_BASE,
		.end	= TEGRA_SDMMC3_BASE + TEGRA_SDMMC3_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource sdhci_resource3[] = {
	[0] = {
		.start  = INT_SDMMC4,
		.end    = INT_SDMMC4,
		.flags  = IORESOURCE_IRQ,
	},
	[1] = {
		.start	= TEGRA_SDMMC4_BASE,
		.end	= TEGRA_SDMMC4_BASE + TEGRA_SDMMC4_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
};

#ifdef CONFIG_MMC_EMBEDDED_SDIO
static struct embedded_sdio_data embedded_sdio_data0 = {
	.cccr   = {
		.sdio_vsn       = 2,
		.multi_block    = 1,
		.low_speed      = 0,
		.wide_bus       = 0,
		.high_power     = 1,
		.high_speed     = 1,
	},
	.cis  = {
		.vendor	 = 0x02d0,
		.device	 = 0x4329,
	},
};
#endif

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data0 = {
	.mmc_data = {
		.register_status_notify	= macallan_wifi_status_register,
#ifdef CONFIG_MMC_EMBEDDED_SDIO
		.embedded_sdio = &embedded_sdio_data0,
#endif
		.built_in = 0,
		.ocr_mask = MMC_OCR_1V8_MASK,
	},
#ifndef CONFIG_MMC_EMBEDDED_SDIO
	.pm_flags = MMC_PM_KEEP_POWER,
#endif
	.cd_gpio = -1,
	.wp_gpio = -1,
	.power_gpio = -1,
	.tap_delay = 0x2,
	.trim_delay = 0x2,
	.ddr_clk_limit = 41000000,
	.max_clk_limit = 82000000,
	.uhs_mask = MMC_UHS_MASK_DDR50,
	.edp_support = false,
};

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data2 = {
	.cd_gpio = MACALLAN_SD_CD,
	.wp_gpio = -1,
	.power_gpio = -1,
	.tap_delay = 0x3,
	.trim_delay = 0x3,
	.ddr_clk_limit = 41000000,
	.max_clk_limit = 82000000,
	.uhs_mask = MMC_UHS_MASK_DDR50,
	.edp_support = true,
	.edp_states = {966, 0},
};

static struct tegra_sdhci_platform_data tegra_sdhci_platform_data3 = {
	.cd_gpio = -1,
	.wp_gpio = -1,
	.power_gpio = -1,
	.is_8bit = 1,
	.tap_delay = 0x5,
	.trim_delay = 0x3,
	.ddr_clk_limit = 41000000,
	.max_clk_limit = 156000000,
	.mmc_data = {
		.built_in = 1,
		.ocr_mask = MMC_OCR_1V8_MASK,
	},
	.edp_support = true,
	.edp_states = {966, 0},
};

static struct platform_device tegra_sdhci_device0 = {
	.name		= "sdhci-tegra",
	.id		= 0,
	.resource	= sdhci_resource0,
	.num_resources	= ARRAY_SIZE(sdhci_resource0),
	.dev = {
		.platform_data = &tegra_sdhci_platform_data0,
	},
};

static struct platform_device tegra_sdhci_device2 = {
	.name		= "sdhci-tegra",
	.id		= 2,
	.resource	= sdhci_resource2,
	.num_resources	= ARRAY_SIZE(sdhci_resource2),
	.dev = {
		.platform_data = &tegra_sdhci_platform_data2,
	},
};

static struct platform_device tegra_sdhci_device3 = {
	.name		= "sdhci-tegra",
	.id		= 3,
	.resource	= sdhci_resource3,
	.num_resources	= ARRAY_SIZE(sdhci_resource3),
	.dev = {
		.platform_data = &tegra_sdhci_platform_data3,
	},
};

static int macallan_wifi_status_register(
		void (*callback)(int card_present, void *dev_id),
		void *dev_id)
{
	if (wifi_status_cb)
		return -EAGAIN;
	wifi_status_cb = callback;
	wifi_status_cb_devid = dev_id;
	return 0;
}

static int macallan_wifi_set_carddetect(int val)
{
	pr_debug("%s: %d\n", __func__, val);
	if (wifi_status_cb)
		wifi_status_cb(val, wifi_status_cb_devid);
	else
		pr_warning("%s: Nobody to notify\n", __func__);
	return 0;
}

static int macallan_wifi_reset(int on)
{
	pr_debug("%s: do nothing\n", __func__);
	return 0;
}

static int macallan_wifi_power(int on)
{
	pr_debug("%s: %d\n", __func__, on);

	if (on) {
		gpio_set_value(MACALLAN_WLAN_RST, 1);
		mdelay(100);
		gpio_set_value(MACALLAN_WLAN_PWR, 1);
		mdelay(200);
	} else {
		gpio_set_value(MACALLAN_WLAN_RST, 0);
		mdelay(100);
		gpio_set_value(MACALLAN_WLAN_PWR, 0);
	}

	return 0;
}

static int __init macallan_wifi_init(void)
{
	int rc;

	rc = gpio_request(MACALLAN_EN_3V3_COM, "en_3v3_com");
	if (rc)
		pr_err("EN_3V3_COM gpio request failed:%d\n", rc);

	rc = gpio_request(MACALLAN_WLAN_PWR, "wlan_power");
	if (rc)
		pr_err("WLAN_PWR gpio request failed:%d\n", rc);
	rc = gpio_request(MACALLAN_WLAN_RST, "wlan_rst");
	if (rc)
		pr_err("WLAN_RST gpio request failed:%d\n", rc);
	rc = gpio_request(MACALLAN_WLAN_WOW, "bcmsdh_sdmmc");
	if (rc)
		pr_err("WLAN_WOW gpio request failed:%d\n", rc);

	rc = gpio_direction_output(MACALLAN_EN_3V3_COM, 0);
	if (rc)
		pr_err("EN_3V3_COM gpio direction configuration failed:%d\n", rc);

	rc = gpio_direction_output(MACALLAN_WLAN_PWR, 0);
	if (rc)
		pr_err("WLAN_PWR gpio direction configuration failed:%d\n", rc);
	rc = gpio_direction_output(MACALLAN_WLAN_RST, 0);
	if (rc)
		pr_err("WLAN_RST gpio direction configuration failed:%d\n", rc);
	rc = gpio_direction_input(MACALLAN_WLAN_WOW);
	if (rc)
		pr_err("WLAN_WOW gpio direction configuration failed:%d\n", rc);

	wifi_resource[0].start = wifi_resource[0].end =
		gpio_to_irq(MACALLAN_WLAN_WOW);

	gpio_set_value(MACALLAN_EN_3V3_COM, 1);

	platform_device_register(&macallan_wifi_device);
	return 0;
}

#ifdef CONFIG_TEGRA_PREPOWER_WIFI
static int __init macallan_wifi_prepower(void)
{
	if (!(machine_is_macallan() || machine_is_mozart() || machine_is_haydn()))
		return 0;

	macallan_wifi_power(1);

	return 0;
}

subsys_initcall_sync(macallan_wifi_prepower);
#endif

int __init macallan_sdhci_init(void)
{
	int nominal_core_mv;
	int min_vcore_override_mv;
	int boot_vcore_mv;
	int speedo;

	nominal_core_mv =
		tegra_dvfs_rail_get_nominal_millivolts(tegra_core_rail);
	if (nominal_core_mv > 0) {
		tegra_sdhci_platform_data0.nominal_vcore_mv = nominal_core_mv;
		tegra_sdhci_platform_data2.nominal_vcore_mv = nominal_core_mv;
		tegra_sdhci_platform_data3.nominal_vcore_mv = nominal_core_mv;
	}
	min_vcore_override_mv =
		tegra_dvfs_rail_get_override_floor(tegra_core_rail);
	if (min_vcore_override_mv) {
		tegra_sdhci_platform_data0.min_vcore_override_mv =
			min_vcore_override_mv;
		tegra_sdhci_platform_data2.min_vcore_override_mv =
			min_vcore_override_mv;
		tegra_sdhci_platform_data3.min_vcore_override_mv =
			min_vcore_override_mv;
	}
	boot_vcore_mv = tegra_dvfs_rail_get_boot_level(tegra_core_rail);
	if (boot_vcore_mv) {
		tegra_sdhci_platform_data0.boot_vcore_mv = boot_vcore_mv;
		tegra_sdhci_platform_data2.boot_vcore_mv = boot_vcore_mv;
		tegra_sdhci_platform_data3.boot_vcore_mv = boot_vcore_mv;
	}

	speedo = tegra_fuse_readl(FUSE_CORE_SPEEDO_0);
	tegra_sdhci_platform_data0.cpu_speedo = speedo;
	tegra_sdhci_platform_data2.cpu_speedo = speedo;
	tegra_sdhci_platform_data3.cpu_speedo = speedo;

	if ((tegra_sdhci_platform_data3.uhs_mask & MMC_MASK_HS200)
		&& (!(tegra_sdhci_platform_data3.uhs_mask &
		MMC_UHS_MASK_DDR50)))
		tegra_sdhci_platform_data3.trim_delay = 0;

	platform_device_register(&tegra_sdhci_device3);
	platform_device_register(&tegra_sdhci_device2);
	platform_device_register(&tegra_sdhci_device0);
	macallan_wifi_init();
	return 0;
}
