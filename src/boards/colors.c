/* gcompris - colors.c
 *
 * Copyright (C) 2002 Pascal Georges
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "gcompris/gcompris.h"

#define SOUNDLISTFILE PACKAGE

static GcomprisBoard *gcomprisBoard = NULL;
static gboolean board_paused = TRUE;

static void		 start_board (GcomprisBoard *agcomprisBoard);
static void		 pause_board (gboolean pause);
static void		 end_board (void);
static gboolean		 is_our_board (GcomprisBoard *gcomprisBoard);
static int gamewon;

static void		 process_ok(void);
static void		 highlight_selected(int);
static void		 game_won(void);
static void		 repeat(void);
static void		 colors_config_start(GcomprisBoard *agcomprisBoard,
					     GcomprisProfile *aProfile);
static void		 colors_config_stop(void);

/* ================================================================ */
static GnomeCanvasGroup *boardRootItem = NULL;
static GnomeCanvasItem *highlight_image_item = NULL;

static GnomeCanvasItem *colors_create_item(GnomeCanvasGroup *parent);
static void colors_destroy_all_items(void);
static void colors_next_level(void);
static gint item_event(GnomeCanvasItem *item, GdkEvent *event, gpointer data);
static int highlight_width, highlight_height;
static GList * listColors = NULL;

static SoundPolicy sound_policy;

#define LAST_COLOR 10
static gchar *colors[LAST_COLOR*2] = {
  "blue", 	N_("Click on the blue duck"),
  "brown",	N_("Click on the brown duck"),
  "green",	N_("Click on the green duck"),
  "grey",	N_("Click on the grey duck"),
  "orange",	N_("Click on the orange duck"),
  "purple",	N_("Click on the purple duck"),
  "red",	N_("Click on the red duck"),
  "yellow",	N_("Click on the yellow duck"),
  "black",	N_("Click on the black duck"),
  "white",	N_("Click on the white duck")
};

static int X[] = {75,212,242,368,414,533,578,709};
static int Y[] = {25,170,180,335,337,500};

/* Description of this plugin */
static BoardPlugin menu_bp =
  {
    NULL,
    NULL,
    "Colors",
    "Click on the right color",
    "Pascal Georges <pascal.georges1@free.fr>",
    NULL,
    NULL,
    NULL,
    NULL,
    start_board,
    pause_board,
    end_board,
    is_our_board,
    NULL,
    NULL,
    NULL,//set_level,
    NULL,
    repeat,
    colors_config_start,
    colors_config_stop
  };

/* =====================================================================
 *
 * =====================================================================*/
GET_BPLUGIN_INFO(colors)


/* ======================= */
/* = config_start        = */
/* ======================= */

static GcomprisProfile *profile_conf;
static GcomprisBoard   *board_conf;

static void save_table (gpointer key,
			gpointer value,
			gpointer user_data)
{
  gc_db_set_board_conf ( profile_conf,
			    board_conf,
			    (gchar *) key,
			    (gchar *) value);
}

static void
conf_ok(GHashTable *table)
{
  if (!table){
    if (gcomprisBoard)
      pause_board(FALSE);
    return;
  }

  g_hash_table_foreach(table, (GHFunc) save_table, NULL);

  if (gcomprisBoard){
    GHashTable *config = gc_db_get_board_conf();

    if (profile_conf)
      config = gc_db_get_board_conf();
    else
      config = table;

    gc_locale_reset();
    gc_locale_set(g_hash_table_lookup(config, "locale_sound"));

    if (profile_conf)
      g_hash_table_destroy(config);

    colors_next_level();

    pause_board(FALSE);

  }
  board_conf = NULL;
  profile_conf = NULL;
}

static void
colors_config_start(GcomprisBoard *agcomprisBoard,
		    GcomprisProfile *aProfile)
{
  board_conf = agcomprisBoard;
  profile_conf = aProfile;

  gchar *label;

  if (gcomprisBoard)
    pause_board(TRUE);

  label = g_strdup_printf(_("<b>%s</b> configuration\n for profile <b>%s</b>"),
			  agcomprisBoard->name, aProfile ? aProfile->name : "");

  gc_board_config_window_display(label, conf_ok);

  g_free(label);

  /* init the combo to previously saved value */
  GHashTable *config = gc_db_get_conf( profile_conf, board_conf);

  gchar *saved_locale_sound = g_hash_table_lookup( config, "locale_sound");

  gc_board_config_combo_locales_asset( _("Select sound locale"), saved_locale_sound,
				"voices/$LOCALE/colors/purple.ogg");

  g_hash_table_destroy(config);

}


/* ======================= */
/* = config_stop        = */
/* ======================= */
static void
colors_config_stop()
{
}


/* =====================================================================
 * in : boolean TRUE = PAUSE : FALSE = CONTINUE
 * =====================================================================*/
static void pause_board (gboolean pause)
{
  if(gcomprisBoard==NULL)
    return;

  gc_bar_hide(FALSE);
  if(gamewon == TRUE && pause == FALSE) /* the game is won */
    game_won();

  board_paused = pause;
}

/* =====================================================================
 *
 * =====================================================================*/
static void start_board (GcomprisBoard *agcomprisBoard) {
  GcomprisProperties	*properties = gc_prop_get();
  GList * list = NULL;
  int * item;
  int i, list_length;

  GHashTable *config = gc_db_get_board_conf();

  gc_locale_set(g_hash_table_lookup(config, "locale_sound"));

  g_hash_table_destroy(config);

  gc_sound_bg_pause();

  if(agcomprisBoard!=NULL) {
    gcomprisBoard=agcomprisBoard;
    gc_set_background(gnome_canvas_root(gcomprisBoard->canvas), "colors/colors_bg.png");
    gcomprisBoard->level=1;
    gcomprisBoard->maxlevel=1;

    if(properties->fx) {
      gc_bar_set(GC_BAR_CONFIG|GC_BAR_REPEAT);

      /* initial state to restore */
      sound_policy = gc_sound_policy_get();
      gc_sound_policy_set(PLAY_AND_INTERRUPT);

    } else {
      gc_bar_set(GC_BAR_CONFIG);
    }

    gamewon = FALSE;

    // we generate a list of color indexes in a random order
    for (i=0; i<LAST_COLOR; i++)
      list = g_list_append(list, GINT_TO_POINTER(i));

    while ((list_length = g_list_length(list))) {
      i = list_length == 1 ? 0 : g_random_int_range(0,g_list_length(list)-1);
      item = g_list_nth_data(list, i);
      listColors = g_list_append(listColors, item);
      list = g_list_remove(list, item);
    }
    g_list_free(list);

    gtk_signal_connect(GTK_OBJECT(gcomprisBoard->canvas), "event",  (GtkSignalFunc) item_event, NULL);
    colors_next_level();
    pause_board(FALSE);
  }
}

/* =====================================================================
 *
 * =====================================================================*/
static void end_board () {

  if(gcomprisBoard!=NULL){
    GcomprisProperties	*properties = gc_prop_get();

    pause_board(TRUE);
    gc_score_end();
    colors_destroy_all_items();
    // free list
    while (g_list_length(listColors) > 0)
      listColors = g_list_remove(listColors, g_list_nth_data(listColors,0));
    g_list_free(listColors);
    listColors=NULL;

    if(properties->fx) {
      gc_sound_policy_set(sound_policy);
    }

  }
  gc_locale_reset();
  gcomprisBoard = NULL;
  gc_sound_bg_resume();
}

/* =====================================================================
 *
 * =====================================================================*/
static gboolean is_our_board (GcomprisBoard *gcomprisBoard) {
  if (gcomprisBoard) {
    if(g_strcasecmp(gcomprisBoard->type, "colors")==0) {
      /* Set the plugin entry */
      gcomprisBoard->plugin=&menu_bp;
      return TRUE;
    }
  }
  return FALSE;
}
/* =====================================================================
 * set initial values for the next level
 * =====================================================================*/
static void colors_next_level() {
  colors_destroy_all_items();
  gamewon = FALSE;

  /* Try the next level */
  colors_create_item(gnome_canvas_root(gcomprisBoard->canvas));
  repeat();
}
/* ======================================= */
static void repeat (){

  if(gcomprisBoard!=NULL)
    {
      char *str  = NULL;
      GcomprisProperties *properties = gc_prop_get();

      str = g_strdup_printf("voices/$LOCALE/colors/%s.ogg",
			    colors[GPOINTER_TO_INT(g_list_nth_data(listColors, 0))*2]);

      /* If we don't find a sound in our locale or the sounds are disabled */
      if(str && properties->fx)
	gc_sound_play_ogg(str, NULL);

      g_free(str);

      str = g_strdup_printf(gettext(colors[GPOINTER_TO_INT(g_list_nth_data(listColors, 0))*2+1]));

      gnome_canvas_item_new (boardRootItem,
			     gnome_canvas_text_get_type (),
			     "text", str,
			     "font", gc_skin_font_board_huge_bold,
			     "x", (double) BOARDWIDTH/2+2,
			     "y", (double) BOARDHEIGHT-25+2,
			     "anchor", GTK_ANCHOR_CENTER,
			     "fill_color", "black",
			     NULL);

      gnome_canvas_item_new (boardRootItem,
			     gnome_canvas_text_get_type (),
			     "text", str,
			     "font", gc_skin_font_board_huge_bold,
			     "x", (double) BOARDWIDTH/2,
			     "y", (double) BOARDHEIGHT-25,
			     "anchor", GTK_ANCHOR_CENTER,
			     "fill_color", "blue",
			     NULL);
      g_free(str);
    }
}
/* =====================================================================
 * Destroy all the items
 * =====================================================================*/
