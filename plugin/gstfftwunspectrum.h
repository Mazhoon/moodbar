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


#ifndef __GST_FFTWUNSPECTRUM_H__
#define __GST_FFTWUNSPECTRUM_H__

#include <gst/gst.h>
#include <fftw3.h>

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_FFTWUNSPECTRUM \
  (gst_fftwunspectrum_get_type())
#define GST_FFTWUNSPECTRUM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_FFTWUNSPECTRUM,GstFFTWUnSpectrum))
#define GST_FFTWUNSPECTRUM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_FFTWUNSPECTRUM,GstFFTWUnSpectrumClass))

typedef struct _GstFFTWUnSpectrum      GstFFTWUnSpectrum;
typedef struct _GstFFTWUnSpectrumClass GstFFTWUnSpectrumClass;

struct _GstFFTWUnSpectrum
{
  GstElement element;

  GstPad *sinkpad, *srcpad;

  /* Stream data */
  gint rate, size, step;

  /* This is used to store samples for which there is overlapping 
   * spectrum data (when size > step) */
  gfloat *extra_samples;

  /* State data for fftw */
  float      *fftw_in;
  float      *fftw_out;
  fftwf_plan  fftw_plan;

  /* Property */
  gboolean hi_q;
};

struct _GstFFTWUnSpectrumClass 
{
  GstElementClass parent_class;
};

GType gst_fftwunspectrum_get_type (void);

G_END_DECLS

#endif /* __GST_FFTWUNSPECTRUM_H__ */
