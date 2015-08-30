/*
 * TI Palmas MFD Driver
 *
 * Copyright 2011-2012 Texas Instruments Inc.
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Graeme Gregory <gg@slimlogic.co.uk>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/regmap.h>
#include <linux/err.h>
#include <linux/mfd/core.h>
#include <linux/mfd/palmas.h>
#include <asm/mach-types.h>

//=================stree test=================
struct palmas *tps65913_palmas = NULL;
struct workqueue_struct *tps65913_strees_work_queue = NULL;

#define TPS65913_IOC_MAGIC		0xFB
#define TPS65913_IOC_MAXNR	5
#define TPS65913_POLLING_DATA _IOR(TPS65913_IOC_MAGIC, 1, int)
#define TEST_END	(0)
#define START_NORMAL 	(1)
#define START_HEAVY	(2)
#define IOCTL_ERROR 	(-1)
//=================stree test end=================

#define EXT_PWR_REQ (PALMAS_EXT_CONTROL_ENABLE1 |	\
			PALMAS_EXT_CONTROL_ENABLE2 |	\
			PALMAS_EXT_CONTROL_NSLEEP)

static const struct resource gpadc_resource[] = {
	{
		.name = "EOC_SW",
		.start = PALMAS_GPADC_EOC_SW_IRQ,
		.end = PALMAS_GPADC_EOC_SW_IRQ,
		.flags = IORESOURCE_IRQ,
	}
};

static const struct resource usb_resource[] = {
	{
		.name = "ID",
		.start = PALMAS_ID_OTG_IRQ,
		.end = PALMAS_ID_OTG_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.name = "ID_WAKEUP",
		.start = PALMAS_ID_IRQ,
		.end = PALMAS_ID_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.name = "VBUS",
		.start = PALMAS_VBUS_OTG_IRQ,
		.end = PALMAS_VBUS_OTG_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.name = "VBUS_WAKEUP",
		.start = PALMAS_VBUS_IRQ,
		.end = PALMAS_VBUS_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static const struct resource palma_extcon_resource[] = {
	{
		.name = "VBUS-IRQ",
		.start = PALMAS_VBUS_IRQ,
		.end = PALMAS_VBUS_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.name = "ID-IRQ",
		.start = PALMAS_ID_IRQ,
		.end = PALMAS_ID_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static const struct resource rtc_resource[] = {
	{
		.name = "RTC_ALARM",
		.start = PALMAS_RTC_ALARM_IRQ,
		.end = PALMAS_RTC_ALARM_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static const struct resource pwron_resource[] = {
	{
		.name = "PWRON_BUTTON",
		.start = PALMAS_PWRON_IRQ,
		.end = PALMAS_PWRON_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static const struct resource wdt_resource[] = {
	{
		.name = "WDT",
		.start = PALMAS_WDT_IRQ,
		.end = PALMAS_WDT_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static const struct resource thermal_resource[] = {
	{
		.name = "palmas-junction-temp",
		.start = PALMAS_HOTDIE_IRQ,
		.end = PALMAS_HOTDIE_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

enum palmas_ids {
	PALMAS_PIN_MUX_ID,
	PALMAS_PMIC_ID,
	PALMAS_GPIO_ID,
	PALMAS_LEDS_ID,
	PALMAS_WDT_ID,
	PALMAS_RTC_ID,
	PALMAS_PWRBUTTON_ID,
	PALMAS_GPADC_ID,
	PALMAS_RESOURCE_ID,
	PALMAS_CLK_ID,
	PALMAS_PWM_ID,
	PALMAS_USB_ID,
	PALMAS_EXTCON_ID,
	PALMAS_THERM_ID,
};

static const struct mfd_cell palmas_children[] = {
	{
		.name = "palmas-pinctrl",
		.id = PALMAS_PIN_MUX_ID,
	},
	{
		.name = "palmas-pmic",
		.id = PALMAS_PMIC_ID,
	},
	{
		.name = "palmas-gpio",
		.id = PALMAS_GPIO_ID,
	},
	{
		.name = "palmas-leds",
		.id = PALMAS_LEDS_ID,
	},
	{
		.name = "palmas-wdt",
		.num_resources = ARRAY_SIZE(wdt_resource),
		.resources = wdt_resource,
		.id = PALMAS_WDT_ID,
	},
	{
		.name = "palmas-rtc",
		.num_resources = ARRAY_SIZE(rtc_resource),
		.resources = rtc_resource,
		.id = PALMAS_RTC_ID,
	},
	{
		.name = "palmas-pwrbutton",
		.num_resources = ARRAY_SIZE(pwron_resource),
		.resources = pwron_resource,
		.id = PALMAS_PWRBUTTON_ID,
	},
	{
		.name = "palmas-gpadc",
		.num_resources = ARRAY_SIZE(gpadc_resource),
		.resources = gpadc_resource,
		.id = PALMAS_GPADC_ID,
	},
	{
		.name = "palmas-resource",
		.id = PALMAS_RESOURCE_ID,
	},
	{
		.name = "palmas-clk",
		.id = PALMAS_CLK_ID,
	},
	{
		.name = "palmas-pwm",
		.id = PALMAS_PWM_ID,
	},
	{
		.name = "palmas-usb",
		.num_resources = ARRAY_SIZE(usb_resource),
		.resources = usb_resource,
		.id = PALMAS_USB_ID,
	},
	{
		.name = "palmas-extcon",
		.num_resources = ARRAY_SIZE(palma_extcon_resource),
		.resources = palma_extcon_resource,
		.id = PALMAS_EXTCON_ID,
	},
	{
		.name = "palmas-thermal",
		.num_resources = ARRAY_SIZE(thermal_resource),
		.resources = thermal_resource,
		.id = PALMAS_THERM_ID,
	},
};

static bool is_volatile_palma_func_reg(struct device *dev, unsigned int reg)
{
	if ((reg >= (PALMAS_SMPS12_CTRL + 0x20)) &&
			(reg <= (PALMAS_SMPS9_VOLTAGE + 0x20)))
		return false;
	return true;
}

static const struct regmap_config palmas_regmap_config[PALMAS_NUM_CLIENTS] = {
	{
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = PALMAS_BASE_TO_REG(PALMAS_PU_PD_OD_BASE,
					PALMAS_PRIMARY_SECONDARY_PAD3),
		.volatile_reg = is_volatile_palma_func_reg,
		.cache_type  = REGCACHE_RBTREE,
	},
	{
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = PALMAS_BASE_TO_REG(PALMAS_GPADC_BASE,
					PALMAS_GPADC_SMPS_VSEL_MONITORING),
	},
	{
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = PALMAS_BASE_TO_REG(PALMAS_TRIM_GPADC_BASE,
					PALMAS_GPADC_TRIM16),
	},
};

#define PALMAS_MAX_INTERRUPT_MASK_REG	4
#define PALMAS_MAX_INTERRUPT_EDGE_REG	8

struct palmas_regs {
	int reg_base;
	int reg_add;
};

struct palmas_irq_regs {
	struct palmas_regs mask_reg[PALMAS_MAX_INTERRUPT_MASK_REG];
	struct palmas_regs status_reg[PALMAS_MAX_INTERRUPT_MASK_REG];
	struct palmas_regs edge_reg[PALMAS_MAX_INTERRUPT_EDGE_REG];
};

#define PALMAS_REGS(base, add)	{ .reg_base = base, .reg_add = add, }
static struct palmas_irq_regs palmas_irq_regs = {
	.mask_reg = {
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT1_MASK),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT2_MASK),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT3_MASK),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT4_MASK),
	},
	.status_reg = {
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT1_STATUS),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT2_STATUS),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT3_STATUS),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT4_STATUS),
	},
	.edge_reg = {
		PALMAS_REGS(PALMAS_INTERRUPT_BASE,
					PALMAS_INT1_EDGE_DETECT1_RESERVED),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE,
					PALMAS_INT1_EDGE_DETECT2_RESERVED),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE,
					PALMAS_INT2_EDGE_DETECT1_RESERVED),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE,
					PALMAS_INT2_EDGE_DETECT2_RESERVED),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE,
					PALMAS_INT3_EDGE_DETECT1_RESERVED),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE,
					PALMAS_INT3_EDGE_DETECT2_RESERVED),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT4_EDGE_DETECT1),
		PALMAS_REGS(PALMAS_INTERRUPT_BASE, PALMAS_INT4_EDGE_DETECT2),
	},
};

struct palmas_irq {
	unsigned int	interrupt_mask;
	unsigned int	rising_mask;
	unsigned int	falling_mask;
	unsigned int	edge_mask;
	unsigned int	mask_reg_index;
	unsigned int	edge_reg_index;
};

#define PALMAS_IRQ(_nr, _imask, _mrindex, _rmask, _fmask, _erindex)	\
[PALMAS_##_nr] = {							\
			.interrupt_mask = PALMAS_##_imask,		\
			.mask_reg_index = _mrindex,			\
			.rising_mask = _rmask,				\
			.falling_mask = _fmask,				\
			.edge_mask = _rmask | _fmask,			\
			.edge_reg_index = _erindex			\
		}

static struct palmas_irq palmas_irqs[] = {
	/* INT1 IRQs */
	PALMAS_IRQ(CHARG_DET_N_VBUS_OVV_IRQ,
			INT1_STATUS_CHARG_DET_N_VBUS_OVV, 0, 0, 0, 0),
	PALMAS_IRQ(PWRON_IRQ, INT1_STATUS_PWRON, 0, 0, 0, 0),
	PALMAS_IRQ(LONG_PRESS_KEY_IRQ, INT1_STATUS_LONG_PRESS_KEY, 0, 0, 0, 0),
	PALMAS_IRQ(RPWRON_IRQ, INT1_STATUS_RPWRON, 0, 0, 0, 0),
	PALMAS_IRQ(PWRDOWN_IRQ, INT1_STATUS_PWRDOWN, 0, 0, 0, 0),
	PALMAS_IRQ(HOTDIE_IRQ, INT1_STATUS_HOTDIE, 0, 0, 0, 0),
	PALMAS_IRQ(VSYS_MON_IRQ, INT1_STATUS_VSYS_MON, 0, 0, 0, 0),
	PALMAS_IRQ(VBAT_MON_IRQ, INT1_STATUS_VBAT_MON, 0, 0, 0, 0),
	/* INT2 IRQs */
	PALMAS_IRQ(RTC_ALARM_IRQ, INT2_STATUS_RTC_ALARM, 1, 0, 0, 0),
	PALMAS_IRQ(RTC_TIMER_IRQ, INT2_STATUS_RTC_TIMER, 1, 0, 0, 0),
	PALMAS_IRQ(WDT_IRQ, INT2_STATUS_WDT, 1, 0, 0, 0),
	PALMAS_IRQ(BATREMOVAL_IRQ, INT2_STATUS_BATREMOVAL, 1, 0, 0, 0),
	PALMAS_IRQ(RESET_IN_IRQ, INT2_STATUS_RESET_IN, 1, 0, 0, 0),
	PALMAS_IRQ(FBI_BB_IRQ, INT2_STATUS_FBI_BB, 1, 0, 0, 0),
	PALMAS_IRQ(SHORT_IRQ, INT2_STATUS_SHORT, 1, 0, 0, 0),
	PALMAS_IRQ(VAC_ACOK_IRQ, INT2_STATUS_VAC_ACOK, 1, 0, 0, 0),
	/* INT3 IRQs */
	PALMAS_IRQ(GPADC_AUTO_0_IRQ, INT3_STATUS_GPADC_AUTO_0, 2, 0, 0, 0),
	PALMAS_IRQ(GPADC_AUTO_1_IRQ, INT3_STATUS_GPADC_AUTO_1, 2, 0, 0, 0),
	PALMAS_IRQ(GPADC_EOC_SW_IRQ, INT3_STATUS_GPADC_EOC_SW, 2, 0, 0, 0),
	PALMAS_IRQ(GPADC_EOC_RT_IRQ, INT3_STATUS_GPADC_EOC_RT, 2, 0, 0, 0),
	PALMAS_IRQ(ID_OTG_IRQ, INT3_STATUS_ID_OTG, 2, 0, 0, 0),
	PALMAS_IRQ(ID_IRQ, INT3_STATUS_ID, 2, 0, 0, 0),
	PALMAS_IRQ(VBUS_OTG_IRQ, INT3_STATUS_VBUS_OTG, 2, 0, 0, 0),
	PALMAS_IRQ(VBUS_IRQ, INT3_STATUS_VBUS, 2, 0, 0, 0),
	/* INT4 IRQs */
	PALMAS_IRQ(GPIO_0_IRQ, INT4_STATUS_GPIO_0, 3,
			PALMAS_INT4_EDGE_DETECT1_GPIO_0_RISING,
			PALMAS_INT4_EDGE_DETECT1_GPIO_0_FALLING, 6),
	PALMAS_IRQ(GPIO_1_IRQ, INT4_STATUS_GPIO_1, 3,
			PALMAS_INT4_EDGE_DETECT1_GPIO_1_RISING,
			PALMAS_INT4_EDGE_DETECT1_GPIO_1_FALLING, 6),
	PALMAS_IRQ(GPIO_2_IRQ, INT4_STATUS_GPIO_2, 3,
			PALMAS_INT4_EDGE_DETECT1_GPIO_2_RISING,
			PALMAS_INT4_EDGE_DETECT1_GPIO_2_FALLING, 6),
	PALMAS_IRQ(GPIO_3_IRQ, INT4_STATUS_GPIO_3, 3,
			PALMAS_INT4_EDGE_DETECT1_GPIO_3_RISING,
			PALMAS_INT4_EDGE_DETECT1_GPIO_3_FALLING, 6),
	PALMAS_IRQ(GPIO_4_IRQ, INT4_STATUS_GPIO_4, 3,
			PALMAS_INT4_EDGE_DETECT2_GPIO_4_RISING,
			PALMAS_INT4_EDGE_DETECT2_GPIO_4_FALLING, 7),
	PALMAS_IRQ(GPIO_5_IRQ, INT4_STATUS_GPIO_5, 3,
			PALMAS_INT4_EDGE_DETECT2_GPIO_5_RISING,
			PALMAS_INT4_EDGE_DETECT2_GPIO_5_FALLING, 7),
	PALMAS_IRQ(GPIO_6_IRQ, INT4_STATUS_GPIO_6, 3,
			PALMAS_INT4_EDGE_DETECT2_GPIO_6_RISING,
			PALMAS_INT4_EDGE_DETECT2_GPIO_6_FALLING, 7),
	PALMAS_IRQ(GPIO_7_IRQ, INT4_STATUS_GPIO_7, 3,
			PALMAS_INT4_EDGE_DETECT2_GPIO_7_RISING,
			PALMAS_INT4_EDGE_DETECT2_GPIO_7_FALLING, 7),
};

struct palmas_irq_chip_data {
	struct palmas		*palmas;
	int			irq_base;
	int			irq;
	struct mutex		irq_lock;
	struct irq_chip		irq_chip;
	struct irq_domain	*domain;

	struct palmas_irq_regs	*irq_regs;
	struct palmas_irq	*irqs;
	int			num_irqs;
	unsigned int		mask_value[PALMAS_MAX_INTERRUPT_MASK_REG];
	unsigned int		status_value[PALMAS_MAX_INTERRUPT_MASK_REG];
	unsigned int		edge_value[PALMAS_MAX_INTERRUPT_EDGE_REG];
	unsigned int		mask_def_value[PALMAS_MAX_INTERRUPT_MASK_REG];
	unsigned int		edge_def_value[PALMAS_MAX_INTERRUPT_EDGE_REG];
	int			num_mask_regs;
	int			num_edge_regs;
	int			wake_count;
};

static inline const struct palmas_irq *irq_to_palmas_irq(
	struct palmas_irq_chip_data *data, int irq)
{
	return &data->irqs[irq];
}

static void palmas_irq_lock(struct irq_data *data)
{
	struct palmas_irq_chip_data *d = irq_data_get_irq_chip_data(data);

	mutex_lock(&d->irq_lock);
}

static void palmas_irq_sync_unlock(struct irq_data *data)
{
	struct palmas_irq_chip_data *d = irq_data_get_irq_chip_data(data);
	int i, ret;

	for (i = 0; i < d->num_mask_regs; i++) {
		ret = palmas_update_bits(d->palmas,
				d->irq_regs->mask_reg[i].reg_base,
				d->irq_regs->mask_reg[i].reg_add,
				d->mask_def_value[i], d->mask_value[i]);
		if (ret < 0)
			dev_err(d->palmas->dev, "Failed to sync masks in %x\n",
					d->irq_regs->mask_reg[i].reg_add);
	}

	for (i = 0; i < d->num_edge_regs; i++) {
		if (!d->edge_def_value[i])
			continue;

		ret = palmas_update_bits(d->palmas,
				d->irq_regs->edge_reg[i].reg_base,
				d->irq_regs->edge_reg[i].reg_add,
				d->edge_def_value[i], d->edge_value[i]);
		if (ret < 0)
			dev_err(d->palmas->dev, "Failed to sync edge in %x\n",
					d->irq_regs->edge_reg[i].reg_add);
	}

	/* If we've changed our wakeup count propagate it to the parent */
	if (d->wake_count < 0)
		for (i = d->wake_count; i < 0; i++)
			irq_set_irq_wake(d->irq, 0);
	else if (d->wake_count > 0)
		for (i = 0; i < d->wake_count; i++)
			irq_set_irq_wake(d->irq, 1);

	d->wake_count = 0;

	mutex_unlock(&d->irq_lock);
}

