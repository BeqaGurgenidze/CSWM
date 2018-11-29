#ifndef INCLUDE_BASEMONSTER_H
#define INCLUDE_BASEMONSTER_H

class EHANDLE;

enum MONSTERSTATE
{
	MONSTERSTATE_NONE = 0,
	MONSTERSTATE_IDLE,
	MONSTERSTATE_COMBAT,
	MONSTERSTATE_ALERT,
	MONSTERSTATE_HUNT,
	MONSTERSTATE_PRONE,
	MONSTERSTATE_SCRIPT,
	MONSTERSTATE_PLAYDEAD,
	MONSTERSTATE_DEAD
};

class CBaseToggle;
class CBaseMonster;

#endif