#include "favorites.h"

#include <algorithm>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "common/inifile.h"

// Defined in fileBrowse.cpp
extern std::string recentlyPlayedIniPath;

// '|' cannot appear in FAT file names, so it is a safe separator
static const char DELIM = '|';

static std::string iniPath() {
	std::string p = recentlyPlayedIniPath;
	size_t pos = p.rfind('/');
	if (pos == std::string::npos) {
		return "sd:/_nds/TWiLightMenu/extras/favorites.ini";
	}
	return p.substr(0, pos + 1) + "favorites.ini";
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

std::vector<std::string> favoritesLoad() {
	CIniFile ini(iniPath());
	std::vector<std::string> list;
	ini.GetStringVector("FAVORITES", "LIST", list, DELIM);
	// drop empty entries
	list.erase(std::remove(list.begin(), list.end(), std::string()), list.end());
	return list;
}

bool favoritesIsFav(const std::string &absPath) {
	const std::vector<std::string> list = favoritesLoad();
	return std::find(list.begin(), list.end(), absPath) != list.end();
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
