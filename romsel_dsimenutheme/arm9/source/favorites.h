#ifndef FAVORITES_H
#define FAVORITES_H

#include <string>
#include <vector>

// Favorites: games stay in their folders, but favorited games are ALSO listed
// in the "root" folder (default: <device>:/roms/nds/, editable via ROOT= in
// favorites.ini). Entries are stored as absolute paths.

// All stored favorites (absolute paths, e.g. "sd:/roms/nds/A/Game.nds")
std::vector<std::string> favoritesLoad();

bool favoritesIsFav(const std::string &absPath);
void favoritesSet(const std::string &absPath, bool fav);

// Current working directory with trailing slash
std::string favoritesCwd();

// Root folder where favorites are shown (with trailing slash)
std::string favoritesRootDir();

// For the current working directory: return the favorites to inject, as paths
// relative to the cwd (e.g. "A/Game.nds"). Empty unless cwd is the root.
std::vector<std::string> favoritesForCwd();

#endif // FAVORITES_H
