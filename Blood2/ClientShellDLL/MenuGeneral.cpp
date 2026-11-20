// MenuGeneral.cpp: implementation of the CMenuGeneral class.
//
// A new screen added to this port. Retail's own "General Options" screen (Menu.cpp, an
// older ring menu) was never wired into the game and is not compiled here. 
// Of its seven toggles only Crosshair and Always Run (RunLock) have anything underneath 
// in the live client shell.
//
//////////////////////////////////////////////////////////////////////

#include "MenuBase.h"
#include "MainMenus.h"
#include "MenuGeneral.h"
#include "MenuCommands.h"
#include "BloodClientShell.h"
#include "ClientRes.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMenuGeneral::CMenuGeneral()
{
	m_bCrosshair = DTRUE;
	m_bAlwaysRun = DFALSE;
}

CMenuGeneral::~CMenuGeneral()
{

}

// Build the menu
void CMenuGeneral::Build()
{
	// Make sure to call the base class
	CMenuBase::Build();

	CreateTitle("interface\\mainmenus\\options.pcx", IDS_MENU_TITLE_OPTIONS, m_pMainMenus->GetTitlePos());
	SetOptionPos(m_pMainMenus->GetOptionsPos());
	SetItemSpacing(5);

	// Load the general settings
	LoadGeneralSettings();

	// Add the controls
	AddOnOffOption(IDS_MENU_GENERAL_CROSSHAIR, m_pMainMenus->GetSmallFont(), 100, &m_bCrosshair);
	AddOnOffOption(IDS_MENU_GENERAL_ALWAYSRUN, m_pMainMenus->GetSmallFont(), 100, &m_bAlwaysRun);

	UpdateData(DFALSE);
}

// Load the general settings from the live client shell state
void CMenuGeneral::LoadGeneralSettings()
{
	if (!g_pBloodClientShell)
	{
		return;
	}

	m_bCrosshair = g_pBloodClientShell->IsShowCrosshair();
	m_bAlwaysRun = g_pBloodClientShell->IsRunLock();
}

// Persist the general settings. '+' gives each variable VARFLAG_SAVE, so it is written to autoexec.cfg
void CMenuGeneral::SaveGeneralSettings()
{
	if (!m_pClientDE)
	{
		return;
	}

	char strConsole[256];

	sprintf(strConsole, "+Crosshair %f", (float)(m_bCrosshair ? 1 : 0));
	m_pClientDE->RunConsoleString(strConsole);

	sprintf(strConsole, "+RunLock %f", (float)(m_bAlwaysRun ? 1 : 0));
	m_pClientDE->RunConsoleString(strConsole);
}

void CMenuGeneral::OnFocus(DBOOL bFocus)
{
	if (!bFocus)
	{
		UpdateData(DTRUE);

		g_pBloodClientShell->SetShowCrosshair(m_bCrosshair);
		g_pBloodClientShell->SetRunLock(m_bAlwaysRun);

		SaveGeneralSettings();
	}
}

DDWORD CMenuGeneral::OnCommand(DDWORD dwCommand, DDWORD dwParam1, DDWORD dwParam2)
{
	return 0;
}
