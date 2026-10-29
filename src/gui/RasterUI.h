// Raster Damage - GUI (VSTGUI). Independent of the VST3 plumbing: everything the UI needs from
// the plugin goes through ui::Host, so the same views are used by the real editor and by the
// offscreen screenshot tool.
#pragma once

#include "RasterParams.h"
#include "vstgui/vstgui.h"

#include <string>

namespace rd {
namespace ui {

static const int kWidth = 1000;
static const int kHeight = 676;

class Host
{
public:
	virtual ~Host () = default;
	// parameters (normalized 0..1)
	virtual double getParam (int id) = 0;
	virtual void editBegin (int id) = 0;
	virtual void editSet (int id, double normalized) = 0;
	virtual void editEnd (int id) = 0;
	virtual ParamSet params () = 0;
	// presets
	virtual std::string presetName () = 0;
	virtual bool presetModified () = 0;
	virtual int presetRevision () = 0;
	virtual void presetMenu (VSTGUI::CFrame* frame, const VSTGUI::CPoint& frameLocation) = 0;
	virtual void presetStep (int direction) = 0;
	virtual void presetSave (VSTGUI::CFrame* frame) = 0;
	virtual void presetLoad (VSTGUI::CFrame* frame) = 0;
	virtual void presetInit () = 0;
};

struct Syncable
{
	virtual ~Syncable () = default;
	virtual void sync (double dtSeconds) = 0;
};

class RasterUI : public VSTGUI::CViewContainer
{
public:
	explicit RasterUI (Host* host);
	// call ~30x per second: pulls parameter values / preset name, animates the raster display
	void tick (double dtSeconds);

private:
	std::vector<Syncable*> syncables_;
};

} // namespace ui
} // namespace rd
