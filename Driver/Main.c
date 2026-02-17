#include "..\Common\Ioctl.h"
#include <ntddk.h>
#include <ntstrsafe.h>

// Missing declarations for undocumented kernel functions
NTSYSAPI
NTSTATUS
NTAPI
ObReferenceObjectByName(__in PUNICODE_STRING ObjectName, __in ULONG Attributes,
                        __in_opt PACCESS_STATE AccessState,
                        __in_opt ACCESS_MASK DesiredAccess,
                        __in POBJECT_TYPE ObjectType,
                        __in KPROCESSOR_MODE AccessMode,
                        __in_opt PVOID ParseContext, __out PVOID *Object);

extern POBJECT_TYPE *IoDriverObjectType;

//
// Typedefs for internal mouse class structures
//

// MOUSE_INPUT_DATA is defined in ntddmou.h, we use the standard definition
// but we need to include kcom.h or similar if not present.
// However, since it's a standard WDK type, we'll just use it.
#include <ntddmou.h>

typedef VOID (*PMouseClassServiceCallback)(PDEVICE_OBJECT DeviceObject,
                                           PMOUSE_INPUT_DATA InputDataStart,
                                           PMOUSE_INPUT_DATA InputDataEnd,
                                           PULONG InputDataConsumed);

typedef struct _MOUSE_OBJECT {
  PDEVICE_OBJECT MouseDeviceObject;
  PMouseClassServiceCallback ServiceCallback;
  BOOLEAN Found;
} MOUSE_OBJECT, *PMOUSE_OBJECT;

// Simplified structure for the mouse class extension
typedef struct _MOUSE_CLASS_EXTENSION {
  PVOID Reserved;
  PDEVICE_OBJECT UpperDeviceObject;
  PMouseClassServiceCallback ServiceCallback;
} MOUSE_CLASS_EXTENSION, *PMOUSE_CLASS_EXTENSION;

// Global mouse object
MOUSE_OBJECT g_MouseObject = {0};

// Clicker State
typedef struct _CLICKER_STATE {
  KTIMER Timer;
  KDPC Dpc;
  BOOLEAN Active;
  BOOLEAN ButtonDown;
  AUTOCLICK_CONFIG Config;
  ULONG Seed;
} CLICKER_STATE, *PCLICKER_STATE;

CLICKER_STATE g_ClickerState = {0};

// Forward declarations
VOID InjectMousePacket(USHORT buttonFlags);
NTSTATUS FindMouseObject();

// Simple LCG random number generator for kernel mode
ULONG RtlRandomRange(PULONG Seed, ULONG Min, ULONG Max) {
  if (Max <= Min)
    return Min;
  *Seed = *Seed * 1103515245 + 12345;
  return Min + ((*Seed / 65536) % (Max - Min + 1));
}

// DPC Routine for clicking
VOID ClickTimerDpc(PKDPC Dpc, PVOID DeferredContext, PVOID SystemArgument1,
                   PVOID SystemArgument2) {
  UNREFERENCED_PARAMETER(Dpc);
  UNREFERENCED_PARAMETER(DeferredContext);
  UNREFERENCED_PARAMETER(SystemArgument1);
  UNREFERENCED_PARAMETER(SystemArgument2);

  if (!g_ClickerState.Active)
    return;

  if (!g_ClickerState.ButtonDown) {
    // Send DOWN
    InjectMousePacket(0x01); // MOUSE_LEFT_BUTTON_DOWN
    g_ClickerState.ButtonDown = TRUE;

    // Re-set timer for the "Hold" duration
    LARGE_INTEGER dueTime;
    dueTime.QuadPart = -(LONGLONG)RtlRandomRange(&g_ClickerState.Seed, 10, 20) *
                       10000; // 10-20ms hold
    KeSetTimer(&g_ClickerState.Timer, dueTime, &g_ClickerState.Dpc);
  } else {
    // Send UP
    InjectMousePacket(0x02); // MOUSE_LEFT_BUTTON_UP
    g_ClickerState.ButtonDown = FALSE;

    // Re-set timer for the next click interval
    LARGE_INTEGER dueTime;
    ULONG delay =
        RtlRandomRange(&g_ClickerState.Seed, g_ClickerState.Config.MinDelay,
                       g_ClickerState.Config.MaxDelay);
    dueTime.QuadPart = -(LONGLONG)delay * 10000;
    KeSetTimer(&g_ClickerState.Timer, dueTime, &g_ClickerState.Dpc);
  }
}

// Function prototypes
DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD DriverUnload;
NTSTATUS DriverCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DriverDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);

// Helper to find the mouse object by scanning \Driver\Mouclass
NTSTATUS FindMouseObject() {
  UNICODE_STRING driverName;
  PDRIVER_OBJECT mouseClassDriver = NULL;
  NTSTATUS status;

  RtlInitUnicodeString(&driverName, L"\\Driver\\Mouclass");
  status = ObReferenceObjectByName(&driverName, OBJ_CASE_INSENSITIVE, NULL, 0,
                                   *IoDriverObjectType, KernelMode, NULL,
                                   (PVOID *)&mouseClassDriver);

  if (NT_SUCCESS(status)) {
    PDEVICE_OBJECT currentDevice = mouseClassDriver->DeviceObject;

    while (currentDevice) {
      PMOUSE_CLASS_EXTENSION extension =
          (PMOUSE_CLASS_EXTENSION)currentDevice->DeviceExtension;

      // Look for the callback in the extension
      if (extension && extension->ServiceCallback) {
        g_MouseObject.MouseDeviceObject = currentDevice;
        g_MouseObject.ServiceCallback = extension->ServiceCallback;
        g_MouseObject.Found = TRUE;
        break;
      }
      currentDevice = currentDevice->NextDevice;
    }
    ObDereferenceObject(mouseClassDriver);
  }

  return status;
}

