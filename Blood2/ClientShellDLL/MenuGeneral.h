// MenuGeneral.h: interface for the CMenuGeneral class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MENUGENERAL_H__INCLUDED_)
#define AFX_MENUGENERAL_H__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "MenuBase.h"

class CMenuGeneral : public CMenuBase
{
public:
	CMenuGeneral();
	virtual ~CMenuGeneral();

	// Build the menu
	void	Build();

	void	OnFocus(DBOOL bFocus);

	// Port Enhancements' ON/OFF greys Customize Enhancements as it changes
	void	OnLeft();
	void	OnRight();
	DBOOL	OnLButtonDown(int x, int y);

protected:
	void	EnableEnhancementControls();
	void	OnEnhancementsToggled();

	// Load/Save the general settings
	void	LoadGeneralSettings();
	void	SaveGeneralSettings();

protected:
	DBOOL	m_bCrosshair;			// Show the crosshair
	DBOOL	m_bAlwaysRun;			// Always run (RunLock)
	DBOOL	m_bEnhancements;		// PortEnhancements console variable

	CLTGUIOnOffCtrl			*m_pEnhancementsCtrl;	// Port Enhancements
	CLTGUIColumnTextCtrl	*m_pCustomizeCtrl;		// Customize Enhancements...

	DDWORD	OnCommand(DDWORD dwCommand, DDWORD dwParam1, DDWORD dwParam2);
};

#endif // !defined(AFX_MENUGENERAL_H__INCLUDED_)
