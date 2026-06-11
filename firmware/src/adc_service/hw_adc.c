#include <errno.h>

#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>

#include "hardware.h"

LOG_MODULE_REGISTER(hw_adc, LOG_LEVEL_INF);

#define ADC_RESOLUTION_BITS 12

static const struct device *adc_dev = DEVICE_DT_GET(DT_ALIAS(hil_adc));

/* Tracks channels that have already been configured. */
static uint32_t configured_channels_mask;

static int validate_channel_selection(int channel)
{
    switch (channel) {
    case 0: case 1: case 4: case 5: 
    case 6: case 7: case 8: case 9:
        return 0;

    default:
        return -EINVAL;
    }
}

int handle_adc_sample_channel(int channel)
{
    int ret;
    int16_t sample;

    ret = validate_channel_selection(channel);
    if (ret < 0) {
        LOG_ERR("ADC channel %d is not supported", channel);
        return ret;
    }

    /* Lazy channel setup: configure only on first use. */
    if (!(configured_channels_mask & BIT(channel))) {

        struct adc_channel_cfg ch_cfg = {
            .gain = ADC_GAIN_1,
            .reference = ADC_REF_INTERNAL,
            .acquisition_time = ADC_ACQ_TIME_DEFAULT,
            .channel_id = channel,
            .differential = 0,
        };

        ret = adc_channel_setup(adc_dev, &ch_cfg);
        if (ret < 0) {
            LOG_ERR("Failed to configure ADC channel %d (err %d)",
                    channel, ret);
            return ret;
        }

        configured_channels_mask |= BIT(channel);

        LOG_INF("ADC channel %d configured", channel);
    }

    struct adc_sequence seq = {
        .channels = BIT(channel),
        .buffer = &sample,
        .buffer_size = sizeof(sample),
        .resolution = ADC_RESOLUTION_BITS,
    };

    ret = adc_read(adc_dev, &seq);
    if (ret < 0) {
        LOG_ERR("ADC read failed on channel %d (err %d)",
                channel, ret);
        return ret;
    }

    return (int)sample;
}

int verify_adc_ready(void)
{
    if (!adc_dev || !device_is_ready(adc_dev)) { LOG_ERR("ADC device not ready"); return -ENODEV; }
    return 0;
}