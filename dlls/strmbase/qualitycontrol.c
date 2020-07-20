/*
 * Quality Control Interfaces
 *
 * Copyright 2010 Maarten Lankhorst for CodeWeavers
 *
 * rendering qos functions based on, the original can be found at
 * gstreamer/libs/gst/base/gstbasesink.c which has copyright notice:
 *
 * Copyright (C) 2005-2007 Wim Taymans <wim.taymans@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "strmbase_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(strmbase_qc);

static inline struct strmbase_qc *impl_from_IQualityControl(IQualityControl *iface)
{
    return CONTAINING_RECORD(iface, struct strmbase_qc, IQualityControl_iface);
}

static HRESULT WINAPI quality_control_QueryInterface(IQualityControl *iface, REFIID riid, void **ppv)
{
    struct strmbase_qc *This = impl_from_IQualityControl(iface);
    return IBaseFilter_QueryInterface(&This->pin->filter->IBaseFilter_iface, riid, ppv);
}

static ULONG WINAPI quality_control_AddRef(IQualityControl *iface)
{
    struct strmbase_qc *This = impl_from_IQualityControl(iface);
    return IBaseFilter_AddRef(&This->pin->filter->IBaseFilter_iface);
}

static ULONG WINAPI quality_control_Release(IQualityControl *iface)
{
    struct strmbase_qc *This = impl_from_IQualityControl(iface);
    return IBaseFilter_Release(&This->pin->filter->IBaseFilter_iface);
}

static HRESULT WINAPI quality_control_Notify(IQualityControl *iface, IBaseFilter *sender, Quality qm)
{
    struct strmbase_qc *This = impl_from_IQualityControl(iface);
    HRESULT hr = S_FALSE;

    TRACE("iface %p, sender %p, type %#x, proportion %u, late %s, timestamp %s.\n",
        iface, sender, qm.Type, qm.Proportion, debugstr_time(qm.Late), debugstr_time(qm.TimeStamp));

    if (This->tonotify)
        return IQualityControl_Notify(This->tonotify, &This->pin->filter->IBaseFilter_iface, qm);

    if (This->pin->peer)
    {
        IQualityControl *qc = NULL;
        IPin_QueryInterface(This->pin->peer, &IID_IQualityControl, (void **)&qc);
        if (qc)
        {
            hr = IQualityControl_Notify(qc, &This->pin->filter->IBaseFilter_iface, qm);
            IQualityControl_Release(qc);
        }
    }

    return hr;
}

static HRESULT WINAPI quality_control_SetSink(IQualityControl *iface, IQualityControl *tonotify)
{
    struct strmbase_qc *This = impl_from_IQualityControl(iface);
    TRACE("%p %p\n", This, tonotify);
    This->tonotify = tonotify;
    return S_OK;
}

static const IQualityControlVtbl quality_control_vtbl =
{
    quality_control_QueryInterface,
    quality_control_AddRef,
    quality_control_Release,
    quality_control_Notify,
    quality_control_SetSink,
};

/* Macros copied from gstreamer, weighted average between old average and new ones */
#define DO_RUNNING_AVG(avg,val,size) (((val) + ((size)-1) * (avg)) / (size))

/* generic running average, this has a neutral window size */
#define UPDATE_RUNNING_AVG(avg,val)   DO_RUNNING_AVG(avg,val,8)

/* the windows for these running averages are experimentally obtained.
 * positive values get averaged more while negative values use a small
 * window so we can react faster to badness. */
#define UPDATE_RUNNING_AVG_P(avg,val) DO_RUNNING_AVG(avg,val,16)
#define UPDATE_RUNNING_AVG_N(avg,val) DO_RUNNING_AVG(avg,val,4)

void QualityControlRender_Start(struct strmbase_qc *This, REFERENCE_TIME tStart)
{
    This->avg_render = This->last_in_time = This->last_left = This->avg_duration = This->avg_pt = -1;
    This->clockstart = tStart;
    This->avg_rate = -1.0;
    This->rendered = This->dropped = 0;
    This->is_dropped = FALSE;
    This->qos_handled = TRUE; /* Lie that will be corrected on first adjustment */
}

static BOOL QualityControlRender_IsLate(struct strmbase_qc *This, REFERENCE_TIME jitter,
                                        REFERENCE_TIME start, REFERENCE_TIME stop)
{
    REFERENCE_TIME max_lateness = 200000;

    TRACE("jitter %s, start %s, stop %s.\n", debugstr_time(jitter),
            debugstr_time(start), debugstr_time(stop));

    /* we can add a valid stop time */
    if (stop >= start)
        max_lateness += stop;
    else
        max_lateness += start;

    /* if the jitter bigger than duration and lateness we are too late */
    if (start + jitter > max_lateness) {
        WARN("buffer is too late %i > %i\n", (int)((start + jitter)/10000),  (int)(max_lateness/10000));
        /* !!emergency!!, if we did not receive anything valid for more than a
         * second, render it anyway so the user sees something */
        if (This->last_in_time < 0 ||
            start - This->last_in_time < 10000000)
            return TRUE;
        FIXME("A lot of buffers are being dropped.\n");
        FIXME("There may be a timestamping problem, or this computer is too slow.\n");
    }
    This->last_in_time = start;
    return FALSE;
}

void QualityControlRender_DoQOS(struct strmbase_qc *priv)
{
    REFERENCE_TIME start, stop, jitter, pt, entered, left, duration;
    double rate;

    TRACE("%p\n", priv);

    if (!priv->pin->filter->clock || priv->current_rstart < 0)
        return;

    start = priv->current_rstart;
    stop = priv->current_rstop;
    jitter = priv->current_jitter;

    if (jitter < 0) {
        /* this is the time the buffer entered the sink */
        if (start < -jitter)
            entered = 0;
        else
            entered = start + jitter;
        left = start;
    } else {
        /* this is the time the buffer entered the sink */
        entered = start + jitter;
        /* this is the time the buffer left the sink */
        left = start + jitter;
    }

    /* calculate duration of the buffer */
    if (stop >= start)
        duration = stop - start;
    else
        duration = 0;

    /* if we have the time when the last buffer left us, calculate
     * processing time */
    if (priv->last_left >= 0) {
        if (entered > priv->last_left) {
            pt = entered - priv->last_left;
        } else {
            pt = 0;
        }
    } else {
        pt = priv->avg_pt;
    }

    TRACE("start %s, entered %s, left %s, pt %s, duration %s, jitter %s.\n",
            debugstr_time(start), debugstr_time(entered), debugstr_time(left),
            debugstr_time(pt), debugstr_time(duration), debugstr_time(jitter));

    TRACE("average duration %s, average pt %s, average rate %.16e.\n",
            debugstr_time(priv->avg_duration), debugstr_time(priv->avg_pt), priv->avg_rate);

    /* collect running averages. for first observations, we copy the
    * values */
    if (priv->avg_duration < 0)
        priv->avg_duration = duration;
    else
        priv->avg_duration = UPDATE_RUNNING_AVG (priv->avg_duration, duration);

    if (priv->avg_pt < 0)
        priv->avg_pt = pt;
    else
        priv->avg_pt = UPDATE_RUNNING_AVG (priv->avg_pt, pt);

    if (priv->avg_duration != 0)
        rate =
            (double)priv->avg_pt /
            (double)priv->avg_duration;
    else
        rate = 0.0;

    if (priv->last_left >= 0) {
        if (priv->is_dropped || priv->avg_rate < 0.0) {
            priv->avg_rate = rate;
        } else {
            if (rate > 1.0)
                priv->avg_rate = UPDATE_RUNNING_AVG_N (priv->avg_rate, rate);
            else
                priv->avg_rate = UPDATE_RUNNING_AVG_P (priv->avg_rate, rate);
        }
    }

    if (priv->avg_rate >= 0.0) {
        HRESULT hr;
        Quality q;
        /* if we have a valid rate, start sending QoS messages */
        if (priv->current_jitter < 0) {
            /* make sure we never go below 0 when adding the jitter to the
             * timestamp. */
            if (priv->current_rstart < -priv->current_jitter)
                priv->current_jitter = -priv->current_rstart;
        }
        else
            priv->current_jitter += (priv->current_rstop - priv->current_rstart);
        q.Type = (jitter > 0 ? Famine : Flood);
        q.Proportion = (LONG)(1000. / priv->avg_rate);
        if (q.Proportion < 200)
            q.Proportion = 200;
        else if (q.Proportion > 5000)
            q.Proportion = 5000;
        q.Late = priv->current_jitter;
        q.TimeStamp = priv->current_rstart;
        TRACE("Late: %s from %s, rate: %g\n", debugstr_time(q.Late), debugstr_time(q.TimeStamp), 1./priv->avg_rate);
        hr = IQualityControl_Notify(&priv->IQualityControl_iface, &priv->pin->filter->IBaseFilter_iface, q);
        priv->qos_handled = hr == S_OK;
    }

    /* record when this buffer will leave us */
    priv->last_left = left;
}


void QualityControlRender_BeginRender(struct strmbase_qc *This, REFERENCE_TIME start, REFERENCE_TIME stop)
{
    This->start = -1;

    This->current_rstart = start;
    This->current_rstop = max(stop, start);

    if (start >= 0)
    {
        REFERENCE_TIME now;
        IReferenceClock_GetTime(This->pin->filter->clock, &now);
        This->current_jitter = (now - This->clockstart) - start;
    }
    else
        This->current_jitter = 0;

    /* FIXME: This isn't correct; we don't drop samples, nor should. */
    This->is_dropped = QualityControlRender_IsLate(This, This->current_jitter, start, stop);
    TRACE("dropped %d, start %s, stop %s, jitter %s.\n", This->is_dropped,
            debugstr_time(start), debugstr_time(stop), debugstr_time(This->current_jitter));
    if (This->is_dropped)
        This->dropped++;
    else
        This->rendered++;

    if (!This->pin->filter->clock)
        return;

    IReferenceClock_GetTime(This->pin->filter->clock, &This->start);

    TRACE("Starting at %s.\n", debugstr_time(This->start));
}

void QualityControlRender_EndRender(struct strmbase_qc *This)
{
    REFERENCE_TIME elapsed;

    TRACE("%p\n", This);

    if (!This->pin->filter->clock || This->start < 0
            || FAILED(IReferenceClock_GetTime(This->pin->filter->clock, &This->stop)))
        return;

    elapsed = This->start - This->stop;
    if (elapsed < 0)
        return;
    if (This->avg_render < 0)
        This->avg_render = elapsed;
    else
        This->avg_render = UPDATE_RUNNING_AVG (This->avg_render, elapsed);
}

void strmbase_qc_init(struct strmbase_qc *qc, struct strmbase_pin *pin)
{
    memset(qc, 0, sizeof(*qc));
    qc->pin = pin;
    qc->current_rstart = qc->current_rstop = -1;
    qc->IQualityControl_iface.lpVtbl = &quality_control_vtbl;
}
