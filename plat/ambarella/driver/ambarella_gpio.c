/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */
#include <assert.h>
#include <errno.h>
#include <lib/mmio.h>
#include <lib/utils_def.h>
#include <lib/spinlock.h>
#include <drivers/gpio.h>
#include <drivers/delay_timer.h>
#include <boot_cookie.h>
#include <plat_private.h>
#include <ambarella_def.h>

static spinlock_t gpio_lock[GPIO_BANK];
static spinlock_t pinctrl_lock;

struct ambarella_gpio_security_t {
	u_register_t pinctrl_reg[2];
	uint32_t gpio_mask[GPIO_BANK];

	/* Number of secure GPIO bank */
	uint32_t nr_bank_req;
	struct {
		/* The index of this GPIO bank */
		uint32_t index;
		uintptr_t gpio_reg[2];
	} gpio[GPIO_BANK];
} gpio_security;


#define PINID_TO_BANK(p)	((p) >> 5)
#define PINID_TO_OFFSET(p)	((p) & 0x1f)

static uintptr_t gpio_base[] = {
	GPIO0_BASE, GPIO1_BASE, GPIO2_BASE, GPIO3_BASE,
	GPIO4_BASE, GPIO5_BASE, GPIO6_BASE,
};

static inline uint32_t gpio_base_to_bank(uintptr_t base)
{
	uint32_t bank;

	for (bank = 0; bank < ARRAY_SIZE(gpio_base); bank++) {
		if ((gpio_base[bank] & 0xffffffff) == base)
			break;
	}

	assert(bank < ARRAY_SIZE(gpio_base));

	return bank;
}

static inline uintptr_t gpio_bank_to_base(uint32_t bank)
{
	assert(bank < ARRAY_SIZE(gpio_base));
	return gpio_base[bank];
}

static int ambarella_get_direction(int gpio)
{
	uint32_t bank, offset;
	uintptr_t base;
	int rval;

	bank = PINID_TO_BANK(gpio);
	offset = PINID_TO_OFFSET(gpio);
	base = gpio_bank_to_base(bank);

	spin_lock(&gpio_lock[bank]);
	rval = !(mmio_read_32(base + GPIO_DIR_OFFSET) & BIT(offset));
	spin_unlock(&gpio_lock[bank]);

	return rval;
}

static void ambarella_set_direction(int gpio, int direction)
{
	uint32_t bank, offset, i;
	uintptr_t base;

	bank = PINID_TO_BANK(gpio);
	offset = PINID_TO_OFFSET(gpio);
	base = gpio_bank_to_base(bank);

	spin_lock(&gpio_lock[bank]);
	if (direction == GPIO_DIR_IN)
		mmio_clrbits_32(base + GPIO_DIR_OFFSET, BIT(offset));
	else
		mmio_setbits_32(base + GPIO_DIR_OFFSET, BIT(offset));

	mmio_clrbits_32(base + GPIO_AFSEL_OFFSET, BIT(offset));
	mmio_setbits_32(base + GPIO_ENABLE_OFFSET, 0xFFFFFFFF);

	spin_unlock(&gpio_lock[bank]);

	spin_lock(&pinctrl_lock);
	/* Configure iomux to make sure the pin in GPIO mode */
	for (i = 0; i < 3; i++)
		mmio_clrbits_32(IOMUX_BASE + IOMUX_REG_OFFSET(bank, i), BIT(offset));

	mmio_write_32(IOMUX_BASE + IOMUX_CTRL_SET_OFFSET, 0x1);
	mmio_write_32(IOMUX_BASE + IOMUX_CTRL_SET_OFFSET, 0x0);

	spin_unlock(&pinctrl_lock);
}

static int ambarella_get_value(int gpio)
{
	uint32_t bank, offset;
	uintptr_t base;
	int rval;

	bank = PINID_TO_BANK(gpio);
	offset = PINID_TO_OFFSET(gpio);
	base = gpio_bank_to_base(bank);

	spin_lock(&gpio_lock[bank]);
	rval = !!(mmio_read_32(base + GPIO_DATA_OFFSET) & BIT(offset));
	spin_unlock(&gpio_lock[bank]);

	return rval;
}

