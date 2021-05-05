// Netstick-3ds - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.

#pragma once

#include <stdbool.h>
#include "hid_device.h"
#include "options.h"

#if defined(__cplusplus)
extern "C" {
#endif

//---------------------------------------------------------------------------
/**
 * @brief hid_touch_init Specialize a hid_device_t object for use as a
 * touchscreen device.
 *
 * @param device_ uninitialized object that will become a touchscreen HID
 * device on success.
 *
 * @param options_ program-options structure - used to initialize the device
 * according to the options passed by the user.
 *
 * @return true if object is successfully initialized.
 */
bool hid_touch_init(hid_device_t* device_, const program_options_t* options_);

#if defined(__cplusplus)
} // extern "C"
#endif
