// Netstick-3ds - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.

#include "options.h"

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
//---------------------------------------------------------------------------
typedef enum {
    PROGRAM_OPTION_HOST,
    PROGRAM_OPTION_PORT,
    PROGRAM_OPTION_SWAP_AB,
    PROGRAM_OPTION_SWAP_XY,
    PROGRAM_OPTION_INVERT_CSTICK_X,
    PROGRAM_OPTION_INVERT_CSTICK_Y,
    PROGRAM_OPTION_INVERT_CIRCLE_PAD_X,
    PROGRAM_OPTION_INVERT_CIRCLE_PAD_Y,
    PROGRAM_OPTION_USE_TOUCH,
    PROGRAM_OPTION_SEND_TOUCH_DOWN_EVENT,
    PROGRAM_OPTION_USE_ACCEL,
    PROGRAM_OPTION_USE_GYRO,
    PROGRAM_OPTION_TOUCH_OFFSET,
//--
    PROGRAM_OPTION_COUNT
} program_option_t;

//---------------------------------------------------------------------------
typedef bool (*option_handler_fn_t)(const char* value_, void* option_, bool* optionSet_);

//---------------------------------------------------------------------------
typedef struct {
    const char* optionName;
    option_handler_fn_t handler;
    void* optionVal;
    bool* optionSet;
} option_handler_map_t;

//---------------------------------------------------------------------------
static bool opt_handler_bool(const char* value_, void* option_, bool* optionSet_) {
    bool* boolOption = option_;

    if (0 == strcmp("true", value_)) {
        *boolOption = true;
        if (optionSet_) {
            *optionSet_ = true;
        }
        return true;
    } else if (0 == strcmp("false", value_)) {
        *boolOption = false;
        if (optionSet_) {
            *optionSet_ = true;
        }
        return true;
    }
    return false;
}

//---------------------------------------------------------------------------
static bool opt_handler_int(const char* value_, void* option_, bool* optionSet_) {
    int* intOption = option_;

    *intOption = atoi(value_);
    if (optionSet_) {
        *optionSet_ = true;
    }
    return true;
}

//---------------------------------------------------------------------------
static bool opt_handler_string(const char* value_, void* option_, bool* optionSet_) {
    char* option = option_;
    strcpy(option, value_);

    if (optionSet_) {
        *optionSet_ = true;
    }
    return true;
}

//---------------------------------------------------------------------------
void program_options_init(program_options_t* options_) {
    memset(options_, 0, sizeof(*options_));
}

//---------------------------------------------------------------------------
static void program_option_parse_config_line(const char* line_, option_handler_map_t* handlerMap_)
{
    char key[64]   = {};
    char value[64] = {};

    char* matchIdx = strstr(line_, ":");

    if (!matchIdx) {
        return;
    }

    *matchIdx = ' ';

    sscanf(line_, "%64s %64s", key, value);
    for (int i = 0; i < PROGRAM_OPTION_COUNT; i++) {
        if (0 == strcmp(handlerMap_[i].optionName, key)) {
            handlerMap_[i].handler(value, handlerMap_[i].optionVal, handlerMap_[i].optionSet);
            return;
        }
    }
}

//---------------------------------------------------------------------------
bool program_options_load(program_options_t* options_, const char* filePath_) {
    option_handler_map_t handlerMap[PROGRAM_OPTION_COUNT] = {
        [PROGRAM_OPTION_HOST] = {"server", opt_handler_string, &options_->host, &options_->hostFound},
        [PROGRAM_OPTION_PORT] = {"port", opt_handler_int, &options_->port, &options_->portFound},
        [PROGRAM_OPTION_SWAP_AB] = {"swap_ab", opt_handler_bool, &options_->swapAB, NULL},
        [PROGRAM_OPTION_SWAP_XY] = {"swap_xy", opt_handler_bool, &options_->swapXY, NULL},
        [PROGRAM_OPTION_INVERT_CSTICK_X] = {"invert_cstick_x", opt_handler_bool, &options_->invertCStickX, NULL},
        [PROGRAM_OPTION_INVERT_CSTICK_Y] = {"invert_cstick_y", opt_handler_bool, &options_->invertCStickY, NULL},
        [PROGRAM_OPTION_INVERT_CIRCLE_PAD_X] = {"invert_circle_pad_x", opt_handler_bool, &options_->invertCirclePadX, NULL},
        [PROGRAM_OPTION_INVERT_CIRCLE_PAD_Y] = {"invert_circle_pad_y", opt_handler_bool, &options_->invertCirclePadY, NULL},
        [PROGRAM_OPTION_USE_TOUCH] = {"use_touch", opt_handler_bool, &options_->useTouch, NULL},
        [PROGRAM_OPTION_SEND_TOUCH_DOWN_EVENT] = {"send_touch_event", opt_handler_bool, &options_->sendTouchDownEvent, NULL},
        [PROGRAM_OPTION_USE_ACCEL] = {"use_accel", opt_handler_bool, &options_->useAccel, NULL},
        [PROGRAM_OPTION_USE_GYRO] = {"use_gyro", opt_handler_bool, &options_->useGyro, NULL},
        [PROGRAM_OPTION_TOUCH_OFFSET] = {"touch_offset", opt_handler_int, &options_->touchOffset, NULL},
    };

    // Open file and read contents into a buffer...
    FILE* configFile = fopen(filePath_, "r");

    if (configFile == NULL) {
        printf("Error opening %s\n", filePath_);
        return false;
    }

    fseek(configFile, 0, SEEK_END);

    int fileSize = ftell(configFile);

    fseek(configFile, 0, SEEK_SET);

    char* fileBuffer = (char*)malloc(fileSize);
    if (!fileBuffer) {
        printf("Error allocating %d bytes\n", fileSize);
        fclose(configFile);
        return false;
    }

    int nRead = fread(fileBuffer, 1, fileSize, configFile);
    fclose(configFile);

    if (nRead != fileSize) {
        printf("Could not read entire file (got %d, expected %d)\n", nRead, fileSize);
        free(fileBuffer);
        return false;
    }

    // Destructively parse through the read configuration file and read-out key/value pairs.
    char* lineStart = fileBuffer;
    char* curr      = lineStart;
    while (*curr) {
        if (*curr == '\n') {
            *curr = '\0';
            program_option_parse_config_line(lineStart, handlerMap);
            lineStart = ++curr;
        } else if (*curr == '#') {
            *curr = '\0';
            program_option_parse_config_line(lineStart, handlerMap);

            // Silently consume the rest of the line
            curr++;
            while (*curr && (*curr != '\n')) { curr++; }
            lineStart = curr;
        } else {
            curr++;
        }
    }
    program_option_parse_config_line(lineStart, handlerMap);
    free(fileBuffer);

    if (!options_->hostFound || !options_->portFound) {
        printf("Missing hostname/port\n");
        return false;
    }

    return true;
}

