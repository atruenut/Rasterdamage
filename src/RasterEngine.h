// Raster Damage - DSP engine (header only, no plugin-SDK dependency).
//
// The signal is cut into short "frames" (slices). Every frame makes its own,
// *stateless* damage decisions from a hash of (seed, frame number, channel
// stream, purpose). Because nothing depends on history, the result is
//   - identical for any host block size,
//   - perfectly repeatable (same seed + same position = same corruption),
//   - loop-able (Frame Loop repeats the damage pattern every N frames),
//   - lock-able to the host's musical grid (Tempo Sync).
//
// Per channel signal flow:
//
//   in -> [tape delay line, ghost feedback]
//      -> TEAR      read tap with per-frame delay offsets (and sync-loss displacement)
//      -> SMEAR     average / sample-hold / bleed / crunch  (+ colour depth)
//      -> SCANLINES periodic dropouts (black / snow / previous line / negative / hold)
//      -> snow burst during sync loss
//      -> BANDWIDTH low-pass -> DRIVE -> mix with (latency-matched) dry -> out gain
#pragma once

#include "RasterParams.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace rd {

//------------------------------------------------------------------------
// Stateless random numbers
//------------------------------------------------------------------------
inline uint64_t mix64 (uint64_t z)
{
	z += 0x9E3779B97F4A7C15ULL;
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
	return z ^ (z >> 31);
}

// Uniform [0,1) from (seed, frame, stream, purpose).
inline double hash01 (uint32_t seed, int64_t frame, uint32_t stream, uint32_t purpose)
{
	uint64_t h = mix64 (static_cast<uint64_t> (seed) + 0x1234567ULL);
	h = mix64 (h ^ static_cast<uint64_t> (frame));
	h = mix64 (h ^ ((static_cast<uint64_t> (stream) << 32) | purpose));
	return static_cast<double> (h >> 11) * (1.0 / 9007199254740992.0);
}

inline int64_t floorMod (int64_t a, int64_t b)
{
	int64_t m = a % b;
	return m < 0 ? m + b : m;
}

inline double dbToLin (double db)
{
	return std::pow (10.0, db / 20.0);
}

inline int nextPow2 (int v)
{
	int p = 1;
	while (p < v)
		p <<= 1;
	return p;
}

//------------------------------------------------------------------------
class Engine
{
public:
	struct FrameCtl
	{
		double tearStart = 0.0, tearEnd = 0.0; // delay (samples) at start/end of frame
		double slewCoef = 1.0;                 // per-sample one-pole coefficient for d
		int lossStyle = -1;                    // -1: none, else LossStyle (Jitter/Roll/Stutter)
		int lossIdx = 0;                       // frame index inside burst
		int lossLen = 1;                       // burst length (frames)
		double jit0 = 0.0, jit1 = 0.0;         // jitter displacement (samples)
		double rollStart = 0.0, rollEnd = 0.0; // roll displacement (samples)
		int stutDiv = 1;
		double stutBase = 0.0;
		double noiseGain = 0.0;
		bool dropped = false;
		bool smear = false;
	};


	static constexpr int kMaxChannels = 2;
	static constexpr int kBaseDelay = 2; // samples; Hermite needs >= 2, dry path matches it

	int latencySamples () const { return kBaseDelay; }

	//--------------------------------------------------------------------
	void prepare (double sampleRate, int numChannels)
	{
		sr = std::max (8000.0, sampleRate);
		nch = std::max (1, std::min (kMaxChannels, numChannels));

		maxDelay = static_cast<int> (6.0 * sr);
		bufSize = nextPow2 (maxDelay + 16);
		bufMask = bufSize - 1;
		smearCap = nextPow2 (static_cast<int> (0.021 * sr) + 8);
		smearMask = smearCap - 1;

		for (int c = 0; c < kMaxChannels; ++c)
		{
			ch[c].tape.assign (static_cast<size_t> (bufSize), 0.f);
			ch[c].smearRing.assign (static_cast<size_t> (smearCap), 0.f);
		}
		reset ();
		setParams (params);
	}

