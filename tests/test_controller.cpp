// Raster Damage - controller behaviour test (no host needed): preset changes must be announced
// to the host, user presets must round-trip, and restoring a project must not be overwritten.
#include "RasterController.h"
#include "RasterPresetIO.h"
#include "RasterPresets.h"

#include "base/source/fstreamer.h"
#include "public.sdk/source/common/memorystream.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"

#include <cstdio>
#include <map>

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace rd;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...)                                                      \
	do                                                                        \
	{                                                                         \
		if (cond)                                                             \
			++g_pass;                                                         \
		else                                                                  \
		{                                                                     \
			++g_fail;                                                         \
			std::printf ("  FAIL line %d: %s  ", __LINE__, #cond);            \
			std::printf (__VA_ARGS__);                                        \
			std::printf ("\n");                                               \
		}                                                                     \
	} while (0)

// A stand-in for the DAW: records everything the plugin tells it.
class FakeHost : public IComponentHandler
{
public:
	std::map<ParamID, ParamValue> performed;
	int begins = 0, ends = 0, performs = 0;
	void reset ()
	{
		performed.clear ();
		begins = ends = performs = 0;
	}
	tresult PLUGIN_API beginEdit (ParamID) SMTG_OVERRIDE { ++begins; return kResultOk; }
	tresult PLUGIN_API performEdit (ParamID id, ParamValue v) SMTG_OVERRIDE { ++performs; performed[id] = v; return kResultOk; }
	tresult PLUGIN_API endEdit (ParamID) SMTG_OVERRIDE { ++ends; return kResultOk; }
	tresult PLUGIN_API restartComponent (int32) SMTG_OVERRIDE { return kResultOk; }
	tresult PLUGIN_API queryInterface (const TUID iid, void** obj) SMTG_OVERRIDE
	{
		if (FUnknownPrivate::iidEqual (iid, IComponentHandler::iid) || FUnknownPrivate::iidEqual (iid, FUnknown::iid))
		{
			*obj = this;
			return kResultOk;
		}
		*obj = nullptr;
		return kNoInterface;
	}
	uint32 PLUGIN_API addRef () SMTG_OVERRIDE { return 1; }
	uint32 PLUGIN_API release () SMTG_OVERRIDE { return 1; }
};

static int countPresetParams ()
{
	int n = 0;
	for (int i = 0; i < kNumParams; ++i)
		n += isPresetParam (i) ? 1 : 0;
	return n;
}

int main ()
{
	FakeHost host;
	RasterController* c = new RasterController ();
	c->initialize (nullptr);
	c->setComponentHandler (&host);

	std::printf ("host picks a factory preset from its own preset list\n");
	for (int idx : {1, 7, 20, kNumPresets - 1, 0})
	{
		host.reset ();
		c->setParamNormalized (kPresetParam, static_cast<double> (idx) / (kNumPresets - 1));
		ParamSet want;
		loadPreset (idx, want);
		int wrong = 0;
		for (int i = 0; i < kNumParams; ++i)
		{
			if (!isPresetParam (i))
				continue;
			const double w = toNormalized (spec (i), want[i]);
			if (std::fabs (c->getParamNormalized (i) - w) > 1e-9)
				++wrong;
			auto it = host.performed.find (static_cast<ParamID> (i));
			if (it == host.performed.end () || std::fabs (it->second - w) > 1e-9)
				++wrong;
		}
		CHECK (wrong == 0, "preset %d: %d parameters not updated/announced", idx, wrong);
		CHECK (host.performs == countPresetParams (), "preset %d: host told about %d params, expected %d", idx, host.performs, countPresetParams ());
		CHECK (host.begins == host.ends, "begin/end balanced");
		CHECK (c->presetName () == kPresets[idx].name && !c->presetModified () && c->factoryIndex () == idx, "preset %d name/state", idx);
	}

	std::printf("editing a knob marks the preset modified\n");
	host.reset ();
	c->editParam (kTearDepth, 0.9);
	CHECK (c->presetModified (), "modified");
	CHECK (host.performed.count (kTearDepth) == 1, "edit announced");
	CHECK (c->presetName () == kPresets[0].name, "name kept");

	std::printf ("user presets: save, change, load\n");
	namespace fs = std::filesystem;
	const fs::path dir = fs::temp_directory_path () / "rd_ctrl_test";
	std::error_code ec;
	fs::remove_all (dir, ec);
	c->selectFactoryPreset (5);
	c->editParam (kSeed, 0.321);
	c->editParam (kMix, 0.42);
	const ParamSet saved = c->currentParams ();
	CHECK (c->saveUserPreset (dir / "mine.rdpreset", "mine"), "save");
	CHECK (c->presetName () == "mine" && !c->presetModified (), "saved name");
	c->selectFactoryPreset (12);
	host.reset ();
	CHECK (c->loadUserPreset (dir / "mine.rdpreset"), "load");
	const ParamSet back = c->currentParams ();
	int diff = 0;
	for (int i = 0; i < kNumParams; ++i)
		if (isPresetParam (i) && std::fabs (back[i] - saved[i]) > 1e-4 * std::max (1.0, std::fabs (saved[i])))
			++diff;
	CHECK (diff == 0, "%d parameters differ after load", diff);
	CHECK (host.performs == countPresetParams () && c->presetName () == "mine" && c->factoryIndex () == -1, "load announced + named");
	CHECK (!c->loadUserPreset (dir / "nope.rdpreset"), "missing file fails safely");

	std::printf ("restoring a saved project must not be overwritten by the preset\n");
	// state: preset 3 selected, but the user had changed Tear Depth and Mix afterwards
	MemoryStream state;
	{
		IBStreamer s (&state, kLittleEndian);
		s.writeInt32 (1);
		for (int i = 0; i < kNumParams; ++i)
		{
			double v = defaultNormalized (i);
			if (i == kTearDepth)
				v = 0.777;
			if (i == kMix)
				v = 0.123;
			s.writeDouble (v);
		}
		s.writeDouble (3.0 / (kNumPresets - 1));
	}
	state.seek (0, IBStream::kIBSeekSet, nullptr);
	CHECK (c->setComponentState (&state) == kResultOk, "setComponentState");
	CHECK (std::fabs (c->getParamNormalized (kTearDepth) - 0.777) < 1e-9, "Tear Depth restored (%g)", c->getParamNormalized (kTearDepth));
	CHECK (std::fabs (c->getParamNormalized (kMix) - 0.123) < 1e-9, "Mix restored (%g)", c->getParamNormalized (kMix));

	std::printf ("controller state keeps the preset name\n");
	c->selectFactoryPreset (9);
	c->editParam (kSeed, 0.5);
	MemoryStream cs;
	CHECK (c->getState (&cs) == kResultOk, "getState");
	RasterController* c2 = new RasterController ();
	c2->initialize (nullptr);
	cs.seek (0, IBStream::kIBSeekSet, nullptr);
	CHECK (c2->setState (&cs) == kResultOk, "setState");
	CHECK (c2->presetName () == kPresets[9].name && c2->presetModified (), "name '%s' modified=%d", c2->presetName ().c_str (), c2->presetModified ());

	fs::remove_all (dir, ec);
	c->terminate ();
	c2->terminate ();
	std::printf ("\n%d checks passed, %d failed\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
