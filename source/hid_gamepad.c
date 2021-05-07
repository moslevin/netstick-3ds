// Netstick-3ds - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.

#include "hid_device.h"
#include "hid_common.h"

#include "net_util.h"
#include <math.h>

#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <3ds.h>

#define NINTENDO_3DS_NAME_GAMEPAD (NINTENDO_3DS_NAME " - Gamepad")

//---------------------------------------------------------------------------
#define RAW_REPORT_SIZE                                                                                                \
    ((sizeof(uint8_t) * NDS_BUTTON_COUNT) + (sizeof(int32_t) * NDS_ABS_AXIS_COUNT)                                     \
     + (sizeof(int32_t) * NDS_REL_AXIS_COUNT))

#define ABS(x) (((x) < 0) ? ((x) * -1) : (x))

//---------------------------------------------------------------------------
// Indexes of the 3DS buttons in the report structure -- for the gamepad
#define NDS_IDX_A 0
#define NDS_IDX_B 1
#define NDS_IDX_SELECT 2
#define NDS_IDX_START 3
#define NDS_IDX_DPAD_RIGHT 4
#define NDS_IDX_DPAD_LEFT 5
#define NDS_IDX_DPAD_UP 6
#define NDS_IDX_DPAD_DOWN 7
#define NDS_IDX_R 8
#define NDS_IDX_L 9
#define NDS_IDX_X 10
#define NDS_IDX_Y 11
#define NDS_IDX_ZL 12
#define NDS_IDX_ZR 13
#define NDS_IDX_CSTICK_RIGHT 14
#define NDS_IDX_CSTICK_LEFT 15
#define NDS_IDX_CSTICK_UP 16
#define NDS_IDX_CSTICK_DOWN 17
#define NDS_BUTTON_COUNT 18 // Raw buttons

#define NDS_ABS_IDX_X 0
#define NDS_ABS_IDX_Y 1
#define NDS_ABS_IDX_RX 2
#define NDS_ABS_IDX_RY 3
#define NDS_ABS_IDX_WHEEL 4
#define NDS_ABS_AXIS_COUNT 5

#define NDS_REL_AXIS_COUNT 0

//---------------------------------------------------------------------------
#define WHEEL_MAX (255)
#define WHEEL_MIN (-255)
#define COARSE_DEADZONE_COUNT (6)
#define FINE_DEADZONE_COUNT (3)
#define MAX_ANGLE (45)
//---------------------------------------------------------------------------

#define MAP_MAX ((size_t)(32))
//---------------------------------------------------------------------------
typedef struct {
    int buttons[MAP_MAX];
} js_index_map_t;

//---------------------------------------------------------------------------
static js_index_map_t indexMap;

//---------------------------------------------------------------------------
static void js_index_map_init(js_index_map_t* indexMap_)
{
    for (size_t i = 0; i < MAP_MAX; i++) { indexMap_->buttons[i] = -1; }
}

//---------------------------------------------------------------------------
static void js_index_map_set(js_index_map_t* indexMap_, int eventType_, int eventId_, int index_)
{
    if (eventType_ == EV_KEY) {
        indexMap_->buttons[eventId_] = index_;
    }
}

//---------------------------------------------------------------------------
static int js_index_map_get_index(js_index_map_t* indexMap_, int eventType_, int eventId_)
{
    if (eventType_ == EV_KEY) {
        return indexMap_->buttons[eventId_];
    }
    return -1;
}

//---------------------------------------------------------------------------
// Initialize the configuration data structure for a 3DS joystick.  There's
// no joystick enumeration process, but we do have to map the 3DS button/axis
// identifiers to their corresponding linux values.
static bool hid_gamepad_config(hid_device_t* device_, const program_options_t* options_)
{
    (void)options_;
    js_config_t* config = &device_->config;

    // Set the device identifiers for the 3DS
    config->pid = NINTENDO_USB_PID; // Dummy value
    config->vid = NINTENDO_USB_VID;
    strcpy(config->name, NINTENDO_3DS_NAME_GAMEPAD);

    // Set the count of buttons/axis supported by the 3DS
    config->absAxisCount = NDS_ABS_AXIS_COUNT;
    config->buttonCount  = NDS_BUTTON_COUNT;
    config->relAxisCount = NDS_REL_AXIS_COUNT;

    // Set up the 3DS's absolution-axis identifiers, mapping the supported
    // absolute axis IDs to their corresponding IDs on linux

    config->absAxis[NDS_ABS_IDX_X]     = LINUX_ABS_X;
    config->absAxis[NDS_ABS_IDX_Y]     = LINUX_ABS_Y;
    config->absAxis[NDS_ABS_IDX_RX]    = LINUX_ABS_RX;
    config->absAxis[NDS_ABS_IDX_RY]    = LINUX_ABS_RY;
    config->absAxis[NDS_ABS_IDX_WHEEL] = LINUX_ABS_WHEEL;

    // Set up the absolute axis limits for circle pad
    for (int i = 0; i < 4; i++) {
        config->absAxisMin[i] = NDS_CIRCLE_PAD_MIN;
        config->absAxisMax[i] = NDS_CIRCLE_PAD_MAX;
    }

    // Setup a steering wheel axis.
    config->absAxisMin[NDS_ABS_IDX_WHEEL] = WHEEL_MIN;
    config->absAxisMax[NDS_ABS_IDX_WHEEL] = WHEEL_MAX;

    // Set up the mappings between the 3DS's buttons and the corresponding
    // linux IDs.
    config->buttons[NDS_IDX_A]          = LINUX_BTN_EAST;
    config->buttons[NDS_IDX_B]          = LINUX_BTN_SOUTH;
    config->buttons[NDS_IDX_SELECT]     = LINUX_BTN_SELECT;
    config->buttons[NDS_IDX_START]      = LINUX_BTN_START;
    config->buttons[NDS_IDX_DPAD_RIGHT] = LINUX_BTN_DPAD_RIGHT;
    config->buttons[NDS_IDX_DPAD_LEFT]  = LINUX_BTN_DPAD_LEFT;
    config->buttons[NDS_IDX_DPAD_UP]    = LINUX_BTN_DPAD_UP;
    config->buttons[NDS_IDX_DPAD_DOWN]  = LINUX_BTN_DPAD_DOWN;
    config->buttons[NDS_IDX_R]          = LINUX_BTN_TR;
    config->buttons[NDS_IDX_L]          = LINUX_BTN_TL;
    config->buttons[NDS_IDX_X]          = LINUX_BTN_NORTH;
    config->buttons[NDS_IDX_Y]          = LINUX_BTN_WEST;
    config->buttons[NDS_IDX_ZL]         = LINUX_BTN_TL2;
    config->buttons[NDS_IDX_ZR]         = LINUX_BTN_TR2;

    device_->rawReportSize = RAW_REPORT_SIZE;
    device_->rawReport     = (uint8_t*)malloc(device_->rawReportSize);

    return true;
}

//---------------------------------------------------------------------------
// Create a simulated steering wheel control by using the combination of X and
// Y accelerometer values.
static int32_t hid_steering_wheel_value()
{
    accelVector accel;
    hidAccelRead(&accel);

    // Apply a coarse deadzone to the report
    if (ABS(accel.x) < COARSE_DEADZONE_COUNT) {
        return 0;
    }

    // Compute vector and compute angle in X-Y plane (using X-Z axis from accel)
    double fX  = (float)accel.x;
    double fY  = (float)accel.z;
    double vec = sqrt((fX * fX) + (fY * fY));

    // Get the X/Y component values, normalized to the vector.
    if (fabs(vec) > 1.0e-6) {
        fX = fX / vec;
        fY = fY / vec;
    } else {
        // If the vector is too small to reliable to use in computations, bail
        return 0;
    }

    // Get the Angle in the X/Y plane
    double angle = 0.0f;
    if (fabs(fX) > 1.0e-6) {
        angle = atan(fY / fX) * (180.0 / 3.141592);
    }

    // Convert angle to degrees, then clamp and scale the result to the
    // resolution of the axis as set in the report.  Interpret out-of-bounds
    // data in a way that makes sense.

    // If the X accel value is +ve (wheel turned counterclockwise)
    if (fX > 0.0f) {
        // If the measured angle doesn't make sense/Y-axis indicates the unit it
        // held upside down, etc.
        if (angle > 0.0f) {
            // Return a default value that won't cause the axis to jump.
            return (float)(WHEEL_MIN);
        } else {
            // Normalize result to value between 0 and 1
            angle = ((angle + 90.0f) / MAX_ANGLE);
            if (angle >= 1.0f) {
                angle = 1.0f;
            }
        }
    } else {
        // If the measured angle doesn't make sense/Y-axis indicates the unit it
        // held upside down, etc.
        if (angle < 0.0f) {
            // Return a default value that won't cause the axis to jump.
            return (float)(WHEEL_MAX);
        } else {
            // Normalize result to value between 0 and -1
            angle = ((angle - 90.0f) / MAX_ANGLE);
            if (angle <= -1.0f) {
                angle = -1.0f;
            }
        }
    }

    // scale to resolution of the device
    angle *= (WHEEL_MIN);

    // "Fine" deadzone detection...
    if (fabs(angle) < (float)(FINE_DEADZONE_COUNT)) {
        angle = 0;
    }

    // Return as int.
    return (int32_t)(angle);
}

