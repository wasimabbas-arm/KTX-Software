#! /usr/bin/env bash
# Copyright 2015-2020 The Khronos Group Inc.
# SPDX-License-Identifier: Apache-2.0

# Build on Linux.

# Exit if any command fails.
set -e

# Set parameters from command-line arguments, if any.
for i in $@; do
  eval $i
done

# Set some defaults
ARCH=${ARCH:-$(uname -m)}
CMAKE_GEN=${CMAKE_GEN:-Ninja Multi-Config}
CONFIGURATION=${CONFIGURATION:-Release}
FEATURE_DOC=${FEATURE_DOC:-OFF}
FEATURE_JNI=${FEATURE_JNI:-OFF}
FEATURE_LOADTESTS=${FEATURE_LOADTESTS:-ON}
FEATURE_TESTS=${FEATURE_TESTS:-ON}
FEATURE_TOOLS=${FEATURE_TOOLS:-ON}
FEATURE_VULKAN=${FEATURE_VULKAN:-ON}
PACKAGE=${PACKAGE:-NO}
SUPPORT_SSE=${SUPPORT_SSE:-ON}
SUPPORT_OPENCL=${SUPPORT_OPENCL:-OFF}

BUILD_DIR=${BUILD_DIR:-build/linux-$CONFIGURATION}

mkdir -p $BUILD_DIR

cmake_args=("-G" "$CMAKE_GEN" \
  "-B" $BUILD_DIR \
  "-D" "CMAKE_BUILD_TYPE=$CONFIGURATION" \
  "-D" "KTX_FEATURE_DOC=$FEATURE_DOC" \
  "-D" "KTX_FEATURE_JNI=$FEATURE_JNI" \
  "-D" "KTX_FEATURE_LOADTEST_APPS=$FEATURE_LOADTESTS" \
  "-D" "KTX_FEATURE_TESTS=$FEATURE_TESTS" \
  "-D" "KTX_FEATURE_TOOLS=$FEATURE_TOOLS" \
  "-D" "KTX_FEATURE_VULKAN=$FEATURE_VULKAN" \
  "-D" "BASISU_SUPPORT_OPENCL=$SUPPORT_OPENCL" \
  "-D" "BASISU_SUPPORT_SSE=$SUPPORT_SSE"
)
config_display="Configure KTX-Software (Linux): "
for arg in "${cmake_args[@]}"; do
  case $arg in
    "-A") config_display+="Arch=" ;;
    "-G") config_display+="Generator=" ;;
    "-B") config_display+="Build Dir=" ;;
    "-D") ;;
    *) config_display+="$arg, " ;;
  esac
done

echo ${config_display%??}
cmake . "${cmake_args[@]}"

pushd $BUILD_DIR

oldifs=$IFS
#; is necessary because `for` is a Special Builtin.
IFS=, ; for config in $CONFIGURATION
do
  IFS=$oldifs # Because of ; IFS set above will still be present.
  # Build and test
  echo "Build KTX-Software (Linux $ARCH $config)"
  cmake --build . --config $config
  if [ "$ARCH" = "$(uname -m)" ]; then
    echo "Test KTX-Software (Linux $ARCH $config)"
    ctest -C $config #--verbose
  fi
  if [ "$config" = "Release" -a "$PACKAGE" = "YES" ]; then
    echo "Pack KTX-Software (Linux $ARCH $config)"
    if ! cpack -C $config -G DEB; then
      # The DEB generator does not seem to write any log files.
      #cat _CPack_Packages/Linux/DEB/DEBOutput.log
      exit 1
    fi
    if ! cpack -C $config -G RPM; then
      echo "RPM generator .err file"
      cat _CPack_Packages/Linux/RPM/rpmbuildktx-software.err
      echo "RPM generator .out file"
      cat _CPack_Packages/Linux/RPM/rpmbuildktx-software.out
      exit 1
    fi
    if ! cpack -C $config -G TBZ2; then
      # The TBZ2 generator does not seem to write any log files.
      # cat _CPack_Packages/Linux/TBZ2/TBZ2Output.log
      exit 1
    fi
  fi
done

#echo "***** toktx version.h *****"
#cat tools/toktx/version.h
#echo "****** toktx version ******"
#build/linux-release/tools/toktx/toktx --version
#echo "***************************"

popd

# vim:ai:ts=4:sts=2:sw=2:expandtab

