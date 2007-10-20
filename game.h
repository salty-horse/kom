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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $URL$
 * $Id$
 *
 */

#ifndef KOM_GAME_H
#define KOM_GAME_H

#include "common/scummsys.h"
#include "common/array.h"
#include "common/util.h"

#include "kom/database.h"

namespace Kom {

struct RoomObject {
	uint16 objectId;
	int16 actorId;
	int16 z2;
	int16 priority;
};

struct RoomDoor {
	int actorId;
};

struct Settings {
	Settings() : gameCycles(6000), dayMode(1) {}
	uint8 selectedChar; // 0 - thidney. 1 - shahron
	uint8 selectedQuest;
	uint8 dayMode; // 0 - day. 1 - night. 2 - dawn. 3 - dusk
	uint8 currLocation;
	uint16 gameCycles;
	bool fightEnabled;
};

struct Player {
	Player() : isNight(0), sleepTimer(0) {}
	uint8 isNight;
	uint16 sleepTimer;
};

class Game {
public:
	Game(KomEngine *vm, OSystem *system);
	~Game();

	void enterLocation(uint16 locId);
	void processTime();
	bool doStat(const Command *cmd);
	void loopMove();

	Settings* settings() { return &_settings; }
	Player* player() { return &_player; }

	void setDay();
	void setNight();

	void setScope(uint16 charId, int16 scope);

private:

	OSystem *_system;
	KomEngine *_vm;

	Common::Array<RoomObject> _roomObjects;
	Common::Array<RoomDoor> _roomDoors;

	Common::RandomSource _rnd;

    // Settings
    Settings _settings;
    Player _player;

	void processChars();
	void processChar(int proc);
	void changeMode(int value, int mode);
	int16 doExternalAction(const char *action);

	void doActionDusk();
	void doActionDawn();

	void setScopeX(uint16 charId, int16 scope);
	void moveChar(uint16 charId, bool param);
};

} // End of namespace Kom

#endif
