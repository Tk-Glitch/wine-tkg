/*
 * Plug and Play support for hid devices found through udev
 *
 * Copyright 2016 CodeWeavers, Aric Stewart
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

#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_POLL_H
# include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
# include <sys/poll.h>
#endif
#ifdef HAVE_LIBUDEV_H
# include <libudev.h>
#endif
#ifdef HAVE_LINUX_HIDRAW_H
# include <linux/hidraw.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

#ifdef HAVE_LINUX_INPUT_H
# include <linux/input.h>
# undef SW_MAX
# if defined(EVIOCGBIT) && defined(EV_ABS) && defined(BTN_PINKIE)
#  define HAS_PROPER_INPUT_HEADER
# endif
# ifndef SYN_DROPPED
#  define SYN_DROPPED 3
# endif
#endif

#define NONAMELESSUNION

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "winternl.h"
#include "ddk/wdm.h"
#include "ddk/hidtypes.h"
#include "wine/debug.h"
#include "wine/heap.h"
#include "wine/unicode.h"

#ifdef HAS_PROPER_INPUT_HEADER
# include "hidusage.h"
#endif

#ifdef WORDS_BIGENDIAN
#define LE_WORD(x) RtlUshortByteSwap(x)
#define LE_DWORD(x) RtlUlongByteSwap(x)
#else
#define LE_WORD(x) (x)
#define LE_DWORD(x) (x)
#endif

#include "controller.h"
#include "bus.h"

WINE_DEFAULT_DEBUG_CHANNEL(plugplay);

#ifdef HAVE_UDEV

WINE_DECLARE_DEBUG_CHANNEL(hid_report);

static struct udev *udev_context = NULL;
static DWORD disable_hidraw = 0;
static DWORD disable_input = 0;
static HANDLE deviceloop_handle;
static int deviceloop_control[2];

static const WCHAR hidraw_busidW[] = {'H','I','D','R','A','W',0};
static const WCHAR lnxev_busidW[] = {'L','N','X','E','V',0};

struct platform_private
{
    struct udev_device *udev_device;
    int device_fd;

    HANDLE report_thread;
    int control_pipe[2];
};

static inline struct platform_private *impl_from_DEVICE_OBJECT(DEVICE_OBJECT *device)
{
    return (struct platform_private *)get_platform_private(device);
}

#ifdef HAS_PROPER_INPUT_HEADER

static const BYTE REPORT_ABS_AXIS_TAIL[] = {
    0x17, 0x00, 0x00, 0x00, 0x00,  /* LOGICAL_MINIMUM (0) */
    0x27, 0xff, 0x00, 0x00, 0x00,  /* LOGICAL_MAXIMUM (0xff) */
    0x37, 0x00, 0x00, 0x00, 0x00,  /* PHYSICAL_MINIMUM (0) */
    0x47, 0xff, 0x00, 0x00, 0x00,  /* PHYSICAL_MAXIMUM (256) */
    0x75, 0x20,                    /* REPORT_SIZE (32) */
    0x95, 0x00,                    /* REPORT_COUNT (2) */
    0x81, 0x02,                    /* INPUT (Data,Var,Abs) */
};
#define IDX_ABS_LOG_MINIMUM 1
#define IDX_ABS_LOG_MAXIMUM 6
#define IDX_ABS_PHY_MINIMUM 11
#define IDX_ABS_PHY_MAXIMUM 16
#define IDX_ABS_AXIS_COUNT 23

static const BYTE ABS_TO_HID_MAP[][2] = {
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_X},              /*ABS_X*/
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_Y},              /*ABS_Y*/
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_Z},              /*ABS_Z*/
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_RX},             /*ABS_RX*/
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_RY},             /*ABS_RY*/
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_RZ},             /*ABS_RZ*/
    {HID_USAGE_PAGE_SIMULATION, HID_USAGE_SIMULATION_THROTTLE}, /*ABS_THROTTLE*/
    {HID_USAGE_PAGE_SIMULATION, HID_USAGE_SIMULATION_RUDDER},   /*ABS_RUDDER*/
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_WHEEL},          /*ABS_WHEEL*/
    {HID_USAGE_PAGE_SIMULATION, HID_USAGE_SIMULATION_ACCELERATOR}, /*ABS_GAS*/
    {HID_USAGE_PAGE_SIMULATION, HID_USAGE_SIMULATION_BRAKE},    /*ABS_BRAKE*/
    {0,0},                                                      /*ABS_HAT0X*/
    {0,0},                                                      /*ABS_HAT0Y*/
    {0,0},                                                      /*ABS_HAT1X*/
    {0,0},                                                      /*ABS_HAT1Y*/
    {0,0},                                                      /*ABS_HAT2X*/
    {0,0},                                                      /*ABS_HAT2Y*/
    {0,0},                                                      /*ABS_HAT3X*/
    {0,0},                                                      /*ABS_HAT3Y*/
    {HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_TIP_PRESSURE}, /*ABS_PRESSURE*/
    {0, 0},                                                     /*ABS_DISTANCE*/
    {HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_X_TILT},     /*ABS_TILT_X*/
    {HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_Y_TILT},     /*ABS_TILT_Y*/
    {0, 0},                                                     /*ABS_TOOL_WIDTH*/
    {0, 0},
    {0, 0},
    {0, 0},
    {HID_USAGE_PAGE_CONSUMER, HID_USAGE_CONSUMER_VOLUME}        /*ABS_VOLUME*/
};
#define HID_ABS_MAX (ABS_VOLUME+1)
#define TOP_ABS_PAGE (HID_USAGE_PAGE_DIGITIZER+1)

static const BYTE REL_TO_HID_MAP[][2] = {
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_X},     /* REL_X */
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_Y},     /* REL_Y */
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_Z},     /* REL_Z */
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_RX},    /* REL_RX */
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_RY},    /* REL_RY */
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_RZ},    /* REL_RZ */
    {0, 0},                                            /* REL_HWHEEL */
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_DIAL},  /* REL_DIAL */
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_WHEEL}, /* REL_WHEEL */
    {0, 0}                                             /* REL_MISC */
};

#define HID_REL_MAX (REL_MISC+1)
#define TOP_REL_PAGE (HID_USAGE_PAGE_CONSUMER+1)

struct wine_input_absinfo {
    struct input_absinfo info;
    BYTE report_index;
};

struct wine_input_private {
    struct platform_private base;

    int buffer_length;
    BYTE *last_report_buffer;
    BYTE *current_report_buffer;
    enum { FIRST, NORMAL, DROPPED } report_state;

    int report_descriptor_size;
    BYTE *report_descriptor;

