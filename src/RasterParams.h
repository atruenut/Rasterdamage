// Raster Damage - parameter table (shared by the DSP engine, VST3 processor,
// VST3 controller, presets and the offline test tools).
//
// Everything here is plain C++17 with no plugin-SDK dependency.
#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace rd {

//------------------------------------------------------------------------
// Parameter ids. These are also the VST3 parameter ids - never reorder or
// renumber them once you have shipped presets / saved projects.
//------------------------------------------------------------------------
enum ParamId : int32_t
{
	// --- SCAN: how the signal is cut into "frames" (slices) -------------
	kSliceTime = 0,
	kTempoSync,
	kTempoDiv,
	kSliceJitter,
	kFrameLoop,
	kSeed,
	kDamage,
	kInputFollow,
	kInterlace,

	// --- TEAR: horizontal tearing (per-slice delay offsets) -------------
	kTearAmount,
	kTearDepth,
	kTearShape,
	kTearSlew,
	kGhost,

	// --- SCANLINES: missing lines (periodic dropouts) --------------------
	kLinePeriod,
	kLineMissing,
	kLinePhase,
	kLineChaos,
	kLineMode,
	kLineDepth,
	kLineEdge,

	// --- SMEAR: pixel smearing (short-window average / sample & hold) ---
	kSmearAmount,
	kSmearMode,
	kSmearSize,
	kColorDepth,

	// --- SYNC LOSS: controlled bursts of timing disruption --------------
	kLossRate,
	kLossLength,
	kLossIntensity,
	kLossStyle,
	kLossTrigger,

	// --- OUTPUT ----------------------------------------------------------
	kBandwidth,
	kStatic,
	kDrive,
	kFieldSplit,
	kMix,
	kOutGain,
	kBypass,

	kNumParams,

	// The preset (program list) selector. It lives outside the normal
	// parameter array, so it can never collide with the ids above.
	kPresetParam = 1000
};

enum UnitId : int32_t
{
	kUnitRoot = 0,
	kUnitScan = 1,
	kUnitTear,
	kUnitScanlines,
	kUnitSmear,
	kUnitSyncLoss,
	kUnitOutput,
	kNumUnits = 7
};

inline const char* unitName (int unit)
{
	static const char* const names[kNumUnits] = {"Raster Damage", "Scan",     "Tear",
	                                              "Scanlines",     "Smear",    "Sync Loss",
	                                              "Output"};
	return names[unit < 0 || unit >= kNumUnits ? 0 : unit];
}

//------------------------------------------------------------------------
// Enumerations used by list parameters
//------------------------------------------------------------------------
enum Interlace { kInterlaceOff = 0, kInterlaceEven, kInterlaceOdd, kInterlaceWeave, kNumInterlace };
enum TearShape { kTearRandom = 0, kTearStair, kTearSkew, kTearSine, kTearZigzag, kNumTearShapes };
enum LineMode
{
	kLineBlack = 0,
	kLineSnow,
	kLinePrevious,
	kLineNegative,
	kLineHold,
	kNumLineModes
};
enum SmearMode { kSmearAverage = 0, kSmearHold, kSmearBleed, kSmearCrunch, kNumSmearModes };
enum LossStyle { kLossJitter = 0, kLossRoll, kLossStutter, kLossChaos, kNumLossStyles };

//------------------------------------------------------------------------
// Parameter descriptions
//------------------------------------------------------------------------
enum class Kind
{
	Float,
	Int,
	List,
	Toggle
};
enum class Curve
{
	Linear,
	Log,
	Squared
};

struct ParamSpec
{
	const char* name;
	const char* shortName;
	const char* units;
	Kind kind;
	Curve curve;
	double minV;
	double maxV;
	double defV;
	const char* const* list; // for Kind::List
	int listCount;
	int unit;
	int precision;
	const char* zeroText; // text shown at minV (e.g. "Off") or nullptr
};

namespace detail {
static const char* const kDivNames[] = {"1/64", "1/32T", "1/32", "1/16T", "1/16",
                                        "1/8T", "1/8",   "1/4T", "1/4",   "1/2"};
static const double kDivBeats[] = {1.0 / 16, 1.0 / 12, 1.0 / 8, 1.0 / 6, 0.25,
                                   1.0 / 3,  0.5,      2.0 / 3, 1.0,     2.0};
static const char* const kInterlaceNames[] = {"Off", "Even Field", "Odd Field", "Weave L/R"};
static const char* const kShapeNames[] = {"Random", "Stair", "Skew", "Sine", "Zigzag"};
static const char* const kLineNames[] = {"Black", "Snow", "Previous Line", "Negative", "Hold"};
static const char* const kSmearNames[] = {"Average", "Sample-Hold", "Bleed", "Crunch"};
static const char* const kLossNames[] = {"Jitter", "Roll", "Stutter", "Chaos"};
} // namespace detail

