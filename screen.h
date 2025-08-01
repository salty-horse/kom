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

#ifndef KOM_SCREEN_H
#define KOM_SCREEN_H

#include "common/scummsys.h"
#include "common/str.h"

#include "kom/video_player.h"

class OSystem;

namespace Common {
struct Path;
struct Rect;
template<typename t_T> class List;
}

namespace Graphics {
struct Surface;
}

namespace Kom {

class Font;
class KomEngine;
struct Inventory;

enum {
	SCREEN_W = 320,
	SCREEN_H = 200,
	MOUSE_W = 40,
	MOUSE_H = 40,
	PANEL_H = 32,
	ROOM_H = SCREEN_H - PANEL_H,
	INVENTORY_OFFSET = 344
};

struct ColorSet {
	ColorSet(const Common::Path &filename);
	ColorSet(const char *filename): ColorSet(Common::Path(filename)) {}
	~ColorSet();

	uint size;
	byte *data;
};

class Screen {
public:

	Screen(KomEngine *vm, OSystem *system);
	~Screen();

	bool init();

	byte *screenBuf() { return _screenBuf; }

	void processGraphics(int mode, bool samplePlaying = false);
	void drawDirtyRects();
	void gfxUpdate();
	void clearScreen(bool now = false);
	void clearRoom();
	void drawActorFrameScaled(const int8 *data, uint16 width, uint16 height, int16 xStart, int16 yStart,
			int16 xEnd, int16 yEnd, int maskDepth, bool invisible = false);
	void drawActorFrameScaledAura(const int8 *data, uint16 width, uint16 height, int16 xStart, int16 yStart,
			int16 xEnd, int16 yEnd, int maskDepth);
	void drawActorFrame(const int8 *data, uint16 width, uint16 height, int16 xStart, int16 yStart, bool greyedOut = false);
	void drawMouseFrame(const int8 *data, uint16 width, uint16 height, int16 xOffset, int16 yOffset);

	void setMouseCursor(const byte *buf, uint w, uint h, int hotspotX, int hotspotY);
	void showMouseCursor(bool show);
	void updateCursor();
	bool isCursorVisible();

	void drawPanel(const byte *panelData);
	void clearPanel();
	void updatePanelOnScreen(bool clearScreenFlag);

	void loadBackground(const Common::Path &filename);
	void updateBackground();
	void drawBackground();
	void pauseBackground(bool pause) { _roomBackgroundFlic.pauseVideo(pause); }
	void drawTalkFrame(const Graphics::Surface *frame, const byte *background);
	void loadMask(const Common::Path &filename);

	void drawInventory(Inventory *inv);
	void drawFightBars();

	void useColorSet(ColorSet *cs, uint start);
	void setPaletteColor(int index, const byte color[]);
	void createSepia(bool shop);
	void freeSepia();
	void copySepia();

	void setBrightness(uint16 brightness) { _newBrightness = brightness; }

	void displayDoors();

	void narratorScrollInit(char *text);
	void narratorScrollUpdate();
	void narratorScrollDelete();
	void narratorWriteLine(byte *buf);

	uint16 calcWordWidth(const char *word);

	void writeTextCentered(byte *buf, const char *text, uint8 row, uint8 color, bool isEmbossed);
	void writeTextWrap(byte *buf, const char *text, uint8 row, uint16 col, uint16 width, uint8 color, bool isEmbossed);
	void writeTextRight(byte *buf, const char *text, uint8 row, uint16 col, uint8 color, bool isEmbossed);
	void writeText(byte *buf, const char *text, uint8 row, uint16 col, uint8 color, bool isEmbossed);

	uint16 getTextWidth(const char *text);

	void drawBoxScreen(int x, int y, int width, int height, byte color);
	void drawBox(byte *surface, int x, int y, int width, int height, byte color);

	byte *createZoomBlur(int x, int y);

private:

	void copyRectListToScreen(const Common::List<Common::Rect> *);

	void drawActorFrameLine(byte *outBuffer, const int8 *rowData, uint16 length);

	void writeTextStyle(byte *buf, const char *text, uint8 startRow, uint16 startCol, uint8 color, bool isBackground);

	void setPaletteBrightness();

	void printIcon(Inventory *inv, int objNum, int mode);

	void updateActionStrings();

	OSystem *_system;
	KomEngine *_vm;

	uint8 *_screenBuf;
	uint8 *_mouseBuf;
	FlicDecoder _roomBackgroundFlic;
	FlicDecoder _roomMaskFlic;
	const Graphics::Surface *_roomBackground;
	const Graphics::Surface *_roomMask;
	Font *_font;

	bool _fullRedraw;

	byte *_sepiaScreen;
	byte _backupPalette[256 * 4];

	ColorSet *_c0ColorSet;
	ColorSet *_orangeColorSet;
	ColorSet *_greenColorSet;
	bool _paletteChanged;

	uint16 _currBrightness;
	uint16 _newBrightness;

	uint32 _lastFrameTime;

	Common::List<Common::Rect> *_dirtyRects;
	Common::List<Common::Rect> *_prevDirtyRects;

	char *_narratorScrollText;
	char *_narratorWord;
	byte *_narratorTextSurface;
	uint8 _narratorScrollStatus;
};

} // End of namespace Kom

#endif
