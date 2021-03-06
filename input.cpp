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

#include "common/system.h"
#include "common/events.h"
#include "common/textconsole.h"
#include "common/keyboard.h"
#include "common/rect.h"

#include "kom/kom.h"
#include "kom/character.h"
#include "kom/database.h"
#include "kom/screen.h"
#include "kom/game.h"
#include "kom/input.h"

namespace Kom {

Input::Input(KomEngine *vm, OSystem *system) : _system(system), _vm(vm), _debugMode(false),
	_mouseX(0), _mouseY(0), _leftClick(false), _rightClick(false) {
}

Input::~Input() {

}

void Input::loopInput() {
	// TODO - more checks
	if (_vm->_flicLoaded == 0) {
		handleMouse();
	}
}

void Input::handleMouse() {
	static Settings *settings = _vm->game()->settings();
	static Player *player = _vm->game()->player();
	Character *playerChar = _vm->database()->getChar(0);
	uint16 gotoX, gotoY = 0;
	int8 donutState;

	if (settings->mouseMode == 0 && _leftClick) {

		// Panel click
		if (settings->mouseY >= INVENTORY_OFFSET) {

			// Drop weapon/spell before entering inventory
			if (settings->objectNum >= 0 && settings->objectType != OBJECT_ITEM) {
				settings->objectType = OBJECT_NONE;
				settings->objectNum = -1;
			}

			if (settings->objectNum >= 0) {
				_vm->game()->doInventory(&settings->object2Num, &settings->object2Type, 0, 7);

				if (settings->objectNum < 0) {
					settings->objectType = settings->object2Type;
					settings->objectNum = settings->object2Num;
					settings->object2Type = OBJECT_NONE;
					settings->object2Num = -1;
					player->commandState = 0;
				}

				if (settings->object2Num >= 0) {
					playerChar->stopChar();
					player->command = CMD_USE;
					player->commandState = 1;
				} else {
					_vm->game()->checkUseImmediate(settings->object2Type, settings->object2Num);
					_vm->game()->checkUseImmediate(settings->objectType, settings->objectNum);
				}

			} else {
				settings->object2Type = OBJECT_NONE;
				settings->object2Num = -1;
				_vm->game()->doInventory(&settings->objectNum, &settings->objectType, 0, 7);
				_vm->game()->checkUseImmediate(settings->objectType, settings->objectNum);
				player->commandState = 0;
			}

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

		// Room click
		} else {
			if (settings->mouseState == 2) {
				gotoX = settings->collideBoxX;
				gotoY = settings->collideBoxY;
				settings->collideType = COLLIDE_BOX;

			} else switch (settings->overType) {
			case COLLIDE_CHAR:
				gotoX = settings->collideCharX;
				gotoY = settings->collideCharY;
				settings->collideType = COLLIDE_CHAR;
				break;
			case COLLIDE_OBJECT:
				gotoX = settings->collideObjX;
				gotoY = settings->collideObjY;
				settings->collideType = COLLIDE_OBJECT;
				break;
			default:
				gotoX = settings->collideBoxX;
				gotoY = settings->collideBoxY;
				settings->collideType = COLLIDE_BOX;
			}

			if (settings->collideType == COLLIDE_CHAR || settings->collideType == COLLIDE_OBJECT) {
				if (settings->objectNum < 0) {
					donutState = _vm->game()->doDonut(0, 0);
				} else {
					donutState = 0;
					switch (settings->objectType) {
					case OBJECT_SPELL:
						if (_vm->database()->getChar(settings->collideChar)->_isAlive)
							player->command = CMD_CAST_SPELL;
						break;
					case OBJECT_WEAPON:
						if (_vm->database()->getChar(settings->collideChar)->_isAlive)
							player->command = CMD_FIGHT;
						break;
					case OBJECT_ITEM:
						player->command = CMD_USE;
						break;
					default:
						break;
					}

				}

			} else if (settings->objectNum >= 0) {
				donutState = -1;
			} else {
				donutState = 0;
				player->command = CMD_WALK;
			}

			if (donutState != -1) {
				switch (player->command) {
				case CMD_WALK:
					if (settings->collideType == COLLIDE_NONE)
						break;
					playerChar->_gotoX = gotoX;
					playerChar->_gotoY = gotoY;
					player->commandState = 1;
					player->collideType = COLLIDE_NONE;
					player->collideNum = -1;
					break;

				case CMD_NOTHING:
					break;

				case CMD_USE:
					if (settings->collideType == COLLIDE_NONE)
						break;
					playerChar->_gotoX = gotoX;
					playerChar->_gotoY = gotoY;
					player->commandState = 1;

					switch (settings->collideType) {
					case COLLIDE_CHAR:
						player->collideType = COLLIDE_CHAR;
						player->collideNum = settings->collideChar;
						break;
					case COLLIDE_OBJECT:
						player->collideType = COLLIDE_OBJECT;
						player->collideNum = settings->collideObj;
						break;
					default:
						error("Error in USE");
					}

					playerChar->_isBusy = true;
					break;

				case CMD_TALK_TO:
					if (settings->collideType == COLLIDE_NONE)
						break;
					playerChar->_gotoX = gotoX;
					playerChar->_gotoY = gotoY;
					player->commandState = 1;
					player->collideType = COLLIDE_CHAR;
					player->collideNum = settings->collideChar;
					playerChar->_isBusy = true;
					break;

				case CMD_PICKUP:
					if (settings->collideType == COLLIDE_NONE)
						break;
					playerChar->_gotoX = gotoX;
					playerChar->_gotoY = gotoY;
					player->commandState = 1;
					player->collideType = COLLIDE_OBJECT;
					player->collideNum = settings->collideObj;
					playerChar->_isBusy = true;
					break;

				case CMD_LOOK_AT:
					if (settings->collideType == COLLIDE_NONE)
						break;
					playerChar->_gotoX = gotoX;
					playerChar->_gotoY = gotoY;
					player->commandState = 1;

					// Character
					if (settings->collideType == COLLIDE_CHAR) {
						player->collideType = COLLIDE_CHAR;
						player->collideNum = settings->collideChar;
						if (_vm->database()->getChar(settings->collideChar)->_isAlive) {
							playerChar->_gotoX = playerChar->_screenX;
							playerChar->_gotoY = playerChar->_screenY;
							player->commandState = 2;
						} else {
							playerChar->_isBusy = true;
						}

					// Object
					} else if (settings->collideType == COLLIDE_OBJECT) {
						player->collideType = COLLIDE_OBJECT;
						player->collideNum = settings->collideObj;
						playerChar->_isBusy = true;
					} else {
						error("Error in LOOK_AT");
					}
					break;

				case CMD_FIGHT:
					if (settings->collideType == COLLIDE_NONE)
						break;

					// Select weapon from inventory
					if (settings->objectNum < 0 || settings->objectType != OBJECT_WEAPON)
						_vm->game()->doInventory(&settings->objectNum, &settings->objectType, 0, 2);

					if (settings->objectNum < 0 || settings->objectType != OBJECT_WEAPON ||
					    !_vm->database()->getChar(settings->collideChar)->_isAlive) {

						player->command = CMD_FIGHT;
						player->commandState = 0;
						playerChar->stopChar();
					} else {
						// Walk to char
						playerChar->_gotoX = gotoX;
						playerChar->_gotoY = gotoY;

						player->commandState = 1;
						player->collideType = COLLIDE_CHAR;
						player->collideNum = settings->collideChar;
						playerChar->_isBusy = true;
					}

					break;

				case CMD_CAST_SPELL:
					if (settings->collideType == COLLIDE_NONE)
						break;

					// Select spell from inventory
					if (settings->objectNum < 0 || settings->objectType != OBJECT_SPELL)
						_vm->game()->doInventory(&settings->objectNum, &settings->objectType, 0, 4);

					if (settings->objectNum < 0 || settings->objectType != OBJECT_SPELL ||
					    !_vm->database()->getChar(settings->collideChar)->_isAlive) {

						player->command = CMD_CAST_SPELL;
						player->commandState = 0;
						playerChar->stopChar();
					} else {
						// Stand in place
						playerChar->_gotoX = playerChar->_screenX;
						playerChar->_gotoY = playerChar->_screenY;

						player->command = CMD_CAST_SPELL;
						player->commandState = 1;
						player->collideType = COLLIDE_CHAR;
						player->collideNum = settings->collideChar;
						playerChar->_isBusy = true;
					}
					break;
				default:
					error("Wrong player command");
				}
			}
		}
	}

	// TODO - handle right click, menu request
	if (_rightClick) {
		// TODO - handle menu

		_vm->game()->stopNarrator();

		if (player->commandState < 2 /* TODO - menu check */) {

			// Discard held object
			if (settings->objectNum >= 0) {
				settings->objectType = OBJECT_NONE;
				settings->objectNum = -1;
			}

			player->command = CMD_NOTHING;
			player->commandState = 0;
			playerChar->stopChar();
		}
	}
}

// TODO: hack
void Input::checkKeys() {
	Common::Event event;

	Common::EventManager *eventMan = _system->getEventManager();
	while (eventMan->pollEvent(event)) {
		switch (event.type) {
		case Common::EVENT_KEYDOWN:
			if (event.kbd.hasFlags(Common::KBD_CTRL)) {
				if (event.kbd.keycode == 'd') {
					_debugMode = true;
				}
			} else {
				_inKey = event.kbd.keycode;
				switch (_inKey) {
					case Common::KEYCODE_ESCAPE:
						_vm->quitGame();
						break;
					default:
						break;
				}
			}
			break;

		case Common::EVENT_MOUSEMOVE:
			_mouseX = event.mouse.x;
			_mouseY = event.mouse.y;
			break;

		case Common::EVENT_LBUTTONDOWN:
			_leftClick = true;
			break;

		case Common::EVENT_RBUTTONDOWN:
			_rightClick = true;
			break;

		case Common::EVENT_QUIT:
			_vm->quitGame();
			break;

		default:
			break;
		}
	}
}


void Input::setMousePos(uint16 x, uint16 y) {
	_mouseX = x;
	_mouseY = y;
	_system->warpMouse(x, y);
}

} // End of namespace Kom