	void reset ()
	{
		for (int c = 0; c < kMaxChannels; ++c)
		{
			Chan& k = ch[c];
			std::fill (k.tape.begin (), k.tape.end (), 0.f);
			std::fill (k.smearRing.begin (), k.smearRing.end (), 0.f);
			k.wp = 0;
			k.d = 0.0;
			k.lastWet = 0.f;
			k.x1 = k.x2 = 0.f;
			k.smearSum = 0.0;
			k.smearWin = 1;
			k.smearHead = 0;
			k.holdCounter = 0;
			k.holdVal = 0.f;
			k.bleed = 0.f;
			k.smearAmt = 0.f;
			k.dropAmt = 0.f;
			k.dropHeld = 0.f;
			k.latchPending = false;
			k.rng = 0x1234567u + 7919u * static_cast<uint32_t> (c + 1);
			k.z1 = k.z2 = 0.f;
			k.fc = FrameCtl ();
		}
		env = 0.0;
		frameIndex = 0;
		framePos = 0.0;
		pendingTrigger = false;
		manualStart = -1000000;
		mixSm = bypass ? 0.0 : mixT;
		outSm = bypass ? 1.0 : outT;
		driveSm = driveT;
		curLen = 1.0;
		started = false;
	}

	//--------------------------------------------------------------------
	void setParams (const ParamSet& p)
	{
		params = p;
		seed = static_cast<uint32_t> (std::lround (p[kSeed]));
		damage = p[kDamage] * 0.01;
		follow = p[kInputFollow] * 0.01;
		interlace = static_cast<int> (std::lround (p[kInterlace]));
		loop = static_cast<int> (std::lround (p[kFrameLoop]));
		jitter = p[kSliceJitter] * 0.01;
		split = p[kFieldSplit] * 0.01;

		tearAmt = p[kTearAmount] * 0.01;
		tearDepthS = p[kTearDepth] * 0.001 * sr;
		tearShape = static_cast<int> (std::lround (p[kTearShape]));
		tearSlew = p[kTearSlew] * 0.01;
		ghost = p[kGhost] * 0.01;

		linePeriod = std::max (2, static_cast<int> (std::lround (p[kLinePeriod])));
		lineMissing = std::max (0, std::min (linePeriod, static_cast<int> (std::lround (p[kLineMissing]))));
		linePhase = static_cast<int> (std::lround (p[kLinePhase]));
		lineChaos = p[kLineChaos] * 0.01;
		lineMode = static_cast<int> (std::lround (p[kLineMode]));
		lineDepth = p[kLineDepth] * 0.01;
		lineEdgeS = std::max (0.0, p[kLineEdge] * 0.001 * sr);

		smearProb = p[kSmearAmount] * 0.01;
		smearMode = static_cast<int> (std::lround (p[kSmearMode]));
		smearWinF = std::max (1.0, p[kSmearSize] * 0.001 * sr);
		colorBits = static_cast<int> (std::lround (p[kColorDepth]));

		lossRate = p[kLossRate] * 0.01;
		lossLen = std::max (1, static_cast<int> (std::lround (p[kLossLength])));
		lossInt = p[kLossIntensity] * 0.01;
		lossStyle = static_cast<int> (std::lround (p[kLossStyle]));

		staticLin = p[kStatic] <= -59.9 ? 0.0 : dbToLin (p[kStatic]);
		bandwidth = p[kBandwidth];
		mixT = p[kMix] * 0.01;
		outT = dbToLin (p[kOutGain]);
		driveT = dbToLin (p[kDrive]);
		syncOn = p[kTempoSync] >= 0.5;
		divBeats = divisionBeats (static_cast<int> (std::lround (p[kTempoDiv])));
		sliceMs = p[kSliceTime];

		updateFilter ();

		// Before the first block, jump straight to the target values (no load-time ramp).
		if (!started)
		{
			mixSm = bypass ? 0.0 : mixT;
			outSm = bypass ? 1.0 : outT;
			driveSm = driveT;
		}
	}

	void setBypass (bool b)
	{
		bypass = b;
		if (!started)
			mixSm = bypass ? 0.0 : mixT;
	}
	void triggerSync () { pendingTrigger = true; }

	// Host transport, call once per block *before* process().
	// ppq = musical position (quarter notes) of the first sample of the block.
	void setTransport (bool playing, bool tempoValid, double tempo, bool ppqValid, double ppq)
	{
		hostPlaying = playing;
		hostTempo = tempoValid && tempo > 1.0 ? tempo : 0.0;
		hostPpq = ppq;
		hostPpqValid = ppqValid;
	}

	//--------------------------------------------------------------------
	// Process `n` samples. in/out may alias (in-place) and hold `numCh` channels.
	void process (const float* const* in, float* const* out, int n, int numCh)
	{
		const int channels = std::min (numCh, nch);
		if (!started)
		{
			frameIndex = 0;
			framePos = 0.0;
			beginFrame (nch);
			resyncToTransport (true);
			started = true;
		}
		else
			resyncToTransport (false);

		const double smooth = 1.0 - std::exp (-1.0 / (0.005 * sr)); // 5 ms
		const double envRel = std::exp (-1.0 / (0.12 * sr));
		const double tgtMix = bypass ? 0.0 : mixT;
		const double tgtOut = bypass ? 1.0 : outT;

		for (int i = 0; i < n; ++i)
		{
			// ---- frame clock (shared by all channels) ----------------------
			if (framePos >= curLen)
			{
				framePos -= curLen;
				++frameIndex;
				beginFrame (channels);
			}
			const double frac = framePos / curLen;

			mixSm += (tgtMix - mixSm) * smooth;
			outSm += (tgtOut - outSm) * smooth;
			driveSm += (driveT - driveSm) * smooth;

			// ---- envelope follower on dry input ----------------------------
			double peak = 0.0;
			for (int c = 0; c < channels; ++c)
				peak = std::max (peak, static_cast<double> (std::fabs (in[c][i])));
			env = std::max (peak, env * envRel);

			for (int c = 0; c < channels; ++c)
				out[c][i] = processSample (ch[c], c, in[c][i], frac);

			framePos += 1.0;
		}

		// keep the musical position moving when a host block is processed in several pieces
		if (hostPlaying && hostPpqValid && hostTempo > 0.0)
			hostPpq += static_cast<double> (n) / sr * hostTempo / 60.0;
	}

	// GUI / tools: what the engine decides for slice `fAbs` on channel `c`.
	// Mutates internal state, so use a separate Engine instance from the audio one.
	FrameCtl previewFrame (int64_t fAbs, int c, double level01 = 1.0)
	{
		curLen = frameLengthFor (fAbs);
		const int64_t f = loop > 0 ? floorMod (fAbs, loop) : fAbs;
		return makeFrame (std::max (0, std::min (c, kMaxChannels - 1)), fAbs, f, damageFor (level01));
	}
	double previewFrameLength () const { return curLen; }
	double tearDepthSamples () const { return tearDepthS; }

	// Introspection for tests / tools
	int64_t currentFrame () const { return frameIndex; }
	double currentFrameLength () const { return curLen; }

private:
	struct Chan
	{
		std::vector<float> tape;
		std::vector<float> smearRing;
		int wp = 0;
		double d = 0.0;
		float lastWet = 0.f;
		float x1 = 0.f, x2 = 0.f;
		double smearSum = 0.0;
		int smearWin = 1;
		int smearHead = 0;
		int holdCounter = 0;
		float holdVal = 0.f;
		float bleed = 0.f;
		float smearAmt = 0.f;
		float dropAmt = 0.f;
		float dropHeld = 0.f;
		bool latchPending = false;
		uint32_t rng = 1;
		float z1 = 0.f, z2 = 0.f; // low-pass state
		FrameCtl fc;
	};

	//--------------------------------------------------------------------
	double frameLengthFor (int64_t f) const
	{
		double len;
		if (syncOn && hostTempo > 0.0)
			len = divBeats * 60.0 / hostTempo * sr;
		else
		{
			len = sliceMs * 0.001 * sr;
			if (jitter > 0.0)
			{
				const int64_t fj = loop > 0 ? floorMod (f, loop) : f;
				const double u = hash01 (seed, fj, 0, 20);
				len *= 1.0 + jitter * (u * 2.0 - 1.0) * 0.8;
			}
		}
		return std::max (16.0, std::min (len, 5.0 * sr));
	}

	// Align frame counter to the host's musical grid when Tempo Sync is on.
	void resyncToTransport (bool force)
	{
		if (!(syncOn && hostPlaying && hostPpqValid && hostTempo > 0.0))
			return;
		const double lenSamples = divBeats * 60.0 / hostTempo * sr;
		if (lenSamples < 16.0)
			return;
		const double framePosExact = hostPpq / divBeats;
		const int64_t idx = static_cast<int64_t> (std::floor (framePosExact));
		const double wantPos = (framePosExact - static_cast<double> (idx)) * lenSamples;
		const double haveAbs = static_cast<double> (frameIndex) + framePos / curLen;
		const double wantAbs = static_cast<double> (idx) + wantPos / lenSamples;
		if (force || std::fabs (haveAbs - wantAbs) * lenSamples > 2.5)
		{
			frameIndex = idx;
			framePos = wantPos;
			beginFrame (nch);
		}
	}

	bool interlaceGate (int c, int64_t fAbs) const
	{
		const int parity = static_cast<int> (fAbs & 1);
		switch (interlace)
		{
			case kInterlaceEven: return parity == 0;
			case kInterlaceOdd: return parity == 1;
			case kInterlaceWeave: return nch < 2 ? parity == 0 : (c == 0 ? parity == 0 : parity == 1);
			default: return true;
		}
	}

	// Returns true when a sync-loss burst covers frame f. Fills idx / len / burst id.
	bool findBurst (uint32_t stream, int64_t f, double D, int& idx, int& len, int64_t& burstId) const
	{
		const double pStart = lossRate * D * 0.25;
		if (pStart <= 0.0)
			return false;
		const int maxLen = std::min (96, static_cast<int> (lossLen * 1.5 + 1.0));
		for (int k = 0; k < maxLen; ++k)
		{
			int64_t g = f - k;
			if (loop > 0)
				g = floorMod (g, loop);
			else if (g < 0)
				break;
			if (hash01 (seed, g, stream, 10) < pStart)
			{
				const int l = std::max (
				    1, std::min (96, static_cast<int> (std::lround (lossLen * (0.5 + hash01 (seed, g, stream, 11))))));
				if (k < l)
				{
					idx = k;
					len = l;
					burstId = g;
					return true;
				}
			}
		}
		return false;
	}

	void beginFrame (int channels)
	{
		curLen = frameLengthFor (frameIndex);

		// manual "Sync Loss Now" trigger starts at the frame boundary
		if (pendingTrigger)
		{
			manualStart = frameIndex;
			pendingTrigger = false;
		}

		// Input follow: scale the overall damage by the input level
		double D = damage;
		if (follow != 0.0)
		{
			const double db = 20.0 * std::log10 (env + 1e-9);
			D = damageFor (std::max (0.0, std::min (1.0, (db + 48.0) / 48.0)));
		}

		const int64_t fAbs = frameIndex;
		const int64_t f = loop > 0 ? floorMod (fAbs, loop) : fAbs;

		for (int c = 0; c < channels; ++c)
		{
			Chan& k = ch[c];
			k.fc = makeFrame (c, fAbs, f, D);
			if (k.fc.dropped && lineMode == kLineHold)
				k.latchPending = true;
		}
	}

	// All per-slice damage decisions for one channel. Pure function of the parameters
	// (+ frame number); shared by the audio path and the GUI preview.
	FrameCtl makeFrame (int c, int64_t fAbs, int64_t f, double D)
	{
		FrameCtl fc;

		const uint32_t st =
		    (c > 0 && hash01 (seed, f, static_cast<uint32_t> (c), 900) < split) ? static_cast<uint32_t> (c) : 0u;
		const bool gate = interlaceGate (c, fAbs);

		// ---------------- TEAR ----------------
		if (gate && hash01 (seed, f, st, 1) < tearAmt * D)
		{
			double s0 = 0.0, s1 = 0.0;
			switch (tearShape)
			{
				case kTearRandom: s0 = s1 = hash01 (seed, f, st, 2); break;
				case kTearStair: s0 = s1 = static_cast<double> (floorMod (f, 4)) / 3.0; break;
				case kTearSkew:
				{
					const double kf = static_cast<double> (floorMod (f, 8));
					s0 = kf / 8.0;
					s1 = (kf + 1.0) / 8.0;
					break;
				}
				case kTearSine:
				{
					const double kk = static_cast<double> (floorMod (f, 8));
					s0 = 0.5 * (1.0 + std::sin (2.0 * 3.14159265358979 * kk / 8.0));
					s1 = 0.5 * (1.0 + std::sin (2.0 * 3.14159265358979 * (kk + 1.0) / 8.0));
					break;
				}
				default: s0 = s1 = (f & 1) ? 1.0 : 0.0; break; // zigzag
			}
			fc.tearStart = s0 * tearDepthS;
			fc.tearEnd = s1 * tearDepthS;
		}

		// ---------------- SYNC LOSS ----------------
		int bIdx = 0, bLen = 1;
		int64_t bId = 0;
		bool inBurst = false;
		if (manualStart > -1000000 && fAbs - manualStart >= 0 && fAbs - manualStart < lossLen)
		{
			inBurst = true;
			bIdx = static_cast<int> (fAbs - manualStart);
			bLen = lossLen;
			bId = manualStart + 7777;
		}
		else if (gate || true)
			inBurst = findBurst (st, f, D, bIdx, bLen, bId);

		double slewTau = tearSlew * curLen * 0.5; // seconds*sr
		if (inBurst)
		{
			int style = lossStyle;
			if (style == kLossChaos)
				style = static_cast<int> (hash01 (seed, f, st, 13) * 3.0) % 3;
			fc.lossStyle = style;
			fc.lossIdx = bIdx;
			fc.lossLen = bLen;
			const double I = lossInt;
			const double dir = hash01 (seed, bId, st, 14) < 0.5 ? 1.0 : -1.0;
			if (style == kLossJitter)
			{
				const double amp = I * 0.150 * sr;
				const double u1 = hash01 (seed, bId + bIdx, st, 15);
				const double u0 = bIdx == 0 ? 0.0 : hash01 (seed, bId + bIdx - 1, st, 15);
				fc.jit0 = u0 * amp;
				fc.jit1 = u1 * amp;
				slewTau = std::min (slewTau, 0.0005 * sr);
			}
			else if (style == kLossRoll)
			{
				const double span = std::min (0.9 * I * bLen * curLen, 0.6 * sr);
				const double p0 = static_cast<double> (bIdx) / bLen;
				const double p1 = static_cast<double> (bIdx + 1) / bLen;
				if (dir > 0.0)
				{
					fc.rollStart = span * p0;
					fc.rollEnd = span * p1;
				}
				else
				{
					fc.rollStart = span * (1.0 - p0);
					fc.rollEnd = span * (1.0 - p1);
				}
				slewTau = 0.001 * sr;
			}
			else // stutter
			{
				fc.stutDiv = 1 + std::min (3, static_cast<int> (I * 3.999));
				const int capFrames =
				    std::max (1, static_cast<int> ((0.85 * maxDelay - tearDepthS) / curLen));
				fc.stutBase = static_cast<double> (bIdx % capFrames) * curLen;
				slewTau = 0.0004 * sr;
			}
			fc.noiseGain = staticLin * I * hash01 (seed, f, st, 16);
		}
		fc.slewCoef = slewTau <= 0.5 ? 1.0 : 1.0 - std::exp (-1.0 / slewTau);

		// ---------------- SCANLINES ----------------
		{
			const int idx = static_cast<int> (floorMod (f + linePhase, linePeriod));
			bool flag = idx < lineMissing;
			if (lineChaos > 0.0 && hash01 (seed, f, st, 3) < lineChaos)
				flag = hash01 (seed, f, st, 4) < static_cast<double> (lineMissing) / linePeriod;
			fc.dropped = gate && flag && hash01 (seed, f, st, 5) < D;
		}

		// ---------------- SMEAR ----------------
		fc.smear = gate && hash01 (seed, f, st, 6) < smearProb * D;

		return fc;
	}

	double damageFor (double level01) const
	{
		if (follow == 0.0)
			return damage;
		const double g = std::fabs (follow);
		const double scale = follow > 0.0 ? (1.0 - g + g * level01) : (1.0 - g + g * (1.0 - level01));
		return std::max (0.0, std::min (1.0, damage * scale));
	}

	//--------------------------------------------------------------------
	static inline float hermite (const float* b, int mask, int w, double delay)
	{
		const double pos = static_cast<double> (w) - delay;
		const int i = static_cast<int> (std::floor (pos));
		const float t = static_cast<float> (pos - i);
		const float y0 = b[(i - 1) & mask];
		const float y1 = b[i & mask];
		const float y2 = b[(i + 1) & mask];
		const float y3 = b[(i + 2) & mask];
		const float c0 = y1;
		const float c1 = 0.5f * (y2 - y0);
		const float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
		const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
		return ((c3 * t + c2) * t + c1) * t + c0;
	}

	static inline float noise (uint32_t& s)
	{
		s ^= s << 13;
		s ^= s >> 17;
		s ^= s << 5;
		return static_cast<float> (static_cast<int32_t> (s)) * (1.0f / 2147483648.0f);
	}

	void updateFilter ()
	{
		lpOn = bandwidth < 19900.0 && bandwidth < 0.45 * sr;
		if (!lpOn)
			return;
		const double w0 = 2.0 * 3.14159265358979 * bandwidth / sr;
		const double cw = std::cos (w0);
		const double alpha = std::sin (w0) / (2.0 * 0.70710678);
		const double a0 = 1.0 + alpha;
		b0 = static_cast<float> ((1.0 - cw) * 0.5 / a0);
		b1 = static_cast<float> ((1.0 - cw) / a0);
		b2 = b0;
		a1 = static_cast<float> (-2.0 * cw / a0);
		a2 = static_cast<float> ((1.0 - alpha) / a0);
	}

	//--------------------------------------------------------------------
	inline float processSample (Chan& k, int c, float x, double frac)
	{
		FrameCtl& fc = k.fc;

		// ---- write into tape (with ghost feedback) --------------------------
		float fb = 0.f;
		if (ghost > 0.0)
		{
			const double ramp = std::min (1.0, k.d / (0.002 * sr));
			fb = static_cast<float> (ghost * ramp) * std::tanh (k.lastWet);
		}
		k.tape[static_cast<size_t> (k.wp & bufMask)] = x + fb;
		const int w = k.wp;
		k.wp = (k.wp + 1) & bufMask;

		// ---- delay target ------------------------------------------------------
		double target = fc.tearStart + (fc.tearEnd - fc.tearStart) * frac;
		double extra = 0.0;
		if (fc.lossStyle == kLossJitter)
			extra = fc.jit0 + (fc.jit1 - fc.jit0) * frac;
		else if (fc.lossStyle == kLossRoll)
			extra = fc.rollStart + (fc.rollEnd - fc.rollStart) * frac;
		else if (fc.lossStyle == kLossStutter)
		{
			const double wlen = curLen / fc.stutDiv;
			const double t = frac * curLen;
			extra = fc.stutBase + std::floor (t / wlen) * wlen;
		}
		target += extra;
		target = std::max (0.0, std::min (target, static_cast<double> (maxDelay - 64)));
		k.d += (target - k.d) * fc.slewCoef;

		// ---- TEAR read ------------------------------------------------------------
		const double D = std::max (static_cast<double> (kBaseDelay), k.d + kBaseDelay);
		float y = hermite (k.tape.data (), bufMask, w, D);
		k.lastWet = (std::fabs (y) < 1e-20f) ? 0.f : y;
		const float tapeNow = y; // un-smeared, un-dropped (used by previous-line fill)

		// ---- SMEAR ------------------------------------------------------------------
		{
			const int W = std::max (1, std::min (static_cast<int> (smearWinF), smearCap - 4));
			// continuous input to the box / hold stages
			float held = k.holdVal;
			if (k.holdCounter <= 0)
			{
				held = y;
				k.holdVal = y;
				k.holdCounter = W;
			}
			--k.holdCounter;

			float boxIn = (smearMode == kSmearCrunch) ? held : y;
			// ring buffer + running sum
			if (k.smearWin != W)
			{
				k.smearWin = W;
				double s = 0.0;
				for (int j = 0; j < W; ++j)
					s += k.smearRing[static_cast<size_t> ((k.smearHead - 1 - j) & smearMask)];
				k.smearSum = s;
			}
			const float oldest = k.smearRing[static_cast<size_t> ((k.smearHead - W) & smearMask)];
			k.smearSum += static_cast<double> (boxIn) - static_cast<double> (oldest);
			k.smearRing[static_cast<size_t> (k.smearHead & smearMask)] = boxIn;
			k.smearHead = (k.smearHead + 1) & smearMask;
			const float avg = static_cast<float> (k.smearSum / W);

			// asymmetric one-pole ("bleed"): peaks smear out slowly
			const float aSlow = 1.f - std::exp (-1.f / static_cast<float> (W));
			const float aFast = 1.f - std::exp (-1.f / (0.1f * static_cast<float> (W) + 1.f));
			k.bleed += (y - k.bleed) * (std::fabs (y) > std::fabs (k.bleed) ? aFast : aSlow);
			if (std::fabs (k.bleed) < 1e-20f)
				k.bleed = 0.f;

			float sm;
			switch (smearMode)
			{
				case kSmearHold: sm = held; break;
				case kSmearBleed: sm = k.bleed; break;
				default: sm = avg; break; // Average, Crunch
			}

			// ramp the smear amount to avoid clicks at frame edges
			const float tgt = fc.smear ? 1.f : 0.f;
			k.smearAmt += (tgt - k.smearAmt) * 0.02f;
			if (k.smearAmt < 1e-4f && tgt == 0.f)
				k.smearAmt = 0.f;
			else if (k.smearAmt > 0.9999f && tgt == 1.f)
				k.smearAmt = 1.f; // fully smeared: exact, so Sample-Hold really holds
			if (k.smearAmt > 0.f)
			{
				float z = (k.smearAmt >= 1.f) ? sm : y + (sm - y) * k.smearAmt;
				if (colorBits < 16)
				{
					const float levels = std::exp2 (static_cast<float> (colorBits - 1));
					const float q = std::floor (z * levels + 0.5f) / levels;
					z += (q - z) * k.smearAmt;
				}
				y = z;
			}
		}

		// ---- SCANLINE dropouts ----------------------------------------------------------
		{
			const float tgt = fc.dropped ? static_cast<float> (lineDepth) : 0.f;
			if (lineEdgeS < 1.0)
				k.dropAmt = tgt;
			else
			{
				const float step = static_cast<float> (1.0 / lineEdgeS);
				if (k.dropAmt < tgt)
					k.dropAmt = std::min (tgt, k.dropAmt + step);
				else if (k.dropAmt > tgt)
					k.dropAmt = std::max (tgt, k.dropAmt - step);
			}
			const float a = k.dropAmt;
			if (k.latchPending && fc.dropped)
			{
				k.dropHeld = y;
				k.latchPending = false;
			}
			if (lineMode == kLineHold)
				k.dropHeld *= 0.99998f;
			if (a > 0.f)
			{
				switch (lineMode)
				{
					case kLineBlack: y *= (1.f - a); break;
					case kLineSnow:
						y = y * (1.f - a) + noise (k.rng) * 0.5f * static_cast<float> (staticLin) * a;
						break;
					case kLinePrevious:
					{
						const double Dp = std::min (static_cast<double> (maxDelay - 8), D + curLen);
						const float prev = hermite (k.tape.data (), bufMask, w, Dp);
						y = y + (prev - y) * a;
						(void)tapeNow;
						break;
					}
					case kLineNegative: y *= (1.f - 2.f * a); break;
					default: y = y * (1.f - a) + k.dropHeld * a; break;
				}
			}
		}

		// ---- snow burst during sync loss --------------------------------------------------
		if (fc.noiseGain > 0.0)
			y += noise (k.rng) * 0.35f * static_cast<float> (fc.noiseGain);

		// ---- bandwidth ------------------------------------------------------------------------
		if (lpOn)
		{
			const float o = b0 * y + k.z1;
			k.z1 = b1 * y - a1 * o + k.z2;
			k.z2 = b2 * y - a2 * o;
			if (std::fabs (k.z1) < 1e-20f)
				k.z1 = 0.f;
			if (std::fabs (k.z2) < 1e-20f)
				k.z2 = 0.f;
			y = o;
		}
		else
		{
			k.z1 = k.z2 = 0.f;
		}

		// ---- drive ---------------------------------------------------------------------------------
		if (driveSm > 1.0005)
		{
			const float g = static_cast<float> (driveSm);
			y = std::tanh (g * y) / std::sqrt (g);
		}

		// ---- mix with latency-matched dry -------------------------------------------------------------
		const float dry = k.x2;
		k.x2 = k.x1;
		k.x1 = x;
		const float m = static_cast<float> (mixSm);
		return (dry + (y - dry) * m) * static_cast<float> (outSm);
		(void)c;
	}

