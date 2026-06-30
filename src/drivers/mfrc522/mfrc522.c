/* SPDX-License-Identifier: Apache-2.0 */
#define DT_DRV_COMPAT nxp_mfrc522

#include <string.h>
#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "drivers/mfrc522.h"

LOG_MODULE_REGISTER(mfrc522, CONFIG_SENSOR_LOG_LEVEL);

#define REG_COMMAND	0x01U
#define REG_COM_IRQ	0x04U
#define REG_DIV_IRQ	0x05U
#define REG_ERROR	0x06U
#define REG_FIFO_DATA	0x09U
#define REG_FIFO_LEVEL	0x0AU
#define REG_BIT_FRAMING 0x0DU
#define REG_MODE	0x11U
#define REG_TX_ASK	0x15U
#define REG_TX_CONTROL	0x14U
#define REG_T_MODE	0x2AU
#define REG_T_PRESCALER 0x2BU
#define REG_T_RELOAD_H	0x2CU
#define REG_T_RELOAD_L	0x2DU
#define REG_CRC_RESULT_M 0x21U
#define REG_CRC_RESULT_L 0x22U
#define REG_VERSION	0x37U

#define CMD_IDLE	 0x00U
#define CMD_CALC_CRC	 0x03U
#define CMD_TRANSCEIVE	 0x0CU
#define CMD_SOFT_RESET	 0x0FU

#define PICC_REQA	  0x26U
#define PICC_WUPA	  0x52U
#define PICC_HLTA	  0x50U
#define PICC_CT		  0x88U /* Cascade Tag byte */
#define PICC_CL1	  0x93U
#define PICC_CL2	  0x95U
#define PICC_CL3	  0x97U
#define PICC_SELECT_NVB	  0x70U

#define TRANSCEIVE_TIMEOUT_MS 200

struct mfrc522_data {
	uint8_t uid_bytes[CONFIG_MFRC522_UID_MAX_LEN];
	uint8_t uid_len;
	uint8_t sak;
	bool	card_present;
};

struct mfrc522_config {
	struct spi_dt_spec  spi;
	struct gpio_dt_spec reset;
};


static const struct spi_dt_spec *cfg_spi(const struct device *dev)
{
	return &((const struct mfrc522_config *)dev->config)->spi;
}

static int reg_write(const struct device *dev, uint8_t addr, uint8_t val)
{
	uint8_t tx[2] = {(addr << 1) & 0x7EU, val};
	struct spi_buf	   tx_buf = {.buf = tx, .len = sizeof(tx)};
	struct spi_buf_set tx_set = {.buffers = &tx_buf, .count = 1U};

	return spi_write_dt(cfg_spi(dev), &tx_set);
}

static int reg_read(const struct device *dev, uint8_t addr, uint8_t *val)
{
	uint8_t tx[2] = {((addr << 1) & 0x7EU) | 0x80U, 0x00U};
	uint8_t rx[2] = {0};
	struct spi_buf	   tx_buf = {.buf = tx, .len = sizeof(tx)};
	struct spi_buf	   rx_buf = {.buf = rx, .len = sizeof(rx)};
	struct spi_buf_set tx_set = {.buffers = &tx_buf, .count = 1U};
	struct spi_buf_set rx_set = {.buffers = &rx_buf, .count = 1U};
	int ret;

	ret = spi_transceive_dt(cfg_spi(dev), &tx_set, &rx_set);
	if (ret == 0) {
		LOG_DBG("reg_read: addr=0x%02X, val=0x%02X", addr, rx[1]);
		*val = rx[1];
	}
	return ret;
}


/*
 * Send tx_data, receive into rx_data via CMD_TRANSCEIVE.
 * last_bits: number of bits in last TX byte (0 = full byte, 7 = REQA).
 * Returns byte count received, or negative errno.
 */