    int button_start;
    BYTE button_map[KEY_MAX];
    BYTE rel_map[HID_REL_MAX];
    BYTE hat_map[8];
    int hat_values[8];
    struct wine_input_absinfo abs_map[HID_ABS_MAX];
};

#define test_bit(arr,bit) (((BYTE*)(arr))[(bit)>>3]&(1<<((bit)&7)))

static BYTE *add_axis_block(BYTE *report_ptr, BYTE count, BYTE page, BYTE *usages, BOOL absolute, const struct wine_input_absinfo *absinfo)
{
    int i;
    memcpy(report_ptr, REPORT_AXIS_HEADER, sizeof(REPORT_AXIS_HEADER));
    report_ptr[IDX_AXIS_PAGE] = page;
    report_ptr += sizeof(REPORT_AXIS_HEADER);
    for (i = 0; i < count; i++)
    {
        memcpy(report_ptr, REPORT_AXIS_USAGE, sizeof(REPORT_AXIS_USAGE));
        report_ptr[IDX_AXIS_USAGE] = usages[i];
        report_ptr += sizeof(REPORT_AXIS_USAGE);
    }
    if (absolute)
    {
        memcpy(report_ptr, REPORT_ABS_AXIS_TAIL, sizeof(REPORT_ABS_AXIS_TAIL));
        if (absinfo)
        {
            *((int*)&report_ptr[IDX_ABS_LOG_MINIMUM]) = LE_DWORD(absinfo->info.minimum);
            *((int*)&report_ptr[IDX_ABS_LOG_MAXIMUM]) = LE_DWORD(absinfo->info.maximum);
            *((int*)&report_ptr[IDX_ABS_PHY_MINIMUM]) = LE_DWORD(absinfo->info.minimum);
            *((int*)&report_ptr[IDX_ABS_PHY_MAXIMUM]) = LE_DWORD(absinfo->info.maximum);
        }
        report_ptr[IDX_ABS_AXIS_COUNT] = count;
        report_ptr += sizeof(REPORT_ABS_AXIS_TAIL);
    }
    else
    {
        memcpy(report_ptr, REPORT_REL_AXIS_TAIL, sizeof(REPORT_REL_AXIS_TAIL));
        report_ptr[IDX_REL_AXIS_COUNT] = count;
        report_ptr += sizeof(REPORT_REL_AXIS_TAIL);
    }
    return report_ptr;
}

static const BYTE* what_am_I(struct udev_device *dev)
{
    static const BYTE Unknown[2]     = {HID_USAGE_PAGE_GENERIC, 0};
    static const BYTE Mouse[2]       = {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_MOUSE};
    static const BYTE Keyboard[2]    = {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_KEYBOARD};
    static const BYTE Gamepad[2]     = {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_GAMEPAD};
    static const BYTE Keypad[2]      = {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_KEYPAD};
    static const BYTE Tablet[2]      = {HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_PEN};
    static const BYTE Touchscreen[2] = {HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_TOUCH_SCREEN};
    static const BYTE Touchpad[2]    = {HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_TOUCH_PAD};

    struct udev_device *parent = dev;

    /* Look to the parents until we get a clue */
    while (parent)
    {
        if (udev_device_get_property_value(parent, "ID_INPUT_MOUSE"))
            return Mouse;
        else if (udev_device_get_property_value(parent, "ID_INPUT_KEYBOARD"))
            return Keyboard;
        else if (udev_device_get_property_value(parent, "ID_INPUT_JOYSTICK"))
            return Gamepad;
        else if (udev_device_get_property_value(parent, "ID_INPUT_KEY"))
            return Keypad;
        else if (udev_device_get_property_value(parent, "ID_INPUT_TOUCHPAD"))
            return Touchpad;
        else if (udev_device_get_property_value(parent, "ID_INPUT_TOUCHSCREEN"))
            return Touchscreen;
        else if (udev_device_get_property_value(parent, "ID_INPUT_TABLET"))
            return Tablet;

        parent = udev_device_get_parent_with_subsystem_devtype(parent, "input", NULL);
    }
    return Unknown;
}

static void set_button_value(int index, int value, BYTE* buffer)
{
    int bindex = index / 8;
    int b = index % 8;
    BYTE mask;

    mask = 1<<b;
    if (value)
        buffer[bindex] = buffer[bindex] | mask;
    else
    {
        mask = ~mask;
        buffer[bindex] = buffer[bindex] & mask;
    }
}

static void set_abs_axis_value(struct wine_input_private *ext, int code, int value)
{
    int index;
    /* check for hatswitches */
    if (code <= ABS_HAT3Y && code >= ABS_HAT0X)
    {
        index = code - ABS_HAT0X;
        ext->hat_values[index] = value;
        if ((code - ABS_HAT0X) % 2)
            index--;
        /* 8 1 2
         * 7 0 3
         * 6 5 4 */
        if (ext->hat_values[index] == 0)
        {
            if (ext->hat_values[index+1] == 0)
                value = 0;
            else if (ext->hat_values[index+1] < 0)
                value = 1;
            else
                value = 5;
        }
        else if (ext->hat_values[index] > 0)
        {
            if (ext->hat_values[index+1] == 0)
                value = 3;
            else if (ext->hat_values[index+1] < 0)
                value = 2;
            else
                value = 4;
        }
        else
        {
            if (ext->hat_values[index+1] == 0)
                value = 7;
            else if (ext->hat_values[index+1] < 0)
                value = 8;
            else
                value = 6;
        }
        ext->current_report_buffer[ext->hat_map[index]] = value;
    }
    else if (code < HID_ABS_MAX && ABS_TO_HID_MAP[code][0] != 0)
    {
        index = ext->abs_map[code].report_index;
        *((DWORD*)&ext->current_report_buffer[index]) = LE_DWORD(value);
    }
}

static void set_rel_axis_value(struct wine_input_private *ext, int code, int value)
{
    int index;
    if (code < HID_REL_MAX && REL_TO_HID_MAP[code][0] != 0)
    {
        index = ext->rel_map[code];
        if (value > 127) value = 127;
        if (value < -127) value = -127;
        ext->current_report_buffer[index] = value;
    }
}

static INT count_buttons(int device_fd, BYTE *map)
{
    int i;
    int button_count = 0;
    BYTE keybits[(KEY_MAX+7)/8];

    if (ioctl(device_fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits) == -1)
    {
        WARN("ioctl(EVIOCGBIT, EV_KEY) failed: %d %s\n", errno, strerror(errno));
        return FALSE;
    }

    for (i = BTN_MISC; i < KEY_MAX; i++)
    {
        if (test_bit(keybits, i))
        {
            if (map) map[i] = button_count;
            button_count++;
        }
    }
    return button_count;
}

static INT count_abs_axis(int device_fd)
{
    BYTE absbits[(ABS_MAX+7)/8];
    int abs_count = 0;
    int i;

    if (ioctl(device_fd, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits) == -1)
    {
        WARN("ioctl(EVIOCGBIT, EV_ABS) failed: %d %s\n", errno, strerror(errno));
        return 0;
    }

    for (i = 0; i < HID_ABS_MAX; i++)
        if (test_bit(absbits, i) &&
            (ABS_TO_HID_MAP[i][1] >= HID_USAGE_GENERIC_X &&
             ABS_TO_HID_MAP[i][1] <= HID_USAGE_GENERIC_WHEEL))
                abs_count++;
    return abs_count;
}

static BOOL build_report_descriptor(struct wine_input_private *ext, struct udev_device *dev)
{
    int abs_pages[TOP_ABS_PAGE][HID_ABS_MAX+1];
    int rel_pages[TOP_REL_PAGE][HID_REL_MAX+1];
    BYTE absbits[(ABS_MAX+7)/8];
    BYTE relbits[(REL_MAX+7)/8];
    BYTE *report_ptr;
    INT i, descript_size;
    INT report_size;
    INT button_count, abs_count, rel_count, hat_count;
    const BYTE *device_usage = what_am_I(dev);

    if (ioctl(ext->base.device_fd, EVIOCGBIT(EV_REL, sizeof(relbits)), relbits) == -1)
    {
        WARN("ioctl(EVIOCGBIT, EV_REL) failed: %d %s\n", errno, strerror(errno));
        return FALSE;
    }
    if (ioctl(ext->base.device_fd, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits) == -1)
    {
        WARN("ioctl(EVIOCGBIT, EV_ABS) failed: %d %s\n", errno, strerror(errno));
        return FALSE;
    }

    descript_size = sizeof(REPORT_HEADER) + sizeof(REPORT_TAIL);
    report_size = 0;

    abs_count = 0;
    memset(abs_pages, 0, sizeof(abs_pages));
    for (i = 0; i < HID_ABS_MAX; i++)
        if (test_bit(absbits, i))
        {
            abs_pages[ABS_TO_HID_MAP[i][0]][0]++;
            abs_pages[ABS_TO_HID_MAP[i][0]][abs_pages[ABS_TO_HID_MAP[i][0]][0]] = i;

            ioctl(ext->base.device_fd, EVIOCGABS(i), &(ext->abs_map[i]));
        }
    /* Skip page 0, aka HID_USAGE_PAGE_UNDEFINED */
    for (i = 1; i < TOP_ABS_PAGE; i++)
        if (abs_pages[i][0] > 0)
        {
            int j;
            descript_size += sizeof(REPORT_AXIS_USAGE) * abs_pages[i][0];
            for (j = 1; j <= abs_pages[i][0]; j++)
            {
                ext->abs_map[abs_pages[i][j]].report_index = report_size;
                report_size+=4;
            }
            abs_count++;
        }
    descript_size += sizeof(REPORT_AXIS_HEADER) * abs_count;
    descript_size += sizeof(REPORT_ABS_AXIS_TAIL) * abs_count;

    rel_count = 0;
    memset(rel_pages, 0, sizeof(rel_pages));
    for (i = 0; i < HID_REL_MAX; i++)
        if (test_bit(relbits, i))
        {
            rel_pages[REL_TO_HID_MAP[i][0]][0]++;
            rel_pages[REL_TO_HID_MAP[i][0]][rel_pages[REL_TO_HID_MAP[i][0]][0]] = i;
        }
    /* Skip page 0, aka HID_USAGE_PAGE_UNDEFINED */
    for (i = 1; i < TOP_REL_PAGE; i++)
        if (rel_pages[i][0] > 0)
        {
            int j;
            descript_size += sizeof(REPORT_AXIS_USAGE) * rel_pages[i][0];
            for (j = 1; j <= rel_pages[i][0]; j++)
            {
                ext->rel_map[rel_pages[i][j]] = report_size;
                report_size++;
            }
            rel_count++;
        }
    descript_size += sizeof(REPORT_AXIS_HEADER) * rel_count;
    descript_size += sizeof(REPORT_REL_AXIS_TAIL) * rel_count;

    /* For now lump all buttons just into incremental usages, Ignore Keys */
    ext->button_start = report_size;
    button_count = count_buttons(ext->base.device_fd, ext->button_map);
    if (button_count)
    {
        descript_size += sizeof(REPORT_BUTTONS);
        if (button_count % 8)
            descript_size += sizeof(REPORT_PADDING);
        report_size += (button_count + 7) / 8;
    }

    hat_count = 0;
    for (i = ABS_HAT0X; i <=ABS_HAT3X; i+=2)
        if (test_bit(absbits, i))
        {
            ext->hat_map[i - ABS_HAT0X] = report_size;
            ext->hat_values[i - ABS_HAT0X] = 0;
            ext->hat_values[i - ABS_HAT0X + 1] = 0;
            report_size++;
            hat_count++;
        }
    if (hat_count > 0)
        descript_size += sizeof(REPORT_HATSWITCH);

    TRACE("Report Descriptor will be %i bytes\n", descript_size);
    TRACE("Report will be %i bytes\n", report_size);

    ext->report_descriptor = HeapAlloc(GetProcessHeap(), 0, descript_size);
    if (!ext->report_descriptor)
    {
        ERR("Failed to alloc report descriptor\n");
        return FALSE;
    }
    report_ptr = ext->report_descriptor;

    memcpy(report_ptr, REPORT_HEADER, sizeof(REPORT_HEADER));
    report_ptr[IDX_HEADER_PAGE] = device_usage[0];
    report_ptr[IDX_HEADER_USAGE] = device_usage[1];
    report_ptr += sizeof(REPORT_HEADER);
    if (abs_count)
    {
        for (i = 1; i < TOP_ABS_PAGE; i++)
        {
            if (abs_pages[i][0])
            {
                BYTE usages[HID_ABS_MAX];
                int j;
                for (j = 0; j < abs_pages[i][0]; j++)
                    usages[j] = ABS_TO_HID_MAP[abs_pages[i][j+1]][1];
                report_ptr = add_axis_block(report_ptr, abs_pages[i][0], i, usages, TRUE, &ext->abs_map[abs_pages[i][1]]);
            }
        }
    }
    if (rel_count)
    {
        for (i = 1; i < TOP_REL_PAGE; i++)
        {
            if (rel_pages[i][0])
            {
                BYTE usages[HID_REL_MAX];
                int j;
                for (j = 0; j < rel_pages[i][0]; j++)
                    usages[j] = REL_TO_HID_MAP[rel_pages[i][j+1]][1];
                report_ptr = add_axis_block(report_ptr, rel_pages[i][0], i, usages, FALSE, NULL);
            }
        }
    }
    if (button_count)
    {
        report_ptr = add_button_block(report_ptr, 1, button_count);
        if (button_count % 8)
        {
            BYTE padding = 8 - (button_count % 8);
            report_ptr = add_padding_block(report_ptr, padding);
        }
    }
    if (hat_count)
        report_ptr = add_hatswitch(report_ptr, hat_count);

    memcpy(report_ptr, REPORT_TAIL, sizeof(REPORT_TAIL));

    ext->report_descriptor_size = descript_size;
    ext->buffer_length = report_size;
    ext->current_report_buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, report_size);
    if (ext->current_report_buffer == NULL)
    {
        ERR("Failed to alloc report buffer\n");
        HeapFree(GetProcessHeap(), 0, ext->report_descriptor);
        return FALSE;
    }
    ext->last_report_buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, report_size);
    if (ext->last_report_buffer == NULL)
    {
        ERR("Failed to alloc report buffer\n");
        HeapFree(GetProcessHeap(), 0, ext->report_descriptor);
        HeapFree(GetProcessHeap(), 0, ext->current_report_buffer);
        return FALSE;
    }
    ext->report_state = FIRST;

    /* Initialize axis in the report */
    for (i = 0; i < HID_ABS_MAX; i++)
        if (test_bit(absbits, i))
            set_abs_axis_value(ext, i, ext->abs_map[i].info.value);

    return TRUE;
}