	//--------------------------------------------------------------------
	double sr = 44100.0;
	int nch = 2;
	int maxDelay = 0, bufSize = 0, bufMask = 0, smearCap = 0, smearMask = 0;
	Chan ch[kMaxChannels];
	ParamSet params;

	// derived parameters
	uint32_t seed = 1;
	double damage = 0.7, follow = 0.0, jitter = 0.0, split = 0.3;
	int interlace = 0, loop = 0;
	double tearAmt = 0.6, tearDepthS = 0.0, tearSlew = 0.2, ghost = 0.0;
	int tearShape = 0;
	int linePeriod = 4, lineMissing = 1, linePhase = 0, lineMode = 0;
	double lineChaos = 0.0, lineDepth = 1.0, lineEdgeS = 0.0;
	double smearProb = 0.4, smearWinF = 1.0;
	int smearMode = 0, colorBits = 16;
	double lossRate = 0.1, lossInt = 0.6;
	int lossLen = 4, lossStyle = 0;
	double staticLin = 0.03, bandwidth = 20000.0;
	double mixT = 1.0, outT = 1.0, driveT = 1.0;
	double mixSm = 1.0, outSm = 1.0, driveSm = 1.0;
	bool syncOn = false;
	double divBeats = 0.25, sliceMs = 12.0;
	bool bypass = false;

	// filter
	bool lpOn = false;
	float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;

	// frame clock
	int64_t frameIndex = 0;
	double framePos = 0.0, curLen = 1.0;
	bool started = false;
	bool pendingTrigger = false;
	int64_t manualStart = -1000000;
	double env = 0.0;

	// transport
	bool hostPlaying = false, hostPpqValid = false;
	double hostTempo = 0.0, hostPpq = 0.0;
};

} // namespace rd
