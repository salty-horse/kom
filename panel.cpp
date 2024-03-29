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

#include <string.h>
#include "common/file.h"

#include "kom/kom.h"
#include "kom/actor.h"
#include "kom/panel.h"
#include "kom/game.h"
#include "kom/screen.h"
#include "kom/sound.h"

using Common::File;
using Common::String;

namespace Kom {

Panel::Panel(KomEngine *vm, const char *filename) : _vm(vm),
	_isEnabled(true), _isLoading(false), _suppressLoading(0),
	_gotObjTime(0) {

	File f;
	f.open(filename);

	f.seek(4);

	_panelData = new byte[SCREEN_W * PANEL_H];
	f.read(_panelData, SCREEN_W * PANEL_H);

	f.close();

	_panelBuf = new byte[SCREEN_W * PANEL_H];
	memcpy(_panelBuf, _panelData, SCREEN_W * PANEL_H);

	_isDirty = true;
}

Panel::~Panel() {
	delete[] _panelData;
	delete[] _panelBuf;
}

void Panel::clear() {
	memcpy(_panelBuf, _panelData, SCREEN_W * PANEL_H);
}

void Panel::update() {
	_isDirty = false;
	enable(true);
	clear();

	// Draw texts
	// FIXME - the location desc is fully centered, while the original prints it
	//         4 pixels to the right.
	// Actually, the original prints all text 4 pixels to the right, so we're using
	// 6 as the column instead of 10 for the two bottom lines.
	if (!_locationDesc.empty())
		_vm->screen()->writeTextCentered(_panelBuf, _locationDesc.c_str(), 3, 31, true);

	if (!_actionDesc.empty())
		_vm->screen()->writeText(_panelBuf, _actionDesc.c_str(), 12, 6, 31, true);

	if (!_hotspotDesc.empty())
		_vm->screen()->writeText(_panelBuf, _hotspotDesc.c_str(), 22, 6, 31, true);

	// FIXME: check loading in the middle of object animation
	if (_isLoading)
		return;

	_vm->screen()->drawPanel(_panelBuf);

	// Draw got and lost objects

	if (_gotObjTime == 0) {
		if (!_gotObjects.empty())
			_gotObjTime = 24;

	} else {
		// Don't animate if narrator is talking
		if (_vm->game()->isNarratorPlaying())
			_gotObjTime = 24;

		Actor *objectActor = _vm->actorMan()->getObjects();
		uint16 xRatio, yRatio;
		uint8 frame;
		int currObj = *_gotObjects.begin();

		_gotObjTime--;

		// Got object
		if (currObj > 0) {

			// Regular object
			if (currObj < 10000) {
				frame = currObj + 1;
				if (_gotObjTime == 16)
					_vm->sound()->playSampleSFX(_vm->_swipeSample, false);

			// Money
			} else {
				objectActor = _vm->actorMan()->getCoinage();
				currObj -= 10000;
				frame = currObj;
				if (_gotObjTime == 16)
					_vm->sound()->playSampleSFX(_vm->_cashSample, false);
			}

			objectActor->enable(1);

			// Zoom into screen
			if (_gotObjTime > 16) {
				xRatio = yRatio = 0x600 - 0x600 / (24 - _gotObjTime);
				objectActor->setEffect(0);
				objectActor->setPos(303, 185);
				objectActor->setRatio(xRatio, yRatio);
				objectActor->setFrame(frame);

			// Stay on screen
			} else if (_gotObjTime > 8) {
				objectActor->setEffect(4);
				objectActor->setPos(303, 185);
				objectActor->setRatio(1024, 1024);
				objectActor->setFrame(frame);

			// Move below screen
			} else {
				objectActor->setEffect(4);
				objectActor->setPos(303, 185 + (8 - _gotObjTime)*(8 - _gotObjTime));
				objectActor->setRatio(1024, 1024);
				objectActor->setFrame(frame);
			}

		// Lost object
		} else {

			currObj = -currObj;

			if (_gotObjTime == 12)
				_vm->sound()->playSampleSFX(_vm->_loseItemSample, false);

			// Regular object
			if (currObj < 10000) {
				frame = currObj + 1;

			// Money
			} else {
				objectActor = _vm->actorMan()->getCoinage();
				currObj -= 10000;
				frame = currObj;
			}

			objectActor->enable(1);

			// Move from below screen
			if (_gotObjTime > 16) {
				objectActor->setEffect(4);
				objectActor->setPos(303, 185 + (_gotObjTime - 16)*(_gotObjTime - 16));
				objectActor->setRatio(1024, 1024);
				objectActor->setFrame(frame);

			// Stay on screen
			} else if (_gotObjTime > 8) {
				objectActor->setEffect(4);
				objectActor->setPos(303, 185);
				objectActor->setRatio(1024, 1024);
				objectActor->setFrame(frame);

			// Zoom out of screen
			} else {
				xRatio = yRatio = 0x2710 / (24 - _gotObjTime);
				objectActor->setEffect(0);
				objectActor->setPos(303, 185);
				objectActor->setRatio(xRatio, yRatio);
				objectActor->setFrame(frame);
			}
		}

		// Display object
		objectActor->display();
		objectActor->enable(0);
		if (_gotObjTime == 0)
			_gotObjects.pop_front();
	}
}

void Panel::showLoading(bool isLoading, bool clearScreen) {

	if (_suppressLoading > 0)
		return;

	if (_isLoading != isLoading) {
		_isLoading = isLoading;

		update();
		_vm->screen()->drawPanel(_panelBuf);

		Actor *mouse = _vm->actorMan()->getMouse();
		if (_isLoading) {
			mouse->enable(1);
			mouse->setScope(6, 0);
			mouse->setPos(303, 185);
			mouse->display();
			mouse->enable(0);
		} else {
			mouse->setPos(0, 0);
		}

		// The 'loading' icon should be copied directly to screen
		_vm->screen()->updatePanelOnScreen(clearScreen);
	}
}

void Panel::setLocationDesc(const char *desc) {
	_locationDesc = desc;
	_isDirty = true;
}

void Panel::setActionDesc(const char *desc) {
	_actionDesc = desc;
	_isDirty = true;
}

void Panel::setHotspotDesc(const char *desc) {
	_hotspotDesc = desc;
	_isDirty = true;
}

} // End of namespace Kom
