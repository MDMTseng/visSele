
#include "StepperISR.h"

#if defined(ARDUINO_ARCH_ESP32)

#define DEFAULT_TIMER_H_L_TRANSITION 160

// cannot be updated while timer is running => fix it to 0
#define TIMER_PRESCALER 0

// Here are the global variables to interface with the interrupts
StepperQueue fas_queue[NUM_QUEUES];

// Here the associated mapping from queue to mcpwm/pcnt units
static const struct mapping_s queue2mapping[NUM_QUEUES] = {
    {
      mcpwm_unit : MCPWM_UNIT_0,
      timer : 0,
      pwm_output_pin : MCPWM0A,
      pcnt_unit : PCNT_UNIT_0,
      input_sig_index : PCNT_SIG_CH0_IN0_IDX,
      timer_tez_int_clr : MCPWM_TIMER0_TEZ_INT_CLR,
      timer_tez_int_ena : MCPWM_TIMER0_TEZ_INT_ENA
    },
    {
      mcpwm_unit : MCPWM_UNIT_0,
      timer : 1,
      pwm_output_pin : MCPWM1A,
      pcnt_unit : PCNT_UNIT_1,
      input_sig_index : PCNT_SIG_CH0_IN1_IDX,
      timer_tez_int_clr : MCPWM_TIMER1_TEZ_INT_CLR,
      timer_tez_int_ena : MCPWM_TIMER1_TEZ_INT_ENA
    },
    {
      mcpwm_unit : MCPWM_UNIT_0,
      timer : 2,
      pwm_output_pin : MCPWM2A,
      pcnt_unit : PCNT_UNIT_2,
      input_sig_index : PCNT_SIG_CH0_IN2_IDX,
      timer_tez_int_clr : MCPWM_TIMER2_TEZ_INT_CLR,
      timer_tez_int_ena : MCPWM_TIMER2_TEZ_INT_ENA
    },
    {
      mcpwm_unit : MCPWM_UNIT_1,
      timer : 0,
      pwm_output_pin : MCPWM0A,
      pcnt_unit : PCNT_UNIT_3,
      input_sig_index : PCNT_SIG_CH0_IN3_IDX,
      timer_tez_int_clr : MCPWM_TIMER0_TEZ_INT_CLR,
      timer_tez_int_ena : MCPWM_TIMER0_TEZ_INT_ENA
    },
    {
      mcpwm_unit : MCPWM_UNIT_1,
      timer : 1,
      pwm_output_pin : MCPWM1A,
      pcnt_unit : PCNT_UNIT_4,
      input_sig_index : PCNT_SIG_CH0_IN4_IDX,
      timer_tez_int_clr : MCPWM_TIMER1_TEZ_INT_CLR,
      timer_tez_int_ena : MCPWM_TIMER1_TEZ_INT_ENA
    },
    {
      mcpwm_unit : MCPWM_UNIT_1,
      timer : 2,
      pwm_output_pin : MCPWM2A,
      pcnt_unit : PCNT_UNIT_5,
      input_sig_index : PCNT_SIG_CH0_IN5_IDX,
      timer_tez_int_clr : MCPWM_TIMER2_TEZ_INT_CLR,
      timer_tez_int_ena : MCPWM_TIMER2_TEZ_INT_ENA
    },
};

void IRAM_ATTR next_command(StepperQueue *queue, struct queue_entry *e) {
  const struct mapping_s *mapping = queue->mapping;
  mcpwm_unit_t mcpwm_unit = mapping->mcpwm_unit;
  mcpwm_dev_t *mcpwm = mcpwm_unit == MCPWM_UNIT_0 ? &MCPWM0 : &MCPWM1;
  uint8_t timer = mapping->timer;
  uint8_t steps = e->steps;
  PCNT.conf_unit[mapping->pcnt_unit].conf2.cnt_h_lim = steps;  // is updated only on zero
  if (e->toggle_dir) {
    uint8_t dirPin = queue->dirPin;
    digitalWrite(dirPin, digitalRead(dirPin) == HIGH ? LOW : HIGH);
  }
  uint16_t ticks = e->ticks;
  queue->ticks = ticks;
  mcpwm->timer[timer].period.period = ticks;
  mcpwm->channel[timer].cmpr_value[0].cmpr_val = ticks >> 1;
  if (steps == 0) {
    mcpwm->channel[timer].generator[0].utez = 1;  // low at zero
    mcpwm->int_clr.val |= mapping->timer_tez_int_clr;
    mcpwm->int_ena.val |= mapping->timer_tez_int_ena;
  } else {
    mcpwm->channel[timer].generator[0].utez = 2;  // high at zero
    mcpwm->int_ena.val &= ~mapping->timer_tez_int_ena;
  }
}

