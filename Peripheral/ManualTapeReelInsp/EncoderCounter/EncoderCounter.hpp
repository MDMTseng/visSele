#pragma once
#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/pcnt.h>

typedef void (*ENC_COUNTER_CB)(void*);

class EncoderCounter {
public:
	EncoderCounter(int a_pin,int b_pin,int countSkip,ENC_COUNTER_CB cb=nullptr, void* cb_data=nullptr);
	~EncoderCounter();
	void  setCount(int64_t);
	int64_t getCount();
	int64_t clearCount();
	int64_t pauseCount();
	int64_t resumeCount();
	void setFilter(uint16_t value);
	gpio_num_t aPinNumber;
	gpio_num_t bPinNumber;
	pcnt_unit_t unit;
	int countSkip = 2;
	volatile int64_t count=0;
	ENC_COUNTER_CB cb;
	void* cb_data;

  void ___ISR___();
  int countOffset;

private:
	int64_t getCountRaw();
  void INIT();
	pcnt_config_t enc_config_CH0;
	pcnt_config_t enc_config_CH1;
  pcnt_isr_handle_t intr_handler;
};

//Added by Sloeber
#pragma once

