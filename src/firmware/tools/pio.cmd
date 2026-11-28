@echo off
rem PlatformIO do ambiente virtual, no Windows, para usuarios com acento no nome (C:\Users\Joao com til, por exemplo).
rem O compilador da ESP32 nao abre caminhos com acento ("Arduino.h: No such file or directory"), entao a pasta de
rem pacotes do PlatformIO e apontada pelo nome curto (8.3) da pasta do usuario, que nao tem acento.
rem
rem Uso, dentro de src\firmware:   tools\pio.cmd run -e esp32_real_bt -t upload
setlocal
for %%I in ("%USERPROFILE%") do set "PLATFORMIO_CORE_DIR=%%~sI\.platformio"
"%~dp0..\.venv\Scripts\pio.exe" %*
exit /b %ERRORLEVEL%
