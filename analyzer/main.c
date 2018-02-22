/***************************************************************************
                        main.c  -  description
                           -------------------
  begin                : 5th Aug 2006
  copyright            : (C) 2006 by Joseph Rabinoff
  email                : bobqwatson@yahoo.com
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/* This is a simple GStreamer application to set up a generic
 * moodbar-analyzer pipeline and run it.
 */


#include <gst/gst.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define WEBPAGE "http://amarok.kde.org/wiki/Moodbar"

/* The maximum number of times the main loop will run (in case
 * the file's modify time has changed) before it gives up
 */
#define MAX_TRIES 10


/* These should match up with the enum in moodbar.cpp */

#define RETURN_SUCCESS     0
#define RETURN_NOPLUGIN    1
#define RETURN_NOFILE      2
#define RETURN_COMMANDLINE 3

static gint   return_val  = RETURN_SUCCESS;
static gchar *output_file = NULL;


static GstElement *
make_element (const gchar *elt, const gchar *name)
{
  GstElement *ret = gst_element_factory_make (elt, name);

  if (ret == NULL)
    {
      g_print ("Could not create element of type %s, please install it.\n"
	       "A list of plugins can be found at " WEBPAGE "\n"
	       "Also please check that the moodbar package was installed\n"
	       "in the -->same prefix<-- as GStreamer: the moodbar binary\n"
	       "should be in the same directory as gst-inspect.\n",
	       elt);
      exit (RETURN_NOPLUGIN);
    }

  return ret;
}


/* Check for playback errors and end-of-stream */
static gboolean
bus_callback (GstBus *bus,
	      GstMessage *message,
	      gpointer data)
{
  GMainLoop *loop = (GMainLoop *) data;

  (void) bus;  /* Unused */

  switch (GST_MESSAGE_TYPE (message)) 
    {
    case GST_MESSAGE_ERROR: 
      {
	GError *err;
	gchar *debug;
	
	gst_message_parse_error (message, &err, &debug);
	g_print ("Bus error: %s\n"
		 "Please see " WEBPAGE " for troubleshooting tips.\n",
		 err->message);
	g_error_free (err);
	g_free (debug);
	
	return_val = RETURN_NOFILE;
	unlink (output_file);
	g_main_loop_quit (loop);
	break;
      }

    case GST_MESSAGE_EOS:
      /* end-of-stream */
      g_print ("Received end-of-stream, exiting...\n");
      g_main_loop_quit (loop);
      break;

    default:
      /* unhandled message */
      break;
  }

  /* remove message from the queue */
  return TRUE;
}


/* When the auto-decoder creates a new sink pad, check if it
 * looks like an audio pad, and if so, link it to the audio
 * chain.
 */
static void
cb_newpad (GstElement *dec,
           GstPad     *pad,
           gboolean   last,
           gpointer   data)
{
  GstCaps *caps;
  GstStructure *str;
  GstPad *audiopad;
  GstElement *audio = (GstElement *) data;

  (void) last;  /* Unused */
  (void) dec;  /* Unused */

  /* Only link once */
  audiopad = gst_element_get_pad (audio, "sink");
  if (GST_PAD_IS_LINKED (audiopad)) 
    {
      g_object_unref (audiopad);
      return;
    }

  /* Check media type */
  caps = gst_pad_get_caps (pad);
  str = gst_caps_get_structure (caps, 0);
  if (!g_strrstr (gst_structure_get_name (str), "audio")) 
    {
      gst_caps_unref (caps);
      gst_object_unref (audiopad);
      return;
    }
  gst_caps_unref (caps);

  /* link'n'play */
  gst_pad_link (pad, audiopad);
}


/* When the decoder doesn't recognize an input type,
 * this callback is executed.
 */
static void
cb_cantdecode (GstElement *dec,
	       GstPad     *pad,
	       GstCaps    *caps,
	       gpointer   data)
{
  /* Unused parameters */
  (void) dec;
  (void) pad;
  (void) caps;

  GMainLoop *loop = (GMainLoop *) data;

  g_print ("GStreamer does not how to decode the audio file.\n"
	   "You probably do not have the appropriate plugin installed.\n"
	   "Please see the wiki page at " WEBPAGE "\n"
	   "for a plugin list, and troubleshooting tips.\n");
  unlink (output_file);
  return_val = RETURN_NOFILE;
  g_main_loop_quit (loop);
}


/* Run the main loop */
static void
run_loop (gchar *infile, gchar *outfile)
{
  GMainLoop *loop;
  GstPad *audiopad;
  GstBus *bus;
  GstElement *src, *decoder, *sink, *conv;
  GstElement *fft, *moodbar;
  GstElement *pipeline, *audio;

  loop = g_main_loop_new (NULL, FALSE);

  /* Setup the pipeline */
  pipeline = gst_pipeline_new ("pipeline");

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (bus, bus_callback, loop);
  gst_object_unref (bus);

  src = make_element ("filesrc", "source");
  g_object_set (G_OBJECT (src), "location", infile, NULL);
  decoder = make_element ("decodebin", "decoder");
  gst_bin_add_many (GST_BIN (pipeline), src, decoder, NULL);
  gst_element_link (src, decoder);

  /* Create audio output bin */
  audio = gst_bin_new ("audiobin");
  conv  = make_element ("audioconvert", "aconv");
  audiopad = gst_element_get_pad (conv, "sink");

  /* Create analyzer chain */
  fft = make_element ("fftwspectrum", "fft");
  g_object_set (G_OBJECT (fft), "def-size", 2048, "def-step", 1024,
		"hiquality", TRUE, NULL);
  moodbar = make_element ("moodbar", "moodbar");
  g_object_set (G_OBJECT (moodbar), "height", 1, NULL);
  g_object_set (G_OBJECT (moodbar), "max-width", 1000, NULL);
  sink = make_element ("filesink", "sink");
  g_object_set (G_OBJECT (sink), "location", outfile, NULL);

  gst_bin_add_many (GST_BIN (audio), conv, fft, moodbar, sink, NULL);
  gst_element_link_many (conv, fft, moodbar, sink, NULL);
  gst_element_add_pad (audio, gst_ghost_pad_new ("sink", audiopad));
  gst_object_unref (audiopad);
  gst_bin_add (GST_BIN (pipeline), audio);
  
  g_signal_connect (decoder, "new-decoded-pad", 
		    G_CALLBACK (cb_newpad), audio);
  g_signal_connect (decoder, "unknown-type", 
		    G_CALLBACK (cb_cantdecode), loop);

  /* run */
  g_print ("Analyzing file %s\n", infile);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  g_main_loop_run (loop);

  /* cleanup */
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (pipeline));
}


gint
main (gint argc, gchar *argv[])
{
  gint tries;

  /* Command-line parsing */
  gchar *outfile = NULL, *infile = NULL;
  gchar **array = NULL;
  const GOptionEntry entries[] = 
    {
      { "output", 'o', 0, G_OPTION_ARG_FILENAME, &outfile,
	"The output .mood file", NULL },
      { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &array,
	"The file to analyze", NULL },
      { NULL, '\0', 0, 0, NULL, NULL, NULL }
    };
  GOptionContext *ctx;
  GError *err = NULL;

  ctx = g_option_context_new ("[INFILE] - Run moodbar analyzer");
  g_option_context_add_group (ctx, gst_init_get_option_group ());
  g_option_context_add_main_entries (ctx, entries, NULL);

  if (!g_option_context_parse (ctx, &argc, &argv, &err)) 
    {
      g_print ("Error initializing: %s\n", GST_STR_NULL (err->message));
      return RETURN_COMMANDLINE;
    }
  g_option_context_free (ctx);

  if (outfile == NULL) 
    {
      g_print ("Please specify an output .mood file\n\n");
      return RETURN_COMMANDLINE;
    }
  output_file = outfile;

  if (array == NULL  ||  *array == NULL) 
    {
      g_print ("Please specify a file to analyze\n\n");
      return RETURN_COMMANDLINE;
    }
  else
    infile = *array;


  gst_init (&argc, &argv);


  /* Each time the analyzer loop is run, check if the file has been
   * modified; if so, try again.  This prevents metadata updates from
   * screwing up the analyzer.
   */
  for (tries = 0; tries < MAX_TRIES; ++tries)
    {
      struct stat filestats;
      time_t oldtime;

      if (stat (infile, &filestats) == -1)
        return RETURN_NOFILE;
      oldtime = filestats.st_mtime;

      run_loop (infile, outfile);

      if (stat (infile, &filestats) != -1  &&  filestats.st_mtime == oldtime)
        return return_val;
    }


  /* If we get here, that means that the file was modified MAX_TRIES
   * times while we were analyzing; give up.
   */
  return RETURN_NOFILE;
}
