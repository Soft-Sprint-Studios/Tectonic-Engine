@echo off
if not exist buildx64 (
    mkdir buildx64
)

cd buildx64
cmake -G "Visual Studio 17 2022" ..