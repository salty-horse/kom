/* ScummVM - Scumm Interpreter
 * Copyright (C) 2004-2006 The ScummVM project
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

#include "common/array.h"
#include "common/debug.h"
#include "common/system.h"
#include "common/file.h"
#include "common/endian.h"
#include "common/list.h"
#include "common/random.h"
#include "common/rect.h"
#include "common/str.h"
#include "common/util.h"
#include "common/textconsole.h"
#include "engines/util.h"
#include "graphics/cursorman.h"
#include "graphics/surface.h"
#include "graphics/palette.h"

#include "kom/screen.h"
#include "kom/kom.h"
#include "kom/panel.h"
#include "kom/actor.h"
#include "kom/game.h"
#include "kom/character.h"
#include "kom/database.h"
#include "kom/font.h"
#include "kom/input.h"
#include "kom/sound.h"
#include "kom/video_player.h"

using Common::File;
using Common::List;
using Common::Rect;
using Common::String;

namespace Kom {

ColorSet::ColorSet(const char *filename) {
	File f;

	if (!f.open(filename))
		return;

	size = f.size() / 3;
	data = new byte[size * 3];
	f.read(data, size * 3);

	f.close();
}

ColorSet::~ColorSet() {
	delete[] data;
}

Screen::Screen(KomEngine *vm, OSystem *system)
	: _system(system), _vm(vm), _sepiaScreen(0),
	  _fullRedraw(false), _paletteChanged(false), _newBrightness(256),
	  _narratorScrollText(0), _narratorWord(0), _narratorTextSurface(0),
	  _narratorScrollStatus(0) {

	_lastFrameTime = 0;

	_screenBuf = new uint8[SCREEN_W * SCREEN_H];
	memset(_screenBuf, 0, SCREEN_W * SCREEN_H);

	_mouseBuf = new uint8[MOUSE_W * MOUSE_H];
	memset(_mouseBuf, 0, MOUSE_W * MOUSE_H);

	_c0ColorSet = new ColorSet("kom/oneoffs/c0_127.cl");
	_orangeColorSet = new ColorSet("kom/oneoffs/sepia_or.cl");
	_greenColorSet = new ColorSet("kom/oneoffs/sepia_gr.cl");

	_roomMask = 0;
	_roomBackground = 0;

	_font = new Font("kom/oneoffs/packfont.fnt");

	_dirtyRects = new List<Rect>();
	_prevDirtyRects = new List<Rect>();
}

Screen::~Screen() {
	delete[] _screenBuf;
	delete[] _mouseBuf;
	delete _c0ColorSet;
	delete _orangeColorSet;
	delete _greenColorSet;
	delete[] _sepiaScreen;
	delete _font;
	delete _dirtyRects;
	delete _prevDirtyRects;
}

bool Screen::init() {
	initGraphics(SCREEN_W, SCREEN_H, false);

	useColorSet(_c0ColorSet, 0);

	return true;
}

void Screen::processGraphics(int mode, bool samplePlaying) {
	Settings *settings = _vm->game()->settings();
	Player *player = _vm->game()->player();
	Character *playerChar = _vm->database()->getChar(0);

	// handle screen objects
	Common::Array<RoomObject> *roomObjects = _vm->game()->getObjects();

	for (uint i = 0; i < roomObjects->size(); i++) {

		// Remove disappeared objects
		if ((*roomObjects)[i].disappearTimer > 0) {
			if (--(*roomObjects)[i].disappearTimer == 0)
				(*roomObjects)[i].objectId = -1;
		}

		// Set object visibility
		if (mode >= 0 && (*roomObjects)[i].actorId >= 0) {
			Actor *act =
				_vm->actorMan()->get((*roomObjects)[i].actorId);

			if ((*roomObjects)[i].objectId >= 0) {
				Object *obj =
					_vm->database()->getObj((*roomObjects)[i].objectId);

				act->enable(obj->isVisible ? 1 : 0);
			} else
				act->enable(0);
		}
	}

	// handle dust clouds - NPCs
	for (int i = 0; i < 4; i++) {
		Actor *act = _vm->actorMan()->getNPCCloudActor(i);
		act->enable(0);
		if (settings->fightNPCCloud[i].charId == -1)
			continue;

		if (settings->fightNPCCloud[i].timer == 0)
			continue;

		// Fight started
		if (settings->fightNPCCloud[i].timer == 35) {
			_vm->sound()->playSampleSFX(_vm->_weaponSample, false);
		}

		Character *fighter = _vm->database()->getChar(settings->fightNPCCloud[i].charId);

		// Figure out fight distance from player
		int distance = 0;
		if (playerChar->_lastLocation != fighter->_lastLocation) {
			int fightLoc = fighter->_lastLocation;
			int newLoc = playerChar->_lastLocation;
			for (distance = 1; distance < 5; distance++) {
				newLoc = _vm->database()->loc2loc(newLoc, fightLoc);
				if (newLoc == -1)
					break;
			}
		}

		_vm->sound()->setSampleVolume(_vm->_weaponSample, _vm->_distanceVolumeTable[distance]);

		// Draw cloud
		if (playerChar->_lastLocation == fighter->_lastLocation) {
			int scale = fighter->_start5 * 88 / 60;
			act->enable(1);
			act->setPos(fighter->_screenX / 2, fighter->_start4 / 256 / 2);
			act->setRatio(fighter->_ratioX / scale, fighter->_ratioY / scale);
			act->setMaskDepth(fighter->_priority, fighter->_start5);
		}
		settings->fightNPCCloud[i].timer--;

		// End fight
		if (settings->fightNPCCloud[i].timer == 0) {
			act->enable(0);
			fighter->_isBusy = false;
			_vm->database()->getChar(fighter->_fightPartner)->_isBusy = false;
			_vm->database()->getChar(fighter->_fightPartner)->_fightPartner = -1;
			fighter->_fightPartner = -1;
			settings->fightNPCCloud[i].charId = -1;
		}
	}

	// unload actors in other rooms
	for (int i = 1; i < _vm->database()->charactersNum(); ++i) {
		Character *chr = _vm->database()->getChar(i);

		if (playerChar->_lastLocation != chr->_lastLocation ||
			!_vm->database()->getChar(i)->_isVisible) {

			if (chr->_loadedScopeXtend != -1) {
				chr->_spriteTimer = chr->_spriteCutState = 0;
				chr->_isBusy = false;

				if (chr->_actorId >= 0) {
					_vm->actorMan()->unload(chr->_actorId);
					chr->_actorId = chr->_loadedScopeXtend = chr->_scopeInUse = -1;
				}
			}
		}
	}

	// load actors in this room
	for (int i = 0; i < _vm->database()->charactersNum(); ++i) {
		Character *chr = _vm->database()->getChar(i);

		chr->_start5PrevPrev = chr->_start5Prev;
		chr->_start5Prev = chr->_start5;
		chr->_start4PrevPrev = chr->_start4Prev;
		chr->_start4Prev = chr->_start4;
		chr->_start3PrevPrev = chr->_start3Prev;
		chr->_start3Prev = chr->_start3;

		int scale = (chr->_start5 * 88) / 60;

		if (mode > 0) {

			if (playerChar->_lastLocation == chr->_lastLocation &&
				chr->_fightPartner == -1 && chr->_isVisible) {

				int maskDepth;

				chr->setScope(chr->_scopeWanted);
				Actor *act = _vm->actorMan()->get(chr->_actorId);

				if (scale == 256 && chr->_walkSpeed == 0) {
					act->setPos(chr->_screenX / 2,
							(chr->_start4 + (chr->_screenH + chr->_offset78)
							 / scale) / 256 / 2);
					maskDepth = 32767;
				} else if (i == 0 && chr->_spriteTimer != 0 &&
						   player->spriteCutMoving) {

					act->setPos(player->spriteCutX,
								player->spriteCutY);
					maskDepth = chr->_start5;
				} else {
					act->setPos(chr->_screenX / 2,
							(chr->_start4 + (chr->_screenH + chr->_offset78)
							 / scale) / 256 / 2);
					maskDepth = chr->_start5;
				}

				act->setMaskDepth(
						_vm->database()->getPriority(chr->_lastLocation, chr->_lastBox),
						maskDepth);
				act->setRatio(chr->_ratioX / scale, chr->_ratioY / scale);

				_vm->actorMan()->get(chr->_actorId)->enable(1);
			} else if (chr->_actorId != -1) {
				_vm->actorMan()->get(chr->_actorId)->enable(0);
			}
		}
	}

	Character *enemyChar = _vm->database()->getChar(player->enemyId);

	// handle dust clouds
	if (settings->fightTimer != 0) {
		Actor *playerActor = _vm->actorMan()->get(playerChar->_actorId);
		Actor *enemyActor = _vm->actorMan()->get(enemyChar->_actorId);
		playerActor->enable(0);
		enemyActor->enable(0);
		playerChar->stopChar();
		enemyChar->stopChar();
		enemyChar->_spriteCutState = 0;
		enemyChar->_spriteTimer = 0;
	}

	switch (settings->fightState) {
	// No fight. Check player health
	case 0:
		if (player->hitPointsOld == player->hitPoints &&
			(!playerChar->_isAlive || player->hitPoints == 0)) {
			_vm->endGame();
		}
		_vm->actorMan()->getCloudWordActor()->enable(0);
		_vm->actorMan()->getCloudActor()->enable(0);
		break;

	// Fight ended
	case 1:
		settings->fightState = 0;
		_vm->actorMan()->getCloudWordActor()->enable(0);
		_vm->actorMan()->getCloudActor()->enable(0);
		_vm->actorMan()->get(playerChar->_actorId)->enable(1);
		_vm->actorMan()->get(enemyChar->_actorId)->enable(1);
		_vm->sound()->stopSample(_vm->_weaponSample);

		// Play death sound
		if (!_vm->database()->getChar(player->collideNum)->_isAlive) {
			_vm->sound()->playSampleSFX(_vm->_ripSample, false);
		}

		// Return item to cursor
		switch (settings->objectType) {
		case OBJECT_SPELL:
			player->command = CMD_CAST_SPELL;
			break;
		case OBJECT_WEAPON:
			player->command = CMD_FIGHT;
			break;
		case OBJECT_ITEM:
			player->command = CMD_USE;
			break;
		default:
			// Do nothing
			break;
		}
		player->commandState = 0;
		player->collideType = COLLIDE_NONE;
		player->collideNum = -1;
		playerChar->_isBusy = false;
		_vm->database()->getChar(player->enemyId)->_isBusy = false;
		break;

	// Fight started
	case 2:
		if (!_vm->database()->getChar(player->collideNum)->_isAlive) {
			settings->fightState = 0;
			break;
		}
		_vm->game()->declareNewEnemy(player->collideNum);
		player->hitPointsOld = playerChar->_hitPoints;

		// Don't start a fight when the narrator is talking
		if (!samplePlaying) {
			playerChar->_isBusy = false;

			if (player->fightEnemy >= 0) {
				player->fightEnemy = -1;
				_vm->game()->doCommand(6, 2, player->collideNum, 1, player->fightWeapon);
			} else if (settings->objectNum >= 0) {
				_vm->game()->doCommand(6, 2, player->collideNum, 1, settings->objectNum);
			}
		}

		if (!_vm->game()->cb()->cloudActive || !settings->fightEnabled) {
			settings->objectType = OBJECT_NONE;
			settings->objectNum = -1;
			settings->fightState = 0;
			player->commandState = 0;
			player->collideType = COLLIDE_NONE;
			player->collideNum = -1;
			break;
		}

		playerChar->_isBusy = true;
		_vm->database()->getChar(player->enemyId)->_isBusy = true;
		settings->fightState = 3;
		settings->fightTimer = 40;
		settings->fightWordTimer = 0;
		player->commandState = 2;
		_vm->actorMan()->getCloudWordActor()->enable(1);
		_vm->actorMan()->getCloudActor()->enable(1);

		// TODO: consult weapons table. use default effect for now
		/*
		if (settings->objectNum >= 0 && weaponsArray[player->weaponSoundEffect] != settings->objectNum) {
			for (int i = 0; i < 10; i++) {
				if (weaponsArray[i] == settings->objectNum) {
					player->weaponSoundEffect = i;
					_vm->panel()->showLoading(true);
					_vm->loadWeaponSample(i);
					_vm->panel()->showLoading(false);
					break;
				}
			}
		}
		*/

		_vm->sound()->playSampleSFX(_vm->_weaponSample, false); // FIXME: actually mode "2"
		player->fightBarTimer = 110;
		player->enemyFightBarTimer = 110;

		// Set up cloud actors
		{
			int scale = playerChar->_start5 * 88 / 60;
			_vm->actorMan()->getCloudActor()->setPos(playerChar->_screenX / 2, playerChar->_start4 / 256 / 2);
			_vm->actorMan()->getCloudActor()->setRatio(256 * 1024 / scale, 256 * 1024 / scale);
			_vm->actorMan()->getCloudActor()->setMaskDepth(playerChar->_priority, playerChar->_start5 - 10);

			_vm->actorMan()->getCloudWordActor()->setRatio(256 * 1024 / scale, 256 * 1024 / scale);
			_vm->actorMan()->getCloudWordActor()->setMaskDepth(playerChar->_priority, playerChar->_start5 - 11);
		}

		// FALL-THROUGH

	// Fight in progress
	case 3: {
		int scale = playerChar->_start5 * 88 / 60;
		settings->fightTimer--;
		if (settings->fightTimer == 0) {
			settings->fightState = 1;
			settings->fightWordTimer = 0;
		}

		// Set up cloud word
		if (settings->fightWordTimer == 0) {

			settings->fightWordTimer = 3;

			// "Hard" hits stay longer on the screen
			if (player->hitPointsOld - player->hitPoints < 10)
				settings->fightWordTimer++;
			if (player->hitPointsOld - player->hitPoints < 5)
				settings->fightWordTimer++;

			settings->fightWordX = playerChar->_screenX * scale + (_vm->rnd()->getRandomNumber(200) - 100) * 256;
			settings->fightWordY = playerChar->_screenY * scale - _vm->rnd()->getRandomNumberRng(50, 150) * 256;
			settings->fightWordStepX = _vm->rnd()->getRandomNumberRng(256 * 8, 1750 * 8) * 2;
			settings->fightWordStepY = 256 * 8 - _vm->rnd()->getRandomNumberRng(256 * 4, 256 * 64);
			if (playerChar->_screenX * scale > settings->fightWordX) {
				settings->fightWordStepX *= -1;
			}
			if (++settings->fightWordScope >= 4)
				settings->fightWordScope = 0;
			_vm->actorMan()->getCloudWordActor()->setScope(settings->fightWordScope, 2);
		}

		if (settings->fightWordTimer != 0) {
			settings->fightWordTimer--;
			_vm->actorMan()->getCloudWordActor()->setPos(settings->fightWordX / scale / 2, settings->fightWordY / scale / 2);
			settings->fightWordX += settings->fightWordStepX;
			settings->fightWordY += settings->fightWordStepY;
			settings->fightWordStepX /= 16;
			settings->fightWordStepY /= 16;
		}

		if (settings->fightEffectPause != 0) {
			--settings->fightEffectPause;

			// Init effect actor
			if (settings->fightEffectPause == 0) {
				settings->fightEffectTimer = 120;
				if (++settings->fightEffectScope >= 2)
					settings->fightEffectScope = 0;

				_vm->actorMan()->getCloudEffectActor()->setScope(settings->fightEffectScope, 2);
				settings->fightEffectChar._screenX = playerChar->_screenX;
				settings->fightEffectChar._screenY = playerChar->_screenY;
				settings->fightEffectChar._start3 = playerChar->_start3;
				settings->fightEffectChar._start4 = playerChar->_start4;
				settings->fightEffectChar._start5 = playerChar->_start5;
				settings->fightEffectChar._lastLocation = playerChar->_lastLocation;
				settings->fightEffectChar._lastBox = playerChar->_lastBox;
				settings->fightEffectChar._height = 0;
				settings->fightEffectChar._screenH = -0x280000;
				settings->fightEffectChar._screenHDelta = -0xF0000;
				settings->fightEffectChar._walkSpeed = 1536;
				settings->fightEffectChar._relativeSpeed = 1024;
				settings->fightEffectChar._gotoLoc = playerChar->_lastLocation;

				uint16 xPos;
				if (_vm->rnd()->getRandomNumber(100) > 50)
					xPos = 639;
				else
					xPos = 0;

				uint16 yPos = _vm->rnd()->getRandomNumber(40) + settings->fightEffectChar._screenY - 20;

				int16 dummy;
				_vm->database()->getClosestBox(playerChar->_lastLocation, xPos, yPos,
					settings->fightEffectChar._screenX, settings->fightEffectChar._screenY,
					&dummy, &settings->fightEffectChar._gotoX, &settings->fightEffectChar._gotoY);
			}
		}
		break;
	}
	default:
		break;
	}

	// Fight effects stay around after the cloud is gone
	if (settings->fightEffectTimer != 0) {
		settings->fightEffectTimer--;

		settings->fightEffectChar.moveCharOther();
		int scale = settings->fightEffectChar._start5 * 88 / 60;
		if (playerChar->_lastLocation == settings->fightEffectChar._lastLocation &&
			settings->fightEffectTimer != 0) {
			_vm->actorMan()->getCloudEffectActor()->enable(1);
		} else {
			settings->fightEffectTimer = 0;
			_vm->actorMan()->getCloudEffectActor()->enable(0);
		}

		_vm->actorMan()->getCloudEffectActor()->setPos(settings->fightEffectChar._screenX / 2,
				                                       (settings->fightEffectChar._screenH / scale + settings->fightEffectChar._start4) / 256 / 2);
		_vm->actorMan()->getCloudEffectActor()->setRatio(256 * 1024 / scale, 256 * 1024 / scale);
		_vm->actorMan()->getCloudEffectActor()->setMaskDepth(settings->fightEffectChar._priority, settings->fightEffectChar._start5 - 11);


		// FIXME: Bug in original? This is never true (thus fails to reset the effect)
		if (settings->fightEffectChar._screenH == 0 && !settings->fightEffectChar._stopped) {
			settings->fightEffectTimer = 0;
			settings->fightEffectPause = 60;
			_vm->actorMan()->getCloudEffectActor()->enable(0);
		}
	}

	if (mode > 0) {
		// Handle magic characters
		for (int i = 0; i < 10; i++) {
			Character *magicChar = _vm->database()->getMagicChar(i);

			if (magicChar->_id == -1)
				break;

			// Disable actors in other rooms
			if (magicChar->_lastLocation != playerChar->_lastLocation) {
				_vm->actorMan()->get(magicChar->_actorId)->enable(0);
				_vm->actorMan()->getMagicDarkLord(i)->enable(0);
				continue;
			}

			int scale;
			if (_vm->database()->getBox(magicChar->_lastLocation, magicChar->_lastBox)->attrib == 8) {
				scale = magicChar->_start5Prev * 88 / 60;
			} else {
				magicChar->_start5PrevPrev = magicChar->_start5Prev;
				magicChar->_start5Prev = magicChar->_start5;
				scale = magicChar->_start5Prev * 88 / 60;
			}

			int scale2;
			if (magicChar->_id == 9999)
				scale2 = 2048;
			else
				scale2 = 1024;

			Spell *spell = _vm->game()->getSpell(i);

			// Resize when spell dies out
			if (spell->duration <= 8) {
				// TODO: modify scale2
				//scale2 = arrayLookup[spell->duration] * scale2;
			}

			Actor *act;
			if (magicChar->_id == 9999)
				act = _vm->actorMan()->getMagicDarkLord(i);
			else
				act = _vm->actorMan()->get(magicChar->_actorId);

			act->enable(1);
			act->setPos(magicChar->_screenX / 2, (magicChar->_start4 + magicChar->_screenH / scale) / 256 / 2);
			act->setRatio(scale2 * 256 / scale, scale2 * 256 / scale);
			act->setMaskDepth(magicChar->_priority, magicChar->_start5);
		}
	}

	if (mode > 0) {
		pauseBackground(false);
		updateBackground();
		drawBackground();

		displayDoors();
		_vm->actorMan()->displayAll();
	} else {
		pauseBackground(true);
	}

	// TODO - handle snow
	drawFightBars();

	// Handle mouse
	if (player->hitPoints > 0 &&
		!player->narratorTalking &&
		playerChar->_spriteCutState == 0 &&
		(player->commandState == 0 ||
		 player->command == CMD_WALK ||
		 player->command == CMD_NOTHING) &&
		player->spriteCutNum == 0) {

		if (settings->mouseMode > 0)
			settings->mouseMode--;

	} else
		settings->mouseMode = 3;

	// TODO - Check menu request
	if (settings->mouseMode == 0) {
		updateCursor();
		showMouseCursor(true);
	} else {
		showMouseCursor(false);
	}

	updateActionStrings();

	_vm->panel()->showLoading(false);
	_vm->panel()->allowLoading();

	if (mode > 0 && _vm->panel()->isDirty())
		_vm->panel()->update();

	// TODO: check game loop state?

	if (mode == 1)
		gfxUpdate();
}

