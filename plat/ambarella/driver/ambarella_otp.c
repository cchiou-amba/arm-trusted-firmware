/*
 * Copyright (c) 2026 Ambarella International LP
 *
 * License Identifier: AMBARELLA-2-Clause
 */
#include <assert.h>
#include <lib/mmio.h>
#include <lib/spinlock.h>
#include <drivers/gpio.h>
#include <drivers/delay_timer.h>
#include <plat_private.h>
#include "ambarella_otp.h"

/* this is for debug OTP only, it must be disabled in real product to prevent sensitive information on OTP expose to non-secure world */
//#define DEBUG_OTP

#ifdef NEED_RESV_UPPER_128BITS
// if the product already use user data lower 128 bits, and still want to fill upper 128 bits, enable below macro
//#define FILL_USR_DATA_UPPER_PART_IF_ONLY_LOWER_PART_IS_WRITTEN
#endif

#define D_LOCKED_RETCODE (-80)
#define D_NOT_AVAILABLE_RETCODE (-81)

typedef struct {
	unsigned int lock_bits;
	unsigned int invalid_bits;
	unsigned int sysconfig;
	unsigned int sysconfig_mask;

	unsigned int secure_boot_permanent_en : 1;
	unsigned int jtag_efuse : 1;
	unsigned int sysconfig_locked : 1;
	unsigned int huk_locked : 1;
	unsigned int customer_id_locked : 1;
	unsigned int zone_a_locked : 1;
	unsigned int cst_seed_cuk_locked : 1;
	unsigned int usr_cuk_locked : 1;
	unsigned int anti_rollback_en : 1;
	unsigned int secure_usb_boot_dis : 1;
	unsigned int all_rot_key_lock_together : 1;

	unsigned int otp_layout_ver : 4; // cv2x&s6lm ver = 1, cv5 ver = 2
	unsigned int mono_cnts_disabled : 1;
	unsigned int reserved : 3;
	unsigned int not_revokeable_key_index : 8;
	unsigned int has_bst_anti_rollback : 1;
	unsigned int need_fill_and_lock_amba_reserved : 1;
	unsigned int amba_reserved_locked : 1;
	unsigned int mono_cnt_keep_ori_inc_bit_written : 1;
	unsigned int user_data_keep_ori_inc_bit_written : 1;

	unsigned int rot_pubkey_lock_base;
	unsigned int aes_key_lock_base;
	unsigned int ecc_key_lock_base;
	unsigned int usr_slot_g0_lock_base;
	unsigned int usr_slot_g1_lock_base;

	unsigned int num_of_rot_pub_keys;
	unsigned int num_of_aes_keys;
	unsigned int num_of_ecc_keys;
	unsigned int num_of_usr_slot_g0;
	unsigned int num_of_usr_slot_g1;
} otp_setting_t;

typedef struct {
	unsigned int amba_resv_bit_locked : 1;
	unsigned int mono_cnt_keep_ori_inc_bit_written : 1;
	unsigned int user_data_keep_ori_inc_bit_written : 1;
	unsigned int mono_cnt_0_started : 1;
	unsigned int mono_cnt_1_started : 1;
	unsigned int mono_cnt_2_started : 1;
	unsigned int test_region_written : 1;
	unsigned int test_region_locked : 1;

	unsigned int need_fill_and_lock_amba_reserved : 1;
	unsigned int user_data_0_lower_written : 1;
	unsigned int user_data_0_upper_written : 1;
	unsigned int user_data_0_upper_all_1 : 1;
	unsigned int user_data_1_lower_written : 1;
	unsigned int user_data_1_upper_written : 1;
	unsigned int user_data_1_upper_all_1 : 1;
	unsigned int user_data_2_lower_written : 1;

	unsigned int user_data_2_upper_written : 1;
	unsigned int user_data_2_upper_all_1 : 1;
	unsigned int mono_cnts_disabled : 1;
	unsigned int reserved0 : 13;
} otp_diagnosis_report_t;

//#define D_LOCK_ZONE_A_EXPLICITE

/* enable it for debug purpose */
#define D_INTERNAL_CHECK

static spinlock_t otp_access_lock;

#if (OTP_LAYOUT_VERSION == 2)

struct otp_mono_region {
	uint32_t addr;
	uint32_t size;
	uint32_t base_cnt;
};

struct otp_mono_region gs_ori_mono_cnts[] = {
	{MONO_CNT_0_ADDR, MONO_CNT_0_BITS, 0},
	{MONO_CNT_1_ADDR, MONO_CNT_1_BITS, 0},
	{MONO_CNT_2_ADDR, MONO_CNT_2_BITS, 0},
};

struct otp_mono_region gs_new_mono_cnts[] = {
	{MONO_CNT_NEW_0_ADDR, MONO_CNT_NEW_0_BITS, MONO_CNT_NEW_0_BITS / 2},
	{MONO_CNT_NEW_1_ADDR, MONO_CNT_NEW_1_BITS, MONO_CNT_NEW_1_BITS / 2},
	{MONO_CNT_NEW_2_ADDR, MONO_CNT_NEW_2_BITS, MONO_CNT_NEW_2_BITS / 2},
};

struct otp_generic_access_region {
	const char * region_name;
	uint32_t addr;
	uint32_t size;

	uint8_t has_access_indication_bit;
	uint8_t access_indication_bit;
	uint8_t reserved0;
	uint8_t reserved1;
};

struct otp_generic_access_region gs_generic_access_region_list[] = {
#ifdef NEED_RESV_UPPER_128BITS
	{"mono cnt 0", MONO_CNT_NEW_0_ADDR, MONO_CNT_NEW_0_BITS, 1, DISABLE_MONO_CNTS_INC_BIT},
	{"mono cnt 1", MONO_CNT_NEW_1_ADDR, MONO_CNT_NEW_1_BITS, 1, DISABLE_MONO_CNTS_INC_BIT},
	{"mono cnt 2", MONO_CNT_NEW_2_ADDR, MONO_CNT_NEW_2_BITS, 1, DISABLE_MONO_CNTS_INC_BIT},

	{"usr data 1", USER_DATA_BASE_NEW_ADDR, USER_DATA_BITS, 0, 0},
	{"usr data 2", USER_DATA_BASE_NEW_ADDR + USER_DATA_BITS, USER_DATA_BITS, 0, 0},
#else
	{"usr data 0", USER_DATA_BASE_ADDR, USER_DATA_BITS, 0, 0},
	{"usr data 1", USER_DATA_BASE_ADDR + USER_DATA_BITS, USER_DATA_BITS, 0, 0},
	{"usr data 2", USER_DATA_BASE_ADDR + USER_DATA_BITS * 2, USER_DATA_BITS, 0, 0},
#endif
};

#endif

#if defined(GPIO_OTP_PWR_SW) && !IMAGE_BL2
static void ambarella_otp_write_enable(void)
{
	gpio_set_value(GPIO_OTP_PWR_SW, GPIO_LEVEL_HIGH);
	mdelay(10);
}

static void ambarella_otp_write_disable(void)
{
	mdelay(10);
	gpio_set_value(GPIO_OTP_PWR_SW, GPIO_LEVEL_LOW);
}

void ambarella_otp_init(void)
{
	gpio_set_direction(GPIO_OTP_PWR_SW, GPIO_DIR_OUT);
	gpio_set_value(GPIO_OTP_PWR_SW, GPIO_LEVEL_LOW);
}
#else
static void ambarella_otp_write_enable(void)
{
}

static void ambarella_otp_write_disable(void)
{
}

void ambarella_otp_init(void)
{
}
#endif

int ambarella_otp_read(uint32_t bit_addr, uint32_t length, uint32_t *value)
{
	uint32_t _bit_addr = round_down(bit_addr, 32);

#ifdef D_INTERNAL_CHECK
	if (_bit_addr != round_down(bit_addr + length - 1, 32) || bit_addr > OTP_BIT_SIZE) {
		ERROR("Invalid OTP read address: 0x%x - 0x%x\n",
				bit_addr, bit_addr + length);
		return -31;
	}
#endif

	spin_lock(&otp_access_lock);

	ambarella_otp_init();

	mmio_write_32(OTP_CTRL1_REG, 0);
	mmio_clrbits_32(OTP_CTRL1_REG, FSM_WRITE_MODE);		/* fsm_write_mode = 0 */
	mmio_setbits_32(OTP_CTRL1_REG, DBG_READ_MODE);
	mmio_setbits_32(OTP_CTRL1_REG, READ_FSM_ENABLE);

	while (!(mmio_read_32(OTP_OBSV_REG) & READ_OBSV_RDY));

	mmio_clrsetbits_32(OTP_CTRL1_REG, (OTP_BIT_SIZE - 1), _bit_addr);

	mmio_setbits_32(OTP_CTRL1_REG, READ_ENABLE);

	while (!(mmio_read_32(OTP_OBSV_REG) & READ_OBSV_DONE));

	*value = mmio_read_32(OTP_READ_DOUT_REG);
	*value >>= (bit_addr % 32);
	*value &= (1ULL << length) - 1;

	mmio_clrbits_32(OTP_CTRL1_REG, READ_ENABLE);

	while (!(mmio_read_32(OTP_OBSV_REG) & READ_OBSV_RDY));

	spin_unlock(&otp_access_lock);

	return 0;
}

int ambarella_otp_write(uint32_t bit_addr, uint32_t length, uint32_t value)
{
	uint32_t _bit_addr = round_down(bit_addr, 32), i, val, failed;
	uint32_t tmp_val = 0;
	int failed_retry_times;
	int ret = 0;

#ifdef D_INTERNAL_CHECK
	if (_bit_addr != round_down(bit_addr + length - 1, 32) || bit_addr > OTP_BIT_SIZE) {
		ERROR("Invalid OTP write address: 0x%x - 0x%x\n",
				bit_addr, bit_addr + length);
		return -41;
	}
#endif

	if (ambarella_otp_read(bit_addr, length, &val) < 0)
		return -42;

	spin_lock(&otp_access_lock);

	ambarella_otp_init();

	ambarella_otp_write_enable();

	for (i = 0; i < length; i++) {
		/* do nothing if the OTP bit value is the same as required. */
		if (((val ^ value) & (1U << i)) == 0)
			continue;

		/* the OTP bit can only be set, but cannot be clear. */
		if (val & (1U << i)) {
			ERROR("cannot clear OTP bit at 0x%x\n", bit_addr + i);
			ret = -43;
			goto __write_disable;
		}

		/* retry write times, when otp program failed. */
		failed_retry_times = 1;

failed_retry_write_otp:

		mmio_write_32(OTP_CTRL1_REG, 0);
		mmio_setbits_32(OTP_CTRL1_REG, FSM_WRITE_MODE);
		mmio_setbits_32(OTP_CTRL1_REG, PROG_FSM_ENABLE);

		while (!(mmio_read_32(OTP_OBSV_REG) & WRITE_PROG_RDY));

		tmp_val = mmio_read_32(OTP_CTRL1_REG);
		tmp_val &= ~(OTP_BIT_SIZE - 1);
		tmp_val |= ((bit_addr + i) & (OTP_BIT_SIZE - 1));
		mmio_write_32(OTP_CTRL1_REG, tmp_val);

#ifdef HAS_OTP_INVERT_REG
		mmio_write_32(OTP_CTRL1_INVERT_REG, ~(tmp_val | PROG_ENABLE));
#endif

		mmio_setbits_32(OTP_CTRL1_REG, PROG_ENABLE);
		while (!(mmio_read_32(OTP_OBSV_REG) & WRITE_PROG_DONE));
		failed = mmio_read_32(OTP_OBSV_REG) & WRITE_PROG_FAIL;

		mmio_clrbits_32(OTP_CTRL1_REG, PROG_ENABLE);
		while (!(mmio_read_32(OTP_OBSV_REG) & WRITE_PROG_RDY));

		if (failed) {
			if (failed_retry_times > 0) {
				ERROR("OTP write at 0x%08x failed, retry (%d)...\n",
					bit_addr + i, failed_retry_times);
				failed_retry_times --;
				goto failed_retry_write_otp;
			}
			ERROR("OTP write at 0x%08x failed, exit.\n", bit_addr + i);
			ret = -44;
			goto __write_disable;
		}
	}

__write_disable:
	ambarella_otp_write_disable();
	spin_unlock(&otp_access_lock);

	return ret;
}

int ambarella_otp_write_fast_u32(uint32_t start_bit_addr, uint32_t num, uint32_t * p_u32)
{
	uint32_t cur_bit_addr, i, j, failed;
	uint32_t cur_val;
	uint32_t tmp_val = 0;
	int failed_retry_times;
	int ret = 0;

	if ((!num) || (!p_u32)) {
		ERROR("bad parameters: 0x%x\n", start_bit_addr);
		return (-50);
	}

	if (start_bit_addr & 0x1F) {
		ERROR("start_bit_addr not 32bit aligned: 0x%x\n", start_bit_addr);
		return (-51);
	}
	if ((start_bit_addr + (num * 32)) > OTP_BIT_SIZE) {
		ERROR("bit_addr range exceed valid range: 0x%08x, otp_size 0x%08x\n",
			(start_bit_addr + (num * 32)), OTP_BIT_SIZE);
		return (-52);
	}

	spin_lock(&otp_access_lock);

	ambarella_otp_init();

	ambarella_otp_write_enable();

	cur_bit_addr = start_bit_addr;
	for (j = 0; j < num; j++, cur_bit_addr += 32) {

		cur_val = p_u32[j];
		failed = 0;
		for (i = 0; i < 32; i++) {

			/* skip bit 0. */
			if (!(cur_val & (1U << i))) {
				continue;
			}

			/* retry write times, when otp program failed. */
			failed_retry_times = 1;

failed_retry_write_otp_fast_u32:

			mmio_write_32(OTP_CTRL1_REG, 0);
			mmio_setbits_32(OTP_CTRL1_REG, FSM_WRITE_MODE);
			mmio_setbits_32(OTP_CTRL1_REG, PROG_FSM_ENABLE);

			while (!(mmio_read_32(OTP_OBSV_REG) & WRITE_PROG_RDY));

			tmp_val = mmio_read_32(OTP_CTRL1_REG);
			tmp_val &= ~(OTP_BIT_SIZE - 1);
			tmp_val |= ((cur_bit_addr + i) & (OTP_BIT_SIZE - 1));
			mmio_write_32(OTP_CTRL1_REG, tmp_val);

#ifdef HAS_OTP_INVERT_REG
			mmio_write_32(OTP_CTRL1_INVERT_REG, ~(tmp_val | PROG_ENABLE));
#endif
			mmio_setbits_32(OTP_CTRL1_REG, PROG_ENABLE);
			while (!(mmio_read_32(OTP_OBSV_REG) & WRITE_PROG_DONE));
			failed = mmio_read_32(OTP_OBSV_REG) & WRITE_PROG_FAIL;

			mmio_clrbits_32(OTP_CTRL1_REG, PROG_ENABLE);
			while (!(mmio_read_32(OTP_OBSV_REG) & WRITE_PROG_RDY));

			if (failed) {
				if (failed_retry_times > 0) {
					ERROR("OTP write at 0x%08x failed, (start_bit_addr 0x%08x, j %d, i %d), retry (%d)...\n",
						cur_bit_addr + i, cur_bit_addr, j, i, failed_retry_times);
					failed_retry_times --;
					goto failed_retry_write_otp_fast_u32;
				}
				ERROR("OTP write at 0x%08x failed, (start_bit_addr 0x%08x, j %d, i %d), exit.\n",
					cur_bit_addr + i, cur_bit_addr, j, i);
				ret = -53;
				goto __write_disable;
			}

		}

	}

__write_disable:
	ambarella_otp_write_disable();
	spin_unlock(&otp_access_lock);

	return ret;
}

