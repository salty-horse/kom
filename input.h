/* ScummVM - Scumm Interpreter
 * Copyright (C) 2005-2006 The ScummVM project
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

#ifndef KOM_INPUT_H
#define KOM_INPUT_H

namespace Kom {

class Input {
public:

	Input(OSystem *system);
	~Input();

	void checkKeys();
	bool debugMode() const { return _debugMode; }
	void resetDebugMode() { _debugMode = false; }

private:

	OSystem *_system;

	int _inKey;
	bool _debugMode;
};

} // End of namespace Kom

#endif
