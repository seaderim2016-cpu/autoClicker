#include "..\Common\Ioctl.h"
#include <commctrl.h>
#include <string>
#include <windows.h>

#pragma comment(lib, "comctl32.lib")

// Global variables
HANDLE hDevice = INVALID_HANDLE_VALUE;
bool isClicking = false;
HWND hCpsInput, hLeftRadio, hRightRadio, hStatusLabel, hStartBtn;

// CPS to Delay conversion
void UpdateConfig() {
  if (hDevice == INVALID_HANDLE_VALUE)
    return;

  wchar_t buffer[10];
  GetWindowTextW(hCpsInput, buffer, 10);
  int cps = _wtoi(buffer);
  if (cps <= 0)
    cps = 10;
  if (cps > 100)
    cps = 100;

  ULONG delay = 1000 / cps; // Approximate delay in ms
  AUTOCLICK_CONFIG config;
  config.MinDelay = delay - (delay / 5);
  config.MaxDelay = delay + (delay / 5);
  config.ButtonType =
      (SendMessage(hLeftRadio, BM_GETCHECK, 0, 0) == BST_CHECKED)
          ? BUTTON_LEFT
          : BUTTON_RIGHT;

  DWORD bytesReturned;
  DeviceIoControl(hDevice, IOCTL_MOUSE_SET_CONFIG, &config, sizeof(config),
                  NULL, 0, &bytesReturned, NULL);
}

void ToggleClicking(HWND hWnd) {
  if (hDevice == INVALID_HANDLE_VALUE)
    return;

  isClicking = !isClicking;
  DWORD bytesReturned;

  if (isClicking) {
    UpdateConfig();
    DeviceIoControl(hDevice, IOCTL_MOUSE_START_CLICK, NULL, 0, NULL, 0,
                    &bytesReturned, NULL);
    SetWindowTextW(hStatusLabel, L"Status: CLICKING");
    SetWindowTextW(hStartBtn, L"STOP (F11)");
  } else {
    DeviceIoControl(hDevice, IOCTL_MOUSE_STOP_CLICK, NULL, 0, NULL, 0,
                    &bytesReturned, NULL);
    SetWindowTextW(hStatusLabel, L"Status: IDLE");
    SetWindowTextW(hStartBtn, L"START (F11)");
  }
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
  switch (uMsg) {
  case WM_CREATE: {
    CreateWindowW(L"STATIC", L"Speed (CPS):", WS_VISIBLE | WS_CHILD, 20, 20,
                  100, 20, hWnd, NULL, NULL, NULL);
    hCpsInput = CreateWindowW(L"EDIT", L"20",
                              WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER,
                              120, 20, 50, 20, hWnd, NULL, NULL, NULL);

    hLeftRadio =
        CreateWindowW(L"BUTTON", L"Left Button",
                      WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP, 20,
                      50, 120, 20, hWnd, (HMENU)101, NULL, NULL);
    hRightRadio = CreateWindowW(L"BUTTON", L"Right Button",
                                WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON, 150,
                                50, 120, 20, hWnd, (HMENU)102, NULL, NULL);
    SendMessage(hLeftRadio, BM_SETCHECK, BST_CHECKED, 0);

    hStartBtn = CreateWindowW(L"BUTTON", L"START (F11)", WS_VISIBLE | WS_CHILD,
                              20, 90, 250, 40, hWnd, (HMENU)201, NULL, NULL);
    hStatusLabel =
        CreateWindowW(L"STATIC", L"Status: IDLE", WS_VISIBLE | WS_CHILD, 20,
                      140, 250, 20, hWnd, NULL, NULL, NULL);

    // Register Hotkey
    RegisterHotKey(hWnd, 1, 0, VK_F11);
    break;
  }
  case WM_HOTKEY: {
    if (wParam == 1)
      ToggleClicking(hWnd);
    break;
  }
  case WM_COMMAND: {
    if (LOWORD(wParam) == 201)
      ToggleClicking(hWnd);
    break;
  }
  case WM_CLOSE: {
    if (isClicking)
      ToggleClicking(hWnd);
    DestroyWindow(hWnd);
    break;
  }
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
  hDevice =
      CreateFileW(L"\\\\.\\SpicyMouseInjector", GENERIC_WRITE | GENERIC_READ, 0,
                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if (hDevice == INVALID_HANDLE_VALUE) {
    MessageBoxW(NULL, L"Driver not found! Please load the driver first.",
                L"Error", MB_ICONERROR);
    return 1;
  }

  const wchar_t CLASS_NAME[] = L"SpicyClickerWindowClass";
  WNDCLASSW wc = {0};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = CLASS_NAME;
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);

  RegisterClassW(&wc);

  HWND hWnd = CreateWindowExW(
      0, CLASS_NAME, L"Spicy Mouse Injector v2",
      WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, CW_USEDEFAULT,
      CW_USEDEFAULT, 310, 210, NULL, NULL, hInstance, NULL);

  if (hWnd == NULL)
    return 0;

  ShowWindow(hWnd, nCmdShow);

  MSG msg = {0};
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  if (hDevice != INVALID_HANDLE_VALUE)
    CloseHandle(hDevice);
  return 0;
}