/* no parameter check is needed here */
static int ambarella_otp_read_field(uint32_t addr, uint32_t len,
	void *v)
{
	uint32_t i, *pu32 = v;

	for (i = 0; i < len; i += 32) {
		if (ambarella_otp_read(addr + i, 32, pu32++) < 0) {
			ERROR("OTP read 0x%x failed\n", addr + i);
			return -10;
		}
	}

	return 0;
}

/* no parameter check is needed here */
static int ambarella_otp_write_field(uint32_t addr, uint32_t len,
	void *v, uint32_t lock_index,
	uint32_t write_content, uint32_t write_lock, uint32_t simulate_write,
	uint32_t fast_write,
	const char *op_str)
{
	uint32_t i, lock_bit, value, *pu32;
	int ret;

	if (simulate_write) {
		if (write_content) {
			NOTICE("[Simulate OTP write]: %s: addr 0x%08x, length %d\n",
				op_str, addr, len);
		}
		if (write_lock) {
			NOTICE("[Simulate OTP write]: %s's lock bit: %d\n",
				op_str, lock_index);
		}
		return 0;
	}

	if (write_content) {
		if (fast_write) {
			ret = ambarella_otp_write_fast_u32(addr, len >> 5, (uint32_t *) v);
			if (ret) {
				ERROR("ambarella_otp_write_fast_u32(0x%08x, %d) failed\n",
					addr, len >> 5);
				return ret;
			}
		} else {
			for (i = 0, pu32 = v; i < len; i += 32) {
				if (ambarella_otp_write(addr + i, 32, *pu32++) < 0) {
					ERROR("OTP write 0x%x failed\n", addr + i);
					return -20;
				}
			}

			for (i = 0, pu32 = v; i < len; i += 32) {
				if (ambarella_otp_read(addr + i, 32, &value) < 0) {
					ERROR("OTP read back 0x%x failed\n", addr + i);
					return -21;
				}

				if (value != *pu32++) {
					ERROR("OTP field data dismatch[%d]: 0x%08x, 0x%08x\n",
						addr + i, value, *(pu32 - 1));
					return -22;
				}
			}
		}
	}

	if (write_lock) {
		NOTICE("!!write lock index %d\n", lock_index);
		if (ambarella_otp_write(WRITE_LOCK_BIT_ADDR + lock_index, 1, 1) < 0) {
			ERROR("OTP write lock bit failed: %d\n", lock_index);
			return -23;
		}

		ret = ambarella_otp_read(WRITE_LOCK_BIT_ADDR + lock_index, 1, &lock_bit);
		if (ret < 0 || lock_bit == 0) {
			ERROR("OTP write lock, readback check failed: %d\n", lock_index);
			return -24;
		}
	}

	return 0;
}

static int is_locked(uint32_t lock_bit_index)
{
	uint32_t lock_bit = 0;
	int ret = ambarella_otp_read(WRITE_LOCK_BIT_ADDR + lock_bit_index, 1, &lock_bit);

	if (0 > ret) {
		ERROR("read lock bits (%d) failed\n", lock_bit_index);
		return ret;
	}

	if (lock_bit) {
		return 1;
	}

	return 0;
}

static int store_embedded_flag(unsigned char *p,
	unsigned int length, unsigned int content_length,
	unsigned int lock_index, const char *otp_str)
{
	uint32_t *p_embed_flag = NULL;
	int ret;

	if (length == content_length) {
		p_embed_flag = NULL;
	} else if (length == (content_length + 4)) {
		p_embed_flag = (uint32_t *) ((unsigned long) p + content_length);
	} else {
		ERROR("length not expected (%d) for read %s\n", length, otp_str);
		return -2;
	}

	if (p_embed_flag) {
		*p_embed_flag = 0;

		ret = is_locked(lock_index);
		if (0 < ret) {
			*p_embed_flag |= D_FLAG_LOCKED;
		} else if (0 > ret) {
			*p_embed_flag |= D_FLAG_HW_ERROR;
			return -50;
		}

#ifdef AMBARELLA_CV2
		if (!strcmp(otp_str, "huk")) {
			if (is_locked(0)) {
				*p_embed_flag |= D_FLAG_LOCKED;
			}
		}
#endif

	}
	return 0;
}

static int check_length_get_embedded_flag(unsigned char *p,
	unsigned int length, unsigned int content_length,
	unsigned int *write_content, unsigned int *write_lock,
	unsigned int *simulate, unsigned int *fast_write)
{
	unsigned int embedded_flag;

	if (length == content_length) {
		*write_content = 1;
		*write_lock = 1;
		*simulate = 0;
		*fast_write = 0;
	} else if (length == (content_length + 4)) {
		embedded_flag = *((uint32_t *) ((unsigned long) p + content_length));
		if (embedded_flag & D_IN_FLAG_WRITE_CONTENT) {
			*write_content = 1;
		} else {
			*write_content = 0;
		}
		if (embedded_flag & D_IN_FLAG_WRITE_LOCK) {
			*write_lock = 1;
			NOTICE("write lock request\n");
		} else {
			*write_lock = 0;
		}
		if (embedded_flag & D_IN_FLAG_FAST_WRITE) {
			*fast_write = 1;
		} else {
			*fast_write = 0;
		}
		if (embedded_flag & D_IN_FLAG_SIMULATE_WRITE) {
			*simulate = 1;
		} else {
			*simulate = 0;
		}
	} else {
		ERROR("length not expected (%d)\n",
			length);
		return -60;
	}

	return 0;
}

static int count_one_bit_number(uint32_t addr,
	uint32_t size, uint32_t * tot_cnt)
{
	uint32_t i, j, val, cnt;
	int ret;

	cnt = 0;
	for (i = 0; i < size; i += 32) {
		ret = ambarella_otp_read(addr + i, 32, &val);
		if (0 > ret) {
			ERROR("read mono counter failed, addr 0x%x\n", addr + i);
			return ret;
		}

		for (j = 0; j < 32; j++) {
			if (val & (1U << j)) {
				cnt ++;
			}
		}
	}
	*tot_cnt = cnt;

	return 0;
}

static int program_one_bit(uint32_t addr, uint32_t size, uint32_t simulate)
{
	uint32_t i, j, val;
	uint32_t cur_addr;
	int ret;
	int programmed = 0;

	cur_addr = addr;
	for (i = 0; i < size; i += 32) {
		ret = ambarella_otp_read(cur_addr + i, 32, &val);
		if (0 > ret) {
			ERROR("read mono counter failed, addr 0x%x\n", cur_addr + i);
			return ret;
		}

		for (j = 0; j < 32; j++) {
			if (!(val & (1U << j))) {
				val |= 1U << j;
				break;
			}
		}

		if (j >= 32)
			continue;

		programmed = 1;

		if (simulate) {
			NOTICE("[Simulate OTP write]: increase mono counter: bit addr 0x%08x\n",
				addr + i + j);
			break;
		}

		ret = ambarella_otp_write(addr + i + j, 1, 1);
		if (0 > ret) {
			ERROR("write mono counter failed, addr 0x%x\n", addr + i + j);
			return ret;
		}
		break;
	}

	return programmed;
}

#if (OTP_LAYOUT_VERSION == 2)
static int read_mono_counter(uint32_t *p_cnt,
	uint32_t mono_cnt_index)
{
	int ret;
	uint32_t tot_cnt = 0;
	struct otp_mono_region * p_cur_mono_cnts;

#ifdef NEED_RESV_UPPER_128BITS

	uint32_t lock_bits = 0;
	uint32_t is_mono_cnt_transited;

	is_mono_cnt_transited = 0;
	if (ambarella_otp_read(WRITE_LOCK_BIT_ADDR, 32, &lock_bits) < 0) {
		ERROR("read lock bits failed\n");
		return -1;
	}
	if (lock_bits & (1U << LOCK_BIT_AMBA_RESERVED)) {
		if (!(lock_bits & (1U << MONO_CNTS_KEEP_ORI_INC_BIT))) {
			is_mono_cnt_transited = 1;
		}
	}

	if (is_mono_cnt_transited) {
		p_cur_mono_cnts = gs_new_mono_cnts;
	} else {
		p_cur_mono_cnts = gs_ori_mono_cnts;
	}

	tot_cnt = 0;
	ret = count_one_bit_number(p_cur_mono_cnts[mono_cnt_index].addr,
		p_cur_mono_cnts[mono_cnt_index].size, &tot_cnt);
	if (0 > ret) {
		ERROR("count_one_bit_number failed\n");
		return ret;
	}
	if (tot_cnt < p_cur_mono_cnts[mono_cnt_index].base_cnt) {
		ERROR("tot_cnt not expected\n");
		return -6;
	}
	tot_cnt -= p_cur_mono_cnts[mono_cnt_index].base_cnt;

#else

	p_cur_mono_cnts = gs_ori_mono_cnts;
	tot_cnt = 0;
	ret = count_one_bit_number(p_cur_mono_cnts[mono_cnt_index].addr,
		p_cur_mono_cnts[mono_cnt_index].size, &tot_cnt);
	if (0 > ret) {
		ERROR("count_one_bit_number failed\n");
		return ret;
	}

#endif

	*p_cnt = tot_cnt;
	return 0;
}

static int get_ori_user_data_status(
	uint32_t addr,
	uint32_t size,
	uint32_t * is_lower_128bit_written,
	uint32_t * is_upper_128bit_written,
	uint32_t * is_upper_128bit_all_1)
{
	uint32_t i, val;
	uint32_t lower_written = 0, upper_written = 0, upper_all_1 = 1;
	uint32_t seg_idx, seg_num;
	uint32_t seg_addr;
	int ret;

	seg_num = size / 256;
	seg_idx = 0;
	seg_addr = addr;

	for (seg_idx = 0; seg_idx < seg_num; seg_idx ++, seg_addr += 256) {

		//lower 128 bits
		for (i = 0; i < 128; i += 32) {
			ret = ambarella_otp_read(seg_addr + i, 32, &val);
			if (0 > ret) {
				ERROR("read lower user data failed, addr 0x%x\n", seg_addr + i);
				return ret;
			}
			if (val) {
				lower_written = 1;
				break;
			}
		}

		//upper 128 bits
		for (i = 128; i < 256; i += 32) {
			ret = ambarella_otp_read(seg_addr + i, 32, &val);
			if (0 > ret) {
				ERROR("read upper user data failed, addr 0x%x\n", seg_addr + i);
				return ret;
			}
			if (val) {
				upper_written = 1;
			}
			if (val != 0xFFFFFFFF) {
				upper_all_1 = 0;
			}
		}

	}

	*is_lower_128bit_written = lower_written;
	*is_upper_128bit_written = upper_written;
	*is_upper_128bit_all_1 = upper_all_1;

	return 0;
}

#endif

#ifdef NEED_RESV_UPPER_128BITS
static int get_ori_mono_count_status(
	uint32_t addr,
	uint32_t size,
	uint32_t * is_valid_state,
	uint32_t * p_cnt)
{
	uint32_t i, j, val, cnt;
	uint32_t bit_0_comes = 0;
	int ret;

	*is_valid_state = 1;

	cnt = 0;
	bit_0_comes = 0;
	for (i = 0; i < size; i += 32) {
		ret = ambarella_otp_read(addr + i, 32, &val);
		if (0 > ret) {
			ERROR("read mono counter failed, addr 0x%x\n", addr + i);
			return ret;
		}

		for (j = 0; j < 32; j++) {
			if (0 == bit_0_comes) {
				// wait first '0' bit
				if (val & (1U << j)) {
					cnt ++;
				} else {
					bit_0_comes = 1;
				}
			} else {
				// first '0' bit comes, later bits MUST be all '0'
				if (val & (1U << j)) {
					ERROR("mono bits not in valid state\n");
					*is_valid_state = 0;
				}
			}
		}
	}
	*p_cnt = cnt;

	return 0;
}

static int check_mono_cnts_transitable(
	uint32_t * is_transitable,
	uint32_t * ori_cnt_0,
	uint32_t * ori_cnt_1,
	uint32_t * ori_cnt_2)
{
	int ret;

	uint32_t cur_cnt[NUM_OF_MONO_CNT] = {0};
	uint32_t is_valid_state[NUM_OF_MONO_CNT] = {0};

	*is_transitable = 0;

	// get mono counter 0 status
	ret = get_ori_mono_count_status(
		MONO_CNT_0_ADDR,
		MONO_CNT_0_BITS,
		&is_valid_state[0],
		&cur_cnt[0]);
	if ((ret) || (!is_valid_state[0])) {
		ERROR("mono count 0's status is not correct\n");
		return -1;
	}

	// get mono counter 1 status
	ret = get_ori_mono_count_status(
		MONO_CNT_1_ADDR,
		MONO_CNT_1_BITS,
		&is_valid_state[1],
		&cur_cnt[1]);
	if ((ret) || (!is_valid_state[1])) {
		ERROR("mono count 1's status is not correct\n");
		return -2;
	}

	// get mono counter 2 status
	ret = get_ori_mono_count_status(
		MONO_CNT_2_ADDR,
		MONO_CNT_2_BITS,
		&is_valid_state[2],
		&cur_cnt[2]);
	if ((ret) || (!is_valid_state[2])) {
		ERROR("mono count 2's status is not correct\n");
		return -3;
	}

	// check transite condition

	// mono cnt 0 and mono cnt 2 can always transite success

	// mono cnt 1 need check transite condition
	if ((MONO_CNT_NEW_1_BITS - cur_cnt[2]) < (MONO_CNT_1_BITS - cur_cnt[1])) {
		ERROR("mono count 1 can not transite (cur_cnt[1] %d, cur_cnt[2] %d)\n",
			cur_cnt[1], cur_cnt[2]);
		return -4;
	}

	*ori_cnt_0 = cur_cnt[0];
	*ori_cnt_1 = cur_cnt[1];
	*ori_cnt_2 = cur_cnt[2];

	*is_transitable = 1;

	return 0;
}

static int write_desired_bits_in_128bits_region(
	uint32_t addr, uint32_t desired_bits)
{
	uint32_t i, j;
	int ret = 0;

	for (i = 0; i < 128; i += 32) {
		if (desired_bits >= 32) {
			ret = ambarella_otp_write(addr + i, 32, 0xFFFFFFFF);
			if (0 > ret) {
				ERROR("write mono cnt failed, addr 0x%x\n", addr + i);
				return -1;
			}
			desired_bits -= 32;
		} else if (desired_bits) {
			uint32_t remain_val = 0;
			for (j = 0; j < desired_bits; j++) {
				remain_val |= (0x1U << j);
			}
			ret = ambarella_otp_write(addr + i, 32, remain_val);
			if (0 > ret) {
				ERROR("write mono cnt failed, addr 0x%x\n", addr + i);
				return -1;
			}
			desired_bits = 0;
			break;
		} else {
			break;
		}
	}

	return 0;
}

