#include "RasterUI.h"
#include "RasterEngine.h"
#include "RasterPresets.h"

#include "vstgui/lib/controls/coptionmenu.h"
#include "vstgui/lib/events.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>

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
class RdKnob : public CView, public Syncable
{
public:
	RdKnob (const CRect& r, Host* host, int id, CColor accent, double dia = 40)
	: CView (r), host_ (host), id_ (id), accent_ (accent), dia_ (dia)
	{
		norm_ = host->getParam (id);
	}

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
class RdChoice : public CView, public Syncable
{
public:
	RdChoice (const CRect& r, Host* host, int id, CColor accent) : CView (r), host_ (host), id_ (id), accent_ (accent)
	{
		norm_ = host->getParam (id);
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
		invalid ();
		e.consumed = true;
	}
	void onMouseExitEvent (MouseExitEvent& e) override
	{
		hover_ = false;
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
class RdToggle : public CView, public Syncable
{
public:
	RdToggle (const CRect& r, Host* host, int id, std::string label, CColor accent, bool momentary = false)
	: CView (r), host_ (host), id_ (id), label_ (std::move (label)), accent_ (accent), momentary_ (momentary)
	{
		on_ = host->getParam (id) >= 0.5;
	}
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
// The live raster: one row per slice, drawn from the engine's real per-slice decisions
//------------------------------------------------------------------------
class RdRaster : public CView, public Syncable
{
public:
	RdRaster (const CRect& r, Host* host) : CView (r), host_ (host)
	{
		eng_.prepare (48000.0, 2);
		last_ = host_->params ();
		eng_.setParams (last_);
		setMouseEnabled (false);
	}
	void sync (double dt) override
	{
		const ParamSet p = host_->params ();
		if (std::memcmp (p.v, last_.v, sizeof (p.v)) != 0)
		{
			last_ = p;
			eng_.setParams (p);
		}
		scroll_ += dt * 9.0; // rows per second: the picture slowly rolls so you can see the pattern evolve
		invalid ();
	}

	static CColor bar (int sub, bool negative)
	{
		static const CColor cols[8] = {CColor (232, 232, 226), CColor (222, 202, 70), CColor (66, 202, 214), CColor (88, 202, 98),
		                               CColor (202, 78, 190), CColor (212, 68, 68),   CColor (66, 88, 212),  CColor (24, 27, 36)};
		CColor c = cols[((sub / 2) % 8 + 8) % 8];
		if (negative)
			c = CColor (255 - c.red, 255 - c.green, 255 - c.blue, 255);
		return c;
	}

	void draw (CDrawContext* c) override
	{
		c->setDrawMode (kAntiAliasing | kNonIntegralMode);
		const CRect r = getViewSize ();
		roundRect (c, r, 9, pal::panel, &pal::panelEdge);
		CRect bar0 (r.left + 1, r.top + 12, r.left + 4, r.top + 26);
		c->setFillColor (pal::scan);
		c->drawRect (bar0, kDrawFilled);
		text (c, "LIVE RASTER", CRect (r.left + 12, r.top + 8, r.right - 8, r.top + 28), pal::scan, 13, true, kLeftText);
		text (c, "what each slice is doing", CRect (r.left + 12, r.top + 9, r.right - 12, r.top + 28), pal::dim, 11, false, kRightText);

		const CRect pic (r.left + 12, r.top + 34, r.right - 12, r.bottom - 34);
		c->setFillColor (CColor (6, 7, 10));
		c->drawRect (pic, kDrawFilled);
		const double gutter = 22;
		const CRect img (pic.left + gutter, pic.top + 2, pic.right - 2, pic.bottom - 2);
		const int rows = 56;
		const double rowH = img.getHeight () / rows;
		const int64_t base = static_cast<int64_t> (std::floor (scroll_));
		const double frac = scroll_ - std::floor (scroll_);

		c->saveGlobalState ();
		c->setClipRect (pic);
		const int smearMode = static_cast<int> (std::lround (last_[kSmearMode]));
		const int lineMode = static_cast<int> (std::lround (last_[kLineMode]));
		const double depth = last_[kLineDepth] * 0.01;
		const int bits = static_cast<int> (std::lround (last_[kColorDepth]));
		const double sizeMs = last_[kSmearSize];
		const int group = std::max (2, std::min (8, 2 + static_cast<int> (std::log2 (std::max (0.1, sizeMs / 0.1)) * 0.9)));
		const int K = 48;
		const double cw = img.getWidth () / K;

		for (int i = 0; i < rows + 1; ++i)
		{
			const int64_t f = base + i;
			const auto fc = eng_.previewFrame (f, 0, 0.8);
			const double y0 = img.top + (i - frac) * rowH;
			CRect row (img.left, y0, img.right, y0 + rowH - 1);

			// horizontal displacement in sub-cells
			double shift = 0.0;
			const double depthS = std::max (1.0, eng_.tearDepthSamples ());
			shift += (fc.tearStart + fc.tearEnd) * 0.5 / depthS * 7.0;
			if (fc.lossStyle == kLossJitter)
				shift += (fc.jit0 + fc.jit1) * 0.5 / 7200.0 * 6.0;
			else if (fc.lossStyle == kLossRoll)
				shift += (fc.rollStart + fc.rollEnd) * 0.5 / 28800.0 * 24.0;
			const int sh = static_cast<int> (std::lround (shift));

			CColor cells[K];
			for (int k = 0; k < K; ++k)
			{
				int src = k - sh;
				if (fc.lossStyle == kLossStutter)
					src = (k % std::max (1, K / fc.stutDiv)) - sh;
				cells[k] = bar (src, false);
			}
			if (fc.smear)
			{
				CColor sm[K];
				for (int k = 0; k < K; ++k)
				{
					const int g0 = (k / group) * group;
					switch (smearMode)
					{
						case kSmearAverage:
						{
							int rr = 0, gg = 0, bb = 0, n = 0;
							for (int j = g0; j < std::min (K, g0 + group); ++j, ++n)
								rr += cells[j].red, gg += cells[j].green, bb += cells[j].blue;
							sm[k] = CColor (rr / n, gg / n, bb / n, 255);
							break;
						}
						case kSmearBleed: sm[k] = k == 0 ? cells[0] : mixColor (sm[k - 1], cells[k], 0.28); break;
						default: sm[k] = cells[g0]; break; // hold / crunch
					}
					if (smearMode == kSmearCrunch || bits < 16)
					{
						const int lv = std::max (2, std::min (8, bits < 16 ? bits - 1 : 3));
						auto q = [lv] (uint8_t v) { return static_cast<uint8_t> (std::floor (v / 255.0 * (lv - 1) + 0.5) * 255.0 / (lv - 1)); };
						sm[k] = CColor (q (sm[k].red), q (sm[k].green), q (sm[k].blue), 255);
					}
				}
				for (int k = 0; k < K; ++k)
					cells[k] = sm[k];
			}
			if (fc.dropped)
			{
				const CColor black (5, 5, 8);
				for (int k = 0; k < K; ++k)
				{
					CColor d = black;
					switch (lineMode)
					{
						case kLineSnow:
						{
							const double u = hash01 (7, f, static_cast<uint32_t> (k), 77);
							const uint8_t g = static_cast<uint8_t> (30 + u * 190);
							d = CColor (g, g, g, 255);
							break;
						}
						case kLinePrevious: d = bar (k + sh - 3, false); break;
						case kLineNegative: d = bar (k - sh, true); break;
						case kLineHold: d = cells[0]; break;
						default: break;
					}
					cells[k] = mixColor (cells[k], d, depth);
				}
			}
			for (int k = 0; k < K; ++k)
			{
				CRect cell (row.left + k * cw, row.top, row.left + (k + 1) * cw + 0.6, row.bottom);
				c->setFillColor (cells[k]);
				c->drawRect (cell, kDrawFilled);
			}
			if (fc.noiseGain > 0.0)
			{
				c->setFillColor (CColor (255, 255, 255, 150));
				for (int n = 0; n < 6; ++n)
				{
					const double u = hash01 (3, f, static_cast<uint32_t> (n), 78);
					c->drawRect (CRect (row.left + u * row.getWidth (), row.top, row.left + u * row.getWidth () + 2, row.bottom), kDrawFilled);
				}
			}
			// gutter lanes: tear / dropout / smear / sync loss
			const double lane = (gutter - 4) / 4.0;
			const bool torn = std::fabs (shift) > 0.4 && fc.lossStyle < 0;
			const bool flags[4] = {torn, fc.dropped, fc.smear, fc.lossStyle >= 0};
			const CColor lc[4] = {pal::tear, pal::lines, pal::smear, pal::loss};
			for (int l = 0; l < 4; ++l)
			{
				if (!flags[l])
					continue;
				c->setFillColor (lc[l]);
				c->drawRect (CRect (pic.left + 2 + l * lane, row.top, pic.left + 2 + (l + 1) * lane - 1, row.bottom), kDrawFilled);
			}
		}
		c->restoreGlobalState ();
		c->setFrameColor (pal::panelEdge);
		c->setLineWidth (1);
		c->drawRect (pic, kDrawStroked);

		// legend
		const char* names[4] = {"tear", "dropout", "smear", "sync loss"};
		const CColor lc[4] = {pal::tear, pal::lines, pal::smear, pal::loss};
		double x = r.left + 14;
		for (int l = 0; l < 4; ++l)
		{
			c->setFillColor (lc[l]);
			c->drawRect (CRect (x, r.bottom - 22, x + 8, r.bottom - 14), kDrawFilled);
			text (c, names[l], CRect (x + 11, r.bottom - 25, x + 70, r.bottom - 11), pal::dim, 10, false, kLeftText);
			x += (l == 2 ? 62 : 56);
		}
	}

private:
	Host* host_;
	Engine eng_;
	ParamSet last_;
	double scroll_ = 0;
};

//------------------------------------------------------------------------
// Layout
//------------------------------------------------------------------------
RasterUI::RasterUI (Host* host) : CViewContainer (CRect (0, 0, kWidth, kHeight))
{
	setBackgroundColor (pal::bg);
	auto add = [this] (CView* v) {
		addView (v);
		if (auto* s = dynamic_cast<Syncable*> (v))
			syncables_.push_back (s);
		return v;
	};

	const double M = 10;

	// ---------------- header ----------------
	add (new RdText (CRect (M + 4, 10, 300, 34), "RASTER DAMAGE", pal::text, 18, true, kLeftText));
	add (new RdText (CRect (M + 4, 32, 300, 46), "scanline filter for audio", pal::dim, 10, false, kLeftText));
	add (new RdButton (CRect (330, 12, 358, 44), "", RdButton::Prev, [host] (RdButton*) { host->presetStep (-1); }));
	add (new RdPresetField (CRect (362, 12, 662, 44), host));
	add (new RdButton (CRect (666, 12, 694, 44), "", RdButton::Next, [host] (RdButton*) { host->presetStep (1); }));
	{
		RdButton* saveBtn = new RdButton (CRect (704, 12, 764, 44), "Save", RdButton::None, [host] (RdButton* b) { host->presetSave (b->getFrame ()); });
		RdButton* loadBtn = new RdButton (CRect (770, 12, 830, 44), "Load", RdButton::None, [host] (RdButton* b) { host->presetLoad (b->getFrame ()); });
		RdButton* initBtn = new RdButton (CRect (836, 12, 896, 44), "Init", RdButton::None, [host] (RdButton*) { host->presetInit (); });
		add (saveBtn);
		add (loadBtn);
		add (initBtn);
	}
	add (new RdToggle (CRect (912, 16, 990 - M + 10, 40), host, kBypass, "BYPASS", pal::loss));

	// ---------------- SCAN strip ----------------
	const double scanY = 56, scanH = 116;
	add (new RdPanel (CRect (M, scanY, kWidth - M, scanY + scanH), "SCAN", "cut the signal into frames", pal::scan));
	{
		const double y = scanY + 32;
		const double cw = 78, h = 76;
		double x = M + 14;
		add (new RdKnob (CRect (x, y, x + cw, y + h), host, kSliceTime, pal::scan));
		x += cw;
		// tempo sync block
		add (new RdToggle (CRect (x + 4, y + 8, x + 74, y + 30), host, kTempoSync, "SYNC", pal::scan));
		add (new RdChoice (CRect (x + 4, y + 34, x + 74, y + 72), host, kTempoDiv, pal::scan));
		x += 82;
		add (new RdKnob (CRect (x, y, x + cw, y + h), host, kSliceJitter, pal::scan));
		x += cw;
		add (new RdKnob (CRect (x, y, x + cw, y + h), host, kFrameLoop, pal::scan));
		x += cw;
		add (new RdKnob (CRect (x, y, x + cw, y + h), host, kSeed, pal::scan));
		x += cw + 6;
		add (new RdChoice (CRect (x, y + 14, x + 108, y + 52), host, kInterlace, pal::scan));
		x += 124;
		add (new RdKnob (CRect (x, y, x + cw, y + h), host, kInputFollow, pal::scan));
		// big damage knob on the right
		const double dx = kWidth - M - 150;
		add (new RdText (CRect (dx, scanY + 8, dx + 140, scanY + 28), "DAMAGE", pal::text, 13, true, kCenterText));
		add (new RdKnob (CRect (dx + 30, scanY + 28, dx + 110, scanY + 28 + 84), host, kDamage, pal::scan, 62));
	}

	// ---------------- middle: raster + 4 metaphor panels ----------------
	const double midY = scanY + scanH + 10, midH = 372;
	add (new RdRaster (CRect (M, midY, M + 290, midY + midH), host));
	const double px = M + 290 + 10;
	const double pw = (kWidth - M - px - 10) / 2;
	const double ph = (midH - 10) / 2;
	struct P
	{
		double x, y;
		const char* t;
		const char* s;
		CColor c;
	};
	const P panels[4] = {{px, midY, "TEAR", "horizontal tearing", pal::tear},
	                     {px + pw + 10, midY, "SCANLINES", "missing lines", pal::lines},
	                     {px, midY + ph + 10, "SMEAR", "pixel smearing", pal::smear},
	                     {px + pw + 10, midY + ph + 10, "SYNC LOSS", "timing bursts", pal::loss}};
	for (const P& p : panels)
		add (new RdPanel (CRect (p.x, p.y, p.x + pw, p.y + ph), p.t, p.s, p.c));

	const double cw = 78, ch = 76;
	auto knobAt = [&] (double px0, double py0, int col, int row, int id, CColor col2) {
		const double x = px0 + 8 + col * cw, y = py0 + 32 + row * (ch + 2);
		add (new RdKnob (CRect (x, y, x + cw, y + ch), host, id, col2));
	};
	// TEAR: 4 knobs + shape
	knobAt (panels[0].x, panels[0].y, 0, 0, kTearAmount, pal::tear);
	knobAt (panels[0].x, panels[0].y, 1, 0, kTearDepth, pal::tear);
	knobAt (panels[0].x, panels[0].y, 2, 0, kTearSlew, pal::tear);
	knobAt (panels[0].x, panels[0].y, 3, 0, kGhost, pal::tear);
	add (new RdChoice (CRect (panels[0].x + 14, panels[0].y + 32 + ch + 8, panels[0].x + 14 + 130, panels[0].y + 32 + ch + 46), host, kTearShape, pal::tear));
	// SCANLINES: 6 knobs (2 rows) + fill
	knobAt (panels[1].x, panels[1].y, 0, 0, kLinePeriod, pal::lines);
	knobAt (panels[1].x, panels[1].y, 1, 0, kLineMissing, pal::lines);
	knobAt (panels[1].x, panels[1].y, 2, 0, kLinePhase, pal::lines);
	knobAt (panels[1].x, panels[1].y, 3, 0, kLineChaos, pal::lines);
	knobAt (panels[1].x, panels[1].y, 0, 1, kLineDepth, pal::lines);
	knobAt (panels[1].x, panels[1].y, 1, 1, kLineEdge, pal::lines);
	add (new RdChoice (CRect (panels[1].x + 8 + 2 * cw + 4, panels[1].y + 32 + ch + 22, panels[1].x + 8 + 4 * cw - 2, panels[1].y + 32 + ch + 60), host, kLineMode, pal::lines));
	// SMEAR
	knobAt (panels[2].x, panels[2].y, 0, 0, kSmearAmount, pal::smear);
	knobAt (panels[2].x, panels[2].y, 1, 0, kSmearSize, pal::smear);
	knobAt (panels[2].x, panels[2].y, 2, 0, kColorDepth, pal::smear);
	add (new RdChoice (CRect (panels[2].x + 14, panels[2].y + 32 + ch + 8, panels[2].x + 14 + 130, panels[2].y + 32 + ch + 46), host, kSmearMode, pal::smear));
	// SYNC LOSS
	knobAt (panels[3].x, panels[3].y, 0, 0, kLossRate, pal::loss);
	knobAt (panels[3].x, panels[3].y, 1, 0, kLossLength, pal::loss);
	knobAt (panels[3].x, panels[3].y, 2, 0, kLossIntensity, pal::loss);
	add (new RdChoice (CRect (panels[3].x + 14, panels[3].y + 32 + ch + 8, panels[3].x + 14 + 130, panels[3].y + 32 + ch + 46), host, kLossStyle, pal::loss));
	add (new RdToggle (CRect (panels[3].x + 160, panels[3].y + 32 + ch + 22, panels[3].x + 160 + 120, panels[3].y + 32 + ch + 46), host, kLossTrigger, "FIRE NOW", pal::loss, true));

	// ---------------- OUTPUT strip ----------------
	const double outY = midY + midH + 10, outH = kHeight - outY - M;
	add (new RdPanel (CRect (M, outY, kWidth - M, outY + outH), "OUTPUT", "what comes out", pal::out));
	{
		const double y = outY + 26;
		double x = M + 14;
		const int ids[4] = {kBandwidth, kStatic, kDrive, kFieldSplit};
		for (int id : ids)
		{
			add (new RdKnob (CRect (x, y, x + cw, y + ch), host, id, pal::out));
			x += cw + 6;
		}
		const double mx = kWidth - M - 14 - 2 * (cw + 6);
		add (new RdKnob (CRect (mx, y, mx + cw, y + ch), host, kMix, pal::out));
		add (new RdKnob (CRect (mx + cw + 6, y, mx + 2 * cw + 6, y + ch), host, kOutGain, pal::out));
	}
}

void RasterUI::tick (double dt)
{
	for (auto* s : syncables_)
		s->sync (dt);
}

} // namespace ui
} // namespace rd
