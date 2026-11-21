#include "favorites.h"

#include <algorithm>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "common/inifile.h"

#include "common/systemdetails.h"

// '|' cannot appear in FAT file names, so it is a safe separator
static const char DELIM = '|';

static std::string iniPath() {
	return std::string(sys().isRunFromSD() ? "sd" : "fat") + ":/_nds/TWiLightMenu/extras/favorites.ini";
}

void favoritesSetReturnDir(const std::string &dir) {
	const std::string path = iniPath();
	CIniFile ini(path);
	ini.SetString("FAVORITES", "RETURN_DIR", dir);
	ini.SaveIniFile(path);
}

std::string favoritesTakeReturnDir() {
	const std::string path = iniPath();
	CIniFile ini(path);
	std::string dir = ini.GetString("FAVORITES", "RETURN_DIR", "");
	if (!dir.empty()) {
		ini.SetString("FAVORITES", "RETURN_DIR", "");
		ini.SaveIniFile(path);
	}
	return dir;
}

static bool fileExists(const std::string &p) {
	return access(p.c_str(), F_OK) == 0;
}

std::string favoritesCwd() {
	char buf[PATH_MAX] = {0};
	getcwd(buf, sizeof(buf));
	std::string s(buf);
	if (s.empty() || s.back() != '/') s += '/';
	return s;
}

// The list is cached in RAM so drawing titles doesn't read the SD card every time
static std::vector<std::string> g_favCache;
static bool g_favCacheLoaded = false;
static int g_favVersion = 0;

std::vector<std::string> favoritesLoad() {
	if (!g_favCacheLoaded) {
		CIniFile ini(iniPath());
		std::vector<std::string> list;
		ini.GetStringVector("FAVORITES", "LIST", list, DELIM);
		// drop empty entries
		list.erase(std::remove(list.begin(), list.end(), std::string()), list.end());
		g_favCache = list;
		g_favCacheLoaded = true;
	}
	return g_favCache;
}

bool favoritesIsFav(const std::string &absPath) {
	favoritesLoad();
	return std::find(g_favCache.begin(), g_favCache.end(), absPath) != g_favCache.end();
}

bool favoritesIsFavEntry(const std::string &name) {
	// name is relative to the current folder ("Game.nds", or "A/Game.nds" for
	// favorites listed in the root folder)
	return favoritesIsFav(favoritesCwd() + name);
}

int favoritesChangeCount() {
	return g_favVersion;
}

void favoritesSet(const std::string &absPath, bool fav) {
	const std::string path = iniPath();
	CIniFile ini(path);
	std::vector<std::string> list;
	ini.GetStringVector("FAVORITES", "LIST", list, DELIM);
	list.erase(std::remove(list.begin(), list.end(), std::string()), list.end());

	auto it = std::find(list.begin(), list.end(), absPath);
	if (fav && it == list.end()) {
		list.push_back(absPath);
	} else if (!fav && it != list.end()) {
		list.erase(it);
	} else {
		return; // nothing to change
	}
	ini.SetStringVector("FAVORITES", "LIST", list, DELIM);
	ini.SaveIniFile(path);
	g_favCache = list;
	g_favCacheLoaded = true;
	g_favVersion++;
}

std::string favoritesRootDir() {
	CIniFile ini(iniPath());
	std::string root = ini.GetString("FAVORITES", "ROOT", "");
	if (root.empty()) {
		// Default: the very top of the current device, e.g. "sd:/" or "fat:/"
		const std::string cwd = favoritesCwd();
		size_t colon = cwd.find(':');
		std::string dev = (colon == std::string::npos) ? "sd:" : cwd.substr(0, colon + 1);
		root = dev + "/";
	}
	if (root.back() != '/') root += '/';
	return root;
}

std::vector<std::string> favoritesForCwd() {
	std::vector<std::string> out;
	const std::string cwd = favoritesCwd();
	if (strcasecmp(cwd.c_str(), favoritesRootDir().c_str()) != 0) {
		return out;
	}
	for (const std::string &abs : favoritesLoad()) {
		if (abs.size() <= cwd.size()) continue;
		if (strncasecmp(abs.c_str(), cwd.c_str(), cwd.size()) != 0) continue;
		const std::string rel = abs.substr(cwd.size());
		// Only games that live in a sub-folder (games directly in root are already listed)
		if (rel.find('/') == std::string::npos) continue;
		if (!fileExists(abs)) continue; // skip deleted files
		out.push_back(rel);
	}
	return out;
}

// ---- Recent ----
static const size_t RECENT_MAX = 13;

static std::string recentIniPath() {
	return std::string(sys().isRunFromSD() ? "sd" : "fat") + ":/_nds/TWiLightMenu/extras/recent.ini";
}

bool recentIsRootCwd() {
	return strcasecmp(favoritesCwd().c_str(), favoritesRootDir().c_str()) == 0;
}

bool recentIsCwd() {
	return strcasecmp(favoritesCwd().c_str(), (favoritesRootDir() + "Recent/").c_str()) == 0;
}

void recentAdd(const std::string &cwd, const std::string &name) {
	if (name.empty()) return;
	std::string dir = cwd;
	if (dir.empty() || dir.back() != '/') dir += '/';
	// Never record from inside the Recent folder itself (entries there are absolute paths)
	if (strcasecmp(dir.c_str(), (favoritesRootDir() + "Recent/").c_str()) == 0) return;
	const std::string abs = (name.find('/') != std::string::npos && name.find(':') != std::string::npos) ? name : dir + name;

	const std::string path = recentIniPath();
	CIniFile ini(path);
	std::vector<std::string> list;
	ini.GetStringVector("RECENT", "LIST", list, DELIM);
	list.erase(std::remove(list.begin(), list.end(), std::string()), list.end());
	list.erase(std::remove_if(list.begin(), list.end(), [&](const std::string &s) { return strcasecmp(s.c_str(), abs.c_str()) == 0; }), list.end());
	list.insert(list.begin(), abs);
	if (list.size() > RECENT_MAX) list.resize(RECENT_MAX);
	ini.SetStringVector("RECENT", "LIST", list, DELIM);
	ini.SaveIniFile(path);
}

std::vector<std::string> recentList() {
	const std::string path = recentIniPath();
	CIniFile ini(path);
	std::vector<std::string> list;
	ini.GetStringVector("RECENT", "LIST", list, DELIM);
	list.erase(std::remove(list.begin(), list.end(), std::string()), list.end());

	// Same device as the current folder only (e.g. "sd:" or "fat:")
	const std::string cwd = favoritesCwd();
	const size_t colon = cwd.find(':');
	const std::string dev = (colon == std::string::npos) ? std::string() : cwd.substr(0, colon + 1);

	std::vector<std::string> out;
	bool pruned = false;
	for (const std::string &abs : list) {
		const size_t c = abs.find(':');
		const std::string d = (c == std::string::npos) ? std::string() : abs.substr(0, c + 1);
		if (strcasecmp(d.c_str(), dev.c_str()) != 0) { out.push_back(abs); continue; } // other device: keep, but do not list
		if (!fileExists(abs)) { pruned = true; continue; } // deleted or renamed: drop it
		out.push_back(abs);
	}
	if (pruned) {
		ini.SetStringVector("RECENT", "LIST", out, DELIM);
		ini.SaveIniFile(path);
	}
	std::vector<std::string> shown;
	for (const std::string &abs : out) {
		const size_t c = abs.find(':');
		const std::string d = (c == std::string::npos) ? std::string() : abs.substr(0, c + 1);
		if (strcasecmp(d.c_str(), dev.c_str()) == 0) shown.push_back(abs);
	}
	return shown;
}