static int transite_mono_cnt_0(uint32_t cnt_0,
	uint32_t ori_cnt_1)
{
	uint32_t addr;
	uint32_t i;
	int ret = 0;

	if (cnt_0 >= 128) {
		// just use 0 ~ 255, write 256 ~ 511 to '1'
		addr = MONO_CNT_NEW_0_ADDR;
		for (i = 256; i < 512; i += 32) {
			ret = ambarella_otp_write(addr + i, 32, 0xFFFFFFFF);
			if (0 > ret) {
				ERROR("write mono cnt 0 new failed, addr 0x%x\n",
					addr + i);
				return -1;
			}
		}
	} else {
		if (!ori_cnt_1) {
			// use 0 ~ 127 and 256 ~ 383, write 128 ~ 255 and 384 ~ 511 to '1'
			addr = MONO_CNT_NEW_0_ADDR;
			//write 128 ~ 255
			for (i = 128; i < 256; i += 32) {
				ret = ambarella_otp_write(addr + i, 32, 0xFFFFFFFF);
				if (0 > ret) {
					ERROR("write mono cnt 0 new failed, addr 0x%x\n",
						addr + i);
					return -1;
				}
			}
			//write 384 ~ 511
			for (i = 384; i < 512; i += 32) {
				ret = ambarella_otp_write(addr + i, 32, 0xFFFFFFFF);
				if (0 > ret) {
					ERROR("write mono cnt 0 new failed, addr 0x%x\n",
						addr + i);
					return -1;
				}
			}
		} else if (ori_cnt_1 < 128) {
			// use 0 ~ 127 and 384 ~ 511, write 128 ~ 255 and 256 ~ 383 to '1'
			addr = MONO_CNT_NEW_0_ADDR;
			//write 128 ~ 255
			for (i = 128; i < 256; i += 32) {
				ret = ambarella_otp_write(addr + i, 32, 0xFFFFFFFF);
				if (0 > ret) {
					ERROR("write mono cnt 0 new failed, addr 0x%x\n",
						addr + i);
					return -1;
				}
			}
			//write 256 ~ 383
			for (i = 256; i < 384; i += 32) {
				ret = ambarella_otp_write(addr + i, 32, 0xFFFFFFFF);
				if (0 > ret) {
					ERROR("write mono cnt 0 new failed, addr 0x%x\n",
						addr + i);
					return -1;
				}
			}
		} else {
			// borrow some free bits in 128 ~ 255
			uint32_t borrow_bits = ori_cnt_1 - 128;
			uint32_t remain_bits = 128 - borrow_bits;

			addr = MONO_CNT_NEW_0_ADDR + 128;
			ret = write_desired_bits_in_128bits_region(addr, remain_bits);
			if (0 > ret) {
				ERROR("write mono cnt 0 new failed, addr 0x%x\n",
					addr);
				return -1;
			}
		}
	}
	return 0;
}

static int transite_mono_cnt_1(uint32_t cnt_1,
	uint32_t ori_cnt_2)
{
	uint32_t free_bits_ori = MONO_CNT_1_BITS - cnt_1;
	uint32_t free_bits_new = MONO_CNT_NEW_1_BITS - ori_cnt_2;
	uint32_t fill_bits = 0;
	int ret;

	if (free_bits_new == MONO_CNT_NEW_1_BITS) {
		// use 0 ~ 127 and 256 ~ 383, write 128 ~ 255 and 384 ~ 511 to '1'

		ret = write_desired_bits_in_128bits_region(
			MONO_CNT_NEW_1_ADDR + 128, 128);
		if (0 > ret)
			return -1;

		ret = write_desired_bits_in_128bits_region(
			MONO_CNT_NEW_1_ADDR + 384, 128);
		if (0 > ret)
			return -1;

		fill_bits = cnt_1;

		if (fill_bits >= 128) {
			// write 'addr' ~ 'addr + 127'
			ret = write_desired_bits_in_128bits_region(
				MONO_CNT_NEW_1_ADDR, 128);
			if (0 > ret)
				return -1;
			fill_bits -= 128;
		}

		if (fill_bits) {
			// write 'addr + 256' ~ 'addr + 383'
			ret = write_desired_bits_in_128bits_region(
				MONO_CNT_NEW_1_ADDR + 256, fill_bits);
			if (0 > ret)
				return -1;
		}
	} else if (free_bits_new < 128) {
		// does not need fill pattern, just let the mono cnt same with previous one
		fill_bits = 128 - free_bits_ori;

		// write 'addr + 384' ~ 'addr + 511'
		ret = write_desired_bits_in_128bits_region(
			MONO_CNT_NEW_1_ADDR + 384, fill_bits);
		if (0 > ret)
			return -1;
	} else if (free_bits_new < 256) {
		fill_bits = 256 - free_bits_ori;

		if (fill_bits >= 128) {
			// write 'addr + 256' ~ 'addr + 383'
			ret = write_desired_bits_in_128bits_region(
				MONO_CNT_NEW_1_ADDR + 256, 128);
			if (0 > ret)
				return -1;
			fill_bits -= 128;
		}

		if (fill_bits) {
			// write 'addr + 384' ~ 'addr + 511'
			ret = write_desired_bits_in_128bits_region(
				MONO_CNT_NEW_1_ADDR + 384, fill_bits);
			if (0 > ret)
				return -1;
		}
	} else if (free_bits_new >= 384) {
		// use 128 ~ 255 and 384 ~ 511, write 0 ~ 127 and 256 ~ 383 to '1'

		ret = write_desired_bits_in_128bits_region(
			MONO_CNT_NEW_1_ADDR, 128);
		if (0 > ret)
			return -1;

		ret = write_desired_bits_in_128bits_region(
			MONO_CNT_NEW_1_ADDR + 256, 128);
		if (0 > ret)
			return -1;

		fill_bits = cnt_1;

		if (fill_bits >= 128) {
			// write 'addr + 128' ~ 'addr + 255'
			ret = write_desired_bits_in_128bits_region(
				MONO_CNT_NEW_1_ADDR + 128, 128);
			if (0 > ret)
				return -1;
			fill_bits -= 128;
		}

		if (fill_bits) {
			// write 'addr + 384' ~ 'addr + 511'
			ret = write_desired_bits_in_128bits_region(
				MONO_CNT_NEW_1_ADDR + 384, fill_bits);
			if (0 > ret)
				return -1;
		}
	} else {
		// write remaining bits to upper 256
		fill_bits = free_bits_new - free_bits_ori;

		if (fill_bits >= 128) {
			// write 'addr + 256' ~ 'addr + 383'
			ret = write_desired_bits_in_128bits_region(
				MONO_CNT_NEW_1_ADDR + 256, 128);
			if (0 > ret)
				return -1;
			fill_bits -= 128;
		}

		if (fill_bits) {
			// write 'addr + 384' ~ 'addr + 511'
			ret = write_desired_bits_in_128bits_region(
				MONO_CNT_NEW_1_ADDR + 384, fill_bits);
			if (0 > ret)
				return -1;
		}
	}

	return 0;
}

static int transite_mono_cnt_2(uint32_t cnt)
{
	uint32_t i;
	uint32_t seg_idx, seg_num;
	uint32_t seg_addr;
	uint32_t remain_mono_bits = cnt;
	int ret;

	seg_idx = 0;
	seg_num = MONO_CNT_NEW_2_BITS / 256;
	seg_addr = MONO_CNT_NEW_2_ADDR;
	for (seg_idx = 0; seg_idx < seg_num; seg_idx ++, seg_addr += 256) {

		//lower 128 bits
		if (remain_mono_bits >= 128) {
			ret = write_desired_bits_in_128bits_region(seg_addr, 128);
			if (0 > ret)
				return -1;
			remain_mono_bits -= 128;
		} else if (remain_mono_bits) {
			ret = write_desired_bits_in_128bits_region(seg_addr, remain_mono_bits);
			if (0 > ret)
				return -1;
			remain_mono_bits = 0;
		}

		//upper 128 bits
		for (i = 128; i < 256; i += 32) {
			ret = ambarella_otp_write(seg_addr + i, 32, 0xFFFFFFFF);
			if (0 > ret) {
				ERROR("write upper user data failed, addr 0x%x\n", seg_addr + i);
				return -1;
			}
		}
	}

	return 0;
}

static int transite_mono_cnts(
	uint32_t ori_cnt_0,
	uint32_t ori_cnt_1,
	uint32_t ori_cnt_2)
{
	int ret;

	//transite mono cnt 2
	ret = transite_mono_cnt_2(ori_cnt_2);
	if (0 > ret)
		return ret;

	//transite mono cnt 1
	ret = transite_mono_cnt_1(ori_cnt_1, ori_cnt_2);
	if (0 > ret)
		return ret;

	//transite mono cnt 0
	ret = transite_mono_cnt_0(ori_cnt_0, ori_cnt_1);
	if (0 > ret)
		return ret;

	return 0;
}

static int write_user_data_resv_upper_128bits(uint32_t addr, uint32_t size)
{
	uint32_t i;
	uint32_t seg_idx, seg_num;
	uint32_t seg_addr;
	int ret;

	seg_num = size / 256;
	seg_idx = 0;
	seg_addr = addr;

	for (seg_idx = 0; seg_idx < seg_num; seg_idx ++, seg_addr += 256) {

		//upper 128 bits
		for (i = 128; i < 256; i += 32) {
			ret = ambarella_otp_write(seg_addr + i, 32, 0xFFFFFFFF);
			if (0 > ret) {
				ERROR("write upper user data failed, addr 0x%x\n", seg_addr + i);
				return -1;
			}
		}
	}

	return 0;
}

static int handle_mono_cnt_user_data(
	uint32_t * mono_cnt_transited,
	uint32_t * user_data_write_upper)
{
	uint32_t addr, size;
	uint32_t lock_bits = 0;
	uint32_t is_transitable = 0;
	uint32_t ori_cnt_0 = 0;
	uint32_t ori_cnt_1 = 0;
	uint32_t ori_cnt_2 = 0;

	uint32_t user_data_0_is_lower_128bits_written;
	uint32_t user_data_0_is_upper_128bits_written;
	uint32_t user_data_0_is_all_1;

	uint32_t user_data_1_is_lower_128bits_written;
	uint32_t user_data_1_is_upper_128bits_written;
	uint32_t user_data_1_is_all_1;

	uint32_t user_data_2_is_lower_128bits_written;
	uint32_t user_data_2_is_upper_128bits_written;
	uint32_t user_data_2_is_all_1;

	int ret;

	// set initial output params
	*mono_cnt_transited = 0;
	*user_data_write_upper = 0;

	if (ambarella_otp_read(WRITE_LOCK_BIT_ADDR, 32, &lock_bits) < 0) {
		ERROR("read lock bits failed\n");
		return -1;
	}

	if (lock_bits & (1U << LOCK_BIT_AMBA_RESERVED)) {
		//NOTICE("LOCK_BIT_AMBA_RESERVED already locked\n");
		if (!(lock_bits & (1U << MONO_CNTS_KEEP_ORI_INC_BIT))) {
			*mono_cnt_transited = 1;
		}
		if (!(lock_bits & (1U << USER_DATA_KEEP_ORI_INC_BIT))) {
			*user_data_write_upper = 1;
		}
		return 0;
	}

	// get user data 0 status
	addr = USER_DATA_BASE_ADDR;
	size = USER_DATA_BITS;
	ret = get_ori_user_data_status(addr, size,
		&user_data_0_is_lower_128bits_written,
		&user_data_0_is_upper_128bits_written,
		&user_data_0_is_all_1);
	if (0 > ret) {
		ERROR("get_ori_user_data_status(0) failed\n");
		return -3;
	}

	// get user data 1 status
	addr = USER_DATA_BASE_ADDR + USER_DATA_BITS;
	size = USER_DATA_BITS;
	ret = get_ori_user_data_status(addr, size,
		&user_data_1_is_lower_128bits_written,
		&user_data_1_is_upper_128bits_written,
		&user_data_1_is_all_1);
	if (0 > ret) {
		ERROR("get_ori_user_data_status(1) failed\n");
		return -4;
	}

	// get user data 2 status
	addr = USER_DATA_BASE_ADDR + USER_DATA_BITS * 2;
	size = USER_DATA_BITS;
	ret = get_ori_user_data_status(addr, size,
		&user_data_2_is_lower_128bits_written,
		&user_data_2_is_upper_128bits_written,
		&user_data_2_is_all_1);
	if (0 > ret) {
		ERROR("get_ori_user_data_status(2) failed\n");
		return -5;
	}

	// if user data 0 is not written, then check whether mono counter can transite
	if ((!user_data_0_is_lower_128bits_written)
		&& (!user_data_0_is_upper_128bits_written)) {

		// check mono counters
		ret = check_mono_cnts_transitable(&is_transitable,
			&ori_cnt_0, &ori_cnt_1, &ori_cnt_2);
		if (is_transitable) {
			// transite mono counters
			ret = transite_mono_cnts(ori_cnt_0, ori_cnt_1, ori_cnt_2);
			if (0 > ret) {
				ERROR("transite mono counters failed\n");
				return -1;
			}
			*mono_cnt_transited = 1;
		} else {
			// keep mono counters not changed
			NOTICE("keep original mono counters\n");
			ambarella_otp_write(WRITE_LOCK_BIT_ADDR + MONO_CNTS_KEEP_ORI_INC_BIT, 1, 1);

			// write user data 0
			write_user_data_resv_upper_128bits(USER_DATA_BASE_ADDR, USER_DATA_BITS);
		}
	}

	// handle upper 128 bits for data 1 and data 2
	if ((!user_data_1_is_upper_128bits_written)
		&& (!user_data_2_is_upper_128bits_written)) {
		write_user_data_resv_upper_128bits(USER_DATA_BASE_ADDR + USER_DATA_BITS,
			USER_DATA_BITS);
		write_user_data_resv_upper_128bits(USER_DATA_BASE_ADDR + USER_DATA_BITS * 2,
			USER_DATA_BITS);
		*user_data_write_upper = 1;
	} else {

#ifdef FILL_USR_DATA_UPPER_PART_IF_ONLY_LOWER_PART_IS_WRITTEN
		if (!user_data_1_is_upper_128bits_written) {
			write_user_data_resv_upper_128bits(USER_DATA_BASE_ADDR + USER_DATA_BITS,
				USER_DATA_BITS);
		} else {
			// keep user data 1 not changed
			NOTICE("keep original user data 1\n");
		}
		if (!user_data_2_is_upper_128bits_written) {
			write_user_data_resv_upper_128bits(USER_DATA_BASE_ADDR + USER_DATA_BITS * 2,
				USER_DATA_BITS);
		} else {
			// keep user data 2 not changed
			NOTICE("keep original user data 2\n");
		}
#else
		NOTICE("keep original user data 1 and user data 2\n");
#endif

		ambarella_otp_write(WRITE_LOCK_BIT_ADDR + USER_DATA_KEEP_ORI_INC_BIT, 1, 1);
	}

	// handle done
	ambarella_otp_write(WRITE_LOCK_BIT_ADDR + LOCK_BIT_AMBA_RESERVED, 1, 1);

	return 0;
}

#endif

static int is_jtag_efuse_bit_region_locked()
{
	uint32_t lock_bit_index = 0;

#ifdef HAS_FUNCTION_DISABLE
	lock_bit_index = LOCK_BIT_FUNCTION_DISABLE;
#else
	lock_bit_index = LOCK_BIT_SYS_CONFIG;
#endif

	return is_locked(lock_bit_index);
}

static int is_jtag_efuse_bit_written()
{
	uint32_t addr;
	uint32_t val = 0;
	int ret;

#ifdef HAS_FUNCTION_DISABLE
	addr = FUNCTION_DISABLE_ADDR + JTAG_DIS_BIT;
#else
	addr = SYS_CONFIG_BIT_ADDR + JTAG_EFUSE_BIT;
#endif

	ret = ambarella_otp_read(addr, 1, &val);
	if (0 > ret) {
		ERROR("read jtag efuse bit failed\n");
		return ret;
	}

	if (val) {
		return 1;
	}

	return 0;
}

