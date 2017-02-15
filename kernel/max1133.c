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
#include <linux/iio/buffer-dma.h>

#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#include <linux/dma-mapping.h>
#include <linux/workqueue.h>

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

struct max1133_dma {
	/* IRQ routine for the dummy DMA */
	void (*irq_fn)(unsigned int, void *);

	/* Pending DMA transfers */
	struct mutex transfer_list_lock;
	struct list_head transfer_list;

	/* Used to emulate periodic completion of DMA transfers */
	struct delayed_work work;

    struct iio_dev *indio_dev;
};

struct max1133_dma_buffer {
	/* Generic IIO DMA buffer base struct */
	struct iio_dma_buffer_queue queue;

	/* Handle to the "DMA" controller */
	struct max1133_dma dma;

	/* List of submitted blocks */
	struct list_head block_list;
};

static struct max1133_dma_buffer *iio_buffer_to_max1133_dma_buffer(
	struct iio_buffer *buffer)
{
	return container_of(buffer, struct max1133_dma_buffer, queue.buffer);
}

struct max1133_dma_transfer {
	struct list_head head;

	/* Memory address and length of the transfer */
	void *addr;
	unsigned int length;

	/* Data passed to the IRQ callback when the transfer completes */
	void *irq_data;
};

/*
 * This function is called when all references to the buffer have been
 * dropped should free any memory or other resources associated with the
 * buffer.
 */
static void max1133_dma_buffer_release(struct iio_buffer *buf)
{
	struct max1133_dma_buffer *buffer =
		iio_buffer_to_max1133_dma_buffer(buf);

	/*
	 * iio_dma_buffer_release() must be called right before freeing the
	 * memory.
	 */
	iio_dma_buffer_release(&buffer->queue);
	kfree(buffer);
}

/*
 * Most drivers will be able to use the default DMA buffer callbacks. But if
 * necessary it is possible to overwrite certain functions with custom
 * implementations. One exception is the release callback, which always needs to
 * be implemented.
 */
static const struct iio_buffer_access_funcs max1133_dma_buffer_ops = {
	.read_first_n = iio_dma_buffer_read,
	.set_bytes_per_datum = iio_dma_buffer_set_bytes_per_datum,
	.set_length = iio_dma_buffer_set_length,
	.request_update = iio_dma_buffer_request_update,
	.enable = iio_dma_buffer_enable,
	.disable = iio_dma_buffer_disable,
	.data_available = iio_dma_buffer_data_available,
	.release = max1133_dma_buffer_release,

	.modes = INDIO_BUFFER_HARDWARE,
	.flags = INDIO_BUFFER_FLAG_FIXED_WATERMARK,
};

static void max1133_dma_schedule_next(struct max1133_dma *dma)
{
	struct max1133_dma_transfer *transfer;
	unsigned int num_samples;

	if (list_empty(&dma->transfer_list))
		return;

	transfer = list_first_entry(&dma->transfer_list,
		struct max1133_dma_transfer, head);

	num_samples = transfer->length / sizeof(uint16_t);

	/* 10000 SPS */
	schedule_delayed_work(&dma->work, msecs_to_jiffies(num_samples / 10));
}

static int max1133_dma_issue_transfer(struct max1133_dma *dma, void *addr,
	unsigned int length, void *irq_data)
{
	struct max1133_dma_transfer *transfer;

	transfer = kzalloc(sizeof(*transfer), GFP_KERNEL);
	if (!transfer)
		return -ENOMEM;

	transfer->addr = addr;
	transfer->length = length;
	transfer->irq_data = irq_data;

	mutex_lock(&dma->transfer_list_lock);
	list_add_tail(&transfer->head, &dma->transfer_list);

	/* Start "DMA" transfer */
	max1133_dma_schedule_next(dma);
	mutex_unlock(&dma->transfer_list_lock);

	return 0;
}

static int max1133_dma_buffer_submit(struct iio_dma_buffer_queue *queue,
	struct iio_dma_buffer_block *block)
{
	struct max1133_dma_buffer *buffer =
		iio_buffer_to_max1133_dma_buffer(&queue->buffer);
	unsigned long flags;
	int ret;

	/*
	 * submit() is called when the buffer is active and a block becomes
	 * available. It should start a DMA transfer for the submitted block as
	 * soon as possible. submit() can be called even when a DMA transfer is
	 * already active. This gives the driver to prepare and setup the next
	 * transfer to allow a seamless switch to the next block without losing
	 * any samples.
	 */

	spin_lock_irqsave(&queue->list_lock, flags);
	list_add(&block->head, &buffer->block_list);
	spin_unlock_irqrestore(&queue->list_lock, flags);

	ret = max1133_dma_issue_transfer(&buffer->dma, block->vaddr,
		block->size, block);
	if (ret) {
		spin_lock_irqsave(&queue->list_lock, flags);
		list_del(&block->head);
		spin_unlock_irqrestore(&queue->list_lock, flags);
		return ret;
	}

	return 0;
}

static void max1133_dma_buffer_abort(struct iio_dma_buffer_queue *queue)
{
	struct max1133_dma_buffer *buffer =
		iio_buffer_to_max1133_dma_buffer(&queue->buffer);

	/*
	 * When abort() is called is is guaranteed that that submit() is not
	 * called again until abort() has completed. This means no new blocks
	 * will be added to the list. Once the pending DMA transfers are
	 * canceled no blocks will be removed either. So it is save to release
	 * the uncompleted blocks still on the list.
	 *
	 * If a DMA does not support aborting transfers it is OK to keep the
	 * currently active transfers running. In that case the blocks
	 * associated with the transfer must not be marked as done until they
	 * are completed.  Otherwise their memory might be freed while the DMA
	 * transfer is still in progress.
	 *
	 * Special care needs to be taken if the DMA controller does not
	 * support aborting transfers but the converter will stop sending
	 * samples once disabled. In this case the DMA might get stuck until the
	 * converter is re-enabled.
	 */
    mutex_lock(&buffer->dma.transfer_list_lock);
	cancel_delayed_work(&buffer->dma.work);
	INIT_LIST_HEAD(&buffer->dma.transfer_list);
	mutex_unlock(&buffer->dma.transfer_list_lock);

	/*
	 * None of the blocks are any longer in use at this point, give them
	 * back.
	 */
	iio_dma_buffer_block_list_abort(queue, &buffer->block_list);
}

static const struct iio_dma_buffer_ops max1133_dma_buffer_dma_ops = {
	.submit = max1133_dma_buffer_submit,
	.abort = max1133_dma_buffer_abort,
};

static void max1133_dma_work(struct work_struct *work)
{
	struct max1133_dma *dma = container_of(work, struct max1133_dma,
		work.work);
	struct max1133_dma_transfer *transfer;

	/* Get the next pending transfer and then fill it with data. */
	mutex_lock(&dma->transfer_list_lock);
	transfer = list_first_entry(&dma->transfer_list,
		struct max1133_dma_transfer, head);
	list_del(&transfer->head);
	max1133_dma_schedule_next(dma);
	mutex_unlock(&dma->transfer_list_lock);

	/*
	 * For real hardware copying of the data will be done by the DMA in the
	 * background. Here it is done in software.
	 */
	max1133_adc_conversion(dma->indio_dev, transfer->addr);

	/* Generate "interrupt" */
	dma->irq_fn(sizeof(uint16_t), transfer->irq_data);
	kfree(transfer);
}

static void max1133_dma_buffer_irq(unsigned int bytes_transferred,
	void *data)
{
	struct iio_dma_buffer_block *block = data;
	struct iio_dma_buffer_queue *queue = block->queue;
	unsigned long flags;

	/* Protect against races with submit() */
	spin_lock_irqsave(&queue->list_lock, flags);
	list_del(&block->head);
	spin_unlock_irqrestore(&queue->list_lock, flags);

	/*
	 * Update actual number of bytes transferred. This might be less than
	 * the requested number, e.g. due to alignment requirements of the
	 * controller, but must be a multiple of the sample size.
	 */
	block->bytes_used = bytes_transferred;

	/*
	 * iio_dma_buffer_block_done() must be called after the DMA transfer for
	 * the block that has been completed. This will typically be done from
	 * some kind of completion interrupt routine or callback.
	 */
	iio_dma_buffer_block_done(block);
}

int max1133_configure_buffer(struct iio_dev *indio_dev)
{
	struct max1133_dma_buffer *buffer;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	/*
	 * Setup DMA controller. For real hardware this should acquire and setup
	 * all resources that are necessary to operate the DMA controller, like
	 * IRQs, clocks, IO mem regions, etc.
	 */
    INIT_LIST_HEAD(&buffer->dma.transfer_list);
	mutex_init(&buffer->dma.transfer_list_lock);
	INIT_DELAYED_WORK(&buffer->dma.work, max1133_dma_work);
	buffer->dma.irq_fn = max1133_dma_buffer_irq;

    buffer->dma.indio_dev = indio_dev;

	/*
	 * For a real device the device passed to iio_dma_buffer_init() must be
	 * the device that performs the DMA transfers. Often this is not the
	 * device for the converter, but a dedicated DMA controller.
	 */
	dma_coerce_mask_and_coherent(&indio_dev->dev, DMA_BIT_MASK(32));
	iio_dma_buffer_init(&buffer->queue, &indio_dev->dev,
		&max1133_dma_buffer_dma_ops);
	buffer->queue.buffer.access = &max1133_dma_buffer_ops;

	INIT_LIST_HEAD(&buffer->block_list);

	indio_dev->buffer = &buffer->queue.buffer;
	indio_dev->modes |= INDIO_BUFFER_HARDWARE;

	return 0;
}

/**
 * max1133_dma_unconfigure_buffer() - release buffer resources
 * @indio_dev: device instance state
 */
void max1133_unconfigure_buffer(struct iio_dev *indio_dev)
{
	struct max1133_dma_buffer *buffer =
		iio_buffer_to_max1133_dma_buffer(indio_dev->buffer);

	/*
	 * Once iio_dma_buffer_exit() has been called none of the DMA buffer
	 * callbacks will be called. This means it is save to free any resources
	 * that are only used in those callbacks at this point. The memory for
	 * the buffer struct must not be freed since it might be still in use
	 * elsewhere. It will be freed in the buffers release callback.
	 */
	iio_dma_buffer_exit(&buffer->queue);

	/*
	 * Drop our reference to the buffer. Since this might be the last one
	 * the buffer structure must no longer be accessed after this.
	 */
	iio_buffer_put(&buffer->queue.buffer);
}

/**
 * Update active channels
 * @indio_dev: The IIO device
 * @scan_mask: Scan mask with the new active channels
 */
int max1133_update_scan_mode(struct iio_dev *indio_dev,
	const unsigned long *scan_mask)
{
	return 0;
}

static const struct iio_info max1133_info = {
	.read_raw = &max1133_read_raw,
    .update_scan_mode = &max1133_update_scan_mode,
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

    ret = max1133_configure_buffer (indio_dev);
    if (ret) {
		dev_err(&indio_dev->dev, "Failed to setup buffer\n");
		goto error;
	}

	ret = iio_device_register(indio_dev);
	if (ret)
		goto error_buffer;

	return 0;

error_buffer:
    max1133_unconfigure_buffer(indio_dev);
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

    max1133_unconfigure_buffer(indio_dev);

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
