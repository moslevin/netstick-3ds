// Netstick-3ds - Copyright (c) 2021 Funkenstein Software Consulting.  See LICENSE.txt
// for more details.

#pragma once

//---------------------------------------------------------------------------
// Vendor/product IDs to register this device as on the server-side
#define NINTENDO_USB_VID ((uint16_t)(0x057E))
#define NINTENDO_USB_PID ((uint16_t)(0x1337)) // Fake product ID, since the 3ds isn't BT/USB compatbile

#define NINTENDO_3DS_NAME "Nintendo 3DS"

//---------------------------------------------------------------------------
// Linux gamepad button IDs -- which is what netstickd is expecting on the
// other side of the connection; we'll map the 3DS button IDs to the linux
// IDs using a joystick map object

#define LINUX_ABS_X 0x00
#define LINUX_ABS_Y 0x01
#define LINUX_ABS_Z 0x02
#define LINUX_ABS_RX 0x03
#define LINUX_ABS_RY 0x04
#define LINUX_ABS_RZ 0x05
#define LINUX_ABS_WHEEL 0x08

#define LINUX_BTN_GAMEPAD 0x130
#define LINUX_BTN_SOUTH 0x130
#define LINUX_BTN_A BTN_SOUTH
#define LINUX_BTN_EAST 0x131
#define LINUX_BTN_B BTN_EAST
#define LINUX_BTN_C 0x132
#define LINUX_BTN_NORTH 0x133
#define LINUX_BTN_X BTN_NORTH
#define LINUX_BTN_WEST 0x134
#define LINUX_BTN_Y BTN_WEST
#define LINUX_BTN_Z 0x135
#define LINUX_BTN_TL 0x136
#define LINUX_BTN_TR 0x137
#define LINUX_BTN_TL2 0x138
#define LINUX_BTN_TR2 0x139
#define LINUX_BTN_SELECT 0x13a
#define LINUX_BTN_START 0x13b
#define LINUX_BTN_MODE 0x13c
#define LINUX_BTN_THUMBL 0x13d
#define LINUX_BTN_THUMBR 0x13e
#define LINUX_BTN_TOUCH 0x14a

#define LINUX_BTN_DPAD_UP 0x220
#define LINUX_BTN_DPAD_DOWN 0x221
#define LINUX_BTN_DPAD_LEFT 0x222
#define LINUX_BTN_DPAD_RIGHT 0x223

#define LINUX_KEY_UP 103
#define LINUX_KEY_LEFT 105
#define LINUX_KEY_RIGHT 106
#define LINUX_KEY_DOWN 108

#define EV_KEY ((size_t)(1))
#define EV_REL ((size_t)(2))
#define EV_ABS ((size_t)(3))

//---------------------------------------------------------------------------
// 3DS key bits in the HID report structure
#define NDS_KEY_A 0
#define NDS_KEY_B 1
#define NDS_KEY_SELECT 2
#define NDS_KEY_START 3
#define NDS_KEY_DPAD_RIGHT 4
#define NDS_KEY_DPAD_LEFT 5
#define NDS_KEY_DPAD_UP 6
#define NDS_KEY_DPAD_DOWN 7
#define NDS_KEY_R 8
#define NDS_KEY_L 9
#define NDS_KEY_X 10
#define NDS_KEY_Y 11
#define NDS_KEY_ZL 14
#define NDS_KEY_ZR 15
#define NDS_KEY_TOUCH 20

//---------------------------------------------------------------------------
// Max/Min values for circle pad reports
#define NDS_CIRCLE_PAD_MIN -156
#define NDS_CIRCLE_PAD_MAX 156

//---------------------------------------------------------------------------
// Max/Min values for accelerometer reports
#define NDS_ACCEL_MIN -512
#define NDS_ACCEL_MAX 512
#define NDS_ACCEL_FUZZ 16

//---------------------------------------------------------------------------
// Max/Min values for gyro reports
#define NDS_GYRO_MIN -12000
#define NDS_GYRO_MAX 12000
#define NDS_GYRO_FUZZ 5