static int write_jtag_efuse_bit(int simulate)
{
	uint32_t addr;
	int ret;

#ifdef HAS_FUNCTION_DISABLE
	addr = FUNCTION_DISABLE_ADDR + JTAG_DIS_BIT;
#else
	addr = SYS_CONFIG_BIT_ADDR + JTAG_EFUSE_BIT;
#endif

	if (!simulate) {
		NOTICE("write JTAG efuse 0x%08x.\n", addr);
		ret = ambarella_otp_write(addr, 1, 1);
		if (0 > ret) {
			return ret;
		}
	} else {
		NOTICE("[Simulate OTP write]: write JTAG efuse 0x%08x\n", addr);
	}

	return 0;
}

int ambarella_otp_read_amba_unique_id(uint8_t *p, uint32_t length)
{
	uint32_t id_addr, id_len;
	int ret;

	INFO("Reading amba unique id\n");

	if ((!p) || (!length)) {
		ERROR("zero buf or length.\n");
		return -1;
	}

	id_addr = UNIQUE_ID_ADDR;
	id_len = UNIQUE_ID_BITS;

#if defined(AMBARELLA_CV25)
	{
		uint32_t v;

		if (ambarella_otp_read(55, 6, &v) < 0)
			return -1;
		if (!v) {
			NOTICE("cv25 chip rev A0 or engineering sample\n");
			id_addr = UNIQUE_ID_ADDR_ORICV25;
		}
	}
#endif

	ret = store_embedded_flag(p,
		length, UNIQUE_ID_BITS / 8,
		LOCK_BIT_UNIQUE_ID, "amba unique id");
	if (0 > ret) {
		return ret;
	}

	return ambarella_otp_read_field(id_addr, id_len, p);
}

int ambarella_otp_read_rot_pubkey(uint8_t *p, uint32_t length,
	uint32_t key_index)
{
	uint32_t pk_addr, pk_len;
	uint32_t lock_bit, invalid_bis = 0;
	int ret;

	if ((!p) || (!length)) {
		ERROR("zero buf or length.\n");
		return -1;
	}

#if (OTP_LAYOUT_VERSION == 1)
	lock_bit = LOCK_BIT_ROT_BASE + key_index;
#elif (OTP_LAYOUT_VERSION == 2)
	lock_bit = LOCK_BIT_ROT_BASE;
#else
	ERROR("not supported layout\n");
	return -3;
#endif

	if (key_index + 1 > ROT_KEY_NUM) {
		ERROR("bad pubkey index %d.\n", key_index);
		return -1;
	}

	INFO("Reading the ROT Public Key %d\n", key_index);

	ret = store_embedded_flag(p,
		length, ROT_PUBKEY_BITS / 8,
		lock_bit, "pub key");
	if (0 > ret) {
		return ret;
	}

	// store invalid/revoke and program indication information
	if (length == ((ROT_PUBKEY_BITS / 8) + 4)) {
		uint32_t *p_embed_flag =
			(uint32_t *) ((unsigned long) p + (ROT_PUBKEY_BITS / 8));

		if (NON_REVOKABLE_KEY_INDEX != key_index) {
			ret = ambarella_otp_read(DATA_INVALID_BIT_ADDR + key_index, 1, &invalid_bis);
			if (ret < 0) {
				ERROR("read invalid bits 0x%x + %d failed\n",
					DATA_INVALID_BIT_ADDR, key_index);
				return ret;
			}
			if (invalid_bis & 0x1) {
				*p_embed_flag |= D_FLAG_REVOKED;
			}
		}

#ifdef HAS_INDICATION_BITS
		{
			uint32_t indication_bits = 0;
			ambarella_otp_read(INDICATION_BITS_ADDR, 32, &indication_bits);
			if (indication_bits & (1U << key_index)) {
				*p_embed_flag |= D_FLAG_PROG_INDICATED;
			}
		}
#endif

	}

	/* If it's RSA, the pub key is RSA N plus RSA RN */
	pk_addr = ROT_PUBKEY_ADDR + key_index * ROT_PUBKEY_BITS;
	pk_len = ROT_PUBKEY_BITS;

	return ambarella_otp_read_field(pk_addr, pk_len, p);
}

int ambarella_otp_write_rot_pubkey(uint8_t *p, uint32_t length,
	uint32_t key_index)
{
	uint32_t pk_addr, pk_len;
	uint32_t lock_bit;
	uint32_t simulate = 0, write_content = 0, write_lock = 0, fast_write = 0;
	int ret;

	INFO("Writing the Public Key %d\n", key_index);

	if ((key_index + 1) > ROT_KEY_NUM) {
		ERROR("bad pubkey index %d.\n", key_index);
		return -1;
	}

	/* check lock status */
#if (OTP_LAYOUT_VERSION == 1)
	lock_bit = LOCK_BIT_ROT_BASE + key_index;
#elif (OTP_LAYOUT_VERSION == 2)
	lock_bit = LOCK_BIT_ROT_BASE;
#else
	ERROR("not supported layout\n");
	return -3;
#endif
	ret = is_locked(lock_bit);
	if (0 < ret) {
#ifdef HAS_INDICATION_BITS
		uint32_t indic_bits = 0;
		// check indication bit first
		ret = ambarella_otp_read(INDICATION_BITS_ADDR, 32, &indic_bits);
		if (0 > ret) {
			ERROR("read indication bits failed, ret %d\n", ret);
			return -8;
		}
		if (!(indic_bits & (1U << key_index))) {
			// write indication bit for this key
			NOTICE("write indication bit for %d\n", key_index);
			ambarella_otp_write(INDICATION_BITS_ADDR + key_index, 1, 1);
			return 0;
		}
#endif
		ERROR("ROT pubkey (%d) already locked\n", key_index);
		return D_LOCKED_RETCODE;
	} else if (0 > ret) {
		ERROR("Read pubkey (%d) lock bit failed\n", key_index);
		return -4;
	}

	if (p && length) {
		ret = check_length_get_embedded_flag(p,
			length, ROT_PUBKEY_BITS / 8,
			&write_content, &write_lock, &simulate, &fast_write);
		if (0 > ret) {
			ERROR("length not expected (%d) for write pubkey\n",
				length);
			return -5;
		}

		pk_addr = ROT_PUBKEY_ADDR + key_index * ROT_PUBKEY_BITS;
		pk_len = ROT_PUBKEY_BITS;

		/* layout2 must use lock API */
#if (OTP_LAYOUT_VERSION == 2)
		write_lock = 0;
#endif

		ret = ambarella_otp_write_field(pk_addr, pk_len,
			p, lock_bit,
			write_content, write_lock, simulate, fast_write,
			"write rot pubkey");
		if (ret) {
			ERROR("ambarella_otp_write_field failed, ret %d.\n", ret);
			return -7;
		}

#ifdef HAS_INDICATION_BITS
		// write indication bit for this key
		ambarella_otp_write(INDICATION_BITS_ADDR + key_index, 1, 1);
#endif

	} else {
		ERROR("zero data or length.\n");
		return -6;
	}

	return ret;
}

int ambarella_otp_lock_rot_pubkey(uint32_t key_index, uint32_t simulate)
{
	uint32_t lock_bit_addr;

	INFO("Locking the Public Key %d\n", key_index);

#if (OTP_LAYOUT_VERSION == 1)
	if ((key_index + 1) > ROT_KEY_NUM) {
		ERROR("bad pubkey index %d.\n", key_index);
		if (key_index == 3) {
			ERROR("using 'lockallrotpubkeys'? don't use this option on CV2x / S6L.\n");
		}
		return -1;
	}
	lock_bit_addr = WRITE_LOCK_BIT_ADDR + LOCK_BIT_ROT_BASE + key_index;
#elif (OTP_LAYOUT_VERSION == 2)
	if (ROT_KEY_NUM == key_index) {
		lock_bit_addr = WRITE_LOCK_BIT_ADDR + LOCK_BIT_ROT_BASE;
	} else {
		ERROR("lock all pubkeys together, should use index = %d (total num).\n",
			ROT_KEY_NUM);
		return -2;
	}
#else
	ERROR("not supported layout\n");
	return -3;
#endif

	if (simulate) {
		NOTICE("[Simulate OTP write]: rot key's lock bit: %d\n",
			lock_bit_addr - WRITE_LOCK_BIT_ADDR);
		return 0;
	}

	return ambarella_otp_write(lock_bit_addr, 1, 1);
}

int ambarella_otp_read_customer_id(uint8_t *p, uint32_t length)
{
	int ret;
	INFO("Reading the customer ID\n");

	if ((!p) || (!length)) {
		ERROR("zero buf or length.\n");
		return -1;
	}

	ret = store_embedded_flag(p,
		length, CUSTOMER_ID_BITS / 8,
		LOCK_BIT_CUSTOMER_ID, "customer id");
	if (0 > ret) {
		return ret;
	}

	return ambarella_otp_read_field(CUSTOMER_ID_ADDR, CUSTOMER_ID_BITS, p);
}

int ambarella_otp_write_customer_id(uint8_t *p, uint32_t length)
{
	uint32_t lock_bit;
	uint32_t write_content = 0, write_lock = 0, simulate = 0, fast_write = 0;
	int ret = 0;

	INFO("Writing the customer ID\n");

	/* check lock status */
	lock_bit = LOCK_BIT_CUSTOMER_ID;
	ret = is_locked(lock_bit);
	if (0 < ret) {
		ERROR("customer id (serial number) already locked\n");
		return D_LOCKED_RETCODE;
	} else if (0 > ret) {
		ERROR("read lock bit failed\n");
		return -4;
	}

	if (p && length) {
		ret = check_length_get_embedded_flag(p,
			length, CUSTOMER_ID_BITS / 8,
			&write_content, &write_lock, &simulate, &fast_write);
		if (0 > ret) {
			ERROR("length not expected (%d) for write customer id\n",
				length);
			return -5;
		}

		return ambarella_otp_write_field(CUSTOMER_ID_ADDR, CUSTOMER_ID_BITS,
			p, lock_bit,
			write_content, write_lock, simulate, fast_write,
			"write customer id");
	} else {
		ERROR("zero data or length.\n");
		return -6;
	}

	return ret;
}

int ambarella_otp_read_mono_counter(uint32_t *p,  uint32_t length,
	uint32_t mono_cnt_index)
{
	uint32_t tot_cnt = 0;
	int ret;

#if (OTP_LAYOUT_VERSION == 2)
	struct otp_mono_region * p_cur_mono_cnts;

	uint32_t is_mono_cnt_transited = 0;
	uint32_t is_user_data_upper_written = 0;
#endif

	NOTICE("Reading the mono counter\n");

	if ((!p) || (!length)) {
		ERROR("zero buf or length.\n");
		return -1;
	}

	if (length != sizeof(uint32_t)) {
		ERROR("bad data length (%d).\n", length);
		return -2;
	}

	if (mono_cnt_index >= NUM_OF_MONO_CNT) {
		ERROR("bad mono_cnt_index (%d).\n", mono_cnt_index);
		return -3;
	}

#if (OTP_LAYOUT_VERSION == 1)

	tot_cnt = 0;
	ret = count_one_bit_number(MONO_CNT_0_ADDR, MONO_CNT_0_BITS, &tot_cnt);
	if (0 > ret) {
		ERROR("count_one_bit_number failed\n");
		return ret;
	}
	*p = tot_cnt;
	return 0;

#else

#ifdef NEED_RESV_UPPER_128BITS
	ret = handle_mono_cnt_user_data(
		&is_mono_cnt_transited,
		&is_user_data_upper_written);
	if (ret < 0) {
		return ret;
	}
#else
	(void) is_user_data_upper_written;
#endif

	if (is_mono_cnt_transited) {
		p_cur_mono_cnts = gs_new_mono_cnts;
	} else {
		p_cur_mono_cnts = gs_ori_mono_cnts;
	}

	tot_cnt = 0;
	ret = count_one_bit_number(p_cur_mono_cnts[mono_cnt_index].addr,
		p_cur_mono_cnts[mono_cnt_index].size, &tot_cnt);
	if (0 > ret) {
		ERROR("count_one_bit_number failed\n");
		return ret;
	}
	if (tot_cnt < p_cur_mono_cnts[mono_cnt_index].base_cnt) {
		ERROR("tot_cnt not expected\n");
		return -6;
	}
	tot_cnt -= p_cur_mono_cnts[mono_cnt_index].base_cnt;

	*p = tot_cnt;
	return 0;

#endif
}

int ambarella_otp_increase_mono_counter(uint32_t mono_cnt_index,
	uint32_t simulate)
{
	int ret;
#if (OTP_LAYOUT_VERSION == 2)
	struct otp_mono_region * p_cur_mono_cnts;

	uint32_t is_mono_cnt_transited = 0;
	uint32_t is_user_data_upper_written = 0;
#endif

	NOTICE("Increasing the monotonic counter\n");

	if (mono_cnt_index >= NUM_OF_MONO_CNT) {
		ERROR("bad mono_cnt_index (%d).\n", mono_cnt_index);
		return -3;
	}

#if (OTP_LAYOUT_VERSION == 1)
	ret = program_one_bit(MONO_CNT_0_ADDR, MONO_CNT_0_BITS, simulate);
#else

#ifdef NEED_RESV_UPPER_128BITS
	ret = handle_mono_cnt_user_data(
		&is_mono_cnt_transited,
		&is_user_data_upper_written);
	if (ret < 0) {
		return ret;
	}
#else
	(void) is_user_data_upper_written;
#endif

	if (is_mono_cnt_transited) {
		p_cur_mono_cnts = gs_new_mono_cnts;
	} else {
		p_cur_mono_cnts = gs_ori_mono_cnts;
	}

	ret = program_one_bit(p_cur_mono_cnts[mono_cnt_index].addr,
		p_cur_mono_cnts[mono_cnt_index].size, simulate);
#endif

	if (0 > ret) {
		ERROR("increase failed\n");
		return ret;
	}
	return 0;
}