static void IRAM_ATTR what_is_next(StepperQueue *q) {
  uint8_t rp = q->read_idx;
  if (rp != q->next_write_idx) {
    struct queue_entry *e = &q->entry[rp & QUEUE_LEN_MASK];
    rp++;
    q->read_idx = rp;
    next_command(q, e);
  } else {
    // no more commands: stop timer at period end
	// Same as forceStop() => perhaps combine
    const struct mapping_s *mapping = q->mapping;
    mcpwm_unit_t mcpwm_unit = mapping->mcpwm_unit;
    mcpwm_dev_t *mcpwm = mcpwm_unit == MCPWM_UNIT_0 ? &MCPWM0 : &MCPWM1;
    uint8_t timer = mapping->timer;
    mcpwm->timer[timer].mode.start = 1;           // stop at TEP
    mcpwm->channel[timer].generator[0].utez = 1;  // low at zero
    mcpwm->int_ena.val &= ~mapping->timer_tez_int_ena;
    q->isRunning = false;
    q->queue_end.ticks = TICKS_FOR_STOPPED_MOTOR;
  }
}

static void IRAM_ATTR pcnt_isr_service(void *arg) {
  StepperQueue *q = (StepperQueue *)arg;
  what_is_next(q);
}

// MCPWM_SERVICE is only used in case of pause
#define MCPWM_SERVICE(mcpwm, TIMER, pcnt)            \
  if (mcpwm.int_st.timer##TIMER##_tez_int_st != 0) { \
    mcpwm.int_clr.timer##TIMER##_tez_int_clr = 1;    \
    StepperQueue *q = &fas_queue[pcnt];              \
    what_is_next(q);                                 \
  }

static void IRAM_ATTR mcpwm0_isr_service(void *arg) {
  MCPWM_SERVICE(MCPWM0, 0, 0);
  MCPWM_SERVICE(MCPWM0, 1, 1);
  MCPWM_SERVICE(MCPWM0, 2, 2);
}
static void IRAM_ATTR mcpwm1_isr_service(void *arg) {
  MCPWM_SERVICE(MCPWM1, 0, 3);
  MCPWM_SERVICE(MCPWM1, 1, 4);
  MCPWM_SERVICE(MCPWM1, 2, 5);
}

void StepperQueue::init(uint8_t queue_num, uint8_t step_pin) {
  _initVars();
  _step_pin = step_pin;

  digitalWrite(step_pin, LOW);
  pinMode(step_pin, OUTPUT);
  mapping = &queue2mapping[queue_num];
  isRunning = false;

  mcpwm_unit_t mcpwm_unit = mapping->mcpwm_unit;
  mcpwm_dev_t *mcpwm = mcpwm_unit == MCPWM_UNIT_0 ? &MCPWM0 : &MCPWM1;
  pcnt_unit_t pcnt_unit = mapping->pcnt_unit;
  uint8_t timer = mapping->timer;

  pcnt_config_t cfg;
  // if step_pin is not set here (or 0x30), then it does not work
  cfg.pulse_gpio_num = step_pin;          // static 0 is 0x30, static 1 is 0x38
  cfg.ctrl_gpio_num = PCNT_PIN_NOT_USED;  // static 0 is 0x30, static 1 is 0x38
  cfg.lctrl_mode = PCNT_MODE_KEEP;
  cfg.hctrl_mode = PCNT_MODE_KEEP;
  cfg.pos_mode = PCNT_COUNT_INC;
  cfg.neg_mode = PCNT_COUNT_DIS;
  cfg.counter_h_lim = 1;
  cfg.counter_l_lim = 0;
  cfg.unit = pcnt_unit;
  cfg.channel = PCNT_CHANNEL_0;
  pcnt_unit_config(&cfg);

  pcnt_counter_clear(pcnt_unit);
  pcnt_counter_resume(pcnt_unit);
  pcnt_event_enable(pcnt_unit, PCNT_EVT_H_LIM);
  if (queue_num == 0) {
    // isr_service_install apparently enables the interrupt
    PCNT.int_clr.val = PCNT.int_st.val;
    pcnt_isr_service_install(ESP_INTR_FLAG_SHARED | ESP_INTR_FLAG_IRAM);
  }
  pcnt_isr_handler_add(pcnt_unit, pcnt_isr_service, (void *)this);

  if (timer == 0) {
    // Init mcwpm module for use
    periph_module_enable(mcpwm_unit == MCPWM_UNIT_0 ? PERIPH_PWM0_MODULE
                                                    : PERIPH_PWM1_MODULE);
    mcpwm->int_ena.val = 0;  // disable all interrupts
    mcpwm_isr_register(
        mcpwm_unit,
        mcpwm_unit == MCPWM_UNIT_0 ? mcpwm0_isr_service : mcpwm1_isr_service,
        NULL, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_SHARED, NULL);

    mcpwm->clk_cfg.prescale = 10 - 1;  // 160 MHz/10  => 16 MHz

    mcpwm->timer_sel.operator0_sel = 0;  // timer 0 is input for operator 0
    mcpwm->timer_sel.operator1_sel = 1;  // timer 1 is input for operator 1
    mcpwm->timer_sel.operator2_sel = 2;  // timer 2 is input for operator 2
  }
  mcpwm->timer[timer].period.upmethod = 0;  // 0 = immediate update, 1 = TEZ
  mcpwm->timer[timer].period.prescale = TIMER_PRESCALER;
  mcpwm->timer[timer].period.period = 400;
  mcpwm->timer[timer].mode.val = 0;  // freeze
  mcpwm->timer[timer].sync.val = 0;  // no sync
  mcpwm->channel[timer].cmpr_cfg.a_upmethod = 0;
  mcpwm->channel[timer].cmpr_value[0].cmpr_val = DEFAULT_TIMER_H_L_TRANSITION;
  mcpwm->channel[timer].generator[0].val = 0;
  mcpwm->channel[timer].generator[1].val = 0;
  // mcpwm->channel[timer].generator[0].utez = 2;  // high at zero
  mcpwm->channel[timer].generator[0].utez = 1;  // low at zero
  mcpwm->channel[timer].generator[0].utea = 1;  // low at compare A match
  mcpwm->channel[timer].db_cfg.val = 0;
  mcpwm->channel[timer].carrier_cfg.val = 0;  // carrier disabled

  // at last link mcpwm to output pin and back into pcnt input
  connect();
}

