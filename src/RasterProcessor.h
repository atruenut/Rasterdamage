// Raster Damage - VST3 audio processor.
#pragma once

#include "RasterEngine.h"
#include "RasterParams.h"
#include "public.sdk/source/vst/vstaudioeffect.h"

namespace rd {

class RasterProcessor : public Steinberg::Vst::AudioEffect
{
public:
	RasterProcessor ();

	static Steinberg::FUnknown* createInstance (void*)
	{
		return static_cast<Steinberg::Vst::IAudioProcessor*> (new RasterProcessor ());
	}

	Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setBusArrangements (Steinberg::Vst::SpeakerArrangement* inputs,
	                                                  Steinberg::int32 numIns,
	                                                  Steinberg::Vst::SpeakerArrangement* outputs,
	                                                  Steinberg::int32 numOuts) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& setup) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setActive (Steinberg::TBool state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
	Steinberg::uint32 PLUGIN_API getLatencySamples () SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;

private:
	void applyEvent (Steinberg::Vst::ParamID id, double value);
	void pushParams ();
	void runSegment (Steinberg::Vst::ProcessData& data, int start, int end);

	struct Event
	{
		int offset;
		Steinberg::Vst::ParamID id;
		double value;
	};
	std::vector<Event> events_;

	Engine engine_;
	double norm_[kNumParams];
	double preset_ = 0.0;
	int presetIdx_ = -1;
	bool dirty_ = true;
	bool lastTrigger_ = false;
	int channels_ = 2;
	double sampleRate_ = 44100.0;
	std::vector<float> tmpIn_[2];
	std::vector<float> tmpOut_[2];
};

} // namespace rd
