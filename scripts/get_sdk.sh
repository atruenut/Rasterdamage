#!/bin/sh
# Fetch the Steinberg VST3 SDK (MIT licensed) into third_party/vst3sdk
set -e
cd "$(dirname "$0")/.."
mkdir -p third_party
if [ -f third_party/vst3sdk/pluginterfaces/base/funknown.h ]; then
  echo "VST3 SDK already present."
  exit 0
fi
git clone --depth 1 --recursive --shallow-submodules https://github.com/steinbergmedia/vst3sdk.git third_party/vst3sdk
