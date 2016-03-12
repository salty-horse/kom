/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "base/plugins.h"

#include "common/config-manager.h"
#include "common/fs.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "kom/kom.h"

#include "engines/metaengine.h"

namespace Kom {

static const PlainGameDescriptor kom_list[] = {
	{ "kom", "Kingdom O\' Magic" },
	{ 0, 0 }
};

class KomMetaEngine : public MetaEngine {
public:
	virtual const char *getName() const;
	virtual const char *getOriginalCopyright() const;

	virtual GameList getSupportedGames() const;
	virtual GameDescriptor findGame(const char *gameid) const;
	virtual GameList detectGames(const Common::FSList &fslist) const;

	virtual Common::Error createInstance(OSystem *syst, Engine **engine) const;
};

const char *KomMetaEngine::getName() const {
	return "Kingdom O\' Magic";
}

const char *KomMetaEngine::getOriginalCopyright() const {
	return "Kingdom O' Magic (C) 1996 SCi (Sales Curve Interactive) Ltd.";
}

GameList KomMetaEngine::getSupportedGames() const {
	GameList games;
	const PlainGameDescriptor *g = kom_list;

	while (g->gameId) {
		games.push_back(*g);
		g++;
	}
	return games;
}

GameDescriptor KomMetaEngine::findGame(const char *gameid) const {
	const PlainGameDescriptor *g = kom_list;
	while (g->gameId) {
		if (0 == scumm_stricmp(gameid, g->gameId))
			break;
		g++;
	}
	return *g;
}

GameList KomMetaEngine::detectGames(const Common::FSList &fslist) const {
	GameList detectedGames;
	for (Common::FSList::const_iterator file = fslist.begin(); file != fslist.end(); ++file) {
		if (!file->isDirectory()) {
			const char *filename = file->getName().c_str();

			if (0 == scumm_stricmp("thidney.dsk", filename) ||
			    0 == scumm_stricmp("shahron.dsk", filename)) {
				// Only 1 target ATM
				detectedGames.push_back(GameDescriptor(kom_list[0].gameId, kom_list[0].description, Common::EN_ANY, Common::kPlatformDOS, 0, kUnstableGame));
				break;
			}
		}
	}
	return detectedGames;
}

Common::Error KomMetaEngine::createInstance(OSystem *syst, Engine **engine) const {
	Common::FSNode dir(ConfMan.get("path"));

	// Unable to locate game data
	if (!(dir.getChild("thidney.dsk").exists() || dir.getChild("shahron.dsk").exists())) {
		warning("KOM: unable to locate game data at path '%s'", dir.getPath().c_str());
		return Common::kNoGameDataFoundError;
	}

	if (engine == NULL) {
		return Common::kUnknownError;
	}

	*engine = new KomEngine(syst);
	return Common::kNoError;
}

} // End of namespace Kom

using namespace Kom;

#if PLUGIN_ENABLED_DYNAMIC(KOM)
	REGISTER_PLUGIN_DYNAMIC(KOM, PLUGIN_TYPE_ENGINE, KomMetaEngine);
#else
	REGISTER_PLUGIN_STATIC(KOM, PLUGIN_TYPE_ENGINE, KomMetaEngine);
#endif
