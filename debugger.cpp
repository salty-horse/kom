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

#include "gui/debugger.h"

#include "kom/kom.h"
#include "kom/debugger.h"
#include "kom/database.h"
#include "kom/game.h"

namespace Kom {

Debugger::Debugger(KomEngine *vm) : GUI::Debugger(), _vm(vm) {

	registerCmd("room", WRAP_METHOD(Debugger, cmdRoom));
	registerCmd("give", WRAP_METHOD(Debugger, cmdGive));
	registerCmd("proc", WRAP_METHOD(Debugger, cmdProc));
	registerCmd("day", WRAP_METHOD(Debugger, cmdDay));
	registerCmd("night", WRAP_METHOD(Debugger, cmdNight));
	registerCmd("gold", WRAP_METHOD(Debugger, cmdGold));
}

bool Debugger::cmdRoom(int argc, const char **argv) {
	if (argc == 2) {
		uint16 roomNum = atoi(argv[1]);
		if (roomNum == 0)
			return false;

		_vm->game()->settings()->currLocation = roomNum;
		_vm->database()->getChar(0)->_locationId = roomNum;
		_vm->database()->getChar(0)->_lastLocation = roomNum;
		_vm->database()->getChar(0)->_lastBox = 0;

		_vm->game()->enterLocation(roomNum);
		return false;
	}
	return true;
}

bool Debugger::cmdGive(int argc, const char **argv) {
	if (argc == 2) {
		uint16 objId = atoi(argv[1]);
		if (objId <= 1) {
			debugPrintf("No such object\n");
			return true;
		}

		objId--;
		Object *obj = _vm->database()->getObj(objId);
		if (obj == NULL) {
			debugPrintf("No such object\n");
			return true;
		} else {
			if (_vm->database()->giveObject(objId, 0))
				debugPrintf("Got %s\n", obj->desc);
			else
				debugPrintf("Could not get %s\n", obj->desc);
		}
	}
	return true;
}

bool Debugger::cmdProc(int argc, const char **argv) {
	if (argc == 2) {
		uint16 procNum = atoi(argv[1]);
		Process *proc = _vm->database()->getProc(procNum);
		if (proc == NULL) {
			debugPrintf("No such process: %hd\n", procNum);
			return true;
		}

		debugPrintf("Process name is %s\n", proc->name);

		for (Common::List<Command>::iterator i = proc->commands.begin(); i != proc->commands.end(); ++i) {
			debugPrintf("- Command %d - value %hd\n", i->cmd, i->value);

			for (Common::List<OpCode>::iterator j = i->opcodes.begin(); j != i->opcodes.end(); ++j) {
				debugPrintf("|- Opcode %d - (%s, %d, %d, %d, %d, %d)\n", j->opcode,
						j->arg1, j->arg2, j->arg3, j->arg4, j->arg5, j->arg6);
			}
		}
	}
	return true;
}

bool Debugger::cmdDay(int argc, const char **argv) {
	_vm->game()->setDay();
	return false;
}

bool Debugger::cmdNight(int argc, const char **argv) {
	_vm->game()->setNight();
	return false;
}

bool Debugger::cmdGold(int argc, const char **argv) {
	if (argc == 2) {
		Character *chr = _vm->database()->getChar(0);
		int amount = atoi(argv[1]);
		chr->_gold += amount;

		if (chr->_gold < 0)
			chr->_gold = 0;
	}
	return false;
}

} // End of namespace Kom
