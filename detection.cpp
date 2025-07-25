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
#include "common/array.h"
#include "common/error.h"
#include "common/language.h"
#include "common/platform.h"
#include "common/str.h"

#include "kom/kom.h"

#include "engines/metaengine.h"

static const PlainGameDescriptor komSetting =
	{"kom", "Kingdom O\' Magic"};

class KomMetaEngineDetection : public MetaEngineDetection {
public:
	const char *getName() const {
		return "kom";
	}

	const char *getEngineName() const {
		return "Kingdom O\' Magic";
	}

	const char *getOriginalCopyright() const {
		return "Kingdom O' Magic (C) 1996 SCi (Sales Curve Interactive) Ltd.";
	}

	PlainGameList getSupportedGames() const override;
	PlainGameDescriptor findGame(const char *gameid) const override;
	Common::Error identifyGame(DetectedGame &game, const void **descriptor) override;
	DetectedGames detectGames(const Common::FSList &fslist, uint32 /*skipADFlags*/, bool /*skipIncomplete*/) override;

	uint getMD5Bytes() const override {
		return 0;
	}

	void dumpDetectionEntries() const override final {}
};

PlainGameList KomMetaEngineDetection::getSupportedGames() const {
	PlainGameList games;
	games.push_back(komSetting);
	return games;
}

PlainGameDescriptor KomMetaEngineDetection::findGame(const char *gameid) const {
	if (0 == scumm_stricmp(gameid, komSetting.gameId))
		return komSetting;
	return PlainGameDescriptor::empty();
}

Common::Error KomMetaEngineDetection::identifyGame(DetectedGame &game, const void **descriptor) {
	*descriptor = nullptr;
	game = DetectedGame(getName(), findGame(ConfMan.get("gameid").c_str()));
	return game.gameId.empty() ? Common::kUnknownError : Common::kNoError;
}


DetectedGames KomMetaEngineDetection::detectGames(const Common::FSList &fslist, uint32 /*skipADFlags*/, bool /*skipIncomplete*/) {
	DetectedGames detectedGames;
	for (Common::FSList::const_iterator file = fslist.begin(); file != fslist.end(); ++file) {
		if (!file->isDirectory()) {
			const char *filename = file->getName().c_str();

			if (0 == scumm_stricmp("thidney.dsk", filename) ||
			    0 == scumm_stricmp("shahron.dsk", filename)) {
				// Only 1 target ATM
				DetectedGame game = DetectedGame(getName(), komSetting.gameId, komSetting.description, Common::EN_ANY, Common::kPlatformDOS);
				game.gameSupportLevel = kUnstableGame;
				detectedGames.push_back(game);
				break;
			}
		}
	}
	return detectedGames;
}

REGISTER_PLUGIN_STATIC(KOM_DETECTION, PLUGIN_TYPE_ENGINE_DETECTION, KomMetaEngineDetection);
