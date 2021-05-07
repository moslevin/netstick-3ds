// Netstick-3ds - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.

#pragma once

#include "options.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "joystick.h"
#include "options.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct hid_device;

//---------------------------------------------------------------------------
// Callouts invoked to handle initialization + event handling for a HID device
typedef bool (*hid_config_handler_t)(struct hid_device* device_, const program_options_t* options_);
typedef bool (*hid_event_handler_t)(struct hid_device* device_, const program_options_t* options_);

//---------------------------------------------------------------------------
// Generic HID device data strcture, which can be specialized for any specific
// type of input device supported by the system
typedef struct hid_device {
    const char* name;
    bool        isInit;
    int         sockFd;
    js_config_t config;
    uint8_t*    rawReport;
    size_t      rawReportSize;

    hid_config_handler_t configHandlerFn;
    hid_event_handler_t  eventHandlerFn;
} hid_device_t;

//---------------------------------------------------------------------------
/**
 * @brief hid_device_init Initialize a HID device object for use as a specific
 * type of device.
 * @param device_ pointer to the object used to create the object
 * @param name_ name used to give the device additional context in logging functions
 * @param configHandler_ function invoked during HID device configuration
 * @param eventHandler_ function invoked during HID device event handling
 * @param options_ program options object used in configuring the device
 * @return true on success, false on failure
 */
bool hid_device_init(hid_device_t*            device_,
                     const char*              name_,
                     hid_config_handler_t     configHandler_,
                     hid_event_handler_t      eventHandler_,
                     const program_options_t* options_);

//---------------------------------------------------------------------------
/**
 * @brief handle_hid_events run the HID event handling routine associated with
 * the device.  This abstracts the HID device's periodic event polling logic,
 * and performs connection management operations on behalf of the HID device.
 * @param device_ pointer to the HID device object to process
 * @param options_ program options, used by the device to choose how to process
 * its event data.
 * @return true on success, false on failure
 */
bool handle_hid_events(hid_device_t* device_, const program_options_t* options_);

#if defined(__cplusplus)
} // extern "C"
#endif
