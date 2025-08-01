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

#include "common/events.h"
#include "common/system.h"
#include "common/keyboard.h"
#include "common/textconsole.h"
#include "common/path.h"

#include "graphics/surface.h"
#include "graphics/palette.h"
#include "graphics/paletteman.h"
#include "video/video_decoder.h"
#include "video/smk_decoder.h"

#include "kom/kom.h"
#include "kom/screen.h"
#include "kom/sound.h"
#include "kom/video_player.h"

using Common::Path;

namespace Kom {

VideoPlayer::VideoPlayer(KomEngine *vm) : _vm(vm), 
	_smk(), _background(0) {
	_eventMan = _vm->_system->getEventManager();
}

void VideoPlayer::processEvents() {
	Common::Event curEvent;

	while (_eventMan->pollEvent(curEvent)) {
		switch (curEvent.type) {
		case Common::EVENT_KEYDOWN:
			if (curEvent.kbd.keycode == Common::KEYCODE_SPACE)
				_skipVideo = true;
			break;
		case Common::EVENT_RBUTTONDOWN:
			_skipVideo = true;
			break;

		case Common::EVENT_RETURN_TO_LAUNCHER:
		case Common::EVENT_QUIT:
			_skipVideo = true;
			_vm->quitGame();
			break;
		default:
			break;
		}
	}
}

bool VideoPlayer::playVideo(const Path &filename) {
	_skipVideo = false;
	byte backupPalette[256 * 3];

	// Backup the palette
	_vm->_system->getPaletteManager()->grabPalette(backupPalette, 0, 256);

	// Figure out which player to use, based on extension

	Common::String basename = filename.baseName();
	if (basename.hasSuffix("smk")) {
		_player = &_smk;

		if (!_player->loadFile(filename))
			error("Could not load video file: %s\n", filename.toString().c_str());

	// Flic files have associated color set and audio files
	} else {
		_player = &_flic;

		if (!_player->loadFile(filename))
			error("Could not load video file: %s\n", filename.toString().c_str());

		basename.chop(3);
		Path filenameWithoutExt = filename.getParent().appendComponent(basename);

		ColorSet *cs = new ColorSet(filenameWithoutExt.append("cl"));
		_vm->screen()->useColorSet(cs, 0);
		delete cs;

		_vm->sound()->playFileSFX(filenameWithoutExt.append("raw"), &_soundHandle);

		_background = _vm->screen()->createZoomBlur(160, 100);
	}

	_vm->_system->fillScreen(0);
	_player->start();

	while (!_player->endOfVideo() && !_skipVideo && !_vm->shouldQuit()) {
		processEvents();
		processFrame();
	}

	// Sound sample should continue playing after video is done
	// (for example, when talking to the pixie)
	if (_skipVideo)
		_vm->sound()->stopHandle(_soundHandle);

	_player->close();

	if (_background) {
		delete[] _background;
		_background = 0;
	}

	_vm->_system->fillScreen(0);
	_vm->_system->updateScreen();

	// Restore the palette
	_vm->_system->getPaletteManager()->setPalette(backupPalette, 0, 256);

	return true;
}

void VideoPlayer::processFrame() {
	const Graphics::Surface *frame = _player->decodeNextFrame();

	// No frame decoded - this could be if the video is over, but the
	// audio track is still playing. e.g. kom/cutsconv/in2/in201t0.smk
	if (frame == NULL) {
		_vm->_system->delayMillis(10);
		return;
	}

	Graphics::Surface *screen = _vm->_system->lockScreen();

	if (_player->hasDirtyPalette() && _player == &_smk) {
		_vm->_system->getPaletteManager()->setPalette(_smk.getPalette(), 0, 256);
	}

	if (!_background) {
		for (uint16 y = 0; y < frame->h; y++)
			memcpy((byte *)screen->getPixels() + y * screen->pitch, (const byte *)frame->getPixels() + y * frame->pitch, frame->w);

	} else {
		memcpy((byte *)screen->getPixels(), _background, SCREEN_W * ROOM_H);
		for (int i = 0; i < SCREEN_W * ROOM_H; i++) {
			byte color = ((const byte *)frame->getPixels())[i];
			if (color != 0)
				((byte *)screen->getPixels())[i] = color;
		}
	}

	_vm->_system->unlockScreen();

	// Update the screen
	_vm->_system->updateScreen();

	// Wait before showing the next frame
	_vm->_system->delayMillis(_player->getTimeToNextFrame());
}

void VideoPlayer::loadTalkVideo(const Path &filename, byte *background) {
	if (!_flic.loadFile(filename))
		error("Could not load video file: %s\n", filename.toString().c_str());
	_flic.start();
	_background = background;
}

void VideoPlayer::drawTalkFrame(int frame) {
	const Graphics::Surface *surface = NULL;

	if (_flic.getCurFrame() + 1 >= frame) {
		_flic.rewind();
	}

	// Seek to frame
	while (_flic.getCurFrame() + 1 != frame) {
		surface = _flic.decodeNextFrame();
	}

	_vm->screen()->drawTalkFrame(surface, _background);
}

void VideoPlayer::drawTalkFrameCycle() {
	if (_flic.endOfVideo())
		_flic.rewind();

	const Graphics::Surface *surface = _flic.decodeNextFrame();
	_vm->screen()->drawTalkFrame(surface, _background);
}

} // End of namespace Kom
