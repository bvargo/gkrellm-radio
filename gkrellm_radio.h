
#ifndef GKRELLM_RADIO_H
#define GKRELLM_RADIO_H

#include <gkrellm/gkrellm.h>

extern gint nstations;
extern gint currentstation;

void switch_station (void);
void do_switch_station (int);
void gkrellm_radio_turn_onoff();
void gkrellm_radio_finetune_delta (float amount);

#endif /* GKRELLM_RADIO_H */
