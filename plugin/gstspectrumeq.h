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

#ifndef __GST_SPECTRUMEQ_H__
#define __GST_SPECTRUMEQ_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS


typedef enum
{
  GST_SPECTRUM_PRESET_LOW = 1,
  GST_SPECTRUM_PRESET_MED,
  GST_SPECTRUM_PRESET_HIGH
} GstSpectrumEqPresets;


#define GST_TYPE_SPECTRUMEQ \
  (gst_spectrumeq_get_type())
#define GST_SPECTRUMEQ(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SPECTRUMEQ,GstSpectrumEq))
#define GST_SPECTRUMEQ_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SPECTRUMEQ,GstSpectrumEqClass))
#define GST_IS_SPECTRUMEQ(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SPECTRUMEQ))
#define GST_IS_SPECTRUMEQ_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SPECTRUMEQ))

typedef struct _GstSpectrumEq      GstSpectrumEq;
typedef struct _GstSpectrumEqClass GstSpectrumEqClass;

struct _GstSpectrumEq {
  GstBaseTransform element;

  /* Scale factors for the different bands (set by a property) */
  gfloat *bands;
  guint   numbands;

  /* The number of complex numbers in each buffer */
  guint numfreqs;
};

struct _GstSpectrumEqClass {
  GstBaseTransformClass parent_class;
};

GType gst_spectrumeq_get_type (void);

G_END_DECLS

#endif /* __GST_SPECTRUMEQ_H__ */
