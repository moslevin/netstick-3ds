// Netstick-3ds - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.

#include "hid_device.h"
#include "hid_common.h"

#include "net_util.h"

#include <string.h>
#include <malloc.h>

#include <3ds.h>

#define NINTENDO_3DS_NAME_GYRO (NINTENDO_3DS_NAME " - Gyroscope")
#define RAW_REPORT_GYRO_SIZE (3 * sizeof(int32_t))

//---------------------------------------------------------------------------
static bool hid_gyro_config(hid_device_t* device_, const program_options_t* options_)
{
    (void)options_;

    js_config_t* config = &device_->config;

    // Set the device identifiers for the 3DS
    config->pid = NINTENDO_USB_PID + 3; // Dummy value
    config->vid = NINTENDO_USB_VID;
    strcpy(config->name, NINTENDO_3DS_NAME_GYRO);

    config->absAxisCount = 3;
    config->buttonCount  = 0;
    config->relAxisCount = 0;

    // Set up the 3DS's absolution-axis identifiers, mapping the supported
    // absolute axis IDs to their corresponding IDs on linux
    config->absAxis[0] = LINUX_ABS_X;
    config->absAxis[1] = LINUX_ABS_Y;
    config->absAxis[2] = LINUX_ABS_Z;

    for (int i = 0; i < 3; i++) {
        config->absAxisMin[i]  = NDS_GYRO_MIN;
        config->absAxisMax[i]  = NDS_GYRO_MAX;
        config->absAxisFuzz[i] = NDS_GYRO_FUZZ;
    }

    device_->rawReportSize = RAW_REPORT_GYRO_SIZE;
    device_->rawReport = (uint8_t*)malloc(device_->rawReportSize);

    return true;
}

//---------------------------------------------------------------------------
static bool hid_gyro_event(hid_device_t* device_, const program_options_t* options_)
{
    (void)options_;

    js_report_t report = {};
    js_config_t* config = &device_->config;

    uint8_t* rawReport = device_->rawReport;
    report.absAxis     = (int32_t*)rawReport;
    report.relAxis     = (int32_t*)(rawReport + (sizeof(int32_t) * config->absAxisCount));
    report.buttons
        = (uint8_t*)(rawReport + (sizeof(int32_t) * config->absAxisCount) + (sizeof(int32_t) * config->relAxisCount));

    angularRate gyro;
    hidGyroRead(&gyro);

    report.absAxis[0] = gyro.x;
    report.absAxis[1] = gyro.y;
    report.absAxis[2] = gyro.z;

    return net_util_encode_and_transmit(device_->sockFd, 1, device_->rawReport, device_->rawReportSize);
}

//---------------------------------------------------------------------------
bool hid_gyro_init(hid_device_t* device_, const program_options_t* options_) {
    return hid_device_init(device_, "gyro", hid_gyro_config, hid_gyro_event, options_);
}
