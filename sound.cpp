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

namespace Kom {


// Custom implementation of DVI_ADPCMStream, due to flipped nibbles compared to DVI_ADPCM
// It also decodes the sample into a raw buffer so it could be queried later.
class KOMADPCMStream : public Audio::Ima_ADPCMStream {
public:

	KOMADPCMStream(Common::SeekableReadStream *stream, DisposeAfterUse::Flag disposeAfterUse, uint32 size, int rate, int channels, uint32 blockAlign = 0)
		: Audio::Ima_ADPCMStream(stream, disposeAfterUse, size, rate, channels, blockAlign) {

		_decodedSampleCount = 0;
		int rawBufferSize = size * 2;
		_buffer = new int16[rawBufferSize];

		_sampleCount = readBuffer(_buffer, size * 2);
		_stream->seek(0);
	}

	~KOMADPCMStream() {
		delete[] _buffer;
	}

	bool endOfData() const { return (_stream->eos() || _stream->pos() >= _endpos); }

	int16 *getSamples() {
		return _buffer;
	}

	int getSampleCount() {
		return _sampleCount;
	}

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
	int16 *_buffer;
	int _sampleCount;
};


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
			warning("Could not find %s in %s", entry.c_str(), newName.c_str());
			delete[] contents;
			return false;
		}

		int offset = READ_LE_UINT32(contents + i*22 + 14);
		int size = READ_LE_UINT32(contents + i*22 + 18);
		debug(1, "file %s, entry %s, offset %d, size %d", newName.c_str(), entry.c_str(), offset, size);
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

		_stream = new KOMADPCMStream(
				new Common::MemoryReadStream(data, size, DisposeAfterUse::YES),
				DisposeAfterUse::YES,
				size,
				11025,
				1);

		_isCompressed = true;
		_sampleData = ((KOMADPCMStream *)_stream)->getSamples();
		_sampleCount = ((KOMADPCMStream *)_stream)->getSampleCount();
	}
}

int16 const *SoundSample::getSamples() {
	assert(_isCompressed);
	return _sampleData;
}

uint SoundSample::getSampleCount() {
	assert(_isCompressed);
	return _sampleCount;
}

Sound::Sound(Audio::Mixer *mixer)
	: _mixer(mixer), _musicEnabled(true),
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