int ambarella_otp_permanently_enable_secure_boot(uint32_t simulate)
{
	uint32_t lock_bits = 0, v = 0, addr;
	uint32_t sysconfig_lock_bit = LOCK_BIT_SYS_CONFIG;
	int ret = 0;

	if (ambarella_otp_read(WRITE_LOCK_BIT_ADDR, 32, &lock_bits) < 0) {
		ERROR("enable secure boot: read lock bits failed\n");
		return -1;
	}

	if (!(lock_bits & (1U << sysconfig_lock_bit))) {

		addr = SYS_CONFIG_BIT_ADDR + SECURE_BOOT_BIT;
		NOTICE("Permanently enable secure boot: write OTP[0x%08x]\n", addr);
		if (!simulate) {
			ret= ambarella_otp_write(addr, 1, 1);
			if (0 > ret)
				return ret;
		} else {
			NOTICE("[Simulate OTP write]: write secure boot bit 0x%08x\n", addr);
		}

		addr = SYS_CONFIG_BIT_ADDR + SECURE_BOOT_BIT + 32;
		NOTICE("Permanently enable secure boot: write OTP[0x%08x]\n", addr);
		if (!simulate) {
			ret = ambarella_otp_write(addr, 1, 1);
			if (0 > ret)
				return ret;
		} else {
			NOTICE("[Simulate OTP write]: write secure boot mask bit 0x%08x\n", addr);
		}

		/* when enable secure boot, JTAG efuse shall be set also, otherwise JTAG is always enabled */
		if (!is_jtag_efuse_bit_region_locked()) {
			if (!is_jtag_efuse_bit_written()) {
				write_jtag_efuse_bit(simulate);
			}
		}

#if (OTP_LAYOUT_VERSION == 1)
#ifndef AMBARELLA_CV2
		addr = WRITE_LOCK_BIT_ADDR + sysconfig_lock_bit;
		NOTICE("Permanently enable secure boot: lock sysconfig.\n");
		if (!simulate) {
			ret = ambarella_otp_write(addr, 1, 1);
			if (0 > ret) {
				return ret;
			}
		} else {
			NOTICE("[Simulate OTP write]: lock sysconfig 0x%08x\n", addr);
		}
#endif
#endif

#ifdef NEED_RESV_UPPER_128BITS
		if (!is_locked(LOCK_BIT_AMBA_RESERVED)) {
			uint32_t is_mono_cnt_transited = 0;
			uint32_t is_user_data_upper_written = 0;
			NOTICE("Permanently enable secure boot: handle mono cnt and user data.\n");
			handle_mono_cnt_user_data(&is_mono_cnt_transited, &is_user_data_upper_written);
		}
#endif

		NOTICE("Permanently enable secure boot: done.\n");
	} else {
	   addr = SYS_CONFIG_BIT_ADDR + SECURE_BOOT_BIT;
	   ret = ambarella_otp_read(addr, 1, &v);
	   if (0 > ret) {
		   ERROR("error: read OTP[%d] failed.\n", addr);
		   return ret;
	   }
	   if (v & 0x1) {
		   addr = SYS_CONFIG_BIT_ADDR + SECURE_BOOT_BIT + 32;
		   ret = ambarella_otp_read(addr, 1, &v);
		   if (0 > ret) {
			   ERROR("error: read OTP[%d] failed.\n", addr);
			   return ret;
		   }
		   if (v & 0x1) {
			   NOTICE("Permanently enable secure boot: already enabled.\n");
		   } else {
			   ERROR("error: sysconfig already locked, but otp[%d] is 0.\n",
				   addr);
			   return -3;
		   }
	   } else {
		   ERROR("error: sysconfig already locked, but otp[%d] is 0.\n",
			   addr);
		   return -4;
	   }
	}

	return 0;
}

int ambarella_otp_read_huk(uint8_t *p, uint32_t length)
{
	int ret;

	if ((!p) || (!length)) {
		ERROR("zero buf or length.\n");
		return -1;
	}

	ret = store_embedded_flag(p,
		length, HUK_BITS / 8,
		LOCK_BIT_HUK_NONCE, "huk");
	if (0 > ret) {
		return ret;
	}

	return ambarella_otp_read_field(HUK_ADDR, HUK_BITS, p);
}

int ambarella_otp_read_huk_nonce(uint8_t *p, uint32_t length)
{
	int ret;

	if ((!p) || (!length)) {
		ERROR("zero buf or length.\n");
		return -1;
	}

	ret = store_embedded_flag(p,
		length, (HUK_BITS + HW_NONCE_BITS) / 8,
		LOCK_BIT_HUK_NONCE, "huk_nonce");
	if (0 > ret) {
		return ret;
	}

	return ambarella_otp_read_field(HUK_ADDR, HUK_BITS + HW_NONCE_BITS, p);
}

int ambarella_otp_write_huk_nonce(uint8_t *p, uint32_t length)
{
	uint32_t lock_bit;
	uint32_t write_content = 0, write_lock = 0, simulate = 0, fast_write = 0;
	int ret;

	if ((!p) || (!length)) {
		ERROR("zero buf or length.\n");
		return -1;
	}

#ifdef AMBARELLA_CV2
	if (is_locked(0)) {
		return 0;
	}
#endif

	lock_bit = LOCK_BIT_HUK_NONCE;
	ret = is_locked(lock_bit);
	if (0 < ret) {
		ERROR("HUK already locked\n");
		return D_LOCKED_RETCODE;
	} else if (0 > ret) {
		ERROR("Read HUK lock bit failed\n");
		return -4;
	}

	ret = check_length_get_embedded_flag(p,
		length, (HUK_BITS + HW_NONCE_BITS) / 8,
		&write_content, &write_lock, &simulate, &fast_write);
	if (0 > ret) {
		ERROR("length not expected (%d) for write huk nonce\n",
			length);
		return -5;
	}

	return ambarella_otp_write_field(HUK_ADDR,
		HUK_BITS + HW_NONCE_BITS,
		p, lock_bit,
		write_content, write_lock, simulate, 0,
		"write huk nonce");
}

int ambarella_otp_read_aes_key(uint8_t *p, uint32_t length,
	uint32_t key_index)
{
	uint32_t aes_addr;
	int ret;

	if ((!p) || (!length)) {
		ERROR("zero buf or length.\n");
		return -1;
	}

	if ((key_index + 1) > AES_KEY_NUM) {
		ERROR("bad aeskey index %d.\n", key_index);
		return -1;
	}

	ret = store_embedded_flag(p,
		length, AES_KEY_BITS / 8,
		LOCK_BIT_AES_KEY_BASE + key_index, "aes key");
	if (0 > ret) {
		return ret;
	}

	aes_addr = AES_KEY_BASE_ADDR + key_index * AES_KEY_BITS;

	return ambarella_otp_read_field(aes_addr, AES_KEY_BITS, p);
}

int ambarella_otp_write_aes_key(uint8_t *p, uint32_t length,
	uint32_t key_index)
{
	uint32_t aes_addr, lock_bit;
	uint32_t write_content = 0, write_lock = 0, simulate = 0, fast_write = 0;
	int ret = 0;

	if (key_index + 1 > AES_KEY_NUM) {
		ERROR("bad aeskey index %d.\n", key_index);
		return -1;
	}

	lock_bit = LOCK_BIT_AES_KEY_BASE + key_index;
	ret = is_locked(lock_bit);
	if (0 < ret) {
		ERROR("AES key[%d] already locked\n", key_index);
		return D_LOCKED_RETCODE;
	} else if (0 > ret) {
		ERROR("Read AES key[%d] lock bit failed\n", key_index);
		return -4;
	}

	if (p && length) {
		ret = check_length_get_embedded_flag(p,
			length, AES_KEY_BITS / 8,
			&write_content, &write_lock, &simulate, &fast_write);
		if (0 > ret) {
			ERROR("length not expected (%d) for write aes key\n",
				length);
			return -5;
		}

		aes_addr = AES_KEY_BASE_ADDR + key_index * AES_KEY_BITS;

		return ambarella_otp_write_field(aes_addr, AES_KEY_BITS,
			p, lock_bit,
			write_content, write_lock, simulate, fast_write,
			"write aes key");
	} else {
		ERROR("zero data or length.\n");
		return -6;
	}

	return ret;
}

int ambarella_otp_read_ecc_key(uint8_t *p, uint32_t length,
	uint32_t key_index)
{
	uint32_t ecc_addr;
	int ret;

	if ((!p) || (!length)) {
		ERROR("zero buf or length.\n");
		return -1;
	}

	if ((key_index + 1) > ECC_KEY_NUM) {
		ERROR("bad ecckey index %d.\n", key_index);
		return -1;
	}

	ret = store_embedded_flag(p,
		length, ECC_KEY_BITS / 8,
		LOCK_BIT_ECC_KEY_BASE + key_index, "ecc key");
	if (0 > ret) {
		return ret;
	}

	ecc_addr = ECC_KEY_BASE_ADDR + key_index * ECC_KEY_BITS;

	return ambarella_otp_read_field(ecc_addr, ECC_KEY_BITS, p);
}

int ambarella_otp_write_ecc_key(uint8_t *p, uint32_t length,
	uint32_t key_index)
{
	uint32_t ecc_addr, lock_bit;
	uint32_t write_content = 0, write_lock = 0, simulate = 0, fast_write = 0;
	int ret = 0;

	if ((key_index + 1) > ECC_KEY_NUM) {
		ERROR("bad ecckey index %d.\n", key_index);
		return -1;
	}

	lock_bit = LOCK_BIT_ECC_KEY_BASE + key_index;
	ret = is_locked(lock_bit);
	if (0 < ret) {
		ERROR("ECC key[%d] already locked\n", key_index);
		return D_LOCKED_RETCODE;
	} else if (0 > ret) {
		ERROR("Read ECC key[%d] lock bit failed\n", key_index);
		return -4;
	}

	if (p && length) {
		ret = check_length_get_embedded_flag(p,
			length, ECC_KEY_BITS / 8,
			&write_content, &write_lock, &simulate, &fast_write);
		if (0 > ret) {
			ERROR("length not expected (%d) for write ecc key\n",
				length);
			return -5;
		}

		ecc_addr = ECC_KEY_BASE_ADDR + key_index * ECC_KEY_BITS;

		return ambarella_otp_write_field(ecc_addr, ECC_KEY_BITS,
			p, lock_bit,
			write_content, write_lock, simulate, fast_write,
			"write ecc key");
	} else {
		ERROR("zero data or length.\n");
		return -6;
	}

	return ret;
}

int ambarella_otp_read_user_slot_g0(uint8_t *p, uint32_t length,
	uint32_t slot_index)
{
	uint32_t slot_addr;
	int ret;

	if ((!p) || (!length)) {
		ERROR("zero buf or length.\n");
		return -1;
	}

	if ((slot_index + 1) > USR_SLOT_G0_NUM) {
		ERROR("bad user slot group 0 index %d.\n", slot_index);
		return -1;
	}

	ret = store_embedded_flag(p,
		length, USR_SLOT_G0_BITS / 8,
		LOCK_BIT_USR_SLOT_G0_BASE + slot_index, "usr slot g0");
	if (0 > ret) {
		return ret;
	}

	slot_addr = USR_SLOT_G0_ADDR + slot_index * USR_SLOT_G0_BITS;

	return ambarella_otp_read_field(slot_addr, USR_SLOT_G0_BITS, p);
}

int ambarella_otp_write_user_slot_g0(uint8_t *p, uint32_t length,
	uint32_t slot_index)
{
	uint32_t slot_addr, lock_bit;
	uint32_t write_content = 0, write_lock = 0, simulate = 0, fast_write = 0;
	int ret = 0;

	if ((slot_index + 1) > USR_SLOT_G0_NUM) {
		ERROR("bad user slot group0 index %d.\n", slot_index);
		return -1;
	}

	lock_bit = LOCK_BIT_USR_SLOT_G0_BASE + slot_index;
	ret = is_locked(lock_bit);
	if (0 < ret) {
		ERROR("User slot group0 [%d] already locked\n", slot_index);
		return D_LOCKED_RETCODE;
	} else if (0 > ret) {
		ERROR("Read User slot group0 [%d] lock bit failed\n", slot_index);
		return -4;
	}

	if (p && length) {
		ret = check_length_get_embedded_flag(p,
			length, USR_SLOT_G0_BITS / 8,
			&write_content, &write_lock, &simulate, &fast_write);
		if (0 > ret) {
			ERROR("length not expected (%d) for write usr slot g0\n",
				length);
			return -5;
		}

		slot_addr = USR_SLOT_G0_ADDR + slot_index * USR_SLOT_G0_BITS;

		return ambarella_otp_write_field(slot_addr, USR_SLOT_G0_BITS,
			p, lock_bit,
			write_content, write_lock, simulate, fast_write,
			"write usr slot g0");
	} else {
		ERROR("zero data or length.\n");
		return -6;
	}

	return ret;
}

int ambarella_otp_read_user_slot_g1(uint8_t *p, uint32_t length,
	uint32_t slot_index)
{
#if (OTP_LAYOUT_VERSION == 2)
	uint32_t slot_addr;
	int ret;

	if ((!p) || (!length)) {
		ERROR("zero buf or length.\n");
		return -1;
	}

	if ((slot_index + 1) > USR_SLOT_G1_NUM) {
		ERROR("bad user slot group1 index %d.\n", slot_index);
		return -1;
	}

	ret = store_embedded_flag(p,
		length, USR_SLOT_G1_BITS / 8,
		LOCK_BIT_USR_SLOT_G1_BASE + slot_index, "usr slot g1");
	if (0 > ret) {
		return ret;
	}

	slot_addr = USR_SLOT_G1_ADDR + slot_index * USR_SLOT_G1_BITS;

	return ambarella_otp_read_field(slot_addr, USR_SLOT_G1_BITS, p);
#else
	ERROR("not available on this platform\n");
	return D_NOT_AVAILABLE_RETCODE;
#endif
}

int ambarella_otp_write_user_slot_g1(uint8_t *p, uint32_t length,
	uint32_t slot_index)
{
#if (OTP_LAYOUT_VERSION == 2)
	uint32_t slot_addr, lock_bit;
	uint32_t write_content = 0, write_lock = 0, simulate = 0, fast_write = 0;
	int ret = 0;

	if (slot_index + 1 > USR_SLOT_G1_NUM) {
		ERROR("bad user slot group1 index %d.\n", slot_index);
		return -1;
	}

	lock_bit = LOCK_BIT_USR_SLOT_G1_BASE + slot_index;
	ret = is_locked(lock_bit);
	if (0 < ret) {
		ERROR("User slot group1 [%d] already locked\n", slot_index);
		return D_LOCKED_RETCODE;
	} else if (0 > ret) {
		ERROR("Read User slot group1 [%d] lock bit failed\n", slot_index);
		return -4;
	}

	if (p && length) {
		ret = check_length_get_embedded_flag(p,
			length, USR_SLOT_G1_BITS / 8,
			&write_content, &write_lock, &simulate, &fast_write);
		if (0 > ret) {
			ERROR("length not expected (%d) for write usr slot g1\n",
				length);
			return -5;
		}

		slot_addr = USR_SLOT_G1_ADDR + slot_index * USR_SLOT_G1_BITS;
		return ambarella_otp_write_field(slot_addr, USR_SLOT_G1_BITS,
			p, lock_bit,
			write_content, write_lock, simulate, fast_write,
			"write usr slot g1");
	} else {
		ERROR("zero data or length.\n");
		return -6;
	}

	return ret;
#else
	ERROR("not available on this platform\n");
	return D_NOT_AVAILABLE_RETCODE;
#endif
}

int ambarella_otp_read_test_region(uint8_t *p, uint32_t length)
{
	int ret = 0;

	if ((!p) || (!length)) {
		ERROR("zero buf or length.\n");
		return -1;
	}

	ret = store_embedded_flag(p,
		length, TEST_REGION_BITS / 8,
		LOCK_BIT_TEST_REGION, "test region");
	if (0 > ret) {
		return ret;
	}
	return ambarella_otp_read_field(TEST_REGION_ADDR, TEST_REGION_BITS, p);
}

int ambarella_otp_write_test_region(uint8_t *p, uint32_t length)
{
	uint32_t write_content = 1, write_lock = 0, simulate = 0, fast_write = 0;
	int ret;

	ret = is_locked(LOCK_BIT_TEST_REGION);
	if (ret) {
		ERROR("test region locked\n");
		return D_LOCKED_RETCODE;
	}

	if (p && length) {
		ret = check_length_get_embedded_flag(p,
			length, TEST_REGION_BITS / 8,
			&write_content, &write_lock, &simulate, &fast_write);
		if (0 > ret) {
			ERROR("length not expected (%d) for write test region\n",
				length);
			return -5;
		}

		return ambarella_otp_write_field(TEST_REGION_ADDR, TEST_REGION_BITS,
			p, LOCK_BIT_TEST_REGION,
			write_content, write_lock, simulate, fast_write,
			"write test region");
	} else {
		ERROR("zero data or length.\n");
		return -6;
	}

	return 0;
}

