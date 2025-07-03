@echo off
if not exist build (
    mkdir build
)

cd build
cmake -G "Visual Studio 17 2022" ..