#include "RasterProcessor.h"
#include "RasterCids.h"
#include "RasterPresets.h"

#include <algorithm>
#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace rd {

static const int32 kStateVersion = 1;

RasterProcessor::RasterProcessor ()
{
	setControllerClass (kControllerUID);
	for (int i = 0; i < kNumParams; ++i)
		norm_[i] = defaultNormalized (i);
}

tresult PLUGIN_API RasterProcessor::initialize (FUnknown* context)
{
	tresult r = AudioEffect::initialize (context);
	if (r != kResultOk)
		return r;
	addAudioInput (STR16 ("Stereo In"), SpeakerArr::kStereo);
	addAudioOutput (STR16 ("Stereo Out"), SpeakerArr::kStereo);
	return kResultOk;
}

tresult PLUGIN_API RasterProcessor::setBusArrangements (SpeakerArrangement* inputs, int32 numIns,
                                                        SpeakerArrangement* outputs, int32 numOuts)
{
	if (numIns != 1 || numOuts != 1)
		return kResultFalse;
	const bool stereo = inputs[0] == SpeakerArr::kStereo && outputs[0] == SpeakerArr::kStereo;
	const bool mono = inputs[0] == SpeakerArr::kMono && outputs[0] == SpeakerArr::kMono;
	if (!stereo && !mono)
		return kResultFalse;
	return AudioEffect::setBusArrangements (inputs, numIns, outputs, numOuts);
}

tresult PLUGIN_API RasterProcessor::canProcessSampleSize (int32 symbolicSampleSize)
{
	return (symbolicSampleSize == kSample32 || symbolicSampleSize == kSample64) ? kResultTrue :
	                                                                              kResultFalse;
}

tresult PLUGIN_API RasterProcessor::setupProcessing (ProcessSetup& setup)
{
	sampleRate_ = setup.sampleRate;
	channels_ = 2;
	if (AudioBus* bus = getAudioOutput (0))
		channels_ = SpeakerArr::getChannelCount (bus->getArrangement ());
	engine_.prepare (sampleRate_, channels_);
	for (int c = 0; c < 2; ++c)
	{
		tmpIn_[c].assign (static_cast<size_t> (std::max (1, setup.maxSamplesPerBlock)), 0.f);
		tmpOut_[c].assign (static_cast<size_t> (std::max (1, setup.maxSamplesPerBlock)), 0.f);
	}
	dirty_ = true;
	return AudioEffect::setupProcessing (setup);
}

tresult PLUGIN_API RasterProcessor::setActive (TBool state)
{
	if (state)
	{
		engine_.reset ();
		dirty_ = true;
	}
	return AudioEffect::setActive (state);
}

uint32 PLUGIN_API RasterProcessor::getLatencySamples ()
{
	return static_cast<uint32> (engine_.latencySamples ());
}

void RasterProcessor::applyEvent (ParamID id, double v)
{
	if (id == kPresetParam)
	{
		// Only remembered for the saved state. The *controller* applies a preset by changing the
		// individual parameters through the host, so they arrive here as normal changes.
		preset_ = clamp01 (v);
	}
	else if (id < static_cast<ParamID> (kNumParams))
	{
		norm_[id] = clamp01 (v);
		dirty_ = true;
	}
}

void RasterProcessor::pushParams ()
{
	ParamSet p;
	for (int i = 0; i < kNumParams; ++i)
		p[i] = toPlain (spec (i), norm_[i]);
	engine_.setParams (p);
	engine_.setBypass (p[kBypass] >= 0.5);
	dirty_ = false;

	// momentary "Sync Loss Now": a rising edge fires one burst
	const bool trig = norm_[kLossTrigger] >= 0.5;
	if (trig && !lastTrigger_)
		engine_.triggerSync ();
	lastTrigger_ = trig;
}

// Process samples [start, end) of the host block.
void RasterProcessor::runSegment (ProcessData& data, int start, int end)
{
	const int n = end - start;
	if (n <= 0)
		return;
	AudioBusBuffers& in = data.inputs[0];
	AudioBusBuffers& out = data.outputs[0];
	const int numCh = static_cast<int> (std::min (std::min (in.numChannels, out.numChannels), 2));

	if (data.symbolicSampleSize == kSample32)
	{
		float* ins[2] = {nullptr, nullptr};
		float* outs[2] = {nullptr, nullptr};
		for (int c = 0; c < numCh; ++c)
		{
			ins[c] = in.channelBuffers32[c] + start;
			outs[c] = out.channelBuffers32[c] + start;
		}
		engine_.process (ins, outs, n, numCh);
	}
	else
	{
		float* ins[2] = {tmpIn_[0].data (), tmpIn_[1].data ()};
		float* outs[2] = {tmpOut_[0].data (), tmpOut_[1].data ()};
		for (int c = 0; c < numCh; ++c)
			for (int i = 0; i < n; ++i)
				ins[c][i] = static_cast<float> (in.channelBuffers64[c][start + i]);
		engine_.process (ins, outs, n, numCh);
		for (int c = 0; c < numCh; ++c)
			for (int i = 0; i < n; ++i)
				out.channelBuffers64[c][start + i] = static_cast<double> (outs[c][i]);
	}
}

