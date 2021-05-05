// Netstick-3ds - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.

#include "hid_device.h"
#include "hid_common.h"

#include "net_util.h"

#include <string.h>
#include <malloc.h>

#include <3ds.h>

//---------------------------------------------------------------------------
#define NINTENDO_3DS_NAME_TOUCH (NINTENDO_3DS_NAME " - Touchscreen")

//---------------------------------------------------------------------------
#define TOUCHSCREEN_WIDTH   (320)
#define TOUCHSCREEN_HEIGHT  (240)

//---------------------------------------------------------------------------
#define RAW_REPORT_TOUCH_SIZE (sizeof(uint8_t) + (2 * sizeof(int32_t)))

//---------------------------------------------------------------------------
static bool hid_touch_config(hid_device_t* device_, const program_options_t* options_)
{
    js_config_t* config = &device_->config;

    // Set the device identifiers for the 3DS
    config->pid = NINTENDO_USB_PID + 1; // Dummy value
    config->vid = NINTENDO_USB_VID;
    strcpy(config->name, NINTENDO_3DS_NAME_TOUCH);

    // Set the count of buttons/axis supported by the 3DS
    config->absAxisCount = 2;

    // Set up a single button to handle touch events
    config->buttonCount = 1;
    config->buttons[0]  = LINUX_BTN_TOUCH;

    config->relAxisCount = 0;

    // Set up the 3DS's touchscreen parameters
    config->absAxis[0] = LINUX_ABS_X;
    config->absAxis[1] = LINUX_ABS_Y;

    config->absAxisMin[0] = 0;
    config->absAxisMax[0] = TOUCHSCREEN_WIDTH - (options_->touchOffset * 2);

    config->absAxisMin[1] = 0;
    config->absAxisMax[1] = TOUCHSCREEN_HEIGHT - (options_->touchOffset * 2);

    device_->rawReportSize = RAW_REPORT_TOUCH_SIZE;
    device_->rawReport = (uint8_t*)malloc(device_->rawReportSize);

    return true;
}


//---------------------------------------------------------------------------
static bool hid_touch_event(hid_device_t* device_, const program_options_t* options_)
{
    js_report_t report = {};
    js_config_t* config = &device_->config;

    uint8_t* rawReport = device_->rawReport;
    report.absAxis     = (int32_t*)rawReport;
    report.relAxis     = (int32_t*)(rawReport + (sizeof(int32_t) * config->absAxisCount));
    report.buttons
        = (uint8_t*)(rawReport + (sizeof(int32_t) * config->absAxisCount) + (sizeof(int32_t) * config->relAxisCount));

    bool doUpdate = true;

    static int32_t  lastX    = 0;
    static int32_t  lastY    = 0;
    static uint32_t lastKeys = 0;

    uint32_t      keys = hidKeysHeld();
    touchPosition touch;
    hidTouchRead(&touch);

    // clamp touch events to limits set by border offsets
    if (touch.px < options_->touchOffset) {
        touch.px = 0 ;
    } else if (touch.px > (TOUCHSCREEN_WIDTH - (options_->touchOffset * 2))) {
        touch.px = TOUCHSCREEN_WIDTH - (options_->touchOffset * 2);
    } else {
        touch.px -= options_->touchOffset;
    }

    if (touch.py < options_->touchOffset) {
        touch.py = 0;
    } else if (touch.py > (TOUCHSCREEN_HEIGHT - (options_->touchOffset * 2))) {
        touch.py = TOUCHSCREEN_HEIGHT - (options_->touchOffset * 2);
    } else {
        touch.py -= options_->touchOffset;
    }

    // Only send an update if there's a change in the data...
    if ((keys == lastKeys) && (touch.px == lastX) && (touch.py == lastY)) {
        doUpdate = false;
    }

    if (keys & (1 << NDS_KEY_TOUCH)) {
        report.absAxis[0] = touch.px;
        report.absAxis[1] = touch.py;

        lastX = touch.px;
        lastY = touch.py;

        if (options_->sendTouchDownEvent == true) {
            report.buttons[0] = 1;
        }
    } else {
        report.absAxis[0] = lastX;
        report.absAxis[1] = lastY;

        if (options_->sendTouchDownEvent == true) {
            report.buttons[0] = 0;
        }
    }

    lastKeys = keys;

    if (doUpdate) {
        return net_util_encode_and_transmit(device_->sockFd, 1, device_->rawReport, device_->rawReportSize);
    }
    return true;
}

//---------------------------------------------------------------------------
bool hid_touch_init(hid_device_t* device_, const program_options_t* options_) {
    return hid_device_init(device_, "touch", hid_touch_config, hid_touch_event, options_);
}
