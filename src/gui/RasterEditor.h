// Raster Damage - VST3 editor (VSTGUI): hosts the UI and connects it to the controller.
#pragma once

#include <memory>

#include "RasterController.h"
#include "RasterUI.h"
#include "public.sdk/source/vst/vstguieditor.h"

namespace rd {

class RasterEditor : public Steinberg::Vst::VSTGUIEditor, public ui::Host
{
public:
	explicit RasterEditor (RasterController* controller);
	~RasterEditor () override;

	bool PLUGIN_API open (void* parent, const VSTGUI::PlatformType& platformType) SMTG_OVERRIDE;
	void PLUGIN_API close () SMTG_OVERRIDE;

	// ui::Host
	double getParam (int id) override;
	void editBegin (int id) override;
	void editSet (int id, double normalized) override;
	void editEnd (int id) override;
	ParamSet params () override;
	std::string presetName () override;
	bool presetModified () override;
	int presetRevision () override;
	void presetMenu (VSTGUI::CFrame* frame, const VSTGUI::CPoint& frameLocation) override;
	void presetStep (int direction) override;
	void presetSave (VSTGUI::CFrame* frame) override;
	void presetLoad (VSTGUI::CFrame* frame) override;
	void presetInit () override;

private:
	RasterController* ctrl_;
	VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> timer_;
	ui::RasterUI* ui_ = nullptr;
	// Menus and file dialogs finish asynchronously; their callbacks check this so they never touch
	// an editor that was closed (or destroyed) while the menu/dialog was open.
	std::shared_ptr<bool> alive_ = std::make_shared<bool> (true);
};

} // namespace rd
