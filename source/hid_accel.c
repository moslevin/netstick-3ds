// Netstick-3ds - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.

#include "hid_device.h"
#include "hid_common.h"

#include "net_util.h"

#include <string.h>
#include <malloc.h>

#include <3ds.h>

#define NINTENDO_3DS_NAME_MOTION (NINTENDO_3DS_NAME " - Accelerometer")

#define RAW_REPORT_ACCEL_SIZE (3 * sizeof(int32_t))

//---------------------------------------------------------------------------
static bool hid_accel_config(hid_device_t* device_, const program_options_t* options_)
{
    (void)options_;

    // Set the device identifiers for the 3DS
    js_config_t* config = &device_->config;
    config->pid         = NINTENDO_USB_PID + 2; // Dummy value
    config->vid         = NINTENDO_USB_VID;
    strcpy(config->name, NINTENDO_3DS_NAME_MOTION);

    config->absAxisCount = 3;
    config->buttonCount  = 0;
    config->relAxisCount = 0;

    // Set up the 3DS's absolution-axis identifiers, mapping the supported
    // absolute axis IDs to their corresponding IDs on linux
    config->absAxis[0] = LINUX_ABS_X;
    config->absAxis[1] = LINUX_ABS_Y;
    config->absAxis[2] = LINUX_ABS_Z;

    for (int i = 0; i < 3; i++) {
        config->absAxisMin[i]  = NDS_ACCEL_MIN;
        config->absAxisMax[i]  = NDS_ACCEL_MAX;
        config->absAxisFuzz[i] = NDS_ACCEL_FUZZ;
    }

    device_->rawReportSize = RAW_REPORT_ACCEL_SIZE;
    device_->rawReport     = (uint8_t*)malloc(device_->rawReportSize);

    return true;
}

//---------------------------------------------------------------------------
static bool hid_accel_event(hid_device_t* device_, const program_options_t* options_)
{
    (void)options_;

    js_report_t report = {};

    uint8_t*     rawReport = device_->rawReport;
    js_config_t* config    = &device_->config;

    report.absAxis = (int32_t*)rawReport;
    report.relAxis = (int32_t*)(rawReport + (sizeof(int32_t) * config->absAxisCount));
    report.buttons
        = (uint8_t*)(rawReport + (sizeof(int32_t) * config->absAxisCount) + (sizeof(int32_t) * config->relAxisCount));

    accelVector accel;
    hidAccelRead(&accel);

    report.absAxis[0] = accel.x;
    report.absAxis[1] = accel.y;
    report.absAxis[2] = accel.z;

    return net_util_encode_and_transmit(device_->sockFd, 1, device_->rawReport, device_->rawReportSize);
}

//---------------------------------------------------------------------------
bool hid_accel_init(hid_device_t* device_, const program_options_t* options_)
{
    return hid_device_init(device_, "accel", hid_accel_config, hid_accel_event, options_);
}
