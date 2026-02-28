@echo off
setlocal
set DRIVER_PATH=Driver.sys
set CERT_NAME=SpicyMouseInjectorTestCert

echo --- Spicy Driver Signing Tool ---

:: Check if signtool exists
where signtool >nul 2>nul
if %errorlevel% neq 0 (
    echo [!] signtool.exe is not in PATH. Please install Windows SDK/WDK.
    pause
    exit /b
)

echo [*] Creating self-signed certificate...
powershell -Command "New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=%CERT_NAME%' -CertStoreLocation 'Cert:\CurrentUser\My' -NotAfter (Get-Date).AddYears(10)" > cert_info.txt

echo [*] Exporting certificate...
:: This part is simplified, usually you'd need the thumbprint. 
:: For an automated script, we'll try to find the last created cert with our name.
for /f "tokens=*" %%i in ('powershell -Command "(Get-ChildItem Cert:\CurrentUser\My | where {$_.Subject -like '*%CERT_NAME%*'} | sort NotBefore -Descending | select -First 1).Thumbprint"') do set THUMBPRINT=%%i

if "%THUMBPRINT%"=="" (
    echo [!] Failed to create or find certificate.
    pause
    exit /b
)

echo [*] Using Certificate Thumbprint: %THUMBPRINT%

echo [*] Signing driver...
signtool sign /v /s My /n "%CERT_NAME%" /t http://timestamp.digicert.com "%DRIVER_PATH%"

if %errorlevel% equ 0 (
    echo [+] Driver signed successfully!
    echo [!] Note: You still need to enable TESTSIGNING: 'bcdedit /set testsigning on'
) else (
    echo [!] Signing failed.
)

del cert_info.txt
pause