inline double divisionBeats (int index)
{
	index = std::max (0, std::min (index, 9));
	return detail::kDivBeats[index];
}

inline const ParamSpec& spec (int id)
{
	using namespace detail;
	// clang-format off
	static const ParamSpec table[kNumParams] = {
	//  name              short       units    kind          curve          min    max     def    list            n  unit            prec zero
	/* kSliceTime   */ {"Slice Time",     "Slice",   "ms",   Kind::Float,  Curve::Log,     0.5,   250.0,  12.0,  nullptr,        0, kUnitScan,      2, nullptr},
	/* kTempoSync   */ {"Tempo Sync",     "Sync",    "",     Kind::Toggle, Curve::Linear,  0.0,   1.0,    0.0,   nullptr,        0, kUnitScan,      0, nullptr},
	/* kTempoDiv    */ {"Slice Division", "Div",     "",     Kind::List,   Curve::Linear,  0.0,   9.0,    4.0,   kDivNames,     10, kUnitScan,      0, nullptr},
	/* kSliceJitter */ {"Slice Jitter",   "Jitter",  "%",    Kind::Float,  Curve::Linear,  0.0,   100.0,  0.0,   nullptr,        0, kUnitScan,      1, nullptr},
	/* kFrameLoop   */ {"Frame Loop",     "Loop",    "slices", Kind::Int,  Curve::Linear,  0.0,   64.0,   0.0,   nullptr,        0, kUnitScan,      0, "Off"},
	/* kSeed        */ {"Seed",           "Seed",    "",     Kind::Int,    Curve::Linear,  0.0,   999.0,  1.0,   nullptr,        0, kUnitScan,      0, nullptr},
	/* kDamage      */ {"Damage",         "Damage",  "%",    Kind::Float,  Curve::Linear,  0.0,   100.0,  70.0,  nullptr,        0, kUnitScan,      1, nullptr},
	/* kInputFollow */ {"Input Follow",   "Follow",  "%",    Kind::Float,  Curve::Linear, -100.0, 100.0,  0.0,   nullptr,        0, kUnitScan,      0, nullptr},
	/* kInterlace   */ {"Interlace",      "Interlc", "",     Kind::List,   Curve::Linear,  0.0,   3.0,    0.0,   kInterlaceNames, 4, kUnitScan,    0, nullptr},

	/* kTearAmount  */ {"Tear Amount",    "Tear",    "%",    Kind::Float,  Curve::Linear,  0.0,   100.0,  60.0,  nullptr,        0, kUnitTear,      1, nullptr},
	/* kTearDepth   */ {"Tear Depth",     "Depth",   "ms",   Kind::Float,  Curve::Squared, 0.0,   100.0,  8.0,   nullptr,        0, kUnitTear,      2, nullptr},
	/* kTearShape   */ {"Tear Shape",     "Shape",   "",     Kind::List,   Curve::Linear,  0.0,   4.0,    0.0,   kShapeNames,    5, kUnitTear,      0, nullptr},
	/* kTearSlew    */ {"Tear Slew",      "Slew",    "%",    Kind::Float,  Curve::Linear,  0.0,   100.0,  20.0,  nullptr,        0, kUnitTear,      1, nullptr},
	/* kGhost       */ {"Ghosting",       "Ghost",   "%",    Kind::Float,  Curve::Linear,  0.0,   90.0,   0.0,   nullptr,        0, kUnitTear,      1, nullptr},

	/* kLinePeriod  */ {"Line Period",    "Period",  "lines", Kind::Int,   Curve::Linear,  2.0,   32.0,   4.0,   nullptr,        0, kUnitScanlines, 0, nullptr},
	/* kLineMissing */ {"Missing Lines",  "Missing", "lines", Kind::Int,   Curve::Linear,  0.0,   32.0,   1.0,   nullptr,        0, kUnitScanlines, 0, nullptr},
	/* kLinePhase   */ {"Line Phase",     "Phase",   "lines", Kind::Int,   Curve::Linear,  0.0,   31.0,   0.0,   nullptr,        0, kUnitScanlines, 0, nullptr},
	/* kLineChaos   */ {"Line Chaos",     "Chaos",   "%",    Kind::Float,  Curve::Linear,  0.0,   100.0,  0.0,   nullptr,        0, kUnitScanlines, 1, nullptr},
	/* kLineMode    */ {"Dropout Fill",   "Fill",    "",     Kind::List,   Curve::Linear,  0.0,   4.0,    0.0,   kLineNames,     5, kUnitScanlines, 0, nullptr},
	/* kLineDepth   */ {"Dropout Depth",  "Depth",   "%",    Kind::Float,  Curve::Linear,  0.0,   100.0,  100.0, nullptr,        0, kUnitScanlines, 1, nullptr},
	/* kLineEdge    */ {"Line Edge",      "Edge",    "ms",   Kind::Float,  Curve::Squared, 0.0,   10.0,   0.5,   nullptr,        0, kUnitScanlines, 2, nullptr},

	/* kSmearAmount */ {"Smear Amount",   "Smear",   "%",    Kind::Float,  Curve::Linear,  0.0,   100.0,  40.0,  nullptr,        0, kUnitSmear,     1, nullptr},
	/* kSmearMode   */ {"Smear Mode",     "Mode",    "",     Kind::List,   Curve::Linear,  0.0,   3.0,    0.0,   kSmearNames,    4, kUnitSmear,     0, nullptr},
	/* kSmearSize   */ {"Smear Size",     "Size",    "ms",   Kind::Float,  Curve::Log,     0.02,  20.0,   0.4,   nullptr,        0, kUnitSmear,     3, nullptr},
	/* kColorDepth  */ {"Color Depth",    "Bits",    "bit",  Kind::Int,    Curve::Linear,  2.0,   16.0,   16.0,  nullptr,        0, kUnitSmear,     0, nullptr},

	/* kLossRate    */ {"Sync Loss Rate", "Rate",    "%",    Kind::Float,  Curve::Linear,  0.0,   100.0,  10.0,  nullptr,        0, kUnitSyncLoss,  1, nullptr},
	/* kLossLength  */ {"Burst Length",   "Length",  "slices", Kind::Int,  Curve::Linear,  1.0,   64.0,   4.0,   nullptr,        0, kUnitSyncLoss,  0, nullptr},
	/* kLossIntensity*/{"Sync Intensity", "Intens",  "%",    Kind::Float,  Curve::Linear,  0.0,   100.0,  60.0,  nullptr,        0, kUnitSyncLoss,  1, nullptr},
	/* kLossStyle   */ {"Sync Style",     "Style",   "",     Kind::List,   Curve::Linear,  0.0,   3.0,    0.0,   kLossNames,     4, kUnitSyncLoss,  0, nullptr},
	/* kLossTrigger */ {"Sync Loss Now",  "Trig",    "",     Kind::Toggle, Curve::Linear,  0.0,   1.0,    0.0,   nullptr,        0, kUnitSyncLoss,  0, nullptr},

	/* kBandwidth   */ {"Bandwidth",      "BW",      "Hz",   Kind::Float,  Curve::Log,     200.0, 20000.0, 20000.0, nullptr,     0, kUnitOutput,    0, nullptr},
	/* kStatic      */ {"Static Level",   "Static",  "dB",   Kind::Float,  Curve::Linear, -60.0,  0.0,    -30.0, nullptr,        0, kUnitOutput,    1, nullptr},
	/* kDrive       */ {"Burn-In Drive",  "Drive",   "dB",   Kind::Float,  Curve::Linear,  0.0,   24.0,   0.0,   nullptr,        0, kUnitOutput,    1, nullptr},
	/* kFieldSplit  */ {"Field Split",    "Split",   "%",    Kind::Float,  Curve::Linear,  0.0,   100.0,  30.0,  nullptr,        0, kUnitOutput,    1, nullptr},
	/* kMix         */ {"Mix",            "Mix",     "%",    Kind::Float,  Curve::Linear,  0.0,   100.0,  100.0, nullptr,        0, kUnitOutput,    1, nullptr},
	/* kOutGain     */ {"Output",         "Out",     "dB",   Kind::Float,  Curve::Linear, -24.0,  12.0,   0.0,   nullptr,        0, kUnitOutput,    1, nullptr},
	/* kBypass      */ {"Bypass",         "Byp",     "",     Kind::Toggle, Curve::Linear,  0.0,   1.0,    0.0,   nullptr,        0, kUnitRoot,      0, nullptr},
	};
	// clang-format on
	return table[id < 0 || id >= kNumParams ? 0 : id];
}

