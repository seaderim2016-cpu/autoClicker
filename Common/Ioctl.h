#pragma once

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
#include <winioctl.h>
#endif

// Define the device type
#define MOUSE_INJECTOR_DEVICE_TYPE 0x8001

// Define the IOCTL codes
#define IOCTL_MOUSE_INJECT_CLICK                                               \
  CTL_CODE(MOUSE_INJECTOR_DEVICE_TYPE, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MOUSE_START_CLICK                                                \
  CTL_CODE(MOUSE_INJECTOR_DEVICE_TYPE, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MOUSE_STOP_CLICK                                                 \
  CTL_CODE(MOUSE_INJECTOR_DEVICE_TYPE, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MOUSE_SET_CONFIG                                                 \
  CTL_CODE(MOUSE_INJECTOR_DEVICE_TYPE, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Structure for mouse button state
typedef struct _MOUSE_CLICK_DATA {
  USHORT ButtonFlags; // e.g., MOUSE_LEFT_BUTTON_DOWN, MOUSE_LEFT_BUTTON_UP
  LONG LastX;
  LONG LastY;
} MOUSE_CLICK_DATA, *PMOUSE_CLICK_DATA;

// Button types
#define BUTTON_LEFT 1
#define BUTTON_RIGHT 2

// Configuration for the autoclicker
typedef struct _AUTOCLICK_CONFIG {
  ULONG MinDelay;
  ULONG MaxDelay;
  ULONG ButtonType; // BUTTON_LEFT or BUTTON_RIGHT
} AUTOCLICK_CONFIG, *PAUTOCLICK_CONFIG;
