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
#include <string.h>
#include "common/file.h"
#include "common/path.h"
#include "common/array.h"
#include "common/memstream.h"
#include "common/textconsole.h"

#include "kom/kom.h"
#include "kom/actor.h"
#include "kom/character.h"
#include "kom/screen.h"
#include "kom/database.h"

using Common::File;
using Common::Path;
using Common::MemoryReadStream;

namespace Kom {

ActorManager::ActorManager(KomEngine *vm) : _vm(vm) {
	_actors.resize(6);
	for (uint i = 0; i < _actors.size(); ++i)
		_actors[i] = NULL;
}

ActorManager::~ActorManager() {
	unloadAll();
	delete _mouseActor;
	delete _donutActor;
	delete _objectsActor;
	delete _charIconActor;
	delete _coinageActor;
	delete _fightBarLActor;
	delete _fightBarRActor;
}

int ActorManager::load(const Path &filename) {

	Actor *act = new Actor(_vm, filename);

	// find an empty place in the array. if none exist, create a new cell
	for (uint i = 0; i < _actors.size(); ++i)
		if (_actors[i] == NULL) {
			_actors[i] = act;
			return i;
		}

	_actors.push_back(act);
	return _actors.size() - 1;
}

void ActorManager::loadExtras() {

	// TODO: this should be changed to allow different mouse actors

	_mouseActor = new Actor(_vm, "kom/oneoffs/m_icons.act", true);
	_mouseActor->defineScope(0, 0, 3, 0);
	_mouseActor->defineScope(1, 4, 4, 4);
	_mouseActor->defineScopeAlias(2, Actor::_exitCursorAnimation, 16);
	_mouseActor->defineScope(3, 15, 20, 15);
	_mouseActor->defineScope(4, 14, 14, 14);
	_mouseActor->defineScopeAlias(5, Actor::_inventoryCursorAnimation, 23);
	_mouseActor->defineScope(6, 31, 31, 31);  // Loading... icon

	_mouseActor->setScope(0, 2);
	_mouseActor->setPos(0, 0);
	_mouseActor->setEffect(4);

	// Init CursorMan
	_mouseActor->display();

	_donutActor = new Actor(_vm, "kom/oneoffs/donut.act");
	_donutActor->enable(0);
	_donutActor->setEffect(4);
	_donutActor->setMaskDepth(0, 2);

	_objectsActor = new Actor(_vm, "kom/oneoffs/inv_obj.act");
	_objectsActor->enable(0);
	_objectsActor->setEffect(4);
	_objectsActor->setMaskDepth(0, 1);

	_charIconActor = new Actor(_vm, "kom/oneoffs/charicon.act");
	_charIconActor->enable(0);

	_coinageActor = new Actor(_vm, "kom/oneoffs/coins.act");
	_coinageActor->enable(0);

	_fightBarLActor = new Actor(_vm, "kom/oneoffs/healthl.act");
	_fightBarLActor->enable(0);
	_fightBarLActor->setEffect(4);

	_fightBarRActor = new Actor(_vm, "kom/oneoffs/healthr.act");
	_fightBarRActor->enable(0);
	_fightBarRActor->setEffect(4);

	Actor *act;
	_cloudActorId = load("kom/oneoffs/cloud.act");
	act = get(_cloudActorId);
	act->enable(0);
	act->defineScope(0, 0, 3, 0);
	act->setScope(0, 2);

	_cloudEffectActorId = load("kom/oneoffs/cloudic1.act");
	act = get(_cloudEffectActorId);
	act->enable(0);
	act->setMaskDepth(0, 1);
	act->defineScope(0, 0, 7, 0);
	act->defineScope(1, 8, 12, 8);

	_cloudWordActorId = load("kom/oneoffs/cloudic2.act");
	act = get(_cloudWordActorId);
	act->enable(0);
	act->defineScope(0, 0, 0, 0);
	act->defineScope(1, 1, 1, 1);
	act->defineScope(2, 2, 2, 2);
	act->defineScope(3, 3, 3, 3);

	for (int i = 0; i < 4; i++) {
		_cloudNPC[i] = load("kom/oneoffs/cloud.act");
		act = get(_cloudNPC[i]);
		act->enable(0);
		act->defineScope(0, 0, 3, 0);
		act->setScope(0, 2);
	}

	for (int i = 0; i < 10; i++) {
		Character *magicChar = _vm->database()->getMagicChar(i);
		magicChar->_actorId = load("kom/oneoffs/magic.act");
		act = get(magicChar->_actorId);
		act->enable(0);
		act->defineScope(0, 0, act->_framesNum - 1, 0);
		act->setScope(0, 1);

		_magicDarkLord[i] = load("kom/oneoffs/magic2.act");
		act = get(_magicDarkLord[i]);
		act->enable(0);
		act->defineScope(0, 0, act->_framesNum - 1, 0);
		act->setScope(0, 1);
	}
}

void ActorManager::displayAll() {
	Actor *act;

	for (uint i = 0; i < _actors.size(); ++i) {
		act = getFarthestActor();
		if (act == NULL)
			break;

		act->display();
		act->_isActive = 99; // == "displayed"
	}

	for (uint i = 0; i < _actors.size(); ++i) {
		act = _actors[i];
		if (act != NULL && act->_isActive == 99) {
			act->_isActive = 1;
		}
	}
}

void ActorManager::pauseAnimAll(bool pause) {
	Actor *act;

	// Sanity check
	static bool pauseState = false;
	if (pause == pauseState)
		return;
	pauseState = pause;

	for (uint i = 0; i < _actors.size(); ++i) {
		act = _actors[i];
		if (act != NULL) {
			if (pause) {
				act->_pausedAnim = act->_isAnimating;
				act->_isAnimating = false;
			} else
				act->_isAnimating = act->_pausedAnim;
		}
	}
}

Actor *ActorManager::getFarthestActor() {
	Common::Array<Actor *>::iterator act;
	int maxDepth = 0;
	Actor *maxActor = NULL;

	for (act = _actors.begin(); act != _actors.end(); act++) {
		if (*act != NULL && (*act)->_isActive == 1)
			if ((*act)->_depth > maxDepth) {
				maxDepth = (*act)->_depth;
				maxActor = *act;
			}
	}

	return maxActor;
}

Actor::Actor(KomEngine *vm, const Path &filename, bool isMouse) : _vm(vm) {
	File f;
	char magicName[8];

	_scope = 255;
	_isAnimating = false;
	_animDuration = 0;
	_isActive = 1;
	_currentFrame = _minFrame = _maxFrame = 0;
	_xPos = _yPos = 0;
	_xRatio = _yRatio = 1024;
	_displayLeft = _displayRight = _displayTop = _displayBottom = 1000;
	_maskDepth = 0;
	_depth = 0;
	_effect = 0;

	f.open(filename);

	f.read(magicName, 7);
	magicName[7] = '\0';
	assert(strcmp(magicName, "DCB_ACT") == 0);

	_isMouse = isMouse;
	_isPlayerControlled = !f.readByte();
	_framesNum = f.readSint16LE();

	f.seek(10);
	_framesDataSize = f.size() - f.pos();
	_framesData = new byte[_framesDataSize];
	f.read(_framesData, _framesDataSize);

	f.close();
}

Actor::~Actor() {
	delete[] _framesData;
}

void Actor::defineScope(uint8 scopeId, int16 minFrame, int16 maxFrame, int16 startFrame) {
	assert(scopeId < 8);

	_scopes[scopeId].minFrame = minFrame;
	_scopes[scopeId].maxFrame = maxFrame;
	_scopes[scopeId].startFrame = startFrame;
}

void Actor::defineScopeAlias(uint8 scopeId, const int16 *aliasData, uint8 length) {
	assert(scopeId < 8);

	_scopes[scopeId].minFrame = 0;
	_scopes[scopeId].maxFrame = length - 1;
	_scopes[scopeId].aliasData = aliasData;
	_scopes[scopeId].startFrame = 0;
}

void Actor::setScope(uint8 scopeId, uint16 animDuration) {
	assert(scopeId < 8);

	_scope = scopeId;

	// TODO: figure out why minFrame is abs()-ed in the original
	setAnim(_scopes[_scope].minFrame, _scopes[_scope].maxFrame, animDuration);
	_currentFrame = _scopes[_scope].startFrame;
}

void Actor::switchScope(uint8 scopeId, uint16 animDuration) {
	if (_scope != scopeId)
		setScope(scopeId, animDuration);
}

void Actor::setAnim(int16 minFrame, int16 maxFrame, uint16 animDuration) {
	if (minFrame <= maxFrame) {
		_minFrame = minFrame;
		_maxFrame = maxFrame;
		_reverseAnim = false;
	} else {
		_minFrame = maxFrame;
		_maxFrame = minFrame;
		_reverseAnim = true;
	}

	if (_currentFrame < 255) {
		_animDurationTimer = _animDuration = animDuration;
	}

	_isAnimating = true;
}

void Actor::animate() {
	if (_animDuration == 0 || !_isAnimating || _minFrame == _maxFrame)
		return;

	_animDurationTimer--;
	if (_animDurationTimer != 0)
		return ;

	_animDurationTimer = _animDuration;

	if (_reverseAnim) {
		_currentFrame--;
		if (_currentFrame <= _minFrame)
			_currentFrame = _maxFrame;
	} else {
		_currentFrame++;
		if (_currentFrame > _maxFrame)
			_currentFrame = _minFrame;
	}
}

void Actor::display() {
	int16 frame;
	int32 offset;
	int16 width, height;
	int32 xStart, yStart;
	uint32 scaledWidth, scaledHeight;

	animate();

	// Handle scope alias
	if (_scope != 255 && _scopes[_scope].aliasData != NULL)
		frame = _scopes[_scope].aliasData[_currentFrame];
	else
		frame = _currentFrame;

	MemoryReadStream frameStream(_framesData, _framesDataSize);

	frameStream.seek(frame * 4);
	offset = frameStream.readSint32LE() - 10;

	frameStream.seek(offset);
	width = frameStream.readSint16LE();
	height = frameStream.readSint16LE();

	if (width <= 0 || height <= 0)
		return;

	xStart = _xPos + frameStream.readSint16LE() * _xRatio / 1024;
	yStart = _yPos + frameStream.readSint16LE() * _yRatio / 1024;

	scaledWidth = width * _xRatio / 1024;
	scaledHeight = height * _yRatio / 1024;

	_displayLeft = xStart;
	_displayRight = xStart + scaledWidth - 1;
	_displayTop = yStart;
	_displayBottom = yStart + scaledHeight - 1;

	if (_isPlayerControlled) {
		error("TODO: display player-controlled actors");
	} else {
		// TODO:
		// * more drawing functions for effects
		// * more arguments for scaling
		// * displayMask

		// The loading icon is NOT a mouse cursor, but is stored in the mouse actor
		if (_isMouse && _scope != 6) {
			_vm->screen()->drawMouseFrame((int8 *)(_framesData + frameStream.pos()),
												  width, height, xStart, yStart);
		} else {
			//debug("drawing actor: %s", _name.c_str());
			switch (_effect) {
			case 4:
				_vm->screen()->drawActorFrame((int8 *)(_framesData + frameStream.pos()),
					width, height, xStart, yStart);
				break;
			case 5:
				// Used only when trying to cast Spell O' Kolagate Shield on another person
				_vm->screen()->drawActorFrame((int8 *)(_framesData + frameStream.pos()),
					width, height, xStart, yStart, /* greyed out */ true);
				break;
			case 0:
				_vm->screen()->drawActorFrameScaled((int8 *)(_framesData + frameStream.pos()),
					width, height, xStart, yStart, xStart + scaledWidth - 1, yStart + scaledHeight - 1, _maskDepth);
				break;
			case 2:
				_vm->screen()->drawActorFrameScaledAura((int8 *)(_framesData + frameStream.pos()),
					width, height, xStart, yStart, xStart + scaledWidth - 1, yStart + scaledHeight - 1, _maskDepth);
				break;
			case 3:
				_vm->screen()->drawActorFrameScaled((int8 *)(_framesData + frameStream.pos()),
					width, height, xStart, yStart, xStart + scaledWidth - 1, yStart + scaledHeight - 1, _maskDepth, /*invisible=*/true);
				break;
			default:
				warning("Unhandled effect %d", _effect);
			}
		}
	}
}

const int16 Actor::_exitCursorAnimation[] = {
	5, 6, 7, 8, 9, 10, 11, 12, 13, 12, 11, 10, 9, 8, 7, 6
};

const int16 Actor::_inventoryCursorAnimation[] = {
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 21, 21, 21
};

} // End of namespace Kom
