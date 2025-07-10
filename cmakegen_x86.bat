@echo off
if not exist buildx64 (
    mkdir buildx86
)

cd buildx86
cmake -G "Visual Studio 17 2022" -A Win32 ..