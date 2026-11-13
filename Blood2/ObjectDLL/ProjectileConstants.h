// ProjectileConstants.h: the projectiles' class constants a mod can change.
//
// Out of GameProjectiles.h and GameProjectiles.cpp unchanged, so that BloodModProjectiles.cpp's
// table and tests/Blood2Tables can name retail's values without the classes. The code that used
// them reads BloodModProjectile_Float and friends, which answer these unless Mod\Projectiles.txt
// says otherwise.

#ifndef __PROJECTILECONSTANTS_H__
#define __PROJECTILECONSTANTS_H__

// CGasGrenade (the Nightmare Levels')
#define		GAS_GRENADE_SMOKE_TIME		10.0f
#define		GAS_GRENADE_DAMAGE_DIST		200.0f
#define		GAS_GRENADE_MAX_DAMAGE		3.0f
#define		GAS_GRENADE_DAMAGE_DELAY	0.5f

// CDeathRayProjectile
#define		DEATHRAY_BOUNCE_DIST		1500.0f

// COrbProjectile, and COrbAltProjectile's DAMAGE_TIME
#define		THEORB_DETECT_RADIUS		175.0f
#define		THEORB_DAMAGE_TIME			5.1f

// CSingularityProjectile, and CSingularityAltProjectile, which waits twice it
#define		SINGULARITY_ATTRACT_TIME		5.0f

// CFlayerPortal (the Nightmare Levels')
#define		FLAYER_ALT_SEARCH_RANGE		800.0f
#define		FLAYER_ALT_CHAIN_INTERVAL	2.0f

// The bombs' explosions, which their AddExplosion wrote as literals: the effect (SFXMsgIds.h's
// EXP_), its sound and the sound's radius.
#define		TIMEBOMB_EXPLOSION				EXP_DEFAULT_SMALL
#define		TIMEBOMB_EXPLOSION_SOUND		"Sounds\\Weapons\\time\\explode.wav"
#define		PROXIMITYBOMB_EXPLOSION			EXP_DEFAULT_MEDIUM
#define		PROXIMITYBOMB_EXPLOSION_SOUND	"Sounds\\Weapons\\proximities\\explode.wav"
#define		REMOTEBOMB_EXPLOSION			EXP_DEFAULT_SMALL
#define		REMOTEBOMB_EXPLOSION_SOUND		"Sounds\\Weapons\\time\\explode.wav"
#define		BOMB_EXPLOSION_SOUND_RADIUS		1000.0f

// CSkullProjectile's flying sound (flyskull.wav), its radius
#define		SKULL_FLY_SOUND_RADIUS		750.0f

#endif // __PROJECTILECONSTANTS_H__
