/* GStreamer spectrum-based equalizer
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
 * SECTION:element-spectrumeq
 *
 * <refsect2>
 * <title>Example launch line</title>
 * <para>
 * The spectrumeq element scales the amplitudes of the bands in a spectrum.
 * It is recommended to have the size of the input spectra be twice the 
 * value of the step, since the reconstituted signal is rather rough
 * without some overlap.  I also recommend not using scale factors > 1.0,
 * since that tends to max the signal and produce artifacts.
 * </para>
 * <para>
 * <programlisting>
 * gst-launch audiotestsrc ! audioconvert ! fftwspectrum def-size=1024 def-step=512 ! spectrumeq preset=lowpreset ! fftwunspectrum ! audioconvert ! alsasink
 * </programlisting>
 * </para>
 * </refsect2>
 */

/* TODO: implement an interface and a control like the volume plugin */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <string.h>
#include <math.h>

#include "gstspectrumeq.h"
#include "spectrum.h"

#define GST_CAT_DEFAULT gst_spectrumeq_debug
GST_DEBUG_CATEGORY (gst_spectrumeq_debug);


/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  ARG_0,
  ARG_EQUALIZER,
  ARG_PRESET
};

static GstStaticPadTemplate spectrumeq_sink_template 
  = GST_STATIC_PAD_TEMPLATE ("sink",
			     GST_PAD_SINK,
			     GST_PAD_ALWAYS,
			     GST_STATIC_CAPS 
			       ( SPECTRUM_FREQ_CAPS )
			     );

static GstStaticPadTemplate spectrumeq_src_template 
  = GST_STATIC_PAD_TEMPLATE ("src",
			     GST_PAD_SRC,
			     GST_PAD_ALWAYS,
			     GST_STATIC_CAPS 
			       ( SPECTRUM_FREQ_CAPS )
			     );

GST_BOILERPLATE (GstSpectrumEq, gst_spectrumeq, GstBaseTransform,
    GST_TYPE_BASE_TRANSFORM);

static void spectrumeq_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec);
static void spectrumeq_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec);

static GstFlowReturn spectrumeq_transform_ip (GstBaseTransform *base,
    GstBuffer *outbuf);
static gboolean spectrumeq_set_caps (GstBaseTransform *base, GstCaps *incaps,
    GstCaps *outcaps);


/* Presets -- these are different sections of a Gaussian */

static const gfloat preset_low[] =
  { 
    1.0000000f,
    0.99502491f,
    0.98039471f,
    0.95696559f,
    0.92607189f,
    0.88940039f,
    0.84883816f,
    0.80631319f,
    0.76364621f,
    0.72242903f,
    0.68393972f,
    0.64909863f,
    0.61846387f,
    0.59225976f,
    0.57042921f,
    0.55269961f,
    0.53865237f,
    0.52778810f,
    0.51958194f,
    0.51352592f,
    0.50915781f,
  };

static const gfloat preset_med[] =
  {
    0.68393972f,
    0.72242903f,
    0.76364621f,
    0.80631319f,
    0.84883816f,
    0.88940039f,
    0.92607189f,
    0.95696559f,
    0.98039471f,
    0.99502491f,
    1.0000000f,
    0.99502491f,
    0.98039471f,
    0.95696559f,
    0.92607189f,
    0.88940039f,
    0.84883816f,
    0.80631319f,
    0.76364621f,
    0.72242903f,
    0.68393972f,
  };

static const gfloat preset_high[] =
  {
    0.50915781f,
    0.51352592f,
    0.51958194f,
    0.52778810f,
    0.53865237f,
    0.55269961f,
    0.57042921f,
    0.59225976f,
    0.61846387f,
    0.64909863f,
    0.68393972f,
    0.72242903f,
    0.76364621f,
    0.80631319f,
    0.84883816f,
    0.88940039f,
    0.92607189f,
    0.95696559f,
    0.98039471f,
    0.99502491f,
    1.0000000f,
  };



/***************************************************************/
/* GObject boilerplate stuff                                   */
/***************************************************************/

#define GST_TYPE_SPECTRUMEQ_PRESETS (spectrumeq_presets_get_type())