int ambarella_otp_revoke_key(uint32_t index, uint32_t simulate)
{
	uint32_t v;
	uint32_t invalid_bit;
	int ret;

	if (ROT_KEY_NUM <= index) {
		ERROR("error: bad revoke key index %d\n", index);
		return -1;
	}

	if (NON_REVOKABLE_KEY_INDEX == index) {
		ERROR("error: key index %d is not revokable\n", index);
		return -1;
	}

	NOTICE("Revoke key %d\n", index);

	invalid_bit = DATA_INVALID_BIT_ADDR + index;

	if (simulate) {
		NOTICE("[Simulate OTP write]: Revoke key %d, write invalid bit 0x%08x\n",
			index, invalid_bit);
		return 0;
	}

	ret = ambarella_otp_write(invalid_bit, 1, 1);
	if (ret < 0) {
		ERROR("write 0x%x failed, revoke(%d) failed\n",
			invalid_bit, index);
		return ret;
	}

	ret = ambarella_otp_read(invalid_bit, 1, &v);
	if (ret < 0) {
		ERROR("write 0x%x, read back failed, revoke(%d) failed\n",
			invalid_bit, index);
		return ret;
	}

	if (v != 1) {
		ERROR("invalid bit (0x%x) read back not expected, revoke key %d failed\n",
			invalid_bit, index);
		return -3;
	}

	return 0;
}

int ambarella_otp_get_chip_repair_info(uint32_t *p, uint32_t length)
{
	uint32_t repair_addr, repair_len;

	if (!p || length != (500 * 4)) {
		ERROR("invalid params %p, %d.\n", p, length);
		return -1;
	}

	NOTICE("read chip repair info\n");

	repair_addr = CHIP_REPAIR_INFO_ADDR;

#if defined(AMBARELLA_CV72)
	repair_len = 576;
	memset(p + (repair_len / 32), 0x0, length - (repair_len / 8));
#else
	repair_len = CHIP_REPAIR_INFO_BITS;
#endif

	return ambarella_otp_read_field(repair_addr, repair_len, p);
}

int ambarella_otp_query_otp_setting(uint32_t *p, uint32_t length)
{
	otp_setting_t *setting = (otp_setting_t *)p;
	uint32_t sysconfig_lock_bit = LOCK_BIT_SYS_CONFIG;
#if (OTP_LAYOUT_VERSION == 2)
	uint32_t v;
#endif

	if (!p || length != (sizeof(otp_setting_t))) {
		ERROR("invalid params %p, %d.\n", p, length);
		return -1;
	}

#if (OTP_LAYOUT_VERSION == 1)
	setting->otp_layout_ver = 1;
	setting->not_revokeable_key_index = 2;
	setting->need_fill_and_lock_amba_reserved = 0;
#elif (OTP_LAYOUT_VERSION == 2)
	setting->otp_layout_ver = 2;
	setting->not_revokeable_key_index = 0;

#ifdef HAS_BST_ANTI_ROLLBACK
	setting->has_bst_anti_rollback = 1;
#else
	setting->has_bst_anti_rollback = 0;
#endif

#ifdef NO_NEED_FILL_AMBA_RESERVED
	setting->need_fill_and_lock_amba_reserved = 0;
#else
	setting->need_fill_and_lock_amba_reserved = 1;
#endif

#endif

	if (ambarella_otp_read(WRITE_LOCK_BIT_ADDR, 32, &setting->lock_bits) < 0) {
		ERROR("read lock bit failed.\n");
		return -2;
	}

	if (ambarella_otp_read(DATA_INVALID_BIT_ADDR, 32, &setting->invalid_bits) < 0) {
		ERROR("read data invalid bit failed.\n");
		return -3;
	}

	if (ambarella_otp_read(SYS_CONFIG_BIT_ADDR, 32, &setting->sysconfig) < 0) {
		ERROR("read sys config failed.\n");
		return -4;
	}

	if (ambarella_otp_read(SYS_CONFIG_BIT_ADDR + 32, 32, &setting->sysconfig_mask) < 0) {
		ERROR("read sys config mask failed.\n");
		return -5;
	}

	if ((setting->sysconfig & (1U << SECURE_BOOT_BIT))
		&& (setting->sysconfig_mask & (1U << SECURE_BOOT_BIT))) {
		setting->secure_boot_permanent_en = 1;
	} else {
		setting->secure_boot_permanent_en = 0;
	}

	setting->jtag_efuse = is_jtag_efuse_bit_written();

	if (setting->lock_bits & (1U << sysconfig_lock_bit)) {
		setting->sysconfig_locked = 1;
	} else {
		setting->sysconfig_locked = 0;
	}

	if (setting->lock_bits & (1U << LOCK_BIT_HUK_NONCE)) {
		setting->huk_locked = 1;
	} else {
		setting->huk_locked = 0;
	}

	if (setting->lock_bits & (1U << LOCK_BIT_CUSTOMER_ID)) {
		setting->customer_id_locked = 1;
	} else {
		setting->customer_id_locked = 0;
	}

	if (setting->lock_bits & (1U << LOCK_BIT_A)) {
		setting->zone_a_locked = 1;
	} else {
		setting->zone_a_locked = 0;
	}

#ifdef NEED_RESV_UPPER_128BITS
	if (setting->lock_bits & (1U << DISABLE_MONO_CNTS_INC_BIT)) {
		setting->mono_cnts_disabled = 1;
	} else {
		setting->mono_cnts_disabled = 0;
	}
#else
	setting->mono_cnts_disabled = 0;
#endif

#if (OTP_LAYOUT_VERSION == 1)
	setting->cst_seed_cuk_locked = 0;
	setting->usr_cuk_locked = 0;
	setting->anti_rollback_en = 0;
	setting->secure_usb_boot_dis = 0;
	setting->all_rot_key_lock_together = 0;
#elif (OTP_LAYOUT_VERSION == 2)
	if (setting->lock_bits & (1 << LOCK_BIT_CST_PLANTED_SEED_CUK)) {
		setting->cst_seed_cuk_locked = 1;
	} else {
		setting->cst_seed_cuk_locked = 0;
	}
	if (setting->lock_bits & (1 << LOCK_BIT_USR_PLANTED_CUK)) {
		setting->usr_cuk_locked = 1;
	} else {
		setting->usr_cuk_locked = 0;
	}
	if (ambarella_otp_read(EN_ANTI_ROLLBACK_BIT, 1, &v) < 0) {
		ERROR("read bst anti rollback failed.\n");
		return -7;
	}
	if (v & 0x1) {
		setting->anti_rollback_en = 1;
	} else {
		setting->anti_rollback_en = 0;
	}
	if (ambarella_otp_read(DIS_SECURE_USB_BOOT_BIT, 1, &v) < 0) {
		ERROR("read disable secure usb boot failed.\n");
		return -8;
	}
	if (v & 0x1) {
		setting->secure_usb_boot_dis = 1;
	} else {
		setting->secure_usb_boot_dis = 0;
	}
	setting->all_rot_key_lock_together = 1;

	if (setting->lock_bits & (1U << LOCK_BIT_AMBA_RESERVED)) {
		setting->amba_reserved_locked = 1;
	} else {
		setting->amba_reserved_locked = 0;
	}

	if (setting->lock_bits & (1U << MONO_CNTS_KEEP_ORI_INC_BIT)) {
		setting->mono_cnt_keep_ori_inc_bit_written = 1;
	} else {
		setting->mono_cnt_keep_ori_inc_bit_written = 0;
	}
	if (setting->lock_bits & (1U << USER_DATA_KEEP_ORI_INC_BIT)) {
		setting->user_data_keep_ori_inc_bit_written = 1;
	} else {
		setting->user_data_keep_ori_inc_bit_written = 0;
	}
#endif

	setting->rot_pubkey_lock_base = LOCK_BIT_ROT_BASE;
	setting->aes_key_lock_base = LOCK_BIT_AES_KEY_BASE;
	setting->ecc_key_lock_base = LOCK_BIT_ECC_KEY_BASE;
	setting->usr_slot_g0_lock_base = LOCK_BIT_USR_SLOT_G0_BASE;
#if (OTP_LAYOUT_VERSION == 1)
	setting->usr_slot_g1_lock_base = 0;
#else
	setting->usr_slot_g1_lock_base = LOCK_BIT_USR_SLOT_G1_BASE;
#endif

	setting->num_of_rot_pub_keys = ROT_KEY_NUM;
	setting->num_of_aes_keys = AES_KEY_NUM;
	setting->num_of_ecc_keys = ECC_KEY_NUM;
	setting->num_of_usr_slot_g0 = USR_SLOT_G0_NUM;
#if (OTP_LAYOUT_VERSION == 1)
	setting->num_of_usr_slot_g1 = 0;
#else
	setting->num_of_usr_slot_g1 = USR_SLOT_G1_NUM;
#endif

	return 0;
}

int ambarella_otp_set_jtag_efuse(uint32_t simulate)
{
	int ret= 0;

	if (!is_jtag_efuse_bit_region_locked()) {
		if (!is_jtag_efuse_bit_written()) {
			ret = write_jtag_efuse_bit(simulate);
		}
	}

	return ret;
}

int ambarella_otp_lock_zone_a(uint32_t simulate)
{
#if (LOCK_BIT_SYS_CONFIG == 0)
#ifdef D_LOCK_ZONE_A_EXPLICITE
	uint32_t v = 0;

	if (simulate) {
		NOTICE("[Simulate OTP write]: lock zone a, write bit 0x%08x\n",
				WRITE_LOCK_BIT_ADDR);
		return 0;
	}

	if (ambarella_otp_write(WRITE_LOCK_BIT_ADDR, 1, 1) < 0)
		return -1;

	if (ambarella_otp_read(WRITE_LOCK_BIT_ADDR, 1, &v) < 0)
		return -2;

	if (v != 1) {
		ERROR("Lock zone a failed.\n");
		return -3;
	}

	NOTICE("Lock zone a done!\n");
#endif
#else
	NOTICE("Chip does not need customer lock zone A\n");
#endif
	return 0;
}

int ambarella_otp_read_sysconfig(uint8_t *p, uint32_t length)
{
	uint32_t sysconfig_lock_bit = LOCK_BIT_SYS_CONFIG;
	int ret;

	if ((!p) || (!length)) {
		ERROR("zero buf or length.\n");
		return -1;
	}

	NOTICE("Reading the sysconfig\n");

	ret = store_embedded_flag(p, length, SYS_CONFIG_BITS / 8, sysconfig_lock_bit, "sysconfig");
	if (0 > ret) {
		return ret;
	}
	return ambarella_otp_read_field(SYS_CONFIG_BIT_ADDR, SYS_CONFIG_BITS, p);
}

int ambarella_otp_write_sysconfig(uint8_t *p, uint32_t length)
{
	uint32_t sysconfig_lock_bit = LOCK_BIT_SYS_CONFIG;
	uint32_t write_content = 0, write_lock = 0, simulate = 0, fast_write = 0;
	int ret = 0;

	NOTICE("Writing the sysconfig\n");

	/* check lock status */
	ret = is_locked(sysconfig_lock_bit);
	if (0 < ret) {
		ERROR("sysconfig and its selection already locked\n");
		return D_LOCKED_RETCODE;
	} else if (0 > ret) {
		ERROR("read lock bit failed\n");
		return -4;
	}

	if (p && length) {
		ret = check_length_get_embedded_flag(p,
			length, SYS_CONFIG_BITS / 8,
			&write_content, &write_lock, &simulate, &fast_write);
		if (0 > ret) {
			ERROR("length not expected (%d) for write sys config\n",
				length);
			return -5;
		}

#if defined(AMBARELLA_CV2)
		write_lock = 0;
#endif

#ifdef NEED_RESV_UPPER_128BITS
		if ((!simulate) && write_lock && (!is_locked(LOCK_BIT_AMBA_RESERVED))) {
			uint32_t is_mono_cnt_transited = 0;
			uint32_t is_user_data_upper_written = 0;
			NOTICE("write sysconfig: handle mono cnt and user data.\n");
			handle_mono_cnt_user_data(&is_mono_cnt_transited, &is_user_data_upper_written);
		}
#endif

		return ambarella_otp_write_field(SYS_CONFIG_BIT_ADDR,
			SYS_CONFIG_BITS, p, sysconfig_lock_bit,
			write_content, write_lock, simulate, fast_write,
			"write sysconfig");
	} else {
		ERROR("zero data or length.\n");
		return -6;
	}

	return ret;
}

int ambarella_otp_read_cst_planted_seed(uint8_t *p, uint32_t length)
{
#if (OTP_LAYOUT_VERSION == 2)
	int ret;

	if ((!p) || (!length)) {
		ERROR("zero buf or length.\n");
		return -1;
	}

	INFO("Reading the cst seed\n");

	ret = store_embedded_flag(p,
		length, CST_PLANTED_SEED_BITS / 8,
		LOCK_BIT_CST_PLANTED_SEED_CUK, "cst seed");
	if (0 > ret) {
		return ret;
	}
	return ambarella_otp_read_field(CST_PLANTED_SEED_ADDR, CST_PLANTED_SEED_BITS, p);
#else
	ERROR("not available on this platform\n");
	return D_NOT_AVAILABLE_RETCODE;
#endif
}

int ambarella_otp_read_cst_planted_cuk(uint8_t *p, uint32_t length)
{
#if (OTP_LAYOUT_VERSION == 2)
	int ret;

	if ((!p) || (!length)) {
		ERROR("zero buf or length.\n");
		return -1;
	}

	INFO("Reading the cst cuk\n");

	ret = store_embedded_flag(p,
		length, CST_PLANTED_CUK_BITS / 8,
		LOCK_BIT_CST_PLANTED_SEED_CUK, "cst cuk");
	if (0 > ret) {
		return ret;
	}
	return ambarella_otp_read_field(CST_PLANTED_CUK_ADDR, CST_PLANTED_CUK_BITS, p);
#else
	ERROR("not available on this platform\n");
	return D_NOT_AVAILABLE_RETCODE;
#endif
}

int ambarella_otp_write_cst_planted_seed_and_cuk(uint8_t *p,
	uint32_t length)
{
#if (OTP_LAYOUT_VERSION == 2)
	uint32_t write_content = 0, write_lock = 0, simulate = 0, fast_write = 0;
	int ret = 0;

	INFO("Writing customer seed and CUK\n");

	/* check lock status */
	ret = is_locked(LOCK_BIT_CST_PLANTED_SEED_CUK);
	if (0 < ret) {
		ERROR("cst seed and secret already locked\n");
		return D_LOCKED_RETCODE;
	} else if (0 > ret) {
		ERROR("read lock bit failed\n");
		return -4;
	}

	if (p && length) {

		ret = check_length_get_embedded_flag(p,
			length, (CST_PLANTED_SEED_BITS + CST_PLANTED_CUK_BITS) / 8,
			&write_content, &write_lock, &simulate, &fast_write);
		if (0 > ret) {
			ERROR("length not expected (%d) for write cst seed and cuk\n",
				length);
			return -5;
		}

		ret = ambarella_otp_write_field(CST_PLANTED_SEED_ADDR,
			CST_PLANTED_SEED_BITS, p, 0,
			write_content, 0, simulate, 0,
			"write cst seed");
		if (0 > ret) {
			ERROR("write cst seed failed\n");
			return ret;
		}

		ret = ambarella_otp_write_field(CST_PLANTED_CUK_ADDR,
			CST_PLANTED_CUK_BITS, p + (CST_PLANTED_SEED_BITS / 8),
			LOCK_BIT_CST_PLANTED_SEED_CUK,
			write_content, write_lock, simulate, 0,
			"write cst cuk");
		if (0 > ret) {
			ERROR("write cst cuk failed\n");
			return ret;
		}
	} else {
		ERROR("zero data or length.\n");
		return -6;
	}

	return ret;
#else
	ERROR("not available on this platform\n");
	return D_NOT_AVAILABLE_RETCODE;
#endif
}

int ambarella_otp_read_user_planted_cuk(uint8_t *p, uint32_t length)
{
#if (OTP_LAYOUT_VERSION == 2)
	int ret;

	if ((!p) || (!length)) {
		ERROR("zero buf or length.\n");
		return -1;
	}

	INFO("Reading the user planted cuk\n");

	ret = store_embedded_flag(p,
		length, USR_PLANTED_CUK_BITS / 8,
		LOCK_BIT_USR_PLANTED_CUK, "cst cuk");
	if (0 > ret) {
		return ret;
	}
	return ambarella_otp_read_field(USR_PLANTED_CUK_ADDR, USR_PLANTED_CUK_BITS, p);
#else
	ERROR("not available on this platform\n");
	return D_NOT_AVAILABLE_RETCODE;
#endif
}


int ambarella_otp_write_user_planted_cuk(uint8_t *p, uint32_t length)
{
#if (OTP_LAYOUT_VERSION == 2)
	uint32_t write_content = 0, write_lock = 0, simulate = 0, fast_write = 0;
	int ret = 0;

	INFO("Writing user planted cuk\n");

	/* check lock status */
	ret = is_locked(LOCK_BIT_USR_PLANTED_CUK);
	if (0 < ret) {
		ERROR("user planted cuk already locked\n");
		return D_LOCKED_RETCODE;
	} else if (0 > ret) {
		ERROR("read lock bit failed\n");
		return -4;
	}

	if (p && length) {
		ret = check_length_get_embedded_flag(p,
			length, USR_PLANTED_CUK_BITS / 8,
			&write_content, &write_lock, &simulate, &fast_write);
		if (0 > ret) {
			ERROR("length not expected (%d) for write usr seed\n",
				length);
			return -5;
		}

		ret = ambarella_otp_write_field(USR_PLANTED_CUK_ADDR,
			USR_PLANTED_CUK_BITS, p, LOCK_BIT_USR_PLANTED_CUK,
			write_content, write_lock, simulate,
			fast_write,
			"write usr cuk");
		if (0 > ret) {
			ERROR("write user planted cuk failed\n");
			return ret;
		}
	} else {
		ERROR("zero data or length.\n");
		return -6;
	}

	return ret;
#else
	ERROR("not available on this platform\n");
	return D_NOT_AVAILABLE_RETCODE;
#endif
}

int ambarella_otp_read_bst_ver(uint32_t *p, uint32_t length)
{
#if (OTP_LAYOUT_VERSION == 2)
	uint32_t i, j, val, cnt;
	int ret;

	NOTICE("Reading the bst version counter\n");

	if (length != sizeof(uint32_t)) {
		ERROR("bad data length (%d).\n", length);
		return -1;
	}

	cnt = 0;
	for (i = 0; i < BST_VER_BITS; i += 32) {
		ret = ambarella_otp_read(BST_VER_ADDR + i, 32, &val);
		if (0 > ret) {
			ERROR("read bst version counter failed, addr 0x%x\n", BST_VER_ADDR + i);
			return ret;
		}

		for (j = 0; j < 32; j++) {
			if (val & (1U << j)) {
				cnt ++;
			}
		}
	}
	*p = cnt;

	return 0;
#else
	ERROR("not available on this platform\n");
	return D_NOT_AVAILABLE_RETCODE;
#endif
}

int ambarella_otp_increase_bst_ver(uint32_t simulate)
{
#if (OTP_LAYOUT_VERSION == 2)
	uint32_t i, j, val;
	int ret;

	NOTICE("Increasing the bst version counter\n");

	for (i = 0; i < BST_VER_BITS; i += 32) {
		ret = ambarella_otp_read(BST_VER_ADDR + i, 32, &val);
		if (0 > ret) {
			ERROR("read bst version counter failed, addr 0x%x\n", BST_VER_ADDR + i);
			return ret;
		}

		for (j = 0; j < 32; j++) {
			if (!(val & (1U << j))) {
				val |= 1U << j;
				break;
			}
		}

		if (j >= 32)
			continue;

		if (simulate) {
			NOTICE("[Simulate OTP write]: increase bst counter: bit addr 0x%08x\n",
				BST_VER_ADDR + i + j);
			return 0;
		}
		ret = ambarella_otp_write(BST_VER_ADDR + i + j, 1, 1);
		if (0> ret) {
			ERROR("write bst version counter failed, addr 0x%x\n", BST_VER_ADDR + i + j);
			return ret;
		}

		break;
	}

	return 0;
#else
	ERROR("not available on this platform\n");
	return D_NOT_AVAILABLE_RETCODE;
#endif
}

int ambarella_otp_set_bst_anti_rollback(uint32_t simulate)
{
#ifdef HAS_BST_ANTI_ROLLBACK
	int ret = 0;

	NOTICE("Set BST anti-rollback\n");

	if (simulate) {
		NOTICE("[Simulate OTP write]: enable bst anti rollback: bit addr 0x%08x\n",
			EN_ANTI_ROLLBACK_BIT);
		return 0;
	}

	ret = ambarella_otp_write(EN_ANTI_ROLLBACK_BIT, 1, 1);
	if (0 > ret) {
		NOTICE("Set BST anti-rollback failed, ret %d\n", ret);
		return ret;
	}

	NOTICE("Set BST anti-rollback done\n");
	return 0;
#else
	ERROR("not available on this platform\n");
	return D_NOT_AVAILABLE_RETCODE;
#endif
}

int ambarella_otp_disable_secure_usb_boot(uint32_t simulate)
{
#if (OTP_LAYOUT_VERSION == 2)
	int ret = 0;

	NOTICE("Disable Secure USB boot\n");

	if (simulate) {
		NOTICE("[Simulate OTP write]: disable Secure USB boot: bit addr 0x%08x\n",
			DIS_SECURE_USB_BOOT_BIT);
		return 0;
	}

	ret = ambarella_otp_write(DIS_SECURE_USB_BOOT_BIT, 1, 1);
	if (0 > ret) {
		NOTICE("Disable Secure USB boot failed, ret %d\n", ret);
		return ret;
	}

	NOTICE("Disable Secure USB boot done\n");

	return 0;
#else
	ERROR("not available on this platform\n");
	return D_NOT_AVAILABLE_RETCODE;
#endif
}

int ambarella_otp_disable_mono_counters(uint32_t simulate)
{
#if (OTP_LAYOUT_VERSION == 2)
	int ret = 0;
	unsigned int dis_mono_inc_bit =
		WRITE_LOCK_BIT_ADDR + DISABLE_MONO_CNTS_INC_BIT;

	NOTICE("Disable mono counters\n");

	if (simulate) {
		NOTICE("[Simulate OTP write]: disable mono counters: bit addr 0x%08x\n",
			dis_mono_inc_bit);
		return 0;
	}

	ret = ambarella_otp_write(dis_mono_inc_bit, 1, 1);
	if (0 > ret) {
		NOTICE("Disable mono counters failed, ret %d\n", ret);
		return ret;
	}

	NOTICE("Disable mono counters done\n");

	return 0;
#else
	ERROR("not available on this platform\n");
	return D_NOT_AVAILABLE_RETCODE;
#endif
}

int ambarella_otp_read_misc_config(uint32_t *p)
{
	int ret = 0;

	if (!p) {
		ERROR("zero buf\n");
		return -1;
	}

	INFO("Reading misc config\n");

	ret = ambarella_otp_read(MISC_CONFIG_ADDR, MISC_CONFIG_BITS, p);
	if (0 > ret)
		return ret;

	p[1] = MISC_CONFIG_BITS;

	return 0;
}

int ambarella_otp_write_misc_config(uint32_t misc_config, uint32_t simulate)
{
	if (!simulate) {
		int ret = 0;

		ret = ambarella_otp_write(MISC_CONFIG_ADDR, MISC_CONFIG_BITS, misc_config);
		if (0 > ret)
			return ret;
	} else {
		NOTICE("[Simulate OTP write]: write misc config: bit addr 0x%08x, size %d\n",
			MISC_CONFIG_ADDR, MISC_CONFIG_BITS);
	}

	return 0;
}

int ambarella_otp_read_function_disable(uint8_t *p, uint32_t length)
{
#ifdef HAS_FUNCTION_DISABLE
	uint32_t fun_dis_lock_bit = LOCK_BIT_FUNCTION_DISABLE;
	int ret;

	if ((!p) || (!length)) {
		ERROR("zero buf or length.\n");
		return -1;
	}

	NOTICE("Reading the function disable\n");

	ret = store_embedded_flag(p, length, FUNCTION_DISABLE_BITS / 8, fun_dis_lock_bit, "function_disable");
	if (0 > ret) {
		return ret;
	}

	return ambarella_otp_read_field(FUNCTION_DISABLE_ADDR, FUNCTION_DISABLE_BITS, p);
#else
	ERROR("this chip do not have function disable region\n");
	return -1;
#endif
}

int ambarella_otp_write_function_disable(uint8_t *p, uint32_t length)
{
#ifdef HAS_FUNCTION_DISABLE
	uint32_t fun_dis_lock_bit = LOCK_BIT_FUNCTION_DISABLE;
	uint32_t write_content = 0, write_lock = 0, simulate = 0, fast_write = 0;
	int ret = 0;

	NOTICE("Writing the function disable\n");

	/* check lock status */
	ret = is_locked(fun_dis_lock_bit);
	if (0 < ret) {
		ERROR("function disable already locked\n");
		return D_LOCKED_RETCODE;
	} else if (0 > ret) {
		ERROR("read lock bit failed\n");
		return -4;
	}

	if (p && length) {
		ret = check_length_get_embedded_flag(p,
			length, FUNCTION_DISABLE_BITS / 8,
			&write_content, &write_lock, &simulate, &fast_write);
		if (0 > ret) {
			ERROR("length not expected (%d) for write sys config\n",
				length);
			return -5;
		}

		ret = ambarella_otp_write_field(FUNCTION_DISABLE_ADDR,
			FUNCTION_DISABLE_BITS, p, fun_dis_lock_bit,
			write_content, write_lock, simulate, fast_write,
			"write function disable");
	} else {
		ERROR("zero data or length.\n");
		return -6;
	}

	return ret;
#else
	ERROR("this chip do not have function disable region\n");
	return -1;
#endif
}

int ambarella_otp_write_and_lock_amba_reserved(uint32_t simulate)
{
#ifdef NEED_RESV_UPPER_128BITS
	if (!simulate) {
		if (!is_locked(LOCK_BIT_AMBA_RESERVED)) {
			uint32_t is_mono_cnt_transited = 0;
			uint32_t is_user_data_upper_written = 0;
			int ret = 0;
			ret = handle_mono_cnt_user_data(&is_mono_cnt_transited, &is_user_data_upper_written);
			NOTICE("write and lock amba reserved: handle mono cnt and user data, ret %d.\n",
				ret);
			NOTICE("is_mono_cnt_transited %d, is_user_data_upper_written %d\n",
				is_mono_cnt_transited, is_user_data_upper_written);
		} else {
			NOTICE("already locked\n");
			return 0;
		}
	} else {
		NOTICE("[Simulate OTP write]: write and lock amba reserved\n");
	}
#endif
	return 0;
}

int ambarella_otp_lock_amba_reserved(uint32_t simulate)
{
#ifdef NEED_RESV_UPPER_128BITS
#if 0
	if (!simulate) {
		uint32_t lock_bits = 0;

		if (ambarella_otp_read(WRITE_LOCK_BIT_ADDR + LOCK_BIT_AMBA_RESERVED, 1, &lock_bits) < 0)
			return 0;

		if (lock_bits) {
			return 0;
		}

		ambarella_otp_write(WRITE_LOCK_BIT_ADDR + LOCK_BIT_AMBA_RESERVED, 1, 1);
	} else {
		NOTICE("[Simulate OTP write]: lock amba reserved\n");
	}
#endif
	ERROR("obsolete now, please invoke AMBA_SIP_SET_AND_LOCK_AMBA_RESERVED instead\n");
	return -1;
#endif
	return 0;
}

int ambarella_otp_diagnosis(uint32_t *p, uint32_t length)
{
#if (OTP_LAYOUT_VERSION == 2)
	otp_diagnosis_report_t * report = (otp_diagnosis_report_t *) p;
	uint32_t lock_bits = 0;
	uint32_t addr = 0, addr_limit = 0, size = 0;
	uint32_t val = 0, written = 0;
	int ret;

	uint32_t user_data_0_is_lower_128bits_written;
	uint32_t user_data_0_is_upper_128bits_written;
	uint32_t user_data_0_is_all_1;

	uint32_t user_data_1_is_lower_128bits_written;
	uint32_t user_data_1_is_upper_128bits_written;
	uint32_t user_data_1_is_all_1;

	uint32_t user_data_2_is_lower_128bits_written;
	uint32_t user_data_2_is_upper_128bits_written;
	uint32_t user_data_2_is_all_1;

	uint32_t mono_cnt = 0;

	if ((!p) || (length != sizeof(otp_diagnosis_report_t))) {
		ERROR("null buf or bad size\n");
		return -1;
	}

	if (ambarella_otp_read(WRITE_LOCK_BIT_ADDR, 32, &lock_bits) < 0) {
		ERROR("read lock bits failed\n");
		return -2;
	}

#ifdef NEED_RESV_UPPER_128BITS
	report->need_fill_and_lock_amba_reserved = 1;
#else
	report->need_fill_and_lock_amba_reserved = 0;
#endif

	if (lock_bits & (1U << LOCK_BIT_AMBA_RESERVED)) {
		report->amba_resv_bit_locked = 1;
	} else {
		report->amba_resv_bit_locked = 0;
	}

	if (lock_bits & (1U << MONO_CNTS_KEEP_ORI_INC_BIT)) {
		report->mono_cnt_keep_ori_inc_bit_written = 1;
	} else {
		report->mono_cnt_keep_ori_inc_bit_written = 0;
	}

	if (lock_bits & (1U << USER_DATA_KEEP_ORI_INC_BIT)) {
		report->user_data_keep_ori_inc_bit_written = 1;
	} else {
		report->user_data_keep_ori_inc_bit_written = 0;
	}

	if (lock_bits & (1U << LOCK_BIT_TEST_REGION)) {
		report->test_region_locked = 1;
	} else {
		report->test_region_locked = 0;
	}

#ifdef NEED_RESV_UPPER_128BITS
	if (lock_bits & (1U << DISABLE_MONO_CNTS_INC_BIT)) {
		report->mono_cnts_disabled = 1;
	} else {
		report->mono_cnts_disabled = 0;
	}
#else
	report->mono_cnts_disabled = 0;
#endif

	written = 0;
	val = 0;
	addr = TEST_REGION_ADDR;
	addr_limit = TEST_REGION_ADDR + TEST_REGION_BITS;
	for (; addr < addr_limit; addr += 32) {
		ret = ambarella_otp_read(addr, 32, &val);
		if (0 > ret) {
			ERROR("read test region failed, addr 0x%x\n", addr);
			return ret;
		}
		if (val) {
			written = 1;
			break;
		}
	}
	if (written) {
		report->test_region_written = 1;
	} else {
		report->test_region_written = 0;
	}

	// get user data 0 status
	addr = USER_DATA_BASE_ADDR;
	size = USER_DATA_BITS;
	ret = get_ori_user_data_status(addr, size,
		&user_data_0_is_lower_128bits_written,
		&user_data_0_is_upper_128bits_written,
		&user_data_0_is_all_1);
	if (0 > ret) {
		ERROR("get_ori_user_data_status(0) failed\n");
		return -3;
	}

	// get user data 1 status
	addr = USER_DATA_BASE_ADDR + USER_DATA_BITS;
	size = USER_DATA_BITS;
	ret = get_ori_user_data_status(addr, size,
		&user_data_1_is_lower_128bits_written,
		&user_data_1_is_upper_128bits_written,
		&user_data_1_is_all_1);
	if (0 > ret) {
		ERROR("get_ori_user_data_status(1) failed\n");
		return -4;
	}

	// get user data 2 status
	addr = USER_DATA_BASE_ADDR + USER_DATA_BITS * 2;
	size = USER_DATA_BITS;
	ret = get_ori_user_data_status(addr, size,
		&user_data_2_is_lower_128bits_written,
		&user_data_2_is_upper_128bits_written,
		&user_data_2_is_all_1);
	if (0 > ret) {
		ERROR("get_ori_user_data_status(2) failed\n");
		return -5;
	}

	if (user_data_0_is_lower_128bits_written) {
		report->user_data_0_lower_written = 1;
	} else {
		report->user_data_0_lower_written = 0;
	}

	if (user_data_0_is_upper_128bits_written) {
		report->user_data_0_upper_written = 1;
	} else {
		report->user_data_0_upper_written = 0;
	}

	if (user_data_0_is_all_1) {
		report->user_data_0_upper_all_1 = 1;
	} else {
		report->user_data_0_upper_all_1 = 0;
	}

	if (user_data_1_is_lower_128bits_written) {
		report->user_data_1_lower_written = 1;
	} else {
		report->user_data_1_lower_written = 0;
	}

	if (user_data_1_is_upper_128bits_written) {
		report->user_data_1_upper_written = 1;
	} else {
		report->user_data_1_upper_written = 0;
	}

	if (user_data_1_is_all_1) {
		report->user_data_1_upper_all_1 = 1;
	} else {
		report->user_data_1_upper_all_1 = 0;
	}

	if (user_data_2_is_lower_128bits_written) {
		report->user_data_2_lower_written = 1;
	} else {
		report->user_data_2_lower_written = 0;
	}

	if (user_data_2_is_upper_128bits_written) {
		report->user_data_2_upper_written = 1;
	} else {
		report->user_data_2_upper_written = 0;
	}

	if (user_data_2_is_all_1) {
		report->user_data_2_upper_all_1 = 1;
	} else {
		report->user_data_2_upper_all_1 = 0;
	}

	ret = read_mono_counter(&mono_cnt, 0);
	if (ret < 0) {
		return -6;
	}
	if (!mono_cnt) {
		report->mono_cnt_0_started = 0;
	} else {
		report->mono_cnt_0_started = 1;
	}

	ret = read_mono_counter(&mono_cnt, 1);
	if (ret < 0) {
		return -7;
	}
	if (!mono_cnt) {
		report->mono_cnt_1_started = 0;
	} else {
		report->mono_cnt_1_started = 1;
	}

	ret = read_mono_counter(&mono_cnt, 2);
	if (ret < 0) {
		return -8;
	}
	if (!mono_cnt) {
		report->mono_cnt_2_started = 0;
	} else {
		report->mono_cnt_2_started = 1;
	}

	report->reserved0 = 0;
#endif

	return 0;
}

static int is_generic_otp_addr_accessible(
	uint32_t bit_addr_u32)
{
#ifdef DEBUG_OTP
	NOTICE("[debug access 0x%x], be caution\n", bit_addr_u32);
	return 1;
#else

#if (OTP_LAYOUT_VERSION == 2)
	struct otp_generic_access_region * p_region;
	unsigned int num =
		sizeof(gs_generic_access_region_list) / sizeof(struct otp_generic_access_region);
	unsigned int i = 0;

	p_region = gs_generic_access_region_list;
	for (i = 0; i < num; i ++, p_region ++) {
		if ((bit_addr_u32 >= p_region->addr)
			&& ((bit_addr_u32 + 32) <= (p_region->addr + p_region->size))) {
			if (p_region->has_access_indication_bit) {
				uint32_t access_bits = 0;
				ambarella_otp_read(WRITE_LOCK_BIT_ADDR + p_region->access_indication_bit,
					1, &access_bits);
				if (!access_bits) {
					return 0;
				}
			}
			return 1;
		}
	}
#endif

#endif

	return 0;
}

int ambarella_otp_generic_read_u32(uint8_t *p, uint32_t length,
	uint32_t bit_addr_u32)
{
	int ret = 0;

	if ((!p) || (!length)) {
		ERROR("zero buf or length.\n");
		return -1;
	}

	if (length != 4) {
		ERROR("bad length %d.\n", length);
		return -2;
	}

	if (bit_addr_u32 & 0x1F) {
		ERROR("Bug, addr (0x%x) + not aligned\n", bit_addr_u32);
		return -3;
	}

	if ((bit_addr_u32 + 32) > OTP_BIT_SIZE) {
		ERROR("Bug, addr (0x%x) + length (32) out of range\n",
				bit_addr_u32);
		return -4;
	}

	if (!is_generic_otp_addr_accessible(bit_addr_u32)) {
		ERROR("[0x%x] not accessiable via generic way\n",
			bit_addr_u32);
		return -5;
	}

	ret = ambarella_otp_read(bit_addr_u32, 32, (uint32_t*) p);
	return ret;
}

int ambarella_otp_generic_write_u32(uint32_t value,
	uint32_t continue_write,
	uint32_t bit_addr_u32)
{
	int ret = 0;
	uint32_t cur_val = 0;

	if (bit_addr_u32 & 0x1F) {
		ERROR("Bug, addr (0x%x) + not aligned\n", bit_addr_u32);
		return -3;
	}

	if ((bit_addr_u32 + 32) > OTP_BIT_SIZE) {
		ERROR("Bug, addr (0x%x) + length (32) out of range\n",
				bit_addr_u32);
		return -4;
	}

	ret = ambarella_otp_read(bit_addr_u32, 32, &cur_val);
	if (ret) {
		ERROR("read (0x%x) failed, ret %d\n",
				bit_addr_u32, ret);
		return -5;
	}

	if (!continue_write) {
		// aim to write to empty region

		if (cur_val) {
			// already have some bits written, reject this write operation
			ERROR("(0x%x) already have content\n", bit_addr_u32);
			return -6;
		}
	} else {
		uint32_t diff_mask, check_mask;
		// aim to continue write to a region

		diff_mask = value ^ cur_val;
		check_mask = diff_mask & cur_val;

		if (check_mask) {
			ERROR("it's not possible to change bit '1' to bit '0', addr 0x%0x, cur_val 0x%0x, val 0x%0x, check_mask 0x%x\n",
				bit_addr_u32, cur_val, value, check_mask);
			return -7;
		}
	}

	if (!is_generic_otp_addr_accessible(bit_addr_u32)) {
		ERROR("[0x%x] not accessiable via generic way\n",
			bit_addr_u32);
		return -5;
	}

	ret = ambarella_otp_write(bit_addr_u32, 32, value);
	if (ret) {
		ERROR("write (0x%x, value 0x%x) failed, ret %d\n",
				bit_addr_u32, value, ret);
		return -8;
	}

	return 0;
}

int ambarella_otp_read_temp_sensor_params(
	uint32_t *p, uint32_t length)
{
#ifdef HAS_ON_DIE_TEMPERATURE_SENSOR
	int ret;
	INFO("Reading temperature sensor params\n");

	if ((!p) || (!length)) {
		ERROR("zero buf or length.\n");
		return -1;
	}

	ret = store_embedded_flag((unsigned char *) p,
		length, ON_DIE_TEMP_SENSOR_PARAMS_BITS / 8,
		LOCK_BIT_A, "temperature sensor params");
	if (0 > ret) {
		return ret;
	}

	return ambarella_otp_read_field(ON_DIE_TEMP_SENSOR_PARAMS_ADDR,
		ON_DIE_TEMP_SENSOR_PARAMS_BITS, p);
#else
	(void) p;
	(void) length;
	ERROR("no on die temp sensor\n");
	return -1;
#endif
}

// query, index and type
#define NW_QUERY_SUB_INDEX_MASK		0xff00
#define NW_QUERY_SUB_INDEX_SHIFT	8
#define NW_QUERY_SUB_TYPE_MASK		0x00ff
#define NW_QUERY_SUB_TYPE_SHIFT		0
// sub type of query
#define NW_QUERY_OTP_SYSCONFIG		0x01
#define NW_QUERY_OTP_WRITELOCK		0x02
#define NW_QUERY_OTP_DATAINV		0x03
#define NW_QUERY_OTP_UNIQUEID		0x04
#define NW_QUERY_OTP_CUSTOMERID		0x05
#define NW_QUERY_OTP_SECUREBOOT		0x06
#define NW_QUERY_OTP_ROTKEY			0x07
#define NW_QUERY_OTP_DEBUGQUERY		0x08

int ambarella_otp_nw_query_otp(
	unsigned int type_index,
	unsigned int *p_buf, unsigned int buf_size,
	unsigned int *is_locked, unsigned int *is_invalid)
{
	unsigned int lock_bits = 0;
	unsigned int invalid_bits = 0;
	unsigned int lock_bit_index;

	if ((!p_buf) || (!buf_size)) {
		ERROR("null or zero params\n");
		return -1;
	}

	if (NW_QUERY_OTP_SYSCONFIG == type_index) {
#if (OTP_LAYOUT_VERSION == 2)
		lock_bit_index = 3;
#elif defined(AMBARELLA_CV28)
 		lock_bit_index = 24;
#else
		lock_bit_index = 0;
#endif

		if (8 != buf_size) {
			ERROR("size (%d) not expected.\n", buf_size);
			return -2;
		}

		if (ambarella_otp_read(SYS_CONFIG_BIT_ADDR, 32, p_buf) < 0) {
			ERROR("read sys config failed.\n");
			return -3;
		}

		if (ambarella_otp_read(SYS_CONFIG_BIT_ADDR + 32, 32, p_buf + 1) < 0) {
			ERROR("read sys config mask failed.\n");
			return -4;
		}

		if (ambarella_otp_read(WRITE_LOCK_BIT_ADDR, 32, &lock_bits) < 0) {
			ERROR("read lock bits failed.\n");
			return -5;
		}

		if (lock_bits & (1U << lock_bit_index)) {
			*is_locked = 1;
		} else {
			*is_locked = 0;
		}

		return 0;
	} else if (NW_QUERY_OTP_WRITELOCK == type_index) {

		if (4 != buf_size) {
			ERROR("size (%d) not expected.\n", buf_size);
			return -2;
		}

		if (ambarella_otp_read(WRITE_LOCK_BIT_ADDR, 32, p_buf) < 0) {
			ERROR("read lock bits failed.\n");
			return -3;
		}

		return 0;
	} else if (NW_QUERY_OTP_DATAINV == type_index) {

		if (4 != buf_size) {
			ERROR("size (%d) not expected.\n", buf_size);
			return -2;
		}

		if (ambarella_otp_read(DATA_INVALID_BIT_ADDR, 32, p_buf) < 0) {
			ERROR("read invalid bits failed.\n");
			return -3;
		}

		return 0;
	} else if (NW_QUERY_OTP_ROTKEY == (type_index & NW_QUERY_SUB_TYPE_MASK)) {
		uint32_t key_index
			= (type_index & NW_QUERY_SUB_INDEX_MASK) >> NW_QUERY_SUB_INDEX_SHIFT;
		uint32_t rot_addr;
		uint32_t rot_key_num;
		uint32_t rot_size;
		uint32_t i, limit;

#if (OTP_LAYOUT_VERSION == 2)
		rot_addr = 0xD000;
		rot_key_num = 16;
		rot_size = 32;
		lock_bit_index = 8;
#else
		rot_addr = 0x5000;
		rot_key_num = 3;
		rot_size = 512;
		lock_bit_index = 5 + key_index;
#endif

		if (key_index >= rot_key_num) {
			ERROR("key index (%d) exceed.\n", key_index);
			return -5;
		}

		rot_addr += key_index * (rot_size * 8);

		if (buf_size < rot_size) {
			ERROR("buf size (%d) not enough, need %d.\n", buf_size, rot_size);
			return -5;
		}

		if (ambarella_otp_read(WRITE_LOCK_BIT_ADDR, 32, &lock_bits) < 0) {
			ERROR("read lock bits failed.\n");
			return -5;
		}

		if (lock_bits & (1U << lock_bit_index)) {
			*is_locked = 1;
		} else {
			*is_locked = 0;
		}

		if (ambarella_otp_read(DATA_INVALID_BIT_ADDR, 32, &invalid_bits) < 0) {
			ERROR("read invalid bits failed.\n");
			return -3;
		}

		if (invalid_bits & (1U << key_index)) {
			*is_invalid = 1;
		} else {
			*is_invalid = 0;
		}

		limit = rot_size / 4;
		for (i = 0; i < limit; i++, rot_addr += 32) {
			if (ambarella_otp_read(rot_addr, 32, p_buf++) < 0) {
				ERROR("OTP read 0x%x failed\n", rot_addr);
				return -10;
			}
		}

		return 0;
	} else if (type_index == NW_QUERY_OTP_DEBUGQUERY) {
#if (OTP_LAYOUT_VERSION == 2)
		uint32_t addr, addr_limit;

		if (buf_size < 64) {
			ERROR("buf size (%d) not enough, need 64.\n", buf_size);
			return -5;
		}

		addr = 0;
		addr_limit = 512;
		for (; addr < addr_limit; addr += 32) {
			if (ambarella_otp_read(addr, 32, p_buf++) < 0) {
				ERROR("OTP read 0x%x failed\n", addr);
				return -10;
			}
		}
		return 0;
#else
		ERROR("otp layput v1 not support this api\n");
		return (-1);
#endif
	} else if (type_index == NW_QUERY_OTP_UNIQUEID) {

		if (buf_size < 16) {
			ERROR("buf size (%d) not enough, need 16.\n", buf_size);
			return -5;
		}

		return ambarella_otp_read_amba_unique_id((uint8_t *) p_buf, buf_size);
	} else if (type_index == NW_QUERY_OTP_CUSTOMERID) {

		if (buf_size < 16) {
			ERROR("buf size (%d) not enough, need 16.\n", buf_size);
			return -5;
		}

#if (OTP_LAYOUT_VERSION == 2)
		lock_bit_index = 5;
#else
		lock_bit_index = 2;
#endif

		if (ambarella_otp_read(WRITE_LOCK_BIT_ADDR, 32, &lock_bits) < 0) {
			ERROR("read lock bits failed.\n");
			return -5;
		}

		if (lock_bits & (1U << lock_bit_index)) {
			*is_locked = 1;
		} else {
			*is_locked = 0;
		}

		return ambarella_otp_read_customer_id((uint8_t *) p_buf, buf_size);
	} else {
		ERROR("not supported type %x.\n", type_index);
		return -6;
	}

	return 0;
}