//------------------------------------------------------------------------
// Normalized <-> plain conversion
//------------------------------------------------------------------------
inline double clamp01 (double v)
{
	return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
}

inline int stepCount (const ParamSpec& s)
{
	switch (s.kind)
	{
		case Kind::Toggle: return 1;
		case Kind::List: return s.listCount - 1;
		case Kind::Int: return static_cast<int> (std::lround (s.maxV - s.minV));
		default: return 0; // continuous
	}
}

inline double toPlain (const ParamSpec& s, double norm)
{
	norm = clamp01 (norm);
	double v;
	switch (s.curve)
	{
		case Curve::Log: v = s.minV * std::pow (s.maxV / s.minV, norm); break;
		case Curve::Squared: v = s.minV + (s.maxV - s.minV) * norm * norm; break;
		default: v = s.minV + (s.maxV - s.minV) * norm; break;
	}
	if (s.kind != Kind::Float)
		v = std::floor (v + 0.5);
	return std::max (s.minV, std::min (s.maxV, v));
}

inline double toNormalized (const ParamSpec& s, double plain)
{
	plain = std::max (s.minV, std::min (s.maxV, plain));
	double n;
	switch (s.curve)
	{
		case Curve::Log: n = std::log (plain / s.minV) / std::log (s.maxV / s.minV); break;
		case Curve::Squared:
			n = std::sqrt ((plain - s.minV) / (s.maxV - s.minV));
			break;
		default: n = (plain - s.minV) / (s.maxV - s.minV); break;
	}
	return clamp01 (n);
}

inline double defaultNormalized (int id)
{
	return toNormalized (spec (id), spec (id).defV);
}

// Human readable text for a plain value (ASCII).
inline std::string formatValue (const ParamSpec& s, double plain)
{
	char buf[96];
	if (s.kind == Kind::List)
	{
		int i = std::max (0, std::min (s.listCount - 1, static_cast<int> (std::lround (plain))));
		return s.list[i];
	}
	if (s.kind == Kind::Toggle)
		return plain >= 0.5 ? "On" : "Off";
	if (s.zeroText && plain <= s.minV + 1e-9)
		return s.zeroText;
	if (s.kind == Kind::Int)
		std::snprintf (buf, sizeof (buf), "%d", static_cast<int> (std::lround (plain)));
	else if (std::strcmp (s.units, "Hz") == 0 && plain >= 1000.0)
		std::snprintf (buf, sizeof (buf), "%.2f k", plain / 1000.0);
	else
		std::snprintf (buf, sizeof (buf), "%.*f", s.precision, plain);
	return buf;
}

// Parse text typed by the user into a plain value. Returns false on failure.
inline bool parseValue (const ParamSpec& s, const char* text, double& plain)
{
	if (!text)
		return false;
	if (s.kind == Kind::List)
	{
		for (int i = 0; i < s.listCount; ++i)
		{
			const char* a = s.list[i];
			const char* b = text;
			bool same = true;
			while (*a || *b)
			{
				if (std::tolower (static_cast<unsigned char> (*a)) !=
				    std::tolower (static_cast<unsigned char> (*b)))
				{
					same = false;
					break;
				}
				if (*a)
					++a;
				if (*b)
					++b;
			}
			if (same)
			{
				plain = i;
				return true;
			}
		}
	}
	if (s.kind == Kind::Toggle)
	{
		if (!std::strcmp (text, "On") || !std::strcmp (text, "on") || !std::strcmp (text, "1"))
		{
			plain = 1;
			return true;
		}
		if (!std::strcmp (text, "Off") || !std::strcmp (text, "off") || !std::strcmp (text, "0"))
		{
			plain = 0;
			return true;
		}
		return false;
	}
	if (s.zeroText && (!std::strcmp (text, s.zeroText) || !std::strcmp (text, "off")))
	{
		plain = s.minV;
		return true;
	}
	char* end = nullptr;
	double v = std::strtod (text, &end);
	if (end == text)
		return false;
	while (*end == ' ')
		++end;
	if (*end == 'k' || *end == 'K')
		v *= 1000.0;
	plain = std::max (s.minV, std::min (s.maxV, v));
	return true;
}

//------------------------------------------------------------------------
// A full set of plain (de-normalized) parameter values.
//------------------------------------------------------------------------
struct ParamSet
{
	double v[kNumParams];

	ParamSet () { setDefaults (); }

	void setDefaults ()
	{
		for (int i = 0; i < kNumParams; ++i)
			v[i] = spec (i).defV;
	}
	double& operator[] (int id) { return v[id]; }
	double operator[] (int id) const { return v[id]; }
};

} // namespace rd
