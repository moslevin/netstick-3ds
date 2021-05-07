// Netstick-3ds - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

//---------------------------------------------------------------------------
typedef struct {
    char host[64];           //!< Hostname (IP) of the device to connect to
    bool hostFound;          //!< Hostname was found in the config file
    int  port;               //!< Port of the device to connect to
    bool portFound;          //!< Port was found in the config file
    bool swapAB;             //!< Swap A/B buttons on the gamepad
    bool swapXY;             //!< Swap X/Y buttons on the gamepad
    bool invertCStickX;      //!< Invert the X axis on the CStick
    bool invertCStickY;      //!< Invert the Y axis on the CStick
    bool invertCirclePadX;   //!< Invert the X axis on the circlepad
    bool invertCirclePadY;   //!< Invert the Y axis on the circlepad
    bool useTouch;           //!< Send touchpad events on its own socket
    bool sendTouchDownEvent; //!< Sena a touch-down event with every press (and release when touch removed)
    bool useAccel;           //!< Send accelerometer events on its own socket
    bool useGyro;            //!< Send gyro events on its own socket
    int  touchOffset; //!< Set the offset from the edges of the screen that are ignored for touch events (reduce active
                      //!< area to make it easier to reach corners)
    bool useSteeringControls; //!< Send motion-based steering-wheel controls with the gamepad device
} program_options_t;

//---------------------------------------------------------------------------
/**
 * @brief program_options_init Initialize a program_options_t structure prior
 * to its use.  This must be called prior using the object.
 *
 * @param options_ pointer to the struct to initialize
 */
void program_options_init(program_options_t* options_);

//---------------------------------------------------------------------------
/**
 * @brief program_options_load Read the contents of a config file, and load
 * its configuration values into the provided object.
 * @param options_ Initialized option that will contain the configuration data
 * after parsing the file
 * @param filePath_ File to read containing configuration data
 * @return true on success, false on error reading file/missing mandatory options
 */
bool program_options_load(program_options_t* options_, const char* filePath_);

#if defined(__cplusplus)
}
#endif
