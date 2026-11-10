#include "RasterUI.h"
#include "RasterPresets.h"

#include "vstgui/lib/controls/coptionmenu.h"
#include "vstgui/lib/events.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>
#include <functional>
#include <random>

using namespace VSTGUI;

namespace rd {
namespace ui {

//------------------------------------------------------------------------
// palette + helpers
//------------------------------------------------------------------------
namespace pal {
static const CColor bg (17, 19, 25);
static const CColor panel (28, 31, 39);
static const CColor panelEdge (44, 49, 61);
static const CColor track (50, 55, 68);
static const CColor knobBody (40, 44, 55);
static const CColor text (218, 223, 233);
static const CColor dim (125, 134, 152);
static const CColor scan (79, 209, 197);
static const CColor tear (246, 160, 77);
static const CColor lines (226, 94, 194);
static const CColor smear (139, 212, 80);
static const CColor loss (242, 92, 84);
static const CColor out (138, 164, 200);
} // namespace pal

static CColor withAlpha (CColor c, double a)
{
	c.alpha = static_cast<uint8_t> (std::max (0.0, std::min (255.0, a * 255.0)));
	return c;
}

static CColor mixColor (const CColor& a, const CColor& b, double t)
{
	t = std::max (0.0, std::min (1.0, t));
	auto m = [t] (uint8_t x, uint8_t y) { return static_cast<uint8_t> (x + (y - x) * t + 0.5); };
	return CColor (m (a.red, b.red), m (a.green, b.green), m (a.blue, b.blue), 255);
}

static CFontRef font (double size, bool bold = false)
{
	static SharedPointer<CFontDesc> f[2][24];
	const int s = std::max (0, std::min (23, static_cast<int> (size)));
	auto& slot = f[bold ? 1 : 0][s];
	if (!slot)
		slot = makeOwned<CFontDesc> ("Arial", static_cast<CCoord> (s), bold ? kBoldFace : kNormalFace);
	return slot;
}

static void text (CDrawContext* c, const std::string& s, CRect r, const CColor& col, double size,
                  bool bold = false, CHoriTxtAlign align = kCenterText)
{
	c->setFont (font (size, bold));
	c->setFontColor (col);
	c->drawString (s.c_str (), r, align, true);
}

static void roundRect (CDrawContext* c, const CRect& r, double radius, const CColor& fill,
                       const CColor* edge = nullptr)
{
	if (auto p = owned (c->createGraphicsPath ()))
	{
		p->addRoundRect (r, radius);
		c->setFillColor (fill);
		c->drawGraphicsPath (p, CDrawContext::kPathFilled);
		if (edge)
		{
			c->setFrameColor (*edge);
			c->setLineWidth (1.0);
			c->drawGraphicsPath (p, CDrawContext::kPathStroked);
		}
	}
}

static void arc (CDrawContext* c, CPoint center, double radius, double fromDeg, double toDeg,
                 const CColor& col, double width)
{
	CDrawContext::PointList pts;
	const int n = std::max (2, static_cast<int> (std::fabs (toDeg - fromDeg) / 6.0));
	for (int i = 0; i <= n; ++i)
	{
		const double a = (fromDeg + (toDeg - fromDeg) * i / n) * 3.14159265358979 / 180.0;
		pts.push_back (CPoint (center.x + radius * std::cos (a), center.y + radius * std::sin (a)));
	}
	CDrawContext::LineList segs;
	for (size_t i = 0; i + 1 < pts.size (); ++i)
		segs.push_back (std::make_pair (pts[i], pts[i + 1]));
	c->setFrameColor (col);
	c->setLineWidth (width);
	c->setLineStyle (CLineStyle (CLineStyle::kLineCapRound, CLineStyle::kLineJoinRound));
	c->drawLines (segs); // open polyline (drawPolygon would close it with a chord)
}

static void triangle (CDrawContext* c, CPoint tip, double size, int dir, const CColor& col)
{
	// dir: 0 = down, -1 = left, 1 = right
	CDrawContext::PointList p;
	if (dir == 0)
		p = {CPoint (tip.x - size, tip.y - size * 0.6), CPoint (tip.x + size, tip.y - size * 0.6), tip};
	else if (dir > 0)
		p = {CPoint (tip.x - size * 0.8, tip.y - size), CPoint (tip.x - size * 0.8, tip.y + size), tip};
	else
		p = {CPoint (tip.x + size * 0.8, tip.y - size), CPoint (tip.x + size * 0.8, tip.y + size), tip};
	c->setFillColor (col);
	c->drawPolygon (p, kDrawFilled);
}

static std::string valueText (int id, double norm)
{
	const ParamSpec& s = spec (id);
	std::string t = formatValue (s, toPlain (s, norm));
	if (s.kind == Kind::Float || s.kind == Kind::Int)
		if (s.units[0] && !(s.zeroText && t == s.zeroText))
		{
			if (std::strcmp (s.units, "Hz") == 0 && t.size () > 2 && t.compare (t.size () - 2, 2, " k") == 0)
				return t.substr (0, t.size () - 2) + " kHz";
			t += std::string (" ") + s.units;
		}
	return t;
}

//------------------------------------------------------------------------
// Hints: hovering any control explains it in the bar at the bottom
//------------------------------------------------------------------------
struct Hint
{
	virtual ~Hint () = default;
	virtual void show (int id) = 0;
};
struct Hinted
{
	Hint* hint = nullptr;
	int hintId = -1;
	void hoverEnter () { if (hint) hint->show (hintId); }
	void hoverLeave () { if (hint) hint->show (-1); }
};

static const char* const kHintText[kNumParams] = {
	/* kSliceTime   */ "Length of each slice (frame) the signal is cut into. Short = buzzy and grainy, long = rhythmic glitches.",
	/* kTempoSync   */ "Lock the slice length to the host tempo so damage lands on the beat.",
	/* kTempoDiv    */ "Slice length as a note value (used when Sync is on).",
	/* kSliceJitter */ "Randomly varies every slice's length for an unstable, drifting feel.",
	/* kFrameLoop   */ "Repeat the same N slices of damage forever. Off = the pattern never repeats.",
	/* kSeed        */ "Picks a different random pattern. The same seed always gives the same damage.",
	/* kDamage      */ "Master amount: scales how many slices are damaged and how hard.",
	/* kInputFollow */ "Ties damage to input level. + : louder = more damage (hits). - : quieter = more damage (the gaps).",
	/* kInterlace   */ "Damage only even or odd slices, or alternate slices between left and right.",
	/* kTearAmount  */ "How many slices get torn, i.e. pushed earlier or later in time.",
	/* kTearDepth   */ "Largest time shift a torn slice can receive. Small = flutter, large = obvious displacement.",
	/* kTearShape   */ "How shifts are distributed: random, stair-stepped, skewed, sine or zigzag.",
	/* kTearSlew    */ "How smoothly the shift glides inside a slice. Low = hard digital edge, high = pitch-bendy tape.",
	/* kGhost       */ "Feeds delayed audio back so torn slices leave echoes behind.",
	/* kLinePeriod  */ "Length of the scanline pattern, in lines per slice.",
	/* kLineMissing */ "How many lines out of every period are dropped.",
	/* kLinePhase   */ "Slides the dropout pattern against the slice.",
	/* kLineChaos   */ "Replaces the strict pattern with random dropouts at the same average density.",
	/* kLineMode    */ "What replaces a dropped line: silence, noise, the previous line, an inverted copy or a held sample.",
	/* kLineDepth   */ "How completely a dropped line is replaced. Lower = partial ducking.",
	/* kLineEdge    */ "Fade time at the edge of each dropout. 0 = hard, clicky digital cuts.",
	/* kSmearAmount */ "How many slices get smeared.",
	/* kSmearMode   */ "Average blurs, Sample-Hold steps, Bleed leaves trails, Crunch quantises.",
	/* kSmearSize   */ "Width of the smear window. Bigger = duller and lower-resolution.",
	/* kColorDepth  */ "Bit depth of smeared slices. Lower = harsher crunch.",
	/* kLossRate    */ "How often a sync-loss burst starts.",
	/* kLossLength  */ "How many slices a burst lasts.",
	/* kLossIntensity*/"How violent the timing disruption is during a burst.",
	/* kLossStyle   */ "Jitter wobbles, Roll scrolls the signal, Stutter repeats it, Chaos mixes all three.",
	/* kLossTrigger */ "Hold to fire a sync-loss burst right now.",
	/* kBandwidth   */ "Low-pass filter on the damaged signal. Lower = darker, tamer highs.",
	/* kStatic      */ "Level of the snow/noise used by Snow dropouts and sync-loss bursts. Minimum = off.",
	/* kDrive       */ "Saturation applied after the damage.",
	/* kFieldSplit  */ "How often the right channel gets its own, independent damage. 0 = identical (mono-safe).",
	/* kMix         */ "Dry/wet. Keep at 100% for only the damaged sound.",
	/* kOutGain     */ "Output level.",
	/* kBypass      */ "Bypass the effect (latency-matched).",
};

//------------------------------------------------------------------------
// Panel (metaphor group) and plain label
//------------------------------------------------------------------------
class RdPanel : public CView
{
public:
	RdPanel (const CRect& r, std::string title, std::string sub, CColor accent)
	: CView (r), title_ (std::move (title)), sub_ (std::move (sub)), accent_ (accent)
	{
		setMouseEnabled (false);
	}
	void draw (CDrawContext* c) override
	{
		c->setDrawMode (kAntiAliasing | kNonIntegralMode);
		const CRect r = getViewSize ();
		roundRect (c, r, 9, pal::panel, &pal::panelEdge);
		// accent bar
		CRect bar (r.left + 1, r.top + 12, r.left + 4, r.top + 26);
		c->setFillColor (accent_);
		c->drawRect (bar, kDrawFilled);
		text (c, title_, CRect (r.left + 12, r.top + 8, r.right - 8, r.top + 28), accent_, 13, true, kLeftText);
		c->setFont (font (13, true));
		const CCoord tw = c->getStringWidth (title_.c_str ());
		text (c, sub_, CRect (r.left + 12 + tw + 10, r.top + 9, r.right - 8, r.top + 28), pal::dim, 11, false, kLeftText);
	}

private:
	std::string title_, sub_;
	CColor accent_;
};

class RdText : public CView
{
public:
	RdText (const CRect& r, std::string t, const CColor& col, double size, bool bold, CHoriTxtAlign a)
	: CView (r), t_ (std::move (t)), col_ (col), size_ (size), bold_ (bold), align_ (a)
	{
		setMouseEnabled (false);
	}
	void draw (CDrawContext* c) override { text (c, t_, getViewSize (), col_, size_, bold_, align_); }

private:
	std::string t_;
	CColor col_;
	double size_;
	bool bold_;
	CHoriTxtAlign align_;
};

//------------------------------------------------------------------------
// Knob
//------------------------------------------------------------------------
class RdKnob : public CView, public Syncable, public Hinted
{
public:
	RdKnob (const CRect& r, Host* host, int id, CColor accent, double dia = 40)
	: CView (r), host_ (host), id_ (id), accent_ (accent), dia_ (dia)
	{
		norm_ = host->getParam (id);
		hintId = id;
	}
	void onMouseEnterEvent (MouseEnterEvent& e) override { hoverEnter (); e.consumed = true; }
	void onMouseExitEvent (MouseExitEvent& e) override { hoverLeave (); e.consumed = true; }

	void sync (double) override
	{
		const double v = host_->getParam (id_);
		if (!dragging_ && std::fabs (v - norm_) > 1e-9)
		{
			norm_ = v;
			invalid ();
		}
	}

	void draw (CDrawContext* c) override
	{
		c->setDrawMode (kAntiAliasing | kNonIntegralMode);
		const ParamSpec& s = spec (id_);
		const CRect r = getViewSize ();
		text (c, s.shortName, CRect (r.left, r.top, r.right, r.top + 13), pal::dim, 10, false);
		const CPoint ctr (r.left + r.getWidth () / 2, r.top + 14 + dia_ / 2 + 2);
		const double rad = dia_ / 2 - 3;
		const bool bipolar = s.minV < 0 && s.maxV > 0;
		const double a0 = 135, span = 270;
		arc (c, ctr, rad, a0, a0 + span, pal::track, 4);
		const double base = bipolar ? a0 + span * toNormalized (s, 0.0) : a0;
		const double cur = a0 + span * norm_;
		if (std::fabs (cur - base) > 0.5)
			arc (c, ctr, rad, std::min (base, cur), std::max (base, cur), dragging_ ? mixColor (accent_, CColor (255, 255, 255), 0.35) : accent_, 4);
		const double inner = rad - 5;
		CRect body (ctr.x - inner, ctr.y - inner, ctr.x + inner, ctr.y + inner);
		c->setFillColor (pal::knobBody);
		c->drawEllipse (body, kDrawFilled);
		const double ang = cur * 3.14159265358979 / 180.0;
		c->setFrameColor (pal::text);
		c->setLineWidth (2);
		c->setLineStyle (CLineStyle (CLineStyle::kLineCapRound, CLineStyle::kLineJoinRound));
		c->drawLine (CPoint (ctr.x + std::cos (ang) * inner * 0.25, ctr.y + std::sin (ang) * inner * 0.25),
		             CPoint (ctr.x + std::cos (ang) * inner * 0.9, ctr.y + std::sin (ang) * inner * 0.9));
		text (c, valueText (id_, norm_), CRect (r.left - 2, r.bottom - 14, r.right + 2, r.bottom), dragging_ ? pal::text : pal::dim, 10);
	}

	void onMouseDownEvent (MouseDownEvent& e) override
	{
		if (!e.buttonState.isLeft ())
			return;
		e.consumed = true;
		if (e.clickCount >= 2)
		{
			// double-click: back to default
			host_->editBegin (id_);
			norm_ = defaultNormalized (id_);
			host_->editSet (id_, norm_);
			host_->editEnd (id_);
			invalid ();
			return;
		}
		dragging_ = true;
		startY_ = e.mousePosition.y;
		startVal_ = norm_;
		host_->editBegin (id_);
		invalid ();
	}
	void onMouseMoveEvent (MouseMoveEvent& e) override
	{
		if (!dragging_)
			return;
		e.consumed = true;
		const double fine = e.modifiers.has (ModifierKey::Shift) ? 0.0008 : 0.005;
		setNorm (startVal_ + (startY_ - e.mousePosition.y) * fine);
	}
	void onMouseUpEvent (MouseUpEvent& e) override
	{
		if (!dragging_)
			return;
		e.consumed = true;
		finish ();
	}
	void onMouseCancelEvent (MouseCancelEvent& e) override
	{
		if (dragging_)
			finish ();
		e.consumed = true;
	}
	void onMouseWheelEvent (MouseWheelEvent& e) override
	{
		const ParamSpec& s = spec (id_);
		const int steps = stepCount (s);
		double d = e.deltaY > 0 ? 1 : (e.deltaY < 0 ? -1 : 0);
		if (d == 0)
			return;
		const double inc = steps > 0 ? 1.0 / steps : (e.modifiers.has (ModifierKey::Shift) ? 0.002 : 0.02);
		host_->editBegin (id_);
		setNorm (norm_ + d * inc);
		host_->editEnd (id_);
		e.consumed = true;
	}

private:
	void setNorm (double v)
	{
		v = clamp01 (v);
		const int steps = stepCount (spec (id_));
		if (steps > 0)
			v = std::round (v * steps) / steps;
		if (std::fabs (v - norm_) > 1e-9)
		{
			norm_ = v;
			host_->editSet (id_, v);
			invalid ();
		}
	}
	void finish ()
	{
		dragging_ = false;
		host_->editEnd (id_);
		invalid ();
	}

	Host* host_;
	int id_;
	CColor accent_;
	double dia_;
	double norm_ = 0, startY_ = 0, startVal_ = 0;
	bool dragging_ = false;
};

//------------------------------------------------------------------------
// Drop-down for list parameters
//------------------------------------------------------------------------
class RdChoice : public CView, public Syncable, public Hinted
{
public:
	RdChoice (const CRect& r, Host* host, int id, CColor accent) : CView (r), host_ (host), id_ (id), accent_ (accent)
	{
		norm_ = host->getParam (id);
		hintId = id;
	}
	void sync (double) override
	{
		const double v = host_->getParam (id_);
		if (std::fabs (v - norm_) > 1e-9)
		{
			norm_ = v;
			invalid ();
		}
	}
	void draw (CDrawContext* c) override
	{
		c->setDrawMode (kAntiAliasing | kNonIntegralMode);
		const ParamSpec& s = spec (id_);
		const CRect r = getViewSize ();
		text (c, s.name, CRect (r.left, r.top, r.right, r.top + 13), pal::dim, 10, false, kLeftText);
		CRect b (r.left, r.top + 14, r.right, r.bottom);
		roundRect (c, b, 5, hover_ ? CColor (50, 55, 68) : CColor (38, 42, 53), &pal::panelEdge);
		text (c, valueText (id_, norm_), CRect (b.left + 8, b.top, b.right - 20, b.bottom), pal::text, 11, false, kLeftText);
		triangle (c, CPoint (b.right - 12, b.top + b.getHeight () / 2 + 3), 5, 0, accent_);
	}
	void onMouseEnterEvent (MouseEnterEvent& e) override
	{
		hover_ = true;
		hoverEnter ();
		invalid ();
		e.consumed = true;
	}
	void onMouseExitEvent (MouseExitEvent& e) override
	{
		hover_ = false;
		hoverLeave ();
		invalid ();
		e.consumed = true;
	}
	void onMouseDownEvent (MouseDownEvent& e) override
	{
		if (!e.buttonState.isLeft ())
			return;
		e.consumed = true;
		const ParamSpec& s = spec (id_);
		auto menu = makeOwned<COptionMenu> (CRect (0, 0, 10, 10), nullptr, -1, nullptr, nullptr, COptionMenu::kPopupStyle | COptionMenu::kCheckStyle);
		for (int i = 0; i < s.listCount; ++i)
			menu->addEntry (s.list[i]);
		const int steps = std::max (1, s.listCount - 1);
		menu->setCurrent (static_cast<int32_t> (std::lround (norm_ * steps)));
		CPoint where (getViewSize ().left, getViewSize ().bottom);
		localToFrame (where);
		auto self = shared (this);
		const int id = id_;
		Host* host = host_;
		menu->popup (getFrame (), where, [menu, self, id, host, steps] (COptionMenu* m) {
			const int idx = m->getLastResult ();
			if (idx < 0)
				return;
			host->editBegin (id);
			host->editSet (id, static_cast<double> (idx) / steps);
			host->editEnd (id);
		});
	}

private:
	Host* host_;
	int id_;
	CColor accent_;
	double norm_ = 0;
	bool hover_ = false;
};

//------------------------------------------------------------------------
// Toggle / momentary button for toggle parameters
//------------------------------------------------------------------------
class RdToggle : public CView, public Syncable, public Hinted
{
public:
	RdToggle (const CRect& r, Host* host, int id, std::string label, CColor accent, bool momentary = false)
	: CView (r), host_ (host), id_ (id), label_ (std::move (label)), accent_ (accent), momentary_ (momentary)
	{
		on_ = host->getParam (id) >= 0.5;
		hintId = id;
	}
	void onMouseEnterEvent (MouseEnterEvent& e) override { hoverEnter (); e.consumed = true; }
	void onMouseExitEvent (MouseExitEvent& e) override { hoverLeave (); e.consumed = true; }
	void sync (double) override
	{
		const bool v = host_->getParam (id_) >= 0.5;
		if (v != on_)
		{
			on_ = v;
			invalid ();
		}
	}
	void draw (CDrawContext* c) override
	{
		c->setDrawMode (kAntiAliasing | kNonIntegralMode);
		const CRect r = getViewSize ();
		roundRect (c, r, r.getHeight () / 2, on_ ? mixColor (pal::panel, accent_, 0.85) : CColor (38, 42, 53), on_ ? nullptr : &pal::panelEdge);
		text (c, label_, r, on_ ? CColor (14, 16, 20) : pal::dim, 11, true);
	}
	void onMouseDownEvent (MouseDownEvent& e) override
	{
		if (!e.buttonState.isLeft ())
			return;
		e.consumed = true;
		on_ = momentary_ ? true : !on_;
		host_->editBegin (id_);
		host_->editSet (id_, on_ ? 1.0 : 0.0);
		if (!momentary_)
			host_->editEnd (id_);
		invalid ();
	}
	void onMouseUpEvent (MouseUpEvent& e) override
	{
		if (!momentary_)
			return;
		e.consumed = true;
		on_ = false;
		host_->editSet (id_, 0.0);
		host_->editEnd (id_);
		invalid ();
	}

private:
	Host* host_;
	int id_;
	std::string label_;
	CColor accent_;
	bool momentary_;
	bool on_ = false;
};

//------------------------------------------------------------------------
// Header buttons
//------------------------------------------------------------------------
class RdButton : public CView
{
public:
	enum Icon { None, Prev, Next };
	RdButton (const CRect& r, std::string label, Icon icon, std::function<void (RdButton*)> cb)
	: CView (r), label_ (std::move (label)), icon_ (icon), cb_ (std::move (cb))
	{
	}
	void draw (CDrawContext* c) override
	{
		c->setDrawMode (kAntiAliasing | kNonIntegralMode);
		const CRect r = getViewSize ();
		roundRect (c, r, 6, down_ ? CColor (66, 72, 88) : (hover_ ? CColor (52, 58, 72) : CColor (40, 44, 56)), &pal::panelEdge);
		if (icon_ == Prev)
			triangle (c, CPoint (r.left + r.getWidth () / 2 - 4, r.top + r.getHeight () / 2), 5, -1, pal::text);
		else if (icon_ == Next)
			triangle (c, CPoint (r.left + r.getWidth () / 2 + 4, r.top + r.getHeight () / 2), 5, 1, pal::text);
		else
			text (c, label_, r, pal::text, 12, true);
	}
	void onMouseEnterEvent (MouseEnterEvent& e) override { hover_ = true; invalid (); e.consumed = true; }
	void onMouseExitEvent (MouseExitEvent& e) override { hover_ = false; down_ = false; invalid (); e.consumed = true; }
	void onMouseDownEvent (MouseDownEvent& e) override
	{
		if (!e.buttonState.isLeft ())
			return;
		down_ = true;
		invalid ();
		e.consumed = true;
	}
	void onMouseUpEvent (MouseUpEvent& e) override
	{
		if (down_ && hitTest (e.mousePosition))
			cb_ (this);
		down_ = false;
		invalid ();
		e.consumed = true;
	}

private:
	std::string label_;
	Icon icon_;
	std::function<void (RdButton*)> cb_;
	bool hover_ = false, down_ = false;
};

// The preset name field: click opens the preset menu.
class RdPresetField : public CView, public Syncable
{
public:
	RdPresetField (const CRect& r, Host* host) : CView (r), host_ (host) { refresh (); }
	void sync (double) override
	{
		const int rev = host_->presetRevision ();
		if (rev != rev_)
			refresh ();
	}
	void draw (CDrawContext* c) override
	{
		c->setDrawMode (kAntiAliasing | kNonIntegralMode);
		const CRect r = getViewSize ();
		roundRect (c, r, 6, hover_ ? CColor (34, 38, 49) : CColor (24, 27, 35), &pal::panelEdge);
		text (c, "PRESET", CRect (r.left + 10, r.top, r.left + 62, r.bottom), pal::dim, 9, true, kLeftText);
		text (c, name_ + (modified_ ? "  *" : ""), CRect (r.left + 64, r.top, r.right - 26, r.bottom), modified_ ? pal::tear : pal::text, 13, true, kLeftText);
		triangle (c, CPoint (r.right - 14, r.top + r.getHeight () / 2 + 2), 5, 0, pal::scan);
	}
	void onMouseEnterEvent (MouseEnterEvent& e) override { hover_ = true; invalid (); e.consumed = true; }
	void onMouseExitEvent (MouseExitEvent& e) override { hover_ = false; invalid (); e.consumed = true; }
	void onMouseDownEvent (MouseDownEvent& e) override
	{
		if (!e.buttonState.isLeft ())
			return;
		e.consumed = true;
		CPoint where (getViewSize ().left, getViewSize ().bottom + 2);
		localToFrame (where);
		host_->presetMenu (getFrame (), where);
	}

private:
	void refresh ()
	{
		rev_ = host_->presetRevision ();
		name_ = host_->presetName ();
		modified_ = host_->presetModified ();
		invalid ();
	}
	Host* host_;
	std::string name_;
	bool modified_ = false, hover_ = false;
	int rev_ = -1;
};

//------------------------------------------------------------------------
// Small pill button (RND / RST in every section header)
//------------------------------------------------------------------------
class RdMini : public CView, public Hinted
{
public:
	RdMini (const CRect& r, std::string label, CColor accent, std::function<void ()> cb)
	: CView (r), label_ (std::move (label)), accent_ (accent), cb_ (std::move (cb))
	{
	}
	void draw (CDrawContext* c) override
	{
		c->setDrawMode (kAntiAliasing | kNonIntegralMode);
		const CRect r = getViewSize ();
		roundRect (c, r, 4, down_ ? mixColor (pal::panel, accent_, 0.5) : (hover_ ? CColor (58, 64, 80) : CColor (40, 44, 56)), &pal::panelEdge);
		text (c, label_, r, hover_ ? accent_ : pal::dim, 9, true);
	}
	void onMouseEnterEvent (MouseEnterEvent& e) override { hover_ = true; invalid (); e.consumed = true; }
	void onMouseExitEvent (MouseExitEvent& e) override { hover_ = false; down_ = false; invalid (); e.consumed = true; }
	void onMouseDownEvent (MouseDownEvent& e) override
	{
		if (!e.buttonState.isLeft ())
			return;
		down_ = true;
		invalid ();
		e.consumed = true;
	}
	void onMouseUpEvent (MouseUpEvent& e) override
	{
		if (down_ && hitTest (e.mousePosition))
			cb_ ();
		down_ = false;
		invalid ();
		e.consumed = true;
	}

private:
	std::string label_;
	CColor accent_;
	std::function<void ()> cb_;
	bool hover_ = false, down_ = false;
};

//------------------------------------------------------------------------
// Hint bar: name, current value and a one-line explanation of whatever is under the mouse
//------------------------------------------------------------------------
class RdHintBar : public CView, public Syncable, public Hint
{
public:
	RdHintBar (const CRect& r, Host* host) : CView (r), host_ (host) { setMouseEnabled (false); }
	void show (int id) override
	{
		if (id != id_)
		{
			id_ = id;
			invalid ();
		}
	}
	void sync (double) override
	{
		if (id_ >= 0 && id_ < kNumParams)
		{
			const std::string v = valueText (id_, host_->getParam (id_));
			if (v != value_)
			{
				value_ = v;
				invalid ();
			}
		}
	}
	void draw (CDrawContext* c) override
	{
		c->setDrawMode (kAntiAliasing | kNonIntegralMode);
		const CRect r = getViewSize ();
		roundRect (c, r, 7, CColor (22, 25, 32), &pal::panelEdge);
		if (id_ >= 0 && id_ < kNumParams)
		{
			const ParamSpec& s = spec (id_);
			const std::string title = s.name;
			text (c, title, CRect (r.left + 12, r.top, r.left + 160, r.bottom), pal::text, 12, true, kLeftText);
			text (c, value_, CRect (r.left + 160, r.top, r.left + 250, r.bottom), pal::scan, 12, true, kLeftText);
			text (c, kHintText[id_], CRect (r.left + 255, r.top, r.right - 10, r.bottom), pal::dim, 11, false, kLeftText);
		}
		else
			text (c, "Drag = change   |   Shift + drag = fine   |   Double-click = reset   |   Mouse wheel = nudge   |   RND / RST = randomise / reset one section",
			     CRect (r.left + 12, r.top, r.right - 10, r.bottom), pal::dim, 11, false, kLeftText);
	}

private:
	Host* host_;
	int id_ = -1;
	std::string value_;
};

//------------------------------------------------------------------------
// Layout
//------------------------------------------------------------------------
namespace {
struct Range
{
	double lo, hi;
};
// Sensible random ranges (normalised) so a roll never lands on something unusable.
Range randomRange (int id)
{
	switch (id)
	{
		case kSliceTime: return {0.25, 0.8};
		case kSliceJitter: return {0.0, 0.4};
		case kFrameLoop: return {0.0, 0.25};
		case kInputFollow: return {0.4, 0.7};
		case kTearAmount: return {0.3, 1.0};
		case kTearDepth: return {0.1, 0.8};
		case kTearSlew: return {0.0, 0.8};
		case kGhost: return {0.0, 0.5};
		case kLinePeriod: return {0.0, 0.5};
		case kLineMissing: return {0.0, 0.2};
		case kLineChaos: return {0.0, 0.5};
		case kLineDepth: return {0.5, 1.0};
		case kLineEdge: return {0.0, 0.5};
		case kSmearAmount: return {0.2, 1.0};
		case kSmearSize: return {0.2, 0.8};
		case kColorDepth: return {0.4, 1.0};
		case kLossRate: return {0.05, 0.7};
		case kLossLength: return {0.0, 0.3};
		case kLossIntensity: return {0.3, 1.0};
		case kBandwidth: return {0.45, 1.0};
		case kStatic: return {0.0, 0.5};
		case kDrive: return {0.0, 0.4};
		default: return {0.0, 1.0};
	}
}
// never touched by RND / RST
bool isProtected (int id)
{
	return id == kBypass || id == kLossTrigger || id == kMix || id == kOutGain || id == kDamage || id == kTempoSync ||
	       id == kTempoDiv;
}
} // namespace

RasterUI::RasterUI (Host* host)
: CViewContainer (CRect (0, 0, kWidth, kHeight)), host_ (host),
  rng_ (static_cast<uint32_t> (std::chrono::steady_clock::now ().time_since_epoch ().count ()))
{
	setBackgroundColor (pal::bg);
	RdHintBar* hintBar = new RdHintBar (CRect (10, kHeight - 40, kWidth - 10, kHeight - 10), host);
	auto add = [this, hintBar] (CView* v) {
		addView (v);
		if (auto* s = dynamic_cast<Syncable*> (v))
			syncables_.push_back (s);
		if (auto* h = dynamic_cast<Hinted*> (v))
			h->hint = hintBar;
		return v;
	};
	add (hintBar);

	const double M = 10;
	const double cw = 76, kh = 88; // knob cell
	const double dia = 46;

	// ---------------- header ----------------
	add (new RdText (CRect (M + 4, 8, 240, 32), "RASTER DAMAGE", pal::text, 18, true, kLeftText));
	add (new RdText (CRect (M + 4, 30, 240, 44), "scanline filter for audio", pal::dim, 10, false, kLeftText));
	add (new RdButton (CRect (250, 10, 278, 44), "", RdButton::Prev, [host] (RdButton*) { host->presetStep (-1); }));
	add (new RdPresetField (CRect (282, 10, 540, 44), host));
	add (new RdButton (CRect (544, 10, 572, 44), "", RdButton::Next, [host] (RdButton*) { host->presetStep (1); }));
	add (new RdButton (CRect (584, 10, 640, 44), "Save", RdButton::None, [host] (RdButton* b) { host->presetSave (b->getFrame ()); }));
	add (new RdButton (CRect (644, 10, 700, 44), "Load", RdButton::None, [host] (RdButton* b) { host->presetLoad (b->getFrame ()); }));
	add (new RdButton (CRect (704, 10, 760, 44), "Init", RdButton::None, [host] (RdButton*) { host->presetInit (); }));
	add (new RdButton (CRect (772, 10, 868, 44), "Randomize", RdButton::None, [this] (RdButton*) { snapshot (); randomize (-1); }));
	add (new RdButton (CRect (872, 10, 922, 44), "Undo", RdButton::None, [this] (RdButton*) { undo (); }));
	add (new RdToggle (CRect (932, 15, kWidth - M, 39), host, kBypass, "BYPASS", pal::loss));

	auto section = [&] (const CRect& r, const char* title, const char* sub, CColor col, int unit) {
		add (new RdPanel (r, title, sub, col));
		add (new RdMini (CRect (r.right - 78, r.top + 9, r.right - 46, r.top + 25), "RND", col, [this, unit] () { snapshot (); randomize (unit); }));
		add (new RdMini (CRect (r.right - 42, r.top + 9, r.right - 10, r.top + 25), "RST", col, [this, unit] () { snapshot (); resetUnit (unit); }));
	};
	auto knob = [&] (double x, double y, int id, CColor col, double d = 46) { add (new RdKnob (CRect (x, y, x + cw, y + kh), host, id, col, d)); };

	// ---------------- SCAN strip ----------------
	const double scanY = 54, scanH = 124;
	section (CRect (M, scanY, kWidth - M, scanY + scanH), "SCAN", "cut the signal into frames; it then flows through TEAR > SMEAR > SCANLINES > SYNC LOSS > OUTPUT", pal::scan, kUnitScan);
	{
		const double y = scanY + 32;
		double x = M + 12;
		knob (x, y, kSliceTime, pal::scan);
		x += 84;
		add (new RdToggle (CRect (x, y + 6, x + 82, y + 30), host, kTempoSync, "TEMPO SYNC", pal::scan));
		add (new RdChoice (CRect (x, y + 38, x + 82, y + 78), host, kTempoDiv, pal::scan));
		x += 92;
		knob (x, y, kSliceJitter, pal::scan);
		x += 84;
		knob (x, y, kFrameLoop, pal::scan);
		x += 84;
		knob (x, y, kSeed, pal::scan);
		x += 94;
		add (new RdChoice (CRect (x, y + 20, x + 116, y + 60), host, kInterlace, pal::scan));
		x += 130;
		knob (x, y, kInputFollow, pal::scan);
		// the master control, big, on the right
		const double dx = kWidth - M - 150;
		add (new RdKnob (CRect (dx + 24, scanY + 30, dx + 116, scanY + 30 + 92), host, kDamage, pal::scan, 62));
	}

	// ---------------- the four damage sections, in the order the audio flows ----------------
	const double modY = scanY + scanH + 10, modH = 262;
	const double pw = (kWidth - 2 * M - 3 * 10) / 4.0;
	struct Mod
	{
		const char* title;
		const char* sub;
		CColor col;
		int unit;
		int modeId;
		std::vector<int> knobs;
	};
	const Mod mods[4] = {
	    {"TEAR", "time shifts", pal::tear, kUnitTear, kTearShape, {kTearAmount, kTearDepth, kTearSlew, kGhost}},
	    {"SMEAR", "blur / hold", pal::smear, kUnitSmear, kSmearMode, {kSmearAmount, kSmearSize, kColorDepth}},
	    {"SCANLINES", "dropouts", pal::lines, kUnitScanlines, kLineMode, {kLinePeriod, kLineMissing, kLinePhase, kLineChaos, kLineDepth, kLineEdge}},
	    {"SYNC LOSS", "bursts", pal::loss, kUnitSyncLoss, kLossStyle, {kLossRate, kLossLength, kLossIntensity}}};
	for (int m = 0; m < 4; ++m)
	{
		const double px = M + m * (pw + 10);
		section (CRect (px, modY, px + pw, modY + modH), mods[m].title, mods[m].sub, mods[m].col, mods[m].unit);
		add (new RdChoice (CRect (px + 12, modY + 32, px + pw - 12, modY + 72), host, mods[m].modeId, mods[m].col));
		const double kx = px + (pw - 3 * cw) / 2;
		for (size_t i = 0; i < mods[m].knobs.size (); ++i)
			knob (kx + (i % 3) * cw, modY + 76 + (i / 3) * (kh + 2), mods[m].knobs[i], mods[m].col);
	}
	{
		const double row2 = modY + 76 + kh + 2;
		auto note = [&] (double x, double y, const char* l1, const char* l2) {
			add (new RdText (CRect (x, y, x + 150, y + 14), l1, pal::dim, 10, false, kLeftText));
			add (new RdText (CRect (x, y + 14, x + 150, y + 28), l2, pal::dim, 10, false, kLeftText));
		};
		const double tx = M, sx = M + (pw + 10), lx = M + 3 * (pw + 10);
		note (tx + (pw - 3 * cw) / 2 + cw + 4, row2 + 30, "Pushes slices earlier or", "later. Ghost = echoes.");
		note (sx + (pw - 3 * cw) / 2 + 6, row2 + 30, "Blurs or holds short windows.", "Bits = bit crush.");
		add (new RdToggle (CRect (lx + 16, row2 + 14, lx + 150, row2 + 44), host, kLossTrigger, "FIRE NOW", pal::loss, true));
		note (lx + 16, row2 + 54, "Hold to fire a burst. Rate", "sets how often they occur.");
	}

	// ---------------- OUTPUT strip ----------------
	const double outY = modY + modH + 10, outH = 118;
	section (CRect (M, outY, kWidth - M, outY + outH), "OUTPUT", "tone, grit and level", pal::out, kUnitOutput);
	{
		const double y = outY + 26;
		double x = M + 12;
		const int ids[4] = {kBandwidth, kStatic, kDrive, kFieldSplit};
		for (int id : ids)
		{
			knob (x, y, id, pal::out);
			x += cw + 8;
		}
		const double mx = kWidth - M - 12 - 2 * (cw + 8);
		knob (mx, y, kMix, pal::out);
		knob (mx + cw + 8, y, kOutGain, pal::out);
	}
}

//------------------------------------------------------------------------
void RasterUI::tick (double dt)
{
	for (auto* s : syncables_)
		s->sync (dt);
}

void RasterUI::snapshot ()
{
	undo_.assign (kNumParams, 0.0);
	for (int i = 0; i < kNumParams; ++i)
		undo_[static_cast<size_t> (i)] = host_->getParam (i);
	haveUndo_ = true;
}

void RasterUI::undo ()
{
	if (!haveUndo_)
		return;
	for (int i = 0; i < kNumParams; ++i)
	{
		const double v = undo_[static_cast<size_t> (i)];
		if (isProtected (i) && i != kDamage)
			continue;
		if (std::fabs (host_->getParam (i) - v) < 1e-9)
			continue;
		host_->editBegin (i);
		host_->editSet (i, v);
		host_->editEnd (i);
	}
	haveUndo_ = false;
}

void RasterUI::randomize (int unit)
{
	std::uniform_real_distribution<double> u01 (0.0, 1.0);
	for (int i = 0; i < kNumParams; ++i)
	{
		const ParamSpec& s = spec (i);
		if (isProtected (i) || (unit >= 0 && s.unit != unit))
			continue;
		double v;
		if (s.kind == Kind::List)
			v = std::floor (u01 (rng_) * s.listCount) / std::max (1, s.listCount - 1);
		else
		{
			const Range r = randomRange (i);
			v = r.lo + (r.hi - r.lo) * u01 (rng_);
		}
		v = clamp01 (v);
		const int steps = stepCount (s);
		if (steps > 0)
			v = std::round (v * steps) / steps;
		host_->editBegin (i);
		host_->editSet (i, v);
		host_->editEnd (i);
	}
}

void RasterUI::resetUnit (int unit)
{
	for (int i = 0; i < kNumParams; ++i)
	{
		const ParamSpec& s = spec (i);
		if (i == kBypass || i == kLossTrigger || (unit >= 0 && s.unit != unit))
			continue;
		host_->editBegin (i);
		host_->editSet (i, defaultNormalized (i));
		host_->editEnd (i);
	}
}

} // namespace ui
} // namespace rd