static BOOL set_report_from_event(struct wine_input_private *ext, struct input_event *ie)
{
    switch(ie->type)
    {
#ifdef EV_SYN
        case EV_SYN:
            switch (ie->code)
            {
                case SYN_REPORT:
                    if (ext->report_state == NORMAL)
                    {
                        memcpy(ext->last_report_buffer, ext->current_report_buffer, ext->buffer_length);
                        return TRUE;
                    }
                    else
                    {
                        if (ext->report_state == DROPPED)
                            memcpy(ext->current_report_buffer, ext->last_report_buffer, ext->buffer_length);
                        ext->report_state = NORMAL;
                    }
                    break;
                case SYN_DROPPED:
                    TRACE_(hid_report)("received SY_DROPPED\n");
                    ext->report_state = DROPPED;
            }
            return FALSE;
#endif
#ifdef EV_MSC
        case EV_MSC:
            return FALSE;
#endif
        case EV_KEY:
            set_button_value(ext->button_start + ext->button_map[ie->code], ie->value, ext->current_report_buffer);
            return FALSE;
        case EV_ABS:
            set_abs_axis_value(ext, ie->code, ie->value);
            return FALSE;
        case EV_REL:
            set_rel_axis_value(ext, ie->code, ie->value);
            return FALSE;
        default:
            ERR("TODO: Process Report (%i, %i)\n",ie->type, ie->code);
            return FALSE;
    }
}
#endif

static inline WCHAR *strdupAtoW(const char *src)
{
    WCHAR *dst;
    DWORD len;
    if (!src) return NULL;
    len = MultiByteToWideChar(CP_UNIXCP, 0, src, -1, NULL, 0);
    if ((dst = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR))))
        MultiByteToWideChar(CP_UNIXCP, 0, src, -1, dst, len);
    return dst;
}

static WCHAR *get_sysattr_string(struct udev_device *dev, const char *sysattr)
{
    const char *attr = udev_device_get_sysattr_value(dev, sysattr);
    if (!attr)
    {
        WARN("Could not get %s from device\n", sysattr);
        return NULL;
    }
    return strdupAtoW(attr);
}

static int compare_platform_device(DEVICE_OBJECT *device, void *platform_dev)
{
    struct udev_device *dev1 = impl_from_DEVICE_OBJECT(device)->udev_device;
    struct udev_device *dev2 = platform_dev;
    return strcmp(udev_device_get_syspath(dev1), udev_device_get_syspath(dev2));
}

static NTSTATUS hidraw_get_reportdescriptor(DEVICE_OBJECT *device, BYTE *buffer, DWORD length, DWORD *out_length)
{
#ifdef HAVE_LINUX_HIDRAW_H
    struct hidraw_report_descriptor descriptor;
    struct platform_private *private = impl_from_DEVICE_OBJECT(device);

    if (ioctl(private->device_fd, HIDIOCGRDESCSIZE, &descriptor.size) == -1)
    {
        WARN("ioctl(HIDIOCGRDESCSIZE) failed: %d %s\n", errno, strerror(errno));
        return STATUS_UNSUCCESSFUL;
    }

    *out_length = descriptor.size;

    if (length < descriptor.size)
        return STATUS_BUFFER_TOO_SMALL;
    if (!descriptor.size)
        return STATUS_SUCCESS;

    if (ioctl(private->device_fd, HIDIOCGRDESC, &descriptor) == -1)
    {
        WARN("ioctl(HIDIOCGRDESC) failed: %d %s\n", errno, strerror(errno));
        return STATUS_UNSUCCESSFUL;
    }

    memcpy(buffer, descriptor.value, descriptor.size);
    return STATUS_SUCCESS;
#else
    return STATUS_NOT_IMPLEMENTED;
#endif
}

static NTSTATUS hidraw_get_string(DEVICE_OBJECT *device, DWORD index, WCHAR *buffer, DWORD length)
{
    struct udev_device *usbdev;
    struct platform_private *private = impl_from_DEVICE_OBJECT(device);
    WCHAR *str = NULL;

    usbdev = udev_device_get_parent_with_subsystem_devtype(private->udev_device, "usb", "usb_device");
    if (usbdev)
    {
        switch (index)
        {
            case HID_STRING_ID_IPRODUCT:
                str = get_sysattr_string(usbdev, "product");
                break;
            case HID_STRING_ID_IMANUFACTURER:
                str = get_sysattr_string(usbdev, "manufacturer");
                break;
            case HID_STRING_ID_ISERIALNUMBER:
                str = get_sysattr_string(usbdev, "serial");
                break;
            default:
                ERR("Unhandled string index %08x\n", index);
                return STATUS_NOT_IMPLEMENTED;
        }
    }
    else
    {
#ifdef HAVE_LINUX_HIDRAW_H
        switch (index)
        {
            case HID_STRING_ID_IPRODUCT:
            {
                char buf[MAX_PATH];
                if (ioctl(private->device_fd, HIDIOCGRAWNAME(MAX_PATH), buf) == -1)
                    WARN("ioctl(HIDIOCGRAWNAME) failed: %d %s\n", errno, strerror(errno));
                else
                    str = strdupAtoW(buf);
                break;
            }
            case HID_STRING_ID_IMANUFACTURER:
                break;
            case HID_STRING_ID_ISERIALNUMBER:
                break;
            default:
                ERR("Unhandled string index %08x\n", index);
                return STATUS_NOT_IMPLEMENTED;
        }
#else
        return STATUS_NOT_IMPLEMENTED;
#endif
    }

    if (!str)
    {
        if (!length) return STATUS_BUFFER_TOO_SMALL;
        buffer[0] = 0;
        return STATUS_SUCCESS;
    }

    if (length <= strlenW(str))
    {
        HeapFree(GetProcessHeap(), 0, str);
        return STATUS_BUFFER_TOO_SMALL;
    }

    strcpyW(buffer, str);
    HeapFree(GetProcessHeap(), 0, str);
    return STATUS_SUCCESS;
}

