/*
 * MAX1133 SPI ADC driver
 *
 * Copyright 2016 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GPL-2.
 */

#include <linux/err.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>

#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#define MAX1133_CB_START        BIT(7)	/* control byte start */
#define MAX1133_CB_UNIPOLAR	    BIT(6)	/* set for unipolar mode */
#define MAX1133_CB_INT_CLOCK    BIT(5)	/* internal clock */


enum {
	MAX1132,
	MAX1133,
};

#define MAX1133_VOLTAGE_CHANNEL(num)				\
	{							\
		.type = IIO_VOLTAGE,				\
		.indexed = 1,					\
		.channel = (num),				\
		.address = (num),				\
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),	\
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE), \
		.scan_index = (num), \
		.scan_type = {						\
			.sign = 's',					\
			.realbits = 16,				\
			.storagebits = 16,		         \
			.endianness = IIO_BE,				\
		} \
	}

static const struct iio_chan_spec max1132_channels[] = {
    MAX1133_VOLTAGE_CHANNEL(0),
};

static const struct iio_chan_spec max1133_channels[] = {
    MAX1133_VOLTAGE_CHANNEL(0),
};

struct max1133_chip_info {
	const struct iio_chan_spec *channels;
	unsigned int num_channels;
	unsigned int resolution;
};

struct max1133 {
	struct spi_device *spi;
    struct spi_message msg;
	struct spi_transfer xfer[2];

    struct regulator *reg;
	struct mutex lock;
	const struct max1133_chip_info *chip_info;

    u8 tx_buf ____cacheline_aligned;

    /* Buffer needs to be large enough to hold one 16 bit sample and a
	 * 64 bit aligned 64 bit timestamp.
	 */
	u8 data[ALIGN(4, sizeof(s64)) + sizeof(s16)];
};


static int max1133_adc_conversion(struct iio_dev *indio_dev, int *val)
{
    struct max1133 *adc = iio_priv(indio_dev);
	int ret;

    ret = spi_sync(adc->spi, &adc->msg);
    if (ret < 0)
		return ret;

    printk(KERN_INFO "MAX1133: New data[0]: %i", adc->data[0]);
    printk(KERN_INFO "MAX1133: New data[1]: %i", adc->data[1]);

    *val = (s16) ((adc->data[0] << 8) | adc->data[1]);
    return IIO_VAL_INT;
}

static int max1133_read_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *channel, int *val,
			    int *val2, long mask)
{
	struct max1133 *adc = iio_priv(indio_dev);
	int ret = -EINVAL;

	mutex_lock(&adc->lock);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
        if (iio_buffer_enabled(indio_dev)) {
			ret = -EBUSY;
            goto out;
        }

		ret = max1133_adc_conversion(indio_dev, val);

        printk(KERN_INFO "MAX1133: New data: %i", *val);

		if (ret < 0)
			goto out;
		break;

    case IIO_CHAN_INFO_SCALE:
		ret = regulator_get_voltage(adc->reg);
        if (ret < 0)
			goto out;

		/* convert regulator output voltage to mV */
		*val = ret / 1000;
		*val2 = adc->chip_info->resolution;
		ret = IIO_VAL_FRACTIONAL_LOG2;
		break;
	}

out:
	mutex_unlock(&adc->lock);

	return ret;
}

static const struct iio_info max1133_info = {
	.read_raw = max1133_read_raw,
	.driver_module = THIS_MODULE,
};

static const struct max1133_chip_info max1133_chip_infos[] = {
	[MAX1132] = {
		.channels = max1132_channels,
		.num_channels = ARRAY_SIZE(max1132_channels),
		.resolution = 16
	},
	[MAX1133] = {
		.channels = max1133_channels,
		.num_channels = ARRAY_SIZE(max1133_channels),
		.resolution = 16
	},
};

static irqreturn_t max1133_trigger_h(int irq, void *p)
 {
     struct iio_poll_func *pf = p;
     struct iio_dev *indio_dev = pf->indio_dev;
     struct max1133 *adc = iio_priv(indio_dev);
     int ret;

     printk(KERN_INFO "MAX1133: Trigger call.");

     ret = spi_sync(adc->spi, &adc->msg);
    if (ret < 0)
		goto done;

     iio_push_to_buffers_with_timestamp(indio_dev, adc->data,
                                        iio_get_time_ns());

done:
     iio_trigger_notify_done(indio_dev->trig);

     return IRQ_HANDLED;
}

static int max1133_probe(struct spi_device *spi)
{
	struct iio_dev *indio_dev;
	struct max1133 *adc;
	const struct max1133_chip_info *chip_info;
	int ret;

	indio_dev = devm_iio_device_alloc(&spi->dev, sizeof(*adc));
	if (!indio_dev)
		return -ENOMEM;

	adc = iio_priv(indio_dev);

    spi_set_drvdata(spi, indio_dev);
	adc->spi = spi;

    printk(KERN_INFO "Probing MAX1133 (SPI CS: %i)", adc->spi->chip_select);

	indio_dev->dev.parent = &spi->dev;
	indio_dev->name = spi_get_device_id(spi)->name;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &max1133_info;

	chip_info = &max1133_chip_infos[spi_get_device_id(spi)->driver_data];
	indio_dev->channels = chip_info->channels;
	indio_dev->num_channels = chip_info->num_channels;

	adc->chip_info = chip_info;

    adc->tx_buf = MAX1133_CB_START;

    adc->xfer[0].tx_buf = &adc->tx_buf;
	adc->xfer[0].len = sizeof(adc->tx_buf);
	adc->xfer[1].rx_buf = adc->data;
	adc->xfer[1].len = sizeof(adc->data);

	spi_message_init_with_transfers(&adc->msg, adc->xfer,
					ARRAY_SIZE(adc->xfer));

    adc->reg = devm_regulator_get(&spi->dev, "vref");
	if (IS_ERR(adc->reg))
		return PTR_ERR(adc->reg);

	ret = regulator_enable(adc->reg);
	if (ret < 0)
		return ret;

    mutex_init(&adc->lock);

    ret = iio_triggered_buffer_setup(indio_dev, &iio_pollfunc_store_time,
		&max1133_trigger_h, NULL);
    if (ret) {
		dev_err(&indio_dev->dev, "Failed to setup buffer\n");
		goto error;
	}

	ret = iio_device_register(indio_dev);
	if (ret)
		goto error_ring;

	return 0;

error_ring:
    iio_triggered_buffer_cleanup(indio_dev);
error:
	regulator_disable(adc->reg);
    return ret;
}

static int max1133_remove(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);
    struct max1133 *adc = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);
    regulator_disable(adc->reg);

    iio_triggered_buffer_cleanup(indio_dev);

	return 0;
}

static const struct spi_device_id max1133_id[] = {
	{ "MAX1132", MAX1132 },
	{ "MAX1133", MAX1133 },
	{ }
};
MODULE_DEVICE_TABLE(spi, max1133_id);

static struct spi_driver max1133_driver = {
	.driver = {
		.name = "max1133",
	},
	.probe = max1133_probe,
	.remove = max1133_remove,
	.id_table = max1133_id,
};
module_spi_driver(max1133_driver);

MODULE_AUTHOR("Matthias Klumpp <matthias@tenstral.net>");
MODULE_DESCRIPTION("Maxim MAX1132/MAX1133 ADC");
MODULE_LICENSE("GPL v2");
