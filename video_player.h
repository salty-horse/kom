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

#ifndef KOM_VIDEO_PLAYER_H
#define KOM_VIDEO_PLAYER_H

#include "video/video_decoder.h"
#include "video/smk_decoder.h"
#include "video/flic_decoder.h"

#include "kom/kom.h"
#include "kom/sound.h"

namespace Kom {

class KomEngine;

class FlicDecoder : public Video::FlicDecoder {
	void setPalette(byte *pal) {}
};

class VideoPlayer {
public:
	VideoPlayer(KomEngine *vm);
	~VideoPlayer() { }

	bool playVideo(const Common::Path &filename);

	void loadTalkVideo(const Common::Path &filename, byte *background);
	void drawTalkFrame(int frame);
	void drawTalkFrameCycle();
private:
	void processFrame();
	void processEvents();
	bool _skipVideo;
	KomEngine *_vm;
	Common::EventManager *_eventMan;
	Video::SmackerDecoder _smk;
	FlicDecoder _flic;
	Video::VideoDecoder *_player;
	byte *_background;
	SoundHandle _soundHandle;
};

} // End of namespace Kom

#endif