static void palmas_irq_enable(struct irq_data *data)
{
	struct palmas_irq_chip_data *d = irq_data_get_irq_chip_data(data);
	const struct palmas_irq *irq_data = irq_to_palmas_irq(d, data->hwirq);

	d->mask_value[irq_data->mask_reg_index] &= ~irq_data->interrupt_mask;
}

static void palmas_irq_disable(struct irq_data *data)
{
	struct palmas_irq_chip_data *d = irq_data_get_irq_chip_data(data);
	const struct palmas_irq *irq_data = irq_to_palmas_irq(d, data->hwirq);

	d->mask_value[irq_data->mask_reg_index] |= irq_data->interrupt_mask;
}

static int palmas_irq_set_type(struct irq_data *data, unsigned int type)
{
	struct palmas_irq_chip_data *d = irq_data_get_irq_chip_data(data);
	const struct palmas_irq *irq_data = irq_to_palmas_irq(d, data->hwirq);
	unsigned int reg = irq_data->edge_reg_index;

	if (!irq_data->edge_mask)
		return 0;

	d->edge_value[reg] &= ~irq_data->edge_mask;
	switch (type) {
	case IRQ_TYPE_EDGE_FALLING:
		d->edge_value[reg] |= irq_data->falling_mask;
		break;

	case IRQ_TYPE_EDGE_RISING:
		d->edge_value[reg] |= irq_data->rising_mask;
		break;

	case IRQ_TYPE_EDGE_BOTH:
		d->edge_value[reg] |= irq_data->edge_mask;
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static int palmas_irq_set_wake(struct irq_data *data, unsigned int on)
{
	struct palmas_irq_chip_data *d = irq_data_get_irq_chip_data(data);

	if (on)
		d->wake_count++;
	else
		d->wake_count--;

	return 0;
}

static const struct irq_chip palmas_irq_chip = {
	.irq_bus_lock		= palmas_irq_lock,
	.irq_bus_sync_unlock	= palmas_irq_sync_unlock,
	.irq_disable		= palmas_irq_disable,
	.irq_enable		= palmas_irq_enable,
	.irq_set_type		= palmas_irq_set_type,
	.irq_set_wake		= palmas_irq_set_wake,
};

static irqreturn_t palmas_irq_thread(int irq, void *data)
{
	struct palmas_irq_chip_data *d = data;
	int ret, i;
	bool handled = false;

	for (i = 0; i < d->num_mask_regs; i++) {
		ret = palmas_read(d->palmas,
				d->irq_regs->status_reg[i].reg_base,
				d->irq_regs->status_reg[i].reg_add,
				&d->status_value[i]);

		if (ret != 0) {
			dev_err(d->palmas->dev,
				"Failed to read IRQ status: %d\n", ret);
			return IRQ_NONE;
		}
		d->status_value[i] &= ~d->mask_value[i];
	}

	for (i = 0; i < d->num_irqs; i++) {
		if (d->status_value[d->irqs[i].mask_reg_index] &
				d->irqs[i].interrupt_mask) {
			handle_nested_irq(irq_find_mapping(d->domain, i));
			handled = true;
		}
	}

	if (handled)
		return IRQ_HANDLED;
	else
		return IRQ_NONE;
}

static int palmas_irq_map(struct irq_domain *h, unsigned int virq,
			  irq_hw_number_t hw)
{
	struct palmas_irq_chip_data *data = h->host_data;

	irq_set_chip_data(virq, data);
	irq_set_chip(virq, &data->irq_chip);
	irq_set_nested_thread(virq, 1);

	/* ARM needs us to explicitly flag the IRQ as valid
	 * and will set them noprobe when we do so. */
#ifdef CONFIG_ARM
	set_irq_flags(virq, IRQF_VALID);
#else
	irq_set_noprobe(virq);
#endif

	return 0;
}

static struct irq_domain_ops palmas_domain_ops = {
	.map	= palmas_irq_map,
	.xlate	= irq_domain_xlate_twocell,
};

static int palmas_add_irq_chip(struct palmas *palmas, int irq, int irq_flags,
			int irq_base, struct palmas_irq_chip_data **data)
{
	struct palmas_irq_chip_data *d;
	int i;
	int ret;
	unsigned int status_value;
	int num_irqs = ARRAY_SIZE(palmas_irqs);

	if (irq_base) {
		irq_base = irq_alloc_descs(irq_base, 0, num_irqs, 0);
		if (irq_base < 0) {
			dev_warn(palmas->dev, "Failed to allocate IRQs: %d\n",
				 irq_base);
			return irq_base;
		}
	}

	d = devm_kzalloc(palmas->dev, sizeof(*d), GFP_KERNEL);
	if (!d) {
		dev_err(palmas->dev, "mem alloc for d failed\n");
		return -ENOMEM;
	}

	d->palmas = palmas;
	d->irq = irq;
	d->irq_base = irq_base;
	mutex_init(&d->irq_lock);
	d->irq_chip = palmas_irq_chip;
	d->irq_chip.name = dev_name(palmas->dev);
	d->irq_regs = &palmas_irq_regs;

	d->irqs = palmas_irqs;
	d->num_irqs = num_irqs;
	d->num_mask_regs = 4;
	d->num_edge_regs = 8;
	d->wake_count = 0;
	*data = d;

	for (i = 0; i < d->num_irqs; i++) {
		d->mask_def_value[d->irqs[i].mask_reg_index] |=
						d->irqs[i].interrupt_mask;
		d->edge_def_value[d->irqs[i].edge_reg_index] |=
						d->irqs[i].edge_mask;
	}

	/* Mask all interrupts */
	for (i = 0; i < d->num_mask_regs; i++) {
		d->mask_value[i] = d->mask_def_value[i];
		ret = palmas_update_bits(d->palmas,
				d->irq_regs->mask_reg[i].reg_base,
				d->irq_regs->mask_reg[i].reg_add,
				d->mask_def_value[i], d->mask_value[i]);
		if (ret < 0)
			dev_err(d->palmas->dev, "Failed to update masks in %x\n",
					d->irq_regs->mask_reg[i].reg_add);
	}

	/* Set edge registers */
	for (i = 0; i < d->num_edge_regs; i++) {
		if (!d->edge_def_value[i])
			continue;

		ret = palmas_update_bits(d->palmas,
				d->irq_regs->edge_reg[i].reg_base,
				d->irq_regs->edge_reg[i].reg_add,
				d->edge_def_value[i], 0);
		if (ret < 0)
			dev_err(palmas->dev, "Failed to sync edge in %x\n",
					d->irq_regs->edge_reg[i].reg_add);
	}

	/* Clear all interrupts */
	for (i = 0; i < d->num_mask_regs; i++) {
		ret = palmas_read(d->palmas,
				d->irq_regs->status_reg[i].reg_base,
				d->irq_regs->status_reg[i].reg_add,
				&status_value);

		if (ret != 0) {
			dev_err(palmas->dev, "Failed to read status %x\n",
				d->irq_regs->status_reg[i].reg_add);
		}
	}

	if (irq_base)
		d->domain = irq_domain_add_legacy(palmas->dev->of_node,
						  num_irqs, irq_base, 0,
						  &palmas_domain_ops, d);
	else
		d->domain = irq_domain_add_linear(palmas->dev->of_node,
						  num_irqs,
						  &palmas_domain_ops, d);
	if (!d->domain) {
		dev_err(palmas->dev, "Failed to create IRQ domain\n");
		return -ENOMEM;
	}

	ret = request_threaded_irq(irq, NULL, palmas_irq_thread, irq_flags,
				   dev_name(palmas->dev), d);
	if (ret != 0) {
		dev_err(palmas->dev,
			"Failed to request IRQ %d: %d\n", irq, ret);
		return ret;
	}

	return 0;
}

static void palmas_del_irq_chip(int irq, struct palmas_irq_chip_data *d)
{
	if (d)
		free_irq(irq, d);
}

int palmas_irq_get_virq(struct palmas *palmas, int irq)
{
	struct palmas_irq_chip_data *data = palmas->irq_chip_data;

	if (!data->irqs[irq].interrupt_mask)
		return -EINVAL;

	return irq_create_mapping(data->domain, irq);
}
EXPORT_SYMBOL_GPL(palmas_irq_get_virq);

struct palmas_sleep_requestor_info {
	int id;
	int reg_offset;
	int bit_pos;
};

#define SLEEP_REQUESTOR(_id, _offset, _pos)		\
	[PALMAS_SLEEP_REQSTR_ID_##_id] = {		\
		.id = PALMAS_SLEEP_REQSTR_ID_##_id,	\
		.reg_offset = _offset,			\
		.bit_pos = _pos,			\
	}

static struct palmas_sleep_requestor_info sleep_reqt_info[] = {
	SLEEP_REQUESTOR(REGEN1, 0, 0),
	SLEEP_REQUESTOR(REGEN2, 0, 1),
	SLEEP_REQUESTOR(SYSEN1, 0, 2),
	SLEEP_REQUESTOR(SYSEN2, 0, 3),
	SLEEP_REQUESTOR(CLK32KG, 0, 4),
	SLEEP_REQUESTOR(CLK32KGAUDIO, 0, 5),
	SLEEP_REQUESTOR(REGEN3, 0, 6),
	SLEEP_REQUESTOR(SMPS12, 1, 0),
	SLEEP_REQUESTOR(SMPS3, 1, 1),
	SLEEP_REQUESTOR(SMPS45, 1, 2),
	SLEEP_REQUESTOR(SMPS6, 1, 3),
	SLEEP_REQUESTOR(SMPS7, 1, 4),
	SLEEP_REQUESTOR(SMPS8, 1, 5),
	SLEEP_REQUESTOR(SMPS9, 1, 6),
	SLEEP_REQUESTOR(SMPS10, 1, 7),
	SLEEP_REQUESTOR(LDO1, 2, 0),
	SLEEP_REQUESTOR(LDO2, 2, 1),
	SLEEP_REQUESTOR(LDO3, 2, 2),
	SLEEP_REQUESTOR(LDO4, 2, 3),
	SLEEP_REQUESTOR(LDO5, 2, 4),
	SLEEP_REQUESTOR(LDO6, 2, 5),
	SLEEP_REQUESTOR(LDO7, 2, 6),
	SLEEP_REQUESTOR(LDO8, 2, 7),
	SLEEP_REQUESTOR(LDO9, 3, 0),
	SLEEP_REQUESTOR(LDOLN, 3, 1),
	SLEEP_REQUESTOR(LDOUSB, 3, 2),
};

struct palmas_clk32k_info {
	unsigned int control_reg;
	unsigned int sleep_reqstr_id;
};

static struct palmas_clk32k_info palmas_clk32k_info[] = {
	{
		.control_reg = PALMAS_CLK32KG_CTRL,
		.sleep_reqstr_id = PALMAS_SLEEP_REQSTR_ID_CLK32KG,
	}, {
		.control_reg = PALMAS_CLK32KGAUDIO_CTRL,
		.sleep_reqstr_id = PALMAS_SLEEP_REQSTR_ID_CLK32KGAUDIO,
	},
};

static int palmas_resource_write(struct palmas *palmas, unsigned int reg,
	unsigned int value)
{
	unsigned int addr = PALMAS_BASE_TO_REG(PALMAS_RESOURCE_BASE, reg);

	return regmap_write(palmas->regmap[0], addr, value);
}

static int palmas_resource_update(struct palmas *palmas, unsigned int reg,
	unsigned int mask, unsigned int value)
{
	unsigned int addr = PALMAS_BASE_TO_REG(PALMAS_RESOURCE_BASE, reg);

	return regmap_update_bits(palmas->regmap[0], addr, mask, value);
}

static int palmas_control_update(struct palmas *palmas, unsigned int reg,
	unsigned int mask, unsigned int value)
{
	unsigned int addr = PALMAS_BASE_TO_REG(PALMAS_PMU_CONTROL_BASE, reg);

	return regmap_update_bits(palmas->regmap[0], addr, mask, value);
}

int palmas_ext_power_req_config(struct palmas *palmas,
		int id, int ext_pwr_ctrl, bool enable)
{
	int preq_mask_bit = 0;
	int ret;
	int base_reg = 0;
	int bit_pos;

	if (!(ext_pwr_ctrl & EXT_PWR_REQ))
		return 0;

	if (id >= PALMAS_SLEEP_REQSTR_ID_MAX)
		return 0;

	if (ext_pwr_ctrl & PALMAS_EXT_CONTROL_NSLEEP) {
		base_reg = PALMAS_NSLEEP_RES_ASSIGN;
		preq_mask_bit = 0;
	} else if (ext_pwr_ctrl & PALMAS_EXT_CONTROL_ENABLE1) {
		base_reg = PALMAS_ENABLE1_RES_ASSIGN;
		preq_mask_bit = 1;
	} else if (ext_pwr_ctrl & PALMAS_EXT_CONTROL_ENABLE2) {
		base_reg = PALMAS_ENABLE2_RES_ASSIGN;
		preq_mask_bit = 2;
	}

	bit_pos = sleep_reqt_info[id].bit_pos;
	base_reg += sleep_reqt_info[id].reg_offset;
	if (enable)
		ret = palmas_resource_update(palmas, base_reg,
				BIT(bit_pos), BIT(bit_pos));
	else
		ret = palmas_resource_update(palmas, base_reg,
				BIT(bit_pos), 0);
	if (ret < 0) {
		dev_err(palmas->dev, "Update on resource reg failed\n");
		return ret;
	}

	/* Unmask the PREQ */
	ret = palmas_control_update(palmas, PALMAS_POWER_CTRL,
				BIT(preq_mask_bit), 0);
	if (ret < 0) {
		dev_err(palmas->dev, "Power control register update fails\n");
		return ret;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(palmas_ext_power_req_config);

static void palmas_init_ext_control(struct palmas *palmas)
{
	int ret;
	int i;

	/* Clear all external control for this rail */
	for (i = 0; i < 12; ++i) {
		ret = palmas_resource_write(palmas,
				PALMAS_NSLEEP_RES_ASSIGN + i, 0);
		if (ret < 0)
			dev_err(palmas->dev,
				"Error in clearing res assign register\n");
	}

	/* Mask the PREQ */
	ret = palmas_control_update(palmas, PALMAS_POWER_CTRL, 0x7, 0x7);
	if (ret < 0)
		dev_err(palmas->dev, "Power control reg write failed\n");
}

static void palmas_clk32k_init(struct palmas *palmas,
	struct palmas_platform_data *pdata)
{
	int ret;
	struct palmas_clk32k_init_data *clk32_idata = pdata->clk32k_init_data;
	int data_size = pdata->clk32k_init_data_size;
	unsigned int reg;
	int i;
	int id;

	if (!clk32_idata || !data_size)
		return;

	for (i = 0; i < data_size; ++i) {
		struct palmas_clk32k_init_data *clk32_pd =  &clk32_idata[i];

		reg = palmas_clk32k_info[clk32_pd->clk32k_id].control_reg;
		if (clk32_pd->enable)
			ret = palmas_resource_update(palmas, reg,
					PALMAS_CLK32KG_CTRL_MODE_ACTIVE,
					PALMAS_CLK32KG_CTRL_MODE_ACTIVE);
		else
			ret = palmas_resource_update(palmas, reg,
					PALMAS_CLK32KG_CTRL_MODE_ACTIVE, 0);
		if (ret < 0) {
			dev_err(palmas->dev, "Error in updating clk reg\n");
			return;
		}

		/* Sleep control */
		id = palmas_clk32k_info[clk32_pd->clk32k_id].sleep_reqstr_id;
		if (clk32_pd->sleep_control) {
			ret = palmas_ext_power_req_config(palmas, id,
					clk32_pd->sleep_control, true);
			if (ret < 0) {
				dev_err(palmas->dev,
					"Error in ext power control reg\n");
				return;
			}

			ret = palmas_resource_update(palmas, reg,
					PALMAS_CLK32KG_CTRL_MODE_SLEEP,
					PALMAS_CLK32KG_CTRL_MODE_SLEEP);
			if (ret < 0) {
				dev_err(palmas->dev,
					"Error in updating clk reg\n");
				return;
			}
		} else {

			ret = palmas_resource_update(palmas, reg,
					PALMAS_CLK32KG_CTRL_MODE_SLEEP, 0);
			if (ret < 0) {
				dev_err(palmas->dev,
					"Error in updating clk reg\n");
				return;
			}
		}
	}
}

static struct palmas *palmas_dev;
static void palmas_power_off(void)
{
	int value = 0;

	if (!palmas_dev)
		return;

	if(machine_is_mozart())
	{
		palmas_update_bits(palmas_dev, PALMAS_INTERRUPT_BASE, PALMAS_INT3_MASK, PALMAS_INT3_STATUS_VBUS, PALMAS_INT3_STATUS_VBUS);
		palmas_read(palmas_dev, PALMAS_INTERRUPT_BASE, PALMAS_INT3_MASK, &value);
		printk("%s : set VBUS interrupt to 1 : 0x21B = 0X%02x\n", __func__, value);
	}
	palmas_control_update(palmas_dev, PALMAS_DEV_CTRL, 1, 0);
}

void palmas_reset(void)
{
	int value = 0;

	if (!palmas_dev)
		return;

	if(machine_is_mozart())
	{
		palmas_update_bits(palmas_dev, PALMAS_INTERRUPT_BASE, PALMAS_INT3_MASK, PALMAS_INT3_STATUS_VBUS, PALMAS_INT3_STATUS_VBUS);
		palmas_read(palmas_dev, PALMAS_INTERRUPT_BASE, PALMAS_INT3_MASK, &value);
		printk("%s : set VBUS interrupt to 1 : 0x21B = 0X%02x\n", __func__, value);
	}
	palmas_control_update(palmas_dev, PALMAS_DEV_CTRL, 2, 2);
}
EXPORT_SYMBOL(palmas_reset);

static int palmas_read_version_information(struct palmas *palmas)
{
	unsigned int sw_rev, des_rev;
	int ret;

	ret = palmas_read(palmas, PALMAS_PMU_CONTROL_BASE,
				PALMAS_SW_REVISION, &sw_rev);
	if (ret < 0) {
		dev_err(palmas->dev, "SW_REVISION read failed: %d\n", ret);
		return ret;
	}

	ret = palmas_read(palmas, PALMAS_PAGE3_BASE,
				PALMAS_INTERNAL_DESIGNREV, &des_rev);
	if (ret < 0) {
		dev_err(palmas->dev,
			"INTERNAL_DESIGNREV read failed: %d\n", ret);
		return ret;
	}

	palmas->sw_otp_version = sw_rev;

	dev_info(palmas->dev, "Internal DesignRev 0x%02X, SWRev 0x%02X\n",
			des_rev, sw_rev);
	des_rev = PALMAS_INTERNAL_DESIGNREV_DESIGNREV(des_rev);
	switch (des_rev) {
	case 0:
		palmas->es_major_version = 1;
		palmas->es_minor_version = 0;
		palmas->design_revision = 0xA0;
		break;
	case 1:
		palmas->es_major_version = 2;
		palmas->es_minor_version = 0;
		palmas->design_revision = 0xB0;
		break;
	case 2:
		palmas->es_major_version = 2;
		palmas->es_minor_version = 1;
		palmas->design_revision = 0xB1;
		break;
	case 3:
		palmas->es_major_version = 2;
		palmas->es_minor_version = 2;
		palmas->design_revision = 0xB2;
		break;
	default:
		dev_err(palmas->dev, "Invalid design revision\n");
		return -EINVAL;
	}

	dev_info(palmas->dev, "ES version %d.%d: ChipRevision 0x%02X%02X\n",
		palmas->es_major_version, palmas->es_minor_version,
		palmas->design_revision, palmas->sw_otp_version);
	return 0;
}

//=================stree test=================
static ssize_t show_tps65913_i2c_status(struct device *dev, struct device_attribute *devattr, char *buf)
{
	if(tps65913_palmas)
	{
		return sprintf(buf, "%d\n", tps65913_palmas->i2c_status);
	}
	else
	{
		return sprintf(buf, "%d\n", 0);
	}
}
static DEVICE_ATTR(tps65913_i2c_status, S_IWUSR | S_IRUGO,show_tps65913_i2c_status,NULL);

static struct attribute *tps65913_i2c_attributes[] = {

	&dev_attr_tps65913_i2c_status.attr,
	NULL,
};

static const struct attribute_group tps65913_i2c_group = {
	.attrs = tps65913_i2c_attributes,
};

void tps65913_read_stress_test(struct work_struct *work)
{
	int ret = 0;

	ret = palmas_read_version_information(tps65913_palmas);

	if (ret < 0)
	{
		printk("failed ps65913_read_stress_test \n");
	}

	queue_delayed_work(tps65913_strees_work_queue, &tps65913_palmas->stress_test, 2*HZ);
	return ;
}
long  tps65913_ioctl(struct file *filp,  unsigned int cmd, unsigned long arg)
{
	if (_IOC_TYPE(cmd) ==TPS65913_IOC_MAGIC)
	{
	     printk("  tps65913_ioctl vaild magic \n");
	}
	else
	{
		printk("  tps65913_ioctl invaild magic \n");
		return -ENOTTY;
	}

	switch(cmd)
	{
		case TPS65913_POLLING_DATA :
		if ((arg==START_NORMAL)||(arg==START_HEAVY))
		{
				 printk(" tps65913 stress test start (%s)\n",(arg==START_NORMAL)?"normal":"heavy");
				 queue_delayed_work(tps65913_strees_work_queue, &tps65913_palmas->stress_test, 2*HZ);
		}
		else
		{
				 printk(" t tps65913 tress test end\n");
				 cancel_delayed_work_sync(&tps65913_palmas->stress_test);
		}
		break;

		default:  /* redundant, as cmd was checked against MAXNR */
			printk("  TPS65913: unknow i2c  stress test  command cmd=%x arg=%lu\n",cmd,arg);
			return -ENOTTY;
	}

	return 0;
}

int tps65913_open(struct inode *inode, struct file *filp)
{
	return 0;
}

struct file_operations tps65913_fops = {
	.owner =    THIS_MODULE,
	.unlocked_ioctl =   tps65913_ioctl,
	.open =  tps65913_open,
};
//=================stree test end=================

static int __devinit palmas_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct palmas *palmas;
	struct palmas_platform_data *pdata;
	int ret = 0, i;
	unsigned int reg, addr;
	int slave, value = 0;
	int irq_flag;
	struct mfd_cell *children;
	//=================stree test ===================
	int rc;
	//=================stree test end=================

	pdata = dev_get_platdata(&i2c->dev);
	if (!pdata)
		return -EINVAL;

	palmas = devm_kzalloc(&i2c->dev, sizeof(struct palmas), GFP_KERNEL);
	if (palmas == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, palmas);
	palmas->dev = &i2c->dev;
	palmas->id = id->driver_data;
	palmas->irq = i2c->irq;

	for (i = 0; i < PALMAS_NUM_CLIENTS; i++) {
		if (i == 0)
			palmas->i2c_clients[i] = i2c;
		else {
			palmas->i2c_clients[i] =
					i2c_new_dummy(i2c->adapter,
							i2c->addr + i);
			if (!palmas->i2c_clients[i]) {
				dev_err(palmas->dev,
					"can't attach client %d\n", i);
				ret = -ENOMEM;
				goto err;
			}
		}
		palmas->regmap[i] = devm_regmap_init_i2c(palmas->i2c_clients[i],
				&palmas_regmap_config[i]);
		if (IS_ERR(palmas->regmap[i])) {
			ret = PTR_ERR(palmas->regmap[i]);
			dev_err(palmas->dev,
				"Failed to allocate regmap %d, err: %d\n",
				i, ret);
			goto err;
		}
	}

	ret = palmas_read_version_information(palmas);
	if (ret < 0)
		goto err;

	/* Change interrupt line output polarity */
	slave = PALMAS_BASE_TO_SLAVE(PALMAS_PU_PD_OD_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_PU_PD_OD_BASE, PALMAS_POLARITY_CTRL);
	regmap_read(palmas->regmap[slave], addr, &reg);
	if (pdata->irq_type & IRQ_TYPE_LEVEL_HIGH)
		reg |= PALMAS_POLARITY_CTRL_INT_POLARITY;
	else
		reg &= ~PALMAS_POLARITY_CTRL_INT_POLARITY;
	regmap_write(palmas->regmap[slave], addr, reg);

	/* Change IRQ into clear on read mode for efficiency */
	slave = PALMAS_BASE_TO_SLAVE(PALMAS_INTERRUPT_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE, PALMAS_INT_CTRL);
	reg = PALMAS_INT_CTRL_INT_CLEAR;

	regmap_write(palmas->regmap[slave], addr, reg);

	irq_flag = pdata->irq_type;
	irq_flag |= IRQF_ONESHOT;
	ret = palmas_add_irq_chip(palmas, palmas->irq,
			irq_flag, pdata->irq_base, &palmas->irq_chip_data);
	if (ret < 0)
		goto err;

	reg = pdata->power_ctrl;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_PMU_CONTROL_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_PMU_CONTROL_BASE, PALMAS_POWER_CTRL);

	ret = regmap_write(palmas->regmap[slave], addr, reg);
	if (ret)
		goto err;

	/*
	 * Programming the Long-Press shutdown delay register.
	 * Using "slave" from previous assignment as this register
	 * too belongs to PALMAS_PMU_CONTROL_BASE block.
	 */
	if (pdata->long_press_delay != PALMAS_LONG_PRESS_KEY_TIME_DEFAULT) {
		ret = palmas_update_bits(palmas, PALMAS_PMU_CONTROL_BASE,
					PALMAS_LONG_PRESS_KEY,
					PALMAS_LONG_PRESS_KEY_LPK_TIME_MASK,
					pdata->long_press_delay <<
					PALMAS_LONG_PRESS_KEY_LPK_TIME_SHIFT);
		if (ret) {
			dev_err(palmas->dev,
				"Failed to update palmas long press delay"
				"(hard shutdown delay), err: %d\n", ret);
			goto err;
		}
	}

	/* Programming the system off type by Long press key */
	if (pdata->poweron_lpk != PALMAS_SWOFF_COLDRST_PWRON_LPK_DEFAULT) {
		ret = palmas_update_bits(palmas, PALMAS_PMU_CONTROL_BASE,
					PALMAS_SWOFF_COLDRST,
					PALMAS_SWOFF_COLDRST_PWRON_LPK,
					pdata->poweron_lpk <<
					PALMAS_SWOFF_COLDRST_PWRON_LPK_SHIFT);
		if (ret) {
			dev_err(palmas->dev,
			"Failed to update poweron_lpk err: %d\n", ret);
			goto err;
		}
	}
	palmas_init_ext_control(palmas);

	palmas_clk32k_init(palmas, pdata);

	children = kmemdup(palmas_children, sizeof(palmas_children),
			   GFP_KERNEL);
	if (!children) {
		ret = -ENOMEM;
		goto err;
	}

	children[PALMAS_PMIC_ID].platform_data = pdata->pmic_pdata;
	children[PALMAS_PMIC_ID].pdata_size = sizeof(*pdata->pmic_pdata);
	children[PALMAS_GPADC_ID].platform_data = pdata->adc_pdata;
	children[PALMAS_GPADC_ID].pdata_size = sizeof(*pdata->adc_pdata);

	ret = mfd_add_devices(palmas->dev, -1,
			      children, ARRAY_SIZE(palmas_children),
			      NULL, palmas->irq_chip_data->irq_base);
	kfree(children);

	if (ret < 0)
		goto err;

	if (pdata->use_power_off && !pm_power_off)
		pm_power_off = palmas_power_off;

	if (pdata->auto_ldousb_en)
		/* VBUS detection enables the LDOUSB */
		palmas_control_update(palmas, PALMAS_EXT_CHRG_CTRL, 1,
					PALMAS_EXT_CHRG_CTRL_AUTO_LDOUSB_EN);

	palmas_dev = palmas;

	//=================stree test=================
	tps65913_palmas = palmas;
       tps65913_palmas->i2c_status = 1;
	if (sysfs_create_group(&i2c->dev.kobj, &tps65913_i2c_group))
	{
		dev_err(&i2c->dev, "tps65913_i2c_probe:Not able to create the sysfs\n");
	}
       INIT_DELAYED_WORK(&tps65913_palmas->stress_test,  tps65913_read_stress_test) ;
       tps65913_strees_work_queue = create_singlethread_workqueue("tps65913_strees_test_workqueue");

	tps65913_palmas->tps65913_misc.minor	= MISC_DYNAMIC_MINOR;
	tps65913_palmas->tps65913_misc.name	= "tps65913";
	tps65913_palmas->tps65913_misc.fops  	= &tps65913_fops;
       rc = misc_register(&tps65913_palmas->tps65913_misc);
	 printk(KERN_INFO "tps65913 register misc device for I2C stress test rc=%x\n", rc);
	//=================stree test end=================

	if(machine_is_mozart())
	{
		palmas_update_bits(palmas_dev, PALMAS_INTERRUPT_BASE, PALMAS_INT3_MASK, PALMAS_INT3_STATUS_VBUS, 0);
		palmas_read(palmas_dev, PALMAS_INTERRUPT_BASE, PALMAS_INT3_MASK, &value);
		printk("%s : set VBUS interrupt to 0 : 0x21B = 0X%02x\n", __func__, value);
	}

	return ret;

err:
	mfd_remove_devices(palmas->dev);
	kfree(palmas);
	return ret;
}

static int palmas_i2c_remove(struct i2c_client *i2c)
{
	struct palmas *palmas = i2c_get_clientdata(i2c);

	mfd_remove_devices(palmas->dev);
	palmas_del_irq_chip(palmas->irq, palmas->irq_chip_data);

	return 0;
}

static const struct i2c_device_id palmas_i2c_id[] = {
	{ "palmas", },
	{ "twl6035", },
	{ "twl6037", },
	{ "tps65913", },
	{ /* end */ }
};
MODULE_DEVICE_TABLE(i2c, palmas_i2c_id);

static struct of_device_id __devinitdata of_palmas_match_tbl[] = {
	{ .compatible = "ti,palmas", },
	{ /* end */ }
};

static struct i2c_driver palmas_i2c_driver = {
	.driver = {
		   .name = "palmas",
		   .of_match_table = of_palmas_match_tbl,
		   .owner = THIS_MODULE,
	},
	.probe = palmas_i2c_probe,
	.remove = palmas_i2c_remove,
	.id_table = palmas_i2c_id,
};

static int __init palmas_i2c_init(void)
{
	return i2c_add_driver(&palmas_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(palmas_i2c_init);

static void __exit palmas_i2c_exit(void)
{
	i2c_del_driver(&palmas_i2c_driver);
}
module_exit(palmas_i2c_exit);

MODULE_AUTHOR("Graeme Gregory <gg@slimlogic.co.uk>");
MODULE_DESCRIPTION("Palmas chip family multi-function driver");
MODULE_LICENSE("GPL");
