/* GStreamer FFTW-based spectrum-to-signal converter
 * Copyright (C) 2006 Joseph Rabinoff <bobqwatson@yahoo.com>
 */

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/**
 * SECTION:element-fftwunspectrum
 *
 * <refsect2>
 * <title>Example launch line</title>
 * <para>
 * <programlisting>
 * gst-launch audiotestsrc ! audioconvert ! fftwspectrum ! fftwunspectrum ! audioconvert ! alsasink
 * </programlisting>
 * </para>
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <fftw3.h>
#include <string.h>
#include <math.h>

#include "gstfftwunspectrum.h"
#include "spectrum.h"

GST_DEBUG_CATEGORY (gst_fftwunspectrum_debug);
#define GST_CAT_DEFAULT gst_fftwunspectrum_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  ARG_0,
  ARG_HIQUALITY
};

#define HIQUALITY_DEFAULT TRUE

static GstStaticPadTemplate sink_factory 
  = GST_STATIC_PAD_TEMPLATE ("sink",
			     GST_PAD_SINK,
			     GST_PAD_ALWAYS,
			     GST_STATIC_CAPS 
			       ( SPECTRUM_FREQ_CAPS )
			     );

static GstStaticPadTemplate src_factory 
  = GST_STATIC_PAD_TEMPLATE ("src",
			     GST_PAD_SRC,
			     GST_PAD_ALWAYS,
			     GST_STATIC_CAPS
			       ( SPECTRUM_SIGNAL_CAPS )
			     );

GST_BOILERPLATE (GstFFTWUnSpectrum, gst_fftwunspectrum, GstElement,
    GST_TYPE_ELEMENT);

static void gst_fftwunspectrum_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec);
static void gst_fftwunspectrum_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec);

static gboolean gst_fftwunspectrum_set_sink_caps (GstPad *pad, GstCaps *caps);
static GstCaps *gst_fftwunspectrum_getcaps (GstPad *pad);

static GstFlowReturn gst_fftwunspectrum_chain (GstPad *pad, GstBuffer *buf);
static GstStateChangeReturn gst_fftwunspectrum_change_state 
                              (GstElement *element, GstStateChange transition);


#define INPUT_SIZE(conv) (((conv)->size/2+1)*sizeof(fftwf_complex))
#define NUM_EXTRA_SAMPLES(conv) ((conv)->size - (conv)->step)


/***************************************************************/
/* GObject boilerplate stuff                                   */
/***************************************************************/


static void
gst_fftwunspectrum_base_init (gpointer gclass)
{
  static GstElementDetails element_details = 
    {
      "FFTW-based Inverse Fourier transform",
      "Filter/Converter/Spectrum",
      "Convert a frequency spectrum stream into a raw audio stream",
      "Joe Rabinoff <bobqwatson@yahoo.com>"
    };
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
  gst_element_class_set_details (element_class, &element_details);
}

/* initialize the plugin's class */
static void
gst_fftwunspectrum_class_init (GstFFTWUnSpectrumClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_fftwunspectrum_set_property;
  gobject_class->get_property = gst_fftwunspectrum_get_property;

  g_object_class_install_property (gobject_class, ARG_HIQUALITY,
      g_param_spec_boolean ("hiquality", "High Quality", 
	  "Use a more time-consuming, higher quality algorithm chooser",
	  HIQUALITY_DEFAULT, G_PARAM_READWRITE));

  gstelement_class->change_state 
    = GST_DEBUG_FUNCPTR (gst_fftwunspectrum_change_state);
}

/* initialize the new element
 * instantiate pads and add them to element
 * set functions
 * initialize structure
 */
static void
gst_fftwunspectrum_init (GstFFTWUnSpectrum * conv,
			 GstFFTWUnSpectrumClass * gclass)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (conv);

  conv->sinkpad =
      gst_pad_new_from_template 
          (gst_element_class_get_pad_template (klass, "sink"), "sink");
  gst_pad_set_setcaps_function (conv->sinkpad, 
      GST_DEBUG_FUNCPTR (gst_fftwunspectrum_set_sink_caps));
  gst_pad_set_getcaps_function (conv->sinkpad,
      GST_DEBUG_FUNCPTR (gst_fftwunspectrum_getcaps));
  gst_pad_set_chain_function (conv->sinkpad, 
      GST_DEBUG_FUNCPTR (gst_fftwunspectrum_chain));

  conv->srcpad =
      gst_pad_new_from_template 
          (gst_element_class_get_pad_template (klass, "src"), "src");
  gst_pad_set_getcaps_function (conv->srcpad,
      GST_DEBUG_FUNCPTR (gst_fftwunspectrum_getcaps));


  gst_element_add_pad (GST_ELEMENT (conv), conv->sinkpad);
  gst_element_add_pad (GST_ELEMENT (conv), conv->srcpad);


  /* These are set once the (sink) capabilities are determined */
  conv->rate = 0;
  conv->size = 0;
  conv->step = 0;
  conv->extra_samples = NULL;

  /* These are set when we change to READY */
  conv->fftw_in   = NULL;
  conv->fftw_out  = NULL;
  conv->fftw_plan = NULL;

  /* Parameters */
  conv->hi_q     = HIQUALITY_DEFAULT;
}

static void
gst_fftwunspectrum_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFFTWUnSpectrum *conv = GST_FFTWUNSPECTRUM (object);

  switch (prop_id) 
    {
    case ARG_HIQUALITY:
      conv->hi_q = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gst_fftwunspectrum_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstFFTWUnSpectrum *conv = GST_FFTWUNSPECTRUM (object);

  switch (prop_id) 
    {
    case ARG_HIQUALITY:
      g_value_set_boolean (value, conv->hi_q);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


/* Allocate and deallocate fftw state data */
static void
free_fftw_data (GstFFTWUnSpectrum *conv)
{
  if(conv->fftw_plan != NULL)
    fftwf_destroy_plan (conv->fftw_plan);
  if(conv->fftw_in != NULL)
    fftwf_free (conv->fftw_in);
  if(conv->fftw_out != NULL)
    fftwf_free (conv->fftw_out);

  conv->fftw_in   = NULL;
  conv->fftw_out  = NULL;
  conv->fftw_plan = NULL;
}


static void
alloc_fftw_data (GstFFTWUnSpectrum *conv)
{
  free_fftw_data (conv);

  conv->fftw_in  = (float *) fftwf_malloc (INPUT_SIZE (conv));
  conv->fftw_out = (float *) fftwf_malloc (sizeof(float) * conv->size);
  
  conv->fftw_plan 
    = fftwf_plan_dft_c2r_1d(conv->size, (fftwf_complex *) conv->fftw_in, 
			    conv->fftw_out, 
			    conv->hi_q ? FFTW_MEASURE : FFTW_ESTIMATE);
}


/* Allocate and deallocate the extra samples */

static void
free_extra_samples (GstFFTWUnSpectrum *conv)
{
  if (conv->extra_samples != NULL)
    g_free (conv->extra_samples);

  conv->extra_samples = NULL;
}


static void
alloc_extra_samples (GstFFTWUnSpectrum *conv)
{
  free_extra_samples (conv);

  if(NUM_EXTRA_SAMPLES (conv) > 0)
    conv->extra_samples 
      = (gfloat *) g_malloc (NUM_EXTRA_SAMPLES (conv) * sizeof (gfloat));
}


/***************************************************************/
/* Capabilities negotiation                                    */
/***************************************************************/

/* The input and output capabilities are only related by the "rate"
 * parameter, which has been propagated so that this element can
 * reconstruct an audio signal.  This module does no rate conversion.
 *
 * The size and step parameters need to be given to the sink when
 * negotiating caps, as well as the rate.  The rate is passed on
 * to the source pad.
 * 
 * This module does not support upstream caps renegotiation.  The
 * only negotiable part of the source caps is the rate, and that is
 * set by the sink.  Hence we do not need a setcaps function on 
 * the source; if upstream renegotiation is attempted, by default
 * it will fail unless the source already has the correct caps
 * set.
 */

static gboolean
gst_fftwunspectrum_set_sink_caps (GstPad *pad, GstCaps *caps)
{
  GstFFTWUnSpectrum *conv;
  GstCaps *srccaps, *newsrccaps;
  GstStructure *newstruct;
  gint rate, size, step;
  gboolean res;

  conv = GST_FFTWUNSPECTRUM (gst_pad_get_parent (pad));

  srccaps = gst_pad_get_allowed_caps (conv->srcpad);
  newsrccaps = gst_caps_copy_nth (srccaps, 0);
  gst_caps_unref (srccaps);

  /* Decoding size < step not implemented */
  newstruct = gst_caps_get_structure (caps, 0);
  if (!gst_structure_get_int (newstruct, "rate", &rate) || 
      !gst_structure_get_int (newstruct, "size", &size) ||
      !gst_structure_get_int (newstruct, "step", &step) ||
      size < step)
    {
      gst_caps_unref (newsrccaps);
      gst_object_unref (conv);
      return FALSE;
    }

  /* This gets rid of all ambiguity so should fixate */
  gst_caps_set_simple (newsrccaps, "rate", G_TYPE_INT, rate, NULL);
  res = gst_pad_set_caps (conv->srcpad, newsrccaps);

  if (G_LIKELY (res))
    {
      conv->rate = rate;
      conv->size = size;
      conv->step = step;
      
      /* Re-allocate the fftw data */
      if (GST_STATE (GST_ELEMENT (conv)) >= GST_STATE_READY)
	alloc_fftw_data (conv);

      /* Re-allocate the stream data */
      if (GST_STATE (GST_ELEMENT (conv)) >= GST_STATE_PAUSED)
	alloc_extra_samples (conv);
    }
  
  gst_caps_unref (newsrccaps);
  gst_object_unref (conv);

  return res;
}

/* The only thing that can constrain the caps is the rate. */ 
static GstCaps *
gst_fftwunspectrum_getcaps (GstPad *pad)
{
  GstFFTWUnSpectrum *conv;
  GstCaps *tmplcaps;

  conv = GST_FFTWUNSPECTRUM (gst_pad_get_parent (pad));
  tmplcaps = gst_caps_copy (gst_pad_get_pad_template_caps (pad));
  
  if(conv->rate != 0)
    {
      /* Assumes the template caps are simple */
      gst_caps_set_simple (tmplcaps, "rate", G_TYPE_INT, conv->rate, NULL);
    }

  gst_object_unref (conv);  

  return tmplcaps;
}


/***************************************************************/
/* Actual conversion                                           */
/***************************************************************/


static GstStateChangeReturn 
gst_fftwunspectrum_change_state (GstElement * element,
			       GstStateChange transition)
{
  GstFFTWUnSpectrum *conv = GST_FFTWUNSPECTRUM (element);
  GstStateChangeReturn res;

  switch (transition)
    {
    case GST_STATE_CHANGE_NULL_TO_READY:
      alloc_fftw_data (conv);
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      alloc_extra_samples (conv);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
    }

  res = parent_class->change_state (element, transition);

  switch (transition) 
    {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:      
      free_extra_samples (conv);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      free_fftw_data (conv);
      break;
    default:
      break;
    }

  return res;
}


static GstFlowReturn
gst_fftwunspectrum_chain (GstPad * pad, GstBuffer * buf)
{
  GstFFTWUnSpectrum *conv;
  GstBuffer *outbuf;
  GstFlowReturn res = GST_FLOW_OK;

  conv = GST_FFTWUNSPECTRUM (gst_pad_get_parent (pad));

  /* Pedantry */
  if (GST_BUFFER_SIZE (buf) != INPUT_SIZE (conv))
    return GST_FLOW_ERROR;

  res = gst_pad_alloc_buffer_and_set_caps 
    (conv->srcpad, GST_BUFFER_OFFSET (buf), conv->step * sizeof (gfloat), 
     GST_PAD_CAPS(conv->srcpad), &outbuf);
  if (res != GST_FLOW_OK)
    goto out;
      
  GST_BUFFER_SIZE       (outbuf) = conv->step * sizeof (gfloat);
  GST_BUFFER_OFFSET     (outbuf) = GST_BUFFER_OFFSET     (buf);
  GST_BUFFER_OFFSET_END (outbuf) = GST_BUFFER_OFFSET_END (buf);
  GST_BUFFER_TIMESTAMP  (outbuf) = GST_BUFFER_TIMESTAMP  (buf);
  GST_BUFFER_DURATION   (outbuf) = GST_BUFFER_DURATION   (buf);
      
  /* Do the Fourier transform */
  memcpy (conv->fftw_in, GST_BUFFER_DATA (buf), INPUT_SIZE (conv));
  fftwf_execute (conv->fftw_plan);
  { /* Normalize */
    gint i;
    gfloat root = sqrtf (conv->size);
    for (i = 0; i < conv->size; ++i)
      conv->fftw_out[i] /= root;
  }

  /* Average with overlap sample data */
  if (NUM_EXTRA_SAMPLES (conv) > 0)
    {
      gint i, num_others, extra = NUM_EXTRA_SAMPLES (conv);
      gfloat *out = (gfloat *) GST_BUFFER_DATA (outbuf);
      gfloat start_weight, end_weight, weight, pct;

      /* Average the input data with the overlap.  This code is kind
       * of complicated, since there may be more than one buffer that
       * overlaps the same sample, so we may need to take a _weighted_
       * average of our sample with the stored one.  Plus we linearly
       * interpolate this weighted average so that the new data has
       * its full weight at the end of the overlap data and has zero
       * weight at the beginning -- this is to smooth transitions.
       * The end result doesn't have perfect mathematical properties,
       * but is a good approximation and sounds just fine.
       */

      /* GST_LOG ("Starting interpolation..."); */

      for (i = 0; i < extra; ++i)
	{
	  /* The number of samples this has already been averaged
	   * against so far. */
	  num_others = ((extra - i - 1) / conv->step) + 1;

	  if (num_others == ((extra - 1) / conv->step) + 1)
	    start_weight = 0.f;
	  else
	    start_weight 
	      = (1.f / ((gfloat) num_others) 
		 + 1.f / (((gfloat) num_others) + 1.f)) / 2.f;

	  if (num_others == 1)
	    end_weight = 1.f;
	  else
	    end_weight 
	      = (1.f / ((gfloat) num_others) 
		 + 1.f / (((gfloat) num_others) - 1.f)) / 2.f;
	  
	  /* This is the percentage of the way to the next time 
	   * num_others changes */
	  pct = ((gfloat) (i - MAX (extra - num_others * conv->step, 0)))
	    / ((gfloat) (extra - (num_others-1) * conv->step 
			 - MAX (extra - num_others * conv->step, 0)));
	  weight = start_weight * (1-pct) + end_weight * pct;

	  /* GST_LOG ("%d = pct: %f -> weight: %f", i, pct, weight); */

	  conv->extra_samples[i] 
	    = (conv->fftw_out[i] * weight + 
	       conv->extra_samples[i] * (1-weight));
	}

      /* Copy the overlap part of the data */
      memcpy (out, conv->extra_samples,
	      MIN (extra, conv->step) * sizeof (gfloat));

      /* Now copy the non-overlap part of the data if applicable */
      if (conv->step > extra)
	memcpy (&out[extra], &conv->fftw_out[extra], 
		(conv->step - extra) * sizeof (gfloat));
      
      /* Move extra_data over and copy the end of our data in */
      for (i = 0; i < extra - conv->step; ++i)  /* May run 0 times */
	conv->extra_samples[i] = conv->extra_samples[i + conv->step];

      memcpy (&conv->extra_samples[MAX (extra - conv->step, 0)],
	      &conv->fftw_out[conv->size - MIN (extra, conv->step)],
	      MIN (extra, conv->step) * sizeof (gfloat));
    }

  else  /* conv->size == conv->step */
    memcpy (GST_BUFFER_DATA (outbuf), conv->fftw_out, 
	    conv->size * sizeof (gfloat));
  
  res = gst_pad_push (conv->srcpad, outbuf);

out:
  gst_buffer_unref (buf);
  gst_object_unref (conv);

  return res;
}


