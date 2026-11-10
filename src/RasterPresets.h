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

	// ---- Scenes ----------------------------------------------------
	{"VHS Rental Tape", RD_P ({kTearAmount, 70}, {kSmearAmount, 45}, {kLineMissing, 1}, {kLossRate, 6}, {kSliceTime, 14}, {kDamage, 70}, {kTearDepth, 2.5}, {kTearShape, kTearSine}, {kTearSlew, 85}, {kGhost, 22}, {kLinePeriod, 16}, {kLineDepth, 35}, {kSmearMode, kSmearBleed}, {kSmearSize, 1.2}, {kLossLength, 3}, {kLossIntensity, 40}, {kLossStyle, kLossJitter}, {kBandwidth, 6500}, {kStatic, -40}), 19},
	{"Cassette Warble Dream", RD_P ({kTearAmount, 100}, {kSmearAmount, 35}, {kLineMissing, 0}, {kLossRate, 0}, {kSliceTime, 90}, {kDamage, 75}, {kSliceJitter, 20}, {kTearDepth, 11}, {kTearShape, kTearSine}, {kTearSlew, 95}, {kGhost, 38}, {kSmearMode, kSmearBleed}, {kSmearSize, 3}, {kBandwidth, 5200}, {kDrive, 3}, {kStatic, -46}, {kOutGain, -1}), 17},
	{"Security Camera Loop", RD_P ({kTearAmount, 35}, {kSmearAmount, 20}, {kLineMissing, 1}, {kLossRate, 30}, {kSliceTime, 40}, {kDamage, 85}, {kFrameLoop, 8}, {kTearDepth, 4}, {kTearShape, kTearStair}, {kLinePeriod, 8}, {kLineMode, kLineHold}, {kSmearMode, kSmearHold}, {kSmearSize, 0.6}, {kLossLength, 4}, {kLossIntensity, 55}, {kLossStyle, kLossStutter}, {kBandwidth, 4500}, {kStatic, -38}, {kColorDepth, 12}), 19},
	{"Satellite Link Failing", RD_P ({kTearAmount, 40}, {kSmearAmount, 35}, {kLineMissing, 1}, {kLossRate, 38}, {kSliceTime, 28}, {kDamage, 80}, {kTearDepth, 7}, {kLinePeriod, 6}, {kLineMode, kLineSnow}, {kLineDepth, 70}, {kSmearMode, kSmearCrunch}, {kSmearSize, 0.5}, {kColorDepth, 10}, {kLossLength, 10}, {kLossIntensity, 70}, {kLossStyle, kLossRoll}, {kBandwidth, 3600}, {kStatic, -26}), 18},
	{"Haunted Broadcast", RD_P ({kTearAmount, 60}, {kSmearAmount, 30}, {kLineMissing, 1}, {kLossRate, 12}, {kSliceTime, 120}, {kDamage, 70}, {kSliceJitter, 30}, {kTearDepth, 22}, {kTearShape, kTearSine}, {kTearSlew, 70}, {kGhost, 62}, {kLineChaos, 60}, {kLinePeriod, 7}, {kLineMode, kLineSnow}, {kLineDepth, 45}, {kSmearMode, kSmearBleed}, {kSmearSize, 2.5}, {kLossLength, 6}, {kLossIntensity, 50}, {kLossStyle, kLossJitter}, {kBandwidth, 5200}, {kStatic, -34}, {kFieldSplit, 60}), 23},
	{"Datamosh Frames", RD_P ({kTearAmount, 75}, {kSmearAmount, 60}, {kLineMissing, 1}, {kLossRate, 0}, {kSliceTime, 33}, {kDamage, 90}, {kFrameLoop, 12}, {kTearDepth, 40}, {kTearShape, kTearStair}, {kTearSlew, 5}, {kLinePeriod, 5}, {kLineMode, kLinePrevious}, {kSmearMode, kSmearHold}, {kSmearSize, 1.6}, {kDrive, 2}), 15},
	{"Blue Screen Stutter", RD_P ({kTearAmount, 30}, {kSmearAmount, 25}, {kLineMissing, 2}, {kLossRate, 62}, {kSliceTime, 22}, {kDamage, 100}, {kTearDepth, 5}, {kLinePeriod, 12}, {kLineMode, kLineHold}, {kSmearMode, kSmearHold}, {kSmearSize, 0.8}, {kLossLength, 28}, {kLossIntensity, 90}, {kLossStyle, kLossStutter}, {kBandwidth, 9000}, {kStatic, -40}), 16},
	{"Fax Handshake", RD_P ({kTearAmount, 25}, {kSmearAmount, 100}, {kLineMissing, 1}, {kLossRate, 0}, {kSliceTime, 9}, {kDamage, 90}, {kTearDepth, 1.5}, {kTearShape, kTearZigzag}, {kLinePeriod, 3}, {kLineMode, kLineSnow}, {kLineDepth, 60}, {kSmearMode, kSmearCrunch}, {kSmearSize, 1.1}, {kColorDepth, 5}, {kBandwidth, 3000}, {kStatic, -30}, {kDrive, 8}, {kOutGain, -3}), 18},
	{"Dying Hard Drive", RD_P ({kTearAmount, 80}, {kSmearAmount, 15}, {kLineMissing, 3}, {kLossRate, 25}, {kSliceTime, 55}, {kDamage, 85}, {kSliceJitter, 45}, {kTearDepth, 18}, {kTearShape, kTearSkew}, {kTearSlew, 40}, {kLinePeriod, 9}, {kLineMode, kLineBlack}, {kLineEdge, 2.5}, {kSmearMode, kSmearAverage}, {kSmearSize, 1.8}, {kLossLength, 14}, {kLossIntensity, 75}, {kLossStyle, kLossRoll}, {kBandwidth, 7000}, {kStatic, -44}), 20},
	{"Cathedral of Static", RD_P ({kTearAmount, 40}, {kSmearAmount, 70}, {kLineMissing, 1}, {kLossRate, 0}, {kSliceTime, 180}, {kDamage, 65}, {kSliceJitter, 35}, {kTearDepth, 30}, {kTearShape, kTearSine}, {kTearSlew, 90}, {kGhost, 70}, {kSmearMode, kSmearAverage}, {kSmearSize, 9}, {kLinePeriod, 4}, {kLineMode, kLineSnow}, {kLineDepth, 30}, {kBandwidth, 8000}, {kStatic, -28}, {kFieldSplit, 100}, {kOutGain, -2}), 20},

	// ---- Rhythm (tempo sync) ----------------------------------------------------
	{"Sixteenth Stutter Grid", RD_P ({kTearAmount, 60}, {kSmearAmount, 0}, {kLineMissing, 2}, {kLossRate, 0}, {kTempoSync, 1}, {kTempoDiv, 4}, {kDamage, 100}, {kFrameLoop, 4}, {kTearDepth, 18}, {kTearShape, kTearStair}, {kTearSlew, 0}, {kLinePeriod, 4}, {kLineMode, kLineBlack}, {kLineEdge, 0.2}), 14},
	{"Triplet Gate Tear", RD_P ({kTearAmount, 45}, {kSmearAmount, 0}, {kLineMissing, 1}, {kLossRate, 0}, {kTempoSync, 1}, {kTempoDiv, 5}, {kDamage, 95}, {kTearDepth, 6}, {kTearShape, kTearZigzag}, {kTearSlew, 10}, {kLinePeriod, 3}, {kLineMode, kLineBlack}, {kLineEdge, 0.1}, {kLineDepth, 100}), 14},
	{"Half-Time Dropout", RD_P ({kTearAmount, 0}, {kSmearAmount, 0}, {kLineMissing, 3}, {kLossRate, 0}, {kTempoSync, 1}, {kTempoDiv, 8}, {kDamage, 100}, {kLinePeriod, 8}, {kLineMode, kLineBlack}, {kLineEdge, 4}, {kLineDepth, 100}), 11},
	{"Breakbeat Shredder", RD_P ({kTearAmount, 85}, {kSmearAmount, 20}, {kLineMissing, 1}, {kLossRate, 40}, {kTempoSync, 1}, {kTempoDiv, 4}, {kDamage, 100}, {kFrameLoop, 16}, {kTearDepth, 30}, {kTearShape, kTearZigzag}, {kTearSlew, 15}, {kLinePeriod, 6}, {kLineMode, kLinePrevious}, {kSmearMode, kSmearHold}, {kSmearSize, 0.9}, {kLossLength, 4}, {kLossIntensity, 70}, {kLossStyle, kLossStutter}, {kDrive, 3}), 19},
	{"Rolling Sync Pulse", RD_P ({kTearAmount, 30}, {kSmearAmount, 0}, {kLineMissing, 0}, {kLossRate, 55}, {kTempoSync, 1}, {kTempoDiv, 6}, {kDamage, 90}, {kTearDepth, 8}, {kTearShape, kTearSine}, {kTearSlew, 60}, {kLossLength, 2}, {kLossIntensity, 80}, {kLossStyle, kLossRoll}, {kGhost, 25}), 14},
	{"Gated Ghosts", RD_P ({kTearAmount, 40}, {kSmearAmount, 0}, {kLineMissing, 3}, {kLossRate, 0}, {kTempoSync, 1}, {kTempoDiv, 4}, {kDamage, 100}, {kTearDepth, 12}, {kTearShape, kTearSine}, {kTearSlew, 70}, {kGhost, 72}, {kLinePeriod, 4}, {kLinePhase, 1}, {kLineMode, kLineBlack}, {kLineEdge, 1.5}, {kBandwidth, 11000}), 16},
	{"Static Between Notes", RD_P ({kTearAmount, 50}, {kSmearAmount, 30}, {kLineMissing, 1}, {kLossRate, 45}, {kSliceTime, 18}, {kDamage, 100}, {kInputFollow, -85}, {kTearDepth, 4}, {kLinePeriod, 3}, {kLineMode, kLineSnow}, {kLineDepth, 100}, {kSmearMode, kSmearAverage}, {kSmearSize, 0.5}, {kLossLength, 5}, {kLossIntensity, 65}, {kLossStyle, kLossChaos}, {kStatic, -22}, {kBandwidth, 9000}), 18},

	// ---- Instruments ----------------------------------------------------
	{"Guitar: Lo-Fi Ambient", RD_P ({kTearAmount, 60}, {kSmearAmount, 60}, {kLineMissing, 0}, {kLossRate, 0}, {kSliceTime, 100}, {kDamage, 70}, {kSliceJitter, 25}, {kTearDepth, 9}, {kTearShape, kTearSine}, {kTearSlew, 95}, {kGhost, 45}, {kSmearMode, kSmearBleed}, {kSmearSize, 4}, {kBandwidth, 6000}, {kStatic, -48}, {kFieldSplit, 70}), 16},
	{"Guitar: Djent Digital Chop", RD_P ({kTearAmount, 0}, {kSmearAmount, 0}, {kLineMissing, 1}, {kLossRate, 0}, {kTempoSync, 1}, {kTempoDiv, 4}, {kDamage, 100}, {kLinePeriod, 4}, {kLineMode, kLineBlack}, {kLineEdge, 0.05}, {kLineDepth, 100}, {kDrive, 2}), 12},
	{"Guitar: Broken Pedal", RD_P ({kTearAmount, 35}, {kSmearAmount, 40}, {kLineMissing, 1}, {kLossRate, 18}, {kSliceTime, 16}, {kDamage, 80}, {kTearDepth, 3}, {kTearShape, kTearZigzag}, {kLinePeriod, 7}, {kLineMode, kLineBlack}, {kLineDepth, 85}, {kLineEdge, 0.6}, {kSmearMode, kSmearCrunch}, {kSmearSize, 0.4}, {kColorDepth, 9}, {kLossLength, 3}, {kLossIntensity, 60}, {kLossStyle, kLossJitter}, {kDrive, 6}, {kBandwidth, 7500}, {kOutGain, -3}), 21},
	{"Vocal: Radio Dispatch", RD_P ({kTearAmount, 20}, {kSmearAmount, 45}, {kLineMissing, 1}, {kLossRate, 10}, {kSliceTime, 24}, {kDamage, 75}, {kTearDepth, 2}, {kLinePeriod, 10}, {kLineMode, kLineSnow}, {kLineDepth, 55}, {kSmearMode, kSmearCrunch}, {kSmearSize, 0.4}, {kColorDepth, 9}, {kLossLength, 3}, {kLossIntensity, 45}, {kLossStyle, kLossJitter}, {kBandwidth, 3200}, {kStatic, -34}, {kDrive, 4}, {kOutGain, -3}), 20},
	{"Vocal: Glitch Choir", RD_P ({kTearAmount, 90}, {kSmearAmount, 40}, {kLineMissing, 0}, {kLossRate, 0}, {kSliceTime, 70}, {kDamage, 85}, {kSliceJitter, 30}, {kTearDepth, 18}, {kTearShape, kTearSine}, {kTearSlew, 65}, {kGhost, 50}, {kInterlace, kInterlaceWeave}, {kSmearMode, kSmearBleed}, {kSmearSize, 2}, {kFieldSplit, 100}, {kBandwidth, 14000}), 16},
	{"Vocal: Whispering Datamosh", RD_P ({kTearAmount, 55}, {kSmearAmount, 70}, {kLineMissing, 1}, {kLossRate, 14}, {kSliceTime, 38}, {kDamage, 80}, {kInputFollow, 55}, {kTearDepth, 14}, {kTearShape, kTearStair}, {kTearSlew, 8}, {kSmearMode, kSmearHold}, {kSmearSize, 0.9}, {kLinePeriod, 6}, {kLineMode, kLinePrevious}, {kLineDepth, 60}, {kLossLength, 5}, {kLossIntensity, 60}, {kLossStyle, kLossJitter}, {kBandwidth, 12000}, {kStatic, -40}), 20},
	{"Drums: Snare Shatter", RD_P ({kTearAmount, 80}, {kSmearAmount, 30}, {kLineMissing, 2}, {kLossRate, 22}, {kSliceTime, 12}, {kDamage, 100}, {kInputFollow, 70}, {kTearDepth, 10}, {kTearShape, kTearZigzag}, {kTearSlew, 5}, {kLinePeriod, 5}, {kLineMode, kLineSnow}, {kLineDepth, 80}, {kSmearMode, kSmearCrunch}, {kSmearSize, 0.4}, {kColorDepth, 8}, {kLossLength, 3}, {kLossIntensity, 70}, {kLossStyle, kLossJitter}, {kDrive, 3}), 20},
	{"Drums: Kick Brick", RD_P ({kTearAmount, 35}, {kSmearAmount, 75}, {kLineMissing, 1}, {kLossRate, 0}, {kSliceTime, 30}, {kDamage, 85}, {kTearDepth, 5}, {kTearShape, kTearSkew}, {kLinePeriod, 6}, {kLineMode, kLineBlack}, {kLineEdge, 1}, {kSmearMode, kSmearCrunch}, {kSmearSize, 0.3}, {kColorDepth, 6}, {kBandwidth, 2600}, {kDrive, 10}, {kOutGain, -6}), 17},
	{"Drums: Hat Static Rain", RD_P ({kTearAmount, 15}, {kSmearAmount, 20}, {kLineMissing, 1}, {kLossRate, 0}, {kSliceTime, 7}, {kDamage, 100}, {kTearDepth, 1}, {kLineChaos, 85}, {kLinePeriod, 3}, {kLineMode, kLineSnow}, {kLineDepth, 70}, {kSmearMode, kSmearAverage}, {kSmearSize, 0.2}, {kBandwidth, 18000}, {kStatic, -24}), 15},
	{"Bass: Sub Tear", RD_P ({kTearAmount, 70}, {kSmearAmount, 25}, {kLineMissing, 0}, {kLossRate, 0}, {kSliceTime, 48}, {kDamage, 75}, {kTearDepth, 20}, {kTearShape, kTearSine}, {kTearSlew, 90}, {kSmearMode, kSmearAverage}, {kSmearSize, 3}, {kBandwidth, 1400}, {kDrive, 6}, {kOutGain, -3}), 14},
	{"Synth: Pad Disintegration", RD_P ({kOutGain, -2}, {kTearAmount, 55}, {kSmearAmount, 65}, {kLineMissing, 1}, {kLossRate, 9}, {kSliceTime, 150}, {kDamage, 75}, {kSliceJitter, 40}, {kTearDepth, 24}, {kTearShape, kTearSine}, {kTearSlew, 85}, {kGhost, 60}, {kSmearMode, kSmearAverage}, {kSmearSize, 6}, {kLinePeriod, 5}, {kLineMode, kLineSnow}, {kLineDepth, 25}, {kLossLength, 20}, {kLossIntensity, 55}, {kLossStyle, kLossRoll}, {kStatic, -36}, {kFieldSplit, 80}), 21},
	{"Mix Bus: Worn Glue", RD_P ({kTearAmount, 25}, {kSmearAmount, 15}, {kLineMissing, 1}, {kLossRate, 0}, {kSliceTime, 16}, {kDamage, 25}, {kTearDepth, 0.8}, {kTearSlew, 60}, {kLinePeriod, 16}, {kLineDepth, 30}, {kSmearMode, kSmearAverage}, {kSmearSize, 0.3}, {kBandwidth, 16000}, {kStatic, -52}), 14},
	{"Mix Bus: Tape Eater", RD_P ({kTearAmount, 70}, {kSmearAmount, 30}, {kLineMissing, 2}, {kLossRate, 8}, {kSliceTime, 60}, {kDamage, 55}, {kSliceJitter, 20}, {kTearDepth, 14}, {kTearShape, kTearSkew}, {kTearSlew, 80}, {kGhost, 18}, {kLinePeriod, 9}, {kLineMode, kLineBlack}, {kLineEdge, 3}, {kLineDepth, 60}, {kSmearMode, kSmearBleed}, {kSmearSize, 2}, {kLossLength, 8}, {kLossIntensity, 60}, {kLossStyle, kLossRoll}, {kBandwidth, 9000}, {kStatic, -42}), 22},
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
	{"Scenes", 47},
	{"Rhythm (tempo sync)", 57},
	{"Instruments", 64},
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