static int transceive(const struct device *dev, const uint8_t *tx_data, uint8_t tx_len,
		      uint8_t *rx_data, uint8_t rx_max, uint8_t last_bits)
{
	uint8_t irq, err, n;
	int ret;
	int timeout = TRANSCEIVE_TIMEOUT_MS;

	ret = reg_write(dev, REG_COMMAND, CMD_IDLE);
	if (ret != 0) {
		LOG_DBG("transceive: reg_write(CMD_IDLE) failed: %d", ret);
		return ret;
	}
	ret = reg_write(dev, REG_COM_IRQ, 0x7FU); /* clear all IRQ flags */
	if (ret != 0) {
		LOG_DBG("transceive: reg_write(REG_COM_IRQ) failed: %d", ret);
		return ret;
	}
	ret = reg_write(dev, REG_FIFO_LEVEL, 0x80U); /* flush FIFO */
	if (ret != 0) {
		LOG_DBG("transceive: reg_write(REG_FIFO_LEVEL) failed: %d", ret);
		return ret;
	}

	for (uint8_t i = 0; i < tx_len; i++) {
		ret = reg_write(dev, REG_FIFO_DATA, tx_data[i]);
		if (ret != 0) {
			LOG_DBG("transceive: reg_write(REG_FIFO_DATA) failed: %d", ret);
			return ret;
		}
	}

	ret = reg_write(dev, REG_BIT_FRAMING, last_bits & 0x07U);
	if (ret != 0) {
		LOG_DBG("transceive: reg_write(REG_BIT_FRAMING) failed: %d", ret);
		return ret;
	}
	ret = reg_write(dev, REG_COMMAND, CMD_TRANSCEIVE);
	if (ret != 0) {
		LOG_DBG("transceive: reg_write(REG_COMMAND) failed: %d", ret);
		return ret;
	}
	/* StartSend triggers transmission */
	ret = reg_write(dev, REG_BIT_FRAMING, (last_bits & 0x07U) | 0x80U);
	if (ret != 0) {
		LOG_DBG("transceive: reg_write(REG_BIT_FRAMING) failed: %d", ret);
		return ret;
	}

	/* Poll: RxIRq(5) | IdleIRq(4) | TimerIRq(0) */
	do {
		ret = reg_read(dev, REG_COM_IRQ, &irq);
		if (ret != 0) {
			LOG_DBG("transceive: reg_read(REG_COM_IRQ) failed: %d", ret);
			return ret;
		}
		k_msleep(1);
		timeout--;
	} while (!(irq & 0x31U) && timeout > 0);

	/* Stop the circular Transceive command before touching the FIFO */
	reg_write(dev, REG_COMMAND, CMD_IDLE);

	if (timeout == 0 || (irq & 0x01U)) {
		LOG_DBG("transceive: timed out");
		return -ETIMEDOUT;
	}

	ret = reg_read(dev, REG_ERROR, &err);
	if (ret != 0) {
		LOG_DBG("transceive: reg_read(REG_ERROR) failed: %d", ret);
		return ret;
	}
	if (err & 0x1FU) { /* BufferOvfl | CollErr | CRCErr | ParityErr | ProtocolErr */
		LOG_DBG("transceive: error detected");
		return -EIO;
	}

	ret = reg_read(dev, REG_FIFO_LEVEL, &n);
	if (ret != 0) {
		LOG_DBG("transceive: reg_read(REG_FIFO_LEVEL) failed: %d", ret);
		return ret;
	}

	uint8_t to_read = (n < rx_max) ? n : rx_max;

	for (uint8_t i = 0; i < to_read; i++) {
		ret = reg_read(dev, REG_FIFO_DATA, &rx_data[i]);
		if (ret != 0) {
			LOG_DBG("transceive: reg_read(REG_FIFO_DATA) failed: %d", ret);
			return ret;
		}
	}

	return (int)to_read;
}

static int calc_crc(const struct device *dev, const uint8_t *data, uint8_t len,
		    uint8_t *crc_l, uint8_t *crc_h)
{
	uint8_t irq;
	int ret;
	int timeout = TRANSCEIVE_TIMEOUT_MS;

	ret = reg_write(dev, REG_COMMAND, CMD_IDLE);
	if (ret != 0) {
		LOG_DBG("calc_crc: reg_write(CMD_IDLE) failed: %d", ret);
		return ret;
	}
	ret = reg_write(dev, REG_DIV_IRQ, 0x04U); /* clear CRCIRq */
	if (ret != 0) {
		LOG_DBG("calc_crc: reg_write(REG_DIV_IRQ) failed: %d", ret);
		return ret;
	}
	ret = reg_write(dev, REG_FIFO_LEVEL, 0x80U); /* flush FIFO */
	if (ret != 0) {
		LOG_DBG("calc_crc: reg_write(REG_FIFO_LEVEL) failed: %d", ret);
		return ret;
	}

	for (uint8_t i = 0; i < len; i++) {
		ret = reg_write(dev, REG_FIFO_DATA, data[i]);
		if (ret != 0) {
			LOG_DBG("calc_crc: reg_write(REG_FIFO_DATA) failed: %d", ret);
			return ret;
		}
	}

	ret = reg_write(dev, REG_COMMAND, CMD_CALC_CRC);
	if (ret != 0) {
		LOG_DBG("calc_crc: reg_write(REG_COMMAND) failed: %d", ret);
		return ret;
	}

	do {
		ret = reg_read(dev, REG_DIV_IRQ, &irq);
		if (ret != 0) {
			LOG_DBG("calc_crc: reg_read(REG_DIV_IRQ) failed: %d", ret);
			return ret;
		}
		k_msleep(1);
		timeout--;
	} while (!(irq & 0x04U) && timeout > 0);

	if (timeout == 0) {
		LOG_DBG("calc_crc: timed out");
		return -ETIMEDOUT;
	}

	ret = reg_write(dev, REG_COMMAND, CMD_IDLE);
	if (ret != 0) {
		LOG_DBG("calc_crc: reg_write(CMD_IDLE) failed: %d", ret);
		return ret;
	}
	ret = reg_read(dev, REG_CRC_RESULT_L, crc_l);
	if (ret != 0) {
		LOG_DBG("calc_crc: reg_read(REG_CRC_RESULT_L) failed: %d", ret);
		return ret;
	}
	ret = reg_read(dev, REG_CRC_RESULT_M, crc_h);
	if (ret != 0) {
		LOG_DBG("calc_crc: reg_read(REG_CRC_RESULT_M) failed: %d", ret);
		return ret;
	}

	return 0;
}


/* Returns true if WUPA yields a valid ATQA response (detects both IDLE and HALTed cards). */
static bool chip_card_present(const struct device *dev)
{
	uint8_t rx[2];
	uint8_t req = PICC_WUPA;

	return transceive(dev, &req, 1U, rx, sizeof(rx), 7U) == 2;
}

static int chip_read_uid(const struct device *dev, uint8_t *uid_bytes, uint8_t *uid_len,
			 uint8_t *sak_out)
{
	uint8_t cl = PICC_CL1;
	uint8_t out_uid[CONFIG_MFRC522_UID_MAX_LEN];
	uint8_t out_len = 0U;
	int ret;

	for (uint8_t cascade = 0U; cascade < 3U; cascade++) {
		uint8_t rx[5];
		uint8_t anticoll[2] = {cl, 0x20U};

		ret = transceive(dev, anticoll, sizeof(anticoll), rx, sizeof(rx), 0U);
		if (ret < 5) {
			LOG_DBG("chip_read_uid: transceive failed: %d", ret);
			return (ret < 0) ? ret : -EIO;
		}

		/* BCC check */
		if ((rx[0] ^ rx[1] ^ rx[2] ^ rx[3]) != rx[4]) {
			LOG_DBG("chip_read_uid: BCC check failed");
			return -EBADMSG;
		}

		/* SELECT: [cl, NVB, uid[0..3], BCC, CRC_L, CRC_H] */
		uint8_t buf[9];
		uint8_t crc_l, crc_h;

		buf[0] = cl;
		buf[1] = PICC_SELECT_NVB;
		memcpy(&buf[2], rx, 5U);

		ret = calc_crc(dev, buf, 7U, &crc_l, &crc_h);
		if (ret != 0) {
			LOG_DBG("chip_read_uid: calc_crc failed: %d", ret);
			return ret;
		}
		buf[7] = crc_l;
		buf[8] = crc_h;

		uint8_t sel_rx[3];

		ret = transceive(dev, buf, 9U, sel_rx, sizeof(sel_rx), 0U);
		if (ret < 1) {
			LOG_DBG("chip_read_uid: transceive failed: %d", ret);
			return (ret < 0) ? ret : -EIO;
		}
		*sak_out = sel_rx[0];

		if (rx[0] == PICC_CT) {
			/* CT byte marks cascade; skip it, copy 3 real UID bytes */
			memcpy(&out_uid[out_len], &rx[1], 3U);
			out_len += 3U;
		} else {
			memcpy(&out_uid[out_len], rx, 4U);
			out_len += 4U;
		}

		if (!(*sak_out & 0x04U)) {
			break; /* no further cascade levels */
		}

		if (cl == PICC_CL1) {
			cl = PICC_CL2;
		} else {
			cl = PICC_CL3;
		}
	}

	/* HLTA — card will not respond; ignore timeout */
	uint8_t hlta[4];
	uint8_t crc_l, crc_h;
	uint8_t dummy[2];

	hlta[0] = PICC_HLTA;
	hlta[1] = 0x00U;
	calc_crc(dev, hlta, 2U, &crc_l, &crc_h);
	hlta[2] = crc_l;
	hlta[3] = crc_h;
	transceive(dev, hlta, sizeof(hlta), dummy, sizeof(dummy), 0U);

	memcpy(uid_bytes, out_uid, out_len);
	*uid_len = out_len;
	return 0;
}


static int mfrc522_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	struct mfrc522_data *data = dev->data;

	ARG_UNUSED(chan);

	data->card_present = chip_card_present(dev);
	if (!data->card_present) {
		LOG_DBG("mfrc522_sample_fetch: no card present");
		return 0;
	}
	return chip_read_uid(dev, data->uid_bytes, &data->uid_len, &data->sak);
}

static int mfrc522_channel_get(const struct device *dev, enum sensor_channel chan,
				struct sensor_value *val)
{
	const struct mfrc522_data *data = dev->data;

	switch ((enum sensor_channel_mfrc522)chan) {
	case SENSOR_CHAN_MFRC522_PRESENT:
		val->val1 = data->card_present ? 1 : 0;
		val->val2 = 0;
		return 0;
	case SENSOR_CHAN_MFRC522_UID:
		val->val1 = data->uid_len;
		if (data->uid_len > sizeof(val->val2)) {
			LOG_WRN("UID %u bytes truncated to %zu in val2",
				data->uid_len, sizeof(val->val2));
		}
		memcpy(&val->val2, data->uid_bytes, MIN(data->uid_len, sizeof(val->val2)));
		return 0;
	case SENSOR_CHAN_MFRC522_SAK:
		val->val1 = data->sak;
		val->val2 = 0;
		return 0;
	default:
		return -ENOTSUP;
	}
}

static const struct sensor_driver_api mfrc522_api = {
	.sample_fetch = mfrc522_sample_fetch,
	.channel_get  = mfrc522_channel_get,
};


static int antenna_on(const struct device *dev)
{
	uint8_t val;
	int ret;

	ret = reg_read(dev, REG_TX_CONTROL, &val);
	if (ret != 0) {
		LOG_DBG("antenna_on: reg_read(REG_TX_CONTROL) failed: %d", ret);
		return ret;
	}
	return reg_write(dev, REG_TX_CONTROL, val | 0x03U);
}

static int mfrc522_init(const struct device *dev)
{
	const struct mfrc522_config *cfg = dev->config;
	int ret;

	if (!spi_is_ready_dt(&cfg->spi)) {
		LOG_ERR("SPI bus not ready");
		return -ENODEV;
	}
	if (!gpio_is_ready_dt(&cfg->reset)) {
		LOG_ERR("Reset GPIO not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&cfg->reset, GPIO_OUTPUT_ACTIVE);
	if (ret != 0) {
		LOG_DBG("mfrc522_init: gpio_pin_configure_dt failed: %d", ret);
		return ret;
	}

	/* Hardware reset: assert RST low, then release high */
	ret = gpio_pin_set_dt(&cfg->reset, 1);
	if (ret != 0) {
		LOG_DBG("mfrc522_init: gpio_pin_set_dt failed: %d", ret);
		return ret;
	}
	k_msleep(50);
	ret = gpio_pin_set_dt(&cfg->reset, 0);
	if (ret != 0) {
		LOG_DBG("mfrc522_init: gpio_pin_set_dt failed: %d", ret);
		return ret;
	}
	k_msleep(50);

	ret = reg_write(dev, REG_COMMAND, CMD_SOFT_RESET);
	if (ret != 0) {
		LOG_DBG("mfrc522_init: reg_write failed: %d", ret);
		return ret;
	}
	k_msleep(50);

	ret = reg_write(dev, REG_T_MODE, 0x80U); /* TAuto = 1 */
	if (ret != 0) {
		LOG_DBG("mfrc522_init: reg_write failed: %d", ret);
		return ret;
	}
	ret = reg_write(dev, REG_T_PRESCALER, 0xA9U);
	if (ret != 0) {
		LOG_DBG("mfrc522_init: reg_write failed: %d", ret);
		return ret;
	}
	ret = reg_write(dev, REG_T_RELOAD_H, 0x03U);
	if (ret != 0) {
		LOG_DBG("mfrc522_init: reg_write failed: %d", ret);
		return ret;
	}
	ret = reg_write(dev, REG_T_RELOAD_L, 0xE8U);
	if (ret != 0) {
		LOG_DBG("mfrc522_init: reg_write failed: %d", ret);
		return ret;
	}
	ret = reg_write(dev, REG_TX_ASK, 0x40U); /* 100% ASK */
	if (ret != 0) {
		LOG_DBG("mfrc522_init: reg_write failed: %d", ret);
		return ret;
	}
	ret = reg_write(dev, REG_MODE, 0x3DU); /* CRC preset 0x6363 */
	if (ret != 0) {
		LOG_DBG("mfrc522_init: reg_write failed: %d", ret);
		return ret;
	}

	ret = antenna_on(dev);
	if (ret != 0) {
		LOG_DBG("mfrc522_init: antenna_on failed: %d", ret);
		return ret;
	}

	uint8_t ver;

	ret = reg_read(dev, REG_VERSION, &ver);
	if (ret != 0) {
		LOG_ERR("mfrc522_init: failed to read VersionReg: %d", ret);
		return ret;
	}
	if (ver == 0x00U || ver == 0xFFU) {
		LOG_ERR("mfrc522_init: VersionReg 0x%02X — no SPI communication", ver);
		return -ENODEV;
	}
	if (ver != 0x91U && ver != 0x92U) {
		LOG_WRN("mfrc522_init: clone IC detected (VersionReg=0x%02X), proceeding", ver);
	} else {
		LOG_INF("mfrc522_init: NXP IC v%u.0 detected", ver & 0x0FU);
	}

	LOG_INF("MFRC522 initialized");
	return 0;
}


#define MFRC522_DEFINE(inst)                                                               \
	static struct mfrc522_data mfrc522_data_##inst;                                    \
	static const struct mfrc522_config mfrc522_cfg_##inst = {                          \
		.spi   = SPI_DT_SPEC_INST_GET(inst, SPI_OP_MODE_MASTER | SPI_WORD_SET(8), \
					       0U),                                        \
		.reset = GPIO_DT_SPEC_INST_GET(inst, reset_gpios),                         \
	};                                                                                 \
	DEVICE_DT_INST_DEFINE(inst, mfrc522_init, NULL, &mfrc522_data_##inst,              \
			      &mfrc522_cfg_##inst, POST_KERNEL,                            \
			      CONFIG_SENSOR_INIT_PRIORITY, &mfrc522_api);

DT_INST_FOREACH_STATUS_OKAY(MFRC522_DEFINE)
