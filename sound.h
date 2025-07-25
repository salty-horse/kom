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

#ifndef KOM_SOUND_H
#define KOM_SOUND_H

#include "common/scummsys.h"
#include "common/str.h"
#include "common/file.h"

#include "audio/mixer.h"
#include "audio/audiostream.h"

namespace Common {
class Path;
}

namespace Kom {

class KomEngine;

#define SOUND_HANDLES 16

typedef Audio::SoundHandle SoundHandle;

class SoundSample {
	friend class Sound;
public:
	SoundSample() { _stream = 0; _isCompressed = false; _isSpeech = false; _sampleData = 0; }
	~SoundSample() { unload(); }

	bool loadFile(const Common::Path &filename, bool isSpeech = false);
	void unload();
	bool isLoaded() { return _stream != 0; }
	int16 const *getSamples();
	uint getSampleCount();

private:
	Audio::SoundHandle _handle;
	Audio::RewindableAudioStream *_stream;
	bool _isSpeech;
	bool _isCompressed;
	int16 *_sampleData;
	int _sampleCount;

	void loadCompressed(Common::File &f, int offset, int size);
};

class Sound {
public:
	Sound(Audio::Mixer *mixer);
	~Sound();

	bool _musicEnabled;
	bool _sfxEnabled;

	bool playFileSFX(const Common::Path &filename, SoundHandle *handle);
	bool playFileSpeech(const Common::Path &filename, SoundHandle *handle);
	void playSampleSFX(SoundSample &sample, bool loop);
	void playSampleMusic(SoundSample &sample);
	void playSampleSpeech(SoundSample &sample);

	void stopHandle(SoundHandle handle);
	void stopSample(SoundSample &sample);
	void pauseSample(SoundSample &sample, bool paused);
	void setSampleVolume(SoundSample &sample, byte volume);
	uint16 getSampleElapsedTime(SoundSample &sample);
	bool isPlaying(SoundSample &sample) { return sample.isLoaded() ? _mixer->isSoundHandleActive(sample._handle) : false; }

private:

	bool playFile(const Common::Path &filename, SoundHandle *handle, Audio::Mixer::SoundType type, byte volume);
	void playSample(SoundSample &sample, bool loop, Audio::Mixer::SoundType type, byte volume);

	Audio::Mixer *_mixer;
};

} // end of namespace Kom

#endif
