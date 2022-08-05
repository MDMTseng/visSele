#include "EncoderCounter.hpp"
#include <soc/pcnt_struct.h>
#include "esp_log.h"




#define PCNT_UNIT           PCNT_UNIT_0
#define PCNT_H_LIM_VAL      30000
#define PCNT_L_LIM_VAL     -30000
#define PCNT_THRESH1_VAL    5
#define PCNT_THRESH0_VAL   -5

#define PCNT_INPUT_IO_A     17  // Pulse Input GPIO
#define PCNT_INPUT_IO_B     18  // Pulse Input GPIO

#define INVALID_HANDLE      0
#define ENCODER_TAKSK_SIZE  (1024 * 2)

EncoderCounter::EncoderCounter(int a_pin,int b_pin,int countSkip,ENC_COUNTER_CB cb, void* cb_data):
	aPinNumber{(gpio_num_t) a_pin},
	bPinNumber{(gpio_num_t) b_pin},
  countSkip(countSkip),
	cb(cb),
	cb_data(cb_data),
  countOffset(0)
{
  INIT();
}

EncoderCounter::~EncoderCounter() {}


static void IRAM_ATTR EncoderCounter_pcnt_intr_handler(void *eCounterObj) {
  ((EncoderCounter*)eCounterObj)->___ISR___();
}


void EncoderCounter::INIT() {
    /* Prepare configuration for the PCNT unit */
    pcnt_config_t pcnt_config;
    
    unit=PCNT_UNIT;
    // ch0
    pcnt_config.pulse_gpio_num  = aPinNumber;
    pcnt_config.ctrl_gpio_num   = bPinNumber;
    pcnt_config.channel         = PCNT_CHANNEL_0;
    pcnt_config.pos_mode        = PCNT_COUNT_INC;       // Count up on the positive edge
    pcnt_config.neg_mode        = PCNT_COUNT_DEC;       // Keep the counter value on the negative edge

    pcnt_config.lctrl_mode      = PCNT_MODE_REVERSE;    // Reverse counting direction if low
    pcnt_config.hctrl_mode      = PCNT_MODE_KEEP;       // Keep the primary counter mode if high

    
    pcnt_config.counter_h_lim   = PCNT_H_LIM_VAL;
    pcnt_config.counter_l_lim   = PCNT_L_LIM_VAL;
    pcnt_config.unit            = PCNT_UNIT;



    enc_config_CH0=pcnt_config;
    pcnt_unit_config(&pcnt_config);
    
    // ch1
    pcnt_config.pulse_gpio_num  = PCNT_INPUT_IO_B;
    pcnt_config.ctrl_gpio_num   = PCNT_INPUT_IO_A;
    pcnt_config.channel         = PCNT_CHANNEL_1;
    pcnt_config.pos_mode        = PCNT_COUNT_DEC;       // Count up on the positive edge
    pcnt_config.neg_mode        = PCNT_COUNT_INC;       // Keep the counter value on the negative edge

    


    enc_config_CH1=pcnt_config;
    pcnt_unit_config(&pcnt_config);

    /* Configure and enable the input filter */
    pcnt_set_filter_value(PCNT_UNIT, 100);
    pcnt_filter_enable(PCNT_UNIT);

    // /* Set threshold 0 and 1 values and enable events to watch */
    // pcnt_set_event_value(PCNT_UNIT, PCNT_EVT_THRES_1, PCNT_THRESH1_VAL);
    // pcnt_event_enable(PCNT_UNIT, PCNT_EVT_THRES_1);
    // pcnt_set_event_value(PCNT_UNIT, PCNT_EVT_THRES_0, PCNT_THRESH0_VAL);
    // pcnt_event_enable(PCNT_UNIT, PCNT_EVT_THRES_0);
    /* Enable events on zero, maximum and minimum limit values */
    pcnt_event_enable(PCNT_UNIT, PCNT_EVT_ZERO);
    pcnt_event_enable(PCNT_UNIT, PCNT_EVT_H_LIM);
    pcnt_event_enable(PCNT_UNIT, PCNT_EVT_L_LIM);


    {
      pcnt_set_event_value(PCNT_UNIT, PCNT_EVT_THRES_0, -countSkip);
      pcnt_set_event_value(PCNT_UNIT, PCNT_EVT_THRES_1,  countSkip);
      pcnt_event_enable(PCNT_UNIT, PCNT_EVT_THRES_0);
      pcnt_event_enable(PCNT_UNIT, PCNT_EVT_THRES_1);
    }

    /* Initialize PCNT's counter */
    pcnt_counter_pause(PCNT_UNIT);
    pcnt_counter_clear(PCNT_UNIT);

		pcnt_set_filter_value(PCNT_UNIT, 1023);
		pcnt_filter_enable(PCNT_UNIT);

    /* Register ISR handler and enable interrupts for PCNT unit */
    pcnt_isr_register(EncoderCounter_pcnt_intr_handler, this, 0, &intr_handler);
    pcnt_intr_enable(PCNT_UNIT);

    /* Everything is set up, now go to counting */
    pcnt_counter_resume(PCNT_UNIT);
}


void EncoderCounter::___ISR___()
{
  uint32_t intr_status = PCNT.int_st.val;
  if (intr_status & (BIT(unit))==0) return;
  

  if(PCNT.status_unit[unit].thres0_lat){
    count+=countSkip;
    pcnt_counter_clear((pcnt_unit_t)unit);
  } else if(PCNT.status_unit[unit].thres1_lat){
    count-=countSkip;
    pcnt_counter_clear((pcnt_unit_t)unit);
  }

  PCNT.int_clr.val = BIT(unit);  
  if(cb)cb(cb_data);
}

void EncoderCounter::setCount(int64_t value) {
	countOffset = value - getCountRaw();
}
int64_t EncoderCounter::getCountRaw() {
	int16_t c;
	pcnt_get_counter_value(unit, &c);
	return count+c;
}
int64_t EncoderCounter::getCount() {
	return countOffset + getCountRaw();
}

int64_t EncoderCounter::clearCount() {
	countOffset = 0;
  count=0;
	return pcnt_counter_clear(unit);
}

int64_t EncoderCounter::pauseCount() {
	return pcnt_counter_pause(unit);
}

int64_t EncoderCounter::resumeCount() {
	return pcnt_counter_resume(unit);
}

void EncoderCounter::setFilter(uint16_t value) {

	if(value>1023)value=1023;
	if(value==0) {
		pcnt_filter_disable(unit);
	} else {
		pcnt_set_filter_value(unit, value);
		pcnt_filter_enable(unit);
	}

}
