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

#include <stdio.h>
#include <memory.h>

#include "common/endian.h"
#include "common/file.h"
#include "common/str.h"
#include "common/textconsole.h"

#include "graphics/palette.h"

#include "kom/kom.h"
#include "kom/character.h"
#include "kom/conv.h"
#include "kom/database.h"
#include "kom/game.h"
#include "kom/input.h"
#include "kom/panel.h"
#include "kom/screen.h"

using Common::File;

namespace Kom {

Face::Face(const Common::String &filename, byte *lipBuffer, byte *zoomSurface) :
   _sentenceStatus(0) {

}

Face::~Face() {
	delete[] _faceLinks;
	delete[] _linkFrameList;
	for (int i = 0; i < _stateCount; ++i) {
		delete[] _states[i].loops;
	}
	delete[] _states;
}

void Face::assignLinks(const Common::String &filename) {
	File f;
	f.open(filename);

	_stateCount = f.readSint16BE();
	_states = new State[_stateCount];

	for (int i = 0; i < _stateCount; ++i) {
		_states[i].id = i;
		int16 loopCount = f.readSint16BE();
		if (loopCount > 0)
			_states[i].loops = new Loop[loopCount];
		else
			_states[i].loops = NULL;
		_states[i].loopCount = loopCount;

		for (int j = 0; j < loopCount; ++j) {
			Loop *loop = &_states[i].loops[j];
			loop->startFrame = f.readSint16BE();
			loop->endFrame = f.readSint16BE();
			loop->loopToDepth = 1;
			loop->currDepth = 1;
			loop->currFrame = loop->startFrame;
			loop->maxDepth = (loop->endFrame - loop->startFrame) * 2 + 1;
		}
	}

	_faceLinks = new Link[_stateCount * _stateCount];
	_linkFrameList = new int16[f.readSint16BE()];
	int16 *currFrame = _linkFrameList;

	Link *link = _faceLinks;
	for (int i = 0; i < _stateCount; ++i) {
		for (int j = 0; j < _stateCount; ++j) {
			link->endState = &_states[j];
			link->frameList = currFrame;

			int data;
			do {
				data = f.readSint16BE();
				*currFrame = data;
				currFrame++;
			} while (data >= 0);
			++link;
		}
	}

	_currState = _states;
	_currState->currLoop = _currState->loops;
	_status = 0;

	f.close();
}

Lips::Lips(KomEngine *vm) : _vm(vm) {
	_frontBuffer = NULL;
	_playerFace = NULL;
	_otherFace = NULL;
	_otherZoomSurface = NULL;
}

Lips::~Lips() {
	delete _multiColorSet;
	delete _narrColorSet;
	delete[] _otherZoomSurface;
	delete[] _frontBuffer;
	delete[] _otherFront;
	delete _playerFace;
	delete _otherFace;
}

void Lips::init(Common::String playerCodename, Common::String otherCodename,
				int16 char1ZoomX, int16 char1ZoomY,
                int16 char2ZoomX, int16 char2ZoomY,
				int16 convNum) {
	warning("Lips::init(%s, %s, %d, %d, %d, %d, %d)", playerCodename.c_str(), otherCodename.c_str(), char1ZoomX, char1ZoomY, char2ZoomX, char2ZoomY, convNum);
	playerCodename.toLowercase();
	otherCodename.toLowercase();

	_playerEmotion = 9999;
	_otherEmotion = 9999;
	// TODO dword_mouseY_9BA9E = 0;
	_lastCharacter = 0;
	_narratorConv = false;
	_narrColorSet = NULL;
	_multiColorSet = NULL;
	_colorSetType = 0;
	_multiFullPalette = false;
	// TODO hiword(isfading) = gfxstorepalette();

	if (otherCodename == "s28evs") {
		_multiColorSet = new ColorSet("kom/conv/gribnick.cl");
		_colorSetType = 1;
		_multiFullPalette = true;
	} else if (otherCodename == "m19tre") {
		_multiColorSet = new ColorSet("kom/conv/rwraith.cl");
		_colorSetType = 2;
	} else if (otherCodename == "m8trl1") {
		_multiColorSet = new ColorSet("kom/conv/chrisl.cl");
		_colorSetType = 3;
		_multiFullPalette = true;
	} else if (otherCodename == "m4cnrd") {
		_multiColorSet = new ColorSet("kom/conv/conrad2.cl");
		_colorSetType = 4;
		_multiFullPalette = true;
	}

	if (convNum == 0 || otherCodename == "s99nar") {
		_playerActive = false;
	} else {
		_playerActive = true;

		_narrColorSet = new ColorSet("kom/conv/nartalk.cl");
		byte *zoomSurface = _vm->screen()->createZoomBlur(char1ZoomX, char1ZoomY);
		_frontBuffer = new byte[SCREEN_W * (SCREEN_H - PANEL_H)];
		Common::String filenamePrefix = Common::String("kom/conv/") + playerCodename;
		Common::String flcFilename = filenamePrefix + ".flc";
		Common::String clFilename = filenamePrefix + ".cl";
		Common::String lnkFilename = filenamePrefix + ".lnk";
		_playerFace = new Face(flcFilename, _frontBuffer, zoomSurface);
		_playerColorSet = new ColorSet(clFilename.c_str());
		_playerFace->assignLinks(lnkFilename);

	//	p3 = 0;
	//	for ( i = 0; (signed int)(unsigned __int16)i < 3; ++i )
	//	{
	//		for ( j = 0; (signed int)(unsigned __int16)j < 24; ++j )
	//		{
	//			p3 = 100 * (unsigned __int16)i;
	//			(&menuoptions[(unsigned __int16)j])[p3] = (char *)memmalloc(96);
	//		}
	//		optiontop[50 * (unsigned __int16)i] = 0;
	//		optionbottom[50 * (unsigned __int16)i] = 0;
	//	}
	}

	if (otherCodename == "s99nar") {
		_narratorConv = true;
	}
	//convunused = 1;
	if (!_narratorConv) {
		_otherZoomSurface = _vm->screen()->createZoomBlur(char2ZoomX, char2ZoomY);
	}
	_otherFront = new byte[SCREEN_W * ROOM_H];

	Common::String fnamePrefix;
	if ( _narratorConv) {
		fnamePrefix = Common::String::format("kom/conv/%s/%s%02d", otherCodename.c_str(), otherCodename.c_str(), convNum);
	} else {
		fnamePrefix = Common::String::format("kom/conv/%s/%s", otherCodename.c_str(), otherCodename.c_str());
	}

	Common::String fname = fnamePrefix + ".flc";

	if (otherCodename == "m13rog") {
		_isBalrog = true;
		// TODO: balrog
		//	convflicwindow = flicloadwindow(tmpfilename);
		//	if ( !convflicwindow )
		//		sub_49c40((int)"lips > lips_initconv", (int)tmpfilename);
		//	v35 = 0;
		//	flicplaywindow(convflicwindow, 0, 0, nonplayerlipfrontbuf, blurzoomsurface2);
	} else {
		_isBalrog = false;
		_otherFace = new Face(fname, _otherFront, _narratorConv ? NULL : _otherZoomSurface);
	}

	fname = fnamePrefix + ".cl";
	_otherColorSet = new ColorSet(fname.c_str());
	_fullPalette = _otherColorSet->size > 128;

	fname = fnamePrefix + ".lnk";
	if (!_isBalrog) {
		_otherFace->assignLinks(fname);
	}
	if (_playerActive) {
		_convDir = Common::String::format("kom/conv/%s/conv%d", otherCodename.c_str(), convNum);
	} else {
		_convDir = Common::String::format("kom/conv/%s/response", otherCodename.c_str());
	}
	_vm->screen()->showMouseCursor(false);
	_talkerSample = NULL;
	_textSurface = new byte[(PANEL_H + 8) * SCREEN_W];
	memset(_textSurface, 0, (PANEL_H + 8) * SCREEN_W);
	_exchangeColor = 1;
	_smackerPlayed = 0;
}

void Lips::doTalk(uint16 charId, int16 emotion, const char *filename, const char *text, int pitch) {
	Common::String sampleFilename = Common::String::format("%s/%s.raw", _convDir.c_str(), filename);

	bool pitchSet = false;
	int flicFrame = 0;
	if (text == NULL) {
		_exchangeString = NULL;
	} else {
		_exchangeString = new char[strlen(text) + 4];
		strcpy(_exchangeString, text);
	}

	_scrollTimer = 0;
	_scrollPos = 0;
	_exchangeState = 0;
	_exchangeToken = strtok(_exchangeString, " ");
	_exchangeX = 0;
	_wroteText = false;
	_exchangeDisplay = 1;

	// TODO: check 'show subtitles' setting
	_exchangeColor = charId;

	if (_exchangeString != NULL) {
		_exchangeDisplay = 0;
		_exchangeScrollSpeed = 256;

		for (int i = 0; i < 9; i++) {
			convDialogue();
		}
		_exchangeDisplay = 1;
	}

	_exchangeScrollSpeed = 48;
	_scrollTimer = 0;

	// TODO: handle balrog
	// if !balrog {
		if (charId == 0) {
			// TODO - around loc_4260A
			loadSentence(_playerFace, sampleFilename);
			if (_lastCharacter != 0) {
				_vm->screen()->clearRoom();
				_vm->screen()->gfxUpdate();
			}
			if (_fullPalette || emotion != 3) {
				// TODO: restore palette
			}
			if (emotion == 3) {
				// use narrator color set
			} else {
				// use player color set
			}
			// TODO palette editing - loc_4274D

			if (_playerEmotion != emotion) {
				changeState(_playerFace, emotion);
				_playerEmotion = emotion;
			}
			rewindPlaySentence(_playerFace);
			// TODO - loc_427AD

		} else {
			// TODO - loc_427D8
			loadSentence(_otherFace, sampleFilename);
			if (_lastCharacter == 0) {
				_vm->screen()->clearRoom();
				_vm->screen()->gfxUpdate();
				// TODO: restore palette
			}
			if ((_colorSetType == 1 && emotion == 2) ||
			    (_colorSetType == 2 && emotion == 2) ||
			    (_colorSetType == 3 && emotion == 2) ||
			    (_colorSetType == 4 && emotion == 3)) {
				// loc_42883
				// use color set (multi)
			} else {
				// loc_42891
				// use color set (other)
			}

			if (_lastCharacter != 0) {
				// loc_4293D
			}

			// loc_42978
			if (_otherEmotion != emotion) {
				changeState(_otherFace, emotion);
				_otherEmotion = emotion;
			}
			rewindPlaySentence(_otherFace);
			// TODO - loc_4299D
		}

		// loc_429C3
		_lastCharacter = charId;
		// TODO: change a few colors in the palette
		// [eax+3] = 3Fh
		// [eax+4] = 3Fh
		// [eax+5] = 3Fh
		// [eax+6] = 28h
		// [eax+7] = 20h
		// [eax+8] = 20h
		// palette dirty = true

		while (1) {
			updateSentence(charId == 0 ? _playerFace: _otherFace);

			if (_talkerSample != NULL && _vm->sound()->isPlaying(*_talkerSample) && !pitchSet) {
				// TODO: set pitch
				pitchSet = true;
			}

			if (_exchangeString != NULL)
				convDialogue();

			// loc_42A93
			_vm->screen()->gfxUpdate();
			// TODO
			// if escape, space, right click
			//     loc_42AC3;
			/*
			Face *face;
			if (charId == 0) {
				face = _playerFace;
			} else {
				face = _otherFace;
			}

			// TODO check NULL face?
			if (face->_sentenceStatus != 0) {
				if (face->_sentenceStatus == 1) {
					_vm->sound()->stopSample(*_talkerSample);
				}
				loopTo(face, 1);
				face->_sentenceStatus = 4;
			}

			while (face->_sentenceStatus != 3) {
				updateSentence(face);
			}
			*/

			if (charId == 0 && _playerFace->_sentenceStatus == 3)
				break;

			if (charId != 0 && _otherFace->_sentenceStatus == 3)
				break;
		}

		// TODO: check 'show subtitles' setting

		if (_exchangeString != NULL) {
			_exchangeDisplay = 0;

			while (_exchangeState != 2)
				convDialogue();

			// Scroll up
			memmove(_textSurface, _textSurface + 2 * SCREEN_W, 38 * SCREEN_W);
			memset(_textSurface + 38 * SCREEN_W, 0, 2 * SCREEN_W);

			_exchangeDisplay = 1;
		}

		if (_exchangeString != NULL)
			delete[] _exchangeString;

		_smackerPlayed = 0;
	// } else { // balrog
	// }
}

void Lips::convDialogue() {
	// TODO: check 'show subtitles' setting
	if (_exchangeString == NULL) {
		_vm->screen()->clearPanel();
		return;
	}

	if (_exchangeState != 2) {
		if (!_wroteText) {
			_wroteText = true;

			bool eol = false;
			while (!eol) {
				uint16 width = _vm->screen()->calcWordWidth(_exchangeToken);
				if (width + _exchangeX >= SCREEN_W - 4) {
					eol = true;
				} else {
					_vm->screen()->writeText(_textSurface, _exchangeToken, PANEL_H, _exchangeX, _exchangeColor, /*isEmbossed=*/false);
					_exchangeX += width;
					_exchangeToken = strtok(NULL, " ");
				}

				if (_exchangeToken == NULL) {
					_exchangeState++;
					eol = true;
				}
			}
		} else {
			_scrollTimer += _exchangeScrollSpeed;
			// TODO - control scroll speed with mouse
			if (_scrollTimer >= 256) {
				_scrollTimer -= 256;
				_scrollPos = (_scrollPos + 1) % 8;
				if (_scrollPos == 0) {
					memmove(_textSurface, _textSurface + 8 * SCREEN_W, PANEL_H * SCREEN_W);
					memset(_textSurface + PANEL_H * SCREEN_W, 0, 8 * SCREEN_W);

					if (_exchangeState != 0) {
						_exchangeState = 2;
					} else {
						_wroteText = false;
						// TODO
						// if (show subtitles mode 2) {
						//   _exchangeX = 0;
						// } else {
						_exchangeX = 4;
						// }
					}
				}
			}
		}
	}

	_vm->screen()->drawPanel(_textSurface + _scrollPos * SCREEN_W);
}

void Lips::loadSentence(Face *face, const Common::String &filename) {
	if (face->_sentenceStatus != 0) {
		face->_sample.unload();
	}
	face->_sample.loadFile(filename);
	face->_sentenceStatus = 3;
}

void Lips::updateSentence(Face *face) {
	if (face == NULL) return;

	if (face->_sentenceStatus == 1) {
		//int average = getAverage;
		// TODO
		if (_talkerSample == NULL || !_vm->sound()->isPlaying(*_talkerSample)) {
			// TODO stuff
			loopTo(face, 1);
			face->_sentenceStatus = 4;
		}
	}

	update(face);
}

void Lips::update(Face *face) {
	if (face == NULL) return;
	switch (face->_status) {
	case 1: {
		Loop *loop = face->_currState->currLoop;
		if (loop->currDepth != loop->loopToDepth) {
			loop->currDepth++;
			loop->currFrame++;
		} else {
			face->_status &= ~1;
			face->_status |= 2;
			loop->currFrame = loop->startFrame + loop->endFrame - loop->currFrame;
		}
		// TODO flicSetWindowFrame
		break;
	}
	case 2: {
		Loop *loop = face->_currState->currLoop;
		// TODO flicSetWindowFrame
		if (loop->currDepth != 1) {
			loop->currDepth++;
			loop->currFrame++;
		} else {
			face->_status = 0;
			if (face->_sentenceStatus == 2) {
				face->_sentenceStatus = 1;
				_talkerSample = &face->_sample;
				_vm->sound()->playSampleSpeech(*_talkerSample);
			} else if (face->_sentenceStatus == 4) {
				face->_sentenceStatus = 3;
			}
		}
		break;
	}
	case 4:
		if (*face->_playLink->currFrame < 0) {
			// TODO
			face->_currState = face->_playLink->endState;
			face->_status &= ~4;
			if (face->_sentenceStatus == 2) {
				face->_sentenceStatus = 1;
				_talkerSample = &face->_sample;
				_vm->sound()->playSampleSpeech(*_talkerSample);
			}
		} else {
			// TODO flicSetWindowFrame
			face->_playLink->currFrame++;
		}
		break;
	default:
		// loc_41181
		if (face->_sentenceStatus == 4) {
			face->_sentenceStatus = 3;
		}
		// TODO flicSetWindowFrame
		if (face->_sentenceStatus == 2) {
			face->_sentenceStatus = 1;
			_talkerSample = &face->_sample;
			_vm->sound()->playSampleSpeech(*_talkerSample);
		}

		break;
	}

	// TODO: flic update window
}

void Lips::loopTo(Face *face, int depth) {
	if (face == NULL) return;
	if ((face->_status & 4) == 0) return;
	if (face->_currState->loopCount == 0) return;

	if (depth < 1)
		depth = 1;

	Loop *loop = face->_currState->currLoop;

	switch (face->_status) {
	case 1:
		if (depth < loop->currDepth) {
			loop->currFrame = loop->startFrame + loop->endFrame - loop->currFrame;
			face->_status &= ~1;
			face->_status |= 2;
		}
		loop->loopToDepth = depth;
		break;
	case 2:
		if (depth > loop->currDepth) {
			loop->currFrame = loop->startFrame + loop->endFrame - loop->currFrame;
			face->_status &= ~2;
			face->_status |= 1;
		}
		loop->loopToDepth = depth;
		break;
	default:
		loop->currFrame = loop->startFrame;
		loop->currDepth = 1;
		loop->loopToDepth = depth;
		face->_status |= 1;
		break;
	}
}

void Lips::changeState(Face *face, int emotion) {
	if (face == NULL) return;
	if (face->_currState->id == emotion) return;
	if (face->_status & 4) return;
	if (face->_status & 3) return;
	if (emotion >= face->_stateCount) return;

	face->_playLink = &face->_faceLinks[face->_currState->id * face->_stateCount + emotion];
	face->_playLink->currFrame = face->_playLink->frameList;

	Loop *loops = face->_playLink->endState->loops;
	face->_playLink->endState->currLoop = loops;
	loops->currFrame = loops->startFrame;
	loops->loopToDepth = 1;
	loops->currDepth = 1;
	face->_status = 4;
}

void Lips::rewindPlaySentence(Face *face) {
	if (face == NULL) return;
	if (face->_sentenceStatus == 1)
		_vm->sound()->stopSample(*_talkerSample);
	if (face->_currState->loopCount == 0)
		changeState(face, (face->_currState->id + 1) % face->_stateCount);
	face->_sentenceStatus = 2;
}

Talk::Talk(KomEngine *vm) : Lips(vm) {
}

void Talk::init(uint16 charId, int16 convNum) {
	int playerHeight, charHeight;
	char playerCodename[100];
	int16 playerX, playerY;
	int16 charX, charY;
	Character *playerChar = _vm->database()->getChar(0);
	Character *otherChar = _vm->database()->getChar(charId);

	_vm->game()->cb()->talkInitialized = false;
	_talkingChar = charId;
	_vm->game()->stopNarrator();
	_vm->game()->stopGreeting();

	sprintf(playerCodename, "%s%d",
		playerChar->_name,
		playerChar->_xtend);

	// TODO: Handle pitch based on spell effect
	playerHeight = 0x3C00;
	charHeight = 0x3C00;

	playerX = playerChar->_screenX;
	playerY = playerChar->_screenY;
	charX = otherChar->_screenX;
	charY = otherChar->_screenY;

	switch (charId) {
	case 18:
		charX -= 50;
		charY -= 20;
		break;
	case 19:
		charY += 20;
		break;
	case 46:
		if (_vm->database()->getBox(otherChar->_lastLocation, otherChar->_lastBox)->attrib == 8) {
			charX = 324;
			charY = 160;
		}
		break;
	case 57:
		playerX += 40;
		break;
	case 62:
		charX = 500;
		charY = 290;
		break;
	case 66:
		charX -= 40;
		charY = 400;
		break;
	case 83:
		charX -= 60;
		playerX -= 60;
		break;
	case 84:
		charX += 30;
		playerX += 40;
		break;
	// Spiders
	case 85:
	case 86:
	case 87:
		charY = 240;
		break;
	}

	playerX = playerX * 2 + 25;
	playerY = playerY * 2 - (playerHeight / (playerChar->_start5 + 108));
	charX = charX * 2 + 25;
	charY = charY * 2 - (charHeight / (otherChar->_start5 + 108));

	//charHeight = ((signed __int16)v23 >> 1) - (*(_QWORD *)p2 >> 32) / (signed __int64)*(_DWORD *)(v21 + 108);

	_vm->panel()->setActionDesc("");
	_vm->panel()->setHotspotDesc("");
	_vm->panel()->showLoading(true);
	Lips::init(playerCodename, otherChar->_name, 
		playerX, playerY,
		charX, charY,
		convNum);
	_vm->panel()->showLoading(false);
}

Talk::~Talk() {
	_vm->game()->cb()->talkInitialized = false;
}

void Talk::doTalk(int charId, int emotion, const char *sampleFile, const char *sentence) {
	warning("TODO talk: %d - %s - %s", charId, sampleFile, sentence);

	Character *chr;
	if (charId != 0) {
		chr = _vm->database()->getChar(_talkingChar);
	} else {
		chr = _vm->database()->getChar(0);
	}

	int pitch;
	// TODO set pitch
	switch (chr->_spellMode) {
	case 2:
		pitch = 0x6000;
		break;
	case 3:
		pitch = 0xc000;
		break;
	default:
		pitch = 0x8000;
		break;
	}

	if (charId == 0 && emotion >= 3) {
		pitch = 0xc000;
	}

	int16 id;
	if (charId == 0)
		id = 0;
	else
		id = 1;

	Lips::doTalk(id, emotion, sampleFile, sentence, pitch);
}

Conv::Conv(KomEngine *vm, uint16 charId)
	: _vm(vm), _convs(0), _text(0), _talk(0) {
	_charId = charId;
	_codename = _vm->database()->getChar(charId)->_name;
	_convData = _vm->database()->getConvData();
	_convEntry = _vm->database()->getConvIndex(_codename);

	// Backup the palette
	_vm->_system->getPaletteManager()->grabPalette(_backupPalette, 0, 256);

	_vm->screen()->showMouseCursor(false);
}

Conv::~Conv() {
	delete[] _convs;
	delete[] _text;

	delete _talk;

	// Restore the palette
	_vm->_system->getPaletteManager()->setPalette(_backupPalette, 0, 256);

	_vm->screen()->showMouseCursor(true);
}

void Conv::initConvs(uint32 offset) {
	int16 charId, cmd;
	int32 optNum;
	Common::String lineBuffer;

	_convData->seek(offset);

	do {
		lineBuffer = _convData->readLine();
	} while (lineBuffer.empty());
	sscanf(lineBuffer.c_str(), "%hd", &charId);

	// Read conversations
	while (charId != -2) {
		Conversation convObject;
		convObject.charId = charId;

		sscanf(lineBuffer.c_str(), "%*d %hd", &(convObject.convNum));

		// Read options
		do {
			lineBuffer = _convData->readLine();
		} while (lineBuffer.empty());
		sscanf(lineBuffer.c_str(), "%d", &optNum);

		while (optNum != -1) {
			Option optObject;
			optObject.optNum = optNum;

			sscanf(lineBuffer.c_str(), "%*u %u", &(optObject.textOffset));

			do {
				lineBuffer = _convData->readLine();
			} while (lineBuffer.empty());
			sscanf(lineBuffer.c_str(), "%hd", &cmd);

			// Read statements
			while (cmd != -1) {
				Statement statObject;

				statObject.command = cmd;

				switch (cmd) {

				case 306:
					sscanf(lineBuffer.c_str(), " %*d %hd %hd %u %s",
						&(statObject.charId),
						&(statObject.emotion),
						&(statObject.textOffset),
						statObject.filename);
					break;

				case 307:
					break;

				case 309:
					sscanf(lineBuffer.c_str(), " %*d %u",
						&(statObject.optNum));
					break;

				case 326:
					sscanf(lineBuffer.c_str(), " %*d %hu %hd",
						&(statObject.address),
						&(statObject.val));
					break;

				case 419:
					sscanf(lineBuffer.c_str(), " %*d %hd",
						&(statObject.val));
					break;

				case 467:
				case 469:
					lineBuffer = _convData->readLine();
					strcpy(statObject.filename, lineBuffer.c_str());
					break;

				default:
					error("Unknown command %hd", cmd);
					break;
				}

				optObject.statements.push_back(statObject);

				do {
					lineBuffer = _convData->readLine();
				} while (lineBuffer.empty());
				sscanf(lineBuffer.c_str(), "%hd", &cmd);
			}

			convObject.options.push_back(optObject);

			do {
				lineBuffer = _convData->readLine();
			} while (lineBuffer.empty());
			sscanf(lineBuffer.c_str(), "%d", &optNum);
		}

		_conversations.push_back(convObject);

		do {
			lineBuffer = _convData->readLine();
		} while (lineBuffer.empty());
		sscanf(lineBuffer.c_str(), "%hd", &charId);
	}
}

void Conv::initText(uint32 offset) {
	int num;
	Common::String lineBuffer;

	_convData->seek(offset);
	lineBuffer = _convData->readLine();
	sscanf(lineBuffer.c_str(), "%d", &num);
	_text = new char[num];
	_convData->read(_text, num);
}

bool Conv::doTalk(int16 convNum, int32 optNum) {

	initConvs(READ_LE_UINT32(_convEntry + 20));
	initText(READ_LE_UINT32(_convEntry + 8));

	// Find conversation
	for (Common::List<Conversation>::iterator c = _conversations.begin(); c != _conversations.end(); c++) {
		if (_charId == c->charId && c->convNum == convNum) {
			_talk = new Talk(_vm);
			_talk->init(_charId, convNum);
			bool result = doOptions(&(*c), optNum);
			delete _talk;
			_talk = 0;
			return result;
		}
	}

	return false;
}

void Conv::doResponse(int responseNum) {
	int count;
	int num = -1;
	int offset = 0;
	int emotion;
	char convName[10];
	Common::String lineBuffer;

	initText(READ_LE_UINT32(_convEntry + 12));

	_convData->seek(READ_LE_UINT32(_convEntry + 16));
	lineBuffer = _convData->readLine();
	sscanf(lineBuffer.c_str(), "%d", &count);

	for (int i = 0; i < count && num != responseNum; i++) {
		do {
			lineBuffer = _convData->readLine();
		} while (lineBuffer.empty());
		sscanf(lineBuffer.c_str(), "%d %d %d", &num, &emotion, &offset);
	}

	if (num == responseNum) {
		sprintf(convName, "%d", responseNum);
		_talk = new Talk(_vm);
		_talk->init(_charId, 0);
		_talk->doTalk(_charId, emotion, convName, _text + offset);
	} else {
		error("Could not find response %d of %s", responseNum, _codename);
	}
}

bool Conv::doOptions(Conversation *conv, int32 optNum) {
	while (1) {
		assert(conv);

		assert(optNum != 0);

		for (int i = 0; i < 3; i++) {
			_options[i].offset = 0;
			_options[i].statements = 0;

			// Find option
			for (Common::List<Option>::iterator o = conv->options.begin(); o != conv->options.end(); o++) {
				if (optNum + i == o->optNum) {
					_options[i].offset = _text + o->textOffset;
					_options[i].statements = &(o->statements);
					break;
				}
			}
		}

		int sel = -1;

		while (sel == -1) {
			sel = showOptions();

			if (_vm->shouldQuit()) {
				return false;
			}

			if (sel != -1)
				break;

			if (optNum == 1)
				return false;
		}

		optNum = doStat(sel);

		if (optNum == 0)
			return true;
	}
}

int Conv::showOptions() {
	static const byte active[] = {255, 255, 255};
	static const byte inactive[] = {162, 130, 130};

	int validOptions = 0;

	_vm->screen()->setPaletteColor(1, active);
	_vm->screen()->setPaletteColor(2, inactive);

	// Add a bullet at the beginning
	for (int i = 0; i < 3; i++) {
		if (_options[i].offset == 0) {
			_options[i].text = 0;
		} else {
			if (_options[i].offset) {
				_options[i].text = new char[strlen(_options[i].offset) + 3];
				strcpy(_options[i].text, "\x08 ");
				strcat(_options[i].text, _options[i].offset);
			}

			validOptions++;
		}
	}

	if (validOptions == 1) {
		freeOptions();
		return 0;
	}

	for (int i = 0; i < 3; i++) {

		int col = 4;
		int line = 0;
		char *word;

		if (_options[i].offset == 0) {
			_options[i].text = 0;
			continue;
		}

		word = strtok(_options[i].text, " ");
		while (word) {
			int width = _vm->screen()->getTextWidth(word) + 4 + 1;

			if (col + width > 308) {
				line++;

				if (line >= 24)
					break;
				col = 18;
			}

			_options[i].lines[line] += word;
			_options[i].lines[line] += " ";

			col += width;

			word = strtok(NULL, " ");
		}
	}

	_surfaceHeight = 0;

	// Calculate position of rows
	for (int i = 0; i < 3; i++) {
		if (i == 0)
			_options[i].pixelTop = 0;
		else
			_options[i].pixelTop = _surfaceHeight;

		for (int j = 0; j < 24 && !_options[i].lines[j].empty(); j++)
			_surfaceHeight += 8;
		_surfaceHeight += 3;
		_options[i].pixelBottom = _surfaceHeight;
	}

	_scrollPos = 0;
	_vm->input()->setMousePos(_vm->input()->getMouseX(), 0);

	do {
		_selectedOption = getOption();
		displayMenuOptions();
		_vm->screen()->gfxUpdate();

	} while ((!_vm->input()->getLeftClick() || _selectedOption == 9999) && !_vm->shouldQuit());

	freeOptions();

	return _selectedOption;
}

void Conv::freeOptions() {
	for (int i = 0; i < 3; i++) {
		delete[] _options[i].text;
		_options[i].text = 0;

		for (int j = 0; j < 24; j++) {
			_options[i].lines[j] = "";
		}
	}
}

int Conv::getOption() {

	if (_scrollPos <= _options[0].pixelBottom)
		return 0;

	for (int i = 1; i < 3; i++) {
		if (_options[i].text == 0)
			continue;

		if (_options[i].pixelTop < _scrollPos && _scrollPos <= _options[i].pixelBottom)
			return i;
	}

	return 9999;
}

void Conv::displayMenuOptions() {
	int row = 12;

	byte *optionsSurface = new byte[SCREEN_W * (_surfaceHeight + PANEL_H)];
	memset(optionsSurface, 0, SCREEN_W * (_surfaceHeight + PANEL_H));

	// Original method of scrolling - requires slow mouse sampling

	//_scrollPos += _vm->input()->getMouseY() - PANEL_H;
	//_scrollPos = CLIP(_scrollPos, 0, _surfaceHeight - PANEL_H);
	//_vm->input()->setMousePos(_vm->input()->getMouseX(), PANEL_H);

	// New scrolling method
	_scrollPos = (_surfaceHeight + 24 - PANEL_H) * _vm->input()->getMouseY() / 200;

	for (int i = 0; i < 3; i++) {

		if (_options[i].text == 0)
			continue;

		if (_selectedOption == i) {

			for (int j = 0; j < 24; j++) {
				if (_options[i].lines[j].empty())
					break;
				_vm->screen()->writeText(optionsSurface, _options[i].lines[j].c_str(), row, j == 0 ? 4 : 18, 1, false);
				row += 8;
			}

		} else {

			for (int j = 0; j < 24; j++) {
				if (_options[i].lines[j].empty())
					break;
				_vm->screen()->writeText(optionsSurface, _options[i].lines[j].c_str(), row, j == 0 ? 4 : 18, 2, false);
				row += 8;
			}
		}

		row += 3;
	}

	_vm->screen()->drawPanel(SCREEN_W * _scrollPos + optionsSurface);

	delete[] optionsSurface;
}

int Conv::doStat(int selection) {

	int ret = 0;

	for (Common::List<Statement>::const_iterator i = _options[selection].statements->begin();
			i != _options[selection].statements->end(); i++) {

		switch (i->command) {

		case 306:
			_talk->doTalk(i->charId, i->emotion, i->filename, _text + i->textOffset);
			break;

		case 307:
			ret = 0;
			break;

		case 309:
			ret = i->optNum;
			break;

		case 326:
			_vm->database()->setVar(i->address, i->val);
			break;

		case 419:
			_vm->database()->getChar(i->val)->unsetSpell();
			break;

		case 467:
			_vm->game()->doActionPlayVideo(i->filename);
			break;

		case 469:
			_vm->game()->doActionPlaySample(i->filename);
			break;

		default:
			warning("Unhandled talk opcode: %hd", i->command);
			break;
		}
	}

	return ret;
}

} // End of namespace Kom
