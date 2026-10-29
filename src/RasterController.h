// Raster Damage - VST3 edit controller (parameters, units, presets).
#pragma once

#include "RasterParams.h"
#include "public.sdk/source/vst/vsteditcontroller.h"

#include <filesystem>
#include <string>

namespace rd {

class RasterController : public Steinberg::Vst::EditControllerEx1
{
public:
	static Steinberg::FUnknown* createInstance (void*)
	{
		return static_cast<Steinberg::Vst::IEditController*> (new RasterController ());
	}

	Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setComponentState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setParamNormalized (Steinberg::Vst::ParamID tag,
	                                                  Steinberg::Vst::ParamValue value) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::IPlugView* PLUGIN_API createView (Steinberg::FIDString name) SMTG_OVERRIDE;

	//--- used by the editor ------------------------------------------------------
	ParamSet currentParams ();
	// Set one parameter and tell the host (begin/perform/end in one go).
	void editParam (int id, double normalized);
	// Make a complete sound the current one. Every changed parameter is announced to the host,
	// so host UIs, automation lanes and the audio processor all stay in sync.
	void applyParams (const ParamSet& p, const std::string& name, int factoryIndex, bool announceProgram = true);
	void selectFactoryPreset (int index);
	bool loadUserPreset (const std::filesystem::path& file);
	bool saveUserPreset (const std::filesystem::path& file, const std::string& name);

	const std::string& presetName () const { return presetName_; }
	bool presetModified () const { return modified_; }
	// Increments whenever the preset name / modified flag changes (editors poll this).
	int presetRevision () const { return revision_; }
	// Index of the factory preset that is loaded, or -1 (user preset / default / unknown).
	int factoryIndex () const { return factoryIndex_; }

private:
	void markModified ();

	std::string presetName_ = "Default";
	bool modified_ = false;
	bool applying_ = false;
	bool loadingState_ = false;
	int revision_ = 0;
	int factoryIndex_ = -1;
};

} // namespace rd
