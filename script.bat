@echo off
REM filepath: d:\OPSWAT\Workspace\ARC\ARC\CLIExtractionSchema\lvm.bat
SET vslvmmountPath="%~dp0\vslvmmount.exe"
SET lvmFilePath=%1
SET outputPath=%2
SET UNIQUE_ID=lvm_mount_%RANDOM%
SET mountPoint=%TEMP%\%UNIQUE_ID%

REM Create mount point directory
mkdir "%mountPoint%"
echo Mounting LVM volume from %lvmFilePath% to %mountPoint%

REM Mount the LVM volume in background using a unique window title
start "%UNIQUE_ID%" %vslvmmountPath% "%lvmFilePath%" "%mountPoint%"

REM Wait for mount to be ready (check if mount point is accessible)
timeout /t 2 /nobreak >nul

REM Retrieve PID by unique window title
FOR /F %%i IN ('powershell -NoProfile -Command "Get-Process | Where-Object { $_.MainWindowTitle -eq '%UNIQUE_ID%' } | Select-Object -ExpandProperty Id -First 1"') DO SET MOUNT_PID=%%i
echo Mounted with PID: %MOUNT_PID%

echo Copying files from %mountPoint% to %outputPath%
REM Copy all content from mount point to output path
xcopy "%mountPoint%\*" "%outputPath%\" /E /I /H /Y /C

REM Unmount by killing vslvmmount using its PID
taskkill /F /PID %MOUNT_PID%

REM Wait for process to fully terminate
timeout /t 1 /nobreak >nul

REM Clean up mount point
rmdir /S /Q "%mountPoint%"