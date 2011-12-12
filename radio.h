#ifndef __radio_h__
#define __radio_h__

int open_radio();
void close_radio();
void radio_setfreq(float nfreq);
float radio_getfreq();
void radio_freq_delta(float delta);
void radio_set_mute(int mute);
void radio_mute();
void radio_unmute();
int radio_ismute();

#endif /* __radio_h__ */
