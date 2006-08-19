/* ScummVM - Scumm Interpreter
 * Copyright (C) 2004-2006 The ScummVM project
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

#ifndef KOM_SCREEN_H
#define KOM_SCREEN_H

#include "common/system.h"
#include "common/fs.h"

class OSystem;
class FilesystemNode;

namespace Kom {

class KomEngine;

class Screen {
public:

	enum {
		SCREEN_W = 320,
		SCREEN_H = 200
	};
	
	Screen(KomEngine *vm, OSystem *system);
	~Screen();

	bool init();

	void update();
	void drawActorFrame(int8 *data, uint16 width, uint16 height, uint16 xPos, uint16 yPos);

private:

	byte *loadColorSet(FilesystemNode fsNode);

	OSystem *_system;
	KomEngine *_vm;

	uint8 *_screenBuf;

	byte *_c0ColorSet;
};

} // End of namespace Kom

#endif
