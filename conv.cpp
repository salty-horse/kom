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
#include "common/system.h"
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

static const byte ACTIVE_COLOR[] = {255, 255, 255};
static const byte INACTIVE_COLOR[] = {162, 130, 130};

namespace Kom {

Face::Face(KomEngine *vm, const Common::String &filename, byte *zoomSurface) :
	_sentenceStatus(0), _flic(vm) {

	_flic.loadTalkVideo(filename.c_str(), zoomSurface);
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
	debug("File %s", filename.c_str());
	for (int i = 0; i < _stateCount; ++i) {
		debug("\tState %d", i);
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
			debug("\t\tLoop %d: %d %d %d", j, loop->startFrame, loop->endFrame, loop->maxDepth);
		}
	}

	_faceLinks = new Link[_stateCount * _stateCount];
	_linkFrameList = new int16[f.readSint16BE()];
	int16 *currFrame = _linkFrameList;

	Link *link = _faceLinks;
	for (int i = 0; i < _stateCount; ++i) {
		for (int j = 0; j < _stateCount; ++j) {
			debugN("link %d,%d: ", i, j);
			link->endState = &_states[j];
			link->frameList = currFrame;

			int data;
			do {
				data = f.readSint16BE();
				*currFrame = data;
				debugN("%d ", *currFrame);
				currFrame++;
			} while (data >= 0);
			debug("");
			++link;
		}
	}

	_currState = _states;
	_currState->currLoop = _currState->loops;
	_status = 0;

	f.close();
}

Lips::Lips(KomEngine *vm) : _vm(vm) {
	_playerFace = NULL;
	_otherFace = NULL;
	_otherZoomSurface = NULL;

	// Backup the palette
	_vm->_system->getPaletteManager()->grabPalette(_backupPalette, 0, 256);
}

Lips::~Lips() {
	delete _multiColorSet;
	delete _narrColorSet;
	delete[] _otherZoomSurface;
	delete _playerFace;
	delete _otherFace;

	// Restore the palette
	_vm->_system->getPaletteManager()->setPalette(_backupPalette, 0, 256);

	// Clear screen. Yes, this shouldn't be in a destructor
	_vm->screen()->clearScreen();
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
	_lastCharacter = 0;
	_narratorConv = false;
	_narrColorSet = NULL;
	_multiColorSet = NULL;
	_colorSetType = 0;
	_multiFullPalette = false;

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
		Common::String filenamePrefix = Common::String("kom/conv/") + playerCodename;
		Common::String flcFilename = filenamePrefix + ".flc";
		Common::String clFilename = filenamePrefix + ".cl";
		Common::String lnkFilename = filenamePrefix + ".lnk";
		_playerFace = new Face(_vm, flcFilename, zoomSurface);
		_playerColorSet = new ColorSet(clFilename.c_str());
		_playerFace->assignLinks(lnkFilename);
	}

	if (otherCodename == "s99nar") {
		_narratorConv = true;
	}
	//convunused = 1;
	if (!_narratorConv) {
		_otherZoomSurface = _vm->screen()->createZoomBlur(char2ZoomX, char2ZoomY);
	}

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
		_otherFace = new Face(_vm, fname, _narratorConv ? NULL : _otherZoomSurface);
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
	_exchangeColor = 2;
	_smackerPlayed = 0;
}

void Lips::doTalk(uint16 charId, int16 emotion, const char *filename, const char *text, int pitch) {
	Common::String sampleFilename = Common::String::format("%s/%s.raw", _convDir.c_str(), filename);

	bool pitchSet = false;
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
	_exchangeColor = charId + 1;

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

	if (!_isBalrog) {
		if (charId == 0) {
			// TODO - around loc_4264B

			if (!_playerActive)
				return;

			loadSentence(_playerFace, sampleFilename);
			if (_lastCharacter != 0) {
				_vm->screen()->clearRoom();
				_vm->screen()->gfxUpdate();
			}
			if (_fullPalette || emotion != 3) {
				// Restore palette
				_vm->_system->getPaletteManager()->setPalette(_backupPalette, 0, 256);
			}
			if (emotion == 3) {
				// use narrator color set
				_vm->screen()->useColorSet(_narrColorSet, 0);
			} else {
				// use player color set
				_vm->screen()->useColorSet(_playerColorSet, 0);
			}

			if (_playerEmotion != emotion) {
				changeState(_playerFace, emotion);
				_playerEmotion = emotion;
			}
			rewindPlaySentence(_playerFace);
			// TODO - loc_427AD - set pan and pitch

		} else {
			// TODO - loc_427D8
			loadSentence(_otherFace, sampleFilename);
			if (_lastCharacter == 0) {
				_vm->screen()->clearRoom();
				_vm->screen()->gfxUpdate();
				// Restore palette
				_vm->_system->getPaletteManager()->setPalette(_backupPalette, 0, 256);
			}
			if ((_colorSetType == 1 && emotion == 2) ||
			    (_colorSetType == 2 && emotion == 2) ||
			    (_colorSetType == 3 && emotion == 2) ||
			    (_colorSetType == 4 && emotion == 3)) {
				_vm->screen()->useColorSet(_multiColorSet, 0);
			} else {
				_vm->screen()->useColorSet(_otherColorSet, 0);
			}

			if (_otherEmotion != emotion) {
				changeState(_otherFace, emotion);
				_otherEmotion = emotion;
			}
			rewindPlaySentence(_otherFace);
			// TODO - loc_4299D - set pan and pitch
		}

		// loc_429C3
		_lastCharacter = charId;
		_vm->screen()->setPaletteColor(1, ACTIVE_COLOR);
		_vm->screen()->setPaletteColor(2, INACTIVE_COLOR);

		while (1) {
			Face *face = (charId == 0) ? _playerFace : _otherFace;

			updateSentence(face);

			if (_talkerSample != NULL && _vm->sound()->isPlaying(*_talkerSample) && !pitchSet) {
				// TODO: set pitch
				pitchSet = true;
			}

			if (_exchangeString != NULL)
				convDialogue();

			// loc_42A93
			_vm->screen()->gfxUpdate();

			if (_vm->shouldQuit())
				break;

			// Skip dialogue
			// TODO: break on space and esc as well
			if (_vm->input()->getRightClick()) {
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
			}

			if (face->_sentenceStatus == 3)
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
	} else {
		// TODO: Balrog
	}
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
			// TODO: control scroll speed with mouse
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
		int depth = 1;
		int average = getAverage(face);
		if (average < 80)
			depth = 1;
		else if (average < 90)
			depth = 2;
		else if (average < 100)
			depth = 3;
		else if (average < 105)
			depth = 4;
		else if (average < 110)
			depth = 5;
		else if (average < 115)
			depth = 6;
		else if (average < 120)
			depth = 7;
		else
			depth = 8;
		loopTo(face, depth);

		if (_talkerSample == NULL || !_vm->sound()->isPlaying(*_talkerSample)) {
			loopTo(face, 1);
			face->_sentenceStatus = 4;
		}
	}

	update(face);
}

int Lips::getAverage(Face *face) {
	if (face == NULL || _talkerSample == NULL)
		return -256;

	// Fetch the largest byte in the future sample bytes
	uint16 msc = _vm->sound()->getSampleElapsedTime(*_talkerSample);
	int currentPos = (int)(msc * 11025 / 1000);
	int sampleEndPos = _talkerSample->getRawBufferSize() - 1;
	if (currentPos > sampleEndPos)
		return 0;

	const int16 *sampleBuffer = _talkerSample->getRawBuffer();

	int sampleCount = 250;
	int maxValue = 0;
	while (1) {
		sampleCount--;
		if (sampleCount == -1) {
			break;
		}
		if (currentPos > sampleEndPos)
			break;
		int val = (((sampleBuffer[currentPos]) >> 8) ^ 0x80) / 2;
		if (val > maxValue) {
			maxValue = val;
		}
		currentPos++;
	}

	return maxValue;
}


void Lips::update(Face *face) {
	if (face == NULL) return;

	int16 frame = -1;

	switch (face->_status) {
	case FACE_STATUS_PLAY_FORWARDS: {
		Loop *loop = face->_currState->currLoop;
		if (loop->currDepth != loop->loopToDepth) {
			loop->currDepth++;
			loop->currFrame++;
		} else {
			face->_status &= ~FACE_STATUS_PLAY_FORWARDS;
			face->_status |= FACE_STATUS_PLAY_BACKWARDS;
			loop->currFrame = loop->startFrame + loop->endFrame - loop->currFrame;
		}
		frame = loop->currFrame;
		break;
	}
	case FACE_STATUS_PLAY_BACKWARDS: {
		Loop *loop = face->_currState->currLoop;
		frame = loop->currFrame;
		if (loop->currDepth != 1) {
			loop->currDepth--;
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
	case FACE_STATUS_CHANGE_STATE: // Move from one state to another
		if (*face->_playLink->currFrame < 0) {
			// Reached new state. Start playing sample and moving lips
			face->_currState = face->_playLink->endState;
			face->_status &= ~FACE_STATUS_CHANGE_STATE;
			if (face->_sentenceStatus == 2) {
				face->_sentenceStatus = 1;
				_talkerSample = &face->_sample;
				_vm->sound()->playSampleSpeech(*_talkerSample);
			}
		} else {
			frame = *face->_playLink->currFrame;
			face->_playLink->currFrame++;
		}
		break;
	default:
		if (face->_sentenceStatus == 4) {
			face->_sentenceStatus = 3;
		}
		frame = face->_currState->currLoop->currFrame;
		if (face->_sentenceStatus == 2) {
			face->_sentenceStatus = 1;
			_talkerSample = &face->_sample;
			_vm->sound()->playSampleSpeech(*_talkerSample);
		}

		break;
	}

	// TODO: Is this right?
	if (frame < 1)
		return;
	face->_flic.drawTalkFrame(frame);
}

void Lips::loopTo(Face *face, int depth) {
	if (face == NULL) return;
	if (face->_status & FACE_STATUS_CHANGE_STATE) return;
	if (face->_currState->loopCount == 0) return;

	if (depth < 1)
		depth = 1;

	Loop *loop = face->_currState->currLoop;

	switch (face->_status) {
	case FACE_STATUS_PLAY_FORWARDS:
		if (depth < loop->currDepth) {
			// Switch to the frame at the other end of the loop
			loop->currFrame = loop->startFrame + loop->endFrame - loop->currFrame;
			// Go forward to desired depth
			face->_status &= ~FACE_STATUS_PLAY_FORWARDS;
			face->_status |= FACE_STATUS_PLAY_BACKWARDS;
		}
		loop->loopToDepth = depth;
		break;
	case FACE_STATUS_PLAY_BACKWARDS:
		if (depth > loop->currDepth) {
			// Switch to the frame at the other end of the loop
			loop->currFrame = loop->startFrame + loop->endFrame - loop->currFrame;
			// Go back to desired depth
			face->_status &= ~FACE_STATUS_PLAY_BACKWARDS;
			face->_status |= FACE_STATUS_PLAY_FORWARDS;
		}
		loop->loopToDepth = depth;
		break;
	default:
		loop->currFrame = loop->startFrame;
		loop->currDepth = 1;
		loop->loopToDepth = depth;
		face->_status |= FACE_STATUS_PLAY_FORWARDS;
		break;
	}
}

void Lips::changeState(Face *face, int emotion) {
	if (face == NULL) return;
	if (face->_currState->id == emotion) return;
	if (face->_status & FACE_STATUS_CHANGE_STATE) return;
	if (face->_status & (FACE_STATUS_PLAY_FORWARDS | FACE_STATUS_PLAY_BACKWARDS)) return;
	if (emotion >= face->_stateCount) return;

	face->_playLink = &face->_faceLinks[face->_currState->id * face->_stateCount + emotion];
	face->_playLink->currFrame = face->_playLink->frameList;

	Loop *loops = face->_playLink->endState->loops;
	face->_playLink->endState->currLoop = loops;
	loops->currFrame = loops->startFrame;
	loops->loopToDepth = 1;
	loops->currDepth = 1;
	face->_status = FACE_STATUS_CHANGE_STATE;
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

	// TODO: Handle height based on spell effect
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

	_vm->panel()->setActionDesc("");
	_vm->panel()->setHotspotDesc("");

	// Clear the screen when showing the loading icon to prevent
	// palette corruption when a conversation was just displayed
	_vm->panel()->showLoading(true, true);
	Lips::init(playerCodename, otherChar->_name,
		playerX / 2 - 25,
		playerY / 2 - (playerHeight / playerChar->_start5),
		charX / 2 + 25,
		charY / 2 - (charHeight / otherChar->_start5),
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
	: _vm(vm), _convs(0), _text(0) {
	_charId = charId;
	_codename = _vm->database()->getChar(charId)->_name;
	_convData = _vm->database()->getConvData();
	_convEntry = _vm->database()->getConvIndex(_codename);

	_vm->screen()->showMouseCursor(false);
}

Conv::~Conv() {
	delete[] _convs;
	delete[] _text;

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
			Talk talk(_vm);
			talk.init(_charId, convNum);
			bool result = doOptions(talk, &(*c), optNum);
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

	if (num != responseNum) {
		error("Could not find response %d of %s", responseNum, _codename);
	}
	sprintf(convName, "%d", responseNum);
	Talk talk(_vm);
	talk.init(_charId, 0);
	talk.doTalk(_charId, emotion, convName, _text + offset);
}

bool Conv::doOptions(Talk &talk, Conversation *conv, int32 optNum) {
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
			sel = talk.showOptions(_options);

			if (_vm->shouldQuit()) {
				return false;
			}

			if (sel != -1)
				break;

			if (optNum == 1)
				return false;
		}

		optNum = doStat(talk, sel);

		if (optNum == 0)
			return true;
	}
}

int Lips::showOptions(OptionLine *options) {
	int validOptions = 0;

	// Add a bullet at the beginning
	for (int i = 0; i < 3; i++) {
		if (options[i].offset == 0) {
			options[i].text = 0;
		} else {
			if (options[i].offset) {
				options[i].text = new char[strlen(options[i].offset) + 3];
				strcpy(options[i].text, "\x08 ");
				strcat(options[i].text, options[i].offset);
			}

			validOptions++;
		}
	}

	if (validOptions == 1) {
		freeOptions(options);
		return 0;
	}

	for (int i = 0; i < 3; i++) {

		int col = 4;
		int line = 0;
		char *word;

		if (options[i].offset == 0) {
			options[i].text = 0;
			continue;
		}

		word = strtok(options[i].text, " ");
		while (word) {
			int width = _vm->screen()->getTextWidth(word) + 4 + 1;

			if (col + width > 308) {
				line++;

				if (line >= 24)
					break;
				col = 18;
			}

			options[i].lines[line] += word;
			options[i].lines[line] += " ";

			col += width;

			word = strtok(NULL, " ");
		}
	}

	int surfaceHeight = 0;

	// Calculate position of rows
	for (int i = 0; i < 3; i++) {
		if (i == 0)
			options[i].pixelTop = 0;
		else
			options[i].pixelTop = surfaceHeight;

		for (int j = 0; j < 24 && !options[i].lines[j].empty(); j++)
			surfaceHeight += 8;
		surfaceHeight += 3;
		options[i].pixelBottom = surfaceHeight;
	}

	_vm->screen()->clearScreen();

	// TODO balrog
	_vm->screen()->useColorSet(_playerColorSet, 0);
	updateSentence(_playerFace);
	// TODO more stuff

	_vm->input()->setMousePos(_vm->input()->getMouseX(), 0);

	int selectedOption = 9999;
	do {
		selectedOption = getOption(options, surfaceHeight);
		displayMenuOptions(options, selectedOption, surfaceHeight);
		_vm->screen()->gfxUpdate();

	} while ((!_vm->input()->getLeftClick() || selectedOption == 9999) && !_vm->shouldQuit());

	freeOptions(options);

	return selectedOption;
}

void Lips::freeOptions(OptionLine *options) {
	for (int i = 0; i < 3; i++) {
		delete[] options[i].text;
		options[i].text = 0;

		for (int j = 0; j < 24; j++) {
			options[i].lines[j] = "";
		}
	}
}

int Lips::getOption(OptionLine *options, int surfaceHeight) {

	int scrollPos = surfaceHeight * _vm->input()->getMouseY() / 200;

	if (scrollPos < options[0].pixelBottom)
		return 0;

	for (int i = 1; i < 3; i++) {
		if (options[i].text == 0)
			continue;

		if (options[i].pixelTop <= scrollPos && scrollPos < options[i].pixelBottom)
			return i;
	}

	return 9999;
}

void Lips::displayMenuOptions(OptionLine *options, int selectedOption, int surfaceHeight) {
	_vm->screen()->setPaletteColor(1, ACTIVE_COLOR);
	_vm->screen()->setPaletteColor(2, INACTIVE_COLOR);

	byte *optionsSurface = new byte[SCREEN_W * (surfaceHeight + PANEL_H + 24)]();

	// Original scrolling method - requires slow mouse sampling

	//scrollPos += _vm->input()->getMouseY() - PANEL_H;
	//scrollPos = CLIP(scrollPos, 0, surfaceHeight - PANEL_H);
	//_vm->input()->setMousePos(_vm->input()->getMouseX(), PANEL_H);

	// New scrolling method
	int scrollPos = (surfaceHeight + 24 - PANEL_H) * _vm->input()->getMouseY() / 200;

	int row = 12;
	for (int i = 0; i < 3; i++) {

		if (options[i].text == 0)
			continue;

		if (selectedOption == i) {

			for (int j = 0; j < 24; j++) {
				if (options[i].lines[j].empty())
					break;
				_vm->screen()->writeText(optionsSurface, options[i].lines[j].c_str(), row, j == 0 ? 4 : 18, 1, false);
				row += 8;
			}

		} else {

			for (int j = 0; j < 24; j++) {
				if (options[i].lines[j].empty())
					break;
				_vm->screen()->writeText(optionsSurface, options[i].lines[j].c_str(), row, j == 0 ? 4 : 18, 2, false);
				row += 8;
			}
		}

		row += 3;
	}

	_vm->screen()->drawPanel(optionsSurface + SCREEN_W * scrollPos);

	delete[] optionsSurface;
}

int Conv::doStat(Talk &talk, int selection) {

	int ret = 0;

	for (Common::List<Statement>::const_iterator i = _options[selection].statements->begin();
			i != _options[selection].statements->end(); i++) {

		switch (i->command) {

		case 306:
			talk.doTalk(i->charId, i->emotion, i->filename, _text + i->textOffset);
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
