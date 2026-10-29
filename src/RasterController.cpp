#include "RasterController.h"
#include "RasterPresetIO.h"
#include "RasterPresets.h"
#ifdef RD_WITH_GUI
#include "RasterEditor.h"
#endif

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstunits.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace rd {

static void asciiToTChar (const char* src, TChar* dst, int maxLen = 128)
{
	int i = 0;
	for (; src[i] && i < maxLen - 1; ++i)
		dst[i] = static_cast<TChar> (static_cast<unsigned char> (src[i]));
	dst[i] = 0;
}

static std::string tcharToAscii (const TChar* s)
{
	std::string r;
	for (; s && *s; ++s)
		r.push_back (*s < 128 ? static_cast<char> (*s) : '?');
	return r;
}

//------------------------------------------------------------------------
// A parameter whose display/conversion behaviour comes from the rd::spec table.
//------------------------------------------------------------------------
class RdParameter : public Parameter
{
public:
	RdParameter (int id, const ParamSpec& s) : spec_ (s)
	{
		asciiToTChar (s.name, info.title);
		asciiToTChar (s.shortName, info.shortTitle);
		asciiToTChar (s.units, info.units);
		info.id = static_cast<ParamID> (id);
		info.stepCount = stepCount (s);
		info.defaultNormalizedValue = valueNormalized = defaultNormalized (id);
		info.unitId = s.unit;
		info.flags = ParameterInfo::kCanAutomate;
		if (s.kind == Kind::List)
			info.flags |= ParameterInfo::kIsList;
		if (id == kBypass)
			info.flags |= ParameterInfo::kIsBypass;
	}

	void toString (ParamValue norm, String128 string) const SMTG_OVERRIDE
	{
		asciiToTChar (formatValue (spec_, toPlain (norm)).c_str (), string);
	}

	bool fromString (const TChar* string, ParamValue& norm) const SMTG_OVERRIDE
	{
		double plain = 0.0;
		if (!parseValue (spec_, tcharToAscii (string).c_str (), plain))
			return false;
		norm = toNormalized (plain);
		return true;
	}

	ParamValue toPlain (ParamValue norm) const SMTG_OVERRIDE { return rd::toPlain (spec_, norm); }
	ParamValue toNormalized (ParamValue plain) const SMTG_OVERRIDE
	{
		return rd::toNormalized (spec_, plain);
	}

private:
	const ParamSpec& spec_;
};

//------------------------------------------------------------------------
tresult PLUGIN_API RasterController::initialize (FUnknown* context)
{
	tresult r = EditControllerEx1::initialize (context);
	if (r != kResultOk)
		return r;

	// Units: the root holds the preset list, the others group the parameters
	String128 name;
	asciiToTChar (unitName (kUnitRoot), name);
	addUnit (new Unit (name, kUnitRoot, kNoParentUnitId, kPresetParam));
	for (int u = kUnitScan; u < kNumUnits; ++u)
	{
		asciiToTChar (unitName (u), name);
		addUnit (new Unit (name, u, kUnitRoot));
	}

	for (int i = 0; i < kNumParams; ++i)
		parameters.addParameter (new RdParameter (i, spec (i)));

	// Preset list
	asciiToTChar ("Preset", name);
	auto* list = new ProgramList (name, kPresetParam, kUnitRoot);
	for (int i = 0; i < kNumPresets; ++i)
	{
		String128 pn;
		asciiToTChar (kPresets[i].name, pn);
		list->addProgram (pn);
	}
	addProgramList (list);
	Parameter* presetParam = list->getParameter ();
	presetParam->getInfo ().flags &= ~ParameterInfo::kCanAutomate;
	parameters.addParameter (presetParam);
	return kResultOk;
}

tresult PLUGIN_API RasterController::setComponentState (IBStream* state)
{
	if (!state)
		return kResultFalse;
	IBStreamer s (state, kLittleEndian);
	int32 version = 0;
	if (!s.readInt32 (version) || version != 1)
		return kResultFalse;
	loadingState_ = true; // restoring a project must never re-apply a preset on top of it
	for (int i = 0; i < kNumParams; ++i)
	{
		double v = 0.0;
		if (!s.readDouble (v))
		{
			loadingState_ = false;
			return kResultFalse;
		}
		EditControllerEx1::setParamNormalized (static_cast<ParamID> (i), clamp01 (v));
	}
	double pr = 0.0;
	if (s.readDouble (pr))
	{
		EditControllerEx1::setParamNormalized (kPresetParam, clamp01 (pr));
		// fall back to the factory name until setState() tells us better
		const int idx = static_cast<int> (std::lround (clamp01 (pr) * (kNumPresets - 1)));
		presetName_ = kPresets[idx].name;
		factoryIndex_ = idx;
	}
	loadingState_ = false;
	modified_ = false;
	++revision_;
	return kResultOk;
}

tresult PLUGIN_API RasterController::setParamNormalized (ParamID tag, ParamValue value)
{
	tresult res = EditControllerEx1::setParamNormalized (tag, value);
	if (res != kResultOk || applying_ || loadingState_)
		return res;
	if (tag == kPresetParam)
	{
		// The host's own preset/program selector was used.
		const int idx = static_cast<int> (std::lround (clamp01 (value) * (kNumPresets - 1)));
		ParamSet ps;
		loadPreset (idx, ps);
		applyParams (ps, kPresets[idx].name, idx, false); // host already knows the program value
	}
	else if (tag < static_cast<ParamID> (kNumParams) && isPresetParam (static_cast<int> (tag)))
		markModified ();
	return res;
}

void RasterController::markModified ()
{
	if (!modified_)
	{
		modified_ = true;
		++revision_;
	}
}

ParamSet RasterController::currentParams ()
{
	ParamSet p;
	for (int i = 0; i < kNumParams; ++i)
		p[i] = toPlain (spec (i), getParamNormalized (static_cast<ParamID> (i)));
	return p;
}

void RasterController::editParam (int id, double normalized)
{
	normalized = clamp01 (normalized);
	beginEdit (static_cast<ParamID> (id));
	EditControllerEx1::setParamNormalized (static_cast<ParamID> (id), normalized);
	performEdit (static_cast<ParamID> (id), normalized);
	endEdit (static_cast<ParamID> (id));
	if (isPresetParam (id))
		markModified ();
}

void RasterController::applyParams (const ParamSet& p, const std::string& name, int factoryIndex, bool announceProgram)
{
	applying_ = true;
	startGroupEdit ();
	for (int i = 0; i < kNumParams; ++i)
	{
		if (!isPresetParam (i))
			continue;
		const double n = toNormalized (spec (i), p[i]);
		EditControllerEx1::setParamNormalized (static_cast<ParamID> (i), n);
		beginEdit (static_cast<ParamID> (i));
		performEdit (static_cast<ParamID> (i), n);
		endEdit (static_cast<ParamID> (i));
	}
	if (factoryIndex >= 0 && announceProgram)
	{
		const double pn = kNumPresets > 1 ? static_cast<double> (factoryIndex) / (kNumPresets - 1) : 0.0;
		EditControllerEx1::setParamNormalized (kPresetParam, pn);
		beginEdit (kPresetParam);
		performEdit (kPresetParam, pn);
		endEdit (kPresetParam);
	}
	finishGroupEdit ();
	applying_ = false;
	factoryIndex_ = factoryIndex;
	presetName_ = name;
	modified_ = false;
	++revision_;
}

void RasterController::selectFactoryPreset (int index)
{
	if (index < 0 || index >= kNumPresets)
		return;
	ParamSet ps;
	loadPreset (index, ps);
	applyParams (ps, kPresets[index].name, index);
}

bool RasterController::loadUserPreset (const std::filesystem::path& file)
{
	ParamSet ps;
	std::string name;
	if (!loadPresetFile (file, name, ps))
		return false;
	applyParams (ps, name, -1);
	return true;
}

bool RasterController::saveUserPreset (const std::filesystem::path& file, const std::string& name)
{
	if (!savePresetFile (file, name, currentParams ()))
		return false;
	presetName_ = name;
	modified_ = false;
	++revision_;
	return true;
}

// Editor-only state (not part of the audio state): which preset name to show.
tresult PLUGIN_API RasterController::getState (IBStream* state)
{
	if (!state)
		return kResultFalse;
	IBStreamer s (state, kLittleEndian);
	s.writeInt32 (1);
	s.writeInt32 (modified_ ? 1 : 0);
	s.writeStr8 (presetName_.c_str ());
	return kResultOk;
}

tresult PLUGIN_API RasterController::setState (IBStream* state)
{
	if (!state)
		return kResultFalse;
	IBStreamer s (state, kLittleEndian);
	int32 version = 0, modified = 0;
	if (!s.readInt32 (version) || version != 1 || !s.readInt32 (modified))
		return kResultFalse;
	char8* str = s.readStr8 ();
	if (str)
	{
		presetName_ = str;
		delete[] str;
	}
	modified_ = modified != 0;
	++revision_;
	return kResultOk;
}

IPlugView* PLUGIN_API RasterController::createView (FIDString name)
{
#ifdef RD_WITH_GUI
	if (name && std::strcmp (name, ViewType::kEditor) == 0)
		return new RasterEditor (this);
#else
	(void)name;
#endif
	return nullptr; // host falls back to its generic parameter UI
}

} // namespace rd