static GType
spectrumeq_presets_get_type (void)
{
  static GType type = 0;
  static const GEnumValue presets[] =
    {
      { GST_SPECTRUM_PRESET_LOW,  "Low preset",    "lowpreset" },
      { GST_SPECTRUM_PRESET_MED,  "Medium preset", "mediumpreset" },
      { GST_SPECTRUM_PRESET_HIGH, "High preset",   "highpreset" },
      { 0, NULL, NULL},
    };

  if (!type)
    type = g_enum_register_static ("GstSpectrumEqPresets", presets);

  return type;
}


static void
spectrumeq_dispose (GObject *object)
{
  GstSpectrumEq *spec = GST_SPECTRUMEQ (object);
  
  if(spec->bands)
    g_free (spec->bands);

  spec->bands    = NULL;
  spec->numbands = 0;

  G_OBJECT_CLASS (parent_class)->dispose (object);
}


static void
gst_spectrumeq_base_init (gpointer g_class)
{
  static GstElementDetails spectrumeq_details =
    {
      "Multi-band Spectrum-space Equalizer",
      "Filter/Effect/Audio",
      "Scale amplitudes of bands of a spectrum",
      "Joe Rabinoff <bobqwatson@yahoo.com>"
    };
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&spectrumeq_src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&spectrumeq_sink_template));
  gst_element_class_set_details (element_class, &spectrumeq_details);
}


