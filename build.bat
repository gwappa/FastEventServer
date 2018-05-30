@echo off
cd /d %~dp0
reg Query "HKLM\Hardware\Description\System\CentralProcessor\0" | find /i "x86" > NUL && set _bits=32 || set _bits=64
cl /Wall /EHsc /FeFastEventServer_windows_%_bits%bit src\*.cpp Ws2_32.lib libks\libks.lib
exit /b 0

