// Raster Damage - DSP engine self-test. No plugin SDK required.
//   rd_test            run everything
#include "RasterEngine.h"
#include "RasterPresetIO.h"
#include "RasterPresets.h"

#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

using namespace rd;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...)                                                                           \
	do                                                                                             \
	{                                                                                              \
		if (cond)                                                                                  \
			++g_pass;                                                                              \
		else                                                                                       \
		{                                                                                          \
			++g_fail;                                                                              \
			std::printf ("  FAIL %s:%d  %s  ", __FILE__, __LINE__, #cond);                         \
			std::printf (__VA_ARGS__);                                                             \
			std::printf ("\n");                                                                    \
		}                                                                                          \
	} while (0)

typedef std::vector<float> Buf;

// A drum-ish + guitar-ish test signal
static void makeSignal (Buf& l, Buf& r, int n, double sr, unsigned seed = 1)
{
	std::mt19937 rng (seed);
	std::uniform_real_distribution<float> u (-1.f, 1.f);
	l.assign (n, 0.f);
	r.assign (n, 0.f);
	double ph = 0.0;
	for (int i = 0; i < n; ++i)
	{
		const double t = i / sr;
		const double beat = std::fmod (t, 0.25);
		const float kick = static_cast<float> (std::sin (2 * 3.14159265 * (60 + 120 * std::exp (-beat * 40)) * beat) * std::exp (-beat * 12));
		const float hat = u (rng) * static_cast<float> (std::exp (-std::fmod (t + 0.125, 0.25) * 60)) * 0.4f;
		ph += 2 * 3.14159265 * 110.0 / sr;
		if (ph > 6.2831853)
			ph -= 6.2831853;
		const float gtr = (static_cast<float> (ph / 3.14159265) - 1.f) * 0.3f;
		l[i] = 0.5f * kick + hat + gtr;
		r[i] = 0.5f * kick + hat * 0.8f + gtr * 0.9f;
	}
}

static void render (Engine& e, const Buf& inL, const Buf& inR, Buf& outL, Buf& outR, int block,
                    bool randomBlocks = false)
{
	const int n = static_cast<int> (inL.size ());
	outL.assign (n, 0.f);
	outR.assign (n, 0.f);
	std::mt19937 rng (5);
	int pos = 0;
	while (pos < n)
	{
		int b = block;
		if (randomBlocks)
			b = 1 + static_cast<int> (rng () % 700);
		b = std::min (b, n - pos);
		const float* in[2] = {inL.data () + pos, inR.data () + pos};
		float* out[2] = {outL.data () + pos, outR.data () + pos};
		e.process (in, out, b, 2);
		pos += b;
	}
}

static bool finiteBuf (const Buf& b, float& peak)
{
	peak = 0.f;
	for (float v : b)
	{
		if (!std::isfinite (v))
			return false;
		peak = std::max (peak, std::fabs (v));
	}
	return true;
}

static ParamSet quiet () // all damage stages off
{
	ParamSet p;
	p[kDamage] = 100;
	p[kTearAmount] = 0;
	p[kLineMissing] = 0;
	p[kSmearAmount] = 0;
	p[kLossRate] = 0;
	p[kGhost] = 0;
	p[kStatic] = -60;
	p[kMix] = 100;
	return p;
}

//------------------------------------------------------------------------
static void testParams ()
{
	std::printf ("parameters\n");
	for (int i = 0; i < kNumParams; ++i)
	{
		const ParamSpec& s = spec (i);
		CHECK (s.defV >= s.minV && s.defV <= s.maxV, "default out of range: %s", s.name);
		for (double n : {0.0, 0.1, 0.37, 0.5, 0.83, 1.0})
		{
			const double p = toPlain (s, n);
			CHECK (p >= s.minV - 1e-9 && p <= s.maxV + 1e-9, "%s plain %g", s.name, p);
			const double back = toPlain (s, toNormalized (s, p));
			CHECK (std::fabs (back - p) <= 1e-6 * std::max (1.0, std::fabs (p)), "%s roundtrip %g -> %g", s.name, p, back);
		}
		double parsed;
		const std::string txt = formatValue (s, s.defV);
		CHECK (parseValue (s, txt.c_str (), parsed), "parse '%s' (%s)", txt.c_str (), s.name);
		CHECK (stepCount (s) >= 0, "step");
	}
	CHECK (kNumPresets >= 40, "only %d presets", kNumPresets);
	for (int i = 0; i < kNumPresets; ++i)
		for (int j = i + 1; j < kNumPresets; ++j)
			CHECK (std::strcmp (kPresets[i].name, kPresets[j].name) != 0, "duplicate preset name %s", kPresets[i].name);
}

static void testClean ()
{
	std::printf ("clean passthrough (damage stages off -> bit-exact, 2-sample latency)\n");
	const double sr = 48000;
	Buf l, r, ol, or_;
	makeSignal (l, r, 48000, sr);
	Engine e;
	e.prepare (sr, 2);
	e.setParams (quiet ());
	render (e, l, r, ol, or_, 256);
	int bad = 0;
	for (size_t i = 2; i < l.size (); ++i)
		if (std::fabs (ol[i] - l[i - 2]) > 1e-7f || std::fabs (or_[i] - r[i - 2]) > 1e-7f)
			++bad;
	CHECK (bad == 0, "%d mismatching samples", bad);

	// Mix = 0 must also be the dry signal
	ParamSet p = ParamSet ();
	p[kMix] = 0;
	e.prepare (sr, 2);
	e.setParams (p);
	render (e, l, r, ol, or_, 333);
	bad = 0;
	for (size_t i = 2; i < l.size (); ++i)
		if (std::fabs (ol[i] - l[i - 2]) > 1e-7f)
			++bad;
	CHECK (bad == 0, "mix=0: %d mismatching samples", bad);
}

static void testBlockInvariance ()
{
	std::printf ("block-size invariance + determinism (all presets)\n");
	const double sr = 44100;
	Buf l, r;
	makeSignal (l, r, 44100, sr);
	int bad = 0;
	for (int pi = 0; pi < kNumPresets; ++pi)
	{
		ParamSet p;
		loadPreset (pi, p);
		if (p[kTempoSync] >= 0.5)
			p[kTempoSync] = 0; // sync depends on host transport, tested separately
		Buf a1, a2, b1, b2, c1, c2;
		Engine e1, e2, e3;
		e1.prepare (sr, 2);
		e1.setParams (p);
		render (e1, l, r, a1, a2, 64);
		e2.prepare (sr, 2);
		e2.setParams (p);
		render (e2, l, r, b1, b2, 1024);
		e3.prepare (sr, 2);
		e3.setParams (p);
		render (e3, l, r, c1, c2, 0, true);
		if (a1 != b1 || a2 != b2 || a1 != c1 || a2 != c2)
		{
			++bad;
			std::printf ("  preset '%s' differs between block sizes\n", kPresets[pi].name);
		}
	}
	CHECK (bad == 0, "%d presets are block-size dependent", bad);
}

static void testSafety ()
{
	std::printf ("finite + bounded output (all presets, 44.1k/96k, plus random fuzz)\n");
	for (double sr : {44100.0, 96000.0})
	{
		Buf l, r, ol, orr;
		makeSignal (l, r, static_cast<int> (sr * 3), sr);
		for (int pi = 0; pi < kNumPresets; ++pi)
		{
			ParamSet p;
			loadPreset (pi, p);
			Engine e;
			e.prepare (sr, 2);
			e.setParams (p);
			e.setTransport (true, true, 128.0, true, 0.0);
			render (e, l, r, ol, orr, 480);
			float pk1, pk2;
			const bool f1 = finiteBuf (ol, pk1), f2 = finiteBuf (orr, pk2);
			CHECK (f1 && f2, "preset '%s' @%.0f produced NaN/Inf", kPresets[pi].name, sr);
			CHECK (pk1 < 12.f && pk2 < 12.f, "preset '%s' @%.0f peak %.2f", kPresets[pi].name, sr, std::max (pk1, pk2));
		}
	}
	std::mt19937 rng (99);
	std::uniform_real_distribution<double> u (0.0, 1.0);
	Buf l, r, ol, orr;
	makeSignal (l, r, 44100 * 2, 44100.0);
	int fuzzBad = 0;
	for (int k = 0; k < 150; ++k)
	{
		ParamSet p;
		for (int i = 0; i < kNumParams; ++i)
			p[i] = toPlain (spec (i), u (rng));
		p[kBypass] = 0;
		Engine e;
		e.prepare (44100, 2);
		e.setParams (p);
		e.setTransport (true, true, 60.0 + 180.0 * u (rng), true, u (rng) * 100.0);
		if (k % 3 == 0)
			e.triggerSync ();
		render (e, l, r, ol, orr, 100 + static_cast<int> (u (rng) * 900));
		float a, b;
		if (!finiteBuf (ol, a) || !finiteBuf (orr, b) || std::max (a, b) > 40.f)
			++fuzzBad;
	}
	CHECK (fuzzBad == 0, "%d fuzz cases produced NaN or huge output", fuzzBad);
}

static void testTear ()
{
	std::printf ("horizontal tearing: per-slice delay offsets land where they should\n");
	const double sr = 48000;
	ParamSet p = quiet ();
	p[kSliceTime] = 10; // 480 samples
	p[kTearAmount] = 100;
	p[kTearDepth] = 10; // 480 samples max
	p[kTearShape] = kTearStair;
	p[kTearSlew] = 0;
	Engine e;
	e.prepare (sr, 2);
	e.setParams (p);
	const int L = 480, frames = 24;
	Buf l (L * frames, 0.f), r (L * frames, 0.f), ol, orr;
	for (int f = 0; f < frames; ++f)
		l[f * L + 10] = r[f * L + 10] = 1.f;
	render (e, l, r, ol, orr, 128);
	int found = 0;
	for (int f = 4; f < frames - 3; ++f)
	{
		const int d = static_cast<int> (std::lround (floorMod (f, 4) / 3.0 * 480.0));
		const int pos = f * L + 10 + 2 + d;
		if (std::fabs (ol[pos] - 1.f) < 1e-4f)
			++found;
	}
	CHECK (found == frames - 3 - 4, "%d of %d impulses arrived at the predicted delay", found, frames - 7);
}

static void testScanlines ()
{
	std::printf ("missing scanlines: periodic dropouts\n");
	const double sr = 48000;
	ParamSet p = quiet ();
	p[kSliceTime] = 10;
	p[kLinePeriod] = 4;
	p[kLineMissing] = 1;
	p[kLineMode] = kLineBlack;
	p[kLineEdge] = 0;
	Engine e;
	e.prepare (sr, 2);
	e.setParams (p);
	const int L = 480, frames = 40;
	Buf l (L * frames, 0.5f), r (L * frames, 0.5f), ol, orr;
	render (e, l, r, ol, orr, 200);
	int silent = 0, loud = 0;
	for (int f = 4; f < frames; ++f)
	{
		const int mid = f * L + L / 2;
		if (std::fabs (ol[mid]) < 1e-6f)
			++silent;
		else
			++loud;
		const bool expectSilent = floorMod (f, 4) < 1;
		CHECK ((std::fabs (ol[mid]) < 1e-6f) == expectSilent, "frame %d", f);
	}
	CHECK (silent == 9 && loud == 27, "silent=%d loud=%d", silent, loud);

	// Fill variants all stay finite
	for (int m = 0; m < kNumLineModes; ++m)
	{
		p[kLineMode] = m;
		e.prepare (sr, 2);
		e.setParams (p);
		render (e, l, r, ol, orr, 200);
		float pk;
		CHECK (finiteBuf (ol, pk) && pk < 5.f, "fill mode %d", m);
	}
}

static void testSmear ()
{
	std::printf ("pixel smearing: averaging and sample-and-hold\n");
	const double sr = 48000;
	ParamSet p = quiet ();
	p[kSliceTime] = 20;
	p[kSmearAmount] = 100;
	p[kSmearSize] = 2.0; // 96 samples
	p[kMix] = 100;
	Buf l, r, ol, orr;
	makeSignal (l, r, 48000, sr);

	p[kSmearMode] = kSmearAverage;
	Engine e;
	e.prepare (sr, 2);
	e.setParams (p);
	render (e, l, r, ol, orr, 256);
	// averaging removes high-frequency energy: compare sample-to-sample difference energy
	auto roughness = [] (const Buf& b, int from) {
		double s = 0;
		for (size_t i = from + 1; i < b.size (); ++i)
			s += std::fabs (b[i] - b[i - 1]);
		return s;
	};
	CHECK (roughness (ol, 4800) < 0.7 * roughness (l, 4800), "average smear should smooth the signal (%.1f vs %.1f)", roughness (ol, 4800), roughness (l, 4800));

	p[kSmearMode] = kSmearHold;
	e.prepare (sr, 2);
	e.setParams (p);
	render (e, l, r, ol, orr, 256);
	// held output is piecewise constant: most adjacent samples are equal
	int same = 0, tot = 0;
	for (size_t i = 24000; i < 40000; ++i, ++tot)
		if (ol[i] == ol[i - 1])
			++same;
	CHECK (same > tot * 0.9, "sample-hold should repeat values (%d/%d)", same, tot);

	p[kSmearMode] = kSmearCrunch;
	p[kColorDepth] = 4;
	e.prepare (sr, 2);
	e.setParams (p);
	render (e, l, r, ol, orr, 256);
	// 4 bit => at most ~16 distinct levels in held stage; after averaging still very few values
	std::vector<float> uniq (ol.begin () + 24000, ol.begin () + 40000);
	std::sort (uniq.begin (), uniq.end ());
	uniq.erase (std::unique (uniq.begin (), uniq.end ()), uniq.end ());
	CHECK (uniq.size () < 400, "colour depth should limit values (got %zu)", uniq.size ());
}

static void testSyncLoss ()
{
	std::printf ("sync loss: bursts only when asked, deterministic, manual trigger works\n");
	const double sr = 48000;
	Buf l, r, ol, orr, ol2, orr2;
	makeSignal (l, r, 48000, sr);
	ParamSet p = quiet ();
	p[kSliceTime] = 10;
	p[kLossRate] = 0;
	Engine e;
	e.prepare (sr, 2);
	e.setParams (p);
	render (e, l, r, ol, orr, 480);
	// rate 0 -> clean
	int bad = 0;
	for (size_t i = 2; i < l.size (); ++i)
		if (std::fabs (ol[i] - l[i - 2]) > 1e-7f)
			++bad;
	CHECK (bad == 0, "rate 0 must be clean (%d diffs)", bad);

	// manual trigger -> not clean, for each style
	for (int style = 0; style < kNumLossStyles; ++style)
	{
		ParamSet q = p;
		q[kLossStyle] = style;
		q[kLossLength] = 6;
		q[kLossIntensity] = 100;
		Engine e2;
		e2.prepare (sr, 2);
		e2.setParams (q);
		// run 5 clean frames, then trigger
		const int n = static_cast<int> (l.size ());
		Buf o1 (n), o2 (n);
		const float* in[2] = {l.data (), r.data ()};
		float* out[2] = {o1.data (), o2.data ()};
		e2.process (in, out, 2400, 2);
		e2.triggerSync ();
		const float* in2[2] = {l.data () + 2400, r.data () + 2400};
		float* out2[2] = {o1.data () + 2400, o2.data () + 2400};
		e2.process (in2, out2, n - 2400, 2);
		int diffs = 0;
		for (int i = 2400; i < n; ++i)
			if (std::fabs (o1[i] - l[i - 2]) > 1e-4f)
				++diffs;
		float pk;
		CHECK (diffs > 200, "style %d: trigger caused only %d changed samples", style, diffs);
		CHECK (finiteBuf (o1, pk), "style %d NaN", style);
	}

	// random bursts at high rate cause changes and are repeatable
	p[kLossRate] = 100;
	p[kLossLength] = 5;
	p[kSeed] = 12;
	Engine a, b;
	a.prepare (sr, 2);
	a.setParams (p);
	render (a, l, r, ol, orr, 1000);
	b.prepare (sr, 2);
	b.setParams (p);
	render (b, l, r, ol2, orr2, 77);
	CHECK (ol == ol2 && orr == orr2, "same seed must give same damage");
	int diffs = 0;
	for (size_t i = 2; i < l.size (); ++i)
		if (std::fabs (ol[i] - l[i - 2]) > 1e-4f)
			++diffs;
	CHECK (diffs > 2000, "high sync-loss rate should corrupt the signal (%d)", diffs);

	// different seed -> different damage
	p[kSeed] = 13;
	b.prepare (sr, 2);
	b.setParams (p);
	render (b, l, r, ol2, orr2, 1000);
	CHECK (ol != ol2, "different seed should differ");
}

static void testLoop ()
{
	std::printf ("frame loop: damage pattern repeats every N slices\n");
	const double sr = 48000;
	ParamSet p;
	p[kSliceTime] = 10;
	p[kFrameLoop] = 8;
	p[kDamage] = 100;
	p[kTearAmount] = 70;
	p[kTearDepth] = 6;
	p[kLinePeriod] = 5;
	p[kLineMissing] = 2;
	p[kLineMode] = kLineHold;
	p[kSmearAmount] = 50;
	p[kLossRate] = 60;
	p[kLossLength] = 3;
	p[kLossStyle] = kLossRoll;
	p[kGhost] = 30;
	p[kStatic] = -60;
	p[kSeed] = 77;
	const int period = 8 * 480;
	std::mt19937 rng (3);
	std::uniform_real_distribution<float> u (-1.f, 1.f);
	Buf one (period);
	for (auto& v : one)
		v = u (rng) * 0.5f;
	Buf l, r;
	for (int k = 0; k < 8; ++k)
		l.insert (l.end (), one.begin (), one.end ());
	r = l;
	Buf ol, orr;
	Engine e;
	e.prepare (sr, 2);
	e.setParams (p);
	render (e, l, r, ol, orr, 300);
	double maxd = 0;
	for (int i = 5 * period; i < 7 * period; ++i)
		maxd = std::max (maxd, static_cast<double> (std::fabs (ol[i] - ol[i + period])));
	CHECK (maxd < 1e-3, "pattern should repeat after one loop (max diff %g)", maxd);
}

static void testTempoSync ()
{
	std::printf ("tempo sync: frame grid locks to the host's musical position\n");
	const double sr = 48000;
	ParamSet p = quiet ();
	p[kTempoSync] = 1;
	p[kTempoDiv] = 4; // 1/16 = 0.25 beat
	Buf l (4800, 0.f), r (4800, 0.f), ol, orr;
	for (double ppq : {0.0, 1.0, 3.3, 17.77})
	{
		Engine e;
		e.prepare (sr, 2);
		e.setParams (p);
		e.setTransport (true, true, 120.0, true, ppq);
		const float* in[2] = {l.data (), r.data ()};
		Buf o1 (256), o2 (256);
		float* out[2] = {o1.data (), o2.data ()};
		e.process (in, out, 256, 2);
		const int64_t want = static_cast<int64_t> (std::floor (ppq / 0.25));
		// after 256 samples (0.0053 s = 0.0107 beat) we may or may not have crossed a boundary
		CHECK (e.currentFrame () == want || e.currentFrame () == want + 1, "ppq %.2f -> frame %lld, expected %lld", ppq, static_cast<long long> (e.currentFrame ()), static_cast<long long> (want));
		CHECK (std::fabs (e.currentFrameLength () - 0.25 * 60.0 / 120.0 * sr) < 1.0, "frame length %.2f", e.currentFrameLength ());
	}
}


static void testPresetFiles ()
{
	std::printf ("user preset files (.rdpreset)\n");
	// every factory preset survives a save -> load round trip
	for (int i = 0; i < kNumPresets; ++i)
	{
		ParamSet a, b;
		loadPreset (i, a);
		std::string name;
		const bool ok = parsePreset (serializePreset (kPresets[i].name, a), name, b);
		CHECK (ok && name == kPresets[i].name, "preset %d name", i);
		for (int k = 0; k < kNumParams; ++k)
			if (isPresetParam (k))
				CHECK (std::fabs (a[k] - b[k]) <= 1e-5 * std::max (1.0, std::fabs (a[k])), "preset %d param %s: %g vs %g", i, spec (k).name, a[k], b[k]);
	}
	// forward compatibility: unknown keys ignored, missing keys -> defaults, values clamped
	ParamSet p;
	std::string name;
	CHECK (parsePreset ("format=1\nname=X\nTear Amount=250\nNot A Param=5\nSlice Time=abc\nSeed=3.7\n", name, p), "parse");
	CHECK (p[kTearAmount] == 100.0, "clamped to max (%g)", p[kTearAmount]);
	CHECK (p[kSliceTime] == spec (kSliceTime).defV, "bad number keeps default");
	CHECK (p[kSeed] == 4.0, "ints round (%g)", p[kSeed]);
	CHECK (p[kMix] == spec (kMix).defV, "missing key = default");
	CHECK (!parsePreset ("hello world", name, p), "garbage rejected");
	CHECK (!parsePreset ("", name, p), "empty rejected");
	CHECK (!parsePreset ("name=x\nTear Amount=3\n", name, p), "missing format line rejected");
	// bypass / trigger are never stored
	ParamSet q;
	q[kBypass] = 1;
	q[kLossTrigger] = 1;
	const std::string txt = serializePreset ("t", q);
	CHECK (txt.find ("Bypass") == std::string::npos && txt.find ("Sync Loss Now") == std::string::npos, "bypass/trigger not saved");
	// file names
	CHECK (sanitizeFileName ("a/b\\c:d*e?") == "a_b_c_d_e_", "sanitize '%s'", sanitizeFileName ("a/b\\c:d*e?").c_str ());
	CHECK (sanitizeFileName ("   ") == "Untitled", "blank name");
	CHECK (sanitizeFileName ("My Preset...") == "My Preset", "trailing dots");

	// real files in a temp folder
	namespace fs = std::filesystem;
	const fs::path dir = fs::temp_directory_path () / "rd_preset_test";
	std::error_code ec;
	fs::remove_all (dir, ec);
	userPresetDirOverride () = dir / "nested" / "Presets";
	CHECK (listUserPresets ().empty (), "no folder yet -> empty list");
	ParamSet mine;
	mine[kTearDepth] = 17.5;
	mine[kSeed] = 321;
	CHECK (savePresetFile (presetDir () / "zeta one.rdpreset", "zeta one", mine), "save creates folders");
	CHECK (savePresetFile (presetDir () / "Alpha.rdpreset", "Alpha", mine), "save 2");
	{
		std::ofstream junk (presetDir () / "notes.txt");
		junk << "ignore me";
	}
	const auto list = listUserPresets ();
	CHECK (list.size () == 2 && list[0].name == "Alpha" && list[1].name == "zeta one", "list sorted, only .rdpreset (%zu)", list.size ());
	ParamSet back;
	std::string bn;
	CHECK (loadPresetFile (list[1].path, bn, back) && bn == "zeta one", "load name");
	CHECK (back[kTearDepth] == 17.5 && back[kSeed] == 321, "load values");
	{
		std::ofstream bad (presetDir () / "bad.rdpreset");
		bad << "not a preset";
	}
	CHECK (!loadPresetFile (presetDir () / "bad.rdpreset", bn, back), "bad file rejected");
	CHECK (!loadPresetFile (presetDir () / "missing.rdpreset", bn, back), "missing file rejected");
	userPresetDirOverride () = fs::path ();
	fs::remove_all (dir, ec);
	CHECK (kNumPresetCategories >= 6 && presetCategory (0) == 0 && presetCategory (kNumPresets - 1) == kNumPresetCategories - 1, "categories");
}

static void testPreviewMatchesAudio ()
{
	std::printf ("GUI preview uses the same decisions as the audio path\n");
	const double sr = 48000;
	ParamSet p = quiet ();
	p[kSliceTime] = 10;
	p[kLinePeriod] = 5;
	p[kLineMissing] = 2;
	p[kLineChaos] = 40;
	p[kLineEdge] = 0;
	p[kSeed] = 55;
	Engine audio, viz;
	audio.prepare (sr, 2);
	audio.setParams (p);
	viz.prepare (sr, 2);
	viz.setParams (p);
	const int L = 480, frames = 60;
	Buf l (L * frames, 0.5f), r (L * frames, 0.5f), ol, orr;
	render (audio, l, r, ol, orr, 333);
	int mismatch = 0;
	for (int f = 6; f < frames; ++f)
	{
		const bool predictedDrop = viz.previewFrame (f, 0).dropped;
		const bool measuredDrop = std::fabs (ol[f * L + L / 2]) < 1e-6f;
		if (predictedDrop != measuredDrop)
			++mismatch;
	}
	CHECK (mismatch == 0, "%d slices where preview and audio disagree", mismatch);
}

static void testSpeed ()
{
	std::printf ("performance\n");
	const double sr = 48000;
	Buf l, r, ol, orr;
	makeSignal (l, r, 48000 * 10, sr);
	ParamSet p;
	loadPreset (kNumPresets - 3, p);
	p[kTempoSync] = 0;
	p[kGhost] = 40;
	p[kSmearAmount] = 100;
	p[kSmearMode] = kSmearCrunch;
	p[kBandwidth] = 5000;
	p[kDrive] = 6;
	Engine e;
	e.prepare (sr, 2);
	e.setParams (p);
	const auto t0 = std::chrono::steady_clock::now ();
	render (e, l, r, ol, orr, 512);
	const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now () - t0).count ();
	const double rt = 10.0 / secs;
	std::printf ("  10 s of stereo audio rendered in %.3f s  (%.0fx realtime, %.2f%% of one core)\n", secs, rt, 100.0 / rt);
	CHECK (rt > 20.0, "too slow: %.1fx realtime", rt);
}

int main ()
{
	testParams ();
	testPresetFiles ();
	testPreviewMatchesAudio ();
	testClean ();
	testTear ();
	testScanlines ();
	testSmear ();
	testSyncLoss ();
	testLoop ();
	testTempoSync ();
	testBlockInvariance ();
	testSafety ();
	testSpeed ();
	std::printf ("\n%d checks passed, %d failed\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