//---------------------------------------------------------------------------
static bool hid_gamepad_event(hid_device_t* device_, const program_options_t* options_)
{
    js_report_t  report = {};
    js_config_t* config = &device_->config;

    uint8_t* rawReport = device_->rawReport;
    report.absAxis     = (int32_t*)rawReport;
    report.relAxis     = (int32_t*)(rawReport + (sizeof(int32_t) * config->absAxisCount));
    report.buttons
        = (uint8_t*)(rawReport + (sizeof(int32_t) * config->absAxisCount) + (sizeof(int32_t) * config->relAxisCount));

    //!! ToDo -- clean this up to avoid statics.
    static uint32_t       lastKeys   = 0;
    static circlePosition lastCircle = {};
    static circlePosition lastCstick = {};
    static int32_t        lastWheel  = 0;

    uint32_t keys  = hidKeysHeld();
    int32_t  wheel = 0;

    circlePosition circle;
    circlePosition cstick;

    bool doUpdate = true;

    hidCircleRead(&circle);
    hidCstickRead(&cstick);

    if (options_->invertCirclePadX) {
        circle.dx *= -1;
    }
    if (options_->invertCirclePadY) {
        circle.dy *= -1;
    }
    if (options_->invertCStickX) {
        cstick.dx *= -1;
    }
    if (options_->invertCStickY) {
        cstick.dy *= -1;
    }
    if (options_->useSteeringControls) {
        wheel = hid_steering_wheel_value();
    }

    if ((circle.dx == lastCircle.dx) && (circle.dy == lastCircle.dy) && (cstick.dx == lastCstick.dx)
        && (cstick.dy == lastCstick.dy) && (keys == lastKeys) && (lastWheel == wheel)) {
        doUpdate = false;
    }

    lastCircle = circle;
    lastCstick = cstick;
    lastKeys   = keys;
    lastWheel  = wheel;

    if (doUpdate) {
        for (int bit = 0; bit < 32; bit++) {
            int idx = js_index_map_get_index(&indexMap, EV_KEY, bit);
            if (idx != -1) {
                int val = 0;
                if (keys & (1 << bit)) {
                    val = 1;
                }
                report.buttons[idx] = val;
            }
        }

        report.absAxis[0] = circle.dx;
        report.absAxis[1] = circle.dy;
        report.absAxis[2] = cstick.dx;
        report.absAxis[3] = cstick.dy;
        report.absAxis[4] = wheel;

        // Swap A/B values if configured as such
        if (options_->swapAB) {
            int tmp                   = report.buttons[NDS_IDX_B];
            report.buttons[NDS_IDX_B] = report.buttons[NDS_IDX_A];
            report.buttons[NDS_IDX_A] = tmp;
        }

        // Swap X/Y values if configured as such
        if (options_->swapXY) {
            int tmp                   = report.buttons[NDS_IDX_Y];
            report.buttons[NDS_IDX_Y] = report.buttons[NDS_IDX_X];
            report.buttons[NDS_IDX_X] = tmp;
        }

        return net_util_encode_and_transmit(device_->sockFd, 1, device_->rawReport, device_->rawReportSize);
    }
    return true;
}

//---------------------------------------------------------------------------
bool hid_gamepad_init(hid_device_t* device_, const program_options_t* options_)
{
    // We're going to use a lookup table to map the 3ds button enums to their indexes in the config object.
    //!! ToDo -- ensure this can only be done once; not a huge deal since the same index map applies globally
    js_index_map_init(&indexMap);

    js_index_map_set(&indexMap, EV_KEY, NDS_KEY_A, NDS_IDX_A);
    js_index_map_set(&indexMap, EV_KEY, NDS_KEY_B, NDS_IDX_B);

    js_index_map_set(&indexMap, EV_KEY, NDS_KEY_X, NDS_IDX_X);
    js_index_map_set(&indexMap, EV_KEY, NDS_KEY_Y, NDS_IDX_Y);

    js_index_map_set(&indexMap, EV_KEY, NDS_KEY_SELECT, NDS_IDX_SELECT);
    js_index_map_set(&indexMap, EV_KEY, NDS_KEY_START, NDS_IDX_START);
    js_index_map_set(&indexMap, EV_KEY, NDS_KEY_DPAD_RIGHT, NDS_IDX_DPAD_RIGHT);
    js_index_map_set(&indexMap, EV_KEY, NDS_KEY_DPAD_LEFT, NDS_IDX_DPAD_LEFT);
    js_index_map_set(&indexMap, EV_KEY, NDS_KEY_DPAD_UP, NDS_IDX_DPAD_UP);
    js_index_map_set(&indexMap, EV_KEY, NDS_KEY_DPAD_DOWN, NDS_IDX_DPAD_DOWN);
    js_index_map_set(&indexMap, EV_KEY, NDS_KEY_R, NDS_IDX_R);
    js_index_map_set(&indexMap, EV_KEY, NDS_KEY_L, NDS_IDX_L);
    js_index_map_set(&indexMap, EV_KEY, NDS_KEY_ZL, NDS_IDX_ZL);
    js_index_map_set(&indexMap, EV_KEY, NDS_KEY_ZR, NDS_IDX_ZR);

    return hid_device_init(device_, "gamepad", hid_gamepad_config, hid_gamepad_event, options_);
}
