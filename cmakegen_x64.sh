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

if [ ! -d "buildx64" ]; then
    mkdir buildx64
fi

cd buildx64

echo "Running CMake..."
cmake ..

echo "Building..."
make -j$(nproc)

echo "Build complete."