// Netstick-3ds - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.

#include "hid_device.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "net_util.h"

//---------------------------------------------------------------------------
bool hid_device_init(hid_device_t* device_, const char* name_, hid_config_handler_t configHandler_, hid_event_handler_t eventHandler_, const program_options_t* options_) {
    memset(device_, 0, sizeof(*device_));
    device_->name = name_;
    device_->configHandlerFn = configHandler_;
    device_->eventHandlerFn = eventHandler_;
    device_->sockFd = -1;

    if (device_->configHandlerFn(device_, options_)) {
        device_->isInit = true;
        return true;
    }

    return false;
}

//---------------------------------------------------------------------------
bool handle_hid_events(hid_device_t* device_, const program_options_t* options_)
{
    if (!device_->isInit) {
        return false;
    }

    if (device_->sockFd == -1) {
        device_->sockFd = net_util_connect(options_->host, options_->port);

        // Connection succeeded -- try to send configuration data.
        if (device_->sockFd >= 0) {
            if (!net_util_encode_and_transmit(device_->sockFd, 0, &device_->config, sizeof(js_config_t))) {
                close(device_->sockFd);
                device_->sockFd = -1;
                return false;
            } else {
                printf("connected -- %s!\n", device_->name);
            }
        } else {
            return false;
        }
    } else {
        if (!device_->eventHandlerFn(device_, options_)) {
            close(device_->sockFd);
            device_->sockFd = -1;
            printf("disconnected -- %s!\n", device_->name);
            return false;
        }
    }
    return true;
}