void StepperQueue::connect() {
  mcpwm_unit_t mcpwm_unit = mapping->mcpwm_unit;
  mcpwm_gpio_init(mcpwm_unit, mapping->pwm_output_pin, _step_pin);
  // Doesn't work with gpio_matrix_in
  //  gpio_matrix_in(step_pin, mapping->input_sig_index, false);
  gpio_iomux_in(_step_pin, mapping->input_sig_index);
}

void StepperQueue::disconnect() {
  // sig_index = 0x100 => cancel output
  gpio_matrix_out(_step_pin, 0x100, false, false);
  // untested alternative:
  //	gpio_reset_pin((gpio_num_t)q->step_pin);
}

// Mechanism is like this, starting from stopped motor:
//
// *	init counter
// *	init mcpwm
// *	start mcpwm
// *	-- mcpwm counter counts every L->H-transition at mcpwm.timer = 0
// *	-- if counter reaches planned steps, then counter is reset and
// *	interrupt is created
//
// *	pcnt interrupt: available time is from mcpwm.timer = 0+x to period
//		-	read next commmand: store period in counter shadow and
// steps in pcnt
//		- 	without next command: set mcpwm to stop mode on reaching
// period

void StepperQueue::startQueue() {
  mcpwm_unit_t mcpwm_unit = mapping->mcpwm_unit;
  mcpwm_dev_t *mcpwm = mcpwm_unit == MCPWM_UNIT_0 ? &MCPWM0 : &MCPWM1;
  uint8_t timer = mapping->timer;

  // timer should be either at TEP or at zero
  mcpwm->channel[timer].generator[0].utez = 1;  // low at zero
  mcpwm->timer[timer].period.upmethod = 0;      // 0 = immediate update, 1 = TEZ
  // period will be overwritten later in next_command
  mcpwm->timer[timer].period.period = 65535;
  isRunning = true;

  mcpwm->timer[timer].mode.val = 10;  // free run incrementing

  // busy wait period for timer zero
  while (mcpwm->timer[timer].status.value <= DEFAULT_TIMER_H_L_TRANSITION) {
  }

  // my interrupt cannot be called in this state, so modifying read_idx without
  // interrupts disabled is ok
  struct queue_entry *e = &entry[read_idx++ & QUEUE_LEN_MASK];
  next_command(this, e);
}
void StepperQueue::forceStop() {
  mcpwm_unit_t mcpwm_unit = mapping->mcpwm_unit;
  mcpwm_dev_t *mcpwm = mcpwm_unit == MCPWM_UNIT_0 ? &MCPWM0 : &MCPWM1;
  uint8_t timer = mapping->timer;
  mcpwm->timer[timer].mode.start = 1;           // stop at TEP
  mcpwm->channel[timer].generator[0].utez = 1;  // low at zero
  mcpwm->int_ena.val &= ~mapping->timer_tez_int_ena;
  isRunning = false;
  queue_end.ticks = TICKS_FOR_STOPPED_MOTOR;
}
bool StepperQueue::isValidStepPin(uint8_t step_pin) { return true; }
int8_t StepperQueue::queueNumForStepPin(uint8_t step_pin) { return -1; }
#endif
