// rd_render - run audio through Raster Damage without a DAW.
//
//   rd_render --list
//   rd_render in.wav out.wav --preset "Tracking Error VHS"
//   rd_render in.wav out.wav --preset 3 --set "Tear Amount=90" --set Mix=80 --bpm 140
//   rd_render --demo demo.wav --preset 5       (renders a built-in drum+guitar loop)
//
// Reads 16/24/32-bit PCM or 32-bit float WAV (mono or stereo), writes 32-bit float WAV.
#include "RasterEngine.h"
#include "RasterPresets.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

using namespace rd;

struct Wav
{
	int sr = 44100, ch = 2;
	std::vector<float> d[2];
};

static bool readWav (const char* path, Wav& w)
{
	std::ifstream f (path, std::ios::binary);
	if (!f)
		return false;
	std::vector<unsigned char> b ((std::istreambuf_iterator<char> (f)), {});
	if (b.size () < 44 || std::memcmp (b.data (), "RIFF", 4) || std::memcmp (b.data () + 8, "WAVE", 4))
		return false;
	auto u16 = [&] (size_t o) { return static_cast<int> (b[o] | (b[o + 1] << 8)); };
	auto u32 = [&] (size_t o) { return static_cast<size_t> (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (static_cast<size_t> (b[o + 3]) << 24)); };
	int fmt = 1, bits = 16;
	size_t pos = 12, dataOff = 0, dataLen = 0;
	while (pos + 8 <= b.size ())
	{
		const size_t len = u32 (pos + 4);
		if (!std::memcmp (&b[pos], "fmt ", 4))
		{
			fmt = u16 (pos + 8);
			w.ch = u16 (pos + 10);
			w.sr = static_cast<int> (u32 (pos + 12));
			bits = u16 (pos + 22);
			if (fmt == 0xFFFE && len >= 26)
				fmt = u16 (pos + 8 + 24);
		}
		else if (!std::memcmp (&b[pos], "data", 4))
		{
			dataOff = pos + 8;
			dataLen = std::min (len, b.size () - dataOff);
			break;
		}
		pos += 8 + len + (len & 1);
	}
	if (!dataOff || w.ch < 1 || w.ch > 2)
		return false;
	const int bps = bits / 8;
	const size_t frames = dataLen / (static_cast<size_t> (bps) * w.ch);
	for (int c = 0; c < 2; ++c)
		w.d[c].assign (frames, 0.f);
	for (size_t i = 0; i < frames; ++i)
		for (int c = 0; c < w.ch; ++c)
		{
			const size_t o = dataOff + (i * w.ch + c) * bps;
			float v = 0.f;
			if (fmt == 3 && bits == 32)
				std::memcpy (&v, &b[o], 4);
			else if (bits == 16)
				v = static_cast<int16_t> (b[o] | (b[o + 1] << 8)) / 32768.f;
			else if (bits == 24)
				v = (static_cast<int32_t> ((b[o] << 8) | (b[o + 1] << 16) | (static_cast<uint32_t> (b[o + 2]) << 24)) >> 8) / 8388608.f;
			else if (bits == 32)
				v = static_cast<int32_t> (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (static_cast<uint32_t> (b[o + 3]) << 24)) / 2147483648.f;
			else
				return false;
			w.d[c][i] = v;
		}
	if (w.ch == 1)
		w.d[1] = w.d[0];
	return true;
}

static bool writeWav (const char* path, const Wav& w)
{
	std::ofstream f (path, std::ios::binary);
	if (!f)
		return false;
	const uint32_t frames = static_cast<uint32_t> (w.d[0].size ());
	const uint32_t dataLen = frames * 2 * 4;
	auto w32 = [&] (uint32_t v) { f.write (reinterpret_cast<const char*> (&v), 4); };
	auto w16 = [&] (uint16_t v) { f.write (reinterpret_cast<const char*> (&v), 2); };
	f.write ("RIFF", 4);
	w32 (36 + dataLen);
	f.write ("WAVEfmt ", 8);
	w32 (16);
	w16 (3);
	w16 (2);
	w32 (static_cast<uint32_t> (w.sr));
	w32 (static_cast<uint32_t> (w.sr) * 8);
	w16 (8);
	w16 (32);
	f.write ("data", 4);
	w32 (dataLen);
	for (uint32_t i = 0; i < frames; ++i)
	{
		f.write (reinterpret_cast<const char*> (&w.d[0][i]), 4);
		f.write (reinterpret_cast<const char*> (&w.d[1][i]), 4);
	}
	return true;
}

static void makeDemo (Wav& w, double seconds)
{
	w.sr = 44100;
	const int n = static_cast<int> (seconds * w.sr);
	for (int c = 0; c < 2; ++c)
		w.d[c].assign (n, 0.f);
	uint32_t rng = 12345;
	double ph = 0.0;
	const double bpm = 120.0, beat = 60.0 / bpm;
	const double notes[] = {110.0, 110.0, 130.8, 98.0};
	for (int i = 0; i < n; ++i)
	{
		const double t = i / static_cast<double> (w.sr);
		const double b = std::fmod (t, beat);
		const int bar = static_cast<int> (t / (beat * 4)) & 3;
		rng = rng * 1664525u + 1013904223u;
		const float nz = (static_cast<int32_t> (rng) / 2147483648.f);
		const float kick = static_cast<float> (std::sin (2 * 3.14159265 * (50 + 130 * std::exp (-b * 45)) * b) * std::exp (-b * 9));
		const double hb = std::fmod (t + beat / 2, beat);
		const float hat = nz * static_cast<float> (std::exp (-hb * 70)) * 0.35f;
		const double sb = std::fmod (t + beat, beat * 2);
		const float snare = nz * static_cast<float> (std::exp (-sb * 18)) * 0.5f * (sb < beat ? 1.f : 0.f);
		ph += 2 * 3.14159265 * notes[bar] / w.sr;
		if (ph > 6.2831853)
			ph -= 6.2831853;
		const float saw = static_cast<float> (ph / 3.14159265 - 1.0);
		const float gtr = std::tanh (saw * 3.f) * 0.25f * static_cast<float> (0.6 + 0.4 * std::exp (-std::fmod (t, beat / 2) * 6));
		w.d[0][i] = 0.6f * kick + hat + snare + gtr;
		w.d[1][i] = 0.6f * kick + hat * 0.7f + snare + gtr * 0.95f;
	}
}

static int findParam (const std::string& name)
{
	auto low = [] (std::string s) {
		for (auto& c : s)
			c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
		return s;
	};
	const std::string n = low (name);
	for (int i = 0; i < kNumParams; ++i)
		if (low (spec (i).name) == n || low (spec (i).shortName) == n)
			return i;
	return -1;
}

int main (int argc, char** argv)
{
	std::string in, out, presetArg;
	std::vector<std::string> sets;
	double bpm = 0.0, demoSecs = 0.0;
	bool list = false;
	for (int i = 1; i < argc; ++i)
	{
		const std::string a = argv[i];
		if (a == "--list")
			list = true;
		else if (a == "--preset" && i + 1 < argc)
			presetArg = argv[++i];
		else if (a == "--set" && i + 1 < argc)
			sets.push_back (argv[++i]);
		else if (a == "--bpm" && i + 1 < argc)
			bpm = std::atof (argv[++i]);
		else if (a == "--demo" && i + 1 < argc)
		{
			out = argv[++i];
			demoSecs = 8.0;
		}
		else if (in.empty () && demoSecs == 0.0)
			in = a;
		else if (out.empty ())
			out = a;
	}

	if (list)
	{
		for (int i = 0; i < kNumPresets; ++i)
			std::printf ("%2d  %s\n", i, kPresets[i].name);
		std::printf ("\nParameters (use with --set \"Name=value\"):\n");
		for (int i = 0; i < kNumParams; ++i)
		{
			const ParamSpec& s = spec (i);
			std::printf ("  %-16s [%g .. %g] default %s %s\n", s.name, s.minV, s.maxV, formatValue (s, s.defV).c_str (), s.units);
		}
		return 0;
	}
	if (out.empty ())
	{
		std::printf ("usage: rd_render in.wav out.wav [--preset N|name] [--set \"Param=value\"]... [--bpm N]\n"
		             "       rd_render --demo out.wav [--preset ...]     render a built-in demo loop\n"
		             "       rd_render --list\n");
		return 1;
	}

	Wav w;
	Wav dry;
	if (demoSecs > 0.0)
		makeDemo (w, demoSecs);
	else if (!readWav (in.c_str (), w))
	{
		std::printf ("cannot read %s (need 16/24/32-bit PCM or float WAV, mono/stereo)\n", in.c_str ());
		return 2;
	}

	ParamSet p;
	if (!presetArg.empty ())
	{
		int idx = -1;
		char* end = nullptr;
		const long v = std::strtol (presetArg.c_str (), &end, 10);
		if (*end == 0)
			idx = static_cast<int> (v);
		else
			for (int i = 0; i < kNumPresets; ++i)
				if (std::string (kPresets[i].name).find (presetArg) != std::string::npos)
				{
					idx = i;
					break;
				}
		if (idx < 0 || idx >= kNumPresets)
		{
			std::printf ("unknown preset '%s' (try --list)\n", presetArg.c_str ());
			return 3;
		}
		loadPreset (idx, p);
		std::printf ("preset: %s\n", kPresets[idx].name);
	}
	for (const auto& s : sets)
	{
		const size_t eq = s.find ('=');
		const int id = eq == std::string::npos ? -1 : findParam (s.substr (0, eq));
		double v;
		if (id < 0 || !parseValue (spec (id), s.substr (eq + 1).c_str (), v))
		{
			std::printf ("bad --set '%s' (try --list)\n", s.c_str ());
			return 4;
		}
		p[id] = v;
	}

	Engine e;
	e.prepare (w.sr, 2);
	e.setParams (p);
	const int n = static_cast<int> (w.d[0].size ());
	Wav o;
	o.sr = w.sr;
	o.d[0].assign (n, 0.f);
	o.d[1].assign (n, 0.f);
	const int block = 512;
	for (int pos = 0; pos < n; pos += block)
	{
		const int b = std::min (block, n - pos);
		if (bpm > 0.0)
			e.setTransport (true, true, bpm, true, (pos / static_cast<double> (w.sr)) * bpm / 60.0);
		const float* inp[2] = {w.d[0].data () + pos, w.d[1].data () + pos};
		float* outp[2] = {o.d[0].data () + pos, o.d[1].data () + pos};
		e.process (inp, outp, b, 2);
	}
	// compensate the engine's 2 sample latency so the file lines up with the source
	const int lat = e.latencySamples ();
	for (int c = 0; c < 2; ++c)
	{
		o.d[c].erase (o.d[c].begin (), o.d[c].begin () + lat);
		o.d[c].resize (n, 0.f);
	}
	if (!writeWav (out.c_str (), o))
	{
		std::printf ("cannot write %s\n", out.c_str ());
		return 5;
	}
	std::printf ("wrote %s (%.1f s, %d Hz)\n", out.c_str (), n / static_cast<double> (w.sr), w.sr);
	return 0;
}
