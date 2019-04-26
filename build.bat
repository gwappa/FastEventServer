@echo off
cd /d %~dp0
reg Query "HKLM\Hardware\Description\System\CentralProcessor\0" | find /i "x86" > NUL && set _bits=32 || set _bits=64
cl /c /O2 /EHsc /Iinclude /Ilibks\include src\lib\*.cpp
lib /OUT:libfe.lib *.obj
cl /O2 /EHsc /Iinclude /Ilibks\include /FeFastEventServer_windows_%_bits%bit src\main.cpp Ws2_32.lib libfe.lib libks\libks.lib
cl /O2 /EHsc /Iinclude /Ilibks\include /FeProfileDirect_windows_%_bits%bit src\profile_direct.cpp Ws2_32.lib libfe.lib libks\libks.lib
del *.obj
exit /b 0