tresult PLUGIN_API RasterProcessor::process (ProcessData& data)
{
	// ---- collect every automation point, sample accurate ---------------------------
	events_.clear ();
	if (data.inputParameterChanges)
	{
		const int32 nq = data.inputParameterChanges->getParameterCount ();
		for (int32 i = 0; i < nq; ++i)
		{
			IParamValueQueue* q = data.inputParameterChanges->getParameterData (i);
			if (!q)
				continue;
			const ParamID id = q->getParameterId ();
			const int32 pts = q->getPointCount ();
			for (int32 k = 0; k < pts; ++k)
			{
				int32 offset = 0;
				ParamValue v = 0.0;
				if (q->getPoint (k, offset, v) == kResultTrue)
					events_.push_back ({std::max (0, offset), id, v});
			}
		}
		std::stable_sort (events_.begin (), events_.end (),
		                  [] (const Event& a, const Event& b) { return a.offset < b.offset; });
	}

	// ---- transport -------------------------------------------------------------------------
	if (data.processContext)
	{
		const ProcessContext* pc = data.processContext;
		engine_.setTransport ((pc->state & ProcessContext::kPlaying) != 0,
		                      (pc->state & ProcessContext::kTempoValid) != 0, pc->tempo,
		                      (pc->state & ProcessContext::kProjectTimeMusicValid) != 0,
		                      pc->projectTimeMusic);
	}
	else
		engine_.setTransport (false, false, 0.0, false, 0.0);

	const int n = data.numSamples;
	const bool haveAudio = n > 0 && data.numInputs > 0 && data.numOutputs > 0;

	if (haveAudio && data.symbolicSampleSize == kSample64 && static_cast<int> (tmpIn_[0].size ()) < n)
		for (int c = 0; c < 2; ++c)
		{
			tmpIn_[c].resize (static_cast<size_t> (n));
			tmpOut_[c].resize (static_cast<size_t> (n));
		}

	// apply changes that happen at/before sample 0 first
	size_t e = 0;
	int pos = 0;
	while (e < events_.size () && events_[e].offset <= 0)
	{
		applyEvent (events_[e].id, events_[e].value);
		++e;
	}
	if (dirty_)
		pushParams ();

	while (e < events_.size ())
	{
		const int at = std::min (events_[e].offset, n);
		if (haveAudio)
			runSegment (data, pos, at);
		pos = std::max (pos, at);
		const int offset = events_[e].offset;
		while (e < events_.size () && events_[e].offset == offset)
		{
			applyEvent (events_[e].id, events_[e].value);
			++e;
		}
		if (dirty_)
			pushParams ();
	}
	if (haveAudio)
		runSegment (data, pos, n);

	// ---- tell the host which output channels are digital silence ------------------------
	if (haveAudio)
	{
		AudioBusBuffers& out = data.outputs[0];
		uint64 flags = 0;
		for (int32 c = 0; c < out.numChannels && c < 64; ++c)
		{
			bool silent = true;
			if (data.symbolicSampleSize == kSample32)
			{
				const float* b = out.channelBuffers32[c];
				for (int i = 0; i < n && silent; ++i)
					silent = b[i] == 0.f;
			}
			else
			{
				const double* b = out.channelBuffers64[c];
				for (int i = 0; i < n && silent; ++i)
					silent = b[i] == 0.0;
			}
			if (silent)
				flags |= (static_cast<uint64> (1) << c);
		}
		out.silenceFlags = flags;
	}
	return kResultOk;
}

tresult PLUGIN_API RasterProcessor::setState (IBStream* state)
{
	if (!state)
		return kResultFalse;
	IBStreamer s (state, kLittleEndian);
	int32 version = 0;
	if (!s.readInt32 (version) || version != kStateVersion)
		return kResultFalse;
	for (int i = 0; i < kNumParams; ++i)
	{
		double v = 0.0;
		if (!s.readDouble (v))
			return kResultFalse;
		norm_[i] = clamp01 (v);
	}
	double pr = 0.0;
	if (s.readDouble (pr))
		preset_ = clamp01 (pr);
	dirty_ = true;
	return kResultOk;
}

tresult PLUGIN_API RasterProcessor::getState (IBStream* state)
{
	if (!state)
		return kResultFalse;
	IBStreamer s (state, kLittleEndian);
	s.writeInt32 (kStateVersion);
	for (int i = 0; i < kNumParams; ++i)
		s.writeDouble (norm_[i]);
	s.writeDouble (preset_);
	return kResultOk;
}

} // namespace rd
