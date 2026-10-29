// Renders the Raster Damage UI offscreen to a PNG (Linux, uses VSTGUI's cairo backend).
//   rd_gui_shot out.png [preset index or name]
#include "RasterPresets.h"
#include "gui/RasterUI.h"

#include "vstgui/lib/coffscreencontext.h"
#include "vstgui/lib/vstguiinit.h"
#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/platform/iplatformbitmap.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>

using namespace VSTGUI;

struct FakeHost : rd::ui::Host
{
	rd::ParamSet p;
	std::string name = "Default";
	double getParam (int id) override { return rd::toNormalized (rd::spec (id), p[id]); }
	void editBegin (int) override {}
	void editSet (int id, double n) override { p[id] = rd::toPlain (rd::spec (id), n); }
	void editEnd (int) override {}
	rd::ParamSet params () override { return p; }
	std::string presetName () override { return name; }
	bool presetModified () override { return false; }
	int presetRevision () override { return 1; }
	void presetMenu (CFrame*, const CPoint&) override {}
	void presetStep (int) override {}
	void presetSave (CFrame*) override {}
	void presetLoad (CFrame*) override {}
	void presetInit () override {}
};

int main (int argc, char** argv)
{
	const char* out = argc > 1 ? argv[1] : "shot.png";
	FakeHost host;
	if (argc > 2)
	{
		int idx = std::atoi (argv[2]);
		if (idx == 0 && argv[2][0] != '0')
			for (int i = 0; i < rd::kNumPresets; ++i)
				if (std::string (rd::kPresets[i].name).find (argv[2]) != std::string::npos)
					idx = i;
		rd::loadPreset (idx, host.p);
		host.name = rd::kPresets[idx].name;
	}
	VSTGUI::init (nullptr);
	auto ui = owned (new rd::ui::RasterUI (&host));
	ui->tick (0.0);
	ui->tick (3.2); // advance the raster a bit so it shows a mid-stream picture
	auto ctx = COffscreenContext::create (CPoint (rd::ui::kWidth, rd::ui::kHeight), 1.0);
	if (!ctx)
	{
		std::printf ("no offscreen context\n");
		return 2;
	}
	ctx->beginDraw ();
	ui->drawRect (ctx, CRect (0, 0, rd::ui::kWidth, rd::ui::kHeight));
	ctx->endDraw ();
	auto bmp = ctx->getBitmap ();
	if (!bmp || !bmp->getPlatformBitmap ())
	{
		std::printf ("no bitmap\n");
		return 3;
	}
	auto px = bmp->getPlatformBitmap ()->lockPixels (false);
	if (!px)
	{
		std::printf ("cannot read pixels\n");
		return 4;
	}
	const int W = rd::ui::kWidth, H = rd::ui::kHeight;
	std::ofstream f (out, std::ios::binary);
	f << "P6\n" << W << " " << H << "\n255\n";
	for (int y = 0; y < H; ++y)
	{
		const uint8_t* row = px->getAddress () + static_cast<size_t> (y) * px->getBytesPerRow ();
		for (int x = 0; x < W; ++x)
		{
			const uint8_t* q = row + x * 4;
			uint8_t rgb[3];
			switch (px->getPixelFormat ())
			{
				case IPlatformBitmapPixelAccess::kARGB: rgb[0] = q[1]; rgb[1] = q[2]; rgb[2] = q[3]; break;
				case IPlatformBitmapPixelAccess::kRGBA: rgb[0] = q[0]; rgb[1] = q[1]; rgb[2] = q[2]; break;
				case IPlatformBitmapPixelAccess::kBGRA: rgb[0] = q[2]; rgb[1] = q[1]; rgb[2] = q[0]; break;
				default: rgb[0] = q[1]; rgb[1] = q[2]; rgb[2] = q[3]; break;
			}
			f.write (reinterpret_cast<const char*> (rgb), 3);
		}
	}
	std::printf ("wrote %s\n", out);
	ui = nullptr;
	return 0;
}
