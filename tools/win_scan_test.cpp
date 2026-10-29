// Emulates a host plugin scan: load, enumerate classes, instantiate, terminate, unload - twice, with and without InitDll.
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include <windows.h>
#include <cstdio>
using namespace Steinberg; using namespace Steinberg::Vst;
int main(int, char** argv)
{
	setvbuf(stdout, nullptr, _IONBF, 0);
	for (int pass = 0; pass < 3; ++pass)
	{
		HMODULE m = LoadLibraryA(argv[1]);
		if (!m) { printf("pass %d LOAD FAIL %lu\n", pass, GetLastError()); return 1; }
		auto init = (bool(*)())GetProcAddress(m, "InitDll");
		auto fac = (IPluginFactory*(PLUGIN_API*)())GetProcAddress(m, "GetPluginFactory");
		auto ex = (bool(*)())GetProcAddress(m, "ExitDll");
		if (pass != 1 && init) init();
		IPluginFactory* f = fac();
		PFactoryInfo fi; f->getFactoryInfo(&fi);
		for (int i = 0; i < f->countClasses(); ++i)
		{
			PClassInfo ci; f->getClassInfo(i, &ci);
			printf("pass %d class %d: %s / %s\n", pass, i, ci.name, ci.category);
			FUnknown* o = nullptr;
			if (f->createInstance(ci.cid, FUnknown::iid, (void**)&o) == kResultOk && o)
			{
				IPluginBase* pb = nullptr;
				if (o->queryInterface(IPluginBase::iid, (void**)&pb) == kResultOk) { pb->initialize(nullptr); pb->terminate(); pb->release(); }
				o->release();
			}
		}
		f->release();
		if (pass != 1 && ex) ex();
		FreeLibrary(m);
		printf("pass %d ok\n", pass);
	}
	return 0;
}
