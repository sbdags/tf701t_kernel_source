/*
 * arch/arm/mach-tegra/board-macallan.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/serial_8250.h>
#include <linux/i2c.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/i2c-tegra.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/spi/spi.h>
#include <linux/spi/rm31080a_ts.h>
#include <linux/tegra_uart.h>
#include <linux/memblock.h>
#include <linux/spi-tegra.h>
#include <linux/nfc/pn544.h>
#include <linux/rfkill-gpio.h>
#include <linux/skbuff.h>
#include <linux/ti_wilink_st.h>
#include <linux/regulator/consumer.h>
#include <linux/smb349-charger.h>
#include <linux/max17048_battery.h>
#include <linux/leds.h>
#include <linux/i2c/at24.h>
#include <linux/of_platform.h>
#include <linux/edp.h>

#include <asm/hardware/gic.h>

#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/pinmux-t11.h>
#include <mach/iomap.h>
#include <mach/io.h>
#include <mach/io_dpd.h>
#include <mach/i2s.h>
#include <mach/tegra_asoc_pdata.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/usb_phy.h>
#include <mach/gpio-tegra.h>
#include <linux/platform_data/tegra_usb_modem_power.h>
#include <mach/hardware.h>
#include <mach/xusb.h>

#include "board-touch-raydium.h"
#include "board.h"
#include "board-common.h"
#include "clock.h"
#include "board-macallan.h"
#include "devices.h"
#include "gpio-names.h"
#include "fuse.h"
#include "pm.h"
#include "pm-irq.h"
#include "common.h"
#include "tegra-board-id.h"
#ifdef CONFIG_MACH_ASUSTEK
#include <mach/board_asustek.h>
#endif

#ifdef CONFIG_BT_BLUESLEEP
static struct rfkill_gpio_platform_data macallan_bt_rfkill_pdata = {
        .name           = "bt_rfkill",
        .shutdown_gpio  = TEGRA_GPIO_PQ7,
        .reset_gpio = TEGRA_GPIO_PQ6,
        .type           = RFKILL_TYPE_BLUETOOTH,
};

static struct platform_device macallan_bt_rfkill_device = {
    .name = "rfkill_gpio",
    .id             = -1,
    .dev = {
        .platform_data = &macallan_bt_rfkill_pdata,
    },
};

static struct resource macallan_bluesleep_resources[] = {
    [0] = {
        .name = "gpio_host_wake",
            .start  = TEGRA_GPIO_PU6,
            .end    = TEGRA_GPIO_PU6,
            .flags  = IORESOURCE_IO,
    },
    [1] = {
        .name = "gpio_ext_wake",
            .start  = TEGRA_GPIO_PEE1,
            .end    = TEGRA_GPIO_PEE1,
            .flags  = IORESOURCE_IO,
    },
    [2] = {
        .name = "host_wake",
            .flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
    },
};

tatic struct platform_device macallan_bluesleep_device = {
    .name           = "bluesleep",
    .id             = -1,
    .num_resources  = ARRAY_SIZE(macallan_bluesleep_resources),
    .resource       = macallan_bluesleep_resources,
};

static noinline void __init macallan_setup_bt_rfkill(void)
{
    platform_device_register(&macallan_bt_rfkill_device);
}

static noinline void __init macallan_setup_bluesleep(void)
{
    macallan_bluesleep_resources[2].start =
        macallan_bluesleep_resources[2].end =
            gpio_to_irq(TEGRA_GPIO_PU6);
    platform_device_register(&macallan_bluesleep_device);
    return;
}
#elif defined CONFIG_BLUEDROID_PM
static struct resource macallan_bluedroid_pm_resources[] = {
    [0] = {
        .name   = "shutdown_gpio",
        .start  = TEGRA_GPIO_PQ7,
        .end    = TEGRA_GPIO_PQ7,
        .flags  = IORESOURCE_IO,
    },
    [1] = {
        .name = "host_wake",
        .flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
    },
    [2] = {
        .name = "gpio_ext_wake",
        .start  = TEGRA_GPIO_PEE1,
        .end    = TEGRA_GPIO_PEE1,
        .flags  = IORESOURCE_IO,
    },
    [3] = {
        .name = "gpio_host_wake",
        .start  = TEGRA_GPIO_PU6,
        .end    = TEGRA_GPIO_PU6,
        .flags  = IORESOURCE_IO,
    },
};

static struct platform_device macallan_bluedroid_pm_device = {
    .name = "bluedroid_pm",
    .id             = 0,
    .num_resources  = ARRAY_SIZE(macallan_bluedroid_pm_resources),
    .resource       = macallan_bluedroid_pm_resources,
};

static noinline void __init macallan_setup_bluedroid_pm(void)
{
    macallan_bluedroid_pm_resources[1].start =
        macallan_bluedroid_pm_resources[1].end =
                gpio_to_irq(TEGRA_GPIO_PU6);
    platform_device_register(&macallan_bluedroid_pm_device);
}
#endif

static __initdata struct tegra_clk_init_table macallan_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "pll_m",	NULL,		0,		false},
	{ "hda",	"pll_p",	108000000,	false},
	{ "hda2codec_2x", "pll_p",	48000000,	false},
	{ "pwm",	"pll_p",	3187500,	false},
	{ "blink",	"clk_32k",	32768,		false},
	{ "i2s1",	"pll_a_out0",	0,		false},
	{ "i2s3",	"pll_a_out0",	0,		false},
	{ "i2s4",	"pll_a_out0",	0,		false},
	{ "spdif_out",	"pll_a_out0",	0,		false},
	{ "d_audio",	"clk_m",	12000000,	false},
	{ "dam0",	"clk_m",	12000000,	false},
	{ "dam1",	"clk_m",	12000000,	false},
	{ "dam2",	"clk_m",	12000000,	false},
	{ "audio1",	"i2s1_sync",	0,		false},
	{ "audio3",	"i2s3_sync",	0,		false},
	/* Setting vi_sensor-clk to true for validation purpose, will imapact
	 * power, later set to be false.*/
	{ "vi_sensor",	"pll_p",	150000000,	false},
	{ "cilab",	"pll_p",	150000000,	false},
	{ "cilcd",	"pll_p",	150000000,	false},
	{ "cile",	"pll_p",	150000000,	false},
	{ "i2c1",	"pll_p",	3200000,	false},
	{ "i2c2",	"pll_p",	3200000,	false},
	{ "i2c3",	"pll_p",	3200000,	false},
	{ "i2c4",	"pll_p",	3200000,	false},
	{ "i2c5",	"pll_p",	3200000,	false},
	{ NULL,		NULL,		0,		0},
};

static struct tegra_i2c_platform_data macallan_i2c1_platform_data = {
	.adapter_nr	= 0,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.scl_gpio		= {TEGRA_GPIO_I2C1_SCL, 0},
	.sda_gpio		= {TEGRA_GPIO_I2C1_SDA, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data macallan_i2c2_platform_data = {
	.adapter_nr	= 1,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.is_clkon_always = true,
	.scl_gpio		= {TEGRA_GPIO_I2C2_SCL, 0},
	.sda_gpio		= {TEGRA_GPIO_I2C2_SDA, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data macallan_i2c3_platform_data = {
	.adapter_nr	= 2,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
	.scl_gpio		= {TEGRA_GPIO_I2C3_SCL, 0},
	.sda_gpio		= {TEGRA_GPIO_I2C3_SDA, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data macallan_i2c4_platform_data = {
	.adapter_nr	= 3,
	.bus_count	= 1,
	.bus_clk_rate	= { 10000, 0 },
	.scl_gpio		= {TEGRA_GPIO_I2C4_SCL, 0},
	.sda_gpio		= {TEGRA_GPIO_I2C4_SDA, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data macallan_i2c5_platform_data = {
	.adapter_nr	= 4,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
	.scl_gpio		= {TEGRA_GPIO_I2C5_SCL, 0},
	.sda_gpio		= {TEGRA_GPIO_I2C5_SDA, 0},
	.arb_recovery = arb_lost_recovery,
};

//static struct i2c_board_info __initdata rt5640_board_info = {
//	I2C_BOARD_INFO("rt5640", 0x1c),
//};

static struct i2c_board_info __initdata rt5639_board_info = {
	I2C_BOARD_INFO("rt5639", 0x1c),
};

static struct pn544_i2c_platform_data nfc_pdata = {
	.irq_gpio = TEGRA_GPIO_PW2,
	.ven_gpio = TEGRA_GPIO_PQ3,
	.firm_gpio = TEGRA_GPIO_PH0,
};

static struct i2c_board_info __initdata nfc_board_info = {
	I2C_BOARD_INFO("pn544", 0x28),
	.platform_data = &nfc_pdata,
};

static struct i2c_board_info __initdata macallan_i2c_asuspec_info[] = {
	{
		I2C_BOARD_INFO("asuspec", 0x17),
	},
};


static struct i2c_board_info __initdata macallan_i2c_asuspec_haydn_info[] = {
	{
		I2C_BOARD_INFO("asuspec", 0x15),
	},
};

static struct i2c_board_info __initdata macallan_i2c_aw8ec_info[] = {
        {
                I2C_BOARD_INFO("aw8ec", 0x19),
        },
};



static void macallan_i2c_init(void)
{
	struct board_info board_info;
	hw_rev revision = asustek_get_hw_rev();

	tegra_get_board_info(&board_info);
	tegra11_i2c_device1.dev.platform_data = &macallan_i2c1_platform_data;
	tegra11_i2c_device2.dev.platform_data = &macallan_i2c2_platform_data;
	tegra11_i2c_device3.dev.platform_data = &macallan_i2c3_platform_data;
	tegra11_i2c_device4.dev.platform_data = &macallan_i2c4_platform_data;
	tegra11_i2c_device5.dev.platform_data = &macallan_i2c5_platform_data;

	nfc_board_info.irq = gpio_to_irq(TEGRA_GPIO_PW2);
	i2c_register_board_info(0, &nfc_board_info, 1);

	if(machine_is_haydn()){
		i2c_register_board_info(1, macallan_i2c_asuspec_haydn_info, ARRAY_SIZE(macallan_i2c_asuspec_haydn_info));
		i2c_register_board_info(1, macallan_i2c_aw8ec_info, ARRAY_SIZE(macallan_i2c_aw8ec_info));
	}
	else if(machine_is_mozart()){
		switch (revision) {
		case HW_REV_A://for SR1
			break;
		case HW_REV_C://for ER
			i2c_register_board_info(1, macallan_i2c_asuspec_info, ARRAY_SIZE(macallan_i2c_asuspec_info));
			break;
		case HW_REV_E://for SR2
			i2c_register_board_info(1, macallan_i2c_asuspec_info, ARRAY_SIZE(macallan_i2c_asuspec_info));
			break;
		default:
			i2c_register_board_info(1, macallan_i2c_asuspec_info, ARRAY_SIZE(macallan_i2c_asuspec_info));
			break;
		}
	}

	platform_device_register(&tegra11_i2c_device5);
	platform_device_register(&tegra11_i2c_device4);
	platform_device_register(&tegra11_i2c_device3);
	platform_device_register(&tegra11_i2c_device2);
	platform_device_register(&tegra11_i2c_device1);

	i2c_register_board_info(0, &rt5639_board_info, 1);
}

static struct platform_device *macallan_uart_devices[] __initdata = {
	&tegra_uarta_device,
	&tegra_uartb_device,
	&tegra_uartc_device,
	&tegra_uartd_device,
};
static struct uart_clk_parent uart_parent_clk[] = {
	[0] = {.name = "clk_m"},
	[1] = {.name = "pll_p"},
#ifndef CONFIG_TEGRA_PLLM_RESTRICTED
	[2] = {.name = "pll_m"},
#endif
};

static struct tegra_uart_platform_data macallan_uart_pdata;
static struct tegra_uart_platform_data macallan_loopback_uart_pdata;

static void __init uart_debug_init(void)
{
	int debug_port_id;

	debug_port_id = uart_console_debug_init(3);
	if (debug_port_id < 0)
		return;

	macallan_uart_devices[debug_port_id] = uart_console_debug_device;
}

static void __init macallan_uart_init(void)
{
	struct clk *c;
	int i;

	for (i = 0; i < ARRAY_SIZE(uart_parent_clk); ++i) {
		c = tegra_get_clock_by_name(uart_parent_clk[i].name);
		if (IS_ERR_OR_NULL(c)) {
			pr_err("Not able to get the clock for %s\n",
						uart_parent_clk[i].name);
			continue;
		}
		uart_parent_clk[i].parent_clk = c;
		uart_parent_clk[i].fixed_clk_rate = clk_get_rate(c);
	}
	macallan_uart_pdata.parent_clk_list = uart_parent_clk;
	macallan_uart_pdata.parent_clk_count = ARRAY_SIZE(uart_parent_clk);
	macallan_loopback_uart_pdata.parent_clk_list = uart_parent_clk;
	macallan_loopback_uart_pdata.parent_clk_count =
						ARRAY_SIZE(uart_parent_clk);
	macallan_loopback_uart_pdata.is_loopback = true;
	tegra_uarta_device.dev.platform_data = &macallan_uart_pdata;
	tegra_uartb_device.dev.platform_data = &macallan_uart_pdata;
	tegra_uartc_device.dev.platform_data = &macallan_uart_pdata;
	tegra_uartd_device.dev.platform_data = &macallan_uart_pdata;

	/* Register low speed only if it is selected */
	if (!is_tegra_debug_uartport_hs())
		uart_debug_init();

	platform_add_devices(macallan_uart_devices,
				ARRAY_SIZE(macallan_uart_devices));
}

static struct resource tegra_rtc_resources[] = {
	[0] = {
		.start = TEGRA_RTC_BASE,
		.end = TEGRA_RTC_BASE + TEGRA_RTC_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_RTC,
		.end = INT_RTC,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device tegra_rtc_device = {
	.name = "tegra_rtc",
	.id   = -1,
	.resource = tegra_rtc_resources,
	.num_resources = ARRAY_SIZE(tegra_rtc_resources),
};

static struct tegra_asoc_platform_data macallan_audio_pdata = {
	.gpio_spkr_en		= TEGRA_GPIO_SPKR_EN,
	.gpio_hp_det		= TEGRA_GPIO_HP_DET,
	.gpio_hp_mute		= -1,
	.gpio_int_mic_en	= TEGRA_GPIO_INT_MIC_EN,
	.gpio_ext_mic_en	= TEGRA_GPIO_EXT_MIC_EN,
	.gpio_ldo1_en		= TEGRA_GPIO_LDO1_EN,
	.edp_support		= true,
	.edp_states		= {1100, 1100, 0},
	.edp_vol		= {0x8, 0x8, 0x27},
	//.gpio_codec1 = TEGRA_GPIO_CODEC1_EN,
	.gpio_codec2 = TEGRA_GPIO_CODEC2_EN,
	.gpio_codec3 = TEGRA_GPIO_CODEC3_EN,
	.i2s_param[HIFI_CODEC]	= {
		.audio_port_id	= 1,
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_I2S,
	},
	.i2s_param[BT_SCO]	= {
		.audio_port_id	= 3,
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_DSP_A,
	},
};

/*static struct platform_device macallan_audio_device = {
	.name	= "tegra-snd-rt5640",
	.id	= 0,
	.dev	= {
		.platform_data = &macallan_audio_pdata,
	},
};*/

static struct platform_device macallan_audio_rt5639_device = {
	.name	= "tegra-snd-rt5639",
	.id	= 0,
	.dev	= {
		.platform_data = &macallan_audio_pdata,
	},
};

static struct platform_device *macallan_devices[] __initdata = {
	&tegra_pmu_device,
	&tegra_rtc_device,
	&tegra_udc_device,
#if defined(CONFIG_TEGRA_IOVMM_SMMU) || defined(CONFIG_TEGRA_IOMMU_SMMU)
	&tegra_smmu_device,
#endif
#if defined(CONFIG_TEGRA_AVP)
	&tegra_avp_device,
#endif
#if defined(CONFIG_CRYPTO_DEV_TEGRA_SE)
	&tegra11_se_device,
#endif
	&tegra_ahub_device,
	&tegra_dam_device0,
	&tegra_dam_device1,
	&tegra_dam_device2,
	&tegra_i2s_device1,
	&tegra_i2s_device3,
	&tegra_i2s_device4,
	&tegra_spdif_device,
	&spdif_dit_device,
	&bluetooth_dit_device,
	&tegra_pcm_device,
	&macallan_audio_rt5639_device,
	&tegra_hda_device,
#if defined(CONFIG_CRYPTO_DEV_TEGRA_AES)
	&tegra_aes_device,
#endif
};

#ifdef CONFIG_USB_SUPPORT
static struct tegra_usb_platform_data tegra_udc_pdata = {
	.port_otg = true,
	.has_hostpc = true,
	.support_pmu_vbus = true,
	.id_det_type = TEGRA_USB_PMU_ID,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.unaligned_dma_buf_supported = false,
	.op_mode = TEGRA_USB_OPMODE_DEVICE,
	.u_data.dev = {
		.vbus_pmu_irq = 0,
		.vbus_gpio = -1,
		.charging_supported = false,
		.remote_wakeup_supported = false,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 8,
		.xcvr_lsfslew = 0,
		.xcvr_lsrslew = 3,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
	},
};

static struct tegra_usb_platform_data tegra_ehci1_utmi_pdata = {
	.port_otg = true,
	.has_hostpc = true,
	.support_pmu_vbus = true,
	.id_det_type = TEGRA_USB_PMU_ID,
	.unaligned_dma_buf_supported = false,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.hot_plug = false,
		.remote_wakeup_supported = true,
		.power_off_on_suspend = true,
		.turn_off_vbus_on_lp0 = true,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 15,
		.xcvr_lsfslew = 0,
		.xcvr_lsrslew = 3,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
		.vbus_oc_map = 0x4,
	},
};

static struct tegra_usb_platform_data tegra_ehci3_utmi_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.support_pmu_vbus = true,
	.id_det_type = TEGRA_USB_PMU_ID,
	.unaligned_dma_buf_supported = false,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.hot_plug = false,
		.remote_wakeup_supported = true,
		.power_off_on_suspend = true,
		.turn_off_vbus_on_lp0 = true,
	},
	.u_cfg.utmi = {
	.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 8,
		.xcvr_lsfslew = 0,
		.xcvr_lsrslew = 3,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
		.vbus_oc_map = 0x5,
	},
};

static struct tegra_usb_otg_data tegra_otg_pdata = {
	.ehci_device = &tegra_ehci1_device,
	.ehci_pdata = &tegra_ehci1_utmi_pdata,
	.vbus_extcon_dev_name = "palmas-extcon",
	.id_extcon_dev_name = "palmas-extcon",
};

static void macallan_usb_init(void)
{
	int usb_port_owner_info = tegra_get_usb_port_owner_info();

	/* Set USB wake sources for macallan */
	tegra_set_usb_wake_source();

	if (!(usb_port_owner_info & UTMI1_PORT_OWNER_XUSB)) {
		tegra_otg_device.dev.platform_data = &tegra_otg_pdata;
		platform_device_register(&tegra_otg_device);
		/* Setup the udc platform data */
		tegra_udc_device.dev.platform_data = &tegra_udc_pdata;
	}

	if (!(usb_port_owner_info & UTMI2_PORT_OWNER_XUSB)) {
		tegra_ehci3_device.dev.platform_data = &tegra_ehci3_utmi_pdata;
		platform_device_register(&tegra_ehci3_device);
	}
}

static struct tegra_xusb_board_data xusb_bdata = {
	.portmap = TEGRA_XUSB_SS_P0 | TEGRA_XUSB_USB2_P1,
	/* ss_portmap[0:3] = SS0 map, ss_portmap[4:7] = SS1 map */
	.ss_portmap = (TEGRA_XUSB_SS_PORT_MAP_USB2_P1 << 0),
};

static void macallan_xusb_init(void)
{
	int usb_port_owner_info = tegra_get_usb_port_owner_info();

	if (usb_port_owner_info & UTMI2_PORT_OWNER_XUSB) {
		tegra_xusb_init(&xusb_bdata);
		tegra_xusb_register();
	}

}

static struct gpio modem_gpios[] = { /* Nemo modem */
	{MODEM_EN, GPIOF_OUT_INIT_HIGH, "MODEM EN"},
	{MDM_RST, GPIOF_OUT_INIT_LOW, "MODEM RESET"},
};

static struct tegra_usb_platform_data tegra_ehci2_hsic_baseband_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.unaligned_dma_buf_supported = false,
	.phy_intf = TEGRA_USB_PHY_INTF_HSIC,
	.op_mode = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.hot_plug = false,
		.remote_wakeup_supported = true,
		.power_off_on_suspend = true,
	},
};

static int baseband_init(void)
{
	int ret;

	ret = gpio_request_array(modem_gpios, ARRAY_SIZE(modem_gpios));
	if (ret) {
		pr_warn("%s:gpio request failed\n", __func__);
		return ret;
	}

	/* enable pull-down for MDM_COLD_BOOT */
	tegra_pinmux_set_pullupdown(TEGRA_PINGROUP_ULPI_DATA4,
				    TEGRA_PUPD_PULL_DOWN);

	/* export GPIO for user space access through sysfs */
	gpio_export(MDM_RST, false);

	return 0;
}

static void baseband_stop(void)
{
	/* place baseband into reset state */
	gpio_set_value(MDM_RST, 0);
}

static const struct tegra_modem_operations baseband_operations = {
	.init = baseband_init,
	.stop = baseband_stop,
};

static struct tegra_usb_modem_power_platform_data baseband_pdata = {
	.ops = &baseband_operations,
	.wake_gpio = -1,
	.boot_gpio = MDM_COLDBOOT,
	.boot_irq_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.autosuspend_delay = 2000,
	.short_autosuspend_delay = 50,
	.tegra_ehci_device = &tegra_ehci2_device,
	.tegra_ehci_pdata = &tegra_ehci2_hsic_baseband_pdata,
};

static struct platform_device icera_nemo_device = {
	.name = "tegra_usb_modem_power",
	.id = -1,
	.dev = {
		.platform_data = &baseband_pdata,
	},
};

static void macallan_modem_init(void)
{
	int modem_id = tegra_get_modem_id();
	int usb_port_owner_info = tegra_get_usb_port_owner_info();
	switch (modem_id) {
	case TEGRA_BB_NEMO: /* on board i500 HSIC */
		if (!(usb_port_owner_info & HSIC1_PORT_OWNER_XUSB))
			platform_device_register(&icera_nemo_device);
		break;
	}
}

#else
static void macallan_usb_init(void) { }
static void macallan_modem_init(void) { }
#endif

/*static void macallan_audio_init(void)
{
	struct board_info board_info;

	tegra_get_board_info(&board_info);

	macallan_audio_pdata.codec_name = "rt5640.0-001c";
	macallan_audio_pdata.codec_dai_name = "rt5640-aif1";
}*/

static void macallan_audio_rt5639_init(void)
{
	struct board_info board_info;

	tegra_get_board_info(&board_info);

	macallan_audio_pdata.codec_name = "rt5639.0-001c";
	macallan_audio_pdata.codec_dai_name = "rt5639-aif1";
}


static struct platform_device *macallan_spi_devices[] __initdata = {
	&tegra11_spi_device1,
};

struct spi_clk_parent spi_parent_clk_macallan[] = {
	[0] = {.name = "pll_p"},
#ifndef CONFIG_TEGRA_PLLM_RESTRICTED
	[1] = {.name = "pll_m"},
	[2] = {.name = "clk_m"},
#else
	[1] = {.name = "clk_m"},
#endif
};

static struct tegra_spi_platform_data macallan_spi_pdata = {
	.max_dma_buffer         = 16 * 1024,
	.is_clkon_always        = false,
	.max_rate               = 25000000,
};

static void __init macallan_spi_init(void)
{
	int i;
	struct clk *c;
	struct board_info board_info, display_board_info;

	tegra_get_board_info(&board_info);
	tegra_get_display_board_info(&display_board_info);

	for (i = 0; i < ARRAY_SIZE(spi_parent_clk_macallan); ++i) {
		c = tegra_get_clock_by_name(spi_parent_clk_macallan[i].name);
		if (IS_ERR_OR_NULL(c)) {
			pr_err("Not able to get the clock for %s\n",
					spi_parent_clk_macallan[i].name);
			continue;
		}
		spi_parent_clk_macallan[i].parent_clk = c;
		spi_parent_clk_macallan[i].fixed_clk_rate = clk_get_rate(c);
	}
	macallan_spi_pdata.parent_clk_list = spi_parent_clk_macallan;
	macallan_spi_pdata.parent_clk_count = ARRAY_SIZE(spi_parent_clk_macallan);
	macallan_spi_pdata.is_dma_based = (tegra_revision == TEGRA_REVISION_A01)
							? false : true ;
	tegra11_spi_device1.dev.platform_data = &macallan_spi_pdata;
	platform_add_devices(macallan_spi_devices,
				ARRAY_SIZE(macallan_spi_devices));
}

static __initdata struct tegra_clk_init_table touch_clk_init_table[] = {
	/* name         parent          rate            enabled */
	{ "extern2",    "pll_p",        41000000,       false},
	{ "clk_out_2",  "extern2",      40800000,       false},
	{ NULL,         NULL,           0,              0},
};

// SPI1_CS
static struct tegra_pingroup_config rm31080_suspend_pinmux =
	GPIO_PINMUX(ULPI_STP,  PULL_DOWN,
		NORMAL,   INPUT,   DISABLE);
static struct tegra_pingroup_config rm31080_resume_pinmux =
	DEFAULT_PINMUX(ULPI_STP,      SPI1,
		NORMAL,    NORMAL,   OUTPUT);

static void rm31080_suspend_mode(void)
{
  tegra_pinmux_config_table(&rm31080_suspend_pinmux, 1);
}

static void rm31080_resume_mode(void)
{
  tegra_pinmux_config_table(&rm31080_resume_pinmux, 1);
}
struct rm_spi_ts_platform_data rm31080ts_macallan_data = {
	.gpio_reset = TOUCH_GPIO_RST_RAYDIUM_SPI,
	.config = 0,
	.platform_id = RM_PLATFORM_D010,
	.name_of_clock = "clk_out_2",
	.name_of_clock_con = "extern2",
	.suspend_pinmux = rm31080_suspend_mode,
	.resume_pinmux = rm31080_resume_mode,
};

static struct tegra_spi_device_controller_data dev_cdata = {
	.rx_clk_tap_delay = 0,
	.tx_clk_tap_delay = 16,
};

struct spi_board_info rm31080a_macallan_spi_board[1] = {
	{
	 .modalias = "rm_ts_spidev",
	 .bus_num = 0,
	 .chip_select = 0,
	 .max_speed_hz = 8 * 1000 * 1000,
	 .mode = SPI_MODE_0,
	 .controller_data = &dev_cdata,
	 .platform_data = &rm31080ts_macallan_data,
	 },
};

static int __init macallan_touch_init(void)
{
	struct board_info board_info;

	tegra_get_display_board_info(&board_info);
	tegra_clk_init_from_table(touch_clk_init_table);

	if(machine_is_haydn()) {
           gpio_request(TEGRA_GPIO_PH0, "sis_tp_power");
           gpio_direction_output(TEGRA_GPIO_PH0, 1);
        }

	if(machine_is_mozart()){
		if (board_info.board_id == BOARD_E1582)
			rm31080ts_macallan_data.platform_id = RM_PLATFORM_P005;
		else
			rm31080ts_macallan_data.platform_id = RM_PLATFORM_D010;
		mdelay(20);
		rm31080a_macallan_spi_board[0].irq = gpio_to_irq(TOUCH_GPIO_IRQ_RAYDIUM_SPI);
		touch_init_raydium(TOUCH_GPIO_IRQ_RAYDIUM_SPI,
					TOUCH_GPIO_RST_RAYDIUM_SPI,
					&rm31080ts_macallan_data,
					&rm31080a_macallan_spi_board[0],
					ARRAY_SIZE(rm31080a_macallan_spi_board));
	}
	return 0;
}

static int __init macallan_wdt_init(void)
{
	platform_device_register(&tegra_wdt0_device);

	return 0;
}

static void __init tegra_macallan_init(void)
{
	struct board_info board_info;

	//macallan_sysedp_init();
	tegra_get_display_board_info(&board_info);
	tegra_clk_init_from_table(macallan_clk_init_table);
	tegra_clk_verify_parents();
	tegra_soc_device_init("macallan");
	tegra_enable_pinmux();
	macallan_pinmux_init();
#ifdef CONFIG_MACH_ASUSTEK
	asustek_add_pcbid_devices();
#endif
	macallan_i2c_init();
	macallan_spi_init();
	macallan_usb_init();
	macallan_xusb_init();
	macallan_uart_init();
	macallan_audio_rt5639_init();
	platform_add_devices(macallan_devices, ARRAY_SIZE(macallan_devices));
	tegra_ram_console_debug_init();
	tegra_io_dpd_init();
	macallan_regulator_init();
	macallan_sdhci_init();
	macallan_suspend_init();
	macallan_emc_init();
	macallan_edp_init();
	macallan_touch_init();
	macallan_panel_init();
	macallan_kbc_init();

	if(machine_is_mozart())
	{
		macallan_pmon_init();
	}
#ifdef CONFIG_BT_BLUESLEEP
	macallan_setup_bluesleep();
	macallan_setup_bt_rfkill();
#elif defined CONFIG_BLUEDROID_PM
	macallan_setup_bluedroid_pm();
#endif
	macallan_modem_init();
	macallan_wdt_init();
#ifdef CONFIG_TEGRA_WDT_RECOVERY
	tegra_wdt_recovery_init();
#endif
	macallan_sensors_init();
	macallan_soctherm_init();
	tegra_register_fuse();
	macallan_sysedp_core_init();
	macallan_sysedp_psydepl_init();

}

static void __init macallan_ramconsole_reserve(unsigned long size)
{
	tegra_ram_console_debug_reserve(SZ_1M);
}

static void __init tegra_macallan_dt_init(void)
{
#ifdef CONFIG_USE_OF
	of_platform_populate(NULL,
		of_default_bus_match_table, NULL, NULL);
#endif

	tegra_macallan_init();
}

static void __init tegra_macallan_reserve(void)
{
#if defined(CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM)
	/* 1920*1200*4*2 = 18432000 bytes */
	tegra_reserve(0, SZ_16M + SZ_2M, SZ_16M);
#else
	tegra_reserve(SZ_128M, SZ_16M + SZ_2M, SZ_4M);
#endif
	macallan_ramconsole_reserve(SZ_1M);
}

static const char * const macallan_dt_board_compat[] = {
	"nvidia,macallan",
	NULL
};

static const char * const mozart_dt_board_compat[] = {
	"nvidia,mozart",
	NULL
};

static const char * const haydn_dt_board_compat[] = {
	"nvidia,haydn",
	NULL
};

MACHINE_START(MACALLAN, "macallan")
	.atag_offset	= 0x100,
	.soc		= &tegra_soc_desc,
	.map_io		= tegra_map_common_io,
	.reserve	= tegra_macallan_reserve,
	.init_early	= tegra11x_init_early,
	.init_irq	= tegra_init_irq,
	.handle_irq	= gic_handle_irq,
	.timer		= &tegra_timer,
	.init_machine	= tegra_macallan_dt_init,
	.restart	= tegra_assert_system_reset,
	.dt_compat	= macallan_dt_board_compat,
MACHINE_END

MACHINE_START(MOZART, "mozart")
	.atag_offset	= 0x100,
	.soc		= &tegra_soc_desc,
	.map_io		= tegra_map_common_io,
	.reserve	= tegra_macallan_reserve,
	.init_early	= tegra11x_init_early,
	.init_irq	= tegra_init_irq,
	.handle_irq	= gic_handle_irq,
	.timer		= &tegra_timer,
	.init_machine	= tegra_macallan_dt_init,
	.restart	= tegra_assert_system_reset,
	.dt_compat	= mozart_dt_board_compat,
MACHINE_END

MACHINE_START(HAYDN, "haydn")
	.atag_offset	= 0x100,
	.soc		= &tegra_soc_desc,
	.map_io		= tegra_map_common_io,
	.reserve	= tegra_macallan_reserve,
	.init_early	= tegra11x_init_early,
	.init_irq	= tegra_init_irq,
	.handle_irq	= gic_handle_irq,
	.timer		= &tegra_timer,
	.init_machine	= tegra_macallan_dt_init,
	.restart	= tegra_assert_system_reset,
	.dt_compat	= haydn_dt_board_compat,
MACHINE_END
