// Raster Damage - factory presets. Each preset lists only the parameters that
// differ from the defaults in RasterParams.h.
#pragma once

#include "RasterParams.h"

namespace rd {

struct PresetValue
{
	int id;
	double value; // plain value
};

struct Preset
{
	const char* name;
	PresetValue vals[24];
	int count;
};

#define RD_P(...) {__VA_ARGS__}

// clang-format off
static const Preset kPresets[] = {
	{"Init (Clean Signal)", RD_P ({kDamage, 0}, {kMix, 100}), 2},

	// ---- Broken video ----------------------------------------------------
	{"Dropped Frames", RD_P ({kSliceTime, 20}, {kDamage, 85}, {kTearAmount, 15}, {kTearDepth, 3},
		{kLinePeriod, 5}, {kLineMissing, 2}, {kLineMode, kLineBlack}, {kSmearAmount, 10}, {kLossRate, 0}), 9},
	{"Tracking Error VHS", RD_P ({kSliceTime, 9}, {kTearAmount, 80}, {kTearDepth, 14}, {kTearShape, kTearSkew},
		{kTearSlew, 55}, {kLineMissing, 0}, {kSmearAmount, 55}, {kSmearSize, 0.8}, {kBandwidth, 7500},
		{kStatic, -34}, {kLossRate, 18}, {kLossStyle, kLossRoll}, {kLossLength, 6}, {kLossIntensity, 55}), 14},
	{"Dead Pixel Row", RD_P ({kSliceTime, 3}, {kDamage, 60}, {kTearAmount, 0}, {kLinePeriod, 16},
		{kLineMissing, 1}, {kLineMode, kLineBlack}, {kLineEdge, 0.05}, {kSmearAmount, 0}, {kLossRate, 0}), 9},
	{"Scanline Comb", RD_P ({kSliceTime, 2}, {kDamage, 100}, {kTearAmount, 0}, {kLinePeriod, 2},
		{kLineMissing, 1}, {kLineMode, kLineBlack}, {kLineEdge, 0.02}, {kSmearAmount, 0}, {kLossRate, 0}), 9},
	{"Interlace Flutter", RD_P ({kSliceTime, 6}, {kInterlace, kInterlaceWeave}, {kDamage, 90}, {kTearAmount, 90},
		{kTearDepth, 5}, {kTearShape, kTearZigzag}, {kLineMissing, 0}, {kSmearAmount, 25}, {kLossRate, 0}), 9},
	{"CRT Warm-Up", RD_P ({kSliceTime, 14}, {kDamage, 45}, {kTearAmount, 30}, {kTearDepth, 2}, {kTearSlew, 90},
		{kLineMissing, 1}, {kLinePeriod, 3}, {kLineDepth, 35}, {kSmearAmount, 35}, {kBandwidth, 5200},
		{kDrive, 4}, {kStatic, -42}, {kLossRate, 0}), 13},
	{"Rolling Shutter", RD_P ({kSliceTime, 4}, {kDamage, 100}, {kTearAmount, 100}, {kTearDepth, 9},
		{kTearShape, kTearSkew}, {kTearSlew, 10}, {kLineMissing, 0}, {kSmearAmount, 0}, {kLossRate, 0}), 9},
	{"Vertical Hold Lost", RD_P ({kSliceTime, 40}, {kDamage, 100}, {kTearAmount, 20}, {kLineMissing, 0},
		{kSmearAmount, 10}, {kLossRate, 70}, {kLossLength, 12}, {kLossIntensity, 90}, {kLossStyle, kLossRoll}, {kStatic, -38}), 10},

	// ---- Corrupted media ---------------------------------------------------
	{"Corrupt MP3", RD_P ({kSliceTime, 26}, {kDamage, 75}, {kTearAmount, 25}, {kTearDepth, 6}, {kLinePeriod, 7},
		{kLineMissing, 2}, {kLineMode, kLineHold}, {kSmearAmount, 60}, {kSmearMode, kSmearCrunch},
		{kSmearSize, 1.6}, {kColorDepth, 9}, {kBandwidth, 11000}, {kLossRate, 0}), 13},
	{"Scratched CD", RD_P ({kSliceTime, 55}, {kDamage, 80}, {kTearAmount, 0}, {kLinePeriod, 9}, {kLineMissing, 1},
		{kLineMode, kLineHold}, {kLineChaos, 40}, {kSmearAmount, 0}, {kLossRate, 12}, {kLossStyle, kLossStutter},
		{kLossLength, 3}, {kLossIntensity, 75}, {kStatic, -52}), 13},
	{"Warped Cassette", RD_P ({kSliceTime, 90}, {kDamage, 70}, {kTearAmount, 100}, {kTearDepth, 12},
		{kTearShape, kTearSine}, {kTearSlew, 100}, {kLineMissing, 0}, {kSmearAmount, 20}, {kBandwidth, 6500},
		{kDrive, 5}, {kStatic, -40}, {kLossRate, 0}), 12},
	{"Eaten Tape", RD_P ({kSliceTime, 60}, {kDamage, 85}, {kTearAmount, 70}, {kTearDepth, 30}, {kTearShape, kTearRandom},
		{kTearSlew, 80}, {kGhost, 35}, {kLineMissing, 1}, {kLinePeriod, 6}, {kLineDepth, 70},
		{kBandwidth, 4800}, {kDrive, 6}, {kStatic, -32}, {kLossRate, 25}, {kLossStyle, kLossRoll}, {kLossLength, 5}), 16},
	{"Floppy Disk Read Error", RD_P ({kSliceTime, 35}, {kDamage, 80}, {kTearAmount, 0}, {kLinePeriod, 4},
		{kLineMissing, 1}, {kLineMode, kLineSnow}, {kSmearAmount, 70}, {kSmearMode, kSmearHold}, {kSmearSize, 2.2},
		{kColorDepth, 6}, {kStatic, -28}, {kLossRate, 20}, {kLossStyle, kLossStutter}, {kLossLength, 4}), 14},
	{"Dial-Up Handshake", RD_P ({kSliceTime, 7}, {kDamage, 90}, {kTearAmount, 60}, {kTearDepth, 2.5}, {kSmearAmount, 80},
		{kSmearMode, kSmearHold}, {kSmearSize, 0.35}, {kColorDepth, 5}, {kBandwidth, 3400}, {kStatic, -24},
		{kLineMissing, 0}, {kLossRate, 30}, {kLossStyle, kLossChaos}, {kLossLength, 8}), 14},
	{"Bad Sector", RD_P ({kSliceTime, 18}, {kDamage, 100}, {kFrameLoop, 16}, {kSeed, 404}, {kTearAmount, 40},
		{kTearDepth, 4}, {kLinePeriod, 8}, {kLineMissing, 3}, {kLineMode, kLineNegative}, {kLineDepth, 60},
		{kSmearAmount, 30}, {kLossRate, 0}), 12},

	// ---- Guitar ---------------------------------------------------------------
	{"Guitar: Fraying Amp", RD_P ({kSliceTime, 11}, {kDamage, 55}, {kTearAmount, 45}, {kTearDepth, 2.2},
		{kTearSlew, 60}, {kLinePeriod, 6}, {kLineMissing, 1}, {kLineDepth, 55}, {kLineEdge, 0.6},
		{kSmearAmount, 25}, {kDrive, 3}, {kStatic, -48}, {kLossRate, 5}), 13},
	{"Guitar: Shorting Cable", RD_P ({kSliceTime, 5}, {kDamage, 95}, {kTearAmount, 10}, {kLinePeriod, 11},
		{kLineMissing, 4}, {kLineChaos, 70}, {kLineMode, kLineSnow}, {kLineEdge, 0.05}, {kSmearAmount, 0},
		{kStatic, -26}, {kLossRate, 8}, {kLossStyle, kLossJitter}, {kLossLength, 3}), 13},
	{"Guitar: Tracking Chorus", RD_P ({kSliceTime, 22}, {kDamage, 60}, {kTearAmount, 100}, {kTearDepth, 7},
		{kTearShape, kTearSine}, {kTearSlew, 100}, {kGhost, 20}, {kLineMissing, 0}, {kSmearAmount, 15},
		{kBandwidth, 9500}, {kMix, 100}, {kLossRate, 0}), 12},
	{"Guitar: Glitch Chug Gate", RD_P ({kTempoSync, 1}, {kTempoDiv, 4}, {kDamage, 100}, {kTearAmount, 0},
		{kLinePeriod, 4}, {kLineMissing, 1}, {kLineEdge, 0.4}, {kSmearAmount, 0}, {kLossRate, 0}), 9},
	{"Guitar: Ghost Solo", RD_P ({kSliceTime, 30}, {kDamage, 50}, {kTearAmount, 70}, {kTearDepth, 18},
		{kTearShape, kTearStair}, {kTearSlew, 85}, {kGhost, 55}, {kLineMissing, 0}, {kSmearAmount, 30},
		{kSmearMode, kSmearBleed}, {kSmearSize, 3.5}, {kBandwidth, 8200}, {kMix, 100}, {kLossRate, 0}), 14},
	{"Guitar: Digital Rust", RD_P ({kSliceTime, 8}, {kDamage, 65}, {kTearAmount, 30}, {kTearDepth, 3},
		{kSmearAmount, 70}, {kSmearMode, kSmearCrunch}, {kSmearSize, 0.7}, {kColorDepth, 8}, {kLinePeriod, 8},
		{kLineMissing, 1}, {kLineDepth, 70}, {kBandwidth, 9000}, {kDrive, 5}, {kLossRate, 0}), 14},

	// ---- Vocals -----------------------------------------------------------------
	{"Vocal: Broadcast Interference", RD_P ({kSliceTime, 13}, {kDamage, 55}, {kTearAmount, 35}, {kTearDepth, 2.5},
		{kLinePeriod, 9}, {kLineMissing, 1}, {kLineDepth, 65}, {kSmearAmount, 25}, {kBandwidth, 6800},
		{kStatic, -40}, {kLossRate, 10}, {kLossStyle, kLossJitter}, {kLossLength, 3}, {kLossIntensity, 40}), 14},
	{"Vocal: Lost Transmission", RD_P ({kSliceTime, 45}, {kDamage, 70}, {kTearAmount, 20}, {kTearDepth, 5},
		{kLinePeriod, 3}, {kLineMissing, 1}, {kLineDepth, 85}, {kLineChaos, 30}, {kLineMode, kLineSnow},
		{kSmearAmount, 20}, {kBandwidth, 4200}, {kStatic, -30}, {kLossRate, 18}, {kLossStyle, kLossStutter},
		{kLossLength, 4}), 15},
	{"Vocal: Dead Satellite", RD_P ({kSliceTime, 70}, {kDamage, 75}, {kTearAmount, 60}, {kTearDepth, 22},
		{kTearSlew, 90}, {kGhost, 40}, {kLineMissing, 1}, {kLinePeriod, 5}, {kLineMode, kLinePrevious},
		{kSmearAmount, 40}, {kSmearMode, kSmearBleed}, {kSmearSize, 4}, {kBandwidth, 5000}, {kStatic, -36},
		{kMix, 100}, {kLossRate, 0}), 16},
	{"Vocal: Pixel Throat", RD_P ({kSliceTime, 10}, {kDamage, 70}, {kTearAmount, 20}, {kTearDepth, 1.5},
		{kSmearAmount, 90}, {kSmearMode, kSmearHold}, {kSmearSize, 0.28}, {kColorDepth, 7}, {kLineMissing, 0},
		{kBandwidth, 10000}, {kMix, 100}, {kLossRate, 0}), 12},
	{"Vocal: Haunted Answering Machine", RD_P ({kSliceTime, 50}, {kDamage, 65}, {kTearAmount, 80}, {kTearDepth, 28},
		{kTearShape, kTearSine}, {kTearSlew, 100}, {kGhost, 60}, {kLineMissing, 0}, {kSmearAmount, 30},
		{kBandwidth, 3600}, {kStatic, -34}, {kDrive, 3}, {kMix, 100}, {kLossRate, 8}, {kLossStyle, kLossRoll}), 15},

	// ---- Drums ------------------------------------------------------------------------
	{"Drums: Frame Skipper", RD_P ({kTempoSync, 1}, {kTempoDiv, 4}, {kDamage, 85}, {kTearAmount, 20},
		{kTearDepth, 5}, {kLinePeriod, 8}, {kLineMissing, 2}, {kLineMode, kLineHold}, {kSmearAmount, 0},
		{kLossRate, 0}), 10},
	{"Drums: Stutter Roll", RD_P ({kTempoSync, 1}, {kTempoDiv, 6}, {kDamage, 100}, {kTearAmount, 0},
		{kLineMissing, 0}, {kSmearAmount, 0}, {kLossRate, 35}, {kLossStyle, kLossStutter}, {kLossLength, 3},
		{kLossIntensity, 80}), 10},
	{"Drums: Sample-Hold Crush", RD_P ({kSliceTime, 4}, {kDamage, 90}, {kTearAmount, 15}, {kTearDepth, 1.5},
		{kSmearAmount, 100}, {kSmearMode, kSmearHold}, {kSmearSize, 0.18}, {kColorDepth, 6}, {kLineMissing, 0},
		{kBandwidth, 8000}, {kDrive, 4}, {kLossRate, 0}), 12},
	{"Drums: Tape Tear Break", RD_P ({kTempoSync, 1}, {kTempoDiv, 8}, {kDamage, 90}, {kTearAmount, 85},
		{kTearDepth, 24}, {kTearShape, kTearStair}, {kTearSlew, 30}, {kLineMissing, 1}, {kLinePeriod, 4},
		{kLineMode, kLinePrevious}, {kSmearAmount, 20}, {kLossRate, 12}, {kLossStyle, kLossRoll}, {kLossLength, 4}), 14},
	{"Drums: Dead Frame Gate", RD_P ({kTempoSync, 1}, {kTempoDiv, 2}, {kDamage, 100}, {kTearAmount, 0},
		{kLinePeriod, 4}, {kLineMissing, 2}, {kLineEdge, 0.2}, {kSmearAmount, 0}, {kLossRate, 0}), 9},
	{"Drums: Overdriven Scan", RD_P ({kSliceTime, 7}, {kDamage, 80}, {kTearAmount, 40}, {kTearDepth, 3},
		{kLinePeriod, 6}, {kLineMissing, 1}, {kLineMode, kLineNegative}, {kLineDepth, 50}, {kSmearAmount, 30},
		{kSmearMode, kSmearCrunch}, {kColorDepth, 10}, {kDrive, 9}, {kStatic, -44}, {kLossRate, 0}), 14},

	// ---- Sync loss / extremes ---------------------------------------------------------------
	{"Sync Loss: Light Jitter", RD_P ({kSliceTime, 15}, {kDamage, 60}, {kTearAmount, 15}, {kTearDepth, 2},
		{kLineMissing, 0}, {kSmearAmount, 0}, {kLossRate, 40}, {kLossStyle, kLossJitter}, {kLossLength, 6},
		{kLossIntensity, 30}), 10},
	{"Sync Loss: Roll Over", RD_P ({kSliceTime, 25}, {kDamage, 70}, {kTearAmount, 0}, {kLineMissing, 0},
		{kSmearAmount, 0}, {kLossRate, 55}, {kLossStyle, kLossRoll}, {kLossLength, 10}, {kLossIntensity, 70},
		{kStatic, -45}), 10},
	{"Sync Loss: Total Collapse", RD_P ({kSliceTime, 30}, {kDamage, 100}, {kTearAmount, 100}, {kTearDepth, 40},
		{kLineMissing, 3}, {kLinePeriod, 5}, {kLineChaos, 60}, {kLineMode, kLineSnow}, {kSmearAmount, 60},
		{kSmearMode, kSmearCrunch}, {kColorDepth, 6}, {kLossRate, 100}, {kLossStyle, kLossChaos},
		{kLossLength, 20}, {kLossIntensity, 100}, {kStatic, -20}, {kDrive, 6}), 17},
	{"Sync Loss: Heartbeat", RD_P ({kTempoSync, 1}, {kTempoDiv, 6}, {kDamage, 100}, {kTearAmount, 0},
		{kLineMissing, 0}, {kSmearAmount, 0}, {kLossRate, 20}, {kLossStyle, kLossStutter}, {kLossLength, 2},
		{kLossIntensity, 45}, {kFrameLoop, 8}, {kSeed, 7}), 12},
	{"Sync Loss: Beat-Locked Roll", RD_P ({kTempoSync, 1}, {kTempoDiv, 4}, {kDamage, 100}, {kTearAmount, 0},
		{kLineMissing, 0}, {kSmearAmount, 0}, {kLossRate, 30}, {kLossStyle, kLossRoll}, {kLossLength, 8},
		{kLossIntensity, 60}, {kFrameLoop, 16}, {kSeed, 21}), 12},

	// ---- Textures -------------------------------------------------------------------------------
	{"Input Follow: Hit Triggered", RD_P ({kSliceTime, 10}, {kInputFollow, 100}, {kDamage, 100}, {kTearAmount, 70},
		{kTearDepth, 12}, {kLinePeriod, 4}, {kLineMissing, 1}, {kSmearAmount, 40}, {kLossRate, 20},
		{kLossStyle, kLossJitter}), 10},
	{"Input Follow: Quiet Corruption", RD_P ({kSliceTime, 16}, {kInputFollow, -100}, {kDamage, 100}, {kTearAmount, 60},
		{kTearDepth, 9}, {kLinePeriod, 3}, {kLineMissing, 1}, {kSmearAmount, 50}, {kStatic, -36},
		{kLossRate, 15}), 10},
	{"Frozen Frame Loop", RD_P ({kSliceTime, 20}, {kFrameLoop, 8}, {kSeed, 99}, {kDamage, 100}, {kTearAmount, 70},
		{kTearDepth, 10}, {kLinePeriod, 8}, {kLineMissing, 2}, {kSmearAmount, 35}, {kLossRate, 0}), 10},
	{"Static Bath", RD_P ({kSliceTime, 8}, {kDamage, 100}, {kTearAmount, 0}, {kLinePeriod, 2}, {kLineMissing, 1},
		{kLineMode, kLineSnow}, {kLineDepth, 70}, {kLineEdge, 0.02}, {kSmearAmount, 0}, {kStatic, -18},
		{kMix, 100}, {kLossRate, 0}), 12},
	{"Heat Haze", RD_P ({kSliceTime, 6}, {kDamage, 100}, {kTearAmount, 100}, {kTearDepth, 1.2}, {kTearShape, kTearSine},
		{kTearSlew, 100}, {kLineMissing, 0}, {kSmearAmount, 0}, {kMix, 100}, {kLossRate, 0}), 10},
	{"Datamosh Smear", RD_P ({kSliceTime, 35}, {kDamage, 85}, {kTearAmount, 50}, {kTearDepth, 10},
		{kSmearAmount, 100}, {kSmearMode, kSmearBleed}, {kSmearSize, 6}, {kLineMissing, 1}, {kLinePeriod, 3},
		{kLineMode, kLinePrevious}, {kGhost, 30}, {kLossRate, 20}, {kLossStyle, kLossRoll}, {kLossLength, 4}), 14},
	{"Glass Shatter Fragments", RD_P ({kSliceTime, 3}, {kSliceJitter, 90}, {kDamage, 100}, {kTearAmount, 100},
		{kTearDepth, 18}, {kLinePeriod, 3}, {kLineMissing, 1}, {kLineChaos, 100}, {kLineMode, kLineNegative},
		{kSmearAmount, 20}, {kLossRate, 0}), 11},
	{"Subtle Wear", RD_P ({kSliceTime, 12}, {kDamage, 30}, {kTearAmount, 40}, {kTearDepth, 1.2}, {kLinePeriod, 16},
		{kLineMissing, 1}, {kLineDepth, 40}, {kSmearAmount, 20}, {kStatic, -54}, {kBandwidth, 15000},
		{kLossRate, 0}), 11},
	{"Mono Sum Murder", RD_P ({kSliceTime, 15}, {kDamage, 100}, {kFieldSplit, 100}, {kTearAmount, 90}, {kTearDepth, 15},
		{kInterlace, kInterlaceWeave}, {kLinePeriod, 5}, {kLineMissing, 2}, {kSmearAmount, 40}, {kLossRate, 25},
		{kLossStyle, kLossChaos}), 11},
};
// clang-format on

#undef RD_P

static const int kNumPresets = static_cast<int> (sizeof (kPresets) / sizeof (kPresets[0]));

static const struct { const char* name; int first; } kPresetCategories[] = {
	{"Basics", 0},
	{"Broken video", 1},
	{"Corrupted media", 9},
	{"Guitar", 16},
	{"Vocals", 22},
	{"Drums", 27},
	{"Sync loss / extremes", 33},
	{"Textures", 38},
};
static const int kNumPresetCategories = static_cast<int> (sizeof (kPresetCategories) / sizeof (kPresetCategories[0]));

// Category index of a factory preset.
inline int presetCategory (int presetIndex)
{
	int c = 0;
	for (int i = 0; i < kNumPresetCategories; ++i)
		if (presetIndex >= kPresetCategories[i].first)
			c = i;
	return c;
}

inline void loadPreset (int index, ParamSet& out)
{
	out.setDefaults ();
	if (index < 0 || index >= kNumPresets)
		return;
	const Preset& p = kPresets[index];
	for (int i = 0; i < p.count; ++i)
	{
		const ParamSpec& s = spec (p.vals[i].id);
		out[p.vals[i].id] = std::max (s.minV, std::min (s.maxV, p.vals[i].value));
	}
}

} // namespace rd
