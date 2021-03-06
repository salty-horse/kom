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

#ifndef KOM_GAME_H
#define KOM_GAME_H

#include "common/scummsys.h"
#include "common/array.h"

#include "kom/sound.h"
#include "kom/character.h"

class OSystem;

namespace Kom {

class KomEngine;
class VideoPlayer;
struct Command;

struct RoomObject {
	int16 objectId;
	int16 disappearTimer;
	int16 actorId;
	int16 z2;
	int16 priority;
};

struct RoomDoor {
	RoomDoor() : frame(0), boxHit(0), state(0) {}
	int actorId;
	uint8 frame;
	uint8 boxHit;
	int8 boxOpenFast; // Box closest to exit
	int8 boxOpenSlow; // Box farther from exit
	int8 state;
};

enum CollideType {
	COLLIDE_NONE = 0,
	COLLIDE_BOX,
	COLLIDE_CHAR,
	COLLIDE_OBJECT
};

enum ObjectType {
	OBJECT_NONE = -1,
	OBJECT_SPELL,
	OBJECT_WEAPON,
	OBJECT_ITEM
};


struct Settings {
	Settings() : gameCycles(6000), dayMode(1), mouseState(0),
		mouseX(0), mouseY(0), mouseMode(0),
		narratorPatience(0), lastItemUsed(-1), lastWeaponUsed(-1),
		fightWordScope(0), fightEffectTimer(0), fightEffectScope(0),
		objectType(OBJECT_NONE) {
			for (int i = 0; i < 4; i++) fightNPCCloud[i].charId = -1;
		}
	uint16 mouseState;
	bool mouseOverExit;
	uint16 mouseX;
	uint16 mouseY;
	int16 mouseBox;
	CollideType overType;
	CollideType oldOverType;
	int16 overNum;
	int16 oldOverNum;
	int16 mouseMode;
	CollideType collideType;
	int16 collideBox;
	int16 collideBoxX;
	int16 collideBoxY;
	int16 collideBoxZ;
	int16 collideObj;
	uint16 collideObjX;
	uint16 collideObjY;
	int32 collideObjZ;
	int16 collideChar;
	uint16 collideCharX;
	uint16 collideCharY;
	int32 collideCharZ;
	int16 objectNum;
	ObjectType objectType;
	int16 object2Num;
	ObjectType object2Type;

	uint8 dayMode; // 0 - day. 1 - night. 2 - dawn. 3 - dusk
	uint8 currLocation;
	uint16 gameCycles;
	bool fightEnabled;
	uint16 narratorPatience;
	int16 lastItemUsed;
	int16 lastWeaponUsed;

	// Fight stuff
	int16 fightState;
	int16 fightTimer;
	int16 fightWordTimer;
	int32 fightWordX;
	int32 fightWordY;
	int32 fightWordStepX;
	int32 fightWordStepY;
	int16 fightWordScope;
	int16 fightEffectPause;
	int16 fightEffectTimer;
	int16 fightEffectScope;
	Character fightEffectChar;

	struct NPCCloud {
		int16 charId;
		int16 timer;
	} fightNPCCloud[4];
};

enum CommandType {
	CMD_WALK = 0,
	CMD_NOTHING,
	CMD_USE,
	CMD_TALK_TO,
	CMD_PICKUP,
	CMD_LOOK_AT,
	CMD_FIGHT,
	CMD_CAST_SPELL
};

/** Player settings */
struct Player {
	Player() : isNight(0), sleepTimer(0), colgateTimer(0), collideType(COLLIDE_NONE), collideNum(0),
		greetingChar(-1), greetingLoc(0), spriteCutNum(0) {}
	CommandType command;
	int16 commandState;
	CollideType collideType;
	int16 collideNum;
	bool narratorTalking;
	SoundSample spriteSample;
	SoundSample narratorSample; // Original doesn't store this here
	int16 greetingChar;
	int16 greetingLoc;
	int16 fightEnemy;
	int16 fightWeapon;
	int16 fightBarTimer;
	int16 enemyFightBarTimer;
	int16 hitPointsOld;
	int16 hitPoints;
	int16 enemyHitPointsOld;
	int16 enemyHitPoints;
	int16 enemyId;
	int16 lastEnemy;
	char enemyDesc[50];
	uint8 selectedChar; // 0 - thidney. 1 - shahron
	uint8 selectedQuest;
	uint8 isNight;
	int weaponSoundEffect;
	SoundSample weaponSample;
	int oldGold;
	uint16 sleepTimer;
	int16 colgateTimer;
	bool spriteCutMoving;
	int16 spriteCutX;
	int16 spriteCutY;
	int spriteCutTab;
	int16 spriteCutPos;
	int16 spriteCutNum;
};

struct Cb {
	int16 data1;
	int16 data2;
	bool talkInitialized;
	bool cloudActive;
};

struct Inventory {
	uint16 shopChar;
	uint8 mode;
	int16 mouseState;
	int16 selectedBox;
	int16 selectedBox2;
	bool isSelected;
	int16 scrollY;
	int16 iconX;
	int16 iconY;
	int16 mouseX;
	int16 mouseY;
	int16 mouseRow;
	int16 mouseCol;
	int selectedInvObj;
	int selectedWeapObj;
	int16 selectedSpellObj;
	int16 bottomEdge;
	int16 topEdge;
	int totalRows;
	int16 offset_3E;
	int16 blinkLight;
	int selectedObj;
	int action;
};

struct Spell {
	int16 spellId;
	int16 holdDuration;
	int16 duration;
	int16 targetId;
	bool doneHold;
};

class Game {
public:
	Game(KomEngine *vm, OSystem *system);
	~Game();

	void enterLocation(uint16 locId);
	void processTime();
	bool doStat(const Command *cmd);
	void doCommand(int command, int type, int id, int type2, int id2);
	void checkUseImmediate(ObjectType type, int16 id);
	void loopMove();
	void loopCollide();
	void loopSpriteCut();
	void loopSpells();
	void loopTimeouts();
	void loopInterfaceCollide();

	Settings* settings() { return &_settings; }
	Player* player() { return &_player; }
	Cb* cb() { return &_cb; }

	void setDay();
	void setNight();

	void doActionMoveChar(uint16 charId, int16 loc, int16 box);
	void doActionSpriteScene(const char *name, int charId, int loc, int box);
	void doActionPlayVideo(const char *name);

	void doActionPlaySample(const char *name);
	void narratorStart(const char *filename, const char *codename);
	void stopNarrator();
	bool isNarratorPlaying();
	void doGreeting(int charId, int response);
	void stopGreeting();
	bool isSamplePlaying();
	void doReply(int charId, int reply);
	void doReply(int charId, const char *reply);

	int8 doDonut(int type, Inventory *inv);
	void doInventory(int16 *objectNum, ObjectType *objectType, uint16 shopChar, uint8 num);
	int16 doActionShop(uint16 charId);

	void doActionGotObject(uint16 obj);
	void doActionLostObject(uint16 obj);

	void doLookAt(int charId, int pauseTimer = 0, bool showBackground = false);

	void declareNewEnemy(int16 enemy);
	void doFight(int enemyId, int weaponId);
	void doNPCFight(int attacker, int defender);

	bool castSpell(int16 charId, int16 spellId, int16 target);
	void doSpellAttack(int16 target, int16 spellId);
	void doActionSetSpell(int16 target, int16 spellType);
	void doActionUnsetSpell(int16 target, int16 spellType);

	void exeUse();
	void exeTalk();
	void exePickup();
	void exeLookAt();
	void exeFight();
	void exeMagic();

	Common::Array<RoomObject> *getObjects() { return &_roomObjects; }
	Common::Array<RoomDoor> *getDoors() { return &_roomDoors; }
	Spell *getSpell(int i) { return &_spells[i]; }

private:

	OSystem *_system;
	KomEngine *_vm;

	Common::Array<RoomObject> _roomObjects;
	Common::Array<RoomDoor> _roomDoors;
	Spell _spells[10];

    // Settings
    Settings _settings;
    Player _player;
	Cb _cb;

	VideoPlayer *_videoPlayer;

	void processChars();
	void processChar(int proc);
	bool doProc(int command, int type, int id, int type2, int id2);
	void doNoUse();
	int16 doExternalAction(const char *action);
	bool doActionCollide(int16 char1, int16 char2);

	void doActionDusk();
	void doActionDawn();

	void moveChar(uint16 charId, bool param);
	void moveCharOther(uint16 charId);

	int getDonutSegment(int xPos, int yPos);
};

} // End of namespace Kom

#endif

