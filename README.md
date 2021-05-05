# Netstick for 3DS

Copyright (c) 2021 - Funkenstein Software Consulting, all rights reserved.  See license.txt for more information.

## What is it?

Netstick turns your 3DS into a wifi enabled gamepad!  Control any linux-based device (such as a Raspberry Pi running Retropie) using your 3DS!

## What's supported?

- The dpad, circle-pad, c-stick (new 3ds), and all buttons are all mapped
- Accelerometer (X/Y/Z)
- Gyroscope (X/Y/Z)
- Touchscreen

## Preparation

#### On the Linux host...

Build `netstickd` for your supported linux device and run it (see instructions at https://github.com/moslevin/netstick). Take note of the device's IP address and port.

#### On the 3DS...

Prerequisite:  Netstick requires Homebrew Launcher to be installed on your 3DS; for more information, refer to https://3ds.hacks.guide/.

On your host PC, edit the included `config.txt`, and set the server and port lines to the IP address/port of the Linux device running `netstickd`.

Create a folder named `/3ds/netstick-3ds` on your 3DS, and copy `netstick-3ds.3dsx`, `netstick-3ds.smdh`, and your modified `config.txt` into it.

## Running

Launch Netstick from the Homebrew Launcher, similar to any other homebrew app.  If all goes well, you should see the 3DS indicate success connecting to the Linux host,
and events routed to a device named "Nintendo 3DS" appear under /dev/input.  Use `evtest` on Linux to verify that the events are processed successfully.

## Building

Netstick for 3DS is built using devkitpro (https://devkitpro.org).  Once it has been properly installed and configured, Netstick can be built by typing `make`
from the root of this source package.

## Configuration

The configuration file format is very simple, and consists of lines in key:value format.

The supported options are described below:

`server` - The IP address of the server on the local network
`port` - The port of the server on the local network
`invert_cstick_x` - Invert the values reported natively by the 3DS on the C-stick's X axis (Ignored on Old 3DS)
`invert_cstick_y` - Invert the values reported natively by the 3DS on the C-stick's Y axis (Ignored on Old 3DS)
`invert_circle_pad_x` - Invert the values reported natively by the 3DS on the Circle Pad's X axis
`invert_circle_pad_y` - Invert the values reported natively by the 3DS on the Circle Pad's Y axis
`use_touch` - Enable the touchscreen device when set to 'true'
`use_accel` - Enable the accelerometer when set to 'true'
`use_gyro` - Enable the gyroscope when set to 'true'
`send_touch_event` - send a linux "touch" event (in addition to the X/Y coordinates) when the touchscreen is active 
`touch_offset` - shrink the touchscreen's surface by a number of pixels on each side, making it easier to reach its limits
`swap_ab` - swap the a and b buttons in the joystick report (allows correct button mapping when using a 3DS as a controller for steam, etc.)
`swap_xy` - swap the x and y buttons in the joystick report (allows correct button mapping when using a 3DS as a controller for steam, etc.)

