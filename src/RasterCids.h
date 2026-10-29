// Raster Damage - VST3 class ids and plugin identity.
#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

#define RD_PLUGIN_NAME "Raster Damage"
#define RD_VENDOR "Groundwork NB"
#define RD_VENDOR_URL "https://example.invalid/groundwork-nb"
#define RD_VENDOR_EMAIL "mailto:info@example.invalid"
#define RD_VERSION_STR "1.0.0"
// "Fx|Distortion" is the closest standard VST3 category for a glitch/damage effect
#define RD_SUBCATEGORY "Fx|Distortion"

namespace rd {
// Generated once; never change them or hosts will lose your projects.
static const Steinberg::FUID kProcessorUID (0x5A3C7E11, 0xB2D44F90, 0x8A1E6C02, 0x7D90A4B5);
static const Steinberg::FUID kControllerUID (0x9F0D2B64, 0x4C1A4E37, 0xA6B58D13, 0x2E7C80F1);
} // namespace rd
