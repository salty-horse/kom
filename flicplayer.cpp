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

#include "common/stdafx.h"
#include "common/fs.h"
#include "common/stream.h"
#include "common/file.h"

#include "kom/flicplayer.h"

namespace Kom {

using Common::File;

FlicPlayer::FlicPlayer(FilesystemNode flicNode) : _paletteDirty(false), _offscreen(0) {
	File f;
	f.open(flicNode);
	_flicData = new uint8[f.size()];
	f.read(_flicData, f.size());
	_flicStream = new Common::MemoryReadStream(_flicData, f.size());
	f.close();

	_flicInfo.size = _flicStream->readUint16LE();
	_flicInfo.type = _flicStream->readUint16LE();
	_flicInfo.numFrames = _flicStream->readUint16LE();
	_flicInfo.width = _flicStream->readUint16LE();
	_flicInfo.height = _flicStream->readUint16LE();
	_flicStream->skip(41);
	_flicInfo.offsetFrame1 = _flicStream->readUint32LE();
	_flicInfo.offsetFrame2 = _flicStream->readUint32LE();

	// Check FLC magic number
	if (_flicInfo.type != 0xAF12) {
		error("FlicPlayer::FlicPlayer(): attempted to load non-FLC data (type = 0x%04X)\n", _flicInfo.type);
	}
		
	printf("%dx%d (%d frames)\n", _flicInfo.width, _flicInfo.height, _flicInfo.numFrames);

	_offscreen = new uint8[_flicInfo.width * _flicInfo.height];
	memset(_palette, 0, sizeof(_palette));

	// Seek to the first frame
	_flicStream->seek(_flicInfo.offsetFrame1);
}

FlicPlayer::~FlicPlayer() { 
	delete _flicStream;
	delete[] _flicData;
	delete[] _offscreen;
}

bool FlicPlayer::isValidChunk(uint16 type) {
	//Even though it may be a valid chunk type, only return true if we know how to deal with it
	switch (type) {
		case 4:      //COLOR_256
		case 7:      //DELTA_FLC (FLI_SS2)
		case 15:     //BYTE_RUN
		case 18:     //PSTAMP
		case 0xF1FA: //FRAME_TYPE
			return true;
		
		default:
			error("isValidChunk(0x%04X) is NOT a valid chunk\n", type);
			return false;
	}
}

ChunkHeader FlicPlayer::readChunkHeader() {
	ChunkHeader head;
	head.size = _flicStream->readUint32LE();
	head.type = _flicStream->readUint16LE();

	/* XXX: You'll want to read the rest of the chunk here as well! */
	
	return head;
}

FrameTypeChunkHeader FlicPlayer::readFrameTypeChunkHeader(ChunkHeader chunkHead) {
	FrameTypeChunkHeader head;
	
	head.header = chunkHead;
	head.numChunks = _flicStream->readUint16LE();
	head.delay = _flicStream->readUint16LE();
	head.reserved = _flicStream->readUint16LE();
	head.widthOverride = _flicStream->readUint16LE();
	head.heightOverride = _flicStream->readUint16LE();
	
	return head;
}

void FlicPlayer::decodeByteRun(uint8 *data) {
	uint8 *ptr = (uint8 *)_offscreen;
	while((ptr - _offscreen) < (_flicInfo.width * _flicInfo.height)) {
		uint8 chunks = *data++;
		while (chunks--) {
			int8 count = *data++;
			if (count > 0) {
				while (count--) {
					*ptr++ = *data;	
				}
				data++;
			} else {
				uint8 copyBytes = -count;
				memcpy(ptr, data, copyBytes);
				ptr += copyBytes;
				data += copyBytes;
			}
		}
	}
}

#define OP_PACKETCOUNT   0
#define OP_UNDEFINED     1
#define OP_LASTPIXEL     2
#define OP_LINESKIPCOUNT 3

void FlicPlayer::decodeDeltaFLC(uint8 *data) {
	uint16 linesInChunk = READ_LE_UINT16(data); data += 2;
	uint16 currentLine = 0;
	uint16 packetCount = 0;

	while (linesInChunk--) {
		uint16 opcode; 
		
		// First process all the opcodes.
		do {
			opcode = READ_LE_UINT16(data); data += 2;
			
			switch ((opcode >> 14) & 3) {
				case OP_PACKETCOUNT:
					packetCount = opcode;
					break;
				case OP_UNDEFINED:
					break;
				case OP_LASTPIXEL:
					*(uint8 *)(_offscreen + (currentLine * _flicInfo.width) + (_flicInfo.height - 1)) = (opcode & 0xFF);
					break;
				case OP_LINESKIPCOUNT:
					currentLine += -(int16)opcode;
					break;
			}
		} while (((opcode >> 14) & 3) != OP_PACKETCOUNT);

		uint16 column = 0;
		
		//Now interpret the RLE data
		while (packetCount--) {
			column += *data++;
			int8 rleCount = (int8)*data++;
			
			if (rleCount > 0) {
				memcpy((void *)(_offscreen + (currentLine * _flicInfo.width) + column), data, rleCount * 2);
				data += rleCount * 2;
				column += rleCount * 2;
			} else if (rleCount < 0) {
				uint16 dataWord = *(uint16 *)data; data += 2;
				for (int i = 0; i < -(int16)rleCount; ++i) { 
					*(uint16 *)(_offscreen + (currentLine * _flicInfo.width) + column + (i * 2)) = dataWord;
				}

				column += (-(int16)rleCount) * 2;
			} else { // End of cutscene ?
				return;
			}
		}

		currentLine++;
	}
}

#define COLOR_256  4
#define FLI_SS2    7
#define FLI_BRUN   15
#define PSTAMP     18
#define FRAME_TYPE 0xF1FA

bool FlicPlayer::decodeFrame() {
	FrameTypeChunkHeader frameHeader;
	ChunkHeader cHeader = readChunkHeader();
	do {
		switch(cHeader.type) {
			case COLOR_256:
				setPalette(_flicData + 6);
				_paletteDirty = true;
				break;
			case FLI_SS2:
				decodeDeltaFLC(_flicData + 6);
				break;
			case FLI_BRUN:
				decodeByteRun(_flicData + 6);
				break;
			case PSTAMP:	
				/* PSTAMP - skip for now */
				break;
			case FRAME_TYPE:
				frameHeader = readFrameTypeChunkHeader(cHeader);	
				_flicInfo.numFrames--; // TODO - ori
				//printf("Frames Remaining: %d\n", _flicInfo.numFrames);
				break;
			default:
				error("FlicPlayer::decodeFrame(): unknown chunk type (type = 0x%02X)\n", cHeader.type);
				break;
		 }

		if (cHeader.type != FRAME_TYPE)	
			_flicData += cHeader.size;
			
		cHeader = readChunkHeader();

	} while(isValidChunk(cHeader.type) && cHeader.type != FRAME_TYPE);
	
	return isValidChunk(cHeader.type);
	
}

void FlicPlayer::setPalette(uint8 *mem) {
	uint16 numPackets = READ_LE_UINT16(mem); mem += 2;

	if (0 == READ_LE_UINT16(mem)) { //special case
		mem += 2;
		for (int i = 0; i < 256; ++i)
			for (int j = 0; j < 3; ++j)
				_palette[i * 3 + j] = (mem[i * 3 + j]);
	} else {
		uint8 palPos = 0;

		while (numPackets--) {
			palPos += *mem++;
			uint8 change = *mem++;

			for (int i = 0; i < change; ++i)
				for (int j = 0; j < 3; ++j)
					_palette[(palPos + i) * 3 + j] = (mem[i * 3 + j]);

			palPos += change;
			mem += (change * 3);
		}
	}
}

} // End of namespace Kom