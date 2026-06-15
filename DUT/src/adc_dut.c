#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include "adc_dut.h"

LOG_MODULE_REGISTER(adc_dut, LOG_LEVEL_INF);

static const struct adc_dt_spec adc_chan = ADC_DT_SPEC_GET(DT_ALIAS(adc_thermistor));

void adc_dut_start(void)
{
    if (!adc_is_ready_dt(&adc_chan)) {
        LOG_ERR("ADC not ready");
        return;
    }
    adc_channel_setup_dt(&adc_chan);
}

uint16_t adc_dut_read_mv(void)
{
    int16_t buf;
    struct adc_sequence seq = {
        .buffer = &buf,
        .buffer_size = sizeof(buf),
    };
    adc_sequence_init_dt(&adc_chan, &seq);
    int err = adc_read_dt(&adc_chan, &seq);
    if (err) return 0;
    
    int32_t val_mv = buf;
    adc_raw_to_millivolts_dt(&adc_chan, &val_mv);
    return val_mv;
}
