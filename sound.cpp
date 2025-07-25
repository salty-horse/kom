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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "common/file.h"
#include "common/str.h"
#include "common/path.h"
#include "common/memstream.h"
#include "common/endian.h"
#include "common/ptr.h"
#include "common/stream.h"
#include "common/textconsole.h"
#include "common/types.h"
#include "common/debug.h"

#include "kom/sound.h"

#include "audio/mixer.h"
#include "audio/audiostream.h"
#include "audio/decoders/raw.h"
#include "audio/decoders/adpcm_intern.h"

using Common::File;
using Common::String;
using Common::Path;

namespace Kom {


// Custom implementation of DVI_ADPCMStream, due to flipped nibbles compared to DVI_ADPCM
class KOMADPCMStream : public Audio::Ima_ADPCMStream {
public:

	KOMADPCMStream(Common::SeekableReadStream *stream, DisposeAfterUse::Flag disposeAfterUse, uint32 size, int rate, int channels, uint32 blockAlign = 0)
		: Audio::Ima_ADPCMStream(stream, disposeAfterUse, size, rate, channels, blockAlign) { _decodedSampleCount = 0; }

	bool endOfData() const { return (_stream->eos() || _stream->pos() >= _endpos) && (_decodedSampleCount == 0); }

private:
	int readBuffer(int16 *buffer, const int numSamples) {
		int samples;
		byte data;

		for (samples = 0; samples < numSamples && !endOfData(); samples++) {
			if (_decodedSampleCount == 0) {
				data = _stream->readByte();

				// (data >> 0) and (data >> 4) are flipped, compared to the DVI_ADPCMStream implementation
				_decodedSamples[0] = decodeIMA((data >> 0) & 0x0f, 0);
				_decodedSamples[1] = decodeIMA((data >> 4) & 0x0f, _channels == 2 ? 1 : 0);
				_decodedSampleCount = 2;
			}

			// (1 - (count - 1)) ensures that _decodedSamples acts as a FIFO of depth 2
			buffer[samples] = _decodedSamples[1 - (_decodedSampleCount - 1)];
			_decodedSampleCount--;
		}

		return samples;
	}

	uint8 _decodedSampleCount;
	int16 _decodedSamples[2];
};


bool SoundSample::loadFile(const Path &filename, bool isSpeech) {
	File f;
	byte *data;
	int size = 0;

	unload();

	// Check for archive file
	String entry = filename.getLastComponent().toString();
	if ('1' <= entry[0] && entry[0] <= '9') {
		entry.toUppercase();

		// Construct new filename
		Path newName = filename.getParent().appendComponent("convall.dat");

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
			warning("Could not find %s in %s", entry.c_str(), newName.toString().c_str());
			delete[] contents;
			return false;
		}

		int offset = READ_LE_UINT32(contents + i*22 + 14);
		size = READ_LE_UINT32(contents + i*22 + 18);
		debug(1, "file %s, entry %s, offset %d, size %d", newName.toString().c_str(), entry.c_str(), offset, size);
		delete[] contents;

		data = (byte *)malloc(size);
		f.seek(offset);
		f.read(data, size);
		loadCompressed(f, offset, size);
		f.close();

	// Load file as-is
	} else if (f.open(filename)) {
		size = f.size();
		loadCompressed(f, 0, size);
		f.close();
	} else {
		warning("Could not find sound sample %s", filename.toString().c_str());
		return false;
	}

	_isSpeech = isSpeech;
	if (isSpeech) {
		int rawBufferSize = _isCompressed ? size * 2 : size;
		_sampleData = new int16[rawBufferSize];

		_sampleCount = _stream->readBuffer(_sampleData, rawBufferSize);
		_stream->rewind();
	}

	return (_stream != NULL);
}

void SoundSample::unload() {
	delete _stream;
	_stream = NULL;
	if (_isSpeech) {
		delete[] _sampleData;
	   _sampleData = NULL;
	}
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

		_stream = new KOMADPCMStream(
				new Common::MemoryReadStream(data, size, DisposeAfterUse::YES),
				DisposeAfterUse::YES,
				size,
				11025,
				1);

		_isCompressed = true;
	}
}

int16 const *SoundSample::getSamples() {
	assert(_isSpeech);
	return _sampleData;
}

uint SoundSample::getSampleCount() {
	assert(_isSpeech);
	return _sampleCount;
}

Sound::Sound(Audio::Mixer *mixer)
	: _mixer(mixer), _musicEnabled(true),
	_sfxEnabled(true) {
}

Sound::~Sound() {
}

bool Sound::playFileSFX(const Path &filename, SoundHandle *handle) {
	return playFile(filename, handle, Audio::Mixer::kSFXSoundType, 255);
}

bool Sound::playFileSpeech(const Path &filename, SoundHandle *handle) {
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

bool Sound::playFile(const Path &filename, SoundHandle *handle, Audio::Mixer::SoundType type, byte volume) {
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
	if (sample.isLoaded())
		stopHandle(sample._handle);
}

void Sound::pauseSample(SoundSample &sample, bool paused) {
	if (_mixer->isSoundHandleActive(sample._handle)) {
		_mixer->pauseHandle(sample._handle, paused);
	}
}

void Sound::setSampleVolume(SoundSample &sample, byte volume) {
	if (_mixer->isSoundHandleActive(sample._handle)) {
		_mixer->setChannelVolume(sample._handle, volume);
	}
}

uint16 Sound::getSampleElapsedTime(SoundSample &sample) {
	return _mixer->getSoundElapsedTime(sample._handle);
}

} // end of namespace Kom
