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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/str.h"
#include "common/textconsole.h"
#include "common/util.h"
#include "common/debug.h"
#include "common/file.h"
#include "common/list.h"
#include "common/random.h"
#include "video/flic_decoder.h"

#include "kom/kom.h"
#include "kom/actor.h"
#include "kom/character.h"
#include "kom/game.h"
#include "kom/input.h"
#include "kom/panel.h"
#include "kom/screen.h"
#include "kom/sound.h"
#include "kom/database.h"
#include "kom/video_player.h"
#include "kom/conv.h"


namespace Kom {

using Common::String;
using Common::Path;

Game::Game(KomEngine *vm, OSystem *system) : _system(system), _vm(vm) {
	_videoPlayer = new VideoPlayer(_vm);
}

Game::~Game() {
	delete _videoPlayer;
}

static char filenameBuf[100];

void Game::enterLocation(uint16 locId) {
	_vm->panel()->setActionDesc("");
	_vm->panel()->setHotspotDesc("");
	_vm->panel()->showLoading(true);

	// Don't draw fight bars
	_player.fightBarTimer = _player.enemyFightBarTimer = 0;
	_player.hitPoints = _player.hitPointsOld;

	// Unload last room elements
	for (uint i = 0; i < _roomObjects.size(); i++) {
		if (_roomObjects[i].actorId > -1)
			_vm->actorMan()->unload(_roomObjects[i].actorId);
	}
	_roomObjects.clear();

	for (uint i = 0; i < _roomDoors.size(); i++) {
		_vm->actorMan()->unload(_roomDoors[i].actorId);
	}
	_roomDoors.clear();

	if (locId == 0) {
		_vm->panel()->showLoading(false);
		return;
	}

	// Unload actors
	for (int i = 1; i < _vm->database()->charactersNum(); ++i) {
		Character *chr = _vm->database()->getChar(i);

		chr->_loadedScopeXtend = chr->_scopeInUse = -1;

		if (chr->_actorId >= 0) {
			_vm->actorMan()->unload(chr->_actorId);
			chr->_actorId = -1;
		}
	}

	Location *loc = _vm->database()->getLoc(locId);
	String locName(loc->name);
	locName.toLowercase();
	Path locDir("kom/locs/" + String(locName.c_str(), 2) + "/" + locName);

	// Load room background and mask

	if (_vm->gameLoopTimer() > 1)
		_vm->ambientStart(locId);

	Common::sprintf_s(filenameBuf, sizeof(filenameBuf), "%s%db.flc", locName.c_str(), loc->xtend + _player.isNight);
	_vm->screen()->loadBackground(locDir.appendComponent(filenameBuf));

	// TODO - init some other flic var
	_vm->_flicLoaded = 2;

	filenameBuf[strlen(filenameBuf) - 6] = '0';
	filenameBuf[strlen(filenameBuf) - 5] = 'm';
	Video::FlicDecoder mask;
	_vm->screen()->loadMask(locDir.appendComponent(filenameBuf));

	Database *db = _vm->database();

	// Load room objects
	Common::List<int> objList = loc->objects;
	for (Common::List<int>::iterator objId = objList.begin(); objId != objList.end(); ++objId) {
		Object *obj = db->getObj(*objId);
		RoomObject roomObj;
		roomObj.actorId = -1;
		roomObj.disappearTimer = 0;
		roomObj.objectId = *objId;

		if (obj->isSprite) {
			Common::sprintf_s(filenameBuf, sizeof(filenameBuf), "%s%d.act", obj->name, _player.isNight);
			roomObj.actorId = _vm->actorMan()->load(locDir.appendComponent(filenameBuf));
			roomObj.priority = db->getBox(locId, obj->box)->priority;
			Actor *act = _vm->actorMan()->get(roomObj.actorId);
			act->defineScope(0, 0, act->getFramesNum() - 1, 0);
			act->setScope(0, 3);
			act->setPos(0, SCREEN_H - 1);

			// TODO - move this to processGraphics?
			act->setMaskDepth(roomObj.priority, 32767);
		}

		_roomObjects.push_back(roomObj);
	}

	// Load room doors
	const Exit *exits = db->getExits(locId);
	for (int i = 0; i < 6; ++i) {
		// FIXME: room 45 has one NULL exit. what's it for?
		if (exits[i].exit > 0) {
			String exitName(db->getLoc(exits[i].exitLoc)->name);
			exitName.toLowercase();

			Common::sprintf_s(filenameBuf, sizeof(filenameBuf), "%s%dd.act", exitName.c_str(),
					loc->xtend + _player.isNight);
			Path filename = locDir.appendComponent(filenameBuf);

			// The exit can have no door
			if (!Common::File::exists(filename))
				continue;

			RoomDoor roomDoor;
			roomDoor.actorId = _vm->actorMan()->load(filename);
			Actor *act = _vm->actorMan()->get(roomDoor.actorId);
			act->enable(1);
			act->setEffect(4);
			act->setPos(0, SCREEN_H - 1);
			act->setMaskDepth(0, 32766);

			// Find door-opening boxes (two of them)
			for (int j = 0; j < 6; ++j) {
				roomDoor.boxOpenFast = db->getBoxLink(locId, exits[i].exit, j);
				if (db->getBox(locId, roomDoor.boxOpenFast)->attrib == 0)
					break;
			}

			for (int j = 0; j < 6; ++j) {
				roomDoor.boxOpenSlow = db->getBoxLink(locId, roomDoor.boxOpenFast, j);
				if (roomDoor.boxOpenSlow != exits[i].exit &&
					db->getBox(locId, roomDoor.boxOpenSlow)->attrib == 0)
					break;
			}

			_roomDoors.push_back(roomDoor);
		}
	}

	_vm->panel()->setLocationDesc(loc->desc);
	_vm->panel()->setActionDesc("");
	_vm->panel()->setHotspotDesc("");
	_vm->panel()->showLoading(false);
	_vm->panel()->suppressLoading();
	_vm->panel()->suppressLoading();
}

void Game::processTime() {
	Character *playerChar =_vm->database()->getChar(0);
	if (_settings.dayMode == 0) {
		if (playerChar->_isBusy && _settings.gameCycles >= 6000)
			_settings.gameCycles = 5990;

		if (_vm->database()->getLoc(_settings.currLocation)->allowedTime == 2) {
			_settings.dayMode = 3;
			doActionDusk();
			processChars();
			_settings.dayMode = 1;
			_settings.gameCycles = 0;
		}

		if (_settings.gameCycles < 6000) {

			if (!playerChar->_isBusy)
				(_settings.gameCycles)++;

		} else if (_vm->database()->getLoc(_settings.currLocation)->allowedTime == 0) {
			_settings.dayMode = 3;
			doActionDusk();
			processChars();
			_settings.dayMode = 1;
			_settings.gameCycles = 0;
		}

	} else if (_settings.dayMode == 1) {

		if (playerChar->_isBusy && _settings.gameCycles >= 3600)
			_settings.gameCycles = 3590;

		if (_vm->database()->getLoc(_settings.currLocation)->allowedTime == 1) {
			_settings.dayMode = 2;
			doActionDawn();
			processChars();
			_settings.dayMode = 0;
			_settings.gameCycles = 0;
		}

		if (_settings.gameCycles < 3600) {

			if (!playerChar->_isBusy)
				_settings.gameCycles++;

		} else if (_vm->database()->getLoc(_settings.currLocation)->allowedTime == 0) {
			_settings.dayMode = 2;
			doActionDawn();
			processChars();
			_settings.dayMode = 0;
			_settings.gameCycles = 0;

			playerChar->_hitPoints = MIN(playerChar->_hitPoints + 2, playerChar->_hitPointsMax);
		}
	}

	processChars();
}

void Game::processChars() {
	for (int i = 0; i < _vm->database()->charactersNum(); ++i) {
		Character *ch = _vm->database()->getChar(i);
		if (ch->_isAlive && ch->_proc != -1 && ch->_mode < 6) {
			switch (ch->_mode) {
			case 0:
			case 3:
				processChar(ch->_proc);
				break;
			case 1:
				if (ch->_modeCount ==  0)
					ch->_mode = 0;
				else
					ch->_modeCount--;

				if (ch->_spellMode != 0) {
					if (ch->_spellDuration == 0) {
						ch->unsetSpell();
					}

					if (ch->_spellDuration > 0)
						ch->_spellDuration--;
				}
				break;
			case 2:
				if (ch->_spellDuration == 0)
					ch->unsetSpell();
				else
					ch->_spellDuration--;

				if (ch->_spellMode <= 6) {
					if (ch->_spellMode == 0)
						ch->_mode = 0;
					processChar(ch->_proc);
				}
				break;
			case 4:
			case 5:
				break;
			}
		}
	}
}

void Game::processChar(int proc) {
	bool stop = false;
	Process *p = _vm->database()->getProc(proc);

	for (Common::List<Command>::iterator i = p->commands.begin();
			i != p->commands.end() && !stop; ++i) {
		if (i->cmd == 313) { // Character
			debug(5, "Processing char in %s", p->name);
			stop = doStat(&(*i));
		}
	}
}

bool Game::doProc(int command, int type, int id, int type2, int id2) {
	int proc;
	Process *p;
	bool foundUse = true;
	bool foundLook = false;
	bool foundFight = false;

	switch (type) {
	case 1: { // Object
		// id can be -1 when doFight is called with no last used weapon
		Object *obj = _vm->database()->getObj(id);
		if (obj == NULL)
			return false;
		proc = obj->proc;
		break;
	}
	case 2: // Char
		proc = _vm->database()->getChar(id)->_proc;
		break;
	case 3: // Specific proc
		proc = id;
		break;
	default:
		return false;
	}

	p = _vm->database()->getProc(proc);

	if (p == NULL)
		return false;

	if (command == 319 || command == 320 || command == 321)
		foundUse = false;

	Common::List<Command>::iterator i;
	for (i = p->commands.begin(); i != p->commands.end(); ++i) {
		if (i->cmd == command) {
			switch (command) {
			case 316: // Look at
				foundLook = true;
				if(doStat(&(*i)))
					return true;
				break;
			case 317: // Fight
				foundFight = true;
				if(doStat(&(*i)))
					return true;
				break;
			case 314: // Talk to
			case 315: // Pick up
			case 318: // Enter room
			case 323: // Collide
			case 324: // Reply
				if(doStat(&(*i)))
					return true;
				break;
			case 319: // Use
				foundUse = true;
				if(doStat(&(*i)))
					return true;
				break;
			case 320: // Use item
			case 321:
				if (id2 == i->value) {
					foundUse = true;
					if(doStat(&(*i)))
						return true;
				}
				break;
			default:
				warning("Unhandled proc command: %d", command);
				return true;
			}
		}
	}

	if (command == 317) {
		return foundFight;
	} else if (command == 316) {
		return foundLook;
	} else if (i == p->commands.end() && !foundUse) {
		return false;
	} else if (i == p->commands.end() && command == 319) {
		if (_vm->database()->getObj(id)->type == 2)
			return false;
		else
			return true;
	} else {
		return true;
	}
}

bool Game::doStat(const Command *cmd) {
	bool keepProcessing = true;
	bool rc = false;
	Database *db = _vm->database();
	Conv *conv;

	debug(5, "Trying to execute Command %d - value %hd", cmd->cmd, cmd->value);

	for (Common::List<OpCode>::const_iterator j = cmd->opcodes.begin();
			j != cmd->opcodes.end() && keepProcessing; ++j) {

		if (_vm->shouldQuit())
			break;

		switch (j->opcode) {
		case 327:
			db->setVar(j->arg2, j->arg3);
			break;
		case 328:
			db->setVar(j->arg2, db->getVar(j->arg3));
			break;
		case 331:
			db->setVar(j->arg2, 0);
			break;
		case 334:
			db->setVar(j->arg2, _vm->rnd()->getRandomNumber(j->arg3 - 1));
			break;
		case 337:
			keepProcessing = db->getVar(j->arg2) == 0;
			break;
		case 338:
			keepProcessing = db->getVar(j->arg2) != 0;
			break;
		case 340:
			keepProcessing = db->getVar(j->arg2) == j->arg3;
			break;
		case 345:
			keepProcessing = db->getVar(j->arg2) != j->arg3;
			break;
		case 346:
			keepProcessing = db->getVar(j->arg2) !=
				db->getVar(j->arg3);
			break;
		case 350:
			keepProcessing = db->getVar(j->arg2) > j->arg3;
			break;
		case 353:
			keepProcessing = db->getVar(j->arg2) >= j->arg3;
			break;
		case 356:
			keepProcessing = db->getVar(j->arg2) < j->arg3;
			break;
		case 359:
			keepProcessing = db->getVar(j->arg2) <= j->arg3;
			break;
		case 373:
			db->setVar(j->arg2, db->getVar(j->arg2) + 1);
			break;
		case 374:
			db->setVar(j->arg2, db->getVar(j->arg2) - 1);
			break;
		case 376:
			db->setVar(j->arg2,
				db->getVar(j->arg2) + db->getVar(j->arg3));
			break;
		case 377:
			db->setVar(j->arg2, db->getVar(j->arg2) + j->arg3);
			break;
		case 379:
			db->setVar(j->arg2,
				db->getVar(j->arg2) - db->getVar(j->arg3));
			break;
		case 380:
			db->setVar(j->arg2, db->getVar(j->arg2) - j->arg3);
			break;
		case 381:
			keepProcessing = db->getChar(0)->_locationId == j->arg2;
			break;
		case 382:
			keepProcessing = db->getChar(0)->_locationId != j->arg2;
			break;
		case 383:
			keepProcessing = false;
			if (db->getObj(j->arg2)->ownerType == 3)
				keepProcessing = db->getObj(j->arg2)->ownerId == 0;
			break;
		case 384:
			keepProcessing = true;
			if (db->getObj(j->arg2)->ownerType == 3)
				keepProcessing = db->getObj(j->arg2)->ownerId != 0;
			break;
		case 387:
			keepProcessing = db->giveObject(j->arg2, 0);
			break;
		case 391:
			keepProcessing = db->getChar(0)->_gold != 0;
			break;
		case 392:
			keepProcessing = db->getChar(0)->_gold == 0;
			break;
		case 393:
			if (db->getChar(0)->_gold >= j->arg3) {
				db->getChar(0)->_gold -= j->arg3;
				db->getChar(j->arg2)->_gold += j->arg3;
			}
			break;
		case 394:
			keepProcessing = false;
			if (j->arg4 == -1)
				keepProcessing = db->getChar(j->arg2)->_locationId == j->arg3;
			else if (db->getChar(j->arg2)->_locationId == j->arg3)
				keepProcessing = db->getChar(j->arg2)->_box == j->arg4;
			break;
		case 395:
			keepProcessing = true;
			if (j->arg4 == -1)
				keepProcessing = db->getChar(j->arg2)->_locationId != j->arg3;
			else if (db->getChar(j->arg2)->_locationId == j->arg3)
				keepProcessing = db->getChar(j->arg2)->_box != j->arg4;
			break;
		case 398:
			keepProcessing =
				db->getObj(j->arg3)->ownerType == 3 &&
				db->getObj(j->arg3)->ownerId == j->arg2;
			break;
		case 399:
			keepProcessing = true;
			if (db->getObj(j->arg3)->ownerType == 3)
				keepProcessing = db->getObj(j->arg3)->ownerId != j->arg2;
			break;
		case 403:
			db->setCharPos(j->arg2, j->arg3, j->arg4);
			doActionMoveChar(j->arg2, j->arg3, j->arg4);
			db->getChar(j->arg2)->_destLoc =
			db->getChar(j->arg2)->_destBox = -4;
			break;
		case 402:
			db->getChar(j->arg2)->_destLoc = j->arg3;
			db->getChar(j->arg2)->_destBox = j->arg4;
			break;
		case 404: {
			Character *chr1 = db->getChar(j->arg2);
			Character *chr2 = db->getChar(j->arg3);

			for (int i = 0; i < ARRAYSIZE(_spells); i++) {
				Character *magicChar = _vm->database()->getMagicChar(i);
				if (magicChar->_id < 0)
					continue;

				Spell *spell = &_spells[i];
				if (spell->targetId == j->arg3)
					spell->targetId = j->arg2;
			}
			chr1->stopChar();
			chr2->stopChar();

			chr1->_lastLocation = chr2->_lastLocation;
			chr1->_lastBox = chr2->_lastBox;
			chr1->_gotoBox = chr2->_gotoBox;
			chr1->_screenX = chr2->_screenX;
			chr1->_start3 = chr2->_screenX * 256;
			chr1->_screenY = chr2->_screenY;
			chr1->_start4 = chr2->_screenY * 256;
			chr1->_gotoX = chr2->_gotoX;
			chr1->_gotoY = chr2->_gotoY;
			chr1->_gotoLoc = chr2->_gotoLoc;
			chr1->_start5 = chr2->_start5;

			chr1->stopChar();
			chr2->stopChar();
			break;
		}
		case 405:
			db->getChar(j->arg2)->_destLoc =
			db->getChar(j->arg2)->_destBox = -2;
			break;
		case 406:
			db->getChar(j->arg2)->_destLoc =
			db->getChar(j->arg2)->_destBox = -3;
			break;
		case 407:
			db->getChar(j->arg2)->_destLoc =
			db->getChar(j->arg2)->_destBox = -4;
			break;
		case 408:
			db->getChar(j->arg2)->_destLoc =
			db->getChar(j->arg2)->_destBox = -5;
			break;
		case 409:
			keepProcessing = db->getChar(j->arg2)->_isBusy;
			break;
		case 410:
			keepProcessing = !(db->getChar(j->arg2)->_isBusy);
			break;
		case 411:
			keepProcessing = db->getChar(j->arg2)->_isAlive;
			break;
		case 412:
			keepProcessing = !(db->getChar(j->arg2)->_isAlive);
			break;
		case 413:
			db->getChar(j->arg2)->_isAlive = true;
			break;
		case 414:
			db->getChar(j->arg2)->unsetSpell();
			db->getChar(j->arg2)->_isAlive = false;
			break;
		case 416:
			db->getChar(j->arg2)->_hitPoints =
				(db->getVar(j->arg3) ? db->getVar(j->arg3) : 0);
			if (db->getChar(j->arg2)->_hitPoints > db->getChar(j->arg2)->_hitPointsMax)
				db->getChar(j->arg2)->_hitPoints = db->getChar(j->arg2)->_hitPointsMax;
			break;
		case 417:
			db->getChar(j->arg2)->_gold =
				db->getVar(j->arg3);
			break;
		case 418:
			db->getChar(j->arg2)->_spellPoints =
				(db->getVar(j->arg3) ? db->getVar(j->arg3) : 0);
			if (db->getChar(j->arg2)->_spellPoints > db->getChar(j->arg2)->_spellPointsMax)
				db->getChar(j->arg2)->_spellPoints = db->getChar(j->arg2)->_spellPointsMax;
			break;
		case 419:
			db->getChar(j->arg2)->unsetSpell();
			break;
		case 420: {
			Character *targetChar = db->getChar(j->arg2);
			if (targetChar->_spellMode == 5)
				break;
			targetChar->unsetSpell();

			Object *spellObj = db->getObj(j->arg3);

			doActionSetSpell(j->arg2, spellObj->spellType);

			// Airstrike spell
			if (spellObj->spellType == 9) {
				doActionPlayVideo("msc013g");
				doSpellAttack(j->arg2, j->arg3);

				Location *loc = db->getLoc(targetChar->_locationId);
				for (Common::List<int>::iterator it = loc->characters.begin();
				     it != loc->characters.begin(); it++) {
					Character *chr = db->getChar(*it);

					// Don't harm the player or immortal chars
					if (chr->_type == 1 || !chr->_isMortal)
						continue;

					chr->_hitPoints -= 25;
					if (chr->_hitPoints <= 0) {
						chr->_hitPoints = 0;
						chr->_isAlive = false;
					}
				}

			// Damaging spells
			} else if (spellObj->spellType == 10 ||
			           spellObj->spellType == 11) {
				doSpellAttack(j->arg2, j->arg3);

			} else if (spellObj->spellType == 7 ||
			           spellObj->spellType == 8 ||
			           spellObj->spellType == 12) {
				// Do nothing - handled by scripts

			// Other spells
			} else {
				targetChar->_mode = 2;
				targetChar->_modeCount = 0;
				targetChar->_spellMode = spellObj->spellType;
				targetChar->_spellDuration = 600;

				// Do nothing with Shield
				if (spellObj->spellType == 5)
					break;

				targetChar->_oldStrength = targetChar->_strength;
				targetChar->_oldDefense = targetChar->_defense;
				targetChar->_oldHitPoints = targetChar->_hitPoints;
				if (!targetChar->_isMortal)
					break;

				switch (spellObj->spellType) {
				case 1: // Cabbage
					targetChar->_strength = 1;
					targetChar->_defense = 1;
					targetChar->_hitPoints = 1;
					break;
				case 2: // Slow motion
					targetChar->_strength = MAX(1, (int)(targetChar->_strength * 0.75));
					targetChar->_defense = MAX(1, targetChar->_defense / 2);
					break;
				case 3: // Dwarf
					targetChar->_strength = MAX(1, targetChar->_strength / 2);
					targetChar->_defense = MAX(1, targetChar->_defense / 2);
					targetChar->_hitPoints = MAX(1, targetChar->_hitPoints / 2);
					break;
				case 4: // Floater
					targetChar->_defense = MAX(1, targetChar->_defense / 4);
					break;
				case 6: // Invisibility
					targetChar->_defense = MAX(1, (int)(targetChar->_defense * 1.5));
					break;
				}
			}

			break;
		}
		case 422:
			db->setVar(j->arg2, db->getChar(j->arg3)->_hitPoints);
			break;
		case 423:
			db->setVar(j->arg2, db->getChar(j->arg3)->_spellMode);
			break;
		case 424:
			db->setVar(j->arg2, db->getChar(j->arg3)->_gold);
			break;
		case 425:
			db->setVar(j->arg2, db->getChar(j->arg3)->_spellPoints);
			break;
		case 426:
			keepProcessing = db->getChar(j->arg2)->_locationId ==
			    db->getChar(j->arg3)->_locationId;
			break;
		case 427:
			keepProcessing = db->getChar(j->arg2)->_locationId !=
			    db->getChar(j->arg3)->_locationId;
			break;
		case 428:
			keepProcessing = db->getChar(j->arg2)->_box ==
			    db->getChar(j->arg3)->_box;
			break;
		case 430:
			db->getChar(j->arg2)->_mode = 1;
			db->getChar(j->arg2)->_modeCount = j->arg3;
			break;
		case 431:
			db->getChar(j->arg2)->_mode = 1;
			db->getChar(j->arg2)->_modeCount = db->getVar(j->arg3);
			break;
		case 432:
			db->getChar(j->arg2)->_isVisible = true;
			break;
		case 433:
			db->getChar(j->arg2)->_isVisible = false;
			break;
		case 434:
			keepProcessing = doActionCollide(j->arg2, j->arg3);
			break;
		case 436:
			keepProcessing = db->getObj(j->arg2)->spellCost <=
				db->getChar(j->arg3)->_spellPoints;
			break;
		case 437:
			keepProcessing = db->getObj(j->arg2)->spellCost >
				db->getChar(j->arg3)->_spellPoints;
			break;
		case 438:
			db->getChar(j->arg2)->_strength = j->arg3;
			db->getChar(j->arg2)->_defense = j->arg4;
			db->getChar(j->arg2)->_minDamage = j->arg5;
			db->getChar(j->arg2)->_maxDamage = j->arg6;
			break;
		case 439:
			db->getChar(j->arg2)->_relativeSpeed = j->arg3;
			break;
		case 441:
			keepProcessing = db->giveObject(j->arg2, j->arg3, j->arg4);
			break;
		case 444:
			db->getObj(j->arg2)->isCarryable = 1;
			break;
		case 445:
			db->getObj(j->arg2)->isVisible = 1;
			break;
		case 446:
			db->getObj(j->arg2)->data4 = 0;
			break;
		case 448:
			db->getObj(j->arg2)->isVisible = 0;
			break;
		case 449:
			keepProcessing = _settings.dayMode == 0;
			break;
		case 450:
			keepProcessing = _settings.dayMode == 1;
			break;
		case 451:
			keepProcessing = _settings.dayMode == 2;
			break;
		case 452:
			keepProcessing = _settings.dayMode == 3;
			break;
		case 453:
			setDay();
			break;
		case 454:
			setNight();
			break;
		case 458:
			db->getChar(j->arg2)->_xtend = j->arg3;
			db->getChar(j->arg2)->_scopeInUse = -1;
			db->getChar(j->arg2)->_loadedScopeXtend = -1;
			_vm->actorMan()->unload(db->getChar(j->arg2)->_actorId);
			break;
		case 459:
			db->getLoc(j->arg2)->xtend = j->arg3;
			if (db->getChar(0)->_lastLocation == j->arg2)
				enterLocation(db->getChar(0)->_lastLocation);
			break;
		case 462:
			db->getLoc(j->arg2)->xtend = db->getVar(j->arg3);
			break;
		case 465:
			db->setVar(j->arg2, db->getChar(j->arg3)->_xtend);
			break;
		case 466:
			db->setVar(j->arg2, db->getLoc(j->arg3)->xtend);
			break;
		case 467:
			doActionPlayVideo(j->arg1);
			break;
		case 468:
			doActionSpriteScene(j->arg1, j->arg2, j->arg3, j->arg4);
			break;
		case 469:
			doActionPlaySample(j->arg1);
			break;
		case 473:
			db->getChar(0)->_start3 = db->getChar(0)->_start3PrevPrev;
			db->getChar(0)->_start4 = db->getChar(0)->_start4PrevPrev;
			db->getChar(0)->_start5 = db->getChar(0)->_start5Prev;
			db->getChar(0)->_screenX = db->getChar(0)->_start3 / 256;
			db->getChar(0)->_screenY = db->getChar(0)->_start4 / 256;
			db->getChar(0)->_gotoX = db->getChar(0)->_screenX;
			db->getChar(0)->_gotoY = db->getChar(0)->_screenY;
			db->getChar(0)->_lastBox = db->whatBox(db->getChar(0)->_lastLocation,
				db->getChar(0)->_screenX, db->getChar(0)->_screenY);

			_player.command = CMD_NOTHING;
			_player.commandState = 0;

			break;
		case 474:
			// Rejuvenate hit/spell points
			if (strcmp(j->arg1, "REFRESH") == 0) {
				for (int i = 0; i < db->charactersNum(); ++i) {
					Character *chr = db->getChar(i);
					chr->_hitPoints = MIN(chr->_hitPoints + 5, chr->_hitPointsMax);
					chr->_spellPoints = MIN(chr->_spellPoints + 5, chr->_spellPointsMax);
				}
			} else {
				db->setVar(j->arg2, doExternalAction(j->arg1));
			}
			break;
		case 475:
			keepProcessing = false;
			rc = true;
			break;
		case 476:
			conv = new Conv(_vm, j->arg2);
			conv->doResponse(j->arg3);
			delete conv;
			break;
		case 477:
			conv = new Conv(_vm, j->arg2);
			conv->doResponse(db->getVar(j->arg3));
			delete conv;
			break;
		case 478:
			// Cabbage or floater spells
			if (db->getChar(j->arg2)->_spellMode == 1 ||
			    db->getChar(j->arg2)->_spellMode == 4)
				db->getChar(j->arg2)->unsetSpell();

			conv = new Conv(_vm, j->arg2);
			if (!conv->doTalk(j->arg3, j->arg4)) {
			    rc = 1;
			    keepProcessing = false;
			}
			delete conv;
			break;
		case 479:
			// Cabbage or floater spells
			if (db->getChar(j->arg2)->_spellMode == 1 ||
			    db->getChar(j->arg2)->_spellMode == 4)
				db->getChar(j->arg2)->unsetSpell();

			conv = new Conv(_vm, j->arg2);
			if (!conv->doTalk(db->getVar(j->arg3), j->arg4)) {
			    rc = 1;
			    keepProcessing = false;
			}
			delete conv;
			break;
		case 481:
			doGreeting(j->arg2, db->getVar(j->arg3));
			break;
		case 482:
			doReply(j->arg2, j->arg3);
			break;
		case 483:
			doReply(j->arg2, db->getVar(j->arg3));
			break;
		case 484:
			_player.fightEnemy = j->arg2;
			_player.fightWeapon = j->arg3;
			break;
		case 485:
			_settings.fightEnabled = true;
			break;
		case 486:
			_settings.fightEnabled = false;
			break;
		case 487: {
			Character *chr1 = db->getChar(j->arg2);
			if (!chr1->_isAlive || chr1->_isBusy) {
				keepProcessing = false;
				break;
			}

			Character *chr2 = db->getChar(j->arg3);
			if (!chr2->_isAlive || chr2->_isBusy) {
				keepProcessing = false;
				break;
			}
			if (!doActionCollide(j->arg2, j->arg3)) {
				keepProcessing = false;
				break;
			}

			doNPCFight(j->arg2, j->arg3);
			doNPCFight(j->arg3, j->arg2);

			// Find unused cloud
			for (int i = 0; i < 4; i++) {
				if (_settings.fightNPCCloud[i].charId == -1) {
					_settings.fightNPCCloud[i].charId = MIN(j->arg2, j->arg3);
					_settings.fightNPCCloud[i].timer = 35;
					chr1->_fightPartner = j->arg3;
					chr2->_fightPartner = j->arg2;
					chr1->_isBusy = true;
					chr2->_isBusy = true;
					break;
				}
			}
			break;
		}
		case 488:
			castSpell(j->arg3, j->arg2, j->arg4);
			if (j->arg3 != -1) {
				db->getChar(j->arg3)->_spellPoints -= db->getObj(j->arg2)->spellCost;
			}
			break;
		case 489:
			db->getChar(j->arg2)->_spellMode = j->arg3;
			break;
		case 490:
			db->setVar(j->arg3, doActionShop((uint16)j->arg2));
			break;
		case 491: {
			int16 objId = db->getVar(j->arg2);
			if (objId == -1)
				break;
			Object *obj = db->getObj(objId);
			db->getChar(0)->_gold -= obj->price;
			db->getChar(obj->ownerId)->_gold += obj->price;
			if (!db->giveObject(objId, 0))
				keepProcessing = false;
			break;
		}
		case 492: {
			Common::List<int> *weapons = &db->getChar(j->arg2)->_weapons;

			// Since giveObject removes weapons as we iterate over them,
			// copy them to an array first
			Common::List<int>::size_type count = weapons->size();
			int *weaponArray = new int[count];
			Common::List<int>::size_type i = 0;
			for (Common::List<int>::const_iterator it = weapons->begin();
			     it != weapons->end();
			     it++, i++)
				weaponArray[i] = *it;
			for (i = 0; i < count; i++)
				db->giveObject(weaponArray[i], j->arg3);
			delete[] weaponArray;
			_settings.lastWeaponUsed = -1;
			break;
		}
		case 494:
			_vm->endGame();
			break;
		default:
			warning("Unhandled OpCode: %d - (%s, %d, %d, %d, %d, %d)", j->opcode,
				j->arg1, j->arg2, j->arg3, j->arg4, j->arg5, j->arg6);
		}
	}

	return rc;
}

void Game::doCommand(int command, int type, int id, int type2, int id2) {
	Common::List<EventLink> events;
	Character *chr;

	switch (command) {

	// Talk
	case 2:
		chr = _vm->database()->getChar(id);
		if (!chr->_isAlive) {
			doActionPlaySample("nmess081");
			break;
		}

		switch (chr->_spellMode) {
		case 1:
			doActionPlaySample("nmess078");
			break;
		case 4:
			doActionPlaySample("nmess079");
			break;
		case 6:
			doActionPlaySample("nmess080");
			break;
		default:
			doProc(314, 2, id, -1, -1);
		}

		break;

	// Use
	case 3:
		if (type2 == -1) {
			if (_settings.lastItemUsed != id) {
				_settings.narratorPatience = 0;
				_settings.lastItemUsed = id;
			}

			if (doProc(319, 1, id, -1, -1))
				return;

			if (++_settings.narratorPatience > 4)
				_settings.narratorPatience = 4;
			doNoUse();

		} else {
			bool result;

			if (type2 == 2 && id2 != 0) {
				if (_settings.lastItemUsed != id2) {
					_settings.narratorPatience = 0;
					_settings.lastItemUsed = id2;
				}
			}

			switch (type2) {
			case 1:
				if (doProc(320, 1, id, 1, id2))
					return;

				if (++_settings.narratorPatience > 4)
					_settings.narratorPatience = 4;
				doNoUse();
				break;
			case 2:
				_vm->database()->getChar(id2)->unsetSpell();
				if (!(result = doProc(321, 1, id, 2, id2)))
					result = doProc(320, 2, id2, 1, id);

				if (result)
					return;

				if (++_settings.narratorPatience > 4)
					_settings.narratorPatience = 4;
				doNoUse();
				break;
			default:
				break;
			}
		}
		break;

	// Pick up
	case 4:
		if (!_vm->database()->giveObject(id, 0))
			doProc(315, 1, id, -1, -1);
		break;

	// Look at
	case 5:
		switch (type) {
		case 1: // Object
			doProc(316, 1, id, -1, -1);
			break;
		case 2: // Character
			chr = _vm->database()->getChar(id);
			if (chr->_isAlive) {
				doProc(316, 2, id, -1, -1);
				doLookAt(id);

			// Move all items to player
			} else {
				while (!chr->_inventory.empty()) {
					_vm->database()->giveObject(chr->_inventory.front(), 0);
				}
				while (!chr->_weapons.empty()) {
					_vm->database()->giveObject(chr->_weapons.front(), 0);
				}
				while (!chr->_spells.empty()) {
					_vm->database()->giveObject(chr->_spells.front(), 0);
				}
				_vm->database()->getChar(0)->_gold = chr->_gold;
				chr->_gold = 0;
				chr->_isVisible = false;
				chr->_locationId = 0;
				chr->_box = 0;
				doActionMoveChar(id, 0, 0);
				chr->_destLoc = -4;
				chr->_destBox = -4;
			}
			break;
		case 3:
			break;
		}
		break;

	// Fight
	case 6: {
		if (id2 != -1)
			_settings.lastWeaponUsed = id2;
		else
			id2 = _settings.lastWeaponUsed;

		if (_settings.lastWeaponUsed != -1) {
			Object *weapon = _vm->database()->getObj(_settings.lastWeaponUsed);

			// Special handling of thermal detonator
			if (weapon->minDamage == -10 && weapon->maxDamage == 1) {
				doActionPlayVideo("gin51g");
				_cb.cloudActive = false;
				doActionPlayVideo("msc999s");
				_vm->endGame();
				break;
			}
		}

		if (!_settings.fightEnabled) {
			doProc(317, 2, id, 1, id2);
			break;
		}

		if (doProc(319, 1, id2, -1, -1)) {
			_cb.cloudActive = false;
			break;
		}

		if (doProc(317, 2, id, 1, id2)) {
			_cb.cloudActive = false;
			break;
		}

		doFight(id, id2);
		_cb.cloudActive = true;
		break;
	}

	// Enter room
	case 7:
		events = _vm->database()->getLoc(_settings.currLocation)->events;
		for (Common::List<EventLink>::iterator j = events.begin(); j != events.end(); ++j) {
			if (j->exitBox == id) {
				doProc(318, 3, j->proc, -1, -1);
				break;
			}
		}
		break;

	// Collide
	case 9:
		doProc(323, 2, id, -1, -1);
		break;

	// Reply
	case 10:
		doProc(324, 2, id, -1, -1);
		break;
	}
}

void Game::doNoUse() {
	switch (_settings.narratorPatience) {
	case 1:
		doActionPlaySample("nmess003");
		break;
	case 2:
		doActionPlaySample("nmess004");
		break;
	case 3:
		doActionPlaySample("nmess005");
		break;
	case 4:
		doActionPlaySample("nmess006");
	}
}

void Game::checkUseImmediate(ObjectType type, int16 id) {
	if (id < 0)
		return;

	if (!_vm->database()->getObj(id)->isUseImmediate)
		return;

	if (type != 0) {
		doCommand(3, 1, id, -1, -1);
	} else {
		_player.commandState = 1;
		_player.collideNum = 0;
		_settings.objectType = type;
		_settings.objectNum = id;
		exeMagic();
	}

	_settings.objectType = OBJECT_NONE;
	_settings.objectNum = -1;
	_settings.object2Type = OBJECT_NONE;
	_settings.object2Num = -1;
}

void Game::loopMove() {
	Character *playerChar = _vm->database()->getChar(0);

	if (playerChar->_spriteTimer == 0)
		playerChar->moveChar(true);
	playerChar->moveCharOther();

	if (playerChar->_spriteTimer > 0 && _player.spriteCutMoving) {
		warning("TODO: Draw moving sprite");
	}

	// TODO: something with cbMemory

	if (playerChar->_gotoBox != playerChar->_lastBox) {
		playerChar->_gotoBox = playerChar->_lastBox;

		_vm->database()->setCharPos(0, playerChar->_lastLocation, playerChar->_lastBox);

		// Run "enter room" script
		doCommand(7, -1, playerChar->_lastBox, -1, -1);
	}

	for (uint16 i = 1; i < _vm->database()->charactersNum(); ++i) {
		Character *chr = _vm->database()->getChar(i);

		if (!(chr->_isAlive)) {
			chr->_screenH = 0;
			chr->_height = 0;
			chr->_offset14 = chr->_offset20 = 262144;
			chr->_scopeWanted = 100;
			chr->moveCharOther();

		} else if (chr->_walkSpeed == 0) {
			chr->_scopeWanted = 17;
			chr->_priority = _vm->database()->getPriority(chr->_lastLocation, chr->_lastBox);
			chr->moveCharOther();

		} else {
			if (chr->_spriteCutState == 0 && chr->_fightPartner < 0) {
				chr->_gotoLoc = chr->_destLoc;

				// Special movement types
				if (chr->_destBox <= -2)  {
					switch (chr->_destBox) {
					case -5: // Run away
						chr->_gotoLoc = chr->_runawayLocation;
						chr->_gotoX = chr->_screenX;
						chr->_gotoY = chr->_screenY;
						break;
					case -4: // Stop char
						chr->stopChar();
						break;
					case -3: // Go to player's box
						chr->_gotoLoc = playerChar->_lastLocation;
						chr->_gotoX = _vm->database()->getMidX(chr->_gotoLoc, playerChar->_gotoBox);
						chr->_gotoY = _vm->database()->getMidY(chr->_gotoLoc, playerChar->_gotoBox);
						break;
					case -2: // Go to player's position on screen
						chr->_gotoLoc = playerChar->_lastLocation;
						chr->_gotoX = playerChar->_screenX;
						chr->_gotoY = playerChar->_screenY;
						break;
					}
				} else {
					chr->_gotoX = _vm->database()->getMidX(chr->_gotoLoc, chr->_destBox);
					chr->_gotoY = _vm->database()->getMidY(chr->_gotoLoc, chr->_destBox);
				}
			}

			// Stop characters on collision (when an action is executed on them)
			if (_player.collideType == COLLIDE_CHAR && _player.collideNum == chr->_id) {
				chr->stopChar();
				chr->_lastDirection = 4;
			}

			if (chr->_id == _player.greetingChar)
				chr->stopChar();

			if (chr->_mode == 1)
				chr->stopChar();

			if (chr->_spriteTimer <= 0 && chr->_fightPartner < 0)
				chr->moveChar(true);
			chr->moveCharOther();

			if (chr->_gotoBox != chr->_lastBox) {
				chr->_gotoBox = chr->_lastBox;
				_vm->database()->setCharPos(i, chr->_lastLocation, chr->_lastBox);
			}
		}
	}

	// Move magic characters
	for (int i = 0; i < ARRAYSIZE(_spells); i++) {
		Character *magicChar = _vm->database()->getMagicChar(i);
		Spell *spell = &_spells[i];
		Character *targetChar = NULL;

		if (magicChar->_id == -1)
			continue;

		if (!spell->doneHold) {
			// Stay on the caster for the held duration
			targetChar = magicChar;
			if (spell->holdDuration == 0)
				spell->doneHold = true;
			else
				spell->holdDuration--;
		} else {
			targetChar = _vm->database()->getChar(spell->targetId);
			if (spell->duration == 0) {
				if (magicChar->_id == 9999) { // Dark lord
					_vm->actorMan()->getMagicDarkLord(i)->enable(0);

				} else {
					_vm->actorMan()->get(magicChar->_actorId)->enable(0);
				}
				magicChar->_id = -1;

			} else {
				spell->duration--;
				if (spell->duration <= 8) {
					magicChar->_screenH = 0;
				}
			}
		}

		if (magicChar->_lastLocation == targetChar->_lastLocation ||
		    _vm->database()->loc2loc(magicChar->_lastLocation, targetChar->_lastLocation) != -1) {
			magicChar->_gotoLoc = targetChar->_lastLocation;
			magicChar->_gotoX = targetChar->_screenX;
			magicChar->_gotoY = targetChar->_screenY;
		}
		magicChar->moveChar(false);
		magicChar->moveCharOther();
	}
}

void Game::loopCollide() {

	for (uint16 i = 0; i < _vm->database()->charactersNum(); ++i) {
		Character *chr = _vm->database()->getChar(i);

		if ((_vm->database()->getBox(chr->_lastLocation, chr->_lastBox)->attrib & 1) != 0) {
			chr->hitExit(true);

			_vm->database()->setCharPos(i, chr->_lastLocation, chr->_lastBox);

			// If in the same room as the player
			if (chr->_lastLocation == _vm->database()->getChar(0)->_lastLocation) {

				int found = 0;
				for (uint16 j = 1; j < _vm->database()->charactersNum() && !found; ++j) {
					Character *chr2 = _vm->database()->getChar(j);

					if (chr2->_isAlive && chr2->_isVisible)
						if (_vm->database()->getChar(0)->_lastLocation == chr2->_lastLocation)
							found = true;
				}

				if (found) {
					doCommand(9, 2, chr->_id, -1, -1);
				}
			}
		}
	}

	// Collide magic characters
	for (int i = 0; i < ARRAYSIZE(_spells); i++) {
		Character *magicChar = _vm->database()->getMagicChar(i);
		if (magicChar->_id == -1)
			continue;
		if ((_vm->database()->getBox(magicChar->_lastLocation, magicChar->_lastBox)->attrib & 1) != 0) {
			magicChar->hitExit(false);
		}
	}

	// Collide characters with doors
	for (uint16 i = 0; i < _vm->database()->charactersNum(); ++i) {
		Character *chr = _vm->database()->getChar(i);
		if (chr->_lastLocation != _vm->database()->getChar(0)->_lastLocation)
			continue;

		for (uint j = 0; j < _roomDoors.size(); j++) {
			if (_roomDoors[j].boxHit < 2) {
				if (chr->_lastBox == _roomDoors[j].boxOpenSlow)
					_roomDoors[j].boxHit = 1;
				else if (chr->_lastBox == _roomDoors[j].boxOpenFast)
					_roomDoors[j].boxHit = 2;
			}
		}
	}

	// Animate doors
	for (uint i = 0; i < _roomDoors.size(); i++) {
		int newState = 0;
		Actor *act = _vm->actorMan()->get(_roomDoors[i].actorId);

		switch (_roomDoors[i].boxHit) {
		// Close the door
		case 0:
			newState = 1;
			if (_roomDoors[i].frame > 0)
				(_roomDoors[i].frame)--;
			break;
		// Open slowly
		case 1:
			newState = 0;
			if (_roomDoors[i].frame < act->getFramesNum() - 1)
				(_roomDoors[i].frame)++;
			break;
		// Open fast
		case 2:
			newState = 2;
			_roomDoors[i].frame = act->getFramesNum() - 1;
		}

		_roomDoors[i].boxHit = 0;
		if (_roomDoors[i].frame > 0) {
			switch (newState) {
			case 0:
				if (_roomDoors[i].state == 1 && _vm->_flicLoaded != 2) {
					_vm->sound()->playSampleSFX(_vm->_doorsSample, false);
				}
				break;
			case 1:
				if (_roomDoors[i].state != 1 && _vm->_flicLoaded != 2) {
					_vm->sound()->playSampleSFX(_vm->_doorsSample, false);
				}
			}
		}

		_roomDoors[i].state = newState;
	}
}

void Game::loopSpriteCut() {
	for (uint16 i = 0; i < _vm->database()->charactersNum(); ++i) {
		Character *chr = _vm->database()->getChar(i);

		if (chr->_actorId < 0)
			continue;

		switch (chr->_spriteCutState) {
		case 0:
			break;
		case 1:
			chr->_spriteCutState = 2;
			break;
		case 2:
			if (chr->_stopped) {
				chr->_lastDirection = 4;
				if (chr->_spriteType != 0 || strlen(chr->_spriteName) >= 2) {
					chr->_spriteTimer = 0;
					chr->setScope(101);
					chr->_spriteCutState = 3;
				} else {
					chr->_spriteCutState = 0;
					chr->_isBusy = false;
				}
			}
			break;
		case 3:
			if (chr->_spriteTimer > 0)
				chr->_spriteTimer--;
			if (chr->_spriteTimer <= 1)
				chr->_isBusy = false;

			if (chr->_spriteTimer == 0) {
				chr->_spriteCutState = 0;
				chr->_lastDirection = 4;
			}
			break;
		default:
			break;
		}
	}
}

void Game::loopSpells() {
	for (int i = 0; i < ARRAYSIZE(_spells); i++) {
		Spell *spell = &_spells[i];
		Character *magicChar = _vm->database()->getMagicChar(i);

		if (magicChar->_id == -1)
			continue;

		Character *targetChar = _vm->database()->getChar(spell->targetId);

		if (!spell->doneHold)
			continue;
		if (magicChar->_lastBox != targetChar->_lastBox)
			continue;
		if (ABS(magicChar->_start3 - targetChar->_start3) >= 2 * 256)
			continue;
		if (ABS(magicChar->_start4 - targetChar->_start4) >= 2 * 256)
			continue;

		if (targetChar->_spellMode == 5 ||
		    (targetChar->_id != 0 && _player.colgateTimer != 0)) {

			spell->doneHold = 2;
			spell->duration = 40;
			magicChar->_screenH = -0x640000;
			magicChar->_height = -0x3840000;
			magicChar->_screenHDelta = -0x320000;
			_player.colgateTimer = 3;
			_vm->sound()->playSampleSFX(_vm->_colgateSample, false);

		} else {
			if (!targetChar->_isAlive)
				continue;

			doCommand(3, 1, spell->spellId, 2, spell->targetId);

			if (magicChar->_id == 9999)
				_vm->actorMan()->getMagicDarkLord(i)->enable(0);
			else
				_vm->actorMan()->get(magicChar->_actorId)->enable(0);
			magicChar->_id = -1;
		}
	}
}

/** Play idle animations */
void Game::loopTimeouts() {
	if (_vm->_flicLoaded != 0)
		return;

	_player.spriteCutNum = 0;

	for (uint16 i = 1; i < _vm->database()->charactersNum(); ++i) {
		Character *chr = _vm->database()->getChar(i);

		if (chr->_isAlive) {
			if (!(chr->_stopped)) {
				chr->_stoppedTime = 0;
			} else {
				if (chr->_spriteTimer == 0)
					chr->_stoppedTime++;

				if (chr->_timeout == chr->_stoppedTime)
					chr->_lastDirection = 4;

				if (chr->_timeout > 0 && chr->_stoppedTime >= chr->_timeout) {
					chr->_stoppedTime = 0;
					chr->_scopeWanted = 12;
				}
			}
		}

		// Idle animations don't count as "control freezing"
		if (chr->_lastLocation == _vm->database()->getChar(0)->_lastLocation &&
			chr->_spriteTimer > 0 && chr->_scopeInUse != 12)
				_player.spriteCutNum++;
	}
}

void Game::loopInterfaceCollide() {
	Character *playerChar = _vm->database()->getChar(0);
	int boxId;

	if (playerChar->_lastLocation == 0)
		return;

	_settings.collideBoxX = _settings.mouseX = _vm->input()->getMouseX() * 2;
	_settings.collideBoxY = _settings.mouseY = _vm->input()->getMouseY() * 2;

	_settings.collideBox = _settings.mouseBox =
		_vm->database()->whatBox(playerChar->_lastLocation,
			_settings.collideBoxX, _settings.collideBoxY);

	if (_settings.collideBox < 0) {
		_settings.mouseOverExit = false;

		_vm->database()->getClosestBox(playerChar->_lastLocation,
				_settings.mouseX, _settings.mouseY,
				playerChar->_screenX, playerChar->_screenY,
				&_settings.collideBox, &_settings.collideBoxX,
				&_settings.collideBoxY);

	// Found a box
	} else {
		Box *box = _vm->database()->getBox(playerChar->_lastLocation,
				_settings.collideBox);

		boxId = _settings.collideBox;

		// Change to a linked box
		if ((box->attrib & 8) != 0) {
			// Find last link
			for (int i = 5; i >= 0; i--) {
				int link = _vm->database()->getBoxLink(
							playerChar->_lastLocation,
							_settings.collideBox,
							i);
				if (link != -1) {
					boxId = link;
					break;
				}
			}
		}

		box = _vm->database()->getBox(playerChar->_lastLocation,
				boxId);

		if ((box->attrib & 1) == 0) {
			_settings.mouseOverExit = false;
		} else {
			_settings.collideBox = boxId;

			_settings.collideBoxX =
				_vm->database()->getMidX(playerChar->_lastLocation,
						_settings.collideBox);

			_settings.collideBoxY =
				_vm->database()->getMidY(playerChar->_lastLocation,
						_settings.collideBox);

			_settings.mouseOverExit = true;
		}

		box = _vm->database()->getBox(playerChar->_lastLocation,
				_settings.collideBox);

		if ((box->attrib & 8) != 0) {
			_settings.collideBox = _vm->database()->getFirstLink(
					playerChar->_lastLocation,
					_settings.collideBox);

			_settings.collideBoxX =
				_vm->database()->getMidX(playerChar->_lastLocation,
						_settings.collideBox);

			_settings.collideBoxY =
				_vm->database()->getMidY(playerChar->_lastLocation,
						_settings.collideBox);
		}
	}

	// Hover over objects

	_settings.collideObj = -1;
	_settings.collideObjZ = 1073741824;

	Common::Array<RoomObject> *roomObjects = getObjects();

	for (uint i = 0; i < roomObjects->size(); i++) {
		if ((*roomObjects)[i].objectId < 0)
			continue;

		Object *obj = _vm->database()->getObj((*roomObjects)[i].objectId);
		int32 z = 0;
		int16 x = 0, y = 0;

		if (!obj->isVisible)
			continue;

		// If the mouse box is on the object box
		if (obj->box == _settings.mouseBox) {
			int8 link = _vm->database()->getFirstLink(
					playerChar->_lastLocation, obj->box);
			Box *box = _vm->database()->getBox(playerChar->_lastLocation, link);
			x = _vm->database()->getMidX(playerChar->_lastLocation, link);
			y = _vm->database()->getMidY(playerChar->_lastLocation, link);
			z = box->z1;
		}

		// If the mouse is over the drawn object
		if ((*roomObjects)[i].actorId >= 0) {

			Actor *act = _vm->actorMan()->get((*roomObjects)[i].actorId);
			if (act->inPos(_settings.mouseX / 2, _settings.mouseY / 2)) {
				int8 link = _vm->database()->getFirstLink(
						playerChar->_lastLocation, obj->box);
				Box *box = _vm->database()->getBox(playerChar->_lastLocation, link);
				x = _vm->database()->getMidX(playerChar->_lastLocation, link);
				y = _vm->database()->getMidY(playerChar->_lastLocation, link);
				z = box->z1;
			}
		}

		if (z > 0 && z < _settings.collideObjZ) {
			_settings.collideObj = (*roomObjects)[i].objectId;
			_settings.collideObjX = x;
			_settings.collideObjY = y;
			_settings.collideObjZ = z;
		}
	}

	// Hover over characters

	_settings.collideChar = -1;
	_settings.collideCharZ = 1073741824;

	for (int i = 1; i < _vm->database()->charactersNum(); ++i) {
		Character *chr = _vm->database()->getChar(i);

		if (playerChar->_lastLocation != chr->_lastLocation ||
			!chr->_isVisible)
			continue;

		if (chr->_spriteCutState != 0 && chr->_scopeInUse != 12)
			continue;

		// Characters in the current room may have no actor
		// if they are in a fight
		if (chr->_actorId == -1)
			continue;

		// If the mouse is over the drawn character
		Actor *act = _vm->actorMan()->get(chr->_actorId);
		if (!act->inPos(_settings.mouseX / 2, _settings.mouseY / 2))
			continue;

		if (chr->_start5 >= _settings.collideCharZ)
			continue;

		Box *box = _vm->database()->getBox(chr->_lastLocation,
				chr->_lastBox);

		if (box->attrib == 8) {
			int link = _vm->database()->getFirstLink(chr->_lastLocation,
				chr->_lastBox);
			_settings.collideCharX =
				_vm->database()->getMidX(chr->_lastLocation, link);
			_settings.collideCharY =
				_vm->database()->getMidY(chr->_lastLocation, link);
		} else {
			_settings.collideCharX = chr->_screenX;
			_settings.collideCharY = chr->_screenY;
		}

		_settings.collideChar = i;
		_settings.collideCharZ = chr->_start5;
	}

	// Special handling of the Ninja Baker.
	// His idle animation moves outside his box, so don't allow focus.
	// (This actually makes sense -- ninjas move very fast)
	if (_settings.collideChar == 62 &&
			_vm->database()->getChar(62)->_spriteTimer > 0) {
		_settings.collideChar = -1;
	}

	_settings.mouseBox = _settings.collideBox;

	if (_settings.mouseOverExit) {
		_settings.collideBoxZ = 32000;
	} else {
		_settings.collideBoxZ =
			_vm->database()->getZValue(playerChar->_lastLocation,
					_settings.collideBox, _settings.collideBoxY * 256);
	}

	_settings.oldOverType = _settings.overType;
	_settings.oldOverNum = _settings.overNum;
	_settings.overType = COLLIDE_NONE;
	_settings.overNum = -1;

	// TODO: handle inventory
	if (_vm->_flicLoaded != 0 || _settings.mouseMode != 0)
		return;

	// FIXME - some code duplication in this horrible tree
	if (_settings.objectNum < 0) {
		if (_settings.mouseOverExit &&
		    _settings.collideBoxZ < _settings.collideCharZ &&
		    _settings.collideBoxZ < _settings.collideObjZ) {

			_settings.overType = COLLIDE_BOX;
			_settings.overNum = _settings.collideBox;
			_settings.mouseState = 2; // Exit

		} else if (_settings.collideChar >= 0 &&
			_settings.collideCharZ < _settings.collideObjZ) {

			_settings.overType = COLLIDE_CHAR;
			_settings.overNum = _settings.collideChar;
			_settings.mouseState = 3; // Hotspot

		} else if (_settings.collideObj >= 0) {

			_settings.overType = COLLIDE_OBJECT;
			_settings.overNum = _settings.collideObj;
			_settings.mouseState = 3; // Hotspot

		} else {
			_settings.mouseState = 0; // Walk
		}

	} else if (_player.command == CMD_USE) {

			if (_settings.collideChar >= 0 &&
				_settings.collideCharZ < _settings.collideObjZ) {

				_settings.overType = COLLIDE_CHAR;
				_settings.overNum = _settings.collideChar;
				_settings.mouseState = 3; // Hotspot

			} else if (_settings.collideObj >= 0) {

				_settings.overType = COLLIDE_OBJECT;
				_settings.overNum = _settings.collideObj;
				_settings.mouseState = 3; // Hotspot

			} else {
				_settings.mouseState = 0; // Walk
			}

	} else if (_player.command == CMD_FIGHT ||
			   _player.command == CMD_CAST_SPELL) {

		if (_settings.collideChar < 0)
			_settings.mouseState = 0; // Walk
		else {
			_settings.overType = COLLIDE_CHAR;
			_settings.overNum = _settings.collideChar;
			_settings.mouseState = 3; // Hotspot
		}
	}
}

int16 Game::doExternalAction(const char *action) {
	if (strcmp(action, "getquest") == 0) {
		return _player.selectedQuest;

	} else if (strcmp(action, "sleepon") == 0) {
		setNight();
		_vm->ambientStop();
		_player.sleepTimer = 300;

	} else if (strcmp(action, "sleepoff") == 0) {
		_player.sleepTimer = 0;

	} else if (strcmp(action, "stats") == 0) {
		doLookAt(0, 192, true);

	} else if (strcmp(action, "crowdon") == 0) {
		warning("TODO: crowdon");
		Character *playerChar = _vm->database()->getChar(0);
		Character *mobChar = _vm->database()->getChar(15);
		if (playerChar->_lastLocation != mobChar->_lastLocation) {
			int exitBox = _vm->database()->findFarthestExit(playerChar->_lastLocation, playerChar->_lastBox);
			int nextBox = _vm->database()->box2box(playerChar->_lastLocation, exitBox, playerChar->_lastBox);
			doActionMoveChar(15, playerChar->_lastLocation, nextBox);
			_vm->database()->getExitInfo(mobChar->_lastLocation, exitBox, &mobChar->_runawayLocation, &exitBox);
		}

	} else if (strcmp(action, "crowdOFF") == 0) {
		// Do nothing

	} else {
		warning("TODO: Unknown external action: %s", action);
	}

	return 0;
}

void Game::doActionDusk() {
	_player.isNight = 1;
	enterLocation(_vm->database()->getChar(0)->_locationId);
}

void Game::doActionDawn() {
	_player.isNight = 0;
	enterLocation(_vm->database()->getChar(0)->_locationId);
}

void Game::setDay() {
	if (_vm->database()->getLoc(_settings.currLocation)->allowedTime != 2 && _settings.dayMode != 0) {
		_settings.gameCycles = 3600;
		_settings.dayMode = 1;
	}
}

void Game::setNight() {
	if (_vm->database()->getLoc(_settings.currLocation)->allowedTime != 1 && _settings.dayMode != 1) {
		_settings.gameCycles = 6000;
		_settings.dayMode = 0;
	}
}

void Game::doActionMoveChar(uint16 charId, int16 loc, int16 box) {
	Character *chr = _vm->database()->getChar(charId);
	if (charId == 0 && loc > 0) {
		enterLocation(loc);
		// TODO: flicUpdateBG()
	}

	if (_vm->database()->loc2loc(chr->_lastLocation, loc) == -1) {
		for (int i = 0; i < ARRAYSIZE(_spells); ++i) {
			Character *magicChar = _vm->database()->getMagicChar(i);
			Spell *spell = &_spells[i];
			if (magicChar->_id != -1 && spell->targetId == charId) {
				if (spell->duration >= 10)
					spell->duration = 10;
			}
		}
	}

	chr->_lastLocation = loc;
	chr->_lastBox = box;
	chr->_gotoBox = -1;

	chr->_screenX = _vm->database()->getMidX(loc, box);
	chr->_start3 = chr->_screenX * 256;
	chr->_screenY = _vm->database()->getMidY(loc, box);
	chr->_start4 = chr->_screenY * 256;
	chr->_start5 = _vm->database()->getZValue(loc, box, chr->_start4);
	chr->stopChar();
	chr->_lastDirection = 4;
}

bool Game::doActionCollide(int16 char1, int16 char2) {
	Character *chr1 = _vm->database()->getChar(char1);
	Character *chr2 = _vm->database()->getChar(char2);

	return (chr1->_lastLocation == chr2->_lastLocation &&
	        abs(chr1->_start3 - chr2->_start3) < 10240 &&
	        abs(chr1->_start5 - chr2->_start5) < 40);
}

void Game::doActionSpriteScene(const char *name, int charId, int loc, int box) {
	static const char *prefixes[] = { "DLC001", "DLC006", "EWC000" };
	static int tabs[] = { 0, 1, 2 }; // TODO - check real values
	static int nums[] = { 29, 30, 29 };
	Character *chr = _vm->database()->getChar(charId);
	String spriteName(name);

	spriteName.toUppercase();

	if (loc < 0)
		chr->_gotoLoc = chr->_lastLocation;
	else
		chr->_gotoLoc = loc;

	if (box < 0) {
		chr->_gotoX = chr->_screenX;
		chr->_gotoY = chr->_screenY;
	} else {
		chr->_spriteBox = box;
		chr->_gotoX = _vm->database()->getMidX(loc, box);
		chr->_gotoY = _vm->database()->getMidY(loc, box);
	}

	_player.spriteCutMoving = false;

	// Check if it's a moving sprite
	if (charId == 0 && name[0] != '\0') {

		_player.command = CMD_WALK;

		int8 match = -1;
		for (int8 i = 0; i < 3; ++i) {
			if (spriteName.hasPrefix(prefixes[i])) {
				_player.spriteCutTab = -1;
				match = i;
				break;
			}
		}

		if (match != -1) {
			_player.spriteCutNum = nums[match];
			_player.spriteCutTab = tabs[match];
			_player.spriteCutPos = 0;

			_player.spriteCutMoving = true;
			_player.spriteCutX = chr->_screenX / 2;
			_player.spriteCutY = chr->_screenY / 2;
		}
	}

	// Scope 12 is the idle animation
	if (chr->_spriteCutState == 0 || chr->_scopeInUse == 12)
		chr->_spriteCutState = 1;

	chr->_spriteName = name;
	chr->_spriteType = 0;
	chr->_isBusy = true;
}

void Game::doActionPlayVideo(const char *name) {
	const char *dir = "";
	bool pauseAmbient = true;
	bool noModifier = false;
	char prefix[5] = "";

	static const char *convPrefix[] = {
		"afro", "bak", "bke", "bst",
		"dgn", "dis", "dld", "el1",
		"el2", "evs", "fry", "gd1",
		"gd2", "gd4", "ggn", "gin",
		"gnt", "grn", "in1", "in2",
		"in3", "in4", "jack", "kng",
		"krys", "man", "msh", "pxe",
		"_vm->rnd", "sman", "stw", "sty",
		"ter", "trl1", "trl2", "trl3",
		"wig"
	};

	stopNarrator();
	stopGreeting();

	String videoName(name);
	videoName.toLowercase();

	if (player()->selectedChar == 0) {
		if (videoName.lastChar() == 's')
			videoName.setChar('t', videoName.size() - 1);
		// SHAR -> SMAN
		if (videoName.hasPrefix("shar")) {
			videoName.setChar('m', 1);
			videoName.setChar('n', 3);
		}
	}

	if (videoName[1] == '_' || videoName[1] == '-') {
		switch (videoName[0]) {
		case 'a':
			pauseAmbient = false;
			break;
		case 'e':
			dir = "cutsend";
			noModifier = true;
			break;
		case 's':
			dir = "cutsigns";
			break;
		}

		videoName = videoName.c_str() + 2;
	}

	// No match yet
	if (*dir == '\0') {
		// try to match the file against a cutsconv prefix.
		// if one doesn't exist, it's a game cutscene.
		if (videoName[0] == 'j' || videoName[2] != 'c') {
			for (uint i = 0; i < ARRAYSIZE(convPrefix); i++) {
				if (videoName.hasPrefix(convPrefix[i])) {
					dir = "cutsconv";
					Common::strlcpy(prefix, convPrefix[i], sizeof(prefix));
					break;
				}
			}
		}
	}

	// Must be a game cutscene
	if (*dir == '\0') {
		dir = "cutsgame";
		prefix[0] = videoName[0];
		prefix[1] = videoName[1];
		prefix[2] = '\0';
	}

	if (noModifier)
		Common::sprintf_s(filenameBuf, sizeof(filenameBuf), "kom/%s/%s%s%s.smk",
			dir, prefix, prefix[0] ? "/" : "", videoName.c_str());
	else {
		int len = Common::sprintf_s(filenameBuf, sizeof(filenameBuf), "kom/%s/%s%s%s%d.smk",
			dir, prefix, prefix[0] ? "/" : "", videoName.c_str(),
			player()->isNight);

		// If the file isn't found, try the other day mode
		if (!Common::File::exists(filenameBuf)) {

			filenameBuf[len-5] = '0' + '1' - filenameBuf[len-5];

			if (!Common::File::exists(filenameBuf)) {

				// Still not found. Try flc extension
				filenameBuf[len-5] = '0';
				filenameBuf[len-3] = 'f';
				filenameBuf[len-2] = 'l';
				filenameBuf[len-1] = 'c';
			}
		}
	}

	// Fail silently when a video isn't found.
	// For example, when talking to the disco king, DIS00T is
	// supposed to play, but it doesn't exist
	if (!Common::File::exists(filenameBuf)) {
		return;
	}

	_vm->screen()->clearScreen();

	if (pauseAmbient)
		_vm->ambientPause(true);

	_vm->panel()->suppressLoading();

	// Videos can be played from a conversation, which already hides the cursor
	bool cursorVisible = _vm->screen()->isCursorVisible();
	if (cursorVisible)
		_vm->screen()->showMouseCursor(false);

	_videoPlayer->playVideo(filenameBuf);

	if (cursorVisible)
		_vm->screen()->showMouseCursor(true);

	if (pauseAmbient)
		_vm->ambientPause(false);
}

void Game::doActionPlaySample(const char *name) {
	String sampleName(name);
	char prefix[10];
	int mode = 0;

	warning("TODO: doActionPlaySample(%s)", name);

	stopNarrator();

	if (_player.spriteSample.isLoaded()) {
		_vm->sound()->stopSample(_player.spriteSample);
		_player.spriteSample.unload();
		_player.greetingChar = -1;
	}

	sampleName.toLowercase();

	if (sampleName[1] == '_' || sampleName[1] == '-') {
		switch (sampleName[0]) {
		case 'p':
			mode |= 3;
			break;
		case 'v':
			mode |= 5;
		}

		sampleName.deleteChar(0);
		sampleName.deleteChar(0);
	}

	if (sampleName.hasPrefix("nmess")) {
		Common::sprintf_s(filenameBuf, sizeof(filenameBuf), "kom/nar/%s.raw", sampleName.c_str());

	} else if (sampleName.lastChar() == 'l' || sampleName.lastChar() == 'u' ||
	           sampleName.lastChar() == 'g') {

		strncpy(prefix, sampleName.c_str(), sampleName.size()-2);
		prefix[sampleName.size()-2] = '\0';
		Common::sprintf_s(filenameBuf, sizeof(filenameBuf), "kom/locs/%c%c/%s/%s.raw",
				sampleName[0], sampleName[1], prefix, sampleName.c_str());

	} else {
		Common::sprintf_s(filenameBuf, sizeof(filenameBuf), "kom/obj/%s.raw", sampleName.c_str());
	}

	if ((mode & 4) == 0)
		_vm->panel()->showLoading(true);

	_player.narratorTalking = true;

	// Load sample
	// TODO - support pitch-altering cheat code?
	sampleName.toUppercase();
	narratorStart(filenameBuf, sampleName.c_str());
	_vm->database()->getChar(0)->_isBusy = true;

	if ((mode & 4) == 0)
		_vm->panel()->showLoading(false);

	if ((mode & 1) == 0)
		return;

	// Play sample. input loop
	_vm->actorMan()->pauseAnimAll(true);

	while (isNarratorPlaying()) {
		if (mode & 2) {
			_vm->screen()->processGraphics(1, /*samplePlaying=*/ true);
		}

		// TODO: break on space and esc as well
		if (_vm->input()->getRightClick())
			break;
	}

	stopNarrator();
	_vm->database()->getChar(0)->_isBusy = false;
	_vm->actorMan()->pauseAnimAll(false);
}

void Game::narratorStart(const char *filename, const char *codename) {

	char *text;

	if (_player.narratorSample.isLoaded()) {
		_vm->sound()->stopSample(_player.narratorSample);
		_player.narratorSample.unload();
		_vm->screen()->narratorScrollDelete();
	}

	text = _vm->database()->getNarratorText(codename);
	if (text) {
		_vm->screen()->narratorScrollInit(text);
	}

	_player.narratorSample.loadFile(filename);
	_vm->sound()->playSampleSpeech(_player.narratorSample);
}

void Game::stopNarrator() {
	if (_player.narratorTalking) {
		warning("TODO: stopNarrator");
		_vm->sound()->stopSample(_player.narratorSample);
		_vm->screen()->narratorScrollDelete();
		_vm->database()->getChar(0)->_isBusy = false;
		_player.narratorTalking = false;
	}
}

bool Game::isNarratorPlaying() {
	return _vm->sound()->isPlaying(_player.narratorSample);
}

void Game::doGreeting(int charId, int greeting) {
	if (_vm->rnd()->getRandomNumber(4) == 0)
		return;

	if (isNarratorPlaying())
		return;

	if (_player.spriteSample.isLoaded())
		return;

	String charName(_vm->database()->getChar(charId)->_name);
	charName.toLowercase();
	Common::sprintf_s(filenameBuf, sizeof(filenameBuf), "kom/conv/%s/response/%d.raw", charName.c_str(), greeting);

	_vm->panel()->showLoading(true);
		stopGreeting();
		_player.greetingChar = charId;
		_player.greetingLoc = _vm->database()->getChar(charId)->_lastLocation;
		_player.spriteSample.loadFile(filenameBuf);
		_vm->sound()->playSampleSFX(_player.spriteSample, false);
	_vm->panel()->showLoading(false);
}

void Game::doReply(int charId, int reply) {
	char buf[10];
	Common::sprintf_s(buf, sizeof(buf), "%d", reply);
	doReply(charId, buf);
}

void Game::doReply(int charId, const char *reply) {
	if (isNarratorPlaying())
		return;

	String charName(_vm->database()->getChar(charId)->_name);
	charName.toLowercase();
	Common::sprintf_s(filenameBuf, sizeof(filenameBuf), "kom/conv/%s/response/%s.raw", charName.c_str(), reply);

	_vm->panel()->showLoading(true);
		stopGreeting();
		_player.greetingChar = charId;
		// The location is stored as a negative value, signifying it's a reply
		_player.greetingLoc = -_vm->database()->getChar(charId)->_lastLocation;
		_player.spriteSample.loadFile(filenameBuf);
		_vm->sound()->playSampleSFX(_player.spriteSample, false);
	_vm->panel()->showLoading(false);
}

void Game::stopGreeting() {
	if (_player.spriteSample.isLoaded()) {
		_vm->sound()->stopSample(_player.spriteSample);
		_player.spriteSample.unload();
		_player.greetingChar = -1;
	}
}

bool Game::isSamplePlaying() {
	return _vm->sound()->isPlaying(_player.spriteSample);
}

/**
 * Displays the verb 'donut' on the screen. Sets a variable to the
 * selected verb.
 *
 * @param type where the donut is displayed:
 * 	* 0 - in room.
 * 	* 1 - in inventory.
 * 	* 2 - in store.
 * @param param passed to the inventory function
 *
 * @return whether a valid verb was chosen (0) or not (-1)
 *
 * FIXME: handle mouse key-up events like the original
 */
int8 Game::doDonut(int type, Inventory *inv) {
	uint16 hotspotX, hotspotY;
	Actor *mouse = _vm->actorMan()->getMouse();
	Actor *donut = _vm->actorMan()->getDonut();
	int8 donutLoopState = 1;
	bool overVerb = false;
	CommandType overCommand = CMD_WALK;

	enum {
		PICKUP = 0,
		FIGHT,
		TALK_TO,
		LOOK_AT,
		USE, // Also purchase
		CAST_SPELL
	};

	// Verbs
	// 0 - active
	// 6 - inactive
	uint8 verbs[6];

	_vm->actorMan()->pauseAnimAll(true);

	if (type == 0) {
		_vm->screen()->pauseBackground(true);
		// TODO: pause snow
	}

	// Move the mouse away from the edge of the screen
	if (_settings.mouseX < 78) {
		_settings.mouseX = 78;
		_vm->input()->setMousePos(39, _settings.mouseY / 2);

	} else if (_settings.mouseX > 562) {
		_settings.mouseX = 562;
		_vm->input()->setMousePos(281, _settings.mouseY / 2);
	}

	if (_settings.mouseY < 78) {
		_settings.mouseY = 78;
		_vm->input()->setMousePos(_settings.mouseX / 2, 39);

	} else if (_settings.mouseY > 322) {
		_settings.mouseY = 322;
		_vm->input()->setMousePos(_settings.mouseX / 2, 161);
	}

	hotspotX = _settings.mouseX;
	hotspotY = _settings.mouseY + 78;

	// FIXME - is the first switch to "walk to" required?
	mouse->switchScope(0, 2);
	mouse->switchScope(1, 2);

	// Set active verbs
	if (type != 0)	{
		verbs[PICKUP] = 6;
		verbs[FIGHT] = 6;
		verbs[TALK_TO] = 6;
		verbs[LOOK_AT] = 0;
		verbs[USE] = 0;
		verbs[CAST_SPELL] = 6;

	} else switch (_settings.collideType) {

	case COLLIDE_CHAR:
		if (_vm->database()->getChar(_settings.collideChar)->_isAlive) {
			verbs[PICKUP] = 6;
			verbs[FIGHT] = 0;
			verbs[TALK_TO] = 0;
			verbs[LOOK_AT] = 0;
			verbs[USE] = 6;
			verbs[CAST_SPELL] = 0;
		} else {
			verbs[PICKUP] = 6;
			verbs[FIGHT] = 6;
			verbs[TALK_TO] = 0;
			verbs[LOOK_AT] = 0;
			verbs[USE] = 6;
			verbs[CAST_SPELL] = 6;
		}

		// Special cases:
		// Can't talk to the behemoth, troll statues
		if (_settings.collideChar >= 6 && _settings.collideChar <= 9)
			verbs[TALK_TO] = 6;

		// Can't fight with giant spiders
		else if (_settings.collideChar >= 85 && _settings.collideChar <= 87)
			verbs[FIGHT] = 6;

		break;

	case COLLIDE_OBJECT:
		verbs[PICKUP] =
			_vm->database()->getObj(_settings.collideObj)->isPickable ? 0 : 6;
		verbs[FIGHT] = 6;
		verbs[TALK_TO] = 6;
		verbs[LOOK_AT] = _settings.collideObj == 212 ? 6 : 0; // Arcade machine
		verbs[USE] =
			_vm->database()->getObj(_settings.collideObj)->isUsable ? 0 : 6;
		verbs[CAST_SPELL] = 6;

		break;
	default:
		error("Illegal type for donut");
	}

	while (donutLoopState != 0) {
		int16 donutX = _vm->input()->getMouseX() * 2 - _settings.mouseX;
		int16 donutY = _vm->input()->getMouseY() * 2 - _settings.mouseY;
		int32 segment;
		const char *verbDesc, *hotspotDesc;

		if (abs(donutX) + abs(donutY) == 0) {
			segment = -1;
		} else {
			int32 sum = donutX * donutX + donutY * donutY;

			if (0x4000000 / sum > 0x10000)
				segment = -1;
			else if (0x17C40000 / sum <= 0x10000)
				segment = -2;
			else
				segment = getDonutSegment(donutX, donutY);
		}

		switch (donutLoopState) {
		case 1:
			if (segment != -1)
				donutLoopState = 3;
			else if (!_vm->input()->getLeftClick())
				donutLoopState = 2;
			break;
		case 2:
			if (_vm->input()->getRightClick()) {
				overVerb = false;
				segment = -1;
				donutLoopState = 0;
			}

			if (_vm->input()->getLeftClick()) {
				if (overVerb || overCommand == CMD_WALK)
					donutLoopState = 3;
			}
			break;

		// Outside of donut
		case 3:
			if (!_vm->input()->getLeftClick() && !_vm->input()->getRightClick())
				donutLoopState = 0;
		}

		verbDesc = "";
		overCommand = CMD_WALK;
		overVerb = false;

		// Set strings and action
		if (segment == 0 || segment == 1 || segment == 30 || segment == 31) {
			overCommand = CMD_PICKUP;
			if (verbs[PICKUP] == 0) {
				verbDesc = "Pickup";
				overVerb = true;
			}
		} else if (3 <= segment && segment <= 6) {
			overCommand = CMD_FIGHT;
			if (verbs[FIGHT] == 0) {
				verbDesc = "Fight";
				overVerb = true;
			}
		} else if (9 <= segment && segment <= 12) {
			overCommand = CMD_TALK_TO;
			if (verbs[TALK_TO] == 0) {
				verbDesc = "Talk to";
				overVerb = true;
			}
		} else if (14 <= segment && segment <= 17) {
			overCommand = CMD_LOOK_AT;
			if (verbs[LOOK_AT] == 0) {
				verbDesc = "Look at";
				overVerb = true;
			}
		} else if (19 <= segment && segment <= 22) {
			overCommand = CMD_USE;
			if (verbs[USE] == 0) {
				if (type == 2)
					verbDesc = "Purchase";
				else
					verbDesc = "Use";
				overVerb = true;
			}
		} else if (25 <= segment && segment <= 28) {
			overCommand = CMD_CAST_SPELL;
			if (verbs[CAST_SPELL] == 0) {
				verbDesc = "Cast spell at";
				overVerb = true;
			}
		}

		mouse->switchScope(1, 2);

		if (type != 0) {
			_vm->screen()->drawInventory(inv);
		} else {
			_vm->screen()->drawBackground();
			_vm->screen()->displayDoors();
			_vm->actorMan()->displayAll();
			mouse->display();
			_vm->screen()->drawFightBars();
		}


		// TODO - set strings
		_vm->panel()->setActionDesc(verbDesc);

		if (type == 0) {
			if (_settings.collideType == COLLIDE_CHAR)
				hotspotDesc =
					_vm->database()->getChar(_settings.collideChar)->_desc;
			else if (_settings.collideType == COLLIDE_OBJECT)
				hotspotDesc =
					_vm->database()->getObj(_settings.collideObj)->desc;
			else
				error("Illegal collide type in donut");

			_vm->panel()->setHotspotDesc(hotspotDesc);
		}

		_vm->panel()->update();

		// TODO - do snow

		// Display the donut verbs
		donut->enable(1);
		donut->setPos(hotspotX / 2, hotspotY / 2);
		for (int i = 0; i < ARRAYSIZE(verbs); ++i) {
			donut->setFrame(verbs[i] + i);
			donut->display();
		}
		donut->enable(0);

		_vm->screen()->gfxUpdate();
		if (_vm->shouldQuit())
			return -1;
	}

	if (overVerb) {
		_vm->sound()->playSampleSFX(_vm->_clickSample, false);
	}

	if (type == 0) {
		// TODO: unpause snow
	}

	_vm->screen()->pauseBackground(false);
	_vm->actorMan()->pauseAnimAll(false);

	// TODO - wait for mouse clicks to finish...?

	// Refresh the panel in case the donut was on it
	_vm->panel()->update();

	if (overVerb) {
		_player.command = overCommand;
		return 0;
	} else {
		_vm->panel()->setActionDesc("");
		_vm->panel()->setHotspotDesc("");
		return -1;
	}
}

/**
 * Calculate where the cursor is on the round verb donut
 *
 * @return a number between 0 and 31, equivalent to the
 *         hour in a 32-hour clock
 */
int Game::getDonutSegment(int xPos, int yPos) {
	uint32 tangent;
	int segment;

	if (yPos == 0)
		tangent = 0x40000000;
	else
		tangent = (abs(xPos) << 8) / abs(yPos);

	if (tangent > 0x400)
		segment = 0;
	else if (0x200 < tangent && tangent <= 0x400)
	  segment = 1;
	else if (0x155 < tangent && tangent <= 0x200)
	  segment = 2;
	else if (0x100 < tangent && tangent <= 0x155)
	  segment = 3;
	else if (0xC0 <= tangent && tangent <= 0x100)
	  segment = 4;
	else if (0x80 <= tangent && tangent < 0xC0)
	  segment = 5;
	else if (0x40 <= tangent && tangent < 0x80)
	  segment = 6;
	else // tangent < 0x40
	  segment = 7;


	// Focus on the correct quadrant
	if (xPos < 0) {
		if (yPos < 0)
			return segment + 24;
		else
			return 23 - segment;
	} else if (yPos < 0)
		return 7 - segment;
	else
		return segment + 8;
}

void Game::doInventory(int16 *objectNum, ObjectType *objectType, uint16 shopChar, uint8 mode) {
	bool inInventory = false;
	bool inLoop;
	Character *playerChar = _vm->database()->getChar(0);
	bool leftClick, rightClick;
	Inventory inv;

	do {
		inv.shopChar = shopChar;
		inv.mode = mode;
		inLoop = true;
		inv.selectedBox2 = -1;
		inv.isSelected = false;
		inv.selectedInvObj = -1;
		inv.selectedWeapObj = -1;
		inv.selectedSpellObj = -1;
		inv.blinkLight = 4;
		_vm->panel()->setLocationDesc("");

		_vm->screen()->showMouseCursor(false);

		_vm->screen()->clearScreen();
		_vm->panel()->update();
		_vm->screen()->gfxUpdate();

		// FIXME - why twice?
		_vm->panel()->update();
		_vm->screen()->pauseBackground(true);
		_vm->screen()->drawBackground();
		_vm->screen()->displayDoors();
		_vm->actorMan()->displayAll();
		_vm->screen()->drawFightBars();
		_vm->screen()->createSepia(shopChar);
		_vm->screen()->clearScreen();
		_vm->panel()->update();
		_vm->screen()->gfxUpdate();

		_vm->actorMan()->getMouse()->switchScope(1, 2);
		_vm->actorMan()->getMouse()->display();
		_vm->screen()->showMouseCursor(true);

		_vm->actorMan()->getObjects()->enable(1);
		inv.action = 0;

		int objCount = 0;
		if (mode & 1)
			objCount = playerChar->_inventory.size();

		int weapCount = 0;
		if (mode & 2)
			weapCount = playerChar->_weapons.size();

		int spellCount = 0;
		if (mode & 4)
			spellCount = playerChar->_spells.size();

		inv.mouseState = 0;
		inv.bottomEdge = 83;

		int objRows = (objCount + 2) / 3;
		if (objRows < 1) objRows = 1;
		int weapSpellRows = MAX(weapCount, spellCount);

		inv.totalRows = MAX(objRows, weapSpellRows);

		inv.topEdge =
			inv.bottomEdge - (inv.totalRows - 1) * 40;

		inv.scrollY = 40;

		while (inLoop) {

			// TODO - handle right click

			leftClick = _vm->input()->getLeftClick();
			rightClick = _vm->input()->getRightClick();
			inv.mouseX = _vm->input()->getMouseX();
			inv.mouseY = _vm->input()->getMouseY();

			_settings.mouseX = inv.mouseX * 2;
			_settings.mouseY = inv.mouseY * 2;

			inv.scrollY += 83 - inv.mouseY;

			// Set cursor position
			if (inv.scrollY > inv.bottomEdge) {
				inv.scrollY = inv.bottomEdge;
			} else if (inv.scrollY < inv.topEdge) {
				inv.scrollY = inv.topEdge;
			} else {
				_vm->input()->setMousePos(inv.mouseX, 83);
			}

			inv.mouseRow = (inv.mouseY - inv.scrollY + 20) / 40;
			inv.mouseCol = (inv.mouseX - 20) / 56;

			if (inv.mouseCol >= 5)
				inv.mouseCol = 4;

			if (inv.mouseRow < 0 || inv.mouseCol < 0) {
				inv.selectedBox = -1;
			} else {
				inv.selectedBox = inv.mouseRow * 5 + inv.mouseCol;
			}

			switch (inv.mouseState) {
			case 0:
				if (!leftClick && !rightClick)
					inv.mouseState = 1;
				break;
			case 1:
				inv.selectedBox2 = inv.selectedBox;
				if (leftClick) {
					inv.mouseState = 2;
					inv.offset_3E = 0;
				}

				if (rightClick) {
					if (_settings.objectNum >= 0)
						inv.mouseState = 5;
					else
						inv.mouseState = 4;
				}
				break;
			case 2:
				if (!leftClick) {
					if (inv.mouseY * 2 > 344)
						inv.mouseState = 4;
					else {
						if (inv.isSelected) {
							inv.mouseState = 3;
						} else {
							if (inv.selectedInvObj == 9999) {
								doLookAt(0);
							}
							inv.mouseState = 0;
						}
					}
				}

				if (rightClick)
					inv.mouseState = 0;

				if (inv.isSelected && inv.offset_3E == 0) {
					CommandType cmdBackup;
					int8 donutState;

					_vm->sound()->playSampleSFX(_vm->_clickSample, false);
					cmdBackup = _player.command;

					if (_settings.objectNum < 0 && (mode & 1) && !shopChar) {
						donutState = doDonut(shopChar ? 2 : 1, &inv);
						_vm->input()->setMousePos(_vm->input()->getMouseX(), 83);
					} else {
						_player.command = CMD_USE;
						donutState = false;
					}

					if (donutState != 0) {
						inv.mouseState = 0;
					} else {
						switch (_player.command) {
						case CMD_USE:
							inv.action = 0;
							break;
						case CMD_LOOK_AT:
							inv.action = 1;
						default:
							break;
						}
						inv.mouseState = 3;
					}

					_player.command = cmdBackup;
				}

				inv.offset_3E = 1;
				break;
			case 3:
				inLoop = false;
				break;
			case 4:
				if (!rightClick)
					inv.mouseState = 6;
				_vm->panel()->setActionDesc("");
				_vm->panel()->setHotspotDesc("");
				break;
			case 5:
				_settings.objectNum = -1;
				if (!rightClick)
					inv.mouseState = 1;
				break;
			case 6:
				inLoop = false;
				inv.selectedBox = -1;
			}

			_vm->screen()->drawInventory(&inv);
			_vm->screen()->gfxUpdate();
			_vm->panel()->setActionDesc("");
			_vm->panel()->setHotspotDesc("");

			if (_vm->shouldQuit())
				break;
		}

		_vm->panel()->setLocationDesc(
				_vm->database()->getLoc(playerChar->_lastLocation)->desc);
		_vm->panel()->setActionDesc("");
		_vm->panel()->setHotspotDesc("");
		_vm->screen()->freeSepia();
		_vm->actorMan()->getObjects()->enable(0);
		_vm->screen()->pauseBackground(false);

		// TODO - something with input

		*objectType = OBJECT_NONE;
		*objectNum = -1;

		inInventory = inv.action;

		if (inv.mouseState == 3 && _settings.objectNum != inv.selectedObj) {
			if (inv.selectedInvObj >= 0) {
				*objectType = OBJECT_ITEM;
				*objectNum = inv.selectedInvObj;
			} else if (inv.selectedWeapObj >= 0) {
				*objectType = OBJECT_WEAPON;
				*objectNum = inv.selectedWeapObj;
			} else if (inv.selectedSpellObj >= 0) {
				*objectType = OBJECT_SPELL;
				*objectNum = inv.selectedSpellObj;
			}
		}

		if ((mode & 1) == 0 && *objectNum >= 0 &&
		    _vm->database()->getObj(*objectNum)->isUseImmediate) {
			*objectType = OBJECT_NONE;
			*objectNum = -1;
		}

		_vm->panel()->suppressLoading();

		if (*objectNum == -1 && inInventory) {
			inInventory = false;
		}

		if (*objectNum >= 0 && inInventory) {
			stopNarrator();
			// Look at
			doCommand(5, 1, *objectNum, -1, -1);
			*objectType = OBJECT_NONE;
			*objectNum = -1;
		}

	} while (inInventory);

	stopNarrator();
}

static const char *thidneyShopCostumier[] = {
	"I like the look of this one!",
	"Can you afford such an outfit?",
	"No.",
	"Then I must encourage you to select something else.",
	"That looks great.  It looks like it was made for you.",
	"You know what, I think I\'ll come back later.",
	"Oh no!  That wouldn't suit you at all!",
};

static const char *shahronShopCostumier[] = {
	"I like the look of this one!",
	"Can you afford such an outfit?",
	"Can I afford such an outfit?  You're asking me if I can afford such an outfit?  No.",
	"Then I must encourage you to select something else.",
	"That looks great.  It looks like it was made for you.",
	"You know what, I think I\'ll come back later.",
	"Oh no!  That wouldn't suit you at all!",
};

static const char *dialogueShopkeeper[] = {
	"I'll take this!",
	"Can you afford it?",
	"No.",
	"Then you\'ll have to choose something else.",
	"Sold!",
	"You know what, I think I\'ll come back later.",
};

int16 Game::doActionShop(uint16 charId) {
	assert(charId == 3 || charId == 65);

	Character *playerChar = _vm->database()->getChar(0);
	Character *shopChar = _vm->database()->getChar(charId);

	const char **dialogues;

	_vm->panel()->suppressLoading();
	if (charId == 3) {
		if (_player.selectedChar == 0)
			dialogues = thidneyShopCostumier;
		else
			dialogues = shahronShopCostumier;
	} else {
		dialogues = dialogueShopkeeper;
	}

	int16 objectNum;
	ObjectType objectType;

	doInventory(&objectNum, &objectType, charId, 7);
	{
		Talk talk(_vm);
		talk.init(charId, 999);

		String samplePrefix("sh");
		samplePrefix += shopChar->_name[5];

		if (objectNum < 0) {
			talk.doTalk(0, 0, "youpoo", dialogues[5]);
		} else {
			talk.doTalk(0, 0, "youtake", dialogues[0]);
			if ((_player.selectedChar == 0 && objectNum == 37) ||
				(_player.selectedChar == 1 && objectNum == 38)) {
				objectNum = -1;
				talk.doTalk(1, 0, (samplePrefix + "clit").c_str(), dialogues[6]);
			} else if (objectNum == 94) {
				objectNum = -1;
				doActionPlaySample("nmess061");
			} else if (playerChar->_gold < _vm->database()->getObj(objectNum)->price) {
				objectNum = -1;
				talk.doTalk(1, 0, (samplePrefix + "can").c_str(), dialogues[1]);
				talk.doTalk(0, 1, "youno", dialogues[2]);
				talk.doTalk(1, 0, (samplePrefix + "else").c_str(), dialogues[3]);
			} else {
				talk.doTalk(1, 0, (samplePrefix + "sold").c_str(), dialogues[4]);
			}
		}
	}

	return objectNum;
}

void Game::doActionGotObject(uint16 obj) {
	_cb.data2 = obj;
	_vm->panel()->addObject(obj);

	// Remove object from list
	for (uint i = 0; i < _roomObjects.size(); i++) {
		if (_roomObjects[i].objectId == obj) {
			_roomObjects[i].disappearTimer = 10;
			break;
		}
	}
}

void Game::doActionLostObject(uint16 obj) {
	_vm->panel()->addObject(-obj);

	if (_settings.objectNum == obj) {
		_settings.objectType = OBJECT_NONE;
		_settings.objectNum = -1;
	}
}

#define KOM_LABEL_TEXT(VAR, TEXT) \
	VAR.text = TEXT; \
	VAR.length = strlen(TEXT)

#define KOM_LABEL(VAR, COL, ROW, WRAPCOL, DELAY) \
	VAR.col = COL; \
	VAR.row = ROW; \
	VAR.wrapCol = WRAPCOL; \
	VAR.delay = DELAY

struct Label {
	const char *text;
	uint16 col;
	uint16 row;
	int length;
	uint16 wrapCol;
	int delay;
};

static const char *aliases[] = {
	"Pink Shadow",
	"Julian",
	"The Great Boffo",
	"Lefty",
	"Minty",
	"Bumholio",
	"The Red Baron",
	"Bunty",
	"Sheep Warrior",
	"The Gay Blade",
	"Purple Helmet",
	"Dutch",
	"Vader",
	"Nutter",
	"Auntie McCassar",
	"Toby-Wan"
};

static const char *madeIn[] = {
	"a nice ceramic",
	"the back seat!",
	"Korea",
	"Hong Kong",
	"England",
	"America",
	"a hurry!",
	"just 5 minutes!",
	"a motel room!",
	"error!",
	"instalments!"
};

static const char *invincible[] = {
	"More than you!",
	"Indestructable!",
	"Rock Hard!",
	"I D D Q D",
	"Full On!",
	"Like Connery!",
	"'I work out!'",
	"Ready to rumble!",
	"Feelin' lucky?",
	"Bulletproof!"
};

static const char *noMoney[] = {
	"None!",
	"Bankrupt!",
	"Liquidated!",
	"Spent it all!",
	"Big fat zero!",
	"Insolvent!",
	"1-800-BROKE",
	"Yes please!",
	"'Gold? Pah!'",
	"Overdrawn!"
};

static const char *skills[] = {
	"Fart at will!",
	"X-Ray hearing!",
	"Waterproof 500m",
	"Glow in the dark!",
	"Origami!",
	"Smells of egg!",
	"Shrinks when cold!",
	"Belly ripple!",
	"Armpit noises!",
	"Taxidermy!",
	"Lysdexia!",
	"Maths!",
	"Bedwetting!",
	"Needlepoint!",
	"Plays accordian!"
};

void Game::doLookAt(int charId, int pauseTimer, bool showBackground) {

	Character *chr = _vm->database()->getChar(charId);
	Actor *act = _vm->actorMan()->get(chr->_actorId);

	if (chr->_lastLocation != _vm->database()->getChar(0)->_lastLocation)
		return;

	stopNarrator();
	// TODO: stop greeting
	_vm->screen()->pauseBackground(true);
	_vm->screen()->showMouseCursor(false);

	if (chr->_scopeInUse == 12) {
		chr->_spriteTimer = 0;
		chr->_spriteCutState = 0;
		chr->_isBusy = false;
	}

	int delayTimer = 0;
	int x = chr->_screenX * 256;
	int y = chr->_screenY * 256;
	int z = chr->_start5 * 256 * 88 / 60;
	int xOffset = (256 * 132 - x) / 20;
	int yOffset = (256 * 260 - y) / 20;
	int zOffset = (256 * 256 - z) / 20;
	int loopState = 0;
	int loopTimer = 4;
	int charScope = chr->_scopeWanted;
	int spinTimer = 0;
	int currRow = 15;
	const int rowHeight = 15;
	Label labels[17];
	char hitPoints[16];
	char spellPoints[16];
	char gold[16];
	int displayedObject = -1;
	int objRatio = 0;
	int16 hash[6];

	// Generate codename hash
	for (int i = 0; i < 6; i++) {
		if (i % 2 == 0) {
			hash[i] = chr->_name[i] ^ chr->_name[i+1];
		} else {
			hash[i] = chr->_name[i] | chr->_name[i+1];
		}

		int j = i + 1;
		while (j > i) {
			if (j % 2 == 0) {
				if (chr->_name[j]) {
					hash[i] ^= 0;
				} else {
					hash[i] ^= 0;
				}
			} else {
				hash[i] ^= chr->_name[j];
			}
			j++;
			if (j >= 6)
				j = 0;
		}
	}

	// Set labels

	KOM_LABEL_TEXT(labels[0], chr->_desc);
	KOM_LABEL(labels[0], 135, 5, 319, 0);

	currRow += rowHeight;

	KOM_LABEL_TEXT(labels[5], "AKA:");
	KOM_LABEL(labels[5], 122, currRow, 319, -10);

	KOM_LABEL(labels[6], 205, currRow, 319, -30);

	currRow += rowHeight;

	KOM_LABEL_TEXT(labels[1], "Health:");
	KOM_LABEL(labels[1], 122, currRow, 319, -20);

	if (chr->_hitPoints == -1) {
		KOM_LABEL_TEXT(labels[2], invincible[hash[1] % ARRAYSIZE(invincible)]);
	} else {
		Common::sprintf_s(hitPoints, sizeof(hitPoints), "%d/%d", chr->_hitPoints, chr->_hitPointsMax);
		KOM_LABEL_TEXT(labels[2], hitPoints);
	}
	KOM_LABEL(labels[2], 205, currRow, 319, -40);

	currRow += rowHeight;

	KOM_LABEL_TEXT(labels[3], "Spellpower:");
	KOM_LABEL(labels[3], 122, currRow, 319, -30);

	Common::sprintf_s(spellPoints, sizeof(spellPoints), "%d", chr->_spellPoints);
	KOM_LABEL_TEXT(labels[4], spellPoints);
	KOM_LABEL(labels[4], 205, currRow, 319, -50);

	currRow += rowHeight;

	KOM_LABEL_TEXT(labels[9], "Made in:");
	KOM_LABEL(labels[9], 122, currRow, 319, -40);

	KOM_LABEL(labels[10], 205, currRow, 319, -60);

	currRow += rowHeight;

	KOM_LABEL_TEXT(labels[7], "Serial #:");
	KOM_LABEL(labels[7], 122, currRow, 319, -50);

	KOM_LABEL_TEXT(labels[8], chr->_name);
	KOM_LABEL(labels[8], 205, currRow, 319, -70);

	currRow += rowHeight;

	KOM_LABEL_TEXT(labels[11], "Gold:");
	KOM_LABEL(labels[11], 122, currRow, 319, -60);

	if (chr->_gold == 0) {
		KOM_LABEL_TEXT(labels[12], noMoney[hash[1] % ARRAYSIZE(noMoney)]);
	} else {
		Common::sprintf_s(gold, sizeof(gold), "%d", chr->_gold);
		KOM_LABEL_TEXT(labels[12], gold);
	}
	KOM_LABEL(labels[12], 205, currRow, 319, -80);

	currRow += rowHeight;

	KOM_LABEL_TEXT(labels[13], "Secret skill:");
	KOM_LABEL(labels[13], 122, currRow, 319, -100);

	KOM_LABEL(labels[14], 205, currRow, 319, -115);

	KOM_LABEL_TEXT(labels[15], ""); // Object name
	KOM_LABEL(labels[15], 15, 170, 220, 0);

	KOM_LABEL_TEXT(labels[16], ""); // "Exit" or "More"
	KOM_LABEL(labels[16], 250, 190, 319, 0);

	if (charId == 0) {
		if (_player.selectedChar) {
			KOM_LABEL_TEXT(labels[6], "Mama Reese");
			KOM_LABEL_TEXT(labels[10], "Heaven");
			KOM_LABEL_TEXT(labels[14], "Mud wrestling!");
		} else {
			KOM_LABEL_TEXT(labels[6], "Greenfinger");
			KOM_LABEL_TEXT(labels[10], "Falkirk");
			KOM_LABEL_TEXT(labels[14], "Accountancy!");
		}
	} else {
		KOM_LABEL_TEXT(labels[6], aliases[hash[1] % ARRAYSIZE(aliases)]);
		KOM_LABEL_TEXT(labels[10], madeIn[hash[3] % ARRAYSIZE(madeIn)]);
		KOM_LABEL_TEXT(labels[14], skills[hash[4] % ARRAYSIZE(skills)]);
	}

	// Copy all objects to one list
	int objectCount = chr->_inventory.size() + chr->_weapons.size() + chr->_spells.size();

	int *objects = new int[objectCount];
	int currObject = 0;
	for (Common::List<int>::iterator objId = chr->_inventory.begin(); objId != chr->_inventory.end(); ++objId, ++currObject)
		objects[currObject] = *objId;
	for (Common::List<int>::iterator objId = chr->_weapons.begin(); objId != chr->_weapons.end(); ++objId, ++currObject)
		objects[currObject] = *objId;
	for (Common::List<int>::iterator objId = chr->_spells.begin(); objId != chr->_spells.end(); ++objId, ++currObject)
		objects[currObject] = *objId;
	currObject = -1;

	while (loopState != 4) {

		// Position the character on screen
		if (loopState > 1) {
			x = 132;
			x = 256 * 132;
			y = 260;
			y = 256 * 260;
			z = 256 * 256;
		} else {
			x += xOffset;
			y += yOffset;
			z += zOffset;
		}

		// This is only enabled when talking to Krystal at
		// King Afro's palace, and choosing conversation
		// options 3, 1
		if (loopState == 0 && showBackground) {
			_vm->screen()->drawBackground();
			_vm->screen()->displayDoors();
			_vm->actorMan()->pauseAnimAll(true);
			act->enable(0);
			_vm->actorMan()->displayAll();
			act->enable(1);
			_vm->actorMan()->pauseAnimAll(false);
		} else {
			_vm->screen()->clearScreen();
		}

		act->setPos(x / 512, y / 512);
		act->setMaskDepth(0, 2);
		act->setRatio(262144 * 256 / z, 262144 * 256 / z);
		act->display();

		// Draw current object
		if (loopState >= 3) {
			if (_vm->input()->getLeftClick()) {
				if (++currObject == objectCount) {
					loopState = 4;
					currObject--;
				}
			}

			if (currObject != displayedObject) {
				KOM_LABEL_TEXT(labels[15], _vm->database()->getObj(objects[currObject])->desc);
				displayedObject = currObject;
				objRatio = 8;

				if (currObject == objectCount - 1) {
					KOM_LABEL_TEXT(labels[16], " ...EXIT");
				}
			}

			if (displayedObject != -1) {
				Actor *obj = _vm->actorMan()->getObjects();
				obj->setEffect(0);
				obj->setPos(220, 180);
				obj->setMaskDepth(0, 3);
				obj->setRatio(1024 / objRatio, 1024 / objRatio);
				obj->setFrame(objects[displayedObject] + 1);
				obj->display();

				// Animate the object
				if (objRatio > 1) {
					objRatio--;
				}
			}

		}

		// Write labels
		if (loopState >= 1) {
			for (int i = 0; i < ARRAYSIZE(labels); i++) {
				labels[i].delay++;

				int endPos = MAX(0, MIN(labels[i].length, labels[i].delay));
				char str[42];
				Common::strlcpy(str, labels[i].text, sizeof(str));

				// Special handling of object name - word by word
				if (i == 15) {
					while (str[endPos] != '\0' && str[endPos] != ' ')
						endPos++;
					labels[i].delay = endPos;
				}

				str[endPos] = '\0';

				_vm->screen()->writeTextWrap(_vm->screen()->screenBuf(), str,
						labels[i].row, labels[i].col, labels[i].wrapCol, 31, false);
			}

		}

		delayTimer++;
		if (delayTimer > 256 * 64) {
			delayTimer = 0;
			for (int i = 0; i < ARRAYSIZE(labels); i++) {
				labels[i].delay -= 256 * 64;
			}
		}

		_vm->screen()->gfxUpdate();

		// Rotate character
		if (spinTimer) {
			spinTimer--;
		} else {
			spinTimer = 3;
			switch (charScope - 4) {
			case 0:
				charScope = 5;
				break;
			case 1:
				charScope = 6;
				break;
			case 2:
				charScope = 7;
				break;
			case 3:
				charScope = 8;
				break;
			case 4:
				charScope = 9;
				break;
			case 5:
				charScope = 10;
				break;
			case 6:
				charScope = 11;
				break;
			case 7:
				charScope = 4;
				break;
			default:
				charScope = 8;
			}

			chr->setScope(charScope);
		}

		if (loopTimer) {
			loopTimer--;
		}

		if (_vm->input()->getRightClick() || _vm->shouldQuit() /* TODO: || space key */) {
			loopState = 4;
		}

		switch (loopState) {
		case 0:
			if (loopTimer == 0) {
				loopState = 1;
				loopTimer = 16;
			}
			break;
		case 1:
			if (loopTimer == 0) {
				loopState = 2;
			}
			break;
		case 2:
			if (objectCount == 0) {
				KOM_LABEL_TEXT(labels[15], "Has no objects");
				KOM_LABEL_TEXT(labels[16], " ...EXIT");
			} else {
				currObject = 0;
				KOM_LABEL_TEXT(labels[16], "More...");
			}

			loopState = 3;
			break;
		default:
			break;
		}

		if (pauseTimer > 0) {
			pauseTimer--;
			if (pauseTimer == 0)
				loopState = 4;
		}
	}

	delete[] objects;

	_vm->screen()->showMouseCursor(true);
	_vm->screen()->pauseBackground(false);
	_vm->screen()->clearScreen();
}

static const char *definiteArticle[] = {
	"The ",
	"THE ",
	"Der ",
	"DER ",
	"Die ",
	"DIE ",
	"Das ",
	"DAS ",
	"Le ",
	"LE ",
	"La ",
	"LA ",
	"Les ",
	"LES ",
	NULL
};

void Game::declareNewEnemy(int16 enemy) {
	_player.enemyId = enemy;
	if (_player.enemyId != -1) {
		if (_player.enemyId != _player.lastEnemy) {
			Character *chr = _vm->database()->getChar(_player.enemyId);

			_player.lastEnemy = _player.enemyId;
			_player.enemyHitPoints = _player.enemyHitPointsOld = chr->_hitPoints;

			const char *desc = chr->_desc;

			// Strip definite article
			bool found = false;
			for (int i = 0; definiteArticle[i] != NULL && !found; i++) {
				int len = strlen(definiteArticle[i]);
				if (strncmp(desc, definiteArticle[i], len) == 0) {
					desc += len;
					found = true;
				}
			}
			Common::strlcpy(_player.enemyDesc, desc, sizeof(_player.enemyDesc));
		}
	}
}

void Game::doFight(int enemyId, int weaponId) {
	Character *playerChar = _vm->database()->getChar(0);
	Character *enemy = _vm->database()->getChar(enemyId);

	// Calculate player attack
	if (enemy->_isMortal) {
		int damageRange;
		int damage;

		// Use a weapon
		if (weaponId != -1) {
			Object *weapon = _vm->database()->getObj(weaponId);
			damageRange = MAX(weapon->maxDamage - weapon->minDamage, 1);
			damage = _vm->rnd()->getRandomNumber(damageRange - 1) + weapon->minDamage + 1;
		} else {
			damageRange = MAX(playerChar->_maxDamage - playerChar->_minDamage, 1);
			damage = _vm->rnd()->getRandomNumber(damageRange - 1) + playerChar->_minDamage + 1;
		}

		int baseDamageToEnemy = MAX(playerChar->_strength - enemy->_defense, 0);
		enemy->_hitPoints -= baseDamageToEnemy + damage;
		if (enemy->_hitPoints <= 0) {
			enemy->unsetSpell();
			enemy->_hitPoints = 0;
			enemy->_isAlive = false;
			if (enemy->_spellMode != 0)
				doActionUnsetSpell(enemyId, enemy->_spellMode);
			enemy->_mode = 0;
			enemy->_modeCount = 0;
			enemy->_spellMode = 0;
			enemy->_spellDuration = 0;
			doActionSetSpell(enemyId, 0);
		}
	}

	// Calculate enemy attack
	int enemyDamageRange;
	int enemyDamage;

	// Use held weapon unless it's the thermal detonator
	if (!enemy->_weapons.empty() && _vm->database()->getObj(*enemy->_weapons.begin())->minDamage != -10) {
		Object *weapon = _vm->database()->getObj(*enemy->_weapons.begin());
		enemyDamageRange = MAX(weapon->maxDamage - weapon->minDamage, 1);
		enemyDamage = _vm->rnd()->getRandomNumber(enemyDamageRange - 1) + weapon->minDamage + 1;
	} else {
		enemyDamageRange = MAX(enemy->_maxDamage - enemy->_minDamage, 1);
		enemyDamage = _vm->rnd()->getRandomNumber(enemyDamageRange - 1) + enemy->_minDamage + 1;
	}

	// Damage player
	int baseDamageToPlayer = MAX(enemy->_strength - playerChar->_defense, 0);
	playerChar->_hitPoints -= baseDamageToPlayer + enemyDamage;
	if (playerChar->_hitPoints <= 0) {
		playerChar->unsetSpell();
		playerChar->_hitPoints = 0;
		playerChar->_isAlive = false;
	}
}

void Game::doNPCFight(int attackerId, int defenderId) {
	Character *attacker = _vm->database()->getChar(attackerId);
	Character *defender = _vm->database()->getChar(defenderId);

	if (!defender->_isMortal)
		return;

	int damageRange = MAX(attacker->_maxDamage - attacker->_minDamage, 1);
	int attackDamage = _vm->rnd()->getRandomNumber(damageRange - 1) + attacker->_minDamage + 1;

	// Use held weapon
	if (!attacker->_weapons.empty()) {
		int weaponId = *attacker->_weapons.begin();
		if (attackerId == 0)
			weaponId = _settings.lastWeaponUsed;

		Object *weapon = _vm->database()->getObj(weaponId);

		// Don't use thermal detonator
		if (weapon->minDamage == -10)
			return;

		int weaponRange = MAX(weapon->maxDamage - weapon->minDamage, 1);
		attackDamage = _vm->rnd()->getRandomNumber(weaponRange - 1) + weapon->minDamage + 1;
	}

	int baseDamage = MAX(attacker->_strength - defender->_defense, 0);
	defender->_hitPoints -= baseDamage + attackDamage;
	if (defender->_hitPoints <= 0) {
		defender->unsetSpell();
		defender->_hitPoints = 0;
		defender->_isAlive = false;
		if (defender->_spellMode != 0)
			doActionUnsetSpell(defenderId, defender->_spellMode);
		defender->_mode = 0;
		defender->_modeCount = 0;
		defender->_spellMode = 0;
		defender->_spellDuration = 0;
		doActionSetSpell(defenderId, 0);
	}
}

bool Game::castSpell(int16 charId, int16 spellId, int16 target) {
	Character *magicChar = NULL;
	Spell *spell = NULL;

	// Find unused magic actor
	for (int i = 0; i < ARRAYSIZE(_spells); i++) {
		magicChar = _vm->database()->getMagicChar(i);
		if (magicChar->_id == -1) {
			spell = &_spells[i];
			break;
		}
	}
	if (magicChar->_id != -1)
		return false;

	Character *targetChar = _vm->database()->getChar(target);

	// Dark lord spell
	if (charId == -1) {

		// Target at dark tower, or reachable from there
		if (targetChar->_lastLocation == 83 || _vm->database()->loc2loc(83, targetChar->_lastLocation) != -1) {
			magicChar->_lastLocation = 83;
			magicChar->_lastBox = 0;
		} else {
			magicChar->_lastLocation = 73;
			magicChar->_lastBox = 10;
		}
		magicChar->_screenX = _vm->database()->getMidX(magicChar->_lastLocation, magicChar->_lastBox);
		magicChar->_screenY = _vm->database()->getMidY(magicChar->_lastLocation, magicChar->_lastBox);
		magicChar->_start3 = magicChar->_screenX * 256;
		magicChar->_start4 = magicChar->_screenY * 256;
		magicChar->_start5 = _vm->database()->getZValue(magicChar->_lastLocation, magicChar->_lastBox, magicChar->_start4);
		magicChar->_id = 9999;
		magicChar->_walkSpeed = 1800;
		spell->holdDuration = 0;
		spell->doneHold = true;
		spell->duration = 2400;

	// Regular caster
	} else {
		Character *casterChar = _vm->database()->getChar(charId);

		magicChar->_lastBox = casterChar->_lastBox;
		magicChar->_lastLocation = casterChar->_lastLocation;
		magicChar->_start3 = casterChar->_start3;
		magicChar->_screenX = casterChar->_start3 / 256;
		magicChar->_start4 = casterChar->_start4;
		magicChar->_screenY = casterChar->_start4 / 256;
		magicChar->_start5 = casterChar->_start5;
		magicChar->_id = charId;
		magicChar->_walkSpeed = 2000;
		spell->holdDuration = 16;
		spell->doneHold = false;
		spell->duration = 160;
	}

	magicChar->_relativeSpeed = 1024;
	magicChar->_stopped = false;
	magicChar->_screenH = -0x3C0000;
	magicChar->_height = -0x5A0000;
	magicChar->_screenHDelta = -0x90000;
	spell->spellId = spellId;
	spell->targetId = target;

	if (charId <= 0) {
		if (_player.selectedChar == 0)
			magicChar->_walkSpeed = 2700;
		else
			magicChar->_walkSpeed = 3000;
	} else if (charId == 72) { // Master Ringwraith
		magicChar->_walkSpeed = 2700;
	}
	return true;
}

void Game::doSpellAttack(int16 target, int16 spellId) {
	Object *spellObj = _vm->database()->getObj(spellId);
	if (spellObj->type != 3)
		return;

	Character *targetChar = _vm->database()->getChar(target);
	if (!targetChar->_isMortal)
		return;

	int damageRange = MAX(spellObj->maxDamage - spellObj->minDamage, 1);
	int damage = _vm->rnd()->getRandomNumber(damageRange - 1) + spellObj->minDamage + 1;

	targetChar->_hitPoints -= damage;
	if (targetChar->_hitPoints <= 0) {
		targetChar->unsetSpell();
		targetChar->_hitPoints = 0;
		targetChar->_isAlive = false;
	}
}

void Game::doActionSetSpell(int16 target, int16 spellType) {
	Character *targetChar = _vm->database()->getChar(target);
	targetChar->_spellMode = spellType;

	switch (spellType) {
	case 0:
	case 6: // Invisibility
	case 7: // Incontinence
	case 8: // Left the gas on
	case 12: // Teleport
		break;
	case 1: // Cabbage
		targetChar->_screenH = -0x5A0000;
		targetChar->_screenHDelta = -0xA0000;
		targetChar->_height = 0;
		if (target != 0) {
			declareNewEnemy(target);
			_player.enemyFightBarTimer = 110;
		}
		targetChar->setScope(102);
		break;
	case 2: // Slow motion
		targetChar->_walkSpeed /= 2;
		targetChar->_animSpeed *= 2;
		targetChar->_scopeInUse = -666;
		break;
	case 3: // Dwarf
		targetChar->_offset14 = 0x30000;
		targetChar->_offset1c = 0x2BC00;
		targetChar->_offset20 = 0x20000;
		targetChar->_offset28 = 0x25800;
		if (target != 0) {
			declareNewEnemy(target);
			_player.enemyFightBarTimer = 110;
		}
		break;
	case 4: // Floater
		targetChar->_screenH = -0x3C0000;
		targetChar->_height = -0x5A0000;
		break;
	case 5: // Shield
		warning("TODO: pulseFade");
		_player.colgateTimer = 20;
		break;
	case 9: // Airstrike
	case 10: // WizBang
	case 11: // Kick in the shins
		if (target != 0) {
			declareNewEnemy(target);
			_player.enemyFightBarTimer = 110;
		}
		break;
	default:
		error("Invalid spell type in doActionSetSpell");
	}
}

void Game::doActionUnsetSpell(int16 target, int16 spellType) {
	Character *targetChar = _vm->database()->getChar(target);
	targetChar->_spellMode = spellType;

	switch (spellType) {
	case 0:
	case 6: // Invisibility
	case 7: // Incontinence
	case 8: // Left the gas on
	case 9: // Airstrike
	case 10: // WizBang
	case 11: // Kick in the shins
	case 12: // Teleport
		break;
	case 1: // Cabbage
		targetChar->_scopeInUse = -666;
		targetChar->_offset20 = 0x40000;
		targetChar->_ratioY = 0x10000;
		targetChar->_offset28 = -0x25800;
		break;
	case 2: // Slow motion
		targetChar->_walkSpeed *= 2;
		targetChar->_animSpeed /= 2;
		targetChar->_scopeInUse = -666;
		break;
	case 3: // Dwarf
		targetChar->_offset14 = 0x40000;
		targetChar->_offset20 = 0x40000;
		if (_cb.talkInitialized) {
			targetChar->_offset1c = 0;
			targetChar->_offset28 = 0;
		} else {
			targetChar->_offset1c = -0x25800;
			targetChar->_offset28 = -0x25800;
		}
		if (target != 0) {
			declareNewEnemy(target);
			_player.enemyFightBarTimer = 110;
		}
		break;
	case 4: // Floater
		targetChar->_screenHDelta = 0;
		targetChar->_height = 0;
		break;
	case 5: // Shield
		_vm->sound()->playSampleSFX(_vm->_colgateOffSample, false);
		_player.colgateTimer = 15;
		break;
	default:
		error("Invalid spell type in doActionUnsetSpell");
	}
}

void Game::exeUse() {
	Character *playerChar = _vm->database()->getChar(0);
	playerChar->_isBusy = false;

	if (_settings.objectNum >= 0) {

		if (_settings.object2Num >= 0) {
			doCommand(3, 1, _settings.objectNum, 1, _settings.object2Num);
		} else switch (_settings.collideType) {
		case COLLIDE_CHAR:
			doCommand(3, 1, _settings.objectNum, 2, _player.collideNum);
			break;
		case COLLIDE_OBJECT:
			doCommand(3, 1, _settings.objectNum, 1, _player.collideNum);
			break;
		default:
			error("Illegal type in exeUse");
		}

	} else switch (_settings.collideType) {
	case COLLIDE_CHAR:
		doCommand(3, 1, _player.collideNum, -1, -1);
		break;
	case COLLIDE_OBJECT:
		doCommand(3, 1, _player.collideNum, -1, -1);
		break;
	default:
		error("Illegal type in exeUse");
	}

	_settings.objectNum = _settings.object2Num = -1;
	_player.command = CMD_NOTHING;
	_player.commandState = 0;
	_player.collideType = COLLIDE_NONE;
	_player.collideNum = -1;
}

void Game::exeTalk() {
	_vm->database()->getChar(0)->_isBusy = false;
	doCommand(2, 2, _player.collideNum, -1, -1);
	_player.command = CMD_TALK_TO;
	_player.commandState = 0;
	_player.collideType = COLLIDE_NONE;
	_player.collideNum = -1;
}

void Game::exePickup() {
	Character *playerChar = _vm->database()->getChar(0);
	int distance;
	switch (_player.commandState) {
	case 1:
		playerChar->_isBusy = false;
		_cb.data2 = -1;
		doCommand(4, 1, _player.collideNum, -1, -1);

		if (_cb.data2 >= 0) {
			_player.commandState = 2;
			playerChar->_lastDirection = 3;
			doActionSpriteScene("", 0, -1, -1);
		} else {
			_player.commandState = 0;
		}
		break;
	case 2:
		if (playerChar->_spriteCutState == 0)
			_player.commandState = 3;

		distance = (playerChar->_screenY -
			_vm->database()->getMidY(
				playerChar->_lastLocation,
				_vm->database()->getObj(_player.collideNum)->box)
			) * playerChar->_start5 / 256;

		if (distance < 50)
			playerChar->_spriteType = 1;
		else if (distance <= 120)
			playerChar->_spriteType = 2;
		else
			playerChar->_spriteType = 3;
		break;
	case 3:
		_player.command = CMD_NOTHING;
		_player.commandState = 0;
		_player.collideType = COLLIDE_NONE;
		_player.collideNum = -1;
		break;
	default:
		break;
	}
}

void Game::exeLookAt() {
	assert(_player.collideType != COLLIDE_BOX);

	_vm->database()->getChar(0)->_isBusy = false;

	switch (_player.collideType) {
	case COLLIDE_CHAR:
		doCommand(5, 2, _player.collideNum, -1, -1);
		break;
	case COLLIDE_OBJECT:
		doCommand(5, 1, _player.collideNum, -1, -1);
		break;
	default:
		break;
	}

	_player.command = CMD_NOTHING;
	_player.commandState = 0;
	_player.collideType = COLLIDE_NONE;
	_player.collideNum = -1;
}

void Game::exeFight() {
	switch (_player.commandState) {
	case 1: // Dust clouds
		_settings.fightState = 2;
		break;
	default:
		break;
	}
}

void Game::exeMagic() {
	switch (_player.commandState) {
	case 1: {
		Character *playerChar = _vm->database()->getChar(0);

		// Not enough spell points
		if (playerChar->_spellPoints < _vm->database()->getObj(_settings.objectNum)->spellCost) {
			doActionSpriteScene("", 0, -1, -1);
			playerChar->_spriteType = 4;
			playerChar->_lastDirection = 4;
			_player.commandState = 2;
			_vm->sound()->playSampleSFX(_vm->_fluffSample, false);

		// Spell succeeded
		} else if (castSpell(0, _settings.objectNum, _player.collideNum)) {
			playerChar->_spellPoints -= _vm->database()->getObj(_settings.objectNum)->spellCost;
			doActionSpriteScene("", 0, -1, -1);
			playerChar->_spriteType = 4;
			playerChar->_lastDirection = 4;
			_player.commandState = 2;
			_vm->sound()->playSampleSFX(_vm->_magicSample, false);

		// Spell failed
		} else {
			_player.command = CMD_WALK;
			_player.commandState = 0;
			_player.collideType = COLLIDE_NONE;
			_player.collideNum = -1;
			_vm->database()->getChar(0)->_isBusy = false;
			_vm->sound()->playSampleSFX(_vm->_fluffSample, false);
		}
		break;
	}
	case 2:
		_player.commandState = 3;
		break;
	case 3:
		_settings.objectNum = -1;
		_settings.object2Num = -1;
		_player.command = CMD_WALK;
		_player.commandState = 0;
		_player.collideType = COLLIDE_NONE;
		_player.collideNum = -1;
		_vm->database()->getChar(0)->_isBusy = false;
		break;
	default:
		error("Invalid state in exeMagic");
		break;
	}
}

} // End of namespace Kom