static void colors_destroy_all_items() {
  if(boardRootItem!=NULL)
    gtk_object_destroy (GTK_OBJECT(boardRootItem));

  boardRootItem = NULL;
}

/* =====================================================================
 *
 * =====================================================================*/
static GnomeCanvasItem *colors_create_item(GnomeCanvasGroup *parent) {
  GdkPixbuf *highlight_pixmap = NULL;
  char *str = NULL;

  boardRootItem = GNOME_CANVAS_GROUP(
				     gnome_canvas_item_new (gnome_canvas_root(gcomprisBoard->canvas),
							    gnome_canvas_group_get_type (),
							    "x", (double) 0,
							    "y", (double) 0,
							    NULL));

  str = g_strdup_printf("%s/%s", gcomprisBoard->boarddir, "colors_highlight.png");
  highlight_pixmap = gc_pixmap_load(str);

  highlight_image_item = gnome_canvas_item_new (boardRootItem,
						gnome_canvas_pixbuf_get_type (),
						"pixbuf", highlight_pixmap,
						"x", (double) 0,
						"y", (double) 0,
						"width", (double) gdk_pixbuf_get_width(highlight_pixmap),
						"height", (double) gdk_pixbuf_get_height(highlight_pixmap),
						"width_set", TRUE,
						"height_set", TRUE,
						NULL);

  highlight_width = gdk_pixbuf_get_width(highlight_pixmap);
  highlight_height = gdk_pixbuf_get_height(highlight_pixmap);

  g_free(str);

  gnome_canvas_item_hide(highlight_image_item);

  gdk_pixbuf_unref(highlight_pixmap);

  return NULL;
}
/* =====================================================================
 *
 * =====================================================================*/
static void game_won() {
  gcomprisBoard->sublevel++;

  listColors = g_list_remove(listColors, g_list_nth_data(listColors,0));

  if( g_list_length(listColors) <= 0 ) { // the current board is finished : bail out
    gc_bonus_end_display(GC_BOARD_FINISHED_TUXLOCO);
    return;
  }

  colors_next_level();
}
/* =====================================================================
 *
 * =====================================================================*/
static gboolean process_ok_timeout() {
  gc_bonus_display(gamewon, GC_BONUS_GNU);
  return FALSE;
}

static void process_ok() {
  gc_bar_hide(TRUE);
  // leave time to display the right answer
  g_timeout_add(TIME_CLICK_TO_BONUS, process_ok_timeout, NULL);
}
/* =====================================================================
 *
 * =====================================================================*/
static gint item_event(GnomeCanvasItem *item, GdkEvent *event, gpointer data) {
  double x, y;
  int i, j, clicked;

  x = event->button.x;
  y = event->button.y;

  if (!gcomprisBoard || board_paused)
    return FALSE;

  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
      gnome_canvas_c2w(gcomprisBoard->canvas, x, y, &x, &y);
      clicked = -1;
      for (i=0; i<4; i++) {
	for (j=0; j<2; j++) {
	  if (x>X[i*2] && x<X[i*2+1] && y>Y[j*2] && y<Y[j*2+1]) {
	    clicked = j*4 + i;
	  }
	}
      }
      if (x>X[2] && x<X[3] && y>Y[4] && y<Y[5])
	clicked = 8;
      if (x>X[4] && x<X[5] && y>Y[4] && y<Y[5])
	clicked = 9;

      if (clicked >= 0) {
	gc_sound_play_ogg ("sounds/bleep.wav", NULL);
	board_paused = TRUE;
	highlight_selected(clicked);
	gamewon = (clicked == GPOINTER_TO_INT(g_list_nth_data(listColors,0)));
        process_ok();
      }
      break;

    default:
      break;
    }
  return FALSE;
}

/* =====================================================================
 *
 * =====================================================================*/
static void highlight_selected(int c) {
  int x, y;

  g_assert(c>=0 && c<=9);

  if (c<8) {
    x = (X[(c%4)*2] + X[(c%4)*2+1]) /2;
    y = (Y[(int)(c/4)*2] + Y[(int)(c/4)*2+1]) /2;
  } else {
    y = (Y[4]+Y[5]) /2;
    if (c==8)
      x = (X[2] + X[3]) /2;
    else
      x = (X[4] + X[5]) /2;
  }
  x -= highlight_width/2;
  y -= highlight_height/2;
  gnome_canvas_item_show(highlight_image_item);
  gc_item_absolute_move(highlight_image_item, x, y);
}