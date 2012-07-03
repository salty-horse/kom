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

#ifndef KOM_H
#define KOM_H

#include "engines/engine.h"
#include "common/error.h"
#include "common/scummsys.h"
#include "common/random.h"

#include "kom/sound.h"
#include "kom/debugger.h"

class OSystem;

namespace GUI {
class Debugger;
}


/**
 * This is the namespace of the Kom engine.
 *
 * Status of this engine: ???
 *
 * Games using this engine:
 * - Kingdom O' Magic
 */
namespace Kom {

class ActorManager;
class Database;
class Font;
class Game;
class Input;
class Panel;
class Screen;

enum GameLoopState {
	GAMELOOP_RUNNING = 0,
	GAMELOOP_DEATH,
	GAMELOOP_SELECTION,
	GAMELOOP_QUIT = 4
};

class KomEngine : public Engine {
public:
	KomEngine(OSystem *system);
	~KomEngine();

	Common::Error run();

	::GUI::Debugger *getDebugger() { return _debugger; }

	Input *input() const { return _input; }
	ActorManager *actorMan() const { return _actorMan; }
	Screen *screen() const { return _screen; }
	Database *database() const { return _database; }
	Panel *panel() const { return _panel; }
	Game *game() const { return _game; }
	Sound *sound() const { return _sound; }
	Common::RandomSource *rnd() const { return _rnd; }

	int gameLoopTimer() { return _gameLoopTimer; }
	void endGame() { _gameLoopState = GAMELOOP_DEATH; }
	void quitGame() { _gameLoopState = GAMELOOP_QUIT; }

	void ambientStart(int locId);
	void ambientStop();
	void ambientPause(bool paused) { _sound->pauseSample(_ambientSample, paused); }

	void loadWeaponSample(int id);

	uint8 _flicLoaded;

	// Samples
	SoundSample _ripSample;
	SoundSample _hotspotSample;
	SoundSample _doorsSample;
	SoundSample _clickSample;
	SoundSample _swipeSample;
	SoundSample _cashSample;
	SoundSample _loseItemSample;
	SoundSample _magicSample;
	SoundSample _fluffSample;
	SoundSample _colgateSample;
	SoundSample _colgateOffSample;
	SoundSample _fightSample;
	SoundSample _weaponSample;
	SoundSample _ambientSample;

	// music
	int16 _playingMusicId, _playingMusicVolume;

	static const byte _distanceVolumeTable[];

private:
	void gameLoop();

	Screen *_screen;
	Database *_database;
	ActorManager *_actorMan;
	Input *_input;
	Sound *_sound;
	Panel *_panel;
	Debugger *_debugger;
	Game *_game;

	GameLoopState _gameLoopState;
	int _gameLoopTimer;

	Common::RandomSource *_rnd;
};

} // End of namespace Kom

#endif
