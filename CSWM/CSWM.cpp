#include "Module.h"

#pragma warning (disable : 4701)
#pragma warning (disable : 4996)

edict_t *SVGame_Edicts;
size_t PEV_OFFSET;

//CWeapon Weapons[MAX_CUSTOM_WEAPONS];
ke::Vector<CWeapon> Weapons;
ke::Vector<CAmmo> Ammos;
ke::Vector<CProjectile> Projectiles;
ke::Vector<ItemInfo> ItemInfoes;

BOOL Activated = FALSE;
BOOL Initialized = FALSE;
BOOL EntityHooked = FALSE;

void *FWeapon_Spawn[MAX_WEAPON_TYPES];
void *FWeapon_AddToPlayer[MAX_WEAPON_TYPES];
void *FWeapon_Deploy[MAX_WEAPON_TYPES];
void *FWeapon_PrimaryAttack[MAX_WEAPON_TYPES];
void *FWeapon_SecondaryAttack[MAX_WEAPON_TYPES];
void *FWeapon_Reload[MAX_WEAPON_TYPES + 1];
void *FWeapon_PostFrame[MAX_WEAPON_TYPES];
void *FWeapon_Idle[MAX_WEAPON_TYPES];
void *FWeapon_Holster[MAX_WEAPON_TYPES];
void *FWeapon_GetMaxSpeed[MAX_WEAPON_TYPES];
void *FWeapon_GetItemInfo[MAX_WEAPON_TYPES];
void *FWeapon_ExtractAmmo[MAX_WEAPON_TYPES];
void *FWeapon_Dump;
void *FPlayer_TakeDamage, *FPlayerBot_TakeDamage;
void *FPlayer_GiveAmmo;
void *FWeaponBox_Spawn;
void *FTraceAttackEntity;
void *FTraceAttackPlayer;
void *FPlayerKilled;
void *FProjectileThink;

static const char *GetWeaponName(int WeaponID);

static void SetWeaponHUD(edict_t *Player, int WeaponID);
static void SetWeaponHUDCustom(edict_t *Player, CWeapon *Weapon);
static void StatusIcon(edict_t *PlayerEdict, BOOL Status, const char *Icon, int R, int G, int B);
static void ScreenShake(edict_t *PlayerEdict, int Amplitude, int Duration, int Frequency);
static void ClientPrint(edict_t *PlayerEdict, int Type, const char *Msg);
static void BarTime(edict_t *PlayerEdict, int Duration);
static void SendWeaponAnim(CBasePlayerWeapon *BaseWeapon, int Anim);
static void GiveAmmo2(edict_t *PlayerEdict, CBasePlayer *BasePlayer, int AmmoIndex, CAmmo *Ammo);

extern cell AmmoCount;

const CAmmo DEFAULT_AMMOS[]
{
	{ NULL, NULL, NULL, "\0" },
	{ AMMO_338MAG_PRICE, AMMO_338MAG_BUY, MAX_AMMO_338MAGNUM, "338Magnum" },
	{ AMMO_762MM_PRICE, AMMO_762NATO_BUY, MAX_AMMO_762NATO, "762Nato" },
	{ AMMO_556MM_PRICE, AMMO_556NATOBOX_BUY, MAX_AMMO_556NATOBOX, "556NatoBox" },
	{ AMMO_556MM_PRICE, AMMO_556NATO_BUY, MAX_AMMO_556NATO, "556Nato" },
	{ AMMO_BUCKSHOT_PRICE, AMMO_BUCKSHOT_BUY, MAX_AMMO_BUCKSHOT, "buckshot" },
	{ AMMO_45ACP_PRICE, AMMO_45ACP_BUY, MAX_AMMO_45ACP, "45acp" },
	{ AMMO_57MM_PRICE, AMMO_57MM_BUY, MAX_AMMO_57MM, "57mm" },
	{ AMMO_50AE_PRICE, AMMO_50AE_BUY, MAX_AMMO_50AE, "50AE" },
	{ AMMO_357SIG_PRICE, AMMO_357SIG_BUY, MAX_AMMO_357SIG, "357SIG" },
	{ AMMO_9MM_PRICE, AMMO_9MM_BUY, MAX_AMMO_9MM, "9mm" },
	{ 200, AMMO_FLASHBANG, -1, "Flashbang" },
	{ 300, AMMO_HEGRENADE, -1, "HEGrenade" },
	{ 300, AMMO_SMOKEGRENADE, -1, "SmokeGrenade" },
	{ 0, AMMO_C4, -1, "C4" },
};

// UNINITIALIZED
int SmokeIndex[3];

int RingIndex;
int BloodSprayIndex;
int BloodDropIndex;

BOOL SV_Cheats;
BOOL MP_FriendlyFire;
BOOL ABlood, HBlood;
extern BOOL SV_Log;
extern cell WeaponCount;
int MaxPlayers = 6;

const int WEAPON_FID[] = { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0 };
const int WEAPON_SLOT[] = { 0, 2, 2, 1, 4, 1, 5, 1, 1, 4, 2, 2, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 4, 2, 1, 1, 3, 1 };
const int WEAPON_SLOTID[] = { 1, 0, 0, 0 };
const int WEAPON_NUMINSLOT[] = { 3, 12, 1, 2 };
const int WEAPON_WEIGHT[] = { P228_WEIGHT, XM1014_WEIGHT, AK47_WEIGHT, AWP_WEIGHT };
const int WEAPON_TYPE_ID[] = { CSW_P228, CSW_XM1014, CSW_AK47, CSW_AWP };
//const int WEAPON_CLIP[] = { 13, 7, 30, 10 };
const int GUNSHOT_DECALS[] = { 41, 42, 43, 44, 45 };
const int WEAPONS_BIT_SUM = 1577057706;
const int OFFSET_TEAM = 114;

static CWeapon *Weapon = NULL;
static CIcon CurrentIcon = { NULL, NULL, NULL, NULL };

void PrecacheModule(void);
void **SpecialBot_VTable = NULL;

static BOOL HasPrivateData(edict_t *Edict)
{
	if (FNullEnt(Edict) || !Edict->pvPrivateData)
		return FALSE;

	return TRUE;
}

static BOOL HasPrivateData(entvars_t *EntVars)
{
	if (!EntVars || !HasPrivateData(ENT(EntVars)))
		return FALSE;

	return TRUE;
}

static void __fastcall Weapon_SpawnPistol(CBasePlayerWeapon *BaseWeapon, int)
{
	SET_WEAPON_FID(BaseWeapon, WType::Pistol);
	static_cast<void(__fastcall *)(CBasePlayerWeapon *, int)>(FWeapon_Spawn[WType::Pistol])(BaseWeapon, NULL);
}

static void __fastcall Weapon_SpawnShotgun(CBasePlayerWeapon *BaseWeapon, int)
{
	SET_WEAPON_FID(BaseWeapon, WType::Shotgun);
	static_cast<void(__fastcall *)(CBasePlayerWeapon *, int)>(FWeapon_Spawn[WType::Shotgun])(BaseWeapon, NULL);
}

static void __fastcall Weapon_SpawnRifle(CBasePlayerWeapon *BaseWeapon, int)
{
	SET_WEAPON_FID(BaseWeapon, WType::Rifle);
	static_cast<void(__fastcall *)(CBasePlayerWeapon *, int)>(FWeapon_Spawn[WType::Rifle])(BaseWeapon, NULL);

}

static void __fastcall Weapon_SpawnSniper(CBasePlayerWeapon *BaseWeapon, int)
{
	SET_WEAPON_FID(BaseWeapon, WType::Sniper);
	static_cast<void(__fastcall *)(CBasePlayerWeapon *, int)>(FWeapon_Spawn[WType::Sniper])(BaseWeapon, NULL);
}

static BOOL __fastcall Weapon_AddToPlayer(CBasePlayerWeapon *BaseWeapon, int, CBasePlayer *BasePlayer)
{
	static_cast<BOOL(__fastcall *)(CBasePlayerWeapon *, int, CBasePlayer *)>(FWeapon_AddToPlayer[GET_WEAPON_FID(BaseWeapon)])(BaseWeapon, NULL, BasePlayer);

	edict_t *PlayerEdict = ENT(BasePlayer->pev);

	if (!GET_CUSTOM_WEAPON(BaseWeapon))
		SetWeaponHUD(PlayerEdict, BaseWeapon->m_iId);
	else
	{
		Weapon = &Weapons[GET_WEAPON_KEY(BaseWeapon)];
		SetWeaponHUDCustom(PlayerEdict, Weapon);
		SET_WEAPON_OWNER(BaseWeapon, NUM_FOR_EDICT(PlayerEdict));
		BaseWeapon->m_iPrimaryAmmoType = Weapon->AmmoID;
	}

	return TRUE;
}

static BOOL __fastcall Weapon_Deploy(CBasePlayerWeapon *BaseWeapon, int)
{
	static_cast<BOOL(__fastcall *)(CBasePlayerWeapon *)>(FWeapon_Deploy[GET_WEAPON_FID(BaseWeapon)])(BaseWeapon);

	if (!GET_CUSTOM_WEAPON(BaseWeapon))
		return TRUE;

	Weapon = &Weapons[GET_WEAPON_KEY(BaseWeapon)];
	entvars_t *PlayerEntVars = BaseWeapon->m_pPlayer->pev;
	PlayerEntVars->viewmodel = Weapon->VModel;
	PlayerEntVars->weaponmodel = Weapon->PModel;
	BOOL SwitchON = (Weapon->A2I == A2_Switch && GET_WEAPON_INATTACK2(BaseWeapon)) ? TRUE : FALSE;

	BaseWeapon->m_pPlayer->m_flNextAttack = BaseWeapon->m_flTimeWeaponIdle = BaseWeapon->m_flNextSecondaryAttack = BaseWeapon->m_flNextPrimaryAttack = SwitchON ? Weapon->A2V->WA2_SWITCH_ANIM_DRAW_DURATION : Weapon->AnimD_Duration;
	SendWeaponAnim(BaseWeapon, SwitchON ? Weapon->A2V->WA2_SWITCH_ANIM_DRAW : Weapon->AnimD);

	if (GET_WEAPON_ICON(BaseWeapon))
	{
		CurrentIcon = (Weapon->A2I == A2_Switch && GET_WEAPON_INATTACK2(BaseWeapon)) ? Weapon->A2V->Icon : Weapon->Icon;
		StatusIcon(ENT(PlayerEntVars), ICON_ENABLED, CurrentIcon.Name, CurrentIcon.R, CurrentIcon.G, CurrentIcon.B);
	}

	if (Weapon->Forwards[WForward::DeployPost])
		MF_ExecuteForward(Weapon->Forwards[WForward::DeployPost], NUM_FOR_EDICT(ENT(BaseWeapon->pev)));

	return TRUE;
}

#define VectorSub(x, y, z) z[0] = x[0] - y[0]; z[1] = x[1] - y[1]; z[2] = x[2] - y[2];
#define VectorMulScalar(x, y, z) z[0] = x[0] * y; z[1] = x[1] * y; z[2] = x[2] * y;
#define VectorAdd(x, y, z) z[0] = x[0] + y[1]; z[1] = x[1] + y[1]; z[2] = x[2] + y[2];

inline void Origin_PrimaryAttack(CBasePlayerWeapon *BaseWeapon)
{
	static_cast<void(__fastcall *)(CBasePlayerWeapon *, int)>(FWeapon_PrimaryAttack[GET_WEAPON_FID(BaseWeapon)])(BaseWeapon, NULL);
}

static void __fastcall Weapon_PrimaryAttack(CBasePlayerWeapon *BaseWeapon, int)
{
	if (!GET_CUSTOM_WEAPON(BaseWeapon))
		return Origin_PrimaryAttack(BaseWeapon);

	BOOL InAttack2;
	int Clip, FOV;
	entvars_t *PlayerEntVars;
	Vector PunchAngleOld;

	CWeapon *Weapon = &Weapons[GET_WEAPON_KEY(BaseWeapon)];
	InAttack2 = GET_WEAPON_INATTACK2(BaseWeapon);

	int A2I = A2_None;

	if (InAttack2)
		A2I = Weapon->A2I;

	if (A2I == A2_AutoPistol && BaseWeapon->m_flNextPrimaryAttack > 0.f)
		return;

	PlayerEntVars = BaseWeapon->m_pPlayer->pev;
	Clip = BaseWeapon->m_iClip;
	PunchAngleOld = PlayerEntVars->punchangle;

	if (BaseWeapon->m_fInSpecialReload == 1 && !BaseWeapon->m_iClip)
	{
		BaseWeapon->Reload();
		return;
	}

	if ((Weapon->Forwards[WForward::PrimaryAttackPre]) && MF_ExecuteForward(Weapon->Forwards[WForward::PrimaryAttackPre], NUM_FOR_EDICT(ENT(BaseWeapon->pev))) > WReturn::IGNORED)
		return;

	if (Weapon->Flags & WFlag::AutoSniper)
		FOV = BaseWeapon->m_pPlayer->m_iFOV;

	if (!BaseWeapon->m_iClip)
	{
		BaseWeapon->PlayEmptySound();
		BaseWeapon->m_flNextPrimaryAttack = 0.2;
	}

	Origin_PrimaryAttack(BaseWeapon);

	if ((Weapon->A2I == A2_Burst || Weapon->A2I == A2_MultiShot) && InAttack2)
		BaseWeapon->m_iShotsFired = FALSE;

	if (Weapon->Flags & AutoSniper)
		BaseWeapon->m_pPlayer->m_iFOV = FOV;

	if (Clip <= BaseWeapon->m_iClip)
		return;

	Vector PunchAngle = PlayerEntVars->punchangle;
	VectorSub(PunchAngle, PunchAngleOld, PunchAngle);

	float Delay;
	int Anim;
	float Recoil;

	switch (A2I)
	{
		case A2_MultiShot:
		case A2_None: Delay = Weapon->Delay; Recoil = Weapon->Recoil; Anim = Weapon->AnimS[RANDOM_LONG(0, 2)]; break;
		case A2_Switch: Delay = Weapon->A2V->WA2_SWITCH_DELAY; Anim = Weapon->A2V->WA2_SWITCH_ANIM_SHOOT; Recoil = Weapon->A2V->WA2_SWITCH_RECOIL;  break;
		case A2_AutoPistol: Delay = Weapon->A2V->WA2_AUTOPISTOL_DELAY; Anim = Weapon->A2V->WA2_AUTOPISTOL_ANIM; Recoil = Weapon->A2V->WA2_AUTOPISTOL_RECOIL; break;
		case A2_Burst: Delay = 0.5f;  Recoil = Weapon->Recoil; Anim = Weapon->AnimS[RANDOM_LONG(0, 2)]; break;
		case A2_InstaSwitch: Delay = Weapon->A2V->WA2_INSTASWITCH_DELAY; Recoil = Weapon->A2V->WA2_INSTASWITCH_RECOIL; Anim = Weapon->A2V->WA2_INSTASWITCH_ANIM_SHOOT;
	}

	if (A2I == A2_MultiShot)
		Recoil *= 1.5;

	SET_WEAPON_LASTATTACKINATTACK2(BaseWeapon, A2I);

	VectorMulScalar(PunchAngle, Recoil, PunchAngle);
	VectorAdd(PunchAngle, PunchAngleOld, PunchAngle);
	// This one looks okay
	// PlayerEntVars->punchangle = PlayerEntVars->punchangle * Recoil;
	clamp(PunchAngle[0], -7.f, 7.f);
	clamp(PunchAngle[1], -7.f, 7.f);
	clamp(PunchAngle[2], -7.f, 7.f);
	PlayerEntVars->punchangle = PunchAngle;

	SendWeaponAnim(BaseWeapon, Anim);
	BaseWeapon->m_flNextPrimaryAttack = Delay;
	BaseWeapon->m_flNextSecondaryAttack = 0.1f;
	BaseWeapon->m_flTimeWeaponIdle = (A2I == A2_Switch) ? Weapon->A2V->WA2_SWITCH_ANIM_SHOOT_DURATION : Weapon->AnimS_Duration;

	EMIT_SOUND(ENT(BaseWeapon->pev)/*ENT(PlayerEntVars)*/, CHAN_VOICE, A2I == A2_Switch ? Weapon->A2V->WA2_SWITCH_FSOUND : Weapon->FireSound);

	switch (A2I)
	{
		case A2_None: break;
		case A2_Burst:
		{
			GET_WEAPON_CURBURST(BaseWeapon)--;

			if (GET_WEAPON_INBURST(BaseWeapon))
				break;

			BaseWeapon->m_flLastFire = gpGlobals->time;

			SET_WEAPON_INBURST(BaseWeapon, TRUE);
			SET_WEAPON_CURBURST(BaseWeapon, GET_WEAPON_A2OFFSET(BaseWeapon));
			break;
		}
		case A2_AutoPistol:
		{
			if (InAttack2)
				ScreenShake(ENT(BaseWeapon->m_pPlayer->pev), 4, 3, 7);

			break;
		}
	}

	if (Weapon->Forwards[WForward::PrimaryAttackPost])
		MF_ExecuteForward(Weapon->Forwards[WForward::PrimaryAttackPost], NUM_FOR_EDICT(ENT(BaseWeapon->pev)));

}

static void __fastcall Weapon_SecondaryAttack(CBasePlayerWeapon *BaseWeapon, int)
{
	if (!GET_CUSTOM_WEAPON(BaseWeapon))
		static_cast<void(__fastcall *)(CBasePlayerWeapon *)>(FWeapon_SecondaryAttack[GET_WEAPON_FID(BaseWeapon)])(BaseWeapon);
}

inline int *GetPlayerAmmo(CBasePlayer *BasePlayer, int AmmoID);

static void __fastcall Weapon_Reload(CBasePlayerWeapon *BaseWeapon, int CustomReload) // LETS USE THIS
{
	static_cast<void(__fastcall *)(CBasePlayerWeapon *, int)>(FWeapon_Reload[CustomReload ? WType::Rifle : GET_WEAPON_FID(BaseWeapon)])(BaseWeapon, 0);

	Weapon = &Weapons[GET_WEAPON_KEY(BaseWeapon)];

	if (!GET_CUSTOM_WEAPON(BaseWeapon) || !BaseWeapon->m_pPlayer->m_rgAmmo[Weapon->AmmoID] || BaseWeapon->m_iClip == Weapon->Clip)
		return;

	if (Weapon->Forwards[WForward::ReloadPre] && MF_ExecuteForward(Weapon->Forwards[WForward::ReloadPre], NUM_FOR_EDICT(ENT(BaseWeapon->pev))) > WReturn::IGNORED)
		return;

	BOOL SwitchON = (Weapon->A2I == A2_Switch && GET_WEAPON_INATTACK2(BaseWeapon));

	BaseWeapon->m_pPlayer->m_iFOV = CS_NO_ZOOM;
	BaseWeapon->m_flTimeWeaponIdle = BaseWeapon->m_flNextSecondaryAttack = BaseWeapon->m_pPlayer->m_flNextAttack = SwitchON ? Weapon->A2V->WA2_SWITCH_ANIM_RELOAD_DURATION : Weapon->AnimR_Duration;
	BaseWeapon->m_fInReload = TRUE;
	SendWeaponAnim(BaseWeapon, SwitchON ? Weapon->A2V->WA2_SWITCH_ANIM_RELOAD : Weapon->AnimR);

	if (Weapon->Forwards[WForward::ReloadPost])
		MF_ExecuteForward(Weapon->Forwards[WForward::ReloadPost], NUM_FOR_EDICT(ENT(BaseWeapon->pev)));
}

int LookupSequence(void *pmodel, const char *label)
{
	studiohdr_t *pstudiohdr = (studiohdr_t *)pmodel;

	if (!pstudiohdr)
	{
		return 0;
	}

	// Look up by sequence name.
	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);
	for (int i = 0; i < pstudiohdr->numseq; i++)
	{
		if (!Q_stricmp(pseqdesc[i].label, label))
			return i;
	}

	// Not found
	return ACT_INVALID;
}

int LookupSequence(CBasePlayer *BasePlayer, const char *label)
{
	void *pmodel = GET_MODEL_PTR(ENT(BasePlayer->pev));
	return ::LookupSequence(pmodel, label);
}

void SetAnimation(edict_t *PlayerEdict, int Animation, Activity IActivity, float FrameRate = 1.0);
void SetAnimation2(edict_t *PlayerEdict, PLAYER_ANIM Animation)
{
	CBasePlayer *BasePlayer = (CBasePlayer *)PlayerEdict->pvPrivateData;
	// ONLY RELOAD SUPPORT TODAY
	if (Animation != PLAYER_RELOAD)
		return;

	static char AnimExt[32];
	BOOL Ducking = BasePlayer->pev->flags & FL_DUCKING;

	if (Ducking)
		Q_strcpy(AnimExt, "crouch_reload_shotgun");
	else
		Q_strcpy(AnimExt, "ref_reload_shotgun");

	int IAnimation = LookupSequence(BasePlayer, AnimExt);

	if (IAnimation < -1)
		return;

	SetAnimation(ENT(BasePlayer->pev), IAnimation, ACT_RELOAD);
}

static void __fastcall Weapon_ReloadShotgun(CBasePlayerWeapon *BaseWeapon, int)
{
	if (!GET_CUSTOM_WEAPON(BaseWeapon))
	{
		return static_cast<void(__fastcall *)(CBasePlayerWeapon *)>(FWeapon_Reload[WType::Shotgun])(BaseWeapon);
	}
	else
	{
		Weapon = &Weapons[GET_WEAPON_KEY(BaseWeapon)];

		if (Weapon->AnimR >= WShotgunReloadType::RifleStyle)
			return Weapon_Reload(BaseWeapon, TRUE);

		if (BaseWeapon->m_pPlayer->m_rgAmmo[BaseWeapon->m_iPrimaryAmmoType] <= 0)
			return;

		if (BaseWeapon->m_iClip >= Weapon->Clip)
			return;

		if (BaseWeapon->m_fInSpecialReload == 0)
		{
			SetAnimation2(BaseWeapon->m_pPlayer->edict(), PLAYER_RELOAD);
			SendWeaponAnim(BaseWeapon, XM1014_START_RELOAD);

			BaseWeapon->m_fInSpecialReload = 1;
			BaseWeapon->m_flNextSecondaryAttack = BaseWeapon->m_flTimeWeaponIdle = BaseWeapon->m_pPlayer->m_flNextAttack = 0.55f;
			BaseWeapon->m_flNextPrimaryAttack = 0.55f;
		}
		else if (BaseWeapon->m_fInSpecialReload == 1)
		{
			if (BaseWeapon->m_flTimeWeaponIdle > 0.f)
				return;

			BaseWeapon->m_fInSpecialReload = 2;

			if (!(Weapon->Flags & WFlag::ShotgunCustomReloadSound))
			{
				if (RANDOM_LONG(0, 1))
					EMIT_SOUND(ENT(BaseWeapon->m_pPlayer->pev), CHAN_ITEM, "weapons/reload1.wav", VOL_NORM, ATTN_NORM, 85 + RANDOM_LONG(0, 31));
				else
					EMIT_SOUND(ENT(BaseWeapon->m_pPlayer->pev), CHAN_ITEM, "weapons/reload3.wav", VOL_NORM, ATTN_NORM, 85 + RANDOM_LONG(0, 31));
			}

			SendWeaponAnim(BaseWeapon, XM1014_RELOAD);
			BaseWeapon->m_flTimeWeaponIdle = BaseWeapon->m_flNextReload = Weapon->AnimR_Duration;
		}
		else
		{
			BaseWeapon->m_iClip++;
			BaseWeapon->m_pPlayer->m_rgAmmo[BaseWeapon->m_iPrimaryAmmoType]--;
			BaseWeapon->m_fInSpecialReload = 1;
		}
	}
}

static void __fastcall Weapon_PostFrame_SecondaryAttack_Pre(CBasePlayerWeapon *BaseWeapon, int AttackIndex)
{
	BOOL InAttack2 = GET_WEAPON_A2DELAY(BaseWeapon);

	switch (AttackIndex)
	{
		case A2_None: break;
		case A2_Switch:
		{
			if (InAttack2 && BaseWeapon->m_flNextPrimaryAttack <= 0.f)
			{
				SET_WEAPON_A2DELAY(BaseWeapon, FALSE);
				SET_WEAPON_INATTACK2(BaseWeapon, !GET_WEAPON_INATTACK2(BaseWeapon));

				if (GET_WEAPON_ICON(BaseWeapon))
				{
					Weapon = &Weapons[GET_WEAPON_KEY(BaseWeapon)];

					CurrentIcon = (GET_WEAPON_INATTACK2(BaseWeapon)) ? Weapon->A2V->Icon : Weapon->Icon;
					StatusIcon(ENT(BaseWeapon->m_pPlayer->pev), ICON_ENABLED, CurrentIcon.Name, CurrentIcon.R, CurrentIcon.G, CurrentIcon.B);
				}
			}
			break;
		}
		case A2_Burst:
		{
			if (!GET_WEAPON_INBURST(BaseWeapon))
				break;

			if (GET_WEAPON_CURBURST(BaseWeapon))
			{
				if (!BaseWeapon->m_fInReload && BaseWeapon->m_iClip && gpGlobals->time - 0.025f > BaseWeapon->m_flLastFire)
					return Weapon_PrimaryAttack(BaseWeapon, NULL);
			}

			if ((1 << BaseWeapon->m_iId) & SECONDARY_WEAPONS_BIT_SUM)
				BaseWeapon->m_iShotsFired = TRUE;

			SET_WEAPON_CURBURST(BaseWeapon, NULL);
			SET_WEAPON_INBURST(BaseWeapon, FALSE);
			break;
		}
		case A2_AutoPistol:
		{
			if (GET_WEAPON_INATTACK2(BaseWeapon))
				BaseWeapon->PrimaryAttack();

			break;
		}
		case A2_KnifeAttack:
		{
			if (InAttack2 && BaseWeapon->m_flNextSecondaryAttack <= 0.f)
				Attack2_KnifeAttackPerform(BaseWeapon);

			break;
		}
	}
}

static void __fastcall Weapon_PostFrame_SecondaryAttack_Post(CBasePlayerWeapon *BaseWeapon)
{
	CBasePlayer *BasePlayer = BaseWeapon->m_pPlayer;

	if (BasePlayer->pev->button & IN_ATTACK2)
	{
		if (BaseWeapon->m_flNextSecondaryAttack > 0.f)
			return;

		BasePlayer->pev->button &= ~IN_ATTACK2;
		CWeapon *Weapon = &Weapons[GET_WEAPON_KEY(BaseWeapon)];

		if ((Weapon->Forwards[WForward::SecondaryAttackPre]) && MF_ExecuteForward(Weapon->Forwards[WForward::SecondaryAttackPre], NUM_FOR_EDICT(ENT(BaseWeapon->pev))) > WReturn::IGNORED)
			return;

		switch (Weapon->A2I)
		{
			case A2_Zoom: Attack2_Zoom(BasePlayer, Weapon->A2V->WA2_ZOOM_MODE); BaseWeapon->m_flNextSecondaryAttack = 0.3f; break;
			case A2_Switch: Attack2_Switch(BasePlayer, BaseWeapon, Weapon); break;
			case A2_Burst: Attack2_Burst(BasePlayer, BaseWeapon, Weapon); break;
			case A2_AutoPistol: SET_WEAPON_INATTACK2(BaseWeapon, TRUE); BaseWeapon->m_iShotsFired = FALSE; break;
			case A2_MultiShot: Attack2_MultiShot(BaseWeapon, Weapon); break;
			case A2_KnifeAttack: Attack2_KnifeAttack(BasePlayer, BaseWeapon, Weapon); break;
			case A2_InstaSwitch: Attack2_InstaSwitch(ENT(BasePlayer->pev), BaseWeapon, Weapon); break;
		}

		if (Weapon->Forwards[WForward::SecondaryAttackPost])
			MF_ExecuteForward(Weapon->Forwards[WForward::SecondaryAttackPost], NUM_FOR_EDICT(ENT(BaseWeapon->pev)));
	}
	else if (GET_WEAPON_ATTACK2(BaseWeapon) == A2_AutoPistol && GET_WEAPON_INATTACK2(BaseWeapon))
	{
		SET_WEAPON_INATTACK2(BaseWeapon, FALSE);
	}
}

static void __fastcall CheckWeaponAttack2(CBasePlayerWeapon *BaseWeapon)
{
	if (GET_WEAPON_ATTACK2(BaseWeapon))
	{
		Weapon_PostFrame_SecondaryAttack_Pre(BaseWeapon, GET_WEAPON_ATTACK2(BaseWeapon));
		Weapon_PostFrame_SecondaryAttack_Post(BaseWeapon);
	}
}

static void __fastcall Weapon_PostFrame(CBasePlayerWeapon *BaseWeapon, int)
{
	if (GET_CUSTOM_WEAPON(BaseWeapon))
	{
		if (BaseWeapon->m_fInReload && BaseWeapon->m_flNextPrimaryAttack <= 0.f)
		{
			int MaxClip = Weapons[GET_WEAPON_KEY(BaseWeapon)].Clip;
			int Add = Q_min(MaxClip - BaseWeapon->m_iClip, BaseWeapon->m_pPlayer->m_rgAmmo[BaseWeapon->m_iPrimaryAmmoType]);
			BaseWeapon->m_iClip += Add;
			BaseWeapon->m_pPlayer->m_rgAmmo[BaseWeapon->m_iPrimaryAmmoType] -= Add;
			BaseWeapon->m_fInReload = FALSE;
		}

		CheckWeaponAttack2(BaseWeapon);
	}

	static_cast<void(__fastcall *)(CBasePlayerWeapon *, int)>(FWeapon_PostFrame[GET_WEAPON_FID(BaseWeapon)])(BaseWeapon, NULL);
}

static void __fastcall Weapon_Idle(CBasePlayerWeapon *BaseWeapon, int)
{
	if (GET_CUSTOM_WEAPON(BaseWeapon))
	{
		Weapon = &Weapons[GET_WEAPON_KEY(BaseWeapon)];

		if (BaseWeapon->m_flTimeWeaponIdle <= 0.f)
		{
			BaseWeapon->m_flTimeWeaponIdle = 20.f;

			int Animation;

			if ((Weapon->A2I == A2_Switch && GET_WEAPON_INATTACK2(BaseWeapon)))
				Animation = Weapon->A2V->WA2_SWITCH_ANIM_IDLE;
			else if (Weapon->Flags & WFlag::CustomIdleAnim)
				Animation = BaseWeapon->m_iFamasShotsFired;
			else
				Animation = 0;

			SendWeaponAnim(BaseWeapon, Animation);
		}
	}
	else static_cast<void(__fastcall *)(CBasePlayerWeapon *, int)>(FWeapon_Idle[Rifle])(BaseWeapon, NULL);
}

static void __fastcall Weapon_IdleShotgun(CBasePlayerWeapon *BaseWeapon, int)
{
	if (GET_CUSTOM_WEAPON(BaseWeapon))
	{
		Weapon = &Weapons[GET_WEAPON_KEY(BaseWeapon)];

		if (Weapon->AnimR)
			return Weapon_Idle(BaseWeapon, NULL);

		BaseWeapon->ResetEmptySound();
		BaseWeapon->m_pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);

		if (((CXM1014 *)BaseWeapon)->m_flPumpTime && ((CXM1014 *)BaseWeapon)->m_flPumpTime < 0.f)
			((CXM1014 *)BaseWeapon)->m_flPumpTime = 0;

		if (BaseWeapon->m_flTimeWeaponIdle < 0.f)
		{
			if (BaseWeapon->m_iClip == 0 && BaseWeapon->m_fInSpecialReload == 0 && BaseWeapon->m_pPlayer->m_rgAmmo[BaseWeapon->m_iPrimaryAmmoType])
			{
				BaseWeapon->Reload();
			}
			else if (BaseWeapon->m_fInSpecialReload != 0)
			{
				if (BaseWeapon->m_iClip != Weapon->Clip && BaseWeapon->m_pPlayer->m_rgAmmo[BaseWeapon->m_iPrimaryAmmoType])
				{
					BaseWeapon->Reload();
				}
				else
				{
					SendWeaponAnim(BaseWeapon, XM1014_PUMP);

					BaseWeapon->m_fInSpecialReload = 0;
					BaseWeapon->m_flTimeWeaponIdle = 1.5f;
				}
			}
			else
			{
				SendWeaponAnim(BaseWeapon, XM1014_IDLE);
			}
		}
	}
	else static_cast<void(__fastcall *)(CBasePlayerWeapon *, int)>(FWeapon_Idle[WType::Shotgun])(BaseWeapon, NULL);
}

static void __fastcall Weapon_Holster(CBasePlayerWeapon *BaseWeapon, int, int SkipLocal)
{
	static_cast<void(__fastcall *)(CBasePlayerWeapon *, int, int)>(FWeapon_Holster[GET_WEAPON_FID(BaseWeapon)])(BaseWeapon, NULL, SkipLocal);

	if (!GET_CUSTOM_WEAPON(BaseWeapon))
		return;

	Weapon = &Weapons[GET_WEAPON_KEY(BaseWeapon)];

	if (Weapon->A2I == A2_KnifeAttack && GET_WEAPON_A2DELAY(BaseWeapon))
		STOP_SOUND(ENT(BaseWeapon->pev), CHAN_VOICE, Weapon->A2V->WA2_KNIFEATTACK_SOUND);

	if (Weapon->Flags & SwitchMode_BarTime && GET_WEAPON_INATTACK2(BaseWeapon))
		BarTime(ENT(BaseWeapon->m_pPlayer->pev), 0);

	SET_WEAPON_A2DELAY(BaseWeapon, FALSE);

	if (GET_WEAPON_ICON(BaseWeapon))
	{
		CurrentIcon = (Weapon->A2I == A2_Switch && GET_WEAPON_INATTACK2(BaseWeapon)) ? Weapon->A2V->Icon : Weapon->Icon;
		StatusIcon(ENT(BaseWeapon->m_pPlayer->pev), ICON_DISABLED, CurrentIcon.Name, NULL, NULL, NULL);
	}

	if (Weapon->Type == WType::Shotgun && Weapon->AnimR < WShotgunReloadType::RifleStyle)
		BaseWeapon->m_fInSpecialReload = NULL;

	int Forward;

	if ((Forward = Weapon->Forwards[WForward::HolsterPost]))
		MF_ExecuteForward(Forward, NUM_FOR_EDICT(ENT(BaseWeapon->pev)));
}

static float __fastcall Weapon_GetMaxSpeed(CBasePlayerWeapon *BaseWeapon, int)
{
	if (!GET_CUSTOM_WEAPON(BaseWeapon))
		return static_cast<float(__fastcall *)(CBasePlayerWeapon *, int)>(FWeapon_GetMaxSpeed[GET_WEAPON_FID(BaseWeapon)])(BaseWeapon, NULL);

	float Speed = Weapons[GET_WEAPON_KEY(BaseWeapon)].Speed;

	if (BaseWeapon->m_pPlayer->m_iFOV == DEFAULT_FOV)
		return Speed;

	return Speed - 40.f;
}

static int __fastcall Weapon_GetItemInfo(CBasePlayerWeapon *BaseWeapon, int, ItemInfo *II)
{
	if (GET_CUSTOM_WEAPON(BaseWeapon))
		*II = ItemInfoes[GET_WEAPON_KEY(BaseWeapon)];
	else
		static_cast<int(__fastcall *)(CBasePlayerWeapon *, int, ItemInfo *)>(FWeapon_GetItemInfo[GET_WEAPON_FID(BaseWeapon)])(BaseWeapon, NULL, II);

	return TRUE;
}

static int __fastcall Player_GiveAmmo(CBasePlayer *BasePlayer, int, int Amount, const char *Name, int Max);
static int __fastcall Weapon_ExtractAmmo(CBasePlayerWeapon *BaseWeapon, int, CBasePlayerWeapon *Original)
{
	if (!BaseWeapon->m_iDefaultAmmo)
		return NULL;

	if (GET_CUSTOM_WEAPON(BaseWeapon))
		return Player_GiveAmmo(BaseWeapon->m_pPlayer, NULL, Ammos[BaseWeapon->m_iPrimaryAmmoType].Amount, Ammos[BaseWeapon->m_iPrimaryAmmoType].Name, -1);
	else
		return static_cast<int(__fastcall *)(CBasePlayerWeapon *, int, CBasePlayerWeapon *)>(FWeapon_ExtractAmmo[GET_WEAPON_FID(BaseWeapon)])(BaseWeapon, NULL, Original);
}

static void __fastcall Weapon_SendWeaponAnim(CBasePlayerWeapon *BaseWeapon, int, int Anim, int SkipLocal)
{
	if (!GET_CUSTOM_WEAPON(BaseWeapon) || !Weapons[GET_WEAPON_KEY(BaseWeapon)].AnimR)
		SendWeaponAnim(BaseWeapon, Anim);
}

static void SendWeaponAnim(CBasePlayerWeapon *BaseWeapon, int Anim)
{
	entvars_t *PlayerEntVars = BaseWeapon->m_pPlayer->pev;
	PlayerEntVars->weaponanim = Anim;

	MESSAGE_BEGIN(MSG_ONE, SVC_WEAPONANIM, NULL, ENT(PlayerEntVars));
	WRITE_BYTE(Anim);
	WRITE_BYTE(0);
	MESSAGE_END();
}

static void GetUserOrigin(edict_t *pEntity, int Mode, float Output[3])
{
	Vector vPos = pEntity->v.origin;

	if (Mode && Mode != 2)
		vPos = vPos + pEntity->v.view_ofs;

	if (Mode > 1)
	{
		Vector vAngleVec;
		Vector vAngle = pEntity->v.v_angle;
		float fAngle[3];

		fAngle[0] = vAngle.x;
		fAngle[1] = vAngle.y;
		fAngle[2] = vAngle.z;

		ANGLEVECTORS(fAngle, vAngleVec, NULL, NULL);
		TraceResult trEnd;
		Vector vDest = vPos + vAngleVec * 14189.f;

		float fPos[3];
		fPos[0] = vPos.x;
		fPos[1] = vPos.y;
		fPos[2] = vPos.z;

		float fDest[3];
		fDest[0] = vDest.x;
		fDest[1] = vDest.y;
		fDest[2] = vDest.z;

		TRACE_LINE(fPos, fDest, 0, pEntity, &trEnd);
		vPos = (trEnd.flFraction < 1.f) ? trEnd.vecEndPos : Vector(0.f, 0.f, 0.f);
	}

	Output = vPos;
}

static void GetWeaponAttachment(edict_t *PlayerEdict, Vector OutPut, float fDis = 40.f)
{
	Vector End, Origin = PlayerEdict->v.origin, Angle = PlayerEdict->v.view_ofs;
	GetUserOrigin(PlayerEdict, 3, End);
	VectorAdd(Origin, Angle, Origin);
	Vector Attack;
	VectorSub(End, Origin, Attack);
	VectorSub(End, Origin, Attack);
	float Rate = fDis / Attack.Length();
	VectorMulScalar(Attack, Rate, Attack);
	VectorAdd(Origin, Attack, OutPut);
}

static void __fastcall Player_TakeDamage(CBasePlayer *BasePlayer, int, entvars_t *Inflictor, entvars_t *Attacker, float Damage, int DamageBits)
{
	edict_t *PlayerEdict = ENT(Attacker);
	CBasePlayerItem *BaseItem = NULL;
	Weapon = NULL;

	if (MF_IsPlayerValid(NUM_FOR_EDICT(PlayerEdict)))
		BaseItem = ((CBasePlayer *)PlayerEdict->pvPrivateData)->m_pActiveItem;

	if (BaseItem && GET_CUSTOM_WEAPON(BaseItem) && (DamageBits & DMG_BULLET))
	{
		Weapon = &Weapons[GET_WEAPON_KEY(BaseItem)];
		Damage *= (Weapon->A2I == A2_Switch && GET_WEAPON_INATTACK2(BaseItem)) ? Weapon->A2V->WA2_SWITCH_DAMAGE : Weapon->Damage;
	}

	static_cast<void(__fastcall *)(CBasePlayer *, int, entvars_t *, entvars_t *, float, int)>(BasePlayer->IsBot() ? FPlayerBot_TakeDamage : FPlayer_TakeDamage)(BasePlayer, 0, Inflictor, Attacker, Damage, DamageBits);

	if (Weapon)
	{
		if (Weapon->Forwards[WForward::DamagePost])
			MF_ExecuteForward(Weapon->Forwards[WForward::DamagePost], NUM_FOR_EDICT(ENT(BaseItem->pev)), Damage);
	}
}

static int GetAmmoByName(const char *Name)
{
	for (size_t Index = 1; Index < Ammos.length(); Index++)
	{
		if (!Q_stricmp(Name, Ammos[Index].Name))
			return Index;
	}

	return -1;
}

static BOOL CanHaveAmmo(CBasePlayer *BasePlayer, int AmmoID)
{
	if (Ammos[AmmoID].Max < 0 || BasePlayer->m_rgAmmo[AmmoID] < Ammos[AmmoID].Max)
		return TRUE;

	return FALSE;
}

static void TabulateAmmo(CBasePlayer *BasePlayer)
{
	BasePlayer->ammo_buckshot = INT_MAX;
	BasePlayer->ammo_9mm = INT_MAX;
	BasePlayer->ammo_556nato = INT_MAX;
	BasePlayer->ammo_556natobox = INT_MAX;
	BasePlayer->ammo_762nato = INT_MAX;
	BasePlayer->ammo_45acp = INT_MAX;
	BasePlayer->ammo_50ae = INT_MAX;
	BasePlayer->ammo_338mag = INT_MAX;
	BasePlayer->ammo_57mm = INT_MAX;
	BasePlayer->ammo_357sig = INT_MAX;
}

static int __fastcall Player_GiveAmmoByID(CBasePlayer *BasePlayer, int AmmoID, int Amount)
{
	if (AmmoID < 0)
		return -1;

	if (!CanHaveAmmo(BasePlayer, AmmoID))
		return -1;

	int Add = Ammos[AmmoID].Max == -1 ? 1 : Q_min(Amount, Ammos[AmmoID].Max - BasePlayer->m_rgAmmo[AmmoID]);

	if (Add < 1)
		return -1;

	BasePlayer->m_rgAmmo[AmmoID] += Add;
	MESSAGE_BEGIN(MSG_ONE, MESSAGE_AMMOPICKUP, NULL, ENT(BasePlayer->pev));
	WRITE_BYTE(AmmoID);
	WRITE_BYTE(Add);
	MESSAGE_END();
	EMIT_SOUND(ENT(BasePlayer->pev), CHAN_ITEM, "items/9mmclip1.wav");
	TabulateAmmo(BasePlayer);
	return AmmoID;
}

static int __fastcall Player_GiveAmmo(CBasePlayer *BasePlayer, int, int Amount, const char *Name, int Max)
{
	if (BasePlayer->pev->flags & FL_SPECTATOR)
		return -1;

	if (!Name)
		return -1;

	int AmmoID = GetAmmoByName(Name);

	return Player_GiveAmmoByID(BasePlayer, AmmoID, Amount);
}

static void __fastcall WeaponBox_Spawn(CWeaponBox *WeaponBox, int)
{
	WeaponBox->pev->skin = -65536;
	static_cast<void(__fastcall *)(CWeaponBox *WeaponBox)>(FWeaponBox_Spawn)(WeaponBox);
}

static void TraceAttackContinue(CBaseEntity *BaseEntity, entvars_t *AttackerVars, float Damage, Vector Direction, TraceResult *TR, int DamageBits);
static void __fastcall TraceAttackPlayer(CBaseEntity *BaseEntity, int, entvars_t *AttackerVars, float Damage, Vector Direction, TraceResult *TR, int DamageBits)
{
	static_cast<void(__fastcall *)(CBaseEntity *, int, entvars_t *, float, Vector, TraceResult *, int)>(FTraceAttackPlayer)(BaseEntity, 0, AttackerVars, Damage, Direction, TR, DamageBits);
	TraceAttackContinue(BaseEntity, AttackerVars, Damage, Direction, TR, DamageBits);
}

static void __fastcall TraceAttack(CBaseEntity *BaseEntity, int, entvars_t *AttackerVars, float Damage, Vector Direction, TraceResult *TR, int DamageBits)
{
	static_cast<void(__fastcall *)(CBaseEntity *, int, entvars_t *, float, Vector, TraceResult *, int)>(FTraceAttackEntity)(BaseEntity, 0, AttackerVars, Damage, Direction, TR, DamageBits);
	TraceAttackContinue(BaseEntity, AttackerVars, Damage, Direction, TR, DamageBits);
}

static void TraceAttackContinue(CBaseEntity *BaseEntity, entvars_t *AttackerVars, float Damage, Vector Direction, TraceResult *TR, int DamageBits)
{
	if (!AttackerVars || !HasPrivateData(ENT(AttackerVars)))
		return;

	CBaseEntity *BaseAttacker = (CBaseEntity *)ENT(AttackerVars)->pvPrivateData;

	if (!BaseAttacker->IsPlayer())
		return;

	CBasePlayerWeapon *BaseWeapon = (CBasePlayerWeapon *)((CBasePlayer *)BaseAttacker)->m_pActiveItem;

	if (!GET_CUSTOM_WEAPON(BaseWeapon))
		return;

	Weapon = &Weapons[GET_WEAPON_KEY(BaseWeapon)];

	if (!(Weapon->Flags & WEAPON_FLAG_NODECAL))
	{
		if (BaseEntity->IsPlayer())
		{
			/*MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
			WRITE_BYTE(TE_BLOODSPRITE);
			WRITE_COORD(TR->vecEndPos.x);
			WRITE_COORD(TR->vecEndPos.y);
			WRITE_COORD(TR->vecEndPos.z);
			WRITE_SHORT(BloodSprayIndex);
			WRITE_SHORT(BloodDropIndex);
			WRITE_BYTE(BaseEntity->pev->colormap);
			WRITE_BYTE(RANDOM_LONG(4, 7));
			MESSAGE_END();

			if (TR->iHitgroup & HITGROUP_HEAD)
			UTIL_BloodStream(TR->vecEndPos, Direction, 223, (Damage * 5) + RANDOM_LONG(0, 100));
			*/
			//BloodSplat(ptr->vecEndPos, vecDir, ptr->iHitgroup, flDamage * 5);
			//SpawnBlood(ptr->vecEndPos, BloodColor(), flDamage);			// a little surface blood.
			//TraceBleed(flDamage, vecDir, ptr, bitsDamageType);

			/*MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
			WRITE_BYTE(TE_BLOODSTREAM);
			WRITE_COORD(EndPos[0]);
			WRITE_COORD(EndPos[1]);
			WRITE_COORD(EndPos[2]);
			WRITE_COORD(RANDOM_FLOAT(0.5f, 5.f));
			WRITE_COORD(RANDOM_FLOAT(0.5f, 5.f));
			WRITE_COORD(RANDOM_FLOAT(0.5f, 5.f));
			WRITE_BYTE(BaseEntity->pev->colormap);
			WRITE_BYTE(RANDOM_LONG(4, 7));
			MESSAGE_END();*/
		}
		else
		{

			MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
			WRITE_BYTE(TE_GUNSHOTDECAL);
			WRITE_COORD(TR->vecEndPos.x);
			WRITE_COORD(TR->vecEndPos.y);
			WRITE_COORD(TR->vecEndPos.z);
			WRITE_SHORT(NUM_FOR_EDICT(TR->pHit));
			WRITE_BYTE(GUNSHOT_DECALS[RANDOM_LONG(0, 4)]);
			MESSAGE_END();

			MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, TR->vecEndPos);
			WRITE_BYTE(TE_STREAK_SPLASH);
			WRITE_COORD(TR->vecEndPos.x);
			WRITE_COORD(TR->vecEndPos.y);
			WRITE_COORD(TR->vecEndPos.z);
			WRITE_COORD(TR->vecPlaneNormal.x);
			WRITE_COORD(TR->vecPlaneNormal.y);
			WRITE_COORD(TR->vecPlaneNormal.z);
			WRITE_BYTE(4);
			WRITE_SHORT(RANDOM_LONG(25, 30));
			WRITE_SHORT(20);
			WRITE_SHORT(RANDOM_LONG(77, 85));
			MESSAGE_END();
		}
	}

	// THIS IS NOT GOOD
	/*if (!(Weapon->Flags & WEAPON_FLAG_NOSMOKE))
	{
	if (!BaseEntity->IsAlive())
	{
	MESSAGE_BEGIN(MSG_PAS, SVC_TEMPENTITY, TR->vecEndPos);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(TR->vecEndPos[0] + (9.f * TR->vecPlaneNormal[0]));
	WRITE_COORD(TR->vecEndPos[1] + (9.f * TR->vecPlaneNormal[1]));
	WRITE_COORD(TR->vecEndPos[2] - 10.f + (4.f * TR->vecPlaneNormal[2]));
	WRITE_SHORT(SmokeIndex[RANDOM_LONG(0, 2)]);
	WRITE_BYTE(2);
	WRITE_BYTE(40);
	WRITE_BYTE(TE_EXPLFLAG_NODLIGHTS | TE_EXPLFLAG_NOSOUND | TE_EXPLFLAG_NOPARTICLES);
	MESSAGE_END();
	}
	}*/

	//TRV *TRValues = (Weapon->A2I == A2_Switch && GET_WEAPON_INATTACK2(BaseWeapon)) ? Weapon->A2V->TRV : Weapon->TRV;

	// CURRENTLY ONLY ONE TRACEATTACK IS AVAILABLE
	//if (Weapon->TRI == TR_LaserBeam)
	//	TraceAttack_LaserBeam(GET_WEAPON_OWNER(BaseWeapon), TR->vecEndPos, TRValues->Time, TRValues->Size, TRValues->RMin, TRValues->RMax, TRValues->R, TRValues->G, TRValues->B);
}

static void __fastcall ProjectileThink(CBaseEntity *BaseEntity, int)
{
	static_cast<void(__fastcall *)(CBaseEntity *BaseEntity, int)>(FProjectileThink)(BaseEntity, NULL);

	if (NOT_PROJECTILE(BaseEntity))
		return;

	MF_ExecuteForward(GET_PROJECTILE_FORWARD_TOUCH(BaseEntity), NUM_FOR_EDICT(ENT(BaseEntity->pev)));
}

void GiveWeaponByName(edict_t *PlayerEdict, const char *Name)
{
	if (!Name[0] || IS_USER_DEAD(PlayerEdict))
		return;

	BOOL NoError = FALSE;
	size_t Index;

	for (Index = 0; Index < MAX_CUSTOM_WEAPONS; Index++)
	{
		if (!strcmp(Name, Weapons[Index].Model))
		{
			NoError = TRUE;
			break;
		}
	}

	if (!NoError)
	{
		CLIENT_PRINTF(PlayerEdict, print_console, "Invalid weapon name.\n");
		return;
	}

	GiveWeapon(PlayerEdict, Index);
}

inline int *GetPlayerAmmo(CBasePlayer *BasePlayer, int AmmoID)
{
	return &BasePlayer->m_rgAmmo[AmmoID];
}

void GiveWeapon(edict_t *PlayerEdict, int Index)
{
	if (IS_USER_DEAD(PlayerEdict))
		return;

	CWeapon *Weapon = &Weapons[Index];
	edict_t *WeaponEdict = CREATE_NAMED_ENTITY(MAKE_STRING(GetWeaponName(Weapon->ID)));
	WeaponEdict->v.spawnflags |= SF_NORESPAWN;

	CBasePlayer *BasePlayer = static_cast<CBasePlayer *>(PlayerEdict->pvPrivateData);
	CBasePlayerWeapon *BaseWeapon = static_cast<CBasePlayerWeapon *>(WeaponEdict->pvPrivateData);
	CBasePlayerItem *BaseItem = BasePlayer->m_rgpPlayerItems[WEAPON_SLOT[Weapon->ID]];

	SET_CUSTOM_WEAPON(BaseWeapon, TRUE);
	int AmmoID = BaseWeapon->m_iPrimaryAmmoType = Weapon->AmmoID;

	if (BaseItem)
		UTIL_FakeClientCommand(PlayerEdict, "drop", GetWeaponName(BaseItem->m_iId));

	GET_WEAPON_KEY(BaseWeapon) = GET_WEAPON_KEY_EX(WeaponEdict) = Index;
	SET_NOISE_ENABLED(BaseWeapon);
	SET_WEAPON_CLIP(BaseWeapon, Weapon->Clip);
	SET_WEAPON_OWNER(BaseWeapon, NUM_FOR_EDICT(PlayerEdict));
	SET_WEAPON_FLAGS(BaseWeapon, Weapon->Flags);
	BaseWeapon->ResetEmptySound();

	if (Weapon->Icon.Name)
		SET_WEAPON_ICON(BaseWeapon, TRUE);

	if ((AmmoID = Weapon->A2I))
	{
		SET_WEAPON_ATTACK2(BaseWeapon, Weapon->A2I);

		switch (AmmoID)
		{
			case A2_Burst: SET_WEAPON_A2OFFSET(BaseWeapon, Weapon->A2V->WA2_BURST_VALUE); break;
			case A2_MultiShot: SET_WEAPON_A2OFFSET(BaseWeapon, Weapon->A2V->WA2_MULTISHOT_VALUE); break;
		}
	}

	MDLL_Spawn(WeaponEdict);
	BaseWeapon->m_iDefaultAmmo = NULL;
	MDLL_Touch(WeaponEdict, PlayerEdict);

	if (Weapon->Forwards[WForward::Purchase])
		MF_ExecuteForward(Weapon->Forwards[WForward::Purchase], NUM_FOR_EDICT(WeaponEdict));
}

void PlaybackEvent(int Flags, const edict_t *Invoker, unsigned short EI, float Delay, float *Origin, float *Angles, float F1, float F2, int I1, int I2, int B1, int B2)
{
	if (!Invoker || IS_USER_DEAD(Invoker))
		RETURN_META(MRES_IGNORED);

	CBasePlayerWeapon *BaseWeapon = ((CBasePlayerWeapon *)((CBasePlayer *)(Invoker->pvPrivateData))->m_pActiveItem);

	if (!BaseWeapon || !GET_CUSTOM_WEAPON(BaseWeapon))
		RETURN_META(MRES_IGNORED);

	PLAYBACK_EVENT_FULL(Flags | FEV_HOSTONLY, Invoker, EI, Delay, Origin, Angles, F1, F2, I1, I2, B1, B2);
	RETURN_META(MRES_SUPERCEDE);
}

void SetModel(edict_t *Edict, const char *Model)
{
	if (Edict->v.skin != -65536)
		RETURN_META(MRES_IGNORED);

	CWeaponBox *WeaponBox = (CWeaponBox *)Edict->pvPrivateData;
	CBasePlayerItem *BaseItem = WeaponBox->m_rgpPlayerItems[1];

	if (!BaseItem && !(BaseItem = WeaponBox->m_rgpPlayerItems[2]))
		RETURN_META(MRES_IGNORED);

	//CBasePlayerWeapon *BaseWeapon = (CBasePlayerWeapon *)BaseItem;

	if (!HasPrivateData(ENT(BaseItem->pev)) || !GET_CUSTOM_WEAPON(BaseItem))
		RETURN_META(MRES_IGNORED);

	Weapon = &Weapons[GET_WEAPON_KEY(BaseItem)];
	SET_MODEL(Edict, Weapon->WModel);
	SET_WEAPON_OWNER(BaseItem, NULL);
	SET_WEAPON_OWNER_ED(BaseItem, SVGame_Edicts);

	if (Weapon->Forwards[WForward::DropPost])
		MF_ExecuteForward(Weapon->Forwards[WForward::DropPost], NUM_FOR_EDICT(ENT(BaseItem->pev)));

	RETURN_META(MRES_SUPERCEDE);
}

void EmitSound(edict_t *PlayerEdict, int Channel, const char *Sample, float Volume, float Attenuation, int Flags, int Pitch)
{
	SET_META_RESULT(MRES_IGNORED);

	if (IS_USER_DEAD(PlayerEdict))
		return;

	if (Sample[0] != 'w')
		return;

	if (Sample[8] == 'r' && Sample[9] == 'e')
	{
		CBasePlayerWeapon *BaseWeapon = (CBasePlayerWeapon *)((CBasePlayer *)PlayerEdict->pvPrivateData)->m_pActiveItem;

		if (GET_CUSTOM_WEAPON(BaseWeapon) && Weapons[GET_WEAPON_KEY(BaseWeapon)].Flags & WFlag::ShotgunCustomReloadSound)
			RETURN_META(MRES_SUPERCEDE);
	}
}

void UpdateClientData_Post(const edict_s *PlayerEdict, int SendWeapons, clientdata_s *CD)
{
	CBasePlayerItem *BaseWeapon = (((CBasePlayer *)PlayerEdict->pvPrivateData)->m_pActiveItem);

	if (BaseWeapon && NOISE_DISABLED(BaseWeapon))
		CD->m_flNextAttack = gpGlobals->time + 0.001f;

	RETURN_META(MRES_IGNORED);
}

void BuyAmmo(edict_t *PlayerEdict, int Slot)
{
	CBasePlayer *BasePlayer = (CBasePlayer *)PlayerEdict->pvPrivateData;
	CBasePlayerItem *BaseItem = BasePlayer->m_rgpPlayerItems[Slot];

	if (!BaseItem)
		return;

	int *PlayerMoney = &BasePlayer->m_iAccount;
	int AmmoID = ((CBasePlayerWeapon *)BaseItem)->m_iPrimaryAmmoType;

	if (!(BasePlayer->m_signals.GetState() & SIGNAL_BUY))
		return;
	
	CAmmo *Ammo = &Ammos[AmmoID];
	
	if (BasePlayer->m_rgAmmo[AmmoID] >= Ammo->Max)
		return;
	
	if (Ammo->Cost > 0 && *PlayerMoney < Ammo->Cost)
	{
		ClientPrint(PlayerEdict, HUD_PRINTCENTER, "#Not_Enough_Money");
		goto Flash;
	}
	
	Player_GiveAmmoByID(BasePlayer, AmmoID, Ammo->Amount);
	MESSAGE_BEGIN(MSG_ONE, MESSAGE_MONEY, NULL, PlayerEdict);
	WRITE_LONG(*PlayerMoney -= Ammo->Cost);
	WRITE_BYTE(TRUE);
	MESSAGE_END();

	Flash:
	MESSAGE_BEGIN(MSG_ONE, MESSAGE_BLINKACCT, NULL, PlayerEdict);
	WRITE_BYTE(2);
	MESSAGE_END();
}

void ClientCommand(edict_t *PlayerEdict)
{
	if (IS_USER_DEAD(PlayerEdict))
		RETURN_META(MRES_IGNORED);

	SET_META_RESULT(MRES_SUPERCEDE);

	const char *Command = CMD_ARGV(0);

	if (!strnicmp(Command, "give", 4))
	{
		if (SV_Cheats)
			return GiveWeaponByName(PlayerEdict, CMD_ARGV(1));
	}
	else if (!strncmp(Command, "weapon_", 7))
	{
		for (size_t Index = NULL; Index < MAX_CUSTOM_WEAPONS; Index++)
		{
			Weapon = &Weapons[Index];

			if (!strcmp(&Command[7], Weapon->Model))
				return UTIL_FakeClientCommand(PlayerEdict, GetWeaponName(Weapon->ID));
		}
	}
	else if (!strnicmp(Command, "buyammo1", 8))
		return BuyAmmo(PlayerEdict, PRIMARY_WEAPON_SLOT);
	else if (!strnicmp(Command, "buyammo2", 8))
		return BuyAmmo(PlayerEdict, PISTOL_SLOT);

	RETURN_META(MRES_IGNORED);
}

static const char *GetWeaponName(int WeaponID)
{
	char *WeaponName;

	switch (WeaponID)
	{
		case 1: WeaponName = "weapon_p228"; break;
			//case 2: WeaponName = "weapon_shield"; break;
		case 3: WeaponName = "weapon_scout"; break;
			//case 4: WeaponName = "weapon_hegrenade"; break;
		case 5: WeaponName = "weapon_xm1014"; break;
			//case 6: WeaponName = "weapon_c4"; break;
		case 7: WeaponName = "weapon_mac10"; break;
		case 8: WeaponName = "weapon_aug"; break;
			//case 9: WeaponName = "weapon_smokegrenade"; break;
		case 10: WeaponName = "weapon_elite"; break;
		case 11: WeaponName = "weapon_fiveseven"; break;
		case 12: WeaponName = "weapon_ump45"; break;
		case 13: WeaponName = "weapon_sg550"; break;
		case 14: WeaponName = "weapon_galil"; break;
		case 15: WeaponName = "weapon_famas"; break;
		case 16: WeaponName = "weapon_usp"; break;
		case 17: WeaponName = "weapon_glock18"; break;
		case 18: WeaponName = "weapon_awp"; break;
		case 19: WeaponName = "weapon_mp5navy"; break;
		case 20: WeaponName = "weapon_m249"; break;
		case 21: WeaponName = "weapon_m3"; break;
		case 22: WeaponName = "weapon_m4a1"; break;
		case 23: WeaponName = "weapon_tmp"; break;
		case 24: WeaponName = "weapon_g3sg1"; break;
			//case 25: WeaponName = "weapon_flashbang"; break;
		case 26: WeaponName = "weapon_deagle"; break;
		case 27: WeaponName = "weapon_sg552"; break;
		case 28: WeaponName = "weapon_ak47"; break;
			//case 29: WeaponName = "weapon_knife"; break;
		case 30: WeaponName = "weapon_p90"; break;
		default: WeaponName = "";
	}

	return WeaponName;
}

static void SetWeaponHUD(edict_t *PlayerEdict, int WeaponID)
{
	MESSAGE_BEGIN(MSG_ONE, MESSAGE_WEAPONLIST, NULL, PlayerEdict);
	switch (WeaponID)
	{
		case WEAPON_P228: { WRITE_STRING("weapon_p228");  WRITE_BYTE(9);  WRITE_BYTE(52); WRITE_BYTE(-1); WRITE_BYTE(-1); WRITE_BYTE(1); WRITE_BYTE(3); WRITE_BYTE(1); WRITE_BYTE(0); break; }
		case WEAPON_XM1014: { WRITE_STRING("weapon_xm1014");  WRITE_BYTE(5);  WRITE_BYTE(32); WRITE_BYTE(-1); WRITE_BYTE(-1); WRITE_BYTE(0); WRITE_BYTE(12); WRITE_BYTE(5); WRITE_BYTE(0); break; }
		case WEAPON_AK47: {  WRITE_STRING("weapon_ak47"); WRITE_BYTE(2);  WRITE_BYTE(90); WRITE_BYTE(-1); WRITE_BYTE(-1); WRITE_BYTE(0); WRITE_BYTE(1); WRITE_BYTE(28); WRITE_BYTE(0); break; }
		case WEAPON_AWP: { WRITE_STRING("weapon_awp");  WRITE_BYTE(1);  WRITE_BYTE(30); WRITE_BYTE(-1); WRITE_BYTE(-1); WRITE_BYTE(0); WRITE_BYTE(2); WRITE_BYTE(18); WRITE_BYTE(0); break; }
	}
	MESSAGE_END();
}

static void SetWeaponHUDCustom(edict_t *PlayerEdict, CWeapon *Weapon)
{
	static char HUD[7 + MAX_MODEL_NAME] = "weapon_";
	strcpy(&HUD[7], Weapon->Model);
	MESSAGE_BEGIN(MSG_ONE, MESSAGE_WEAPONLIST, NULL, PlayerEdict);
	WRITE_STRING(HUD);
	WRITE_BYTE(Weapon->AmmoID);
	WRITE_BYTE(Ammos[Weapon->AmmoID].Max);

	switch (Weapon->ID)
	{
		case WEAPON_P228: { WRITE_BYTE(-1); WRITE_BYTE(-1); WRITE_BYTE(1); WRITE_BYTE(3); WRITE_BYTE(1); WRITE_BYTE(0); break; }
		case WEAPON_XM1014: { WRITE_BYTE(-1); WRITE_BYTE(-1); WRITE_BYTE(0); WRITE_BYTE(12); WRITE_BYTE(5); WRITE_BYTE(0); break; }
		case WEAPON_AK47: { WRITE_BYTE(-1); WRITE_BYTE(-1); WRITE_BYTE(0); WRITE_BYTE(1); WRITE_BYTE(28); WRITE_BYTE(0); break; }
		case WEAPON_AWP: { WRITE_BYTE(-1); WRITE_BYTE(-1); WRITE_BYTE(0); WRITE_BYTE(2); WRITE_BYTE(18); WRITE_BYTE(0); break; }
	}

	MESSAGE_END();
}

static void StatusIcon(edict_t *PlayerEdict, BOOL Status, const char *Icon, int R, int G, int B)
{
	MESSAGE_BEGIN(MSG_ONE, MESSAGE_STATUSICON, NULL, PlayerEdict);
	WRITE_BYTE(Status);
	WRITE_STRING(Icon);
	WRITE_BYTE(R);
	WRITE_BYTE(G);
	WRITE_BYTE(B);
	MESSAGE_END();
}

void StatusIconNumber(edict_t *PlayerEdict, BOOL Status, char Number)
{
	static char NumString[] = "number_0";
	NumString[8] = Number;

	MESSAGE_BEGIN(MSG_ONE, MESSAGE_STATUSICON, NULL, PlayerEdict);
	WRITE_BYTE(Status);
	WRITE_STRING(NumString);
	WRITE_BYTE(0);
	WRITE_BYTE(175);
	WRITE_BYTE(125);
	MESSAGE_END();
}

static void ScreenShake(edict_t *PlayerEdict, int Amplitude, int Duration, int Frequency)
{
	MESSAGE_BEGIN(MSG_ONE, MESSAGE_SCREENSHAKE, NULL, PlayerEdict);
	WRITE_SHORT(Amplitude << 12);
	WRITE_SHORT(Duration << 12);
	WRITE_SHORT(Frequency << 12);
	MESSAGE_END();
}

static void BarTime(edict_t *PlayerEdict, int Duration)
{
	MESSAGE_BEGIN(MSG_ONE, MESSAGE_BARTIME, NULL, PlayerEdict);
	WRITE_SHORT(Duration);
	MESSAGE_END();
}

static void ClientPrint(edict_t *PlayerEdict, int Type, const char *Msg)
{
	MESSAGE_BEGIN(MSG_ONE, MESSAGE_TEXTMSG, NULL, PlayerEdict);
	WRITE_BYTE(Type);
	WRITE_STRING(Msg);
	MESSAGE_END();
}

BOOL InViewCone(edict_t *PlayerEdict, Vector Origin, BOOL Accurate)
{
	Vector LOS = (Origin - (PlayerEdict->v.origin + PlayerEdict->v.view_ofs)).Normalize();

	MAKE_VECTORS(PlayerEdict->v.v_angle);

	if (DotProduct(LOS, gpGlobals->v_forward) >= cos(PlayerEdict->v.fov * (M_PI / (Accurate ? 480 : 360))))
		return TRUE;
	else
		return FALSE;
}

static edict_t *GetUserAiming(edict_t *PlayerEdict, int Distance = 32768)
{
	Vector Forward, Source = PlayerEdict->v.origin + PlayerEdict->v.view_ofs;
	ANGLEVECTORS(PlayerEdict->v.v_angle, Forward, NULL, NULL);
	TraceResult TR;
	Vector Destination = Source + Forward * Distance;
	TRACE_LINE(Source, Destination, 0, PlayerEdict, &TR);
	return TR.pHit;
}

static BOOL IsUserAlive(edict_t *PlayerEdict)
{
	return IS_USER_ALIVE(PlayerEdict);
}

static void PlayerKnockback(edict_t *VictimEdict, Vector Origin, float Knockback)
{
	Vector VDistance = (VictimEdict->v.origin - Origin);
	float FLength = VDistance.Length();
	VDistance = (VDistance / FLength) * Knockback;
	VDistance[2] = RANDOM_FLOAT(200.f, 300.f);
	VictimEdict->v.velocity = VDistance;
}

static void PerformDamage(edict_t *PlayerEdict, float Radius, float Damage, CBasePlayerWeapon *BaseWeapon, BOOL Accurate, float Knockback)
{
	Vector MyOrigin = PlayerEdict->v.origin + PlayerEdict->v.view_ofs, Origin;
	edict_t *TargetEdict = NULL;
	TraceResult TR;
	CBaseEntity *BaseEntity;
	entvars_t *PlayerEntVars = &PlayerEdict->v;

	while ((TargetEdict = FIND_ENTITY_IN_SPHERE(TargetEdict, PlayerEdict->v.origin, Radius)) != SVGame_Edicts)
	{
		if (!TargetEdict->pvPrivateData || TargetEdict == PlayerEdict)
			continue;

		if (!TargetEdict->v.takedamage)
			continue;

		BaseEntity = (CBaseEntity *)TargetEdict->pvPrivateData;
		Origin = TargetEdict->v.origin;

		if (!BaseEntity->IsPlayer())
			Origin = Origin + (TargetEdict->v.mins + TargetEdict->v.maxs) * 0.5f;

		if (!InViewCone(PlayerEdict, Origin, Accurate))
			continue;

		if (!(GET_WEAPON_FLAGS(BaseWeapon) & WFlag::KnifeAttack_Penetration))
		{
			TRACE_LINE(MyOrigin, Origin, 0, PlayerEdict, &TR);

			if (TR.pHit != TargetEdict)
				continue;
		}

		BaseEntity->TakeDamage(PlayerEntVars, PlayerEntVars, Damage, DMG_SLASH);

		if (!(GET_WEAPON_FLAGS(BaseWeapon) & WFlag::KnifeAttack_Knockback))
			continue;

		if (BaseEntity->IsPlayer())
		{
			if (!MP_FriendlyFire && *((int *)BaseEntity + 114) == *((int *)BaseWeapon->m_pPlayer + 114))
				continue;

			PlayerKnockback(TargetEdict, MyOrigin, Knockback);
		}
	}

}

/* ===================================================== */
/* =================== ATTACK 2 ======================== */
/* ===================================================== */

static void Attack2_Zoom(CBasePlayer *BasePlayer, int ZoomType)
{
	int FOV = BasePlayer->m_iFOV;

	switch (ZoomType)
	{
		case Zoom_Rifle:
		{
			switch (FOV)
			{
				case CS_NO_ZOOM: FOV = CS_AUGSG552_ZOOM; break;
				case CS_AUGSG552_ZOOM: FOV = CS_NO_ZOOM; break;
			}
			break;
		}
		case Zoom_SniperF:
		{
			switch (FOV)
			{
				case CS_NO_ZOOM: FOV = CS_FIRST_ZOOM; break;
				case CS_FIRST_ZOOM: FOV = CS_NO_ZOOM; break;
			}
			break;
		}
		case Zoom_SniperS:
		{
			switch (FOV)
			{
				case CS_NO_ZOOM: FOV = CS_SECOND_AWP_ZOOM; break;
				case CS_SECOND_AWP_ZOOM: FOV = CS_NO_ZOOM; break;
			}
			break;
		}
		case Zoom_SniperB:
		{
			switch (FOV)
			{
				case CS_NO_ZOOM: FOV = CS_FIRST_ZOOM; break;
				case CS_FIRST_ZOOM: FOV = CS_SECOND_AWP_ZOOM; break;
				case CS_SECOND_AWP_ZOOM: FOV = CS_NO_ZOOM; break;
			}
			break;
		}
	}

	BasePlayer->m_iFOV = FOV;
	CLIENT_COMMAND(ENT(BasePlayer->pev), "spk weapons/zoom\n");
}

static void Attack2_Switch(CBasePlayer *BasePlayer, CBasePlayerWeapon *BaseWeapon, CWeapon *Weapon)
{
	edict_t *PlayerEdict = ENT(BasePlayer->pev);

	switch (GET_WEAPON_INATTACK2(BaseWeapon))
	{
		case FALSE:
		{
			if (GET_WEAPON_ICON(BaseWeapon))
				StatusIcon(PlayerEdict, ICON_FLASH, Weapon->Icon.Name, Weapon->Icon.R, Weapon->Icon.G, Weapon->Icon.B);

			SendWeaponAnim(BaseWeapon, Weapon->A2V->WA2_SWITCH_ANIM_A);
			BaseWeapon->m_flNextReload = BaseWeapon->m_flNextPrimaryAttack = BaseWeapon->m_flNextSecondaryAttack = BaseWeapon->m_flTimeWeaponIdle = Weapon->A2V->WA2_SWITCH_ANIM_A_DURATION;

			if (!(Weapon->Flags & SwitchMode_NoText))
				CLIENT_PRINT(PlayerEdict, print_center, "Switch to Mode B");

			break;
		}
		case TRUE:
		{
			if (GET_WEAPON_ICON(BaseWeapon))
				StatusIcon(PlayerEdict, ICON_FLASH, Weapon->A2V->Icon.Name, Weapon->A2V->Icon.R, Weapon->A2V->Icon.G, Weapon->A2V->Icon.B);

			SendWeaponAnim(BaseWeapon, Weapon->A2V->WA2_SWITCH_ANIM_B);
			BaseWeapon->m_flNextReload = BaseWeapon->m_flNextPrimaryAttack = BaseWeapon->m_flNextSecondaryAttack = BaseWeapon->m_flTimeWeaponIdle = Weapon->A2V->WA2_SWITCH_ANIM_B_DURATION;

			if (!(Weapon->Flags & SwitchMode_NoText))
				CLIENT_PRINT(PlayerEdict, print_center, "Switch to Mode B");

			break;
		}
	}

	if (Weapon->Flags & SwitchMode_BarTime)
		BarTime(PlayerEdict, BaseWeapon->m_flNextPrimaryAttack);

	SET_WEAPON_A2DELAY(BaseWeapon, TRUE);
}

static void Attack2_Burst(CBasePlayer *BasePlayer, CBasePlayerWeapon *BaseWeapon, CWeapon *Weapon)
{
	entvars_t *PlayerEntVars = BasePlayer->pev;

	switch (GET_WEAPON_INATTACK2(BaseWeapon))
	{
		case FALSE:
		{
			SET_WEAPON_INATTACK2(BaseWeapon, TRUE);
			ClientPrint(ENT(PlayerEntVars), HUD_PRINTCENTER, "#Switch_To_BurstFire");
			break;
		}
		case TRUE:
		{
			SET_WEAPON_INATTACK2(BaseWeapon, FALSE);

			if ((1 << BaseWeapon->m_iId) & SECONDARY_WEAPONS_BIT_SUM)
				ClientPrint(ENT(PlayerEntVars), HUD_PRINTCENTER, "#Switch_To_SemiAuto");
			else
				ClientPrint(ENT(PlayerEntVars), HUD_PRINTCENTER, "#Switch_To_FullAuto");
			break;
		}
	}

	BaseWeapon->m_flNextSecondaryAttack = 0.2f;
}

static void Attack2_MultiShot(CBasePlayerWeapon *BaseWeapon, CWeapon *Weapon)
{
	if (BaseWeapon->m_flNextPrimaryAttack > 0.f)
		return;

	SET_WEAPON_INATTACK2(BaseWeapon, TRUE);

	for (int Index = 0; Index < GET_WEAPON_A2OFFSET(BaseWeapon); Index++)
	{
		if (BaseWeapon->m_iClip > 0)
			BaseWeapon->PrimaryAttack();
	}

	BaseWeapon->m_iShotsFired = TRUE;
	BaseWeapon->m_flNextSecondaryAttack = Weapon->Delay;
	SET_WEAPON_INATTACK2(BaseWeapon, FALSE);
}

static void Attack2_KnifeAttack(CBasePlayer *BasePlayer, CBasePlayerWeapon *BaseWeapon, CWeapon *Weapon)
{
	BaseWeapon->m_flNextSecondaryAttack = Weapon->A2V->WA2_KNIFEATTACK_DELAY;
	BaseWeapon->m_flTimeWeaponIdle = BaseWeapon->m_flNextPrimaryAttack = Weapon->A2V->WA2_KNIFEATTACK_DURATION;
	SendWeaponAnim(BaseWeapon, Weapon->A2V->WA2_KNIFEATTACK_ANIMATION);

	SET_WEAPON_A2DELAY(BaseWeapon, TRUE);
	EMIT_SOUND(ENT(BaseWeapon->pev), CHAN_VOICE, Weapon->A2V->WA2_KNIFEATTACK_SOUND);
}

static void Attack2_KnifeAttackPerform(CBasePlayerWeapon *BaseWeapon)
{
	SET_WEAPON_A2DELAY(BaseWeapon, FALSE);
	BaseWeapon->m_flNextSecondaryAttack = BaseWeapon->m_flNextPrimaryAttack;

	CBasePlayer *BasePlayer = BaseWeapon->m_pPlayer;
	Weapon = &Weapons[GET_WEAPON_KEY(BaseWeapon)];

	if (Weapon->Flags & WFlag::KnifeAttack_ScreenShake)
		ScreenShake(ENT(BasePlayer->pev), 4, 4, 4);

	PerformDamage(ENT(BasePlayer->pev), Weapon->A2V->WA2_KNIFEATTACK_RADIUS, (float)RANDOM_LONG(Weapon->A2V->WA2_KNIFEATTACK_DAMAGE_MIN, Weapon->A2V->WA2_KNIFEATTACK_DAMAGE_MAX), BaseWeapon, Weapon->Flags & WFlag::KnifeAttack_Accurate, Weapon->A2V->WA2_KNIFEATTACK_KNOCKBACK);
}

static void Attack2_InstaSwitch(edict_t *PlayerEdict, CBasePlayerWeapon *BaseWeapon, CWeapon *Weapon)
{
	switch (GET_WEAPON_INATTACK2(BaseWeapon))
	{
		case FALSE:
		{
			SET_WEAPON_INATTACK2(BaseWeapon, TRUE);
			ClientPrint(PlayerEdict, HUD_PRINTCENTER, Weapon->A2V->WA2_INSTASWITCH_NAME);
			break;
		}
		case TRUE:
		{
			SET_WEAPON_INATTACK2(BaseWeapon, FALSE);
			ClientPrint(PlayerEdict, HUD_PRINTCENTER, Weapon->A2V->WA2_INSTASWITCH_NAME2);
			break;
		}
	}

	BaseWeapon->m_flNextSecondaryAttack = 0.2f;
}

static Vector VectorToAngles(Vector InPut)
{
	float OutPut[3];
	VEC_TO_ANGLES(InPut, OutPut);
	return OutPut;
}

cell ShootProjectileTimed(edict_t *LauncherEdict, int ProjectileID)
{
	edict_t *ProjectileEdict = CREATE_NAMED_ENTITY(MAKE_STRING("info_target"));
	CBaseEntity *BaseEntity = ((CBaseEntity *)ProjectileEdict->pvPrivateData);
	entvars_t *ProjectileEntVars = BaseEntity->pev;
	entvars_t *LauncherEntVars = &LauncherEdict->v;
	CProjectile *Projectile = &Projectiles[ProjectileID];
	Vector Velocity = gpGlobals->v_forward * Projectile->Speed;
	
	ProjectileEntVars->movetype = MOVETYPE_PUSHSTEP;
	ProjectileEntVars->solid = SOLID_TRIGGER;
	SET_MODEL(ProjectileEdict, Projectile->Model);
	SET_SIZE(ProjectileEdict, Vector(0, 0, 0), Vector(0, 0, 0));
	SET_ORIGIN(ProjectileEdict, LauncherEntVars->origin + LauncherEntVars->view_ofs + gpGlobals->v_forward * 16);
	ProjectileEntVars->gravity = Projectile->Gravity;
	ProjectileEntVars->velocity = Velocity;
	ProjectileEntVars->angles = VectorToAngles(Velocity);
	ProjectileEntVars->owner = LauncherEdict;
	ProjectileEntVars->euser1 = ENT(((CBasePlayer *)LauncherEdict->pvPrivateData)->m_pActiveItem->pev);
	ProjectileEntVars->nextthink = gpGlobals->time + 2.5f;

	SET_PROJECTILE(BaseEntity);
	SET_PROJECTILE_FORWARD_TOUCH(BaseEntity, Projectile->Forward_Touch);
	return NUM_FOR_EDICT(ProjectileEdict);
}


void CallSendWeaponAnim(CBasePlayerWeapon *BaseWeapon, int Anim)
{
	SendWeaponAnim(BaseWeapon, Anim);
}

void SetClientKeyValue(int Index, char *InfoBuffer, const char *Key, const char *Value)
{
	SET_META_RESULT(MRES_IGNORED);

	if (SpecialBot_VTable)
		return;

	edict_t *PlayerEdict = MF_GetPlayerEdict(Index);

	if ((PlayerEdict->v.flags & FL_FAKECLIENT) != FL_FAKECLIENT)
	{
		const char *Auth = GETPLAYERAUTHID(PlayerEdict);

		if (Auth && (strcmp(Auth, "BOT") != 0))
			return;
	}

	if (strcmp(Key, "*bot") != 0 || strcmp(Value, "1") != 0)
		return;

	SpecialBot_VTable = *((void ***)((char *)PlayerEdict->pvPrivateData));
	HookEntityFWByVTable(SpecialBot_VTable, EO_TakeDamage, Player_TakeDamage, FPlayerBot_TakeDamage);
}

BOOL DispatchSpawn(edict_t *Entity)
{
	SET_META_RESULT(MRES_IGNORED);

	if (EntityHooked)
		return NULL;

	FPlayerKilled = GetEntityFW("player", EO_Killed);
	EntityHooked = TRUE;
	return NULL;
}

void UpdateAmmoList()
{
	for (int Index = NULL; Index < AMMO_MAX_TYPES; Index++)
		Ammos.append(DEFAULT_AMMOS[Index]);
}

BOOL DispatchSpawn_Post(edict_s *Entity)
{
	SET_META_RESULT(MRES_IGNORED);

	if (Initialized)
		return NULL;

	Initialized = TRUE;
	RingIndex = PRECACHE_MODEL("sprites/shockwave.spr");
	BloodSprayIndex = PRECACHE_MODEL("sprites/bloodspray.spr");
	BloodDropIndex = PRECACHE_MODEL("sprites/blood.spr");
	PrecacheModule();
	SV_Cheats = CVAR_GET_FLOAT("sv_cheats") != 0.0f;
	MP_FriendlyFire = CVAR_GET_FLOAT("mp_friendlyfire") != 0.0f;
	SV_Log = strchr(CVAR_GET_STRING("log"), 'on') != NULL;
	ABlood = CVAR_GET_FLOAT("violence_ablood") != 0.0f;
	HBlood = CVAR_GET_FLOAT("violence_hblood") != 0.0f;

	MaxPlayers = gpGlobals->maxClients;

	TraceAttack_Precache();

	if (!Ammos.length())
		UpdateAmmoList();

	return NULL;
}

void ServerActivate(edict_t *P1, int TotalEdicts, int TotalClients)
{
	auto PEV = VARS(SVGame_Edicts = P1);
	auto PrivateData = (byte *)(P1->pvPrivateData);

	for (size_t Index = 0; Index < 0xFFF; ++Index)
	{
		if (PEV == *(entvars_t **)(PrivateData + Index))
		{
			PEV_OFFSET = Index;
			break;
		}
	}

	RETURN_META(MRES_IGNORED);
}

void *FWeapon_SpawnFuncs[MAX_WEAPON_TYPES] = { Weapon_SpawnPistol, Weapon_SpawnShotgun, Weapon_SpawnRifle, Weapon_SpawnSniper };
void ServerActivate_Post(edict_t *pEdictList, int edictCount, int clientMax)
{
	SET_META_RESULT(MRES_IGNORED);

	// Why i need this?
	char HUD[7 + MAX_MODEL_NAME] = "weapon_";
	for (size_t Index = 0; Index < Weapons.length(); Index++)
	{
		Weapon = &Weapons[Index];
		strcpy(&HUD[7], Weapon->Model);
		MESSAGE_BEGIN(MSG_INIT, MESSAGE_WEAPONLIST);
		WRITE_STRING(HUD);
		WRITE_BYTE(Weapon->AmmoID);
		WRITE_BYTE(Ammos[Weapon->AmmoID].Max);

		switch (Weapon->ID)
		{
			case WEAPON_P228: { WRITE_BYTE(-1); WRITE_BYTE(-1); WRITE_BYTE(1); WRITE_BYTE(3); WRITE_BYTE(1); WRITE_BYTE(0); break; }
			case WEAPON_XM1014: { WRITE_BYTE(-1); WRITE_BYTE(-1); WRITE_BYTE(0); WRITE_BYTE(12); WRITE_BYTE(5); WRITE_BYTE(0); break; }
			case WEAPON_AK47: { WRITE_BYTE(-1); WRITE_BYTE(-1); WRITE_BYTE(0); WRITE_BYTE(1); WRITE_BYTE(28); WRITE_BYTE(0); break; }
			case WEAPON_AWP: { WRITE_BYTE(-1); WRITE_BYTE(-1); WRITE_BYTE(0); WRITE_BYTE(2); WRITE_BYTE(18); WRITE_BYTE(0); break; }
		}

		MESSAGE_END();
	}

	const char *WeaponName;
	for (int Index = 0; Index < MAX_WEAPON_TYPES; Index++)
	{
		WeaponName = GetWeaponName(WEAPON_TYPE_ID[Index]);

		HookEntityFW(WeaponName, EO_Spawn, FWeapon_SpawnFuncs[Index], FWeapon_Spawn[Index]);
		HookEntityFW(WeaponName, EO_Item_AddToPlayer, Weapon_AddToPlayer, FWeapon_AddToPlayer[Index]);
		HookEntityFW(WeaponName, EO_Item_Deploy, Weapon_Deploy, FWeapon_Deploy[Index]);
		HookEntityFW(WeaponName, EO_Weapon_PrimaryAttack, Weapon_PrimaryAttack, FWeapon_PrimaryAttack[Index]);
		HookEntityFW(WeaponName, EO_Weapon_SecondaryAttack, Weapon_SecondaryAttack, FWeapon_SecondaryAttack[Index]);
		HookEntityFW(WeaponName, EO_Weapon_Reload, Index == 1 ? Weapon_ReloadShotgun : Weapon_Reload, FWeapon_Reload[Index]);
		HookEntityFW(WeaponName, EO_Item_PostFrame, Weapon_PostFrame, FWeapon_PostFrame[Index]);
		HookEntityFW(WeaponName, EO_Weapon_Idle, Index == 1 ? Weapon_IdleShotgun : Weapon_Idle, FWeapon_Idle[Index]);
		HookEntityFW(WeaponName, EO_Item_Holster, Weapon_Holster, FWeapon_Holster[Index]);
		HookEntityFW(WeaponName, EO_Item_GetMaxSpeed, Weapon_GetMaxSpeed, FWeapon_GetMaxSpeed[Index]);
		HookEntityFW(WeaponName, EO_Weapon_ExtractAmmo, Weapon_ExtractAmmo, FWeapon_ExtractAmmo[Index]);
		HookEntityFW(WeaponName, EO_Weapon_SendWeaponAnim, Weapon_SendWeaponAnim, FWeapon_Dump);
	}

	FWeapon_Reload[3] = GetEntityFW("weapon_m3", EO_Weapon_Reload);
	HookEntityFW("player", EO_TakeDamage, Player_TakeDamage, FPlayer_TakeDamage);
	HookEntityFW("player", EO_GiveAmmo, Player_GiveAmmo, FPlayer_GiveAmmo);
	HookEntityFW("weaponbox", EO_Spawn, WeaponBox_Spawn, FWeaponBox_Spawn);
	HookEntityFW("worldspawn", EO_TraceAttack, TraceAttack, FTraceAttackEntity);
	HookEntityFW("func_wall", EO_TraceAttack, TraceAttack, FTraceAttackEntity);
	HookEntityFW("func_tank", EO_TraceAttack, TraceAttack, FTraceAttackEntity);
	HookEntityFW("func_breakable", EO_TraceAttack, TraceAttack, FTraceAttackEntity);
	HookEntityFW("func_door", EO_TraceAttack, TraceAttack, FTraceAttackEntity);
	HookEntityFW("func_door_rotating", EO_TraceAttack, TraceAttack, FTraceAttackEntity);
	HookEntityFW("player", EO_TraceAttack, TraceAttackPlayer, FTraceAttackPlayer);
	HookEntityFW("info_target", EO_Think, ProjectileThink, FProjectileThink);
}

void ServerDeactivate(void)
{
	Initialized = FALSE;

	for (int Index = 0; Index < WeaponCount; Index++)
	{
		Weapon = &Weapons[Index];

		if (Weapon->A2V)
		{
			//if (Weapon->A2V->TRV)
			//	delete(Weapon->A2V->TRV);

			delete(Weapon->A2V);
		}

		/*if (Weapon->TRV)
		{
		delete(Weapon->TRV);
		}*/
	}

	WeaponCount = NULL;

	Weapons.clear();
	Ammos.clear();
	Projectiles.clear();
	ItemInfoes.clear();
	AmmoCount = AMMO_MAX_TYPES;
	RETURN_META(MRES_IGNORED);
}

void OnPluginsUnloaded(void)
{
	const char *WeaponName;
	for (int Index = 0; Index < MAX_WEAPON_TYPES; Index++)
	{
		WeaponName = GetWeaponName(WEAPON_TYPE_ID[Index]);

		ResetEntityFW(WeaponName, EO_Spawn, FWeapon_SpawnFuncs[Index], FWeapon_Spawn[Index]);
		ResetEntityFW(WeaponName, EO_Item_AddToPlayer, Weapon_AddToPlayer, FWeapon_AddToPlayer[Index]);
		ResetEntityFW(WeaponName, EO_Item_Deploy, Weapon_Deploy, FWeapon_Deploy[Index]);
		ResetEntityFW(WeaponName, EO_Weapon_PrimaryAttack, Weapon_PrimaryAttack, FWeapon_PrimaryAttack[Index]);
		ResetEntityFW(WeaponName, EO_Weapon_SecondaryAttack, Weapon_SecondaryAttack, FWeapon_SecondaryAttack[Index]);
		ResetEntityFW(WeaponName, EO_Weapon_Reload, Index == 1 ? Weapon_ReloadShotgun : Weapon_Reload, FWeapon_Reload[Index]);
		ResetEntityFW(WeaponName, EO_Item_PostFrame,  Weapon_PostFrame, FWeapon_PostFrame[Index]);
		ResetEntityFW(WeaponName, EO_Weapon_Idle, Index == 1 ? Weapon_IdleShotgun : Weapon_Idle, FWeapon_Idle[Index]);
		ResetEntityFW(WeaponName, EO_Item_Holster, Weapon_Holster, FWeapon_Holster[Index]);
		ResetEntityFW(WeaponName, EO_Item_GetMaxSpeed, Weapon_GetMaxSpeed, FWeapon_GetMaxSpeed[Index]);
		ResetEntityFW(WeaponName, EO_Weapon_ExtractAmmo, Weapon_ExtractAmmo, FWeapon_ExtractAmmo[Index]);
		ResetEntityFW(WeaponName, EO_Weapon_SendWeaponAnim, Weapon_SendWeaponAnim, FWeapon_Dump);
	}

	ResetEntityFW("player", EO_TakeDamage, Player_TakeDamage, FPlayer_TakeDamage);
	ResetEntityFW("player", EO_GiveAmmo, Player_GiveAmmo, FPlayer_GiveAmmo);
	ResetEntityFW("weaponbox", EO_Spawn, WeaponBox_Spawn, FWeaponBox_Spawn);
	ResetEntityFW("worldspawn", EO_TraceAttack, TraceAttack, FTraceAttackEntity);
	ResetEntityFW("func_wall", EO_TraceAttack, TraceAttack, FTraceAttackEntity);
	ResetEntityFW("func_tank", EO_TraceAttack, TraceAttack, FTraceAttackEntity);
	ResetEntityFW("func_breakable", EO_TraceAttack, TraceAttack, FTraceAttackEntity);
	ResetEntityFW("func_door", EO_TraceAttack, TraceAttack, FTraceAttackEntity);
	ResetEntityFW("func_door_rotating", EO_TraceAttack, TraceAttack, FTraceAttackEntity);
	ResetEntityFW("player", EO_TraceAttack, TraceAttackPlayer, FTraceAttackPlayer);
	ResetEntityFW("info_target", EO_Touch, ProjectileThink, FProjectileThink);

	if (SpecialBot_VTable)
	{
		ResetEntityFWByVTable(SpecialBot_VTable, EO_TakeDamage, Player_TakeDamage, FPlayerBot_TakeDamage);
		SpecialBot_VTable = NULL;
	}
}