static void
gst_spectrumeq_class_init (GstSpectrumEqClass *klass)
{
  GObjectClass *gobject_class;
  GstBaseTransformClass *trans_class;

  gobject_class = (GObjectClass *) klass;
  trans_class = (GstBaseTransformClass *) klass;

  gobject_class->set_property = spectrumeq_set_property;
  gobject_class->get_property = spectrumeq_get_property;
  gobject_class->dispose      = spectrumeq_dispose;

  g_object_class_install_property (gobject_class, ARG_EQUALIZER,
      g_param_spec_value_array ("equalizer", "Equalizer", 
          "An arbitrary number of (equally spaced) band scale factors",
	  g_param_spec_float ("scalefactor", "Scale Factor", 
              "The scale factor for the current band", 
	      0.0f, 1.0e10f, 1.0f, G_PARAM_READWRITE),
	  G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, ARG_PRESET,
      g_param_spec_enum ("preset", "Preset", "Preset equalizer settings",
	  GST_TYPE_SPECTRUMEQ_PRESETS, GST_SPECTRUM_PRESET_MED, 
          G_PARAM_WRITABLE));

  trans_class->transform_ip = GST_DEBUG_FUNCPTR (spectrumeq_transform_ip);
  trans_class->set_caps = GST_DEBUG_FUNCPTR (spectrumeq_set_caps);

  trans_class->passthrough_on_same_caps = FALSE;
}


static void
gst_spectrumeq_init (GstSpectrumEq *spec, GstSpectrumEqClass *g_class)
{
  GstBaseTransform *trans = GST_BASE_TRANSFORM (spec);

  /* Set these for clarity (they should be the defaults) */
  trans->passthrough = FALSE;
  trans->always_in_place = TRUE;

  /* By default there is only one band scaled at 1.0 */
  spec->bands = (gfloat *) g_malloc (1 * sizeof (gfloat));
  spec->bands[0] = 1.0;
  spec->numbands = 1;

  spec->numfreqs = 0;
}


static void
spectrumeq_set_property (GObject *object, guint prop_id, const GValue *value,
			 GParamSpec *pspec)
{
  GstSpectrumEq *spec = GST_SPECTRUMEQ (object);
  GValueArray *array;
  GValue *val;
  guint i;

  switch (prop_id)
    {
    case ARG_EQUALIZER:
      /* spec->bands should ALWAYS be allocated here */
      g_free (spec->bands);
      array = (GValueArray *) g_value_get_boxed (value);

      /* It doesn't make sense to have zero bands */
      if (array->n_values == 0)
	{
	  spec->bands = (gfloat *) g_malloc (1 * sizeof (gfloat));
	  spec->bands[0] = 1.0;
	  spec->numbands = 1;
	}

      else
	{
	  spec->numbands = array->n_values;
	  spec->bands = (gfloat *) g_malloc (spec->numbands * sizeof (gfloat));

	  for (i = 0; i < spec->numbands; ++i)
	    {
	      val = g_value_array_get_nth (array, i);
	      spec->bands[i] = g_value_get_float (val);
	    }
	}

      break;
    case ARG_PRESET:
      {
	GstSpectrumEqPresets val 
	  = (GstSpectrumEqPresets) g_value_get_enum (value);
	guint size;
	const gfloat *bands;
	
	switch (val)
	  {
	  case GST_SPECTRUM_PRESET_LOW:
	    bands = preset_low;
	    size  = sizeof (preset_low) / sizeof (gfloat);
	    break;
	  case GST_SPECTRUM_PRESET_MED:
	    bands = preset_med;
	    size  = sizeof (preset_med) / sizeof (gfloat);
	    break;
	  case GST_SPECTRUM_PRESET_HIGH:
	    bands = preset_high;
	    size  = sizeof (preset_high) / sizeof (gfloat);
	    break;
	  default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	    return;
	  };
	
	/* spec->bands should ALWAYS be allocated here */
	g_free (spec->bands);
	spec->numbands = size;
	spec->bands = (gfloat *) g_malloc (spec->numbands * sizeof (gfloat));
	memcpy (spec->bands, bands, spec->numbands * sizeof (gfloat));
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
spectrumeq_get_property (GObject *object, guint prop_id, GValue *value,
			 GParamSpec *pspec)
{
  GstSpectrumEq *spec = GST_SPECTRUMEQ (object);
  GValueArray *array;
  GValue val;
  guint i;

  switch (prop_id) 
    {
    case ARG_EQUALIZER:
      array = g_value_array_new (spec->numbands);
      for (i = 0; i < spec->numbands; ++i)
	{
	  memset (&val, 0, sizeof (GValue));
	  g_value_init (&val, G_TYPE_FLOAT);
	  g_value_set_float (&val, spec->bands[i]);
	  array = g_value_array_append (array, &val);
	}
      g_value_take_boxed (value, array);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


/***************************************************************/
/* Capabilities negotiation                                    */
/***************************************************************/

/* The incaps and outcaps should be the same here, since the default
 * transform_caps returns that the caps on both pads should be the same.
 */
static gboolean
spectrumeq_set_caps (GstBaseTransform *base, GstCaps *incaps, GstCaps *outcaps)
{
  GstSpectrumEq *spec = GST_SPECTRUMEQ (base);
  GstStructure *s;
  gint size;

  GST_DEBUG_OBJECT (spec,
    "set_caps: in %" GST_PTR_FORMAT " out %" GST_PTR_FORMAT, incaps, outcaps);

  spec->numfreqs = 0;

  if (!gst_caps_is_equal (incaps, outcaps))
    return FALSE;

  s = gst_caps_get_structure (incaps, 0);
  if (!gst_structure_get_int (s, "size", &size))
    return FALSE;

  spec->numfreqs = (guint) (size / 2 + 1);

  return TRUE;
}


/***************************************************************/
/* Actual processing                                           */
/***************************************************************/

static GstFlowReturn
spectrumeq_transform_ip (GstBaseTransform *base, GstBuffer *outbuf)
{
  GstSpectrumEq *spec = GST_SPECTRUMEQ (base);
  gfloat *data;
  guint i;

  /* Pedantry */
  if (GST_BUFFER_SIZE (outbuf) != spec->numfreqs * sizeof (gfloat) * 2)
    return GST_FLOW_ERROR;
  
  data = (gfloat *) GST_BUFFER_DATA (outbuf);

  for (i = 0; i < spec->numfreqs; ++i)
    {
      gfloat pct, band, prevband, scalefactor;

      if (spec->numbands == 1)
	scalefactor = spec->bands[0];

      else
	{
	  /* Do a simple linear interpolation between bands to get the
	   * amplitude scale factor */
	  pct      = ((gfloat) i) / ((gfloat) spec->numfreqs);
	  band     = pct * ((gfloat) (spec->numbands-1));
	  prevband = floorf (band);
	  
	  if(((guint) prevband) >= spec->numbands - 1)
	    scalefactor = spec->bands[spec->numbands - 1];

	  else
	    scalefactor = (band - prevband) * spec->bands[((guint) prevband)+1]
	           + (1 - (band - prevband)) * spec->bands[(guint) prevband];

	}

      *(++data) *= scalefactor;
      *(++data) *= scalefactor;
    } 

  return GST_FLOW_OK;
}

