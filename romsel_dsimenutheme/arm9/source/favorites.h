#ifndef FAVORITES_H
#define FAVORITES_H

#include <string>
#include <vector>

// Favorites: games stay in their folders, but favorited games are ALSO listed
// in the "root" folder (default: top of the device, e.g. sd:/, editable via ROOT= in
// favorites.ini). Entries are stored as absolute paths.

// All stored favorites (absolute paths, e.g. "sd:/roms/nds/A/Game.nds")
std::vector<std::string> favoritesLoad();

bool favoritesIsFav(const std::string &absPath);
// Is the entry (name relative to the current folder) a favorite?
bool favoritesIsFavEntry(const std::string &name);
// Increases every time the favorites list changes
int favoritesChangeCount();
void favoritesSet(const std::string &absPath, bool fav);

// One-shot "return here after the game exits" folder (used when a game is
// launched from a favorite entry in the root folder).
void favoritesSetReturnDir(const std::string &dir);
std::string favoritesTakeReturnDir(); // returns and clears it ("" if none)

// Current working directory with trailing slash
std::string favoritesCwd();

// Root folder where favorites are shown (with trailing slash)
std::string favoritesRootDir();

// For the current working directory: return the favorites to inject, as paths
// relative to the cwd (e.g. "A/Game.nds"). Empty unless cwd is the root.
std::vector<std::string> favoritesForCwd();

// ---- Recent: the last games launched from any folder ----
// A real (empty) "Recent" folder lives in the root folder; its contents are
// injected here. Entries are absolute paths, newest first.

// Is the current working directory the Recent folder?
bool recentIsCwd();
// Is the current working directory the root folder?
bool recentIsRootCwd();
// Record a launched game: cwd = folder it was launched from, name = file name in it
void recentAdd(const std::string &cwd, const std::string &name);
// The recent games that still exist (absolute paths, newest first); also prunes the saved list
std::vector<std::string> recentList();

#endif // FAVORITES_H