VOID InjectMousePacket(USHORT buttonFlags) {
  if (!g_MouseObject.Found)
    return;

  MOUSE_INPUT_DATA inputData = {0};
  inputData.ButtonFlags = buttonFlags;
  ULONG consumed = 0;

  // This is the core 'Gaming Mouse' hack: calling the callback directly
  if (g_MouseObject.ServiceCallback) {
    g_MouseObject.ServiceCallback(g_MouseObject.MouseDeviceObject, &inputData,
                                  &inputData + 1, &consumed);
  }
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject,
                     PUNICODE_STRING RegistryPath) {
  UNREFERENCED_PARAMETER(RegistryPath);

  NTSTATUS status;
  UNICODE_STRING deviceName, symLink;

  RtlInitUnicodeString(&deviceName, L"\\Device\\SpicyMouseInjector");
  RtlInitUnicodeString(&symLink, L"\\DosDevices\\SpicyMouseInjector");

  PDEVICE_OBJECT deviceObject = NULL;
  status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN,
                          FILE_DEVICE_SECURE_OPEN, FALSE, &deviceObject);

  if (!NT_SUCCESS(status))
    return status;

  DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverCreateClose;
  DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;
  DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDeviceControl;
  DriverObject->DriverUnload = DriverUnload;

  // Initialize Clicker State
  KeInitializeTimer(&g_ClickerState.Timer);
  KeInitializeDpc(&g_ClickerState.Dpc, ClickTimerDpc, NULL);
  g_ClickerState.Active = FALSE;
  g_ClickerState.Config.MinDelay = 15;
  g_ClickerState.Config.MaxDelay = 25;
  g_ClickerState.Seed = 1337;

  status = IoCreateSymbolicLink(&symLink, &deviceName);

  // Attempt to find mouse class to prepare for injection
  // FindMouseObject(); // Commented out for initial build test reliability
  FindMouseObject();

  return status;
}

VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
  g_ClickerState.Active = FALSE;
  KeCancelTimer(&g_ClickerState.Timer);

  UNICODE_STRING symLink;
  RtlInitUnicodeString(&symLink, L"\\DosDevices\\SpicyMouseInjector");
  IoDeleteSymbolicLink(&symLink);
  IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS DriverCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
  UNREFERENCED_PARAMETER(DeviceObject);
  Irp->IoStatus.Status = STATUS_SUCCESS;
  Irp->IoStatus.Information = 0;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return STATUS_SUCCESS;
}

NTSTATUS DriverDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
  UNREFERENCED_PARAMETER(DeviceObject);
  PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);
  NTSTATUS status = STATUS_SUCCESS;
  ULONG info = 0;

  switch (IrpSp->Parameters.DeviceIoControl.IoControlCode) {
  case IOCTL_MOUSE_START_CLICK: {
    g_ClickerState.Active = TRUE;
    g_ClickerState.ButtonDown = FALSE;
    LARGE_INTEGER dueTime;
    dueTime.QuadPart = -10000; // Start almost immediately
    KeSetTimer(&g_ClickerState.Timer, dueTime, &g_ClickerState.Dpc);
    break;
  }
  case IOCTL_MOUSE_STOP_CLICK: {
    g_ClickerState.Active = FALSE;
    KeCancelTimer(&g_ClickerState.Timer);
    break;
  }
  case IOCTL_MOUSE_SET_CONFIG: {
    if (IrpSp->Parameters.DeviceIoControl.InputBufferLength <
        sizeof(AUTOCLICK_CONFIG)) {
      status = STATUS_BUFFER_TOO_SMALL;
    } else {
      PAUTOCLICK_CONFIG config =
          (PAUTOCLICK_CONFIG)Irp->AssociatedIrp.SystemBuffer;
      g_ClickerState.Config = *config;
      info = sizeof(AUTOCLICK_CONFIG);
    }
    break;
  }
  case IOCTL_MOUSE_INJECT_CLICK: {
    if (IrpSp->Parameters.DeviceIoControl.InputBufferLength <
        sizeof(MOUSE_CLICK_DATA)) {
      status = STATUS_BUFFER_TOO_SMALL;
    } else {
      PMOUSE_CLICK_DATA data =
          (PMOUSE_CLICK_DATA)Irp->AssociatedIrp.SystemBuffer;
      InjectMousePacket(data->ButtonFlags);
      info = sizeof(MOUSE_CLICK_DATA);
    }
    break;
  }
  default:
    status = STATUS_INVALID_DEVICE_REQUEST;
    break;
  }

  Irp->IoStatus.Status = status;
  Irp->IoStatus.Information = info;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return status;
}