static DWORD CALLBACK device_report_thread(void *args)
{
    DEVICE_OBJECT *device = (DEVICE_OBJECT*)args;
    struct platform_private *private = impl_from_DEVICE_OBJECT(device);
    struct pollfd plfds[2];

    plfds[0].fd = private->device_fd;
    plfds[0].events = POLLIN;
    plfds[0].revents = 0;
    plfds[1].fd = private->control_pipe[0];
    plfds[1].events = POLLIN;
    plfds[1].revents = 0;

    while (1)
    {
        int size;
        BYTE report_buffer[1024];

        if (poll(plfds, 2, -1) <= 0) continue;
        if (plfds[1].revents)
            break;
        size = read(plfds[0].fd, report_buffer, sizeof(report_buffer));
        if (size == -1)
            TRACE_(hid_report)("Read failed. Likely an unplugged device %d %s\n", errno, strerror(errno));
        else if (size == 0)
            TRACE_(hid_report)("Failed to read report\n");
        else
            process_hid_report(device, report_buffer, size);
    }
    return 0;
}

static NTSTATUS begin_report_processing(DEVICE_OBJECT *device)
{
    struct platform_private *private = impl_from_DEVICE_OBJECT(device);

    if (private->report_thread)
        return STATUS_SUCCESS;

    if (pipe(private->control_pipe) != 0)
    {
        ERR("Control pipe creation failed\n");
        return STATUS_UNSUCCESSFUL;
    }

    private->report_thread = CreateThread(NULL, 0, device_report_thread, device, 0, NULL);
    if (!private->report_thread)
    {
        ERR("Unable to create device report thread\n");
        close(private->control_pipe[0]);
        close(private->control_pipe[1]);
        return STATUS_UNSUCCESSFUL;
    }
    else
        return STATUS_SUCCESS;
}

static NTSTATUS hidraw_set_output_report(DEVICE_OBJECT *device, UCHAR id, BYTE *report, DWORD length, ULONG_PTR *written)
{
    struct platform_private* ext = impl_from_DEVICE_OBJECT(device);
    int rc;

    if (id != 0)
        rc = write(ext->device_fd, report, length);
    else
    {
        BYTE report_buffer[1024];

        if (length + 1 > sizeof(report_buffer))
        {
            ERR("Output report buffer too small\n");
            return STATUS_UNSUCCESSFUL;
        }

        report_buffer[0] = 0;
        memcpy(&report_buffer[1], report, length);
        rc = write(ext->device_fd, report_buffer, length + 1);
    }
    if (rc > 0)
    {
        *written = rc;
        return STATUS_SUCCESS;
    }
    else
    {
        TRACE("write failed: %d %d %s\n", rc, errno, strerror(errno));
        *written = 0;
        return STATUS_UNSUCCESSFUL;
    }
}

static NTSTATUS hidraw_get_feature_report(DEVICE_OBJECT *device, UCHAR id, BYTE *report, DWORD length, ULONG_PTR *read)
{
#if defined(HAVE_LINUX_HIDRAW_H) && defined(HIDIOCGFEATURE)
    int rc;
    struct platform_private* ext = impl_from_DEVICE_OBJECT(device);
    report[0] = id;
    length = min(length, 0x1fff);
    rc = ioctl(ext->device_fd, HIDIOCGFEATURE(length), report);
    if (rc >= 0)
    {
        *read = rc;
        return STATUS_SUCCESS;
    }
    else
    {
        TRACE_(hid_report)("ioctl(HIDIOCGFEATURE(%d)) failed: %d %s\n", length, errno, strerror(errno));
        *read = 0;
        return STATUS_UNSUCCESSFUL;
    }
#else
    *read = 0;
    return STATUS_NOT_IMPLEMENTED;
#endif
}

static NTSTATUS hidraw_set_feature_report(DEVICE_OBJECT *device, UCHAR id, BYTE *report, DWORD length, ULONG_PTR *written)
{
#if defined(HAVE_LINUX_HIDRAW_H) && defined(HIDIOCSFEATURE)
    int rc;
    struct platform_private* ext = impl_from_DEVICE_OBJECT(device);
    BYTE *feature_buffer;
    BYTE buffer[8192];

    if (id == 0)
    {
        if (length + 1 > sizeof(buffer))
        {
            ERR("Output feature buffer too small\n");
            return STATUS_UNSUCCESSFUL;
        }
        buffer[0] = 0;
        memcpy(&buffer[1], report, length);
        feature_buffer = buffer;
        length = length + 1;
    }
    else
        feature_buffer = report;
    length = min(length, 0x1fff);
    rc = ioctl(ext->device_fd, HIDIOCSFEATURE(length), feature_buffer);
    if (rc >= 0)
    {
        *written = rc;
        return STATUS_SUCCESS;
    }
    else
    {
        TRACE_(hid_report)("ioctl(HIDIOCSFEATURE(%d)) failed: %d %s\n", length, errno, strerror(errno));
        *written = 0;
        return STATUS_UNSUCCESSFUL;
    }
#else
    *written = 0;
    return STATUS_NOT_IMPLEMENTED;
#endif
}

static const platform_vtbl hidraw_vtbl =
{
    compare_platform_device,
    hidraw_get_reportdescriptor,
    hidraw_get_string,
    begin_report_processing,
    hidraw_set_output_report,
    hidraw_get_feature_report,
    hidraw_set_feature_report,
};

#ifdef HAS_PROPER_INPUT_HEADER

static inline struct wine_input_private *input_impl_from_DEVICE_OBJECT(DEVICE_OBJECT *device)
{
    return (struct wine_input_private*)get_platform_private(device);
}

static NTSTATUS lnxev_get_reportdescriptor(DEVICE_OBJECT *device, BYTE *buffer, DWORD length, DWORD *out_length)
{
    struct wine_input_private *ext = input_impl_from_DEVICE_OBJECT(device);

    *out_length = ext->report_descriptor_size;

    if (length < ext->report_descriptor_size)
        return STATUS_BUFFER_TOO_SMALL;

    memcpy(buffer, ext->report_descriptor, ext->report_descriptor_size);

    return STATUS_SUCCESS;
}

static NTSTATUS lnxev_get_string(DEVICE_OBJECT *device, DWORD index, WCHAR *buffer, DWORD length)
{
    struct wine_input_private *ext = input_impl_from_DEVICE_OBJECT(device);
    char str[255];

    str[0] = 0;
    switch (index)
    {
        case HID_STRING_ID_IPRODUCT:
            ioctl(ext->base.device_fd, EVIOCGNAME(sizeof(str)), str);
            break;
        case HID_STRING_ID_IMANUFACTURER:
            strcpy(str,"evdev");
            break;
        case HID_STRING_ID_ISERIALNUMBER:
            ioctl(ext->base.device_fd, EVIOCGUNIQ(sizeof(str)), str);
            break;
        default:
            ERR("Unhandled string index %i\n", index);
    }

    MultiByteToWideChar(CP_ACP, 0, str, -1, buffer, length);
    return STATUS_SUCCESS;
}

static DWORD CALLBACK lnxev_device_report_thread(void *args)
{
    DEVICE_OBJECT *device = (DEVICE_OBJECT*)args;
    struct wine_input_private *private = input_impl_from_DEVICE_OBJECT(device);
    struct pollfd plfds[2];

    plfds[0].fd = private->base.device_fd;
    plfds[0].events = POLLIN;
    plfds[0].revents = 0;
    plfds[1].fd = private->base.control_pipe[0];
    plfds[1].events = POLLIN;
    plfds[1].revents = 0;

    while (1)
    {
        int size;
        struct input_event ie;

        if (poll(plfds, 2, -1) <= 0) continue;
        if (plfds[1].revents || !private->current_report_buffer || private->buffer_length == 0)
            break;
        size = read(plfds[0].fd, &ie, sizeof(ie));
        if (size == -1)
            TRACE_(hid_report)("Read failed. Likely an unplugged device\n");
        else if (size == 0)
            TRACE_(hid_report)("Failed to read report\n");
        else if (set_report_from_event(private, &ie))
            process_hid_report(device, private->current_report_buffer, private->buffer_length);
    }
    return 0;
}

static NTSTATUS lnxev_begin_report_processing(DEVICE_OBJECT *device)
{
    struct wine_input_private *private = input_impl_from_DEVICE_OBJECT(device);

    if (private->base.report_thread)
        return STATUS_SUCCESS;

    if (pipe(private->base.control_pipe) != 0)
    {
        ERR("Control pipe creation failed\n");
        return STATUS_UNSUCCESSFUL;
    }

    private->base.report_thread = CreateThread(NULL, 0, lnxev_device_report_thread, device, 0, NULL);
    if (!private->base.report_thread)
    {
        ERR("Unable to create device report thread\n");
        close(private->base.control_pipe[0]);
        close(private->base.control_pipe[1]);
        return STATUS_UNSUCCESSFUL;
    }
    return STATUS_SUCCESS;
}

static NTSTATUS lnxev_set_output_report(DEVICE_OBJECT *device, UCHAR id, BYTE *report, DWORD length, ULONG_PTR *written)
{
    *written = 0;
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS lnxev_get_feature_report(DEVICE_OBJECT *device, UCHAR id, BYTE *report, DWORD length, ULONG_PTR *read)
{
    *read = 0;
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS lnxev_set_feature_report(DEVICE_OBJECT *device, UCHAR id, BYTE *report, DWORD length, ULONG_PTR *written)
{
    *written = 0;
    return STATUS_NOT_IMPLEMENTED;
}

static const platform_vtbl lnxev_vtbl = {
    compare_platform_device,
    lnxev_get_reportdescriptor,
    lnxev_get_string,
    lnxev_begin_report_processing,
    lnxev_set_output_report,
    lnxev_get_feature_report,
    lnxev_set_feature_report,
};
#endif

static int check_same_device(DEVICE_OBJECT *device, void* context)
{
    return !compare_platform_device(device, context);
}

static int parse_uevent_info(const char *uevent, DWORD *vendor_id,
                             DWORD *product_id, WORD *input, WCHAR **serial_number)
{
    DWORD bus_type;
    char *tmp;
    char *saveptr = NULL;
    char *line;
    char *key;
    char *value;

    int found_id = 0;
    int found_serial = 0;

    tmp = heap_alloc(strlen(uevent) + 1);
    strcpy(tmp, uevent);
    line = strtok_r(tmp, "\n", &saveptr);
    while (line != NULL)
    {
        /* line: "KEY=value" */
        key = line;
        value = strchr(line, '=');
        if (!value)
        {
            goto next_line;
        }
        *value = '\0';
        value++;

        if (strcmp(key, "HID_ID") == 0)
        {
            /**
             *        type vendor   product
             * HID_ID=0003:000005AC:00008242
             **/
            int ret = sscanf(value, "%x:%x:%x", &bus_type, vendor_id, product_id);
            if (ret == 3)
                found_id = 1;
        }
        else if (strcmp(key, "HID_UNIQ") == 0)
        {
            /* The caller has to free the serial number */
            if (*value)
            {
                *serial_number = strdupAtoW(value);
                found_serial = 1;
            }
        }
        else if (strcmp(key, "HID_PHYS") == 0)
        {
            const char *input_no = strstr(value, "input");
            if (input_no)
                *input = atoi(input_no+5 );
        }

next_line:
        line = strtok_r(NULL, "\n", &saveptr);
    }

    heap_free(tmp);
    return (found_id && found_serial);
}

static DWORD a_to_bcd(const char *s)
{
    DWORD r = 0;
    const char *c;
    int shift = strlen(s) - 1;
    for (c = s; *c; ++c)
    {
        r |= (*c - '0') << (shift * 4);
        --shift;
    }
    return r;
}

static void try_add_device(struct udev_device *dev)
{
    DWORD vid = 0, pid = 0, version = 0;
    struct udev_device *hiddev = NULL, *walk_device;
    DEVICE_OBJECT *device = NULL;
    const char *subsystem;
    const char *devnode;
    WCHAR *serial = NULL;
    BOOL is_gamepad = FALSE;
    WORD input = -1;
    int fd;
    static const CHAR *base_serial = "0000";

    if (!(devnode = udev_device_get_devnode(dev)))
        return;

    if ((fd = open(devnode, O_RDWR)) == -1)
    {
        WARN("Unable to open udev device %s: %s\n", debugstr_a(devnode), strerror(errno));
        return;
    }

    subsystem = udev_device_get_subsystem(dev);
    hiddev = udev_device_get_parent_with_subsystem_devtype(dev, "hid", NULL);
    if (hiddev)
    {
        const char *bcdDevice = NULL;
#ifdef HAS_PROPER_INPUT_HEADER
        const platform_vtbl *other_vtbl = NULL;
        DEVICE_OBJECT *dup = NULL;
        if (strcmp(subsystem, "hidraw") == 0)
            other_vtbl = &lnxev_vtbl;
        else if (strcmp(subsystem, "input") == 0)
            other_vtbl = &hidraw_vtbl;

        if (other_vtbl)
            dup = bus_enumerate_hid_devices(other_vtbl, check_same_device, dev);
        if (dup)
        {
            TRACE("Duplicate cross bus device (%p) found, not adding the new one\n", dup);
            close(fd);
            return;
        }
#endif
        parse_uevent_info(udev_device_get_sysattr_value(hiddev, "uevent"),
                          &vid, &pid, &input, &serial);
        if (serial == NULL)
            serial = strdupAtoW(base_serial);

        walk_device = dev;
        while (walk_device && !bcdDevice)
        {
            bcdDevice = udev_device_get_sysattr_value(walk_device, "bcdDevice");
            walk_device = udev_device_get_parent(walk_device);
        }
        if (bcdDevice)
        {
            version = a_to_bcd(bcdDevice);
        }
    }
#ifdef HAS_PROPER_INPUT_HEADER
    else
    {
        struct input_id device_id = {0};
        char device_uid[255];

        if (ioctl(fd, EVIOCGID, &device_id) < 0)
            WARN("ioctl(EVIOCGID) failed: %d %s\n", errno, strerror(errno));
        device_uid[0] = 0;
        if (ioctl(fd, EVIOCGUNIQ(254), device_uid) >= 0 && device_uid[0])
            serial = strdupAtoW(device_uid);

        vid = device_id.vendor;
        pid = device_id.product;
        version = device_id.version;
    }
#else
    else
        WARN("Could not get device to query VID, PID, Version and Serial\n");
#endif

    if (is_xbox_gamepad(vid, pid))
        is_gamepad = TRUE;
#ifdef HAS_PROPER_INPUT_HEADER
    else
    {
        int axes=0, buttons=0;
        axes = count_abs_axis(fd);
        buttons = count_buttons(fd, NULL);
        is_gamepad = (axes == 6  && buttons >= 14);
    }
#endif
    if (input == (WORD)-1 && is_gamepad)
        input = 0;


    TRACE("Found udev device %s (vid %04x, pid %04x, version %u, serial %s)\n",
          debugstr_a(devnode), vid, pid, version, debugstr_w(serial));

    if (strcmp(subsystem, "hidraw") == 0)
    {
        device = bus_create_hid_device(hidraw_busidW, vid, pid, input, version, 0, serial, is_gamepad,
                                       &hidraw_vtbl, sizeof(struct platform_private));
    }
#ifdef HAS_PROPER_INPUT_HEADER
    else if (strcmp(subsystem, "input") == 0)
    {
        device = bus_create_hid_device(lnxev_busidW, vid, pid, input, version, 0, serial, is_gamepad,
                                       &lnxev_vtbl, sizeof(struct wine_input_private));
    }
#endif

    if (device)
    {
        struct platform_private *private = impl_from_DEVICE_OBJECT(device);
        private->udev_device = udev_device_ref(dev);
        private->device_fd = fd;
#ifdef HAS_PROPER_INPUT_HEADER
        if (strcmp(subsystem, "input") == 0)
            if (!build_report_descriptor((struct wine_input_private*)private, dev))
            {
                ERR("Building report descriptor failed, removing device\n");
                close(fd);
                udev_device_unref(dev);
                bus_unlink_hid_device(device);
                bus_remove_hid_device(device);
                HeapFree(GetProcessHeap(), 0, serial);
                return;
            }
#endif
        IoInvalidateDeviceRelations(bus_pdo, BusRelations);
    }
    else
    {
        WARN("Ignoring device %s with subsystem %s\n", debugstr_a(devnode), subsystem);
        close(fd);
    }

    HeapFree(GetProcessHeap(), 0, serial);
}

static void try_remove_device(struct udev_device *dev)
{
    DEVICE_OBJECT *device = NULL;
    struct platform_private* private;
#ifdef HAS_PROPER_INPUT_HEADER
    BOOL is_input = FALSE;
#endif

    device = bus_find_hid_device(&hidraw_vtbl, dev);
#ifdef HAS_PROPER_INPUT_HEADER
    if (device == NULL)
    {
        device = bus_find_hid_device(&lnxev_vtbl, dev);
        is_input = TRUE;
    }
#endif
    if (!device) return;

    bus_unlink_hid_device(device);
    IoInvalidateDeviceRelations(bus_pdo, BusRelations);

    private = impl_from_DEVICE_OBJECT(device);

    if (private->report_thread)
    {
        write(private->control_pipe[1], "q", 1);
        WaitForSingleObject(private->report_thread, INFINITE);
        close(private->control_pipe[0]);
        close(private->control_pipe[1]);
        CloseHandle(private->report_thread);
#ifdef HAS_PROPER_INPUT_HEADER
        if (strcmp(udev_device_get_subsystem(dev), "input") == 0)
        {
            HeapFree(GetProcessHeap(), 0, ((struct wine_input_private*)private)->current_report_buffer);
            HeapFree(GetProcessHeap(), 0, ((struct wine_input_private*)private)->last_report_buffer);
        }
#endif
    }

#ifdef HAS_PROPER_INPUT_HEADER
    if (is_input)
    {
        struct wine_input_private *ext = (struct wine_input_private*)private;
        HeapFree(GetProcessHeap(), 0, ext->report_descriptor);
    }
#endif

    dev = private->udev_device;
    close(private->device_fd);
    bus_remove_hid_device(device);
    udev_device_unref(dev);
}

static void build_initial_deviceset(void)
{
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;

    enumerate = udev_enumerate_new(udev_context);
    if (!enumerate)
    {
        WARN("Unable to create udev enumeration object\n");
        return;
    }

    if (!disable_hidraw)
        if (udev_enumerate_add_match_subsystem(enumerate, "hidraw") < 0)
            WARN("Failed to add subsystem 'hidraw' to enumeration\n");
#ifdef HAS_PROPER_INPUT_HEADER
    if (!disable_input)
    {
        if (udev_enumerate_add_match_subsystem(enumerate, "input") < 0)
            WARN("Failed to add subsystem 'input' to enumeration\n");
    }
#endif

    if (udev_enumerate_scan_devices(enumerate) < 0)
        WARN("Enumeration scan failed\n");

    devices = udev_enumerate_get_list_entry(enumerate);
    udev_list_entry_foreach(dev_list_entry, devices)
    {
        struct udev_device *dev;
        const char *path;

        path = udev_list_entry_get_name(dev_list_entry);
        if ((dev = udev_device_new_from_syspath(udev_context, path)))
        {
            try_add_device(dev);
            udev_device_unref(dev);
        }
    }

    udev_enumerate_unref(enumerate);
}

static struct udev_monitor *create_monitor(struct pollfd *pfd)
{
    struct udev_monitor *monitor;
    int systems = 0;

    monitor = udev_monitor_new_from_netlink(udev_context, "udev");
    if (!monitor)
    {
        WARN("Unable to get udev monitor object\n");
        return NULL;
    }

    if (!disable_hidraw)
    {
        if (udev_monitor_filter_add_match_subsystem_devtype(monitor, "hidraw", NULL) < 0)
            WARN("Failed to add 'hidraw' subsystem to monitor\n");
        else
            systems++;
    }
#ifdef HAS_PROPER_INPUT_HEADER
    if (!disable_input)
    {
        if (udev_monitor_filter_add_match_subsystem_devtype(monitor, "input", NULL) < 0)
            WARN("Failed to add 'input' subsystem to monitor\n");
        else
            systems++;
    }
#endif
    if (systems == 0)
    {
        WARN("No subsystems added to monitor\n");
        goto error;
    }

    if (udev_monitor_enable_receiving(monitor) < 0)
        goto error;

    if ((pfd->fd = udev_monitor_get_fd(monitor)) >= 0)
    {
        pfd->events = POLLIN;
        return monitor;
    }

error:
    WARN("Failed to start monitoring\n");
    udev_monitor_unref(monitor);
    return NULL;
}

static void process_monitor_event(struct udev_monitor *monitor)
{
    struct udev_device *dev;
    const char *action;

    dev = udev_monitor_receive_device(monitor);
    if (!dev)
    {
        FIXME("Failed to get device that has changed\n");
        return;
    }

    action = udev_device_get_action(dev);
    TRACE("Received action %s for udev device %s\n", debugstr_a(action),
          debugstr_a(udev_device_get_devnode(dev)));

    if (!action)
        WARN("No action received\n");
    else if (strcmp(action, "add") == 0)
        try_add_device(dev);
    else if (strcmp(action, "remove") == 0)
        try_remove_device(dev);
    else
        WARN("Unhandled action %s\n", debugstr_a(action));

    udev_device_unref(dev);
}

static DWORD CALLBACK deviceloop_thread(void *args)
{
    struct udev_monitor *monitor;
    HANDLE init_done = args;
    struct pollfd pfd[2];

    pfd[1].fd = deviceloop_control[0];
    pfd[1].events = POLLIN;
    pfd[1].revents = 0;

    monitor = create_monitor(&pfd[0]);
    build_initial_deviceset();
    SetEvent(init_done);

    while (monitor)
    {
        if (poll(pfd, 2, -1) <= 0) continue;
        if (pfd[1].revents) break;
        process_monitor_event(monitor);
    }

    TRACE("Monitor thread exiting\n");
    if (monitor)
        udev_monitor_unref(monitor);
    return 0;
}

static int device_unload(DEVICE_OBJECT *device, void *context)
{
    try_remove_device(impl_from_DEVICE_OBJECT(device)->udev_device);
    return 1;
}

void udev_driver_unload( void )
{
    TRACE("Unload Driver\n");

    if (!deviceloop_handle)
        return;

    write(deviceloop_control[1], "q", 1);
    WaitForSingleObject(deviceloop_handle, INFINITE);
    close(deviceloop_control[0]);
    close(deviceloop_control[1]);
    CloseHandle(deviceloop_handle);

    bus_enumerate_hid_devices(&hidraw_vtbl, device_unload, NULL);
#ifdef HAS_PROPER_INPUT_HEADER
    bus_enumerate_hid_devices(&lnxev_vtbl, device_unload, NULL);
#endif
}

NTSTATUS udev_driver_init(void)
{
    HANDLE events[2];
    DWORD result;
    static const WCHAR hidraw_disabledW[] = {'D','i','s','a','b','l','e','H','i','d','r','a','w',0};
    static const UNICODE_STRING hidraw_disabled = {sizeof(hidraw_disabledW) - sizeof(WCHAR), sizeof(hidraw_disabledW), (WCHAR*)hidraw_disabledW};
    static const WCHAR input_disabledW[] = {'D','i','s','a','b','l','e','I','n','p','u','t',0};
    static const UNICODE_STRING input_disabled = {sizeof(input_disabledW) - sizeof(WCHAR), sizeof(input_disabledW), (WCHAR*)input_disabledW};

    if (pipe(deviceloop_control) != 0)
    {
        ERR("Control pipe creation failed\n");
        return STATUS_UNSUCCESSFUL;
    }

    if (!(udev_context = udev_new()))
    {
        ERR("Can't create udev object\n");
        goto error;
    }

    disable_hidraw = check_bus_option(&hidraw_disabled, 0);
    if (disable_hidraw)
        TRACE("UDEV hidraw devices disabled in registry\n");

#ifdef HAS_PROPER_INPUT_HEADER
    disable_input = check_bus_option(&input_disabled, 0);
    if (disable_input)
        TRACE("UDEV input devices disabled in registry\n");
#endif

    if (!(events[0] = CreateEventW(NULL, TRUE, FALSE, NULL)))
        goto error;
    if (!(events[1] = CreateThread(NULL, 0, deviceloop_thread, events[0], 0, NULL)))
    {
        CloseHandle(events[0]);
        goto error;
    }

    result = WaitForMultipleObjects(2, events, FALSE, INFINITE);
    CloseHandle(events[0]);
    if (result == WAIT_OBJECT_0)
    {
        deviceloop_handle = events[1];
        TRACE("Initialization successful\n");
        return STATUS_SUCCESS;
    }
    CloseHandle(events[1]);

error:
    ERR("Failed to initialize udev device thread\n");
    close(deviceloop_control[0]);
    close(deviceloop_control[1]);
    if (udev_context)
    {
        udev_unref(udev_context);
        udev_context = NULL;
    }
    return STATUS_UNSUCCESSFUL;
}

#else

NTSTATUS udev_driver_init(void)
{
    return STATUS_NOT_IMPLEMENTED;
}

void udev_driver_unload( void )
{
    TRACE("Stub: Unload Driver\n");
}

#endif /* HAVE_UDEV */