void Screen::copyRectListToScreen(const Common::List<Common::Rect> *list) {

	for (Common::List<Rect>::const_iterator rect = list->begin(); rect != list->end(); ++rect) {
		debug(9, "copyRectToScreen(%hu, %hu, %hu, %hu)", rect->left, rect->top,
				rect->width(), rect->height());
		_system->copyRectToScreen(_screenBuf + SCREEN_W * rect->top + rect->left,
				SCREEN_W, rect->left, rect->top, rect->width(), rect->height());
	}
}

void Screen::drawDirtyRects() {

	// Copy everything
	if (_fullRedraw) {
		_system->copyRectToScreen(_screenBuf, SCREEN_W, 0, 0, SCREEN_W, SCREEN_H);

	} else {
		// No need to save a prev, since the background reports
		// all changes

		if (_roomBackgroundFlic.isVideoLoaded()) {
			copyRectListToScreen(_roomBackgroundFlic.getDirtyRects());
			_roomBackgroundFlic.clearDirtyRects();
		}

		// Copy dirty rects to screen
		copyRectListToScreen(_prevDirtyRects);
		copyRectListToScreen(_dirtyRects);
		delete _prevDirtyRects;
		_prevDirtyRects = _dirtyRects;
		_dirtyRects = new List<Rect>();
	}

	_fullRedraw = false;
}

void Screen::gfxUpdate() {

	narratorScrollUpdate();

	// TODO: set palette brightness

	drawDirtyRects();

	// FIXME: this shouldn't be called. doesn't allow long key presses
	_vm->input()->resetInput();

	if (_paletteChanged) {
		_paletteChanged = false;
	}

	while (_system->getMillis() < _lastFrameTime + 41 /* 24 fps */) {
		_vm->input()->checkKeys();
		_system->delayMillis(10);
	}

	if (CursorMan.isVisible())
		_vm->actorMan()->getMouse()->display();

	_system->updateScreen();
	_lastFrameTime = _system->getMillis();
}

void Screen::clearScreen(bool now) {
	_dirtyRects->clear();
	_prevDirtyRects->clear();

	memset(_screenBuf, 0, SCREEN_W * SCREEN_H);
	_fullRedraw = true;

	if (now) {
		_system->copyRectToScreen(_screenBuf, SCREEN_W, 0, 0, SCREEN_W, SCREEN_H);
		_system->updateScreen();
	}
}

void Screen::clearRoom() {
	memset(_screenBuf, 0, SCREEN_W * (SCREEN_H - PANEL_H));
	_dirtyRects->push_back(Rect(0, 0, SCREEN_W, SCREEN_H - PANEL_H));
}

static byte lineBuffer[SCREEN_W];

void Screen::drawActorFrameScaled(const int8 *data, uint16 width, uint16 height, int16 xStart, int16 yStart,
                            int16 xEnd, int16 yEnd, int maskDepth, bool invisible) {

	uint16 startLine = 0;
	uint16 startCol = 0;
	int32 colSkip = 0;
	int32 rowSkip = 0;
	div_t d;

	if (xStart > xEnd)
		SWAP(xStart, xEnd);

	if (yStart > yEnd)
		SWAP(yStart, yEnd);

	int16 visibleWidth = xEnd - xStart;
	int16 scaledWidth = visibleWidth;
	int16 visibleHeight = yEnd - yStart;
	int16 scaledHeight = visibleHeight;

	//debug("drawActorFrame(%hu, %hu, %hd, %hd, %hu, %hu)", width, height, xStart, yStart, xEnd, yEnd);

	if (visibleWidth == 0 || visibleHeight == 0) return;

	div_t widthRatio = div(width, scaledWidth);
	div_t heightRatio = div(height, scaledHeight);

	if (xStart < 0) {
		// frame is entirely off-screen
		if ((visibleWidth += xStart) <= 0)
			return;

		d = div(-xStart * width, scaledWidth);
		startCol = d.quot;
		colSkip = d.rem;
		//debug("startCol: %hu, colSkip: %d", startCol, colSkip);

		xStart = 0;
	}

	// frame is entirely off-screen
	if (xStart >= SCREEN_W) return;

	// check if frame spills over the edge
	if (visibleWidth + xStart >= SCREEN_W)
		if ((visibleWidth -= visibleWidth + xStart - SCREEN_W) <= 0)
			return;

	if (yStart < 0) {
		// frame is entirely off-screen
		if ((visibleHeight += yStart) <= 0)
			return;

		d = div(-yStart * height, scaledHeight);
		startLine = d.quot;
		rowSkip = d.rem;
		//debug("startLine: %hu, rowSkip: %d", startLine, rowSkip);

		yStart = 0;
	}

	// frame is entirely off-screen
	if (yStart >= SCREEN_H) return;

	// check if frame spills over the edge
	if (visibleHeight + yStart >= SCREEN_H)
		if ((visibleHeight -= visibleHeight + yStart - SCREEN_H) <= 0)
			return;

	uint8 sourceLine = startLine;
	uint8 targetLine = yStart;
	int16 rowThing = scaledHeight - rowSkip;

	for (int i = 0; i < visibleHeight; i += 1) {
		uint16 lineOffset = READ_LE_UINT16(data + sourceLine * 2);

		uint16 targetPixel = targetLine * SCREEN_W + xStart;
		drawActorFrameLine(lineBuffer, data + lineOffset, width);

		// Copy line to screen
		uint8 sourcePixel = startCol;
		int16 colThing = scaledWidth - colSkip;

		for (int j = 0; j < visibleWidth; ++j) {
			if (lineBuffer[sourcePixel] != 0
			    && (targetLine >= ROOM_H || ((byte *)_roomMask->getPixels())[targetPixel] >= maskDepth)) {
				if (invisible)
					_screenBuf[targetPixel] = _screenBuf[targetPixel + 8];
				else
					_screenBuf[targetPixel] = lineBuffer[sourcePixel];
			}

			sourcePixel += widthRatio.quot;
			colThing -= widthRatio.rem;

			if (colThing < 0) {
				sourcePixel++;
				colThing += scaledWidth;
			}

			targetPixel++;
		}

		sourceLine += heightRatio.quot;
		rowThing -= heightRatio.rem;

		if (rowThing < 0) {
			sourceLine++;
			rowThing += scaledHeight;
		}

		targetLine++;
	}

	_dirtyRects->push_back(Rect(xStart, yStart, xStart + visibleWidth, yStart + visibleHeight));
}

void Screen::drawActorFrameScaledAura(const int8 *data, uint16 width, uint16 height, int16 xStart, int16 yStart,
                            int16 xEnd, int16 yEnd, int maskDepth) {

	uint16 startLine = 0;
	uint16 startCol = 0;
	int32 colSkip = 0;
	int32 rowSkip = 0;
	div_t d;

	memset(lineBuffer, 0, sizeof(lineBuffer));

	if (xStart > xEnd)
		SWAP(xStart, xEnd);

	if (yStart > yEnd)
		SWAP(yStart, yEnd);

	int16 visibleWidth = xEnd - xStart;
	int16 scaledWidth = visibleWidth;
	int16 visibleHeight = yEnd - yStart;
	int16 scaledHeight = visibleHeight;

	if (visibleWidth == 0 || visibleHeight == 0) return;

	div_t widthRatio = div(width, scaledWidth);
	div_t heightRatio = div(height, scaledHeight);

	if (xStart < 0) {
		// frame is entirely off-screen
		if ((visibleWidth += xStart) <= 0)
			return;

		d = div(-xStart * width, scaledWidth);
		startCol = d.quot;
		colSkip = d.rem;

		xStart = 0;
	}

	// frame is entirely off-screen
	if (xStart >= SCREEN_W) return;

	// check if frame spills over the edge
	if (visibleWidth + xStart >= SCREEN_W)
		if ((visibleWidth -= visibleWidth + xStart - SCREEN_W) <= 0)
			return;

	if (yStart < 0) {
		// frame is entirely off-screen
		if ((visibleHeight += yStart) <= 0)
			return;

		d = div(-yStart * height, scaledHeight);
		startLine = d.quot;
		rowSkip = d.rem;
		//debug("startLine: %hu, rowSkip: %d", startLine, rowSkip);

		yStart = 0;
	}

	// frame is entirely off-screen
	if (yStart >= SCREEN_H) return;

	// check if frame spills over the edge
	if (visibleHeight + yStart >= SCREEN_H)
		if ((visibleHeight -= visibleHeight + yStart - SCREEN_H) <= 0)
			return;

	uint8 sourceLine = startLine;
	uint8 targetLine = yStart;
	int16 rowThing = scaledHeight - rowSkip;

	for (int i = 0; i < visibleHeight; i += 1) {
		uint16 lineOffset = READ_LE_UINT16(data + sourceLine * 2);

		bool startOfLine = (xStart == 0);
		uint16 targetPixel = targetLine * SCREEN_W + xStart;
		drawActorFrameLine(lineBuffer, data + lineOffset, width);

		// Copy line to screen
		int skipped = 0;
		uint8 sourcePixel = startCol;
		int16 colThing = scaledWidth - colSkip;

		for (int j = 0; j < visibleWidth; ++j) {
			if (lineBuffer[sourcePixel] != 0
			    && (targetLine >= ROOM_H || ((byte *)_roomMask->getPixels())[targetPixel] >= maskDepth)) {

				_screenBuf[targetPixel] = lineBuffer[sourcePixel];

				// Draw border to the left and right
				if (!startOfLine) {
					if (lineBuffer[sourcePixel - widthRatio.quot - skipped] == 0)
						_screenBuf[targetPixel-1] = 14;
				}
				if (xStart + visibleWidth < SCREEN_W) {
					if (lineBuffer[sourcePixel + widthRatio.quot + ((colThing <= widthRatio.rem) ? 1 : 0)] == 0)
						_screenBuf[targetPixel+1] = 14;
				}
			}

			skipped = 0;
			sourcePixel += widthRatio.quot;
			colThing -= widthRatio.rem;

			if (colThing < 0) {
				sourcePixel++;
				colThing += scaledWidth;
				skipped = 1;
			}

			targetPixel++;
			startOfLine = false;
		}

		sourceLine += heightRatio.quot;
		rowThing -= heightRatio.rem;

		if (rowThing < 0) {
			sourceLine++;
			rowThing += scaledHeight;
		}

		targetLine++;
	}

	_dirtyRects->push_back(Rect(MAX(0, xStart - 1), yStart, MIN((int)SCREEN_W, xStart + visibleWidth + 1), yStart + visibleHeight));
}

void Screen::drawActorFrame(const int8 *data, uint16 width, uint16 height, int16 xStart, int16 yStart) {

	uint16 startLine = 0;
	uint16 startCol = 0;

	int16 visibleWidth = width;
	int16 visibleHeight = height;

	if (visibleWidth == 0 || visibleHeight == 0) return;

	if (xStart < 0) {
		// frame is entirely off-screen
		if ((visibleWidth += xStart) <= 0)
			return;

		startCol = -xStart;
		xStart = 0;
	}

	// frame is entirely off-screen
	if (xStart >= SCREEN_W) return;

	// check if frame spills over the edge
	if (visibleWidth + xStart >= SCREEN_W)
		if ((visibleWidth -= visibleWidth + xStart - SCREEN_W) <= 0)
			return;

	if (yStart < 0) {
		// frame is entirely off-screen
		if ((visibleHeight += yStart) <= 0)
			return;

		startLine = -yStart;
		yStart = 0;
	}

	// frame is entirely off-screen
	if (yStart >= SCREEN_H) return;

	// check if frame spills over the edge
	if (visibleHeight + yStart >= SCREEN_H)
		if ((visibleHeight -= visibleHeight + yStart - SCREEN_H) <= 0)
			return;

	uint8 sourceLine = startLine;
	uint8 targetLine = yStart;

	for (int i = 0; i < visibleHeight; i += 1) {
		uint16 lineOffset = READ_LE_UINT16(data + sourceLine * 2);

		uint16 targetPixel = targetLine * SCREEN_W + xStart;
		drawActorFrameLine(lineBuffer, data + lineOffset, width);

		// Copy line to screen
		uint8 sourcePixel = startCol;

		for (int j = 0; j < visibleWidth; ++j) {
			if (lineBuffer[sourcePixel] != 0)
				_screenBuf[targetPixel] = lineBuffer[sourcePixel];
			sourcePixel++;
			targetPixel++;
		}

		sourceLine++;
		targetLine++;
	}

	_dirtyRects->push_back(Rect(xStart, yStart, xStart + visibleWidth, yStart + visibleHeight));
}

void Screen::drawMouseFrame(const int8 *data, uint16 width, uint16 height, int16 xOffset, int16 yOffset) {

	memset(_mouseBuf, 0, MOUSE_W * MOUSE_H);

	for (int line = 0; line <= height - 1; ++line) {
		uint16 lineOffset = READ_LE_UINT16(data + line * 2);

		drawActorFrameLine(lineBuffer, data + lineOffset, width);
		for (int i = 0; i < width; ++i)
			if (lineBuffer[i] != 0)
				_mouseBuf[line * MOUSE_W + i] = lineBuffer[i];
	}

	setMouseCursor(_mouseBuf, MOUSE_W, MOUSE_H, -xOffset, -yOffset);
}

void Screen::drawActorFrameLine(byte *outBuffer, const int8 *rowData, uint16 length) {
	uint8 dataIndex = 0;
	uint8 outIndex = 0;

	while (outIndex < length) {
		// FIXME: valgrind reports invalid read of size 1
		int8 imageData = rowData[dataIndex++];
		if (imageData > 0)
			while (imageData-- > 0)
				outBuffer[outIndex++] = 0;
		else if ((imageData &= 0x7F) > 0)
			while (imageData-- > 0)
				// FIXME: valgrind reports invalid read of size 1
				outBuffer[outIndex++] = rowData[dataIndex++];
	}
}

void Screen::useColorSet(ColorSet *cs, uint start) {
	static const byte black[] = { 0, 0, 0 };

	_system->getPaletteManager()->setPalette(cs->data, start, cs->size);

	// Index 0 is always black
	_system->getPaletteManager()->setPalette(black, 0, 1);

	_paletteChanged = true;
}

void Screen::setPaletteColor(int index, const byte color[]) {
	_system->getPaletteManager()->setPalette(color, index, 1);

	_paletteChanged = true;
}

void Screen::setPaletteBrightness() {
	byte newPalette[256 * 3];

	_system->getPaletteManager()->grabPalette(newPalette, 0, 256);

	if (_currBrightness < 256) {

		for (uint i = 0; i < 256 * 3; i++) {
			newPalette[i] = newPalette[i] * _currBrightness / 256;
		}

		_system->getPaletteManager()->setPalette(newPalette, 0, 256);
		_paletteChanged = true;
		_newBrightness = 9999;
	} else {
		warning("TODO: setPaletteBrightness");
	}
}

void Screen::createSepia(bool shop) {
	_sepiaScreen = new byte[SCREEN_W * ROOM_H];
	ColorSet *cs = shop ? _greenColorSet : _orangeColorSet;

	_fullRedraw = true;
	drawDirtyRects();

	Graphics::Surface *screen = _system->lockScreen();
	assert(screen);

	_system->getPaletteManager()->grabPalette(_backupPalette, 0, 256);

	for (uint y = 0; y < ROOM_H; ++y) {
		for (uint x = 0; x < SCREEN_W; ++x) {
			byte *color = &_backupPalette[((uint8*)screen->getPixels())[y * SCREEN_W + x] * 3];

			// FIXME: this does not produce the same shade as the original
			_sepiaScreen[y * SCREEN_W + x] = ((color[0] + color[1] + color[2]) / 3 * 23 + 255 / 2) / 255 + 232;
			//_sepiaScreen[y * SCREEN_W + x] = (color[0] + color[1] + color[2]) / 12 + 232;
		}
	}

	_system->unlockScreen();

	useColorSet(cs, 224);
}

void Screen::freeSepia() {
	if (!_sepiaScreen)
		return;

	delete[] _sepiaScreen;
	_sepiaScreen = 0;
	_system->getPaletteManager()->setPalette(_backupPalette, 0, 256);

	_fullRedraw = true;
}

void Screen::copySepia() {
	if (!_sepiaScreen)
		return;

	memcpy(_screenBuf, _sepiaScreen, SCREEN_W * ROOM_H);
	_dirtyRects->push_back(Rect(0, 0, SCREEN_W, SCREEN_H));
}

void Screen::setMouseCursor(const byte *buf, uint w, uint h, int hotspotX, int hotspotY) {
	CursorMan.replaceCursor(buf, w, h, hotspotX, hotspotY, 0);
}

void Screen::showMouseCursor(bool show) {
	CursorMan.showMouse(show);
}

bool Screen::isCursorVisible() {
	return CursorMan.isVisible();
}

void Screen::updateCursor() {
	Actor *mouse = _vm->actorMan()->getMouse();
	Settings *settings = _vm->game()->settings();

	if (settings->mouseY >= INVENTORY_OFFSET) {
		mouse->switchScope(5, 2);
	} else {

		// Draw item below cursor
		if (settings->objectNum >= 0) {
			Actor *act = _vm->actorMan()->getObjects();
			act->enable(1);
			act->setEffect(4);
			act->setMaskDepth(0, 1);
			act->setPos(settings->mouseX / 2, settings->mouseY / 2);
			act->setRatio(1024, 1024);
			act->setFrame(settings->objectNum + 1);
			act->display();
			act->enable(0);
		}

		switch (settings->mouseState) {
		case 2: // exit
			mouse->switchScope(2, 2);
			break;
		case 3: // hot spot
			mouse->switchScope(3, 2);
			break;
		case 0: // walk
		default:
			mouse->switchScope(0, 2);
		}
	}

	// FIXME: if the mouse is already over a hotspot when the game starts,
	//        the sound should be playing
	if (settings->overType != settings->oldOverType ||
		settings->overNum != settings->oldOverNum) {

		if (settings->overType <= 1) {
			_vm->sound()->stopSample(_vm->_hotspotSample);
		} else if (settings->overType <= 3) {
			_vm->sound()->playSampleSFX(_vm->_hotspotSample, false);
		}
	}
}

void Screen::updateActionStrings() {
	Settings *settings = _vm->game()->settings();
	int hoverId = -1;
	CollideType hoverType;
	const char *text1, *text2, *text3, *text4, *space1, *space2, *textRIP;
	char panelText[200];

	text1 = text2 = text3 = text4 = space1 = space2 = textRIP = "";

	if (_vm->_flicLoaded != 0)
		return;

	if (_vm->game()->player()->commandState != 0 &&
		_vm->game()->player()->collideType != COLLIDE_NONE) {

		hoverType = _vm->game()->player()->collideType;
		hoverId = _vm->game()->player()->collideNum;

	} else {
		hoverType = settings->overType;

		switch (settings->overType) {
			case 2:
				hoverId = settings->collideChar;
				break;
			case 3:
				hoverId = settings->collideObj;
				break;
			default:
				hoverId = -1;
		}
	}

	// Use # with #
	if (settings->objectNum >= 0) {

		if (_vm->game()->player()->command == CMD_USE) {

			text1 = "Use";
			text2 = _vm->database()->
				getObj(settings->objectNum)->desc;
			text3 = "with";
			switch (hoverType) {
			case COLLIDE_CHAR:
				text4 = _vm->database()->getChar(hoverId)->_desc;
				break;
			case COLLIDE_OBJECT:
				text4 = _vm->database()->getObj(hoverId)->desc;
				break;
			default:
				text4 = "...";
			}
		} else if (_vm->game()->player()->command == CMD_FIGHT ||
		           _vm->game()->player()->command == CMD_CAST_SPELL) {

			if (_vm->game()->player()->command == CMD_FIGHT)
				text3 = "Fight with";
			else
				text3 = "Cast spell at";

			if (hoverType != 2)
				text4 = "...";
			else
				text4 = _vm->database()->getChar(hoverId)->_desc;
		}

	// Use #
	} else {
		text3 = "Walk to";
		if (_vm->game()->player()->commandState > 0) {
			switch (_vm->game()->player()->command) {
			case CMD_USE:
				text3 = "Use";
				break;
            case CMD_TALK_TO:
				text3 = "Talk to";
				break;
            case CMD_PICKUP:
				text3 = "Pickup";
				break;
            case CMD_LOOK_AT:
				text3 = "Look at";
				break;
            case CMD_FIGHT:
				text3 = "Fight with";
				break;
			case CMD_CAST_SPELL:
				text3 = "Cast spell at";
				break;
			case CMD_WALK:
			case CMD_NOTHING:
				break;
			}
		}

		switch (hoverType) {
		case COLLIDE_BOX:
			if (settings->mouseOverExit) {
				int exitLoc, exitBox;
				text3 = "Exit to";
				_vm->database()->getExitInfo(
						_vm->database()->getChar(0)->_lastLocation,
						settings->collideBox,
						&exitLoc, &exitBox);

				text4 = _vm->database()->getLoc(exitLoc)->desc;
			}
			break;
		case COLLIDE_CHAR:
			text4 = _vm->database()->getChar(hoverId)->_desc;
			break;
		case COLLIDE_OBJECT:
			text4 = _vm->database()->getObj(hoverId)->desc;
			break;
		default:
			text4 = "...";
		}
	}

	// Set panel strings

	sprintf(panelText, "%s %s", text1, text2);
	_vm->panel()->setActionDesc(panelText);

	if (text1[0] != 0)
		space1 = " ";
	if (text4[0] != '.')
		space2 = " ";

	if (hoverType == COLLIDE_CHAR) {
		if (!_vm->database()->getChar(hoverId)->_isAlive)
			textRIP = " (RIP)";

		sprintf(panelText, "%s%s%s%s%s", space1, text3, space2, text4, textRIP);

	} else {
		sprintf(panelText, "%s%s%s%s", space1, text3, space2, text4);
	}

	_vm->panel()->setHotspotDesc(panelText);

	// TODO - inventory strings
	if (settings->mouseY >= INVENTORY_OFFSET && settings->mouseMode == 0) {
		_vm->panel()->setActionDesc("");
		_vm->panel()->setHotspotDesc("Inventory...");
	}
}

void Screen::drawPanel(const byte *panelData) {
	memcpy(_screenBuf + SCREEN_W * ROOM_H, panelData, SCREEN_W * PANEL_H);
	_dirtyRects->push_back(Rect(0, ROOM_H, SCREEN_W, SCREEN_H));
}

void Screen::clearPanel() {
	memset(_screenBuf + SCREEN_W * (SCREEN_H - PANEL_H), 0, SCREEN_W * PANEL_H);
	_dirtyRects->push_back(Rect(0, SCREEN_H - PANEL_H, SCREEN_W, SCREEN_H));
}

void Screen::updatePanelOnScreen(bool clearScreen) {
	// Don't update the panel if text is scrolling
	// TODO: actually check if subtitles are enabled
	if (_vm->game()->isNarratorPlaying())
		return;

	if (clearScreen) {
		memset(_screenBuf, 0, SCREEN_W * (SCREEN_H - PANEL_H));
		_system->copyRectToScreen(_screenBuf, SCREEN_W, 0, 0, SCREEN_W, SCREEN_H);
	} else {
		_system->copyRectToScreen(_screenBuf + SCREEN_W * ROOM_H,
			SCREEN_W, 0, ROOM_H, SCREEN_W, PANEL_H);
	}
	_system->updateScreen();
}

void Screen::loadBackground(const String &filename) {
	_roomBackgroundFlic.close();
	_roomBackgroundFlic.loadFile(filename);
	_roomBackgroundFlic.start();

	// Redraw everything
	_dirtyRects->clear();
	_prevDirtyRects->clear();
	// No need to report the rect, since the flic player will report it
}

void Screen::updateBackground() {
	if (_roomBackgroundFlic.isVideoLoaded()) {
		if (!_roomBackgroundFlic.isPaused() && _roomBackgroundFlic.getTimeToNextFrame() == 0) {

			if (_roomBackgroundFlic.endOfVideo())
				_roomBackgroundFlic.rewind();
			_roomBackground = _roomBackgroundFlic.decodeNextFrame();

			if (_roomBackgroundFlic.hasDirtyPalette()) {
				const byte *flicPalette = _roomBackgroundFlic.getPalette();
				_system->getPaletteManager()->setPalette(flicPalette + 128 * 3, 128, 128);
				_paletteChanged = true;
			}
		}
	}
}

void Screen::drawBackground() {
	if (_roomBackgroundFlic.isVideoLoaded()) {
		for (uint16 y = 0; y < _roomBackground->h; y++)
			memcpy(_screenBuf + y * SCREEN_W, (byte *)_roomBackground->getPixels() + y * _roomBackground->pitch, _roomBackground->w);
	}
}

void Screen::drawTalkFrame(const Graphics::Surface *frame, const byte *background) {
	memcpy(_screenBuf, background, SCREEN_W * ROOM_H);
	for (int i = 0; i < SCREEN_W * ROOM_H; i++) {
		byte color = ((byte *)frame->getPixels())[i];
		if (color != 0)
			_screenBuf[i] = color;
	}
	_fullRedraw = true;
}

void Screen::loadMask(const String &filename) {
	_roomMaskFlic.loadFile(filename);
	_roomMask = 0;
	_roomMask = _roomMaskFlic.decodeNextFrame();
}

void Screen::drawInventory(Inventory *inv) {
	Common::List<int> *invList;
	Common::List<int>::iterator invId;
	int invCounter;
	int selectedIndex;
	Settings *settings = _vm->game()->settings();

	copySepia();

	inv->selectedInvObj = inv->selectedWeapObj = inv->selectedSpellObj = -1;
	inv->isSelected = false;

	// Draw lines
	drawBoxScreen(76, 13, 1, 152, 0);
	drawBoxScreen(132, 13, 1, 152, 0);

	// Look for selected item
	if (inv->selectedBox2 % 5 < 2) {
		selectedIndex = -1;
	} else {
		selectedIndex = inv->selectedBox2 / 5 * 3 + (inv->selectedBox2 % 5) - 3;
		if (selectedIndex == -1)
			selectedIndex = inv->selectedInvObj = 9999;
	}

	inv->iconX = 160;
	inv->iconY = inv->scrollY;

	if (selectedIndex == 9999) {
		printIcon(inv, 0, 3);
	} else {
		printIcon(inv, 0, 2);
	}

	inv->iconX += 56;

	// Inventory objects
	if (inv->mode & 1) {
		invList = &_vm->database()->getChar(inv->shopChar)->_inventory;
		for (invId = invList->begin(), invCounter = 0;
			 invId != invList->end(); ++invId, ++invCounter) {

			if (invCounter == selectedIndex) {
				inv->isSelected = true;
				inv->selectedInvObj = *invId;
				printIcon(inv, *invId, 1);
			} else {
				printIcon(inv, *invId, 0);
			}

			inv->iconX += 56;

			// Move to the next line
			if (inv->iconX > 272) {
				inv->iconX = 160;
				inv->iconY += 40;
			}
		}
	}

	// Look for selected item
	if (inv->selectedBox2 % 5 != 1) {
		selectedIndex = -1;
	} else {
		selectedIndex = (inv->selectedBox2 - 1) / 5;
	}

	inv->iconX = 104;
	inv->iconY = inv->scrollY;

	// Inventory weapons
	if (inv->mode & 2) {
		invList = &_vm->database()->getChar(inv->shopChar)->_weapons;
		for (invId = invList->begin(), invCounter = 0;
			 invId != invList->end(); ++invId, ++invCounter) {

			if (invCounter == selectedIndex) {
				inv->isSelected = true;
				inv->selectedWeapObj = *invId;
				printIcon(inv, *invId, 1);
			} else {
				printIcon(inv, *invId, 0);
			}

			// Move to the next line
			inv->iconY += 40;
		}
	}

	// Look for selected item
	if (inv->selectedBox2 % 5 != 0) {
		selectedIndex = -1;
	} else {
		selectedIndex = inv->selectedBox2 / 5;
	}

	inv->iconX = 48;
	inv->iconY = inv->scrollY;

	// Inventory spells
	if (inv->mode & 4) {
		invList = &_vm->database()->getChar(inv->shopChar)->_spells;
		for (invId = invList->begin(), invCounter = 0;
			 invId != invList->end(); ++invId, ++invCounter) {

			if (invCounter == selectedIndex) {
				inv->isSelected = true;
				inv->selectedSpellObj = *invId;
				printIcon(inv, *invId, 1);
			} else {
				printIcon(inv, *invId, 0);
			}

			// Move to the next line
			inv->iconY += 40;
		}
	}

	// Draw item below cursor
	if (settings->objectNum >= 0) {
		Actor *act = _vm->actorMan()->getObjects();
		act->enable(1);
		act->setEffect(4);
		act->setMaskDepth(0, 1);
		act->setPos(inv->mouseX, inv->mouseY);
		act->setRatio(1024, 1024);
		act->setFrame(settings->objectNum + 1);
		act->display();
		act->enable(0);
	}

	// Draw headlines
	if (inv->shopChar) {
		writeText(_screenBuf, "Choose an Item to Purchase...", 3, 29, 0, false);
		writeText(_screenBuf, "Choose an Item to Purchase...", 2, 28, 31, false);
	} else {
		if (inv->mode & 4) {
			writeText(_screenBuf, "Spells", 3, 29, 0, false);
			writeText(_screenBuf, "Spells", 2, 28, 31, false);
		}
		if (inv->mode & 2) {
			writeText(_screenBuf, "Weapons", 3, 81, 0, false);
			writeText(_screenBuf, "Weapons", 2, 80, 31, false);
		}
		if (inv->mode & 1) {
			writeText(_screenBuf, "Inventory", 3, 191, 0, false);
			writeText(_screenBuf, "Inventory", 2, 190, 31, false);
		}
	}

	inv->selectedObj = inv->selectedInvObj & inv->selectedWeapObj & inv->selectedSpellObj;

	// Don't select "held" item
	if (inv->selectedObj == settings->objectNum) {
		inv->selectedObj = inv->selectedInvObj = inv->selectedWeapObj = inv->selectedSpellObj = -1;
		inv->isSelected = false;
	}

	if (0 <= inv->selectedObj && inv->selectedObj < 9999) {
		bool useImmediate = _vm->database()->getObj(inv->selectedObj)->isUseImmediate;

		if (useImmediate && (inv->mode & 1) == 0) {
			inv->selectedObj = inv->selectedInvObj = inv->selectedWeapObj = inv->selectedSpellObj = -1;
			inv->isSelected = false;
		} else {
			switch (inv->action) {
			case 0:
				if (inv->shopChar) {
					inv->blinkLight = 4;
					int price = _vm->database()->getObj(inv->selectedObj)->price;
					String text = String::format("Purchase for %d Gold piece%s,", price, price == 1 ? "" : "s");
					_vm->panel()->setActionDesc(text.c_str());
				} else {
					if (!useImmediate) {
						inv->blinkLight = 4;
					} else {
						// Blink "Use" text
						if ((++inv->blinkLight & 4) == 0)
							_vm->panel()->setActionDesc("Use");
					}
				}
				break;
			case 1:
				inv->blinkLight = 4;
				_vm->panel()->setActionDesc("Look at");
				break;
			case 2:
				inv->blinkLight = 4;
				break;
			}
		}
	}

	if (inv->shopChar) {
		int gold = _vm->database()->getChar(0)->_gold;
		String text = String::format("You have %d Gold piece%s", gold, gold == 1 ? "" : "s");
		_vm->panel()->setLocationDesc(text.c_str());
	}

	if (inv->mouseY * 2 > 344) {
		_vm->panel()->setHotspotDesc(inv->shopChar ? "No sale" : "Exit");
		_vm->panel()->update();
		return;
	}

	const char *descString;
	char panelText[200];

	if (inv->selectedObj < 0 || inv->selectedObj >= 9999) {
		descString = "";
	} else {
		descString = _vm->database()->getObj(inv->selectedObj)->desc;
	}

	if (settings->objectNum > 0) {
		sprintf(panelText, "Use %s", _vm->database()->getObj(settings->objectNum)->desc);
		_vm->panel()->setActionDesc(panelText);

		if (inv->selectedSpellObj >= 0) {
			sprintf(panelText, "with %s (%dpts)", descString,
					_vm->database()->getObj(inv->selectedObj)->spellCost);
		} else {
			sprintf(panelText, "with %s", descString);
		}
		_vm->panel()->setHotspotDesc(panelText);
	} else {
		if (inv->selectedSpellObj >= 0) {
			sprintf(panelText, "%s (%dpts)", descString,
					_vm->database()->getObj(inv->selectedObj)->spellCost);
			_vm->panel()->setHotspotDesc(panelText);
		} else {
			_vm->panel()->setHotspotDesc(descString);
		}
	}

	_vm->panel()->update();
}

void Screen::drawFightBars() {
	char buf[32];
	Player *player = _vm->game()->player();
	Character *playerChar = _vm->database()->getChar(0);
	player->hitPoints = playerChar->_hitPoints;

	if (player->hitPointsOld < player->hitPoints) {
		player->hitPointsOld = player->hitPoints;

	// Tick
	} else if (player->hitPointsOld > player->hitPoints) {
		player->hitPointsOld--;

		if (player->fightBarTimer == 0)
			player->fightBarTimer = 110;
	}

	if (player->enemyId != -1) {
		Character *enemyChar = _vm->database()->getChar(player->enemyId);

		if (playerChar->_lastLocation != enemyChar->_lastLocation) {
			if (_vm->game()->settings()->fightState == 0) {
				enemyChar->_isBusy = false;
				player->enemyId = -1;
			}
		}

		player->enemyHitPoints = enemyChar->_hitPoints;
		_vm->game()->declareNewEnemy(player->enemyId);
		if (player->enemyHitPointsOld != player->enemyHitPoints) {
			if (player->enemyHitPointsOld >= player->enemyHitPoints)
				player->enemyHitPointsOld--;
			else
				player->enemyHitPointsOld++;

			if (player->enemyFightBarTimer == 0) {
				player->fightBarTimer = player->enemyFightBarTimer = 110;
			}
		}
	}

	// Draw player fight bar
	if (player->fightBarTimer) {
		player->fightBarTimer--;

		Actor *act = _vm->actorMan()->getFightBarL();
		act->enable(1);
		act->setPos(0, 168);
		act->setFrame(0);
		act->display();

		// Draw bar
		int drawnHitPoints = MIN(player->hitPointsOld, (int16)256);
		for (int i = 0; i < drawnHitPoints / 2; i++) {
			act->setFrame(i + 129);
			act->display();
		}
		for (int i = drawnHitPoints / 2; i < 128; i++) {
			act->setFrame(i + 1);
			act->display();
		}

		act->enable(0);

		// Write title
		const char *desc = _vm->database()->getChar(0)->_desc;
		writeText(_vm->screen()->screenBuf(), desc, 18, 42, 0, false);
		writeText(_vm->screen()->screenBuf(), desc, 17, 41, 31, false);

		// Write hit points
		sprintf(buf, "%d", drawnHitPoints);
		writeText(_vm->screen()->screenBuf(), buf, 9, 42, 0, false);
		writeText(_vm->screen()->screenBuf(), buf, 8, 41, 31, false);
	}

	if (player->enemyId == -1 || playerChar->_lastLocation != _vm->database()->getChar(player->enemyId)->_lastLocation)
		player->enemyFightBarTimer = 0;

	// Draw enemy fight bar
	if (player->enemyFightBarTimer) {
		player->enemyFightBarTimer--;

		Actor *act = _vm->actorMan()->getFightBarR();
		act->enable(1);
		act->setPos(0, 168);
		act->setFrame(0);
		act->display();

		// Draw bar
		int drawnHitPoints = MIN(player->enemyHitPointsOld, (int16)256);

		// Draw a full bar for immortal enemies
		bool immortal = false;
		if (drawnHitPoints < 0) {
			drawnHitPoints = 256;
			immortal = true;
		}

		for (int i = 0; i < drawnHitPoints / 2; i++) {
			act->setFrame(i + 129);
			act->display();
		}
		for (int i = drawnHitPoints / 2; i < 128; i++) {
			act->setFrame(i + 1);
			act->display();
		}

		act->enable(0);

		// Write title
		const char *desc = player->enemyDesc;
		writeTextRight(_vm->screen()->screenBuf(), desc, 18, 286, 0, false);
		writeTextRight(_vm->screen()->screenBuf(), desc, 17, 285, 31, false);

		// Write hit points
		if (!immortal)
			sprintf(buf, "%d", drawnHitPoints);
		else
			strcpy(buf, "Immortal");
		writeTextRight(_vm->screen()->screenBuf(), buf, 9, 286, 0, false);
		writeTextRight(_vm->screen()->screenBuf(), buf, 8, 285, 31, false);

		if (player->enemyFightBarTimer == 0) {
			if (_vm->game()->settings()->fightState == 0) {
				_vm->database()->getChar(player->enemyId)->_isBusy = false;
				player->enemyId = -1;
			}
		}
	}
}

void Screen::printIcon(Inventory *inv, int objNum, int mode) {
	Actor *act;

	if (mode >= 2)
		act = _vm->actorMan()->getCharIcon();
	else
		act = _vm->actorMan()->getObjects();

	// Grey-out self-use spells - only relevant for Spell O' Kolagate Shield
	if ((inv->mode & 1) == 0 && objNum > 0 && _vm->database()->getObj(objNum)->isUseImmediate) {
		act->setEffect(5);
		act->setMaskDepth(0, 2);
		act->setPos(inv->iconX, inv->iconY);
		act->setRatio(1024, 1024);
		act->display();
	} else {
		act->setEffect(4);
		act->setMaskDepth(0, 2);
		act->setPos(inv->iconX, inv->iconY);
		act->setRatio(1024, 1024);

		// Don't draw "held" item
		if (objNum < 0 || _vm->game()->settings()->objectNum == objNum) return;

		if (mode % 2 == 0) {
			act->setFrame(objNum + 1);
		} else switch (inv->mouseState) {
		case 0:
		case 4:
		case 5:
		case 6:
			act->setFrame(objNum + 1);
			break;
		case 1:
		case 3:
			if (inv->blinkLight & 4)
				act->setFrame(0);
			else
				act->setFrame(1);
			act->display();
			act->setFrame(objNum + 1);
			break;
		case 2:
			if (inv->selectedBox == inv->selectedBox2) {
				act->setEffect(0);
				act->setRatio(768, 768);
				if (inv->blinkLight & 4)
					act->setFrame(0);
				else
					act->setFrame(1);
				act->display();
				act->setFrame(objNum + 1);
			} else {
				if (inv->blinkLight & 4)
					act->setFrame(0);
				else
					act->setFrame(1);
				act->display();
				act->setFrame(objNum + 1);
			}
			break;
		}
	}

	act->display();
}

uint16 Screen::getTextWidth(const char *text) {
	uint16 w = 0;
	for (int i = 0; text[i] != '\0'; ++i) {
		switch (text[i]) {
		case ' ':
			w += 4;
			break;
		// This check isn't in the original engine
		/*case '\t':
			w += 12;
			break;*/
		default:
			w += *((const uint8 *)(_font->getCharData(text[i])));
		}
		++w;
	}
	if (w > 0) --w;

	return w;
}

void Screen::displayDoors() {
	Common::Array<RoomDoor> *roomDoors = _vm->game()->getDoors();

	for (uint i = 0; i < roomDoors->size(); i++) {
		Actor *act = _vm->actorMan()->get((*roomDoors)[i].actorId);

		act->setFrame((*roomDoors)[i].frame);
	}
}

void Screen::narratorScrollInit(char *text) {
	_narratorScrollText = text;
	_narratorTextSurface = new byte[SCREEN_W * 40];
	memset(_narratorTextSurface, 0, SCREEN_W * 40);
	_narratorScrollStatus = 1;

	_narratorWord = strtok(_narratorScrollText, " ");
}

void Screen::narratorScrollUpdate() {
	static int scrollSpeed, scrollTimer, scrollPos;

	if (!_vm->game()->isNarratorPlaying()) {
		// TODO - hack
		return;
	}

	// TODO - another hack
	if (_narratorScrollText == 0)
		return;

	switch (_narratorScrollStatus) {
	case 1:
		scrollSpeed = 40;
		scrollTimer = 0;
		scrollPos = 0;
		_narratorScrollStatus = 2;
		narratorWriteLine(_narratorTextSurface + 20 * SCREEN_W);
		break;
	case 2:
		if (_narratorWord == 0) {
			_narratorScrollStatus = 5;
		} else {
			narratorWriteLine(_narratorTextSurface + 30 * SCREEN_W);
			_narratorScrollStatus = 3;
		}
		break;
	case 3:
		scrollTimer += scrollSpeed;

		// Left click speeds up the scrolling
		// FIXME: doesn't work beyond one frame due to resetInput
		if (_vm->input()->getLeftClick())
			scrollTimer += 2 * scrollSpeed;

		if (scrollTimer >= 256) {
			scrollTimer -= 256;
			scrollPos++;
			if (scrollPos == 10) {
				scrollPos = 0;
				memmove(_narratorTextSurface, _narratorTextSurface + 10 * SCREEN_W, 30 * SCREEN_W);
				memset(_narratorTextSurface + 30 * SCREEN_W, 0, 10 * SCREEN_W);
				_narratorScrollStatus = 2;
			}
		}
		break;
	}

	drawPanel(_vm->panel()->getPanelBackground());

	byte *from = _narratorTextSurface + (scrollPos + 3) * SCREEN_W;
	byte *to = _screenBuf + 172 * SCREEN_W;
	for (int i = 0; i < 27 * SCREEN_W; ++i) {
		if (from[i] != 0)
			to[i] = from[i];
	}
}

void Screen::narratorScrollDelete() {
	delete[] _narratorScrollText;
	_narratorScrollText = 0;
	delete[] _narratorTextSurface;
	_narratorTextSurface = 0;
	_narratorScrollStatus = 0;
}

void Screen::narratorWriteLine(byte *buf) {
	int width;
	int col = 6;
	char word[140];

	buf += col;

	while (_narratorWord) {
		// Calculate the width + space char
		width = getTextWidth(_narratorWord) + 4 + 1;

		if (col + width > 308)
			return;

		strcpy(word, _narratorWord);
		strcat(word, " ");

		writeText(buf, word, 0, col, 25, true);
		col += width;

		_narratorWord = strtok(NULL, " ");
	}
}

uint16 Screen::calcWordWidth(const char *word) {
	uint8 width = 0;
	for (int i = 0; word[i] != '\0'; ++i)
		width += *(const uint8 *)_font->getCharData(word[i]) + 1;
	return width + 4; // +4 for space char
}

void Screen::writeTextCentered(byte *buf, const char *text, uint8 row, uint8 color, bool isEmbossed) {
	uint8 col = (SCREEN_W - getTextWidth(text)) / 2;
	writeText(buf, text, row, col, color, isEmbossed);
}

void Screen::writeTextWrap(byte *buf, const char *text, uint8 row, uint16 col, uint16 wrapCol, uint8 color, bool isEmbossed) {
	uint16 currCol = col;
	char *txt = new char[strlen(text) + 1];
	strcpy(txt, text);
	for (char *word = strtok(txt, " "); word != 0; word = strtok(NULL, " ")) {
		uint16 wordWidth = calcWordWidth(word);
		if (currCol + wordWidth >= wrapCol) {
			buf += SCREEN_W;
			currCol = col;
		}
		writeText(buf, word, row, currCol, color, isEmbossed);
		currCol += wordWidth;
	}
	delete[] txt;
}

void Screen::writeTextRight(byte *buf, const char *text, uint8 row, uint16 col, uint8 color, bool isEmbossed) {
	writeText(buf, text, row, col - getTextWidth(text), color, isEmbossed);
}
void Screen::writeText(byte *buf, const char *text, uint8 row, uint16 col, uint8 color, bool isEmbossed) {
	if (isEmbossed)
		writeTextStyle(buf, text, row, col, 0, true);

	writeTextStyle(buf, text, row, col, color, false);

	// Report dirty rect if above panel area
	if (buf == _screenBuf && row < ROOM_H) {
		if (isEmbossed)
			_dirtyRects->push_back(Rect(col - 1, row - 1, col + getTextWidth(text) + 1, row + 8 + 1));
		else
			_dirtyRects->push_back(Rect(col, row, col + getTextWidth(text), row + 8));
	}
}
void Screen::writeTextStyle(byte *buf, const char *text, uint8 startRow, uint16 startCol, uint8 color, bool isBackground) {
	uint16 col = startCol;
	uint8 charWidth;
	const byte *data;

	// The "Nunchukas" description string in room 64 overflows over the right edge of the screen.
	// This happens in the original as well.

	for (int i = 0; text[i] != '\0'; ++i) {
		switch (text[i]) {
		case ' ':
			col += 4;
			break;
		case '\t':
			col += 12;
			break;
		default:
			data = _font->getCharData(text[i]);
			charWidth = (uint8)*data;
			++data;

			for (uint w = 0; w < charWidth; ++w) {
				for (uint8 h = 0; h < 8; ++h) {
					if (*data != 0) {
						if (isBackground) {
							*(buf + SCREEN_W * (startRow + h) + col + w - 1) = 3;
							*(buf + SCREEN_W * (startRow + h - 1) + col + w) = 3;
							*(buf + SCREEN_W * (startRow + h - 1) + col + w - 1) = 3;
							*(buf + SCREEN_W * (startRow + h) + col + w + 1) = 53;
							*(buf + SCREEN_W * (startRow + h + 1) + col + w) = 53;
							*(buf + SCREEN_W * (startRow + h + 1) + col + w + 1) = 53;
						} else {
							*(buf + SCREEN_W * (startRow + h) + col + w) = color;
						}
					}
					++data;
				}
			}
			col += charWidth;
		}

		col += 1;
	}
}

void Screen::drawBoxScreen(int x, int y, int width, int height, byte color) {
	drawBox(_screenBuf, x, y, width, height, color);
	_dirtyRects->push_back(Rect(x, y, x + width, y + height));
}

void Screen::drawBox(byte *surface, int x, int y, int width, int height, byte color) {
	byte *dest = surface + y * SCREEN_W + x;
	for (int i = 0; i < height; i++)
		memset(dest + i * SCREEN_W, color, width);
}

byte *Screen::createZoomBlur(int x, int y) {
	byte *to, *to2, *result;
	int sourceOffset = 0;
	int tempOffset;

	byte *surface = new byte[SCREEN_W * 179 + 120];
	result = surface;

	x = CLIP(x, 54, 260);
	y = CLIP(y, 28, 136);

	x -= 53;
	y -= 28;

	sourceOffset += y * SCREEN_W + x;

	tempOffset = sourceOffset;
	to = surface;
	surface += 2*SCREEN_W + 18;
	to2 = surface;

	for (int i = 0; i < 57; i++) {
		surface = to2;
		sourceOffset = tempOffset;

		for (int j = 0; j < 108; j++) {
			byte color = ((byte *)_roomBackground->getPixels())[sourceOffset];
			sourceOffset++;

			*surface = color;
			*(surface-2) = color;
			*(surface+2) = color;
			*(surface-2*SCREEN_W-16) = color;
			*(surface+2*SCREEN_W+16) = color;
			*(surface-SCREEN_W-7) = color;
			*(surface+SCREEN_W+7) = color;
			*(surface-SCREEN_W-9) = color;
			*(surface+SCREEN_W+9) = color;

			surface += 3;
		}

		tempOffset += SCREEN_W;
		to2 += SCREEN_W*3 + 24;
	}

	for (int i = 0; i < 168; i++) {
		memcpy(to + i*SCREEN_W,
			   to + 2*SCREEN_W + 18 + i*(SCREEN_W +8),
			   SCREEN_W);
	}

	return result;
}

} // End of namespace Kom
