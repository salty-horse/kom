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

#ifndef KOM_DETECTION_H
#define KOM_DETECTION_H

#include "common/fs.h"
#include "common/str.h"

namespace Kom {

static inline bool isGameDataFile(const Common::String &name) {
	return name.equalsIgnoreCase("thidney.dsk") || name.equalsIgnoreCase("shahron.dsk");
}

static inline bool containsGameData(const Common::FSNode &dir) {
	Common::FSList files;
	if (!dir.isDirectory() || !dir.getChildren(files, Common::FSNode::kListFilesOnly))
		return false;

	for (Common::FSList::const_iterator file = files.begin(); file != files.end(); ++file) {
		if (isGameDataFile(file->getName()))
			return true;
	}

	return false;
}

static inline bool hasGameData(const Common::FSNode &dir) {
	if (containsGameData(dir))
		return true;

	Common::FSList children;
	if (!dir.isDirectory() || !dir.getChildren(children, Common::FSNode::kListDirectoriesOnly))
		return false;

	for (Common::FSList::const_iterator child = children.begin(); child != children.end(); ++child) {
		if (child->getName().matchString("cd?", true) && containsGameData(*child))
			return true;
	}

	return false;
}

} // End of namespace Kom

#endif
