#include "RasterEditor.h"
#include "RasterPresetIO.h"
#include "RasterPresets.h"

#include "vstgui/lib/cfileselector.h"
#include "vstgui/lib/controls/coptionmenu.h"
#include "vstgui/lib/cvstguitimer.h"

using namespace VSTGUI;
using namespace Steinberg;

namespace rd {

RasterEditor::RasterEditor (RasterController* controller)
: VSTGUIEditor (controller), ctrl_ (controller)
{
	ViewRect r (0, 0, ui::kWidth, ui::kHeight);
	setRect (r);
}

RasterEditor::~RasterEditor ()
{
	*alive_ = false;
}

bool PLUGIN_API RasterEditor::open (void* parent, const PlatformType& platformType)
{
	if (frame)
		return false;
	CRect size (0, 0, ui::kWidth, ui::kHeight);
	frame = new CFrame (size, this);
	frame->setBackgroundColor (CColor (17, 19, 25));
	if (!frame->open (parent, platformType))
	{
		frame->forget (); // never leave a half-opened frame behind
		frame = nullptr;
		return false;
	}
	ui_ = new ui::RasterUI (this);
	frame->addView (ui_);
	timer_ = makeOwned<CVSTGUITimer> ([this] (CVSTGUITimer*) {
		try { if (ui_) ui_->tick (0.033); } catch (...) {}
	}, 33, true);
	return true;
}

void PLUGIN_API RasterEditor::close ()
{
	*alive_ = false; // pending menu / dialog callbacks become no-ops
	alive_ = std::make_shared<bool> (true);
	timer_ = nullptr; // stop polling before the views go away
	ui_ = nullptr;
	if (frame)
	{
		frame->close (); // removes the views and releases the frame
		frame = nullptr;
	}
}

//------------------------------------------------------------------------
double RasterEditor::getParam (int id) { return ctrl_->getParamNormalized (static_cast<Vst::ParamID> (id)); }
void RasterEditor::editBegin (int id) { ctrl_->beginEdit (static_cast<Vst::ParamID> (id)); }
void RasterEditor::editSet (int id, double n)
{
	ctrl_->setParamNormalized (static_cast<Vst::ParamID> (id), n);
	ctrl_->performEdit (static_cast<Vst::ParamID> (id), n);
}
void RasterEditor::editEnd (int id) { ctrl_->endEdit (static_cast<Vst::ParamID> (id)); }
ParamSet RasterEditor::params () { return ctrl_->currentParams (); }
std::string RasterEditor::presetName () { return ctrl_->presetName (); }
bool RasterEditor::presetModified () { return ctrl_->presetModified (); }
int RasterEditor::presetRevision () { return ctrl_->presetRevision (); }

//------------------------------------------------------------------------
void RasterEditor::presetMenu (CFrame* f, const CPoint& where)
{
	if (!f)
		return;
	try {
	const auto alive = alive_;
	const int style = COptionMenu::kPopupStyle | COptionMenu::kCheckStyle;
	auto menu = makeOwned<COptionMenu> (CRect (0, 0, 10, 10), nullptr, -1, nullptr, nullptr, style);
	const int current = ctrl_->factoryIndex ();

	for (int c = 0; c < kNumPresetCategories; ++c)
	{
		auto sub = makeOwned<COptionMenu> (CRect (0, 0, 10, 10), nullptr, -1, nullptr, nullptr, style);
		const int first = kPresetCategories[c].first;
		const int last = c + 1 < kNumPresetCategories ? kPresetCategories[c + 1].first : kNumPresets;
		for (int i = first; i < last; ++i)
		{
			auto* item = new CCommandMenuItem (CCommandMenuItem::Desc (kPresets[i].name));
			item->setActions ([this, i, alive] (CCommandMenuItem*) { if (*alive) ctrl_->selectFactoryPreset (i); });
			item->setChecked (i == current);
			sub->addEntry (item);
		}
		menu->addEntry (sub, kPresetCategories[c].name);
	}
	menu->addSeparator ();
	menu->addEntry ("My presets", -1, CMenuItem::kTitle);
	const auto user = listUserPresets ();
	if (user.empty ())
		menu->addEntry ("(none yet - use Save)", -1, CMenuItem::kDisabled);
	for (const auto& u : user)
	{
		auto* item = new CCommandMenuItem (CCommandMenuItem::Desc (u.name.c_str ()));
		const auto path = u.path;
		item->setActions ([this, path, alive] (CCommandMenuItem*) { if (*alive) ctrl_->loadUserPreset (path); });
		item->setChecked (current < 0 && ctrl_->presetName () == u.name && !ctrl_->presetModified ());
		menu->addEntry (item);
	}
	menu->popup (f, where, [menu] (COptionMenu*) {});
	} catch (...) {}
}

void RasterEditor::presetStep (int dir)
{
	try {
	const auto user = listUserPresets ();
	const int total = kNumPresets + static_cast<int> (user.size ());
	int cur = ctrl_->factoryIndex ();
	if (cur < 0)
		for (size_t i = 0; i < user.size (); ++i)
			if (user[i].name == ctrl_->presetName ())
				cur = kNumPresets + static_cast<int> (i);
	int next = cur < 0 ? (dir > 0 ? 0 : total - 1) : (cur + dir + total) % total;
	if (next < kNumPresets)
		ctrl_->selectFactoryPreset (next);
	else
		ctrl_->loadUserPreset (user[static_cast<size_t> (next - kNumPresets)].path);
	} catch (...) {}
}

void RasterEditor::presetSave (CFrame* f)
{
	try {
	const auto alive = alive_;
	namespace fs = std::filesystem;
	std::error_code ec;
	fs::create_directories (presetDir (), ec);
	if (auto* sel = CNewFileSelector::create (f, CNewFileSelector::kSelectSaveFile))
	{
		sel->setTitle ("Save Raster Damage preset");
		sel->setInitialDirectory (presetDir ().string ().c_str ());
		sel->setDefaultSaveName ((sanitizeFileName (ctrl_->presetName ()) + kPresetExtension).c_str ());
		sel->setDefaultExtension (CFileExtension ("Raster Damage preset", "rdpreset"));
		sel->run ([this, alive] (CNewFileSelector* s) {
			if (!*alive || s->getNumSelectedFiles () < 1)
				return;
			try {
			fs::path path = fs::u8path (s->getSelectedFile (0));
			if (path.extension () != kPresetExtension)
				path += kPresetExtension;
			ctrl_->saveUserPreset (path, path.stem ().string ());
			} catch (...) {}
		});
		sel->forget ();
	}
	} catch (...) {}
}

void RasterEditor::presetLoad (CFrame* f)
{
	try {
	const auto alive = alive_;
	namespace fs = std::filesystem;
	std::error_code ec;
	fs::create_directories (presetDir (), ec);
	if (auto* sel = CNewFileSelector::create (f, CNewFileSelector::kSelectFile))
	{
		sel->setTitle ("Load Raster Damage preset");
		sel->setInitialDirectory (presetDir ().string ().c_str ());
		sel->addFileExtension (CFileExtension ("Raster Damage preset", "rdpreset"));
		sel->run ([this, alive] (CNewFileSelector* s) {
			try {
				if (*alive && s->getNumSelectedFiles () > 0)
					ctrl_->loadUserPreset (fs::u8path (s->getSelectedFile (0)));
			} catch (...) {}
		});
		sel->forget ();
	}
	} catch (...) {}
}

void RasterEditor::presetInit ()
{
	ctrl_->applyParams (ParamSet (), "Default", -1, true);
}

} // namespace rd
