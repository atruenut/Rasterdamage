// Minimal Windows VST3 host used to smoke-test the real RasterDamage.vst3 DLL (runs under Wine):
// loads the DLL, creates the controller + editor in a window, switches preset, saves screenshots.
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include <windows.h>
#include <cstdio>
#include <string>

using namespace Steinberg;
using namespace Steinberg::Vst;

static const TUID kControllerCID = INLINE_UID (0x9F0D2B64, 0x4C1A4E37, 0xA6B58D13, 0x2E7C80F1);

static HWND g_hwnd = nullptr;
static int g_performs = 0;

struct Handler : IComponentHandler
{
	tresult PLUGIN_API beginEdit (ParamID) SMTG_OVERRIDE { return kResultOk; }
	tresult PLUGIN_API performEdit (ParamID, ParamValue) SMTG_OVERRIDE { ++g_performs; return kResultOk; }
	tresult PLUGIN_API endEdit (ParamID) SMTG_OVERRIDE { return kResultOk; }
	tresult PLUGIN_API restartComponent (int32) SMTG_OVERRIDE { return kResultOk; }
	tresult PLUGIN_API queryInterface (const TUID iid, void** obj) SMTG_OVERRIDE
	{
		if (FUnknownPrivate::iidEqual (iid, IComponentHandler::iid) || FUnknownPrivate::iidEqual (iid, FUnknown::iid))
		{
			*obj = this;
			return kResultOk;
		}
		*obj = nullptr;
		return kNoInterface;
	}
	uint32 PLUGIN_API addRef () SMTG_OVERRIDE { return 1; }
	uint32 PLUGIN_API release () SMTG_OVERRIDE { return 1; }
};

struct Frame : IPlugFrame
{
	tresult PLUGIN_API resizeView (IPlugView* view, ViewRect* r) SMTG_OVERRIDE
	{
		RECT rc = {0, 0, r->getWidth (), r->getHeight ()};
		AdjustWindowRect (&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE);
		SetWindowPos (g_hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
		view->onSize (r);
		return kResultOk;
	}
	tresult PLUGIN_API queryInterface (const TUID iid, void** obj) SMTG_OVERRIDE
	{
		if (FUnknownPrivate::iidEqual (iid, IPlugFrame::iid) || FUnknownPrivate::iidEqual (iid, FUnknown::iid))
		{
			*obj = this;
			return kResultOk;
		}
		*obj = nullptr;
		return kNoInterface;
	}
	uint32 PLUGIN_API addRef () SMTG_OVERRIDE { return 1; }
	uint32 PLUGIN_API release () SMTG_OVERRIDE { return 1; }
};

static LRESULT CALLBACK wndProc (HWND h, UINT m, WPARAM w, LPARAM l)
{
	if (m == WM_DESTROY)
		PostQuitMessage (0);
	return DefWindowProc (h, m, w, l);
}

static void pump (int ms)
{
	const DWORD end = GetTickCount () + ms;
	while (GetTickCount () < end)
	{
		MSG msg;
		for (int n = 0; n < 8 && PeekMessage (&msg, nullptr, 0, 0, PM_REMOVE); ++n)
		{
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}
		Sleep (10);
	}
}

static bool shot (const char* path)
{
	RECT rc;
	GetClientRect (g_hwnd, &rc);
	const int W = rc.right, H = rc.bottom;
	HDC wdc = GetDC (g_hwnd);
	HDC mdc = CreateCompatibleDC (wdc);
	BITMAPINFO bi = {};
	bi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = W;
	bi.bmiHeader.biHeight = -H;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 24;
	void* bits = nullptr;
	HBITMAP bmp = CreateDIBSection (wdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
	SelectObject (mdc, bmp);
	BitBlt (mdc, 0, 0, W, H, wdc, 0, 0, SRCCOPY | CAPTUREBLT);
	FILE* f = std::fopen (path, "wb");
	if (!f)
		return false;
	std::fprintf (f, "P6\n%d %d\n255\n", W, H);
	const int stride = ((W * 3 + 3) / 4) * 4;
	for (int y = 0; y < H; ++y)
		for (int x = 0; x < W; ++x)
		{
			const unsigned char* p = static_cast<unsigned char*> (bits) + y * stride + x * 3;
			unsigned char rgb[3] = {p[2], p[1], p[0]};
			std::fwrite (rgb, 1, 3, f);
		}
	std::fclose (f);
	DeleteObject (bmp);
	DeleteDC (mdc);
	ReleaseDC (g_hwnd, wdc);
	return true;
}

int main (int argc, char** argv)
{
	setvbuf (stdout, nullptr, _IONBF, 0);
	OleInitialize (nullptr);
	const char* dll = argc > 1 ? argv[1] : "RasterDamage.vst3";
	const std::string out = argc > 2 ? argv[2] : "win";
	HMODULE mod = LoadLibraryA (dll);
	if (!mod)
	{
		std::printf ("LOAD FAILED (error %lu)\n", GetLastError ());
		return 1;
	}
	auto initDll = reinterpret_cast<bool (*) ()> (GetProcAddress (mod, "InitDll"));
	auto getFactory = reinterpret_cast<IPluginFactory* (PLUGIN_API*) ()> (GetProcAddress (mod, "GetPluginFactory"));
	if (!getFactory)
	{
		std::printf ("no GetPluginFactory\n");
		return 2;
	}
	if (initDll)
		initDll ();
	IPluginFactory* factory = getFactory ();
	std::printf ("factory ok, %d classes\n", factory->countClasses ());
	IEditController* ctrl = nullptr;
	if (factory->createInstance (kControllerCID, IEditController::iid, reinterpret_cast<void**> (&ctrl)) != kResultOk || !ctrl)
	{
		std::printf ("cannot create controller\n");
		return 3;
	}
	ctrl->initialize (nullptr);
	Handler handler;
	ctrl->setComponentHandler (&handler);
	std::printf ("parameters: %d\n", ctrl->getParameterCount ());

	WNDCLASSA wc = {};
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandle (nullptr);
	wc.lpszClassName = "rdhost";
	wc.hbrBackground = reinterpret_cast<HBRUSH> (COLOR_WINDOW + 1);
	RegisterClassA (&wc);
	g_hwnd = CreateWindowA ("rdhost", "host", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, 0, 0, 1010, 720, nullptr, nullptr, wc.hInstance, nullptr);
	ShowWindow (g_hwnd, SW_SHOW);

	IPlugView* view = ctrl->createView (ViewType::kEditor);
	if (!view)
	{
		std::printf ("NO EDITOR VIEW\n");
		return 4;
	}
	std::printf ("platform HWND supported: %d\n", view->isPlatformTypeSupported (kPlatformTypeHWND) == kResultTrue);
	Frame frame;
	view->setFrame (&frame);
	const tresult at = view->attached (g_hwnd, kPlatformTypeHWND);
	ViewRect vr;
	view->getSize (&vr);
	std::printf ("attached: %d size %dx%d\n", at == kResultOk, vr.getWidth (), vr.getHeight ());
	std::printf ("pumping\n"); pump (1500); std::printf ("pumped\n");
	shot ((out + "_1.ppm").c_str ());

	// the host switches preset through its own program list -> UI + parameters must follow
	g_performs = 0;
	ctrl->setParamNormalized (1000, 22.0 / 46.0);
	pump (800);
	std::printf ("performEdit calls after preset change: %d\n", g_performs);
	shot ((out + "_2.ppm").c_str ());

	view->removed ();
	view->setFrame (nullptr);
	view->release ();
	ctrl->terminate ();
	ctrl->release ();
	if (auto exitDll = reinterpret_cast<bool (*) ()> (GetProcAddress (mod, "ExitDll")))
		exitDll ();
	std::printf ("clean shutdown\n");
	return 0;
}
