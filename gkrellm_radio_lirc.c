
#include "radio.h"
#include "gkrellm_radio.h"
#include "gkrellm_radio_lirc.h"

#include <lirc/lirc_client.h>


static gint gkrellm_radio_lirc_tag;

struct lirc_command
{
  char *name;
  void (*function) ();
};

#define LIRC_COMMAND(NAME) { #NAME, gkrellm_radio_lirc_ ## NAME }

struct lirc_command lirc_commands[] = {
  LIRC_COMMAND (mute),
  LIRC_COMMAND (prev_station),
  LIRC_COMMAND (next_station),
  LIRC_COMMAND (finetune_up),
  LIRC_COMMAND (finetune_down),
  LIRC_COMMAND (power),
  {NULL, NULL}
};


void inline
gkrellm_radio_lirc_next_station (void) {
  switch_station ();
}

void
gkrellm_radio_lirc_prev_station (void) {
  if (currentstation == -1) {
      if (nstations > 0) do_switch_station (0);
  } else
    do_switch_station (currentstation + nstations - 1);
}

void
gkrellm_radio_lirc_mute (void) {
  if (radio_ismute ())
    radio_unmute ();
  else
    radio_mute ();
}

void
gkrellm_radio_lirc_power (void) {
  gkrellm_radio_turn_onoff();
}

void gkrellm_radio_lirc_finetune_up(void) { 
  gkrellm_radio_finetune_delta(+0.05); 
}

void gkrellm_radio_lirc_finetune_down(void) { 
  gkrellm_radio_finetune_delta(-0.05); 
}

void
gkrellm_radio_lirc_cb (struct lirc_config *config, gint source,
                         GdkInputCondition cond) {
  char *code;
  char *c;
  char *command;
  int ret;
  int i;

  if (lirc_nextcode (&code) == 0 && code != NULL) {
    while ((ret = lirc_code2char (config, code, &c)) == 0 && c != NULL) {
	     i = 0;
	     while ((command = lirc_commands[i++].name) != NULL) {
	       if (g_strcasecmp (command, c) == 0) {
		       lirc_commands[i - 1].function ();
		       break;
		     }
	     }
	  }
    free (code);
    if (ret == -1) gkrellm_radio_lirc_exit ();
  }
}

int
gkrellm_radio_lirc_init (void)
{
  struct lirc_config *config;
  int socket;

  if ((socket = lirc_init (LIRC_PROG, 0)) == -1) return EXIT_FAILURE;

  if (lirc_readconfig (NULL, &config, NULL) == 0) {
      gkrellm_radio_lirc_tag = gdk_input_add_full (socket,
						   GDK_INPUT_READ,
						   GTK_SIGNAL_FUNC(gkrellm_radio_lirc_cb),
						   config,
						   (GdkDestroyNotify) lirc_freeconfig);
  }
  return 0;
}

void
gkrellm_radio_lirc_exit (void)
{
  gdk_input_remove (gkrellm_radio_lirc_tag);

  lirc_deinit ();
}
