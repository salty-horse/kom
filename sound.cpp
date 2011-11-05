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

#include "common/file.h"
#include "common/str.h"
#include "common/memstream.h"

#include "kom/kom.h"
#include "kom/sound.h"

#include "audio/mixer.h"
#include "audio/audiostream.h"
#include "audio/decoders/raw.h"
#include "audio/decoders/adpcm.h"

using Common::File;
using Common::String;

namespace Kom {

bool SoundSample::loadFile(Common::String filename) {
	File f;
	byte *data;

	unload();

	// Check for archive file
	String entry = lastPathComponent(filename, '/');
	if ('1' <= entry[0] && entry[0] <= '9') {
		entry.toUppercase();

		// Construct new filename
		String newName = filename;
		while (newName.lastChar() != '/')
			newName.deleteLastChar();
		newName += "convall.dat";

		if (!f.open(newName))
			return false;

		int16 count = f.readSint16LE();

		char *contents = new char[22 * count];
		f.read(contents, 22 * count);

		bool found = false;
		int i;
		for (i = 0; i < count; i++) {
			if (entry == (char*)contents + i*22) {
				found = true;
				break;
			}
		}

		if (!found) {
			warning("Could not find %s in %s", newName.c_str(), entry.c_str());
			delete[] contents;
			return false;
		}

		int offset = READ_LE_UINT32(contents + i*22 + 14);
		int size = READ_LE_UINT32(contents + i*22 + 18);
		warning("file %s, entry %s, offset %d, size %d", newName.c_str(), entry.c_str(), offset, size);
		delete[] contents;

		data = (byte *)malloc(size);
		f.seek(offset);
		f.read(data, size);
		loadCompressed(f, offset, size);
		f.close();

		return true;

	// Load file as-is
	} else if (f.open(filename)) {
		loadCompressed(f, 0, f.size());
		f.close();

		return true;
	}

	return false;
}

void SoundSample::unload() {
	delete _stream;
	_stream = 0;
}

void SoundSample::loadCompressed(Common::File &f, int offset, int size) {

	char header[9] = {0};

	f.seek(offset + 16);
	f.read(header, sizeof(header) - 1);

	// Not compressed
	if (strcmp(header, "HMIADPCM") != 0) {
		byte *data = (byte *)malloc(size);
		f.seek(offset);
		f.read(data, size);
		_stream = Audio::makeRawStream(data, size,
				11025, Audio::FLAG_UNSIGNED);

	// Compressed
	} else {
		size -= 32;
		byte *data = (byte *)malloc(size);
		f.seek(offset + 32);
		f.read(data, size);

		_stream = Audio::makeADPCMStream(
				new Common::MemoryReadStream(data, size, DisposeAfterUse::YES),
				DisposeAfterUse::YES,
				size,
				Audio::kADPCMMSIma,
				11025,
				1,
				0x2000);
	}
}

Sound::Sound(KomEngine *vm, Audio::Mixer *mixer)
	: _vm(vm), _mixer(mixer), _musicEnabled(true),
	_sfxEnabled(true) {
}

Sound::~Sound() {
}

bool Sound::playFileSFX(Common::String filename, SoundHandle *handle) {
	return playFile(filename, handle, Audio::Mixer::kSFXSoundType, 255);
}

bool Sound::playFileSpeech(Common::String filename, SoundHandle *handle) {
	return playFile(filename, handle, Audio::Mixer::kSpeechSoundType, 255);
}

void Sound::playSampleSFX(SoundSample &sample, bool loop) {
	playSample(sample, loop, Audio::Mixer::kSFXSoundType, 255);
}

void Sound::playSampleMusic(SoundSample &sample) {
	playSample(sample, true, Audio::Mixer::kMusicSoundType, 100);
}

void Sound::playSampleSpeech(SoundSample &sample) {
	playSample(sample, false, Audio::Mixer::kSpeechSoundType, 255);
}

bool Sound::playFile(Common::String filename, SoundHandle *handle, Audio::Mixer::SoundType type, byte volume) {
	File *f = new File();
	Audio::SeekableAudioStream *stream;

	if (!f->open(filename))
		return false;

	stream = Audio::makeRawStream(f, 11025, Audio::FLAG_UNSIGNED);

	_mixer->playStream(type, handle, stream, -1, volume);

	return true;
}

void Sound::playSample(SoundSample &sample, bool loop, Audio::Mixer::SoundType type, byte volume) {

	if (!sample.isLoaded())
		return;

	sample._stream->rewind();

	if (_mixer->isSoundHandleActive(sample._handle)) {
		_mixer->stopHandle(sample._handle);
	}

	if (loop) {
		Audio::AudioStream *stream = new Audio::LoopingAudioStream(sample._stream, 0, DisposeAfterUse::NO);
		_mixer->playStream(type, &(sample._handle), stream, -1, volume, 0, DisposeAfterUse::YES);
	} else {
		_mixer->playStream(type, &(sample._handle), sample._stream, -1, volume, 0, DisposeAfterUse::NO);
	}
}

void Sound::stopHandle(SoundHandle handle) {
	if (_mixer->isSoundHandleActive(handle)) {
		_mixer->stopHandle(handle);
	}
}

void Sound::stopSample(SoundSample &sample) {
	stopHandle(sample._handle);
}

void Sound::pauseSample(SoundSample &sample, bool paused) {
	if (_mixer->isSoundHandleActive(sample._handle)) {
		_mixer->pauseHandle(sample._handle, paused);
	}
}

} // end of namespace Kom
