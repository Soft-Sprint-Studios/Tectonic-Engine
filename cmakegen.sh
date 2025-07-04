#!/bin/bash

set -e

echo "Installing dependencies..."
sudo apt update
sudo apt install -y \
    build-essential cmake \
    libsdl2-dev \
    libglew-dev \
    libfreetype6-dev \
    libpthread-stubs0-dev \
    libdl-dev \
    libgl1-mesa-dev

if [ ! -d "build" ]; then
    mkdir build
fi

cd build

echo "Running CMake..."
cmake ..

echo "Building..."
make -j$(nproc)

echo "Build complete."
