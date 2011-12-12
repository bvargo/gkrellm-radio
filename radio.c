#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#define DEVICE "/dev/radio"              /* major=81, minor=64 */

// file descriptor for radio file
static int radio_fd = -1;

// last set frequency of radio
static float last_freq = 88.5;

// fractional multiplier for frequency setting
// frequency parameter = freq_frac * freq
// where freq_frac is this variable and freq is the desired frequency, as a
// float
static int freq_frac = 0;

// minimum and maximum frequencies
static float freq_min, freq_max;

// the current state of the mute
static int mute_state;

// determine the appropriate frequency multiplier for the first tuner on the
// open video device with the handle radio_fd
// in addition, determine the frequence ranges the tuner handles
// results are stored in the global variables
static void radio_get_tunerinfo()
{
   struct v4l2_tuner tuner;

   if (radio_fd == -1)
      return;

   memset(&tuner, 0, sizeof(tuner));

   // use the first tuner
   tuner.index = 0;

   // make sure the tuner specified is actually a tuner
   if (ioctl(radio_fd, VIDIOC_G_TUNER, &tuner) < 0)
      return;
   if (tuner.type != V4L2_TUNER_RADIO)
      return;

   // determine the frequency fraction
   freq_frac = (tuner.capability & V4L2_TUNER_CAP_LOW) == 0 ? 16 : 16000;

   // frequency minimum and maximum
   freq_min = ((float) tuner.rangelow)  / freq_frac;
   freq_max = ((float) tuner.rangehigh) / freq_frac;
}

// tune the radio to the given frequency
void radio_setfreq(float nfreq)
{
   struct v4l2_frequency freq;

   if (radio_fd == -1)
      return;

   // bound the desired frequency
   if (nfreq < freq_min)
      nfreq = freq_min;
   if (nfreq > freq_max)
      nfreq = freq_max;

   // set the frequency
   memset(&freq, 0, sizeof(freq));
   // use the first tuner
   freq.tuner = 0;
   freq.type = V4L2_TUNER_RADIO;
   freq.frequency = nfreq * freq_frac;

   if (ioctl(radio_fd, VIDIOC_S_FREQUENCY, &freq) < 0)
   {
      perror("VIDIOC_S_FREQUENCY, set frequency");
      return;
   }

   last_freq = nfreq;
}

// get the radio's current frequency
float radio_getfreq()
{
   struct v4l2_frequency freq;

   if (radio_fd == -1)
      return freq_min;

   // get the frequency
   memset(&freq, 0, sizeof(freq));
   // use the first tuner
   freq.tuner = 0;
   if (ioctl(radio_fd, VIDIOC_G_FREQUENCY, &freq) < 0)
   {
      perror("VIDIOC_G_FREQUENCY, get frequency");
      return freq_min;
   }

   last_freq = (float)freq.frequency / freq_frac;

   return last_freq;
}

// set the mute state
// if mute is 0, then unmute; otherwise, mute
void radio_set_mute(int mute)
{
   struct v4l2_control control;
   struct v4l2_queryctrl qcontrol;

   if (radio_fd == -1)
      return;

   // set mute to either 0 or 1
   if(mute > 0)
      mute = 1;
   else if(mute < 0)
      mute = 0;

   // mute or unmute as needed
   memset(&control, 0, sizeof(control));
   control.id = V4L2_CID_AUDIO_MUTE;
   control.value = mute;
   if (ioctl(radio_fd, VIDIOC_S_CTRL, &control) < 0)
   {
      perror("VIDIOC_S_CTRL, set mute/unmute");
      return;
   }

   // set the mute state
   mute_state = mute;

   // get the minimum and maximum volume
   memset(&qcontrol, 0, sizeof(qcontrol));
   qcontrol.id = V4L2_CID_AUDIO_VOLUME;
   if (ioctl(radio_fd, VIDIOC_QUERYCTRL, &qcontrol) < 0)
   {
      perror("VIDIOC_QUERYCTRL, get min/max volume");
      return;
   }

   // set the volume to the minimum or maximum value, depending on the mute
   // state
   memset(&control, 0, sizeof(control));
   control.id = V4L2_CID_AUDIO_VOLUME;
   control.value = qcontrol.minimum +
         mute * (qcontrol.maximum - qcontrol.minimum);
   if (ioctl(radio_fd, VIDIOC_S_CTRL, &control) < 0)
   {
      perror("VIDIOC_S_CTRL, set volume");
      return;
   }
}

// unmute the radio
void radio_unmute(void)
{
   radio_set_mute(0);
}

// mute the radio
void radio_mute(void)
{
   radio_set_mute(1);
}

// returns 0 if the radio is not muted, 1 if the radio is muted
int radio_ismute()
{
   return mute_state;
}

// opens the radio device
int open_radio()
{
   // if already open, do not open again
   if (radio_fd != -1)
      return 0;

   // open the file descriptor for the radio path
   if (-1 == (radio_fd = open(DEVICE, O_RDONLY)))
      return -1;

   // get the tuner information
   radio_get_tunerinfo();

   // unmute the radio
   radio_unmute();

   return 0;
}

// close the radio device
void close_radio()
{
   // if already close, then do not close again
   if (radio_fd == -1)
      return;

   // mute the radio
   radio_mute();

   // close the file descriptor
   close(radio_fd);
   radio_fd = -1;
}

// change the radio frequency by a delta amount
void radio_freq_delta(float delta)
{
   radio_setfreq(last_freq + delta);
}
