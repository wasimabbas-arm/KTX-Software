#! /usr/bin/env bash
# Copyright 2015-2020 The Khronos Group Inc.
# SPDX-License-Identifier: Apache-2.0

# Install software in CI environment necessary to build on Linux.

# Exit if any command fails.
set -e

# Set parameters from command-line arguments, if any.
for i in $@; do
  eval $i
done

ARCH=${ARCH:-$(uname -m)}  # Build for local machine by default.
FEATURE_LOADTESTS=${FEATURE_LOADTESTS:-ON}
VULKAN_SDK_VER=${VULKAN_SDK_VER:-1.3.243}

sudo apt-get -qq update
sudo apt-get -qq install ninja-build:native
sudo apt-get -qq install doxygen:native
sudo apt-get -qq install rpm:native

# TODO: Figure out how to install cross versions of dependent libraries.
# Need `dpkg` Multiarch. See https://wiki.debian.org/Multiarch/HOWTO
# Packages can be specified as 'package:architecture' pretty-much
# anywhere. Can use :native to request a package for the build machine
# architecture.
if [ "$ARCH" = "$(uname -m)" ]; then
  dpkg_arch=native
  # gcc, g++ and binutils for native builds should already be installed
  # on CI platforms together with cmake.
  # sudo apt-get -qq install gcc g++ binutils make
else
  # Adjust for dpkg/apt architecture naming. How irritating that
  # it differs from what uname -m reports.
  if [ "$ARCH" = "x86_64" ]; then
    dpkg_arch=amd64
    gcc_pkg_arch=x86-64
  elif [ "$ARCH" = "aarch64" ]; then
    dpkg_arch=arm64
    gcc_pkg_arch=$ARCH
  fi
  sudo dpkg --add-architecture $dpkg_arch
  sudo apt-get update
  # Don't think this is right to install cross-compiler
  #sudo apt-get -qq install gcc:$dpkg_arch g++:$dpkg_arch binutils:$dpkg_arch
  # or this where `arch` is x86-64 or arm64.
  sudo apt-get -qq install gcc-$gcc_pkg_arch-linux-gnu:native g++-$gcc_pkg_arch-linux-gnu:native binutils-$gcc_pkg_arch-linux-gnu:native
fi
sudo apt-get -qq install opencl-c-headers:$dpkg_arch
sudo apt-get -qq install mesa-opencl-icd:$dpkg_arch

if [ "$FEATURE_LOADTESTS" = "ON" ]; then
  sudo apt-get -qq install libsdl2-dev:$dpkg_arch
  sudo apt-get -qq install libgl1-mesa-glx:$dpkg_arch libgl1-mesa-dev:$dpkg_arch
  sudo apt-get -qq install libvulkan1 libvulkan-dev:$dpkg_arch
  sudo apt-get -qq install libassimp5 libassimp-dev:$dpkg_arch

  os_codename=$(grep -E 'VERSION_CODENAME=[a-zA-Z]+$' /etc/os-release)
  os_codename=${os_codename#VERSION_CODENAME=}

  # TODO: Figure out what to do about this.
  echo "Download Vulkan SDK"
  wget --no-verbose -O - https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo apt-key add -
  sudo wget -O /etc/apt/sources.list.d/lunarg-vulkan-$VULKAN_SDK_VER-$os_codename.list https://packages.lunarg.com/vulkan/$VULKAN_SDK_VER/lunarg-vulkan-$VULKAN_SDK_VER-$os_codename.list
  echo "Install Vulkan SDK"
  sudo apt update
  sudo apt install vulkan-sdk
fi

git lfs pull --include=tests/srcimages,tests/testimages

# vim:ai:ts=4:sts=2:sw=2:expandtab
