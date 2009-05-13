#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include "ol_osd_window.h"
#include "ol_lrc_parser.h"
#include "ol_lrc_utility.h"
#include "ol_player.h"

#define REFRESH_INTERVAL 100
#define MAX_PATH_LEN 1024
static const gchar *lrc_path = ".lyrics";

static OlPlayerController *controller = NULL;
static OlMusicInfo music_info = {0};
static OlOsdWindow *osd = NULL;
static gchar *previous_title = NULL;
static gchar *previous_artist = NULL;
static gint previous_duration = 0;
static gint lrc_id = -1;
static gint lrc_next_id = -1;
static gint current_line = 0;
static LrcQueue *lrc_file = NULL;

void ol_init_osd ();
gint refresh_music_info (gpointer data);
void change_music ();
gboolean is_file_exist (const char *filename);
void get_user_home_directory (char *dir);
void update_osd (int time, int duration);

void
update_osd (int time, int duration)
{
  /* updates the time and lyrics */
  if (lrc_file != NULL && osd != NULL)
  {
    char current_lrc[1024];
    double percentage;
    int id;
    ol_lrc_utility_get_lyric_by_time (lrc_file, time, duration, current_lrc, &percentage, &id);
    if (lrc_id != id)
    {
      printf ("lrc_id = %d, lrc_next_id = %d\n", lrc_id, lrc_next_id);
      printf ("current id = %d\n", id);
      printf ("%0.2lf\n", percentage);
      if (id == -1)
        return;
      if (current_lrc != NULL)
        printf ("lyric: %s\n", current_lrc);
      if (id != lrc_next_id)
      {
        current_line = 0;
        ol_osd_window_set_lyric (osd, 0, current_lrc);
      }
      else
      {
        current_line = 1 - current_line;
      }
      lrc_id = id;
      LrcInfo *info = ol_lrc_parser_get_lyric_by_id (lrc_file, id);
      info = ol_lrc_parser_get_next_of_lyric (info);
      if (info != NULL)
      {
        lrc_next_id = ol_lrc_parser_get_lyric_id (info);
        printf ("next id = %d\n", lrc_next_id);
        printf ("next: %s\n", ol_lrc_parser_get_lyric_text (info));
        ol_osd_window_set_lyric (osd, 1 - current_line,
                                 ol_lrc_parser_get_lyric_text (info));
      }
      else
      {
        lrc_next_id = 0;
      }
      ol_osd_window_set_current_line (osd, current_line);
    }
    ol_osd_window_set_current_percentage (osd, percentage);
    if (!GTK_WIDGET_MAPPED (GTK_WIDGET (osd)))
      gtk_widget_show (GTK_WIDGET (osd));
  }
}

void
get_user_home_directory (char *dir)
{
  if (dir == NULL)
    return;
  struct passwd *buf;
  if (buf = getpwuid (getuid ()))
  {
    if (buf->pw_dir != NULL)
      strcpy (dir, buf->pw_dir);
  }
}

gboolean
is_file_exist (const char *filename)
{
  if (filename == NULL)
    return FALSE;
  struct stat buf;
  printf ("stat:%d\n", stat (filename, &buf));
  return stat (filename, &buf) == 0;
}

void
change_music ()
{
  printf ("%s\n",
          __FUNCTION__);
  gchar *file_name = NULL;
  lrc_id = -1;
  lrc_next_id = -1;
  char home_dir[MAX_PATH_LEN];
  if (lrc_file != NULL)
  {
    free (lrc_file);
    lrc_file = NULL;
  }
  if (previous_title == NULL)
    return;
  get_user_home_directory (home_dir);
  if (previous_artist == NULL)
  {
    file_name = g_strdup_printf ("%s/%s/%s.lrc", home_dir, lrc_path, previous_title);
  }
  else
  {
    file_name = g_strdup_printf ("%s/%s/%s-%s.lrc", home_dir, lrc_path, previous_artist, previous_title);
  }
  printf ("lrc file name:%s\n", file_name);
  if (!is_file_exist (file_name))
  {
    return;
  }
  lrc_file = ol_lrc_parser_get_lyric_info (file_name);
}

void
ol_init_osd ()
{
  osd = OL_OSD_WINDOW (ol_osd_window_new ());
  ol_osd_window_resize (osd, 1024, 200);
  ol_osd_window_set_lyric (osd, 0, "Hellow");
  ol_osd_window_set_lyric (osd, 1, "World");
  ol_osd_window_set_alignment (osd, 0.5, 1);
  gtk_widget_show (GTK_WIDGET (osd));
}

gint
refresh_music_info (gpointer data)
{
  if (controller == NULL)
  {
    controller = ol_player_get_active_player ();
  }
  if (controller == NULL)
    return;
  if (!controller->get_music_info (&music_info))
  {
    controller = NULL;
  }
  guint time = 0;
  if (!controller->get_played_time (&time))
  {
    controller = NULL;
  }
  guint duration = 0;
  if (!controller->get_music_length (&duration))
  {
    controller = NULL;
  }
  /* checks whether the music has been changed */
  gboolean changed = FALSE;
  gboolean stop = FALSE;
  /* compares the previous title with current title */
/*   if (previous_title) */
/*     printf ("%s\n", previous_title); */
  if (music_info.title == NULL)
  {
    if (previous_title != NULL)
    {
      g_free (previous_title);
      previous_title = NULL;
    }
    stop = TRUE;
  }
  else if (previous_title == NULL)
  {
    changed = TRUE;
    previous_title = g_strdup (music_info.title);
  }
  else if (strcmp (previous_title, music_info.title) != 0)
  {
    changed = TRUE;
    g_free (previous_title);
    previous_title = g_strdup (music_info.title);
  }
  /* compares the previous artist with current  */
  if (music_info.artist == NULL)
  {
    if (previous_artist != NULL)
    {
      g_free (previous_artist);
      previous_artist = NULL;
      changed = TRUE;
    }
  }
  else if (previous_artist == NULL)
  {
    changed = TRUE;
    previous_artist = g_strdup (music_info.artist);
  }
  else if (strcmp (previous_artist, music_info.artist) != 0)
  {
    printf ("duration\n");
    changed = TRUE;
    g_free (previous_artist);
    previous_artist = g_strdup (music_info.artist);
  }
  /* compares the previous duration */
  if (previous_duration != duration)
  {
    changed = TRUE;
    previous_duration = duration;
  }

  if (stop)
  {
    if (osd && GTK_WIDGET_MAPPED (osd)) 
      gtk_widget_hide (GTK_WIDGET (osd));
    return TRUE;
  }
  if (changed)
  {
    change_music ();
  }
  update_osd (time, duration);
  return TRUE;
}

int
main (int argc, char **argv)
{
  gtk_init (&argc, &argv);
  ol_player_init ();
  ol_init_osd ();
  ol_get_string_from_hash_table (NULL, NULL);
  g_timeout_add (REFRESH_INTERVAL, refresh_music_info, NULL);
  gtk_main ();
  ol_player_free ();
  return 0;
}