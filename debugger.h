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

#ifndef KOM_DEBUGGER_H
#define KOM_DEBUGGER_H

#include "gui/debugger.h"

namespace Kom {

class KomEngine;

class Debugger : public GUI::Debugger {
public:

	Debugger(KomEngine *vm);

protected:
	bool cmdRoom(int argc, const char **argv);
	bool cmdGive(int argc, const char **argv);
	bool cmdProc(int argc, const char **argv);
	bool cmdDay(int argc, const char **argv);
	bool cmdNight(int argc, const char **argv);
	bool cmdGold(int argc, const char **argv);

private:

	KomEngine *_vm;
};

} // End of namespace Kom

#endif
