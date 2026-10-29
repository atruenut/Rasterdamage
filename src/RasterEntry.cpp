// Raster Damage - VST3 module entry (factory).
#include "RasterCids.h"
#include "RasterController.h"
#include "RasterProcessor.h"

#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

BEGIN_FACTORY_DEF (RD_VENDOR, RD_VENDOR_URL, RD_VENDOR_EMAIL)

	DEF_CLASS2 (INLINE_UID_FROM_FUID (rd::kProcessorUID), PClassInfo::kManyInstances,
	            kVstAudioEffectClass, RD_PLUGIN_NAME, Vst::kDistributable, RD_SUBCATEGORY,
	            RD_VERSION_STR, kVstVersionString, rd::RasterProcessor::createInstance)

	DEF_CLASS2 (INLINE_UID_FROM_FUID (rd::kControllerUID), PClassInfo::kManyInstances,
	            kVstComponentControllerClass, RD_PLUGIN_NAME " Controller", 0, "", RD_VERSION_STR,
	            kVstVersionString, rd::RasterController::createInstance)

END_FACTORY
