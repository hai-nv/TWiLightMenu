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

const std::vector<std::string> &favoritesLoad() {
	if (!g_favCacheLoaded) {
		CIniFile ini(iniPath());
		std::vector<std::string> list;
		ini.GetStringVector("FAVORITES", "LIST", list, DELIM);
		// drop empty entries
		list.erase(std::remove(list.begin(), list.end(), std::string()), list.end());
		g_favCache = std::move(list);
		g_favCacheLoaded = true;
	}
	return g_favCache; // by reference: this is called for every icon that gets drawn
}

bool favoritesIsFav(const std::string &absPath) {
	const std::vector<std::string> &list = favoritesLoad();
	return std::find(list.begin(), list.end(), absPath) != list.end();
}

bool favoritesIsFavEntry(const std::string &name) {
	// name is relative to the current folder ("Game.nds", or "../A/Game.nds" for
	// favorites listed in the Favorites folder)
	std::string cwd = favoritesCwd();
	std::string rel = name;
	while (rel.compare(0, 3, "../") == 0) {
		rel = rel.substr(3);
		if (cwd.size() > 1) {
			cwd.pop_back(); // trailing slash
			const size_t cut = cwd.rfind('/');
			cwd = (cut == std::string::npos) ? std::string() : cwd.substr(0, cut + 1);
		}
	}
	return favoritesIsFav(cwd + rel);
}

int favoritesChangeCount() {
	return g_favVersion;
}

void favoritesSet(const std::string &absPath, bool fav) {
	// The cache is always up to date (only this function changes the list), so decide from
	// it and touch the SD card only when something really changes
	std::vector<std::string> list = favoritesLoad();

	auto it = std::find(list.begin(), list.end(), absPath);
	if (fav && it == list.end()) {
		list.push_back(absPath);
	} else if (!fav && it != list.end()) {
		list.erase(it);
	} else {
		return; // nothing to change
	}
	const std::string path = iniPath();
	CIniFile ini(path); // loaded again so the other keys (RETURN_DIR, ROOT) are kept
	ini.SetStringVector("FAVORITES", "LIST", list, DELIM);
	ini.SaveIniFile(path);
	g_favCache = std::move(list);
	g_favCacheLoaded = true;
	g_favVersion++;
}

// Name of an entry listed in the Favorites folder -> absolute path
static std::string favoritesAbsFromEntry(const std::string &name) {
	std::string cwd = favoritesCwd();
	std::string rel = name;
	while (rel.compare(0, 3, "../") == 0) {
		rel = rel.substr(3);
		if (cwd.size() > 1) {
			cwd.pop_back();
			const size_t cut = cwd.rfind('/');
			cwd = (cut == std::string::npos) ? std::string() : cwd.substr(0, cut + 1);
		}
	}
	return cwd + rel;
}

void favoritesForgetMissing(const std::string &name) {
	if (name.compare(0, 3, "../") != 0 || !favoritesIsFavFolderCwd()) return;
	const std::string abs = favoritesAbsFromEntry(name);
	if (fileExists(abs)) return; // the open failed for another reason: keep the favorite
	favoritesSet(abs, false);
}

std::string favoritesRootDir() {
	// ROOT= never changes while the menu runs, so read the ini only once (this is called
	// several times for every folder that gets opened)
	static std::string iniRoot;
	static bool iniRootRead = false;
	if (!iniRootRead) {
		CIniFile ini(iniPath());
		iniRoot = ini.GetString("FAVORITES", "ROOT", "");
		iniRootRead = true;
	}
	std::string root = iniRoot;
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

bool favoritesIsFavFolderCwd() {
	return strcasecmp(favoritesCwd().c_str(), (favoritesRootDir() + "Favorites/").c_str()) == 0;
}

std::vector<std::string> favoritesForCwd() {
	std::vector<std::string> out;
	const std::string cwd = favoritesCwd();
	const std::string root = favoritesRootDir();
	// Favorites are listed inside the "Favorites" folder (a real, empty folder in the root)
	if (strcasecmp(cwd.c_str(), (root + "Favorites/").c_str()) != 0) {
		return out;
	}
	for (const std::string &abs : favoritesLoad()) {
		if (abs.size() <= root.size()) continue;
		if (strncasecmp(abs.c_str(), root.c_str(), root.size()) != 0) continue;
		// No existence check here: that was one slow SD lookup per favorite, and the banner
		// loader opens every file anyway. A file that is gone is dropped from the list there
		// (favoritesForgetMissing), so it disappears the next time the folder is opened.
		// Paths relative to the Favorites folder, e.g. "../A/Game.nds"
		out.push_back("../" + abs.substr(root.size()));
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
	return strcasecmp(favoritesCwd().c_str(), (favoritesRootDir() + "Recently Played/").c_str()) == 0;
}

void recentAdd(const std::string &cwd, const std::string &name) {
	if (name.empty()) return;
	std::string dir = cwd;
	if (dir.empty() || dir.back() != '/') dir += '/';
	// Never record from inside the Recent folder itself (entries there are absolute paths)
	if (strcasecmp(dir.c_str(), (favoritesRootDir() + "Recently Played/").c_str()) == 0) return;
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
