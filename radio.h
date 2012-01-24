#ifndef __radio_h__
#define __radio_h__

int open_radio();
void close_radio();
float current_freq();
void radio_tune(float newfreq);
void radio_freq_delta(float delta);
void radio_mute();
void radio_unmute();
int radio_ismute();

#endif /* __radio_h__ */
