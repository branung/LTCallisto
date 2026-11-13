// ----------------------------------------------------------------------- //
//
// MODULE  : LaserBeamFX.cpp
//
// PURPOSE : Special FX class for general laser beam FX using models
//
// CREATED : 8/10/98
//
// ----------------------------------------------------------------------- //

#include "LaserBeamFX.h"
#include "BloodModLasers.h"	// Mod\Lasers.txt
#include "cpp_client_de.h"
#include "ClientUtilities.h"
#include "ClientServerShared.h"
#include "BloodClientShell.h"
#include "SFXMsgIds.h"
#include "ExplosionFX.h"

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CLaserBeamFX::Init
//
//	PURPOSE:	Init the stream
//
// ----------------------------------------------------------------------- //

DBOOL CLaserBeamFX::Init(SFXCREATESTRUCT* psfxCreateStruct)
{
	if (!CSpecialFX::Init(psfxCreateStruct)) return DFALSE;

	LASERBEAMCREATESTRUCT	*pLS = (LASERBEAMCREATESTRUCT*)psfxCreateStruct;

	VEC_COPY(m_vSource, pLS->vSource);
	VEC_COPY(m_vDest, pLS->vDest);
	m_nType = pLS->nType;

	m_fStartTime = 0.0f;
	m_bFirstUpdate = 1;
	SetupBeam();
	return DTRUE;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CLaserBeamFX::CreateObject
//
//	PURPOSE:	Create object associated the model
//
// ----------------------------------------------------------------------- //

DBOOL CLaserBeamFX::CreateObject(CClientDE *pClientDE)
{
	if (!pClientDE ) return DFALSE;
	m_pClientDE = pClientDE;

	ObjectCreateStruct	ocStruct;
	INIT_OBJECTCREATESTRUCT(ocStruct);

	ocStruct.m_ObjectType = OT_MODEL;
	ocStruct.m_Flags = FLAG_VISIBLE;

	if(m_hstrModel)		_mbscpy((unsigned char*)ocStruct.m_Filename, (const unsigned char*)m_hstrModel);
		else			_mbscpy((unsigned char*)ocStruct.m_Filename, (const unsigned char*)"Models\\Ammo\\beam.abc");

	if(m_hstrTexture)	_mbscpy((unsigned char*)ocStruct.m_SkinName, (const unsigned char*)m_hstrTexture);
		else			_mbscpy((unsigned char*)ocStruct.m_SkinName, (const unsigned char*)"Skins\\Ammo\\beamred.dtx");

	m_hObject = pClientDE->CreateObject(&ocStruct);

	// Gouraud shade and make full bright...
	DDWORD dwFlags = pClientDE->GetObjectFlags(m_hObject);
	pClientDE->SetObjectFlags(m_hObject, dwFlags | FLAG_MODELGOURAUDSHADE | FLAG_NOLIGHT);

	DFLOAT	r, g, b, a;
	pClientDE->GetObjectColor(m_hObject, &r, &g, &b, &a);
	m_pClientDE->SetObjectColor(m_hObject, r, g, b, m_fAlpha);

	return DTRUE;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CLaserBeamFX::Update
//
//	PURPOSE:	Update the particles
//
// ----------------------------------------------------------------------- //

DBOOL CLaserBeamFX::Update()
{
	if(!m_pClientDE)	return DFALSE;

	DFLOAT	fTime = m_pClientDE->GetTime() - m_fStartTime;

	// All lasers are now burst beams... no constant beams anymore
	if(m_bFirstUpdate)
	{
		m_fStartTime = m_pClientDE->GetTime();
		fTime = 0.0f;
		m_bFirstUpdate = 0;
		UpdateBeam();
	}

	// Scale the beam to the proper width (the length will not change in this type
	if(fTime <= m_fScaleTime)
	{
		DFLOAT	scale = m_fMinScale + ((m_fMaxScale - m_fMinScale) * (fTime / m_fScaleTime));
		VEC_SET(m_vScale, scale, scale, m_vScale.z);
	}
	else
		{ VEC_SET(m_vScale, m_fMaxScale, m_fMaxScale, m_vScale.z); }

	m_pClientDE->SetObjectScale(m_hObject, &m_vScale);

	// Set the alpha for the object
	DFLOAT	r, g, b, curAlpha;
	m_pClientDE->GetObjectColor(m_hObject, &r, &g, &b, &curAlpha);

	if(fTime >= m_fDuration - m_fFadeTime)
	{
		DFLOAT	timeRatio = (fTime - (m_fDuration - m_fFadeTime)) / m_fFadeTime;
		curAlpha = m_fAlpha - (m_fAlpha * timeRatio);
	}
	else
	{
		curAlpha = m_fAlpha;
	}

	m_pClientDE->SetObjectColor(m_hObject, r, g, b, curAlpha);

	return (fTime < m_fDuration);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CLaserBeamFX::SetupBeam()
//
//	PURPOSE:	Update the beam model
//
// ----------------------------------------------------------------------- //

void CLaserBeamFX::SetupBeam()
{
	// Each laser's shape and skin (Mod\Lasers.txt, Shared/BloodModLasers.h): retail's two switches,
	// transcribed there, or the mod's. m_hstrModel and m_hstrTexture point into the table, which keeps
	// its strings for the module's life.
	if (const LaserRecipe* pLaser = BloodModLaser_Get(m_nType))
	{
		m_fMinScale			= pLaser->fMinScale;
		m_fMaxScale			= pLaser->fMaxScale;
		m_fDuration			= pLaser->fDuration;
		m_fScaleTime		= pLaser->fScaleTime;
		m_fFadeTime			= pLaser->fFadeTime;
		m_fAlpha			= pLaser->fAlpha;
		m_bWaveForm			= pLaser->bWaveForm ? DTRUE : DFALSE;
		m_hstrModel			= pLaser->szModel;
		m_hstrTexture		= pLaser->szSkin;
		return;
	}

	// An id no laser has: retail's default case of both switches.
	m_fMinScale			= 0.0f;
	m_fMaxScale			= 1.0f;
	m_fDuration			= 0.5f;
	m_fScaleTime		= 0.25f;
	m_fFadeTime			= 0.25f;
	m_fAlpha			= 0.5f;
	m_bWaveForm			= 0;
	m_hstrModel			= "Models\\Ammo\\beam.abc";
	m_hstrTexture		= "Skins\\Ammo\\beamred.dtx";
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CLaserBeamFX::UpdateBeam()
//
//	PURPOSE:	Update the beam model
//
// ----------------------------------------------------------------------- //

DBOOL CLaserBeamFX::UpdateBeam()
{
	if(!m_pClientDE)	return DFALSE;

	DVector		vDist, vUp;
	DRotation	rRot;

	VEC_SET(vUp, 0.0f, 1.0f, 0.0f);
	VEC_SUB(vDist, m_vDest, m_vSource);

	m_pClientDE->AlignRotation(&rRot, &vDist, &vUp);
	m_pClientDE->SetObjectPos(m_hObject, &m_vSource);
	m_pClientDE->SetObjectRotation(m_hObject, &rRot);

	VEC_SET(m_vScale, m_fMinScale, m_fMinScale, VEC_MAG(vDist));
	return	DTRUE;
}