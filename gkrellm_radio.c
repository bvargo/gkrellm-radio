#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "radio.h"
#include "gkrellm_radio.h"

#ifdef HAVE_LIRC
#include "gkrellm_radio_lirc.h"
#endif

#define CONFIG_NAME "Radio Plugin" /* Name in the configuration window */
#define CONFIG_KEY  "radio"     /* Name in the configuration window */
#define STYLE_NAME  "radio"     /* Theme subdirectory name and gkrellmrc */
				/* style name.                 */

#define LOCATION MON_APM 
/* The location of the plugin, choose between :
 * |*   MON_CLOCK, MON_CPU, MON_PROC, MON_DISK,
 * |*   MON_INET, MON_NET, MON_FS, MON_MAIL,       
 * |*   MON_APM, or MON_UPTIME                    
 * */

static gchar *info_text[] = {
  "<b>GKrellM Radio Plugin\n\n",
  "This plugin allow you to control the radio card of your Linux box,\
   if you have one installed.\n",
  "<b>\nUser Interface:\n",
  "\n\t* Clicking the right button will turn the radio on and off.\n",
  "\t* Clicking the left text will tune to the next channel configured",
  " under configuration.\n"
  "\t* Using your mousewheel will tune up or down.\n",
  "\t*Right mouse button will pop up a channel menu\n",
  "<b>\nThanks to:\n",
  "\n\tLars Christensen - Upstream author untill version 0.2.1\n",
  "\tGerd Knorr - borrowed some of the radio interface code from his Xawtv",
  " radio application.\n",
  "<b>\nHomepage:\n",
  "\n\thttp://gkrellm.luon.net/gkrellm-radio.phtml\n"
};

static GkrellmPanel    *panel;
static GkrellmMonitor  *plugin_monitor;

static GkrellmDecal *station_text, *decal_onoff_pix;

GkrellmDecalbutton *onoff_button;

static gint     style_id;

static gint     button_state;	/* For station_text button */

gint onoff_state;	/* for onoff button */
static gfloat mutetime = 0.15;
static gboolean attempt_reopen = TRUE; 
static gboolean close_atexit = TRUE;

typedef struct station {
  char *station_name;
  float freq;
} station;

GtkWidget *menu = NULL;

static station *stations = NULL;
gint nstations = 0;
gint currentstation = -1;

static void set_text_freq(float freq);

void free_stations() {
  int i;
  for (i=0; i<nstations; i++) 
    free(stations[i].station_name);
  free(stations);
  stations = NULL;
  nstations = 0;
}

static gchar freqname[32];
char *station_name(float freq) {
  int i;
  for (i=0; i<nstations; i++) {
    if (fabs(freq - stations[i].freq) < 0.01) {
      currentstation = i;
      return stations[i].station_name;
    }
  }
  /* none found */
  currentstation = -1;
  sprintf(freqname, "%3.2f", freq);
  return freqname;
}

static gint mute_timeout_tag = -1;

gint mutetimeout(gpointer *user_data) {
  radio_unmute();
  mute_timeout_tag = -1;
  return FALSE;			/* dont call again */
}

void start_mute_timer() {
  if (mutetime > 0.001) {
    if (mute_timeout_tag != -1)
      gtk_timeout_remove(mute_timeout_tag);
    mute_timeout_tag =
      gtk_timeout_add(mutetime * 1000, (GtkFunction)mutetimeout, NULL);
    radio_mute();
  }
}

void 
do_switch_station(int nr) {
  nr %= nstations;
  currentstation = nr;
  start_mute_timer();
  radio_tune(stations[nr].freq);
  gkrellm_draw_decal_text(panel, station_text, 
      stations[nr].station_name, -1);
  gkrellm_draw_panel_layers(panel);
}

static void exit_func(void) {
  if (close_atexit) close_radio();
}

gint
freq_menu_activated(GtkWidget *widget, int freqnr) {
  do_switch_station(freqnr);
  return FALSE;
}

void
create_freq_menu(void) {
  GtkWidget *menuitem;
  int i;
  
  if (menu != NULL) {
    gtk_widget_destroy(menu);
  }
  if (!nstations) { 
    menu = NULL;
    return;
  }
    
  menu = gtk_menu_new();

  gtk_menu_set_title(GTK_MENU(menu),"frequency menu");

  menuitem = gtk_tearoff_menu_item_new();
  gtk_menu_append(GTK_MENU(menu),menuitem);
  gtk_widget_show(menuitem);

  for (i=0; i < nstations; i++) {
    menuitem = gtk_menu_item_new_with_label(stations[i].station_name);
    gtk_menu_append(GTK_MENU(menu),menuitem);

    gtk_signal_connect(GTK_OBJECT(menuitem),"activate",
       GTK_SIGNAL_FUNC(freq_menu_activated),GINT_TO_POINTER(i));
  }
  gtk_widget_show_all(menu);
}

void switch_station(void) {
  if (currentstation == -1) {
    if (nstations > 0) do_switch_station(0);
  } else do_switch_station(currentstation + 1);
}

void set_onoff_button(int on) {
  int imgid;
  if (on)
    imgid = D_MISC_BUTTON_ON;
  else
    imgid = D_MISC_BUTTON_OUT;
  gkrellm_set_decal_button_index(onoff_button, imgid);
  gkrellm_draw_panel_layers(panel);
}

void reopen_radio() {
  if (!attempt_reopen) return;

  if (open_radio() != -1) {
    if (radio_ismute()) {
      close_radio();
      onoff_state = 0;		/* off */
    } else {
      start_mute_timer();
      onoff_state = 1;		/* on */
    }
  }
  set_onoff_button(onoff_state);
}

void gkrellm_radio_turn_onoff(void) {
    onoff_state = !onoff_state;
    if (onoff_state) {
      if (open_radio() == -1) {
        	gkrellm_message_window("GKrellM radio plugin",
			      "Couldn't open /dev/radio", NULL);
      } else {
	    /* radio was opened */
      	start_mute_timer();
      	radio_tune(current_freq());
        set_text_freq(current_freq());
       	set_onoff_button(onoff_state);
      }
    } else {
      set_onoff_button(onoff_state);
      close_radio();
    }
}

void
cb_button(GkrellmDecalbutton *button)
{
  if (GPOINTER_TO_INT(button->data) == 1) { switch_station(); }
  if (GPOINTER_TO_INT(button->data) == 2) { gkrellm_radio_turn_onoff(); }
}

static void set_text_freq(float freq) {
  gkrellm_draw_decal_text(panel, station_text, station_name(freq), -1);
  gkrellm_draw_panel_layers(panel);
}

static gint
panel_expose_event(GtkWidget *widget, GdkEventExpose *ev) {
  gdk_draw_pixmap(widget->window,
		  widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
		  panel->pixmap, ev->area.x, ev->area.y, ev->area.x, ev->area.y,
		  ev->area.width, ev->area.height);
  return FALSE;
}

void gkrellm_radio_finetune_delta (float amount) {
  radio_freq_delta(amount);
  set_text_freq(current_freq());
  gkrellm_config_modified();
}

static gint 
button_release_event(GtkWidget *widget, GdkEventButton *ev, void *N) {

  if  (ev->button == 3) {
    if (menu == NULL) gkrellm_message_window("GKrellM radio plugin",
        "Please setup some channels in the configuration",NULL);
    else gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
        ev->state, ev->time);
  }
  return TRUE;
}

static gint scroll_event(GtkWidget *widget,GdkEventScroll *ev,gpointer data) {
  float delta = ev->direction == GDK_SCROLL_UP ? 0.05 : -0.05;
  gkrellm_radio_finetune_delta(delta);
  return TRUE;
}

static void
create_plugin(GtkWidget *vbox, gint first_create) {
  GkrellmStyle     *style;
  GkrellmTextstyle *ts, *ts_alt;
  GkrellmMargin    *margin;
  GdkPixmap       *pixmap;
  GdkBitmap       *mask;
  gint            y;
  gint            x;

  if (first_create) {
    panel = gkrellm_panel_new0();
    gkrellm_disable_plugin_connect(plugin_monitor,close_radio);
    /* create the frequency menu */
    create_freq_menu();
  } else
    gkrellm_destroy_decal_list(panel);

  style = gkrellm_meter_style(style_id);

  /* Each GkrellmStyle has two text styles.  The theme designer has picked the
     |  colors and font sizes, presumably based on knowledge of what you draw
     |  on your panel.  You just do the drawing.  You probably could assume
     |  the ts font is larger than the ts_alt font, but again you can be
     |  overridden by the theme designer.
  */
  ts = gkrellm_meter_textstyle(style_id);
  ts_alt = gkrellm_meter_alt_textstyle(style_id);
  panel->textstyle = ts;      /* would be used for a panel label */

  y = 2;			/* some border */
  station_text = gkrellm_create_decal_text(panel, "Hello World", ts_alt, style, 2, y, 40);

  /* Create a pixmap decal and place it to the right of station_text.  Use
     |  decals from the builtin decal_misc.xpm.
  */
  pixmap = gkrellm_decal_misc_pixmap();
  mask = gkrellm_decal_misc_mask();

  x = station_text->x + station_text->w + 4;
  decal_onoff_pix = gkrellm_create_decal_pixmap(panel, pixmap, mask,
						N_MISC_DECALS, NULL, x, y);

  /* Configure the panel to hold the above created decals, add in a little
     |  bottom margin for looks, and create the panel.
  */
  gkrellm_panel_configure(panel, NULL, style);
  gkrellm_panel_create(vbox, plugin_monitor,panel);

  /* After the panel is created, the decals can be converted into buttons.
     |  First draw the initial text into the text decal button and then
     |  put the text decal into a meter button.  But for the decal_onoff_pix,
     |  we can directly convert it into a decal button since it is a pixmap
     |  decal.  Just pass the frame we want to be the out image and the
     |  frame for the in or pressed image.
  */
  gkrellm_draw_decal_text(panel, station_text, station_name(current_freq()),
			  button_state);

  margin = gkrellm_get_style_margins(style);
  gkrellm_put_decal_in_meter_button(panel, station_text, cb_button,
				    GINT_TO_POINTER(1),margin);
  onoff_button = 
    gkrellm_make_decal_button(panel, decal_onoff_pix, cb_button,
			      GINT_TO_POINTER(2),
			      onoff_state ? D_MISC_BUTTON_ON : D_MISC_BUTTON_OUT,
			      D_MISC_BUTTON_IN);

  /* Note: all of the above gkrellm_draw_decal_XXX() calls will not
     |  appear on the panel until a  gkrellm_draw_layers(panel); call is
     |  made.  This will be done in update_plugin(), otherwise we would
     |  make the call here and anytime the decals are changed.
  */

  if (first_create) {
    g_signal_connect(GTK_OBJECT (panel->drawing_area), "expose_event",
		       (GtkSignalFunc) panel_expose_event, NULL);
    g_signal_connect(GTK_OBJECT (panel->drawing_area), "button_release_event",
		       GTK_SIGNAL_FUNC(button_release_event), NULL);
    g_signal_connect(GTK_OBJECT (panel->drawing_area), "scroll_event",
		       GTK_SIGNAL_FUNC(scroll_event), NULL);
  }
  
  reopen_radio();
  gkrellm_draw_panel_layers(panel);
}

static GtkWidget *gui_station_list = NULL;
static GtkWidget *gui_station_dialog = NULL;
static GtkWidget *gui_station_name_input, *gui_freq_input;
static GtkWidget *gui_mutetime_entry = NULL;
static GtkWidget *gui_reopen_toggle = NULL;
static GtkWidget *gui_close_toggle = NULL;

static gint gui_station_selected = -1;
static gint gui_station_count = 0;

void close_station_editor() {
  if (gui_station_dialog != NULL)
    gtk_widget_destroy(gui_station_dialog);
  gui_station_dialog = NULL;
}

void close_and_add_station_editor(gpointer *userdata) {
  gint new_entry = (gint)userdata;
  gchar *f[3];
  gfloat freq;
  gchar fstr[32];

  f[0] = (gchar *) gtk_entry_get_text(GTK_ENTRY(gui_station_name_input));
  freq = gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(gui_freq_input));
  sprintf(fstr, "%.2f", freq);
  f[1] = fstr;
  f[2] = "";

  if (new_entry) {
    gtk_clist_append(GTK_CLIST(gui_station_list), f);
    gui_station_count++;
  } else {
    assert(gui_station_selected != -1);
    gtk_clist_set_text(GTK_CLIST(gui_station_list),
		       gui_station_selected, 0,
		       f[0]);
    gtk_clist_set_text(GTK_CLIST(gui_station_list),
		       gui_station_selected, 1,
		       f[1]);
  }
  close_station_editor();
}

void create_station_editor(gint new_entry) {
  GtkWidget *label, *button, *table;
  GtkContainer *action_area, *dialog_area;
  GtkObject *adj;

  close_station_editor();

  gui_station_dialog = gtk_dialog_new();
  gtk_window_set_modal(GTK_WINDOW(gui_station_dialog), TRUE);
  action_area = GTK_CONTAINER (GTK_DIALOG(gui_station_dialog)->action_area);
  dialog_area = GTK_CONTAINER(GTK_DIALOG(gui_station_dialog)->vbox);
  
  table = gtk_table_new(2, 2, 0);
  
  label = gtk_label_new("Station Name:");
  gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_EXPAND, GTK_FILL, 4, 4);
  
  gui_station_name_input = gtk_entry_new();
  gtk_table_attach(GTK_TABLE(table), gui_station_name_input, 1, 2, 0, 1, GTK_EXPAND, GTK_FILL, 4, 4);

  label = gtk_label_new("Frequency:");
  gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_EXPAND, GTK_FILL, 4, 4);
  
  adj = gtk_adjustment_new(current_freq(), 0.05, 999.99, 0.05, 1, 1);
  gui_freq_input = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0.05, 2);
  gtk_table_attach(GTK_TABLE(table), gui_freq_input, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 4, 4);

  gtk_container_add(dialog_area, table);

  /* OK button */
  button = gtk_button_new_with_label("Ok"); 
  gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
			    GTK_SIGNAL_FUNC(close_and_add_station_editor),
			    (gpointer)new_entry);
  gtk_container_add(action_area, button);

  /* Cancel button */
  button = gtk_button_new_with_label("Cancel");
  gtk_signal_connect_object (GTK_OBJECT(button), "clicked",
			     GTK_SIGNAL_FUNC(close_station_editor), NULL);

  gtk_container_add(action_area, button);
}

void gui_new_station(GtkButton *button, gpointer *userdata) {
  close_station_editor();
  create_station_editor(TRUE);
  gtk_widget_show_all(gui_station_dialog);
}

void gui_edit_station(GtkButton *button, gpointer *userdata) {
  gchar *field = NULL;
  gfloat freq;
  close_station_editor();
  create_station_editor(FALSE);
  if (!gtk_clist_get_text(GTK_CLIST(gui_station_list), 
        gui_station_selected, 0, &field)) return;

  gtk_entry_set_text(GTK_ENTRY(gui_station_name_input), field);

  gtk_clist_get_text(GTK_CLIST(gui_station_list), gui_station_selected, 1, &field);
  freq = atof(field);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(gui_freq_input), freq);
  gtk_widget_show_all(gui_station_dialog);
}

void gui_delete_station(GtkButton *button, gpointer *userdata) {
  close_station_editor();
  if (gui_station_selected >= 0 && gui_station_selected < gui_station_count) {
    gtk_clist_remove(GTK_CLIST(gui_station_list), gui_station_selected);
    gui_station_selected = -1;
    gui_station_count--;
  }
}

void gui_moveup_station(GtkButton *button, gpointer *userdata) {
  close_station_editor();
  if (gui_station_selected > 0 && gui_station_selected < gui_station_count) {
    gtk_clist_swap_rows(GTK_CLIST(gui_station_list),
			gui_station_selected,
			gui_station_selected-1);
    gui_station_selected--;
  }
}

void gui_movedown_station(GtkButton *button, gpointer *userdata) {
  close_station_editor();
  if (gui_station_selected >= 0 && gui_station_selected < gui_station_count-1) {
    gtk_clist_swap_rows(GTK_CLIST(gui_station_list),
			gui_station_selected,
			gui_station_selected+1);
    gui_station_selected++;
  }
}

void gui_select_row(GtkCList *clist,
		    gint row,
		    gint column,
		    GdkEventButton *event,
		    gpointer user_data) {
  gui_station_selected = row;
}

static void create_config(GtkWidget *tab) {
  GtkWidget *tabs;
  GtkWidget *text, *label, *panel, *button, *hbox, *scrolled, *frame, *vbox;
  GtkObject *adj;
  gchar *plugin_about_text;
  gchar *station_tab_titles[3] = { "Station", "Frequency", "" };
  char *f[3];
  int i;

  tabs = gtk_notebook_new();
 
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs),GTK_POS_TOP);
  gtk_box_pack_start(GTK_BOX(tab),tabs,TRUE,TRUE,0); 

  /* STATION TAB */

  panel = gtk_vbox_new(0, 0);
  
  gui_station_list = gtk_clist_new_with_titles(3, station_tab_titles);
  gtk_clist_set_column_auto_resize(GTK_CLIST(gui_station_list),
				   0, TRUE);
  gtk_clist_set_column_auto_resize(GTK_CLIST(gui_station_list),
				   0, TRUE);
  gtk_clist_set_reorderable(GTK_CLIST(gui_station_list), TRUE);
  gtk_clist_set_column_justification(GTK_CLIST(gui_station_list),
				     1, GTK_JUSTIFY_RIGHT);

  /* fill data in clist */
  f[1] = malloc(32);
  f[2] = "";
  for (i = 0; i< nstations; i++) {
    f[0] = stations[i].station_name;
    snprintf(f[1],32, "%.2f", stations[i].freq);
    gtk_clist_append(GTK_CLIST(gui_station_list), f);
  }
  gui_station_count = nstations;
  free(f[1]);

  gtk_signal_connect(GTK_OBJECT(gui_station_list), "select-row",
		     (GtkSignalFunc)gui_select_row, NULL);

  scrolled = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(scrolled),gui_station_list);

  gtk_container_add(GTK_CONTAINER(panel), scrolled);

  hbox = gtk_hbox_new(0, 0);

  button = gtk_button_new_with_label("New");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc)gui_new_station, NULL);

  gtk_box_pack_start(GTK_BOX(hbox), button, 0, 0, 2);
  button = gtk_button_new_with_label("Edit"); 
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc)gui_edit_station, NULL);

  gtk_box_pack_start(GTK_BOX(hbox), button, 0, 0, 2);
  button = gtk_button_new_with_label("Delete"); 
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc)gui_delete_station, NULL);

  gtk_box_pack_start(GTK_BOX(hbox), button, 0, 0, 2);
  button = gtk_button_new_with_label("Up");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc)gui_moveup_station, NULL);

  gtk_box_pack_start(GTK_BOX(hbox), button, 0, 0, 2);

  button = gtk_button_new_with_label("Down");
  gtk_signal_connect(GTK_OBJECT(button), "clicked", (GtkSignalFunc)gui_movedown_station, NULL);
  gtk_box_pack_start(GTK_BOX(hbox), button, 0, 0, 2);
  
  label = gtk_label_new("Stations");

  gtk_box_pack_start(GTK_BOX(panel), hbox, FALSE, FALSE, 4);


  
  gtk_notebook_append_page(GTK_NOTEBOOK(tabs), panel, label);

  /* OPTIONS TAB */

  vbox = gtk_vbox_new(0,0);

  /* mutetime */ {

    hbox = gtk_hbox_new(0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, 0, 0, 2);
    
    label = gtk_label_new("Time to mute on channel jump (seconds):");
    gtk_box_pack_start(GTK_BOX(hbox), label, 0, 0, 2);
    
    adj = gtk_adjustment_new(mutetime, 0.0, 9.99, 0.01, 0.1, 1);
    gui_mutetime_entry = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0.01, 2);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gui_mutetime_entry), mutetime);
    gtk_box_pack_start(GTK_BOX(hbox), gui_mutetime_entry, 0, 0, 2);

  }

  /* reopenoption */ {

    gui_reopen_toggle = gtk_check_button_new_with_label("Attempt to reopen radio on startup");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gui_reopen_toggle), attempt_reopen);
    gtk_box_pack_start(GTK_BOX(vbox), gui_reopen_toggle, 0, 0, 2);
  /* close radio on exit toggle */
    gui_close_toggle = 
      gtk_check_button_new_with_label("Turn radio off when exiting gkrellm");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gui_close_toggle), 
        close_atexit);
    gtk_box_pack_start(GTK_BOX(vbox), gui_close_toggle, 0, 0, 2);

  }
  
  label = gtk_label_new("Options");
  gtk_notebook_append_page(GTK_NOTEBOOK(tabs), vbox, label);

  /* INFO TAB */
  
  frame = gtk_frame_new(NULL);
  scrolled = gkrellm_gtk_notebook_page(tabs,"Info");
  text = gkrellm_gtk_scrolled_text_view(scrolled,NULL,
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  gkrellm_gtk_text_view_append_strings(text,info_text,
      sizeof(info_text)/sizeof(gchar *));
  /* ABOUT TAB */

  plugin_about_text = g_strdup_printf(
   "Radio Plugin " VERSION "\n" \
   "GKrellM radio Plugin\n\n" \
   "Copyright (C) 2001-2002 Sjoerd Simons\n" \
   "sjoerd@luon.net\n" \
   "http://gkrellm.luon.net/gkrellm-radio.phtml\n\n" \
   "Released under the GNU General Public Licence");

  text = gtk_label_new(plugin_about_text); 
  label = gtk_label_new("About");
  gtk_notebook_append_page(GTK_NOTEBOOK(tabs),text,label);
  g_free(plugin_about_text);
}

static void apply_config(void) {
  int i;
  char *field = NULL;

  free_stations();
  nstations = gui_station_count;
  stations = malloc(sizeof(struct station) * nstations);

  for (i = 0; i < nstations; i++) {
    gtk_clist_get_text(GTK_CLIST(gui_station_list), i, 0, &field);
    stations[i].station_name = strdup(field);

    gtk_clist_get_text(GTK_CLIST(gui_station_list), i, 1, &field);
    stations[i].freq = atof(field);
  }

  mutetime = gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(gui_mutetime_entry));

  set_text_freq(current_freq()); /* reset the frequency display */

  attempt_reopen =
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui_reopen_toggle));

  close_atexit =
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui_close_toggle));
  /* recreate the frequency menu */
  create_freq_menu();
}

void save_config(FILE *f) {
  int i;
  fprintf(f, "%s freq %.2f\n", CONFIG_KEY, current_freq());
  fprintf(f, "%s nstations %d\n", CONFIG_KEY, nstations);
  for (i = 0; i<nstations; i++) {
    fprintf(f, "%s stationname%d %s\n", CONFIG_KEY, i, stations[i].station_name);
    fprintf(f, "%s stationfreq%d %.2f\n", CONFIG_KEY, i, stations[i].freq);
  }
  fprintf(f, "%s mutetime %.2f\n", CONFIG_KEY, mutetime);
  fprintf(f, "%s attemptreopen %d\n", CONFIG_KEY, attempt_reopen ? 1 : 0);
  fprintf(f, "%s close_atexit %d\n", CONFIG_KEY, close_atexit ? 1 : 0);
}

void load_config(gchar *s) {
  char *key, *value;

  key = s;
  value = strchr(s, ' ');
  if (value == NULL)
    return;
  *value = '\0';
  value++;

  if (strcmp(key, "freq") == 0) {
    start_mute_timer();
    radio_tune(atof(value));
  } else if (strcmp(key, "nstations") == 0) {
    free_stations();
    nstations = atoi(value);
    if (nstations < 0)
      nstations = 0;
    stations = malloc(sizeof(station) * nstations);
    memset(stations, 0, sizeof(station) * nstations);
  }
  else if (strncmp(key, "stationname", 11) == 0) {
    int stnum;
    stnum = atoi(key+11);
    if (stnum >= 0 && stnum < nstations) {
      stations[stnum].station_name = strdup(value);
    }
  }
  else if (strncmp(key, "stationfreq", 11) == 0) {
    int stnum;
    stnum = atoi(key+11);
    if (stnum >= 0 && stnum < nstations) {
      stations[stnum].freq = atof(value);
    }
  }
  else if (strcmp(key, "mutetime") == 0) {
    mutetime = atof(value);
  }
  else if (strcmp(key, "attemptreopen") == 0) {
    attempt_reopen = atoi(value);
  } else if (strcmp(key, "close_atexit") == 0) {
    close_atexit = atoi(value);
  }

}


/* The monitor structure tells GKrellM how to call the plugin routines.
*/
static GkrellmMonitor  plugin_mon  =
{
  CONFIG_NAME,			/* Name, for config tab.    */
  0,				/* Id,  0 if a plugin       */
  create_plugin,		/* The create function      */
  NULL,		/* The update function      */
  create_config,		/* The config tab create function   */
  apply_config,			/* Apply the config function        */

  save_config,			/* Save user config         */
  load_config,			/* Load user config         */
  CONFIG_KEY,			/* config keyword           */

  NULL,				/* Undefined 2  */
  NULL,				/* Undefined 1  */
  NULL,				/* private      */

  LOCATION, 			/* Insert plugin here */

  NULL,				/* Handle if a plugin, filled in by GKrellM */
  NULL				/* path if a plugin, filled in by GKrellM */
};


/* All GKrellM plugins must have one global routine named init_plugin()
|  which returns a pointer to a filled in monitor structure.
*/
GkrellmMonitor *
gkrellm_init_plugin() {
  style_id = gkrellm_add_meter_style(&plugin_mon, STYLE_NAME);
  plugin_monitor = &plugin_mon;
#ifdef HAVE_LIRC
  gkrellm_radio_lirc_init();
  g_atexit(gkrellm_radio_lirc_exit);
#endif
  g_atexit(exit_func);
  return &plugin_mon;
}
