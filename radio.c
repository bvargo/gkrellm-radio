/* based on
 * radio.c - (c) 1998 Gerd Knorr <kraxel@goldbach.in-berlin.de>
 * test tool for bttv + WinTV/Radio
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <asm/types.h>          /* XXX glibc */
#include "videodev.h"

#define DEVICE "/dev/radio"              /* major=81, minor=64 */

static int radio_fd = -1;
static float freq = 101.3;
static int fact = 0;
static float rangelow,rangehigh;

/* * Determine and return the appropriate frequency multiplier for
     the first tuner on the open video device with handle FD. 
   * Determing the freq ranges the tuner handles
*/
static void  radio_get_tunerinfo() {
    struct video_tuner tuner;
    tuner.tuner = 0;
    if (ioctl (radio_fd, VIDIOCGTUNER, &tuner) < 0) return;
    fact = (tuner.flags & VIDEO_TUNER_LOW) == 0 ? 16 : 1600;
    rangelow =  ((float) tuner.rangelow)/fact;
    rangehigh = ((float) tuner.rangehigh)/fact;
};

float
radio_setfreq(int fd, float nfreq)
{
    unsigned long ifreq ;
     
    if (fd == -1) return nfreq;
    if (nfreq < rangelow)  nfreq = rangelow;
    if (nfreq > rangehigh) nfreq = rangehigh;

    /* provide good rounding */
    ifreq = (nfreq * fact + 0.5);

    ioctl(fd, VIDIOCSFREQ, &ifreq);
    return nfreq;
}

void
radio_unmute(void)
{
    struct video_audio vid_aud;

    if (radio_fd == -1) return;

    if (ioctl(radio_fd, VIDIOCGAUDIO, &vid_aud)) perror("VIDIOCGAUDIO");
    if (vid_aud.volume == 0) vid_aud.volume = 65535;
    vid_aud.flags &= ~VIDEO_AUDIO_MUTE;
    if (ioctl(radio_fd, VIDIOCSAUDIO, &vid_aud)) perror("VIDIOCSAUDIO");
}

void radio_mute(void)
{
    struct video_audio vid_aud;

    if (radio_fd == -1)
      return;

    if (ioctl(radio_fd, VIDIOCGAUDIO, &vid_aud))
	perror("VIDIOCGAUDIO");
    vid_aud.flags |= VIDEO_AUDIO_MUTE;
    if (ioctl(radio_fd, VIDIOCSAUDIO, &vid_aud))
	perror("VIDIOCSAUDIO");
}

int radio_ismute() {
    struct video_audio vid_aud;

    if (radio_fd == -1)
      return 1;

    if (ioctl(radio_fd, VIDIOCGAUDIO, &vid_aud))
	perror("VIDIOCGAUDIO");

    return vid_aud.flags & VIDEO_AUDIO_MUTE;
}

int open_radio() {
  if (radio_fd != -1) return 0;
  if (-1 == (radio_fd = open(DEVICE, O_RDONLY))) return -1;
  radio_get_tunerinfo();
  radio_setfreq(radio_fd, freq);
  return 0;
}

void close_radio() {
  if (radio_fd == -1) return;
  radio_mute();
  close(radio_fd);
  radio_fd = -1;
}

float current_freq() {
  return freq;
}

void radio_tune(float newfreq) {
  freq = radio_setfreq(radio_fd, newfreq);
}

void radio_freq_delta(float delta) {
  radio_tune(freq + delta);
}