static void ambarella_set_value(int gpio, int value)
{
	uint32_t bank, offset;
	uintptr_t base;

	bank = PINID_TO_BANK(gpio);
	offset = PINID_TO_OFFSET(gpio);
	base = gpio_bank_to_base(bank);

	spin_lock(&gpio_lock[bank]);
	mmio_setbits_32(base + GPIO_MASK_OFFSET, BIT(offset));

	if (value == GPIO_LEVEL_LOW)
		mmio_clrbits_32(base + GPIO_DATA_OFFSET, BIT(offset));
	else
		mmio_setbits_32(base + GPIO_DATA_OFFSET, BIT(offset));

	mmio_clrbits_32(base + GPIO_MASK_OFFSET, BIT(offset));

	spin_unlock(&gpio_lock[bank]);
}

static const gpio_ops_t ambarella_gpio_ops = {
	.get_direction	= ambarella_get_direction,
	.set_direction	= ambarella_set_direction,
	.get_value	= ambarella_get_value,
	.set_value	= ambarella_set_value,
};

void ambarella_gpio_init(void)
{
	if (!ambarella_is_primary_cluster())
		return ;

	/* FIXME: do we need to enable each GPIO controller here? */
	gpio_init(&ambarella_gpio_ops);
}

void ambarella_gpio_notify_mcu(uint32_t notify)
{
	uint64_t gpiobase;
	uint32_t offset, bank;
	uint32_t notify_gpio = boot_cookie_ptr()->soc_notify_gpioaddr;

	if (notify_gpio == 0) {
#if defined (PLATFORM_SUPPORT_CMVI)
		mmio_write_32((SRAM_BASE + 0x0), IPI_0_BIT_MSK);
		mmio_write_32((SRAM_BASE + 0x4), (uint32_t)((uint64_t)CPU_IPI2CM3_REG & 0xFFFFFFFFU));
		mmio_write_32((SRAM_BASE + 0x8), (uint32_t)((uint64_t)CPU_IPI2CM3_REG >> 32));
		mmio_write_32((SRAM_BASE + 0xC), (uint32_t)((uint64_t)CMVI_SRS_NOTIFY_REG & 0xFFFFFFFFU));
		mmio_write_32((SRAM_BASE + 0x10), (uint32_t)((uint64_t)CMVI_SRS_NOTIFY_REG >> 32));
#else
		mmio_write_32((SRAM_BASE + 0x0), 0x0);
		mmio_write_32((SRAM_BASE + 0x4), 0x0);
		mmio_write_32((SRAM_BASE + 0x8), 0x0);
#endif
		flush_dcache_range(SRAM_BASE, 16);
		return;
	}

	bank = gpio_base_to_bank(notify_gpio & ~0x1f);
	offset = notify_gpio & 0x1f;
	notify_gpio = bank * 32 + offset;
	gpio_set_direction(notify_gpio, GPIO_DIR_OUT);
	gpio_set_value(notify_gpio, GPIO_LEVEL_HIGH);

	switch (notify) {
	case NOTIFY_MCU_RECOVERY:
		/* Do something here if need recovery */
		break;

	case NOTIFY_MCU_SUSPEND: /* 0 -> 1 -> 0 -> 1  -> 0 */
		/* make the gpio ready and only send signal when DDR in SRS state */
		gpiobase = gpio_base[bank];
		mmio_setbits_32((gpiobase + GPIO_MASK_OFFSET), BIT(offset));
		/* use SRAM to pass srs notify gpio */
		mmio_write_32((SRAM_BASE + 0x0), BIT(offset)); /* mask */
		mmio_write_32((SRAM_BASE + 0x4), (uint32_t)(gpiobase & 0xFFFFFFFF));
		mmio_write_32((SRAM_BASE + 0x8), (uint32_t)(gpiobase >> 32));
		flush_dcache_range(SRAM_BASE, 16);
		dsb();
		break;

	case NOTIFY_MCU_POWEROFF: /* 0 -> 1 -> 0 -> 1(long-keep) -> 0 -> 1 */
		NOTICE("PowerOff by PSCI \n");
		mdelay(60);
		gpio_set_value(notify_gpio, GPIO_LEVEL_LOW); /* 0 */
		mdelay(20);
		gpio_set_value(notify_gpio, GPIO_LEVEL_HIGH);/* 1 */
		mdelay(20);
		gpio_set_value(notify_gpio, GPIO_LEVEL_LOW); /* 0 */
		mdelay(20);
		gpio_set_value(notify_gpio, GPIO_LEVEL_HIGH);/* 1 (long for poweroff) */
		mdelay(100);
		gpio_set_value(notify_gpio, GPIO_LEVEL_LOW); /* 0 */
		mdelay(20);
		gpio_set_value(notify_gpio, GPIO_LEVEL_HIGH);/* 1 */
		mdelay(20);
		/* stop and wait power-off */
		while (1)
			wfi();
		break;
	default:
		break;
	}
}

void ambarella_pinctrl_security_request(uintptr_t *reg, uint32_t *mask)
{
	gpio_security.pinctrl_reg[0] = reg[0];
	gpio_security.pinctrl_reg[1] = reg[1];

	memcpy(gpio_security.gpio_mask, mask,
			sizeof(uint32_t) * GPIO_BANK);
}

void ambarella_gpio_security_request(uintptr_t *reg, uint32_t bank)
{
	uint32_t idx = gpio_security.nr_bank_req;

	if (idx >= GPIO_BANK) {
		ERROR("%s: secure GPIO bank count exceeds GPIO_BANK (%u)\n",
		      __func__, GPIO_BANK);
		return;
	}

	gpio_security.gpio[idx].index = bank;
	gpio_security.gpio[idx].gpio_reg[0] = reg[0];
	gpio_security.gpio[idx].gpio_reg[1] = reg[1];

	gpio_security.nr_bank_req ++;
}

int32_t ambarella_gpio_security_handle(uint64_t *value, u_register_t reg, uint32_t sas, uint32_t wnr)
{
	u_register_t *gpio_reg;
	uint32_t i, nr_bank_req;
	nr_bank_req = gpio_security.nr_bank_req;

	for (i = 0; i < nr_bank_req; i++) {
		gpio_reg = gpio_security.gpio[i].gpio_reg;
		if (reg >= gpio_reg[0] && reg < (gpio_reg[0] + GPIO_ENABLE_OFFSET)) {
			uint32_t bank, newval, mask, orgval;

			bank = gpio_security.gpio[i].index;
			mask = gpio_security.gpio_mask[bank];

			/* Only support Word Access */
			if (sas != 2)
				return 0;

			spin_lock(&gpio_lock[bank]);
			if (wnr) {
				newval = orgval =  mmio_read_32(reg);
				newval = (*value & ~mask) | (newval & mask);
				mmio_write_32(reg, newval);

				if ((orgval & mask) != (*value & mask))
					NOTICE("GPIO register: %08lx value: %08lx orgval: %08x mask: %08x newval: %08x\n",
							reg, *value, orgval, mask, newval);
			} else {
				*value = mmio_read_32(reg);
			}
			spin_unlock(&gpio_lock[bank]);

			return 0;
		}
	}

	/* Not Found */
	return -EFAULT;
}

int32_t ambarella_pinctrl_security_handle(uint64_t *value, u_register_t reg, uint32_t sas, uint32_t wnr)
{
	u_register_t *pinctrl_reg = gpio_security.pinctrl_reg;

	if (reg >= pinctrl_reg[0] && reg < (pinctrl_reg[0] + IOMUX_CTRL_SET_OFFSET)) {
		uint32_t bank, mask, newval, orgval;

		bank = (reg - pinctrl_reg[0]) / 0xC;
		if (bank >= GPIO_BANK)
			return 0;

		/* If there no mask in this GPIO bank, handle the RW operation normally. */
		mask = gpio_security.gpio_mask[bank];
		if (!mask)
			return 1;

		/* Only support Word Access */
		if (sas != 2)
			return 0;

		spin_lock(&pinctrl_lock);
		if (wnr) {
			newval = orgval = mmio_read_32(reg);
			newval = (*value & ~mask) | (newval & mask);
			mmio_write_32(reg, newval);

			if ((orgval & mask) != (*value & mask))
				NOTICE("Pinctrl register: %08lx value: %08lx orgval: %08x mask: %08x newval: %08x\n",
						reg, *value, orgval, mask, newval);
		} else {
			*value = mmio_read_32(reg);
		}
		spin_unlock(&pinctrl_lock);

		return 0;
	}

	/* Not Found */
	return -EFAULT;
}
