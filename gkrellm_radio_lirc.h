
#ifndef GKRELLM_RADIO_LIRC_H
#define GKRELLM_RADIO_LIRC_H

#define LIRC_PROG "gkrellm_radio"

void gkrellm_radio_lirc_prev_station (void);
void gkrellm_radio_lirc_next_station (void);
void gkrellm_radio_lirc_mute (void);
void gkrellm_radio_lirc_power (void);
void gkrellm_radio_lirc_finetune_up (void);
void gkrellm_radio_lirc_finetune_down (void);

int gkrellm_radio_lirc_init (void);
void gkrellm_radio_lirc_exit (void);

void inline gkrellm_radio_lirc_next_station (void);

#endif /* GKRELLM_RADIO_LIRC_H */
