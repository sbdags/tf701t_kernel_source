/*
 * arch/arm/mach-tegra/board-dalmore-pinmux-t11x.h
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth floor, Boston, MA  02110-1301, USA
 */

static __initdata struct tegra_pingroup_config dalmore_pinmux_common[] = {

	/* EXTPERIPH1 pinmux */
	DEFAULT_PINMUX(CLK1_OUT,      EXTPERIPH1,  NORMAL,    NORMAL,   OUTPUT),

	/* I2S0 pinmux */
	DEFAULT_PINMUX(DAP1_DIN,      I2S0,        NORMAL,    TRISTATE, INPUT),
	DEFAULT_PINMUX(DAP1_DOUT,     I2S0,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(DAP1_FS,       I2S0,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(DAP1_SCLK,     I2S0,        NORMAL,    NORMAL,   INPUT),

	/* I2S1 pinmux */
	DEFAULT_PINMUX(DAP2_DIN,      I2S1,        NORMAL,    TRISTATE, INPUT),
	DEFAULT_PINMUX(DAP2_DOUT,     I2S1,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(DAP2_FS,       I2S1,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(DAP2_SCLK,     I2S1,        NORMAL,    NORMAL,   INPUT),

	/* I2S3 pinmux */
	DEFAULT_PINMUX(DAP4_DIN,      I2S3,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(DAP4_DOUT,     I2S3,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(DAP4_FS,       I2S3,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(DAP4_SCLK,     I2S3,        NORMAL,    NORMAL,   INPUT),

	/* CLDVFS pinmux */
	DEFAULT_PINMUX(DVFS_PWM,      CLDVFS,      NORMAL,    NORMAL,   OUTPUT),
	DEFAULT_PINMUX(DVFS_CLK,      CLDVFS,      NORMAL,    NORMAL,   OUTPUT),

	/* ULPI pinmux */
	DEFAULT_PINMUX(ULPI_CLK,      ULPI,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(ULPI_DATA0,    ULPI,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(ULPI_DATA1,    ULPI,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(ULPI_DATA2,    ULPI,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(ULPI_DATA3,    ULPI,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(ULPI_DATA4,    ULPI,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(ULPI_DATA5,    ULPI,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(ULPI_DATA6,    ULPI,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(ULPI_DATA7,    ULPI,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(ULPI_DIR,      ULPI,        NORMAL,    TRISTATE, INPUT),
	DEFAULT_PINMUX(ULPI_NXT,      ULPI,        NORMAL,    TRISTATE, INPUT),
	DEFAULT_PINMUX(ULPI_STP,      ULPI,        NORMAL,    NORMAL,   OUTPUT),

	/* I2C3 pinmux */
	I2C_PINMUX(CAM_I2C_SCL, I2C3, NORMAL, NORMAL, INPUT, DISABLE, DISABLE),
	I2C_PINMUX(CAM_I2C_SDA, I2C3, NORMAL, NORMAL, INPUT, DISABLE, DISABLE),

	/* VI pinmux */
	VI_PINMUX(CAM_MCLK, VI_ALT3, NORMAL, NORMAL, OUTPUT, DEFAULT, DEFAULT),

	/* VI_ALT1 pinmux */
	VI_PINMUX(GPIO_PBB0, VI_ALT3, NORMAL, NORMAL, OUTPUT, DEFAULT, DEFAULT),

	/* VGP4 pinmux */
	VI_PINMUX(GPIO_PBB4, VGP4, NORMAL, NORMAL, OUTPUT, DEFAULT, DEFAULT),

	/* I2C2 pinmux */
	I2C_PINMUX(GEN2_I2C_SCL, I2C2, NORMAL, NORMAL, INPUT, DISABLE, DISABLE),
	I2C_PINMUX(GEN2_I2C_SDA, I2C2, NORMAL, NORMAL, INPUT, DISABLE, DISABLE),

	/* UARTD pinmux */
	DEFAULT_PINMUX(GMI_A16,       UARTD,       NORMAL,    NORMAL,   OUTPUT),
	DEFAULT_PINMUX(GMI_A17,       UARTD,       NORMAL,    TRISTATE, INPUT),
	DEFAULT_PINMUX(GMI_A18,       UARTD,       NORMAL,    TRISTATE, INPUT),
	DEFAULT_PINMUX(GMI_A19,       UARTD,       NORMAL,    NORMAL,   OUTPUT),

	/* SPI4 pinmux */
	DEFAULT_PINMUX(GMI_AD5,       SPI4,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(GMI_AD6,       SPI4,        NORMAL,    NORMAL,   OUTPUT),
	DEFAULT_PINMUX(GMI_AD7,       SPI4,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(GMI_CS6_N,     SPI4,        NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(GMI_WR_N,      SPI4,        NORMAL,    NORMAL,   INPUT),

	/* PWM1 pinmux */
	DEFAULT_PINMUX(GMI_AD9,       PWM1,        NORMAL,    NORMAL,   OUTPUT),

	/* SOC pinmux */
	DEFAULT_PINMUX(GMI_CS1_N,     SOC,         NORMAL,    TRISTATE, INPUT),
	DEFAULT_PINMUX(GMI_OE_N,      SOC,         NORMAL,    TRISTATE, INPUT),

	/* EXTPERIPH2 pinmux */
	DEFAULT_PINMUX(CLK2_OUT,      EXTPERIPH2,  NORMAL,    NORMAL,   OUTPUT),

	/* SDMMC1 pinmux */
	DEFAULT_PINMUX(SDMMC1_CLK,    SDMMC1,      NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC1_CMD,    SDMMC1,      PULL_UP,   NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC1_DAT0,   SDMMC1,      PULL_UP,   NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC1_DAT1,   SDMMC1,      PULL_UP,   NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC1_DAT2,   SDMMC1,      PULL_UP,   NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC1_DAT3,   SDMMC1,      PULL_UP,   NORMAL,   INPUT),

	/* SDMMC3 pinmux */
	DEFAULT_PINMUX(SDMMC3_CLK,    SDMMC3,      NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC3_CMD,    SDMMC3,      PULL_UP,   NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC3_DAT0,   SDMMC3,      PULL_UP,   NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC3_DAT1,   SDMMC3,      PULL_UP,   NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC3_DAT2,   SDMMC3,      PULL_UP,   NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC3_DAT3,   SDMMC3,      PULL_UP,   NORMAL,   INPUT),
	DEFAULT_PINMUX(KB_COL4,       SDMMC3,      PULL_UP,   NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC3_CD_N,   SDMMC3,      PULL_UP,   NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC3_CLK_LB_OUT,   SDMMC3,      PULL_UP,   NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC3_CLK_LB_IN,   SDMMC3,      PULL_UP,   NORMAL,   INPUT),

	/* SDMMC4 pinmux */
	DEFAULT_PINMUX(SDMMC4_CLK,    SDMMC4,      NORMAL,    NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC4_CMD,    SDMMC4,      PULL_UP,   NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT0,   SDMMC4,      PULL_UP,   NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT1,   SDMMC4,      PULL_UP,   NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT2,   SDMMC4,      PULL_UP,   NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT3,   SDMMC4,      PULL_UP,   NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT4,   SDMMC4,      PULL_UP,   NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT5,   SDMMC4,      PULL_UP,   NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT6,   SDMMC4,      PULL_UP,   NORMAL,   INPUT),
	DEFAULT_PINMUX(SDMMC4_DAT7,   SDMMC4,      PULL_UP,   NORMAL,   INPUT),

	/* BLINK pinmux */
	DEFAULT_PINMUX(CLK_32K_OUT,   BLINK,       NORMAL,    NORMAL,   OUTPUT),

	/* KBC pinmux */
	DEFAULT_PINMUX(KB_COL0,       KBC,         PULL_UP,   NORMAL, INPUT),
	DEFAULT_PINMUX(KB_COL1,       KBC,         PULL_UP,   NORMAL, INPUT),
	DEFAULT_PINMUX(KB_COL2,       KBC,         PULL_UP,   NORMAL, INPUT),
	DEFAULT_PINMUX(KB_ROW0,       KBC,         PULL_UP,   NORMAL, INPUT),
	DEFAULT_PINMUX(KB_ROW1,       KBC,         PULL_UP,   NORMAL, INPUT),
	DEFAULT_PINMUX(KB_ROW2,       KBC,         PULL_UP,   NORMAL, INPUT),

	/* UARTA pinmux */
	DEFAULT_PINMUX(KB_ROW10,      UARTA,       NORMAL,    TRISTATE, INPUT),
	DEFAULT_PINMUX(KB_ROW9,       UARTA,       NORMAL,    NORMAL,   OUTPUT),

	/* I2CPWR pinmux */
	I2C_PINMUX(PWR_I2C_SCL, I2CPWR, NORMAL, NORMAL, INPUT, DISABLE, DISABLE),
	I2C_PINMUX(PWR_I2C_SDA, I2CPWR, NORMAL, NORMAL, INPUT, DISABLE, DISABLE),

	/* SYSCLK pinmux */
	DEFAULT_PINMUX(SYS_CLK_REQ,   SYSCLK,      NORMAL,    NORMAL,   OUTPUT),

	/* RTCK pinmux */
	DEFAULT_PINMUX(JTAG_RTCK,     RTCK,        NORMAL,    NORMAL, INPUT),

	/* CLK pinmux */
	DEFAULT_PINMUX(CLK_32K_IN,    CLK,         NORMAL,    TRISTATE, INPUT),

	/* PWRON pinmux */
	DEFAULT_PINMUX(CORE_PWR_REQ,  PWRON,       NORMAL,    NORMAL,   OUTPUT),

	/* CPU pinmux */
	DEFAULT_PINMUX(CPU_PWR_REQ,   CPU,         NORMAL,    NORMAL,   OUTPUT),

	/* PMI pinmux */
	DEFAULT_PINMUX(PWR_INT_N,     PMI,         NORMAL,    TRISTATE, INPUT),

	/* RESET_OUT_N pinmux */
	DEFAULT_PINMUX(RESET_OUT_N,   RESET_OUT_N, NORMAL,    NORMAL,   OUTPUT),

	/* EXTPERIPH3 pinmux */
	DEFAULT_PINMUX(CLK3_OUT,      EXTPERIPH3,  NORMAL,    NORMAL,   OUTPUT),

	/* I2C1 pinmux */
	I2C_PINMUX(GEN1_I2C_SCL, I2C1, NORMAL, NORMAL, INPUT, DISABLE, DISABLE),
	I2C_PINMUX(GEN1_I2C_SDA, I2C1, NORMAL, NORMAL, INPUT, DISABLE, DISABLE),

	/* UARTB pinmux */
	DEFAULT_PINMUX(UART2_CTS_N,   UARTB,       NORMAL,    TRISTATE, INPUT),
	DEFAULT_PINMUX(UART2_RTS_N,   UARTB,       NORMAL,    NORMAL,   OUTPUT),

	/* IRDA pinmux */
	DEFAULT_PINMUX(UART2_RXD,     IRDA,        NORMAL,    TRISTATE, INPUT),
	DEFAULT_PINMUX(UART2_TXD,     IRDA,        NORMAL,    NORMAL,   OUTPUT),

	/* UARTC pinmux */
	DEFAULT_PINMUX(UART3_CTS_N,   UARTC,       NORMAL,    TRISTATE, INPUT),
	DEFAULT_PINMUX(UART3_RTS_N,   UARTC,       NORMAL,    NORMAL,   OUTPUT),
	DEFAULT_PINMUX(UART3_RXD,     UARTC,       NORMAL,    TRISTATE, INPUT),
	DEFAULT_PINMUX(UART3_TXD,     UARTC,       NORMAL,    NORMAL,   OUTPUT),

	/* OWR pinmux */
	DEFAULT_PINMUX(OWR,           OWR,         NORMAL,    NORMAL,   INPUT),

	/* CEC pinmux */
	CEC_PINMUX(HDMI_CEC, CEC, NORMAL, NORMAL, INPUT, DISABLE, DISABLE),

	/* I2C4 pinmux */
	DDC_PINMUX(DDC_SCL, I2C4, NORMAL, NORMAL, INPUT, DISABLE, NORMAL),
	DDC_PINMUX(DDC_SDA, I2C4, NORMAL, NORMAL, INPUT, DISABLE, NORMAL),

	/* USB pinmux */
	USB_PINMUX(SPDIF_IN, USB, NORMAL, NORMAL, INPUT, DISABLE, ENABLE),
	USB_PINMUX(USB_VBUS_EN0, USB, NORMAL, NORMAL, INPUT, DISABLE, ENABLE),

	/* nct */
	DEFAULT_PINMUX(GPIO_X6_AUD,   SPI6,        PULL_UP,   TRISTATE, INPUT),

	/* io rdy */
	DEFAULT_PINMUX(GMI_IORDY,       GMI,         PULL_UP,   NORMAL, INPUT),
};

static __initdata struct tegra_pingroup_config unused_pins_lowpower[] = {
	DEFAULT_PINMUX(CLK1_REQ,      RSVD3,       PULL_DOWN, TRISTATE, OUTPUT),
	DEFAULT_PINMUX(USB_VBUS_EN1,  RSVD3,       PULL_DOWN, TRISTATE, OUTPUT),
	DEFAULT_PINMUX(GMI_WP_N,      GMI,         PULL_DOWN, TRISTATE, OUTPUT),
};

static struct gpio_init_pin_info init_gpio_mode_dalmore_common[] = {
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PX4, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PX5, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PX6, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PX7, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PW2, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PW3, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PX1, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PX3, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PP1, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PP2, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PP0, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PP3, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PV0, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PV1, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PBB3, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PBB5, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PBB6, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PBB7, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PCC1, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PCC2, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PG0, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PG1, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PH1, false, 1),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PH2, false, 1),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PH3, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PH4, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PH5, false, 1),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PH6, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PH7, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PG2, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PG3, true, 1),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PG4, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PH0, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PK0, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PK1, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PJ0, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PK3, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PK4, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PK2, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PI6, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PJ3, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PI4, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PI7, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PC7, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PCC5, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PV3, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PQ3, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PQ5, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PQ6, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PQ7, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PR3, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PR4, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PR5, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PR6, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PR7, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PS0, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PEE1, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PU0, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PU1, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PU2, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PU3, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PU4, false, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PU5, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PU6, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PN7, true, 0),
	GPIO_INIT_PIN_MODE(TEGRA_GPIO_PK5, true, 1),
};
