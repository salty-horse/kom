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

#ifndef KOM_CONV_H
#define KOM_CONV_H

#include "common/file.h"
#include "common/list.h"
#include "common/str.h"

#include "sound.h"

namespace Kom {

struct ColorSet;

struct Statement {
	Statement() : command(0), charId(0), emotion(0), textOffset(0),
		          optNum(0), address(0), val(0) { filename[0] = '\0'; }
	int16 command;
	int16 charId;
	int16 emotion;
	uint32 textOffset;
	uint32 optNum;
	char filename[11];
	uint16 address;
	int16 val;
};

struct Option {
	int32 optNum;
	uint32 textOffset;
	Common::List<Statement> statements;
};

struct Conversation {
	int16 charId;
	int16 convNum;
	Common::List<Option> options;
};

struct Loop {
	int16 startFrame;
	int16 endFrame;
	int16 loopToDepth;
	int16 currDepth;
	int16 currFrame;
	int16 maxDepth;
};

struct State {
	int16 id;
	int16 loopCount;
	Loop *loops;
	Loop *currLoop;
};

struct Link {
	State *endState;
	int16 *currFrame;
	int16 *frameList;
};

struct Face {
	Face(const Common::String &filename, byte *lipBuffer, byte *zoomSurface);
	~Face();
	void assignLinks(const Common::String &filename);

	State *_currState;
	State *_states;
	Link *_faceLinks;
	Link *_playLink;
	int16 *_linkFrameList;
	int16 _stateCount;
	byte _status;
	byte _sentenceStatus;
	SoundSample _sample;
};

class Lips {
public:
	Lips(KomEngine *vm);
	~Lips();

	void init(Common::String playerCodename, Common::String otherCodename,
	          int16 char1ZoomX, int16 char1ZoomY,
		      int16 char2ZoomX, int16 char2ZoomY,
			  int16 convNum);

	void doTalk(uint16 charId, int16 emotion, const char *filename, const char *text, int pitch);

protected:
	KomEngine *_vm;

private:
	void loadSentence(Face *face, const Common::String &filename);
	void updateSentence(Face *face);
	void update(Face *face);
	void convDialogue();
	void loopTo(Face *face, int depth);
	void changeState(Face *face, int emotion);
	void rewindPlaySentence(Face *face);

	byte *_otherZoomSurface;
	byte *_otherFront;
	ColorSet *_narrColorSet;
	ColorSet *_playerColorSet;
	ColorSet *_otherColorSet;
	ColorSet *_multiColorSet;
	bool _fullPalette;
	bool _multiFullPalette;
	Common::String _convDir;
	int _colorSetType;
	byte *_frontBuffer;
	bool _playerActive;
	bool _narratorConv;
	Face *_playerFace;
	Face *_otherFace;
	char *_exchangeString;
	char *_exchangeToken;
	int _exchangeScrollSpeed;
	int _scrollTimer;
	int _scrollPos;
	int _exchangeColor;
	int _exchangeState;
	int _exchangeX;
	bool _wroteText;
	int _exchangeDisplay;
	int _smackerPlayed; // TODO: what to do with this? test smacker during conv?
	bool _isBalrog;
	SoundSample *_talkerSample;
	SoundSample *_otherSample;
	int16 _playerEmotion;
	int16 _otherEmotion;

	uint16 _lastCharacter;
	byte *_textSurface;
};

class Talk : Lips {
public:
	Talk(KomEngine *vm);
	~Talk();

	void init(uint16 charId, int16 convNum);

	void doTalk(int charId, int emotion, const char *sampleFile, const char *sentence);

private:
	int16 _talkingChar;
};

class Conv {
public:
	Conv(KomEngine *vm, uint16 charId);
	~Conv();

	bool doTalk(int16 convNum, int32 optNum);
	void doResponse(int responseNum);

private:
	void initConvs(uint32 offset);
	void initText(uint32 offset);

	bool doOptions(Conversation *conv, int32 optNum);
	int showOptions();
	void freeOptions();
	int getOption();
	void displayMenuOptions();
	int doStat(int selection);

	KomEngine *_vm;

	uint16 _charId;
	const char *_codename;
	byte *_convEntry;
	Common::File *_convData;

	byte _backupPalette[256 * 3];

	byte *_convs;
	char *_text;

	Talk *_talk;

	int _selectedOption;
	int _scrollPos;
	int _surfaceHeight;

	struct {
		char *offset;
		char *text;
		int16 b;
		Common::List<Statement>* statements;
		int16 d;
		Common::String lines[24];
		int pixelTop;
		int pixelBottom;
	} _options[3];

	Common::List<Conversation> _conversations;
};

} // End of namespace Kom

#endif
