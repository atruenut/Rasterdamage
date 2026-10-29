// Raster Damage - user preset files (.rdpreset). No plugin-SDK dependency.
//
// Plain text, one "Parameter Name=value" per line, so presets are human readable, survive
// parameter additions (missing keys fall back to defaults, unknown keys are ignored) and can
// be shared / versioned.
//
//   # Raster Damage preset
//   format=1
//   name=My Preset
//   Slice Time=12
//   Tear Amount=60
//   ...
#pragma once

#include "RasterParams.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace rd {

static const char* const kPresetExtension = ".rdpreset";

// Parameters that are performance/host state, not part of a sound.
inline bool isPresetParam (int id)
{
	return id != kBypass && id != kLossTrigger;
}

inline std::string trim (const std::string& s)
{
	size_t a = 0, b = s.size ();
	while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n'))
		++a;
	while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n'))
		--b;
	return s.substr (a, b - a);
}

inline std::string serializePreset (const std::string& name, const ParamSet& p)
{
	std::ostringstream o;
	o << "# Raster Damage preset\nformat=1\nname=" << name << "\n";
	for (int i = 0; i < kNumParams; ++i)
	{
		if (!isPresetParam (i))
			continue;
		const ParamSpec& s = spec (i);
		char buf[64];
		if (s.kind == Kind::Float)
			std::snprintf (buf, sizeof (buf), "%.6g", p[i]);
		else
			std::snprintf (buf, sizeof (buf), "%d", static_cast<int> (std::lround (p[i])));
		o << s.name << "=" << buf << "\n";
	}
	return o.str ();
}

// Returns false if the text is clearly not a Raster Damage preset.
inline bool parsePreset (const std::string& text, std::string& name, ParamSet& out)
{
	out.setDefaults ();
	name.clear ();
	std::istringstream in (text);
	std::string line;
	bool sawFormat = false, sawParam = false;
	while (std::getline (in, line))
	{
		line = trim (line);
		if (line.empty () || line[0] == '#')
			continue;
		const size_t eq = line.find ('=');
		if (eq == std::string::npos)
			continue;
		const std::string key = trim (line.substr (0, eq));
		const std::string val = trim (line.substr (eq + 1));
		if (key == "format")
		{
			sawFormat = true;
			continue;
		}
		if (key == "name")
		{
			name = val;
			continue;
		}
		for (int i = 0; i < kNumParams; ++i)
		{
			if (!isPresetParam (i) || key != spec (i).name)
				continue;
			char* end = nullptr;
			const double v = std::strtod (val.c_str (), &end);
			if (end != val.c_str () && std::isfinite (v))
			{
				const ParamSpec& s = spec (i);
				out[i] = std::max (s.minV, std::min (s.maxV, s.kind == Kind::Float ? v : std::floor (v + 0.5)));
				sawParam = true;
			}
			break;
		}
	}
	return sawFormat && sawParam;
}

inline std::string sanitizeFileName (const std::string& name)
{
	std::string r;
	for (char c : name)
	{
		const unsigned char u = static_cast<unsigned char> (c);
		if (u < 32 || c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*')
			r.push_back ('_');
		else
			r.push_back (c);
	}
	r = trim (r);
	while (!r.empty () && r.back () == '.')
		r.pop_back ();
	if (r.empty ())
		r = "Untitled";
	if (r.size () > 80)
		r.resize (80);
	return r;
}

inline std::filesystem::path userPresetDir ()
{
	namespace fs = std::filesystem;
	fs::path base;
#if defined(_WIN32)
	if (const wchar_t* up = _wgetenv (L"USERPROFILE"))
		base = fs::path (up) / L"Documents";
#elif defined(__APPLE__)
	if (const char* h = std::getenv ("HOME"))
		base = fs::path (h) / "Library" / "Application Support";
#else
	if (const char* x = std::getenv ("XDG_DATA_HOME"))
		base = fs::path (x);
	else if (const char* h = std::getenv ("HOME"))
		base = fs::path (h) / ".local" / "share";
#endif
	if (base.empty ())
		base = fs::temp_directory_path ();
	return base / "Raster Damage" / "Presets";
}

// Allows tests to redirect the folder.
inline std::filesystem::path& userPresetDirOverride ()
{
	static std::filesystem::path p;
	return p;
}
inline std::filesystem::path presetDir ()
{
	return userPresetDirOverride ().empty () ? userPresetDir () : userPresetDirOverride ();
}

struct UserPresetEntry
{
	std::string name; // display name (file name without extension)
	std::filesystem::path path;
};

inline std::vector<UserPresetEntry> listUserPresets ()
{
	namespace fs = std::filesystem;
	std::vector<UserPresetEntry> r;
	std::error_code ec;
	const fs::path dir = presetDir ();
	if (!fs::is_directory (dir, ec))
		return r;
	for (const auto& e : fs::directory_iterator (dir, ec))
	{
		if (ec)
			break;
		if (e.is_regular_file (ec) && e.path ().extension () == kPresetExtension)
			r.push_back ({e.path ().stem ().string (), e.path ()});
	}
	std::sort (r.begin (), r.end (), [] (const UserPresetEntry& a, const UserPresetEntry& b) {
		std::string x = a.name, y = b.name;
		for (auto& c : x)
			c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
		for (auto& c : y)
			c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
		return x < y;
	});
	return r;
}

inline bool savePresetFile (const std::filesystem::path& path, const std::string& name, const ParamSet& p)
{
	namespace fs = std::filesystem;
	std::error_code ec;
	if (path.has_parent_path ())
		fs::create_directories (path.parent_path (), ec);
	std::ofstream f (path, std::ios::binary | std::ios::trunc);
	if (!f)
		return false;
	f << serializePreset (name, p);
	return static_cast<bool> (f);
}

inline bool loadPresetFile (const std::filesystem::path& path, std::string& name, ParamSet& p)
{
	std::ifstream f (path, std::ios::binary);
	if (!f)
		return false;
	std::string text ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char> ());
	if (text.size () > 1 << 20)
		return false;
	if (!parsePreset (text, name, p))
		return false;
	if (name.empty ())
		name = path.stem ().string ();
	return true;
}

} // namespace rd
