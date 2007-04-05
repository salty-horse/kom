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
#include "common/list.h"
#include "common/rect.h"

#include "kom/flicplayer.h"
#include "kom/font.h"

class OSystem;
class FilesystemNode;
class FlicPlayer;

namespace Kom {

enum {
	SCREEN_W = 320,
	SCREEN_H = 200,
	MOUSE_W = 40,
	MOUSE_H = 40,
	PANEL_H = 32
};

class KomEngine;

class Screen {
public:

	
	Screen(KomEngine *vm, OSystem *system);
	~Screen();

	bool init();

	void processGraphics();
	void gfxUpdate();
	void drawActorFrame(const int8 *data, uint16 width, uint16 height, uint16 xPos, uint16 yPos, int16 xOffset, int16 yOffset,
	                    int maskDepth);
	void drawMouseFrame(const int8 *data, uint16 width, uint16 height, int16 xOffset, int16 yOffset);

	void setMouseCursor(const byte *buf, uint w, uint h, int hotspotX, int hotspotY);
	void showMouseCursor(bool show);
	void displayMouse();

	void updateCursor();

	void drawPanel(const byte *panelData);
	void refreshPanelArea();
	void copyPanelToScreen(const byte *data);

	void loadBackground(FilesystemNode node);
	void updateBackground();
	void drawBackground();
	void setMask(const uint8 *data);

	void writeTextCentered(byte *buf, const char *text, uint8 row, uint8 color, bool isEmbossed);
	void writeText(byte *buf, const char *text, uint8 row, uint16 col, uint8 color, bool isEmbossed);

private:

	void copyRectListToScreen(const Common::List<Common::Rect> *);

	void drawActorFrameLine(uint8 *buf, uint16 bufWidth, const int8 *data, uint16 xPos, uint16 yPos, uint16 startPixel, uint16 endPixel,
	                        int maskDepth);
	byte *loadColorSet(FilesystemNode fsNode);

	uint16 getTextWidth(const char *text);
	void writeTextStyle(byte *buf, const char *text, uint8 startRow, uint16 startCol, uint8 color, bool isBackground);

	OSystem *_system;
	KomEngine *_vm;

	uint8 *_screenBuf;
	uint8 *_panelBuf;
	uint8 *_mouseBuf;
	uint8 *_mask;
	FlicPlayer *_roomBackground;
	Font *_font;
	uint32 _roomBackgroundTime;

	byte *_c0ColorSet;

	uint32 _lastFrameTime;

	Common::List<Common::Rect> *_dirtyRects;
	Common::List<Common::Rect> *_prevDirtyRects;
	const Common::List<Common::Rect> *_bgDirtyRects;
};

} // End of namespace Kom

#endif
