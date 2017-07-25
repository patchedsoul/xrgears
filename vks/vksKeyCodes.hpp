/*
* Key codes for multiple platforms
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once


#include <linux/input.h>

#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
// todo: hack for bloom example
#define KEY_KPADD KEY_KPPLUS
#define KEY_KPSUB KEY_KPMINUS
#endif


#define KEY_ESCAPE 0x9
/**/
#define KEY_F1 0x43
#define KEY_F2 0x44
#define KEY_F3 0x45
#define KEY_F4 0x46
#define KEY_W 0x19
#define KEY_A 0x26
#define KEY_S 0x27
#define KEY_D 0x28
#define KEY_P 0x21
#define KEY_SPACE 0x41

#define KEY_KPADD 0x56
#define KEY_KPSUB 0x52
/*
#define KEY_B 0x38
#define KEY_F 0x29
#define KEY_L 0x2E
#define KEY_N 0x39
#define KEY_O 0x20
#define KEY_T 0x1C
*/


// todo: Android gamepad keycodes outside of define for now
#define GAMEPAD_BUTTON_A		0x1000
#define GAMEPAD_BUTTON_B		0x1001
#define GAMEPAD_BUTTON_X		0x1002
#define GAMEPAD_BUTTON_Y		0x1003
#define GAMEPAD_BUTTON_L1		0x1004
#define GAMEPAD_BUTTON_R1		0x1005
#define GAMEPAD_BUTTON_START	0x1006
#define TOUCH_DOUBLE_TAP		0x1100
