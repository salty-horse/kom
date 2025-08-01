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
#include "common/file.h"
#include "common/textconsole.h"

#include "kom/kom.h"
#include "kom/character.h"
#include "kom/actor.h"
#include "kom/database.h"
#include "kom/game.h"
#include "kom/panel.h"
#include "kom/sound.h"


namespace Kom {

using Common::String;
using Common::Path;

void Character::moveChar(bool param) {
	int16 nextLoc, targetBox, targetBoxX, targetBoxY, nextBox;

	//if (_id != 21) return; //FIXME FIXME FIXME - don't forget to delete

	if (_lastLocation == 0) return;

	// Find the final box we're supposed to reach
	if (_gotoLoc != _lastLocation) {

		if (_gotoLoc >= 0 && _lastLocation >= 0)
			nextLoc = _vm->database()->loc2loc(_lastLocation, _gotoLoc);
		else
			nextLoc = -1;

		if (nextLoc > 0) {
			targetBox = _vm->database()->getExitBox(_lastLocation, nextLoc);

			if (_lastLocation >= 0 && targetBox >= 0) {
				Box *b = _vm->database()->getBox(_lastLocation, targetBox);
				targetBoxX = (b->x2 - b->x1) / 2 + b->x1;
				targetBoxY = (b->y2 - b->y1) / 2 + b->y1;
			} else {
				targetBoxX = 319;
				targetBoxY = 389;
			}

			_gotoX = targetBoxX;
			_gotoY = targetBoxY;

		} else
			targetBox = _lastBox;

	} else {
		targetBox = _vm->database()->whatBox(_lastLocation, _gotoX, _gotoY);
	}

	assert(targetBox != -1);

	nextBox = _vm->database()->box2box(_lastLocation, _lastBox, targetBox);

	int16 x, y, xMove, yMove;

	// Walk to the center of the current box
	if (targetBox == _lastBox) {
		x = _gotoX;
		y = _gotoY;

	// Stand in place
	} else if (nextBox == -1) {
		x = _screenX;
		y = _screenY;

	// Walk from box to box
	} else if (_vm->database()->isInLine(_lastLocation, nextBox, _screenX, _screenY)) {
		if (_lastLocation >= 0 && nextBox >= 0) {
			Box *b = _vm->database()->getBox(_lastLocation, nextBox);
			x = b->x1 + (b->x2 - b->x1) / 2;
			y = b->y1 + (b->y2 - b->y1) / 2;
		} else {
			x = 319;
			y = 389;
		}

	} else {
		x = _vm->database()->getMidOverlapX(_lastLocation, _lastBox, nextBox);
		y = _vm->database()->getMidOverlapY(_lastLocation, _lastBox, nextBox);
	}

	xMove = (x - _screenX);
	yMove = (y - _screenY) * 2;

	if (abs(xMove) + abs(yMove) == 0) {
		_somethingX = _somethingY = 0;
	} else {
		int thing;
		assert(_start5 != 0);

		if (_relativeSpeed == 1024) {
			thing = _walkSpeed * 65536 / ((abs(xMove) + abs(yMove)) * _start5);
		} else
			thing = _walkSpeed * _relativeSpeed * 65536 / ((abs(xMove) + abs(yMove)) * _start5);

		// Now we should set 64h and 68h
		_somethingX = xMove * thing / 256;
		_somethingY = yMove * thing / 256;

		if (abs(xMove) * 256 < abs(_somethingX)) {
			_start3 = x * 256;
			_somethingX = 0;
		}

		if (abs(yMove) * 256 < abs(_somethingY)) {
			_start4 = y * 256;
			_somethingY = 0;
		}
	}

	int boxAfterStep, boxAfterStep2;

	boxAfterStep = _lastBox;
	_somethingY /= 2;

	if (!param) {
		Box *currBox;

		boxAfterStep2 = _vm->database()->whatBox(_lastLocation,
				(_start3 + _somethingX) / 256,
				(_start4 + _somethingY) / 256);

		// TODO : check if box doesn't exist?
		currBox = _vm->database()->getBox(_lastLocation, boxAfterStep2);

		if (currBox->attrib == 8)
			boxAfterStep2 = -1;

	} else {
		boxAfterStep2 = _vm->database()->whatBoxLinked(_lastLocation, _lastBox,
				(_start3 + _somethingX) / 256,
				(_start4 + _somethingY) / 256);
	}

	if (boxAfterStep2 != -1) {
		boxAfterStep = boxAfterStep2;
	} else {
		Box *currBox;

		if (!param && nextBox == targetBox) {
			_lastBox = targetBox;
			boxAfterStep = targetBox;
		}

		currBox = _vm->database()->getBox(_lastLocation, _lastBox);

		if (_start3 + _somethingX < currBox->x1 * 256) {
			if (_start4 + _somethingY < currBox->y1 * 256) {
				_somethingX = currBox->x1 * 256 - _start3;
				_somethingY = currBox->y1 * 256 - _start4;
			} else {
				if (_start4 + _somethingY > currBox->y2 * 256) {
					_somethingY = currBox->y2 * 256 - _start4;
				}
				_somethingX = currBox->x1 * 256 - _start3;
			}
		} else if (_start3 + _somethingX > currBox->x2 * 256) {
			if (_start4 + _somethingY < currBox->y1 * 256) {
				_somethingX = currBox->x2 * 256 - _start3;
				_somethingY = currBox->y1 * 256 - _start4;
			} else {
				if (_start4 + _somethingY > currBox->y2 * 256) {
					_somethingY = currBox->y2 * 256 - _start4;
				}
				_somethingX = currBox->x2 * 256 - _start3;
			}
		} else if (_start4 + _somethingY < currBox->y1 * 256) {
			_somethingY = currBox->y1 * 256 - _start4;
		} else if (_start4 + _somethingY > currBox->y2 * 256) {
			_somethingY = currBox->y2 * 256 - _start4;
		}
	}

	if (nextBox == -1 && _somethingX == 0 && _somethingY == 0) {
		_direction = 0;
		_stopped = true;
	} else {
		_stopped = false;
	}

	_start3 += _somethingX;
	_start4 += _somethingY;
	_screenX = _start3 / 256;
	_screenY = _start4 / 256;
	_lastBox = boxAfterStep;

	if (_lastLocation != 0 || _lastBox != 0) {
		_priority = _vm->database()->getBox(_lastLocation, _lastBox)->priority;
	} else {
		_priority = -1;
	}

	_start5 = _vm->database()->getZValue(_lastLocation, _lastBox, _start4);

	// Set direction
	if (_lastLocation == _vm->database()->getChar(0)->_lastLocation) {
		int32 xVector = (_start3 - _start3Prev) * _start5 / 256;
		int32 yVector = (_start5 - _start5PrevPrev) * 256;

		// FIXME: verify the correct condition
		if (_direction != 0 && /*(*/abs(abs(xVector) - abs(yVector)) < 127/*& 0xFFFFFF80) == 0*/) {
			_direction = _lastDirection;
		} else if (abs(xVector) >= abs(yVector)) {
			if (xVector < -4)
				_direction = 1; // left
			else if (xVector > 4)
				_direction = 2; // right
			else
				_direction = 0;
		} else {
			if (yVector < -4)
				_direction = 4; // front
			else if (yVector > 4)
				_direction = 3; // back
			else
				_direction = 0;
		}
	}

	// Set wanted scope
	switch (_direction) {
	case 0:
		switch (_lastDirection - 1) {
		case 0:
			_scopeWanted = 9; // face left
			break;
		case 1:
			_scopeWanted = 7; // face right
			break;
		case 2:
			_scopeWanted = 4; // face back
			break;
		case 3:
		default:
			_scopeWanted = 8; // face front
			break;
		}
		break;
	case 1:
		if (_lastDirection == 1)
			_scopeWanted = 1; // walk left
		else
			_lastDirection = 1;
		break;
	case 2:
		if (_lastDirection == 2)
			_scopeWanted = 0; // walk right
		else
			_lastDirection = 2;
		break;
	case 3:
		if (_lastDirection == 3)
			_scopeWanted = 2; // walk backward
		else
			_lastDirection = 3;
		break;
	case 4:
		if (_lastDirection == 4)
			_scopeWanted = 3; // walk forward
		else
			_lastDirection = 4;
		break;
	}
}

void Character::moveCharOther() {

	// Height
	if (_height != 0 || _screenH != 0) {
		if (_screenH > _height)
			_screenHDelta -= 0x10000;
		else
			_screenHDelta += 0x10000;
	}

	_screenH += _screenHDelta;

	if (_screenH > 0) {
		if (_screenHDelta <= 0x40000) {
			_screenH = _screenHDelta = 0;
		} else {
			_screenH = 1;
			_screenHDelta /= -4;
		}
	}

	// X ratio
	if (_ratioX < _offset14) {
		if (_offset1c < 0)
			_offset1c += 0x20000;
		else
			_offset1c++;
	} else if (_ratioX > _offset14) {
		if (_offset1c > 0)
			_offset1c -= 0x20000;
		else
			_offset1c -= 0x10000;
	}

	if (_ratioX != _offset14) {
		int32 delta = abs(_ratioX - _offset14);

		if (delta <= 0x10000 && abs(_offset1c) <= 0x20000) {
			_offset1c = 0;
			_ratioX = _offset14;
		} else {
			_ratioX += _offset1c;
		}

		if (_ratioX < 0x500) {
			_offset1c /= -2;
			_ratioX = 0x500;
		}
	}

	// Y ratio
	if (_ratioY < _offset20) {
		if (_offset28 < 0)
			_offset28 += 0x20000;
		else
			_offset28++;
	} else if (_ratioY > _offset20) {
		if (_offset28 > 0)
			_offset28 -= 0x20000;
		else
			_offset28 -= 0x10000;
	}

	if (_ratioY != _offset20) {
		int32 delta = abs(_ratioY - _offset20);

		if (delta <= 0x10000 && abs(_offset28) <= 0x20000) {
			_offset28 = 0;
			_ratioY = _offset20;
		} else {
			_ratioY += _offset28;
		}

		if (_ratioY < 0x500) {
			_offset28 /= -2;
			_ratioY = 0x500;
		}
	}
}

void Character::stopChar() {
	_gotoLoc = _lastLocation;
	_gotoX = _screenX;
	_gotoY = _screenY;
	_start3 = _start3Prev = _start3PrevPrev = _screenX * 256;
	_start4 = _start4Prev = _start4PrevPrev = _screenY * 256;
	_lastBox = _vm->database()->whatBox(_lastLocation, _gotoX, _gotoY);
	_start5 = _start5Prev = _start5PrevPrev =
		_vm->database()->getZValue(_lastLocation, _lastBox, _start4);

	_direction = 0;
	_stopped = true;
	_priority = _vm->database()->getPriority(_lastLocation, _lastBox);
	_vm->database()->setCharPos(_id, _lastLocation, _lastBox);
}

//* Make sure no room has 5 characters */
void Character::housingProblem() {
	bool mustMove = false;

	for (int attempt = 0; attempt < 5; ++attempt) {
		int count = 0;

		for (int i = 0; i < _vm->database()->charactersNum(); ++i) {
			Character *chr2 = _vm->database()->getChar(i);

			if (_lastLocation == chr2->_lastLocation) {
				count++;
			}
		}

		if (count >= 5)
			mustMove = true;

		// If we already teleported, and there are still 5 characters,
		// stop trying if the player is not in the same room
		if (count < 5 && (!mustMove || _lastLocation != _vm->database()->getChar(0)->_lastLocation))
			return;

		// Cast teleport spell, and re-check
		_vm->game()->doCommand(3, 1, 65, 2, _id);
	}
}

void Character::hitExit(bool checkHousing) {
	int exitLoc, exitBox;

	_vm->database()->getExitInfo(_lastLocation, _lastBox, &exitLoc, &exitBox);

	// Check pointer since spells also have charId 0
	if (this == _vm->database()->getChar(0)) {
		_vm->game()->enterLocation(exitLoc);
	} else if (checkHousing) {
		housingProblem();
	}

	_runawayLocation = _lastLocation;
	_gotoBox = -1;
	_lastLocation = exitLoc;
	_lastBox = exitBox;

	for (int i = 0; i < 6; ++i) {
		int8 linkBox = _vm->database()->getBoxLink(exitLoc, exitBox, i);

		if (linkBox == -1 || (_vm->database()->getBox(exitLoc, linkBox)->attrib & 0xe) != 0)
			continue;

		_screenX = _vm->database()->getMidX(exitLoc, linkBox);
		_screenY = _vm->database()->getMidY(exitLoc, linkBox);

		if (_spriteCutState == 0) {
			_gotoX = _screenX;
			_gotoY = _screenY;
			_gotoLoc = exitLoc;
		} else if (_lastLocation == _gotoLoc) {
			_gotoX = _vm->database()->getMidX(_lastLocation, _spriteBox);
			_gotoY = _vm->database()->getMidY(_lastLocation, _spriteBox);
		}

		_start3 = _screenX * 256;
		_start4 = _screenY * 256;
		_start3PrevPrev = _start3Prev;
		_start3Prev = _start3;
		_start4PrevPrev = _start4Prev;
		_start4Prev = _start4;
		_start5 = 65280;
		_start5Prev = 65536;
		_start5PrevPrev = 66048;
		_lastDirection = 4;
	}
}

void Character::setScope(int16 scope) {
	int16 newScope = scope;

	bool tick = (_vm->gameLoopTimer() % 3 == 1);

	// Rotate char under Float spell
	if (_spellMode == 4) {
		switch (_scopeInUse - 4) {
		case 0:
			if (tick) newScope = 5;
			break;
		case 1:
			if (tick) newScope = 6;
			break;
		case 2:
			if (tick) newScope = 7;
			break;
		case 3:
			if (tick) newScope = 8;
			break;
		case 4:
			if (tick) newScope = 9;
			break;
		case 5:
			if (tick) newScope = 10;
			break;
		case 6:
			if (tick) newScope = 11;
			break;
		case 7:
			if (tick) newScope = 4;
			break;
		default:
			newScope = 8;
		}
	}

	setScopeX(newScope);
	if (_actorId == -1)
		return;

	if (_spellMode == 6) { // Invisibility
		_vm->actorMan()->get(_actorId)->setEffect(3);
	} else if (_spellMode < 5 || 6 < _spellMode) {
		_vm->actorMan()->get(_actorId)->setEffect(0);
	}

	// Flicker Shield spell
	if (this == _vm->database()->getChar(0)) {
		Player *player = _vm->game()->player();
		int mask;
		if (player->colgateTimer > 5)
			mask = 2;
		else
			mask = 1;
		if ((player->colgateTimer & mask) == 0) {
			_vm->actorMan()->get(_actorId)->setEffect(0);
		} else {
			_vm->actorMan()->get(_actorId)->setEffect(2);
		}
		if (player->colgateTimer > 0)
			player->colgateTimer--;
	}
}

void Character::setScopeX(int16 scope) {
	static Path spritesDir("kom/cutsprit/");
	static Path actorsDir("kom/actors/");
	Path filename;
	String charName(_name);
	charName.toLowercase();
	Actor *act;

	if (_spriteTimer > 0)
		scope = _spriteScope;

	if (_loadedScopeXtend != -1 && _scopeInUse == scope)
		return;

	// If cabbage spell is on, don't change scope
	if (_spellMode == 1 && scope != 102) {
		scope = 102;
		if (_loadedScopeXtend != -1 || _scopeInUse == 102)
			return;
	}

	// grave
	if (scope == 100) {
		if (_loadedScopeXtend != -1) {
			_vm->actorMan()->unload(_actorId);
			_actorId = -1;
		}
		_actorId = _vm->actorMan()->load(actorsDir.appendComponent("kom/actors/grave.act"));
		act = _vm->actorMan()->get(_actorId);
		act->enable(1);
		act->defineScope(0, 0, 0, 0);
		act->setScope(0, 1);
		_scopeInUse = scope;
		_loadedScopeXtend = -9999;

	// sprite cutscene
	} else if (scope == 101) {
		String name(_spriteName);
		name.toLowercase();

		switch (_spriteType) {
		case 0:
			if (_loadedScopeXtend != -1 && _actorId != -1) {
				_vm->actorMan()->unload(_actorId);
				_actorId = -1;
			}

			// If char is thidney
			if (_vm->game()->player()->selectedChar == 0) {
				if (name.lastChar() == 's')
					name.setChar('t', name.size() - 1);
				// SHAR -> SMAN
				if (name.hasPrefix("shar")) {
					name.setChar('m', 1);
					name.setChar('n', 3);
				}
			}

			// Figure out if the prefix directory is 3 or 4 characters long
			// and if the filename has a day/night variant
			char prefix[5];
			char actorFilename[13];
			strncpy(prefix, name.c_str(), 4);
			prefix[4] = '\0';

			Common::sprintf_s(actorFilename, sizeof(actorFilename), "%s%d.act",
					name.c_str(), _vm->game()->player()->isNight);
			filename = spritesDir.appendComponent(prefix).appendComponent(actorFilename);

			if (!Common::File::exists(filename)) {
				actorFilename[strlen(actorFilename) - 5] = '0' + 1 - _vm->game()->player()->isNight;
				filename = filename.getParent().appendComponent(actorFilename);

				if (!Common::File::exists(filename)) {
					prefix[3] = '\0';
					filename = spritesDir.appendComponent(prefix).appendComponent(actorFilename);

					if (!Common::File::exists(filename)) {
						actorFilename[strlen(actorFilename) - 5] = '0' + 1 - _vm->game()->player()->isNight;
						filename = filename.getParent().appendComponent(actorFilename);
					}
				}
			}

			_vm->panel()->showLoading(true);

			_actorId = _vm->actorMan()->load(filename);

			if (_lastLocation == _vm->database()->getChar(0)->_lastLocation) {
				// TODO: stop greeting

				// play sample
				if (_vm->game()->player()->spriteSample.
						loadFile(spritesDir.appendComponent(prefix).appendComponent(name + '0' + ".raw")))
					_vm->sound()->playSampleSFX(_vm->game()->player()->spriteSample, false);
			}

			_vm->panel()->showLoading(false);

			act = _vm->actorMan()->get(_actorId);
			act->enable(1);
			act->defineScope(0, 0, act->getFramesNum() - 1, 0);
			act->setScope(0, 2);

			_spriteTimer = act->getFramesNum() * 2 - 1;
			_spriteScope = scope;
			_scopeInUse = scope;
			_loadedScopeXtend = -8888;

			break;
		case 1:
			setAnimation(13, scope);
			break;
		case 2:
			setAnimation(14, scope);
			break;
		case 3:
			setAnimation(15, scope);
			break;
		case 4:
			setAnimation(16, scope);
			break;
		default:
			error("Illegal sprite type");
		}
	} else if (scope == 102) {
		if (_loadedScopeXtend != -1 && _actorId >= 0) {
			_vm->actorMan()->unload(_actorId);
			_actorId = -1;
		}
		_vm->panel()->showLoading(true);
			_actorId = _vm->actorMan()->load(actorsDir.appendComponent("cabbage.act"));
			act = _vm->actorMan()->get(_actorId);
			act->enable(1);
			act->defineScope(0, 0, act->getFramesNum() - 1, 0);
			act->setScope(0, 3);
			_spriteScope = scope;
			_scopeInUse = scope;
			_loadedScopeXtend = -7777;
		_vm->panel()->showLoading(false);

	// regular scope
	} else if (scope < 100) {

		assert(_scopes[scope].startFrame != -1);

		char xtend = _xtend;

		if (_walkSpeed == 0) {
			xtend += _vm->game()->player()->isNight;

		// Idle animations are stored in a separate actor file for some reason
		} else if (scope == 12)
			xtend += 1;

		if (_loadedScopeXtend != xtend) {

			if (_loadedScopeXtend != -1 && _actorId != -1) {
				_vm->actorMan()->unload(_actorId);
				_actorId = -1;
			}

			char actorFilename[13];
			Common::sprintf_s(actorFilename, sizeof(actorFilename), "%s%c.act", charName.c_str(),
					xtend + (xtend < 10 ? '0' : '7'));
			_loadedScopeXtend = xtend;

			_vm->panel()->showLoading(true);
			_actorId = _vm->actorMan()->load(actorsDir.appendComponent(actorFilename));
			_vm->actorMan()->get(_actorId)->enable(1);
			_vm->panel()->showLoading(false);
		}

		if (_scopeInUse == scope)
			return;

		_scopeInUse = scope;

		act = _vm->actorMan()->get(_actorId);
		act->defineScope(0, _scopes[scope].minFrame, _scopes[scope].maxFrame,
				_scopes[scope].startFrame);
		act->setScope(0, _animSpeed);

		// Handle idle animation
		if (scope == 12) {
			_spriteCutState = 3;
			_spriteTimer = (_scopes[scope].maxFrame -
					_scopes[scope].minFrame) * _animSpeed - 1;
			_spriteScope = scope;
			_scopeInUse = scope;
			_isBusy = true;
		}
	}
}

void Character::setAnimation(int16 anim, int16 scope) {
	Actor *act = _vm->actorMan()->get(_actorId);
	act->defineScope(0, _scopes[anim].minFrame, _scopes[anim].maxFrame,
		_scopes[anim].startFrame);
	act->setScope(0, _animSpeed);
	_spriteTimer = (_scopes[anim].maxFrame - _scopes[anim].minFrame) * _animSpeed - 1;
	_spriteScope = _scopeInUse = scope;
}

void Character::unsetSpell() {
	if (_spellMode == 1 || _spellMode == 2 || _spellMode == 3 ||
		_spellMode == 4 || _spellMode == 6) {

		_strength = _oldStrength;
		_defense = _oldDefense;

		if (_spellMode == 3 && _isMortal)
			_hitPoints *= 2;

		if (_spellMode == 1)
			_hitPoints = _oldHitPoints;
	}

	if (_spellMode != 0) {
		_vm->game()->doActionUnsetSpell(_id, _spellMode);
	}

	_mode = _modeCount = _spellMode = _spellDuration = 0;

	_vm->game()->doActionSetSpell(_id, 0);
}

KomEngine *Character::_vm = 0;

} // End of namespace Kom
