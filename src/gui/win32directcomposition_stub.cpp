// MinGW build: DirectComposition (optional, only used for layered views / GPU-composited child
// views) needs newer headers than MinGW ships. VSTGUI falls back to plain Direct2D when the
// factory is null, which is all Raster Damage needs.
#include "vstgui/lib/platform/win32/win32directcomposition.h"

namespace VSTGUI {
namespace DirectComposition {

struct Factory::Impl
{
};

std::unique_ptr<Factory> Factory::create (IUnknown*) { return nullptr; }
Factory::Factory () = default;
Factory::~Factory () noexcept = default;
bool Factory::enableVisualizeRedrawAreas (bool) { return false; }
bool Factory::isVisualRedrawAreasEnabled () const { return false; }
VisualPtr Factory::createVisualForHWND (HWND) { return nullptr; }
VisualPtr Factory::createChildVisual (const VisualPtr&, uint32_t, uint32_t) { return nullptr; }
bool Factory::removeVisual (const VisualPtr&) { return false; }
ID2D1Device* Factory::getDevice () const { return nullptr; }

} // namespace DirectComposition
} // namespace VSTGUI
