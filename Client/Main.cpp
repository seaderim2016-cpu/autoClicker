#include "..\Common\Ioctl.h"
#include <iostream>
#include <windows.h>

int main() {
  std::cout << "--- Kernel AutoClicker Client ---" << std::endl;

  HANDLE hDevice =
      CreateFileW(L"\\\\.\\SpicyMouseInjector", GENERIC_WRITE | GENERIC_READ, 0,
                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if (hDevice == INVALID_HANDLE_VALUE) {
    std::cerr << "Failed to open driver. Error: " << GetLastError()
              << std::endl;
    std::cout << "Make sure the driver is loaded!" << std::endl;
    return 1;
  }

  std::cout << "Driver connected. Press F11 to start clicking, F12 to exit."
            << std::endl;

  bool clicking = false;
  DWORD bytesReturned;

  while (!(GetAsyncKeyState(VK_F12) & 0x8000)) {
    if (GetAsyncKeyState(VK_F11) & 0x8000) {
      clicking = !clicking;

      if (clicking) {
        // Set config (e.g., 20ms - 40ms interval for high speed but random)
        AUTOCLICK_CONFIG config = {15, 30};
        DeviceIoControl(hDevice, IOCTL_MOUSE_SET_CONFIG, &config,
                        sizeof(config), NULL, 0, &bytesReturned, NULL);

        // Start kernel loop
        DeviceIoControl(hDevice, IOCTL_MOUSE_START_CLICK, NULL, 0, NULL, 0,
                        &bytesReturned, NULL);
        std::cout << "Clicking ON (Kernel Mode)" << std::endl;
      } else {
        // Stop kernel loop
        DeviceIoControl(hDevice, IOCTL_MOUSE_STOP_CLICK, NULL, 0, NULL, 0,
                        &bytesReturned, NULL);
        std::cout << "Clicking OFF" << std::endl;
      }
      Sleep(300); // Debounce
    }
    Sleep(10);
  }

  CloseHandle(hDevice);
  return 0;
}
