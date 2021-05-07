// Netstick-3ds - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <3ds.h>

#include "hid_device.h"
#include "hid_accel.h"
#include "hid_gyro.h"
#include "hid_touch.h"
#include "hid_gamepad.h"

#include "joystick.h"
#include "options.h"
#include "tlvc.h"
#include "slip.h"

//---------------------------------------------------------------------------
// SOC service stuff... from the examples.
#define SOC_ALIGN 0x1000
#define SOC_BUFFERSIZE 0x100000

//---------------------------------------------------------------------------
static program_options_t programOptions;

static hid_device_t hidGamepad;
static hid_device_t hidAccel;
static hid_device_t hidGyro;
static hid_device_t hidTouchscreen;

//---------------------------------------------------------------------------
static bool init_config()
{
    // Read config file on device
    program_options_init(&programOptions);

    if (!program_options_load(&programOptions, "config.txt")) {
        printf("Please create a file named config.txt in the \n"
               "app's directory, with lines containing \n"
               "the server's IP and port, as below:\n\n"
               "server:192.168.1.1\n"
               "port:1234\n\n"
               "\n"
               "Press any key to exit...\n");

        // Fail loop to run if we couldn't read the config file.
        while (aptMainLoop()) {
            gspWaitForVBlank();
            hidScanInput();

            uint32_t keys = hidKeysDown();
            if (keys) {
                break;
            }

            gfxFlushBuffers();
            gfxSwapBuffers();
        }
        return false;
    }
    return true;
}

//---------------------------------------------------------------------------
static void init_hid_devices()
{
    hid_gamepad_init(&hidGamepad, &programOptions);

    if (programOptions.useAccel) {
        hid_accel_init(&hidAccel, &programOptions);
    }

    if (programOptions.useGyro) {
        hid_gyro_init(&hidGyro, &programOptions);
    }

    if (programOptions.useTouch) {
        hid_touch_init(&hidTouchscreen, &programOptions);
    }
}

//---------------------------------------------------------------------------
int main(void)
{
    // ToDo: 3ds init stuff...
    gfxInitDefault();

    consoleInit(GFX_TOP, NULL);

    // Read from the config file and ensure we have all required properties
    if (!init_config()) {
        gfxExit();
        return 0;
    }

    init_hid_devices();

    // allocate buffer for SOC service
    uint32_t* socBuffer = (uint32_t*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    socInit(socBuffer, SOC_BUFFERSIZE);

    // Enable the accel
    if (programOptions.useAccel || programOptions.useSteeringControls) {
        HIDUSER_EnableAccelerometer();
    }
    if (programOptions.useGyro) {
        HIDUSER_EnableGyroscope();
    }

    while (aptMainLoop()) {
        gspWaitForVBlank();

        hidScanInput();

        bool rc = true;

        rc = handle_hid_events(&hidGamepad, &programOptions);
        if (rc && programOptions.useTouch) {
            rc = handle_hid_events(&hidTouchscreen, &programOptions);
        }
        if (rc && programOptions.useAccel) {
            rc = handle_hid_events(&hidAccel, &programOptions);
        }
        if (rc && programOptions.useGyro) { }
        rc = handle_hid_events(&hidGyro, &programOptions);

        if (rc) {
            // Poll input multiple times per vblank in order to reduce latency
            // Don't think the touchscreen/accel latency is as big a concern...
            for (int i = 0; i < 2; i++) {
                svcSleepThread(1000000000ULL / 180ULL);
                hidScanInput();
                rc = handle_hid_events(&hidGamepad, &programOptions);
                if (!rc) {
                    break;
                }
            }
        }

        // If we encountered any socket errors, wait a couple seconds before continuing.
        if (!rc) {
            svcSleepThread(2000000000ULL);
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
    }

    if (programOptions.useGyro) {
        HIDUSER_DisableGyroscope();
    }
    if (programOptions.useAccel || programOptions.useSteeringControls) {
        HIDUSER_DisableAccelerometer();
    }

    socExit();
    gfxExit();

    return 0;
}
