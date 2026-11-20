// MenuGeneral.cpp: implementation of the CMenuGeneral class.
//
// A new screen added to this port. Retail's own "General Options" screen (Menu.cpp, an
// older ring menu) was never wired into the game and is not compiled here. 
// Of its seven toggles only Crosshair and Always Run (RunLock) have anything underneath 
// in the live client shell.
// The port's own settings come first: Port Enhancements, the master switch (OFF plays as retail),
// and Customize Enhancements..., greyed out while it is OFF, which opens CMenuEnhancements.
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
	m_bEnhancements = DFALSE;
	m_pEnhancementsCtrl = DNULL;
	m_pCustomizeCtrl = DNULL;
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
	m_pEnhancementsCtrl = AddOnOffOption(IDS_MENU_GENERAL_ENHANCEMENTS, m_pMainMenus->GetSmallFont(), 100, &m_bEnhancements);
	// A column ending where the on/off labels end, so it lines up with them
	m_pCustomizeCtrl = AddColumnTextOption(MENU_CMD_ENHANCEMENTS, m_pMainMenus->GetSmallFont());
	if (m_pCustomizeCtrl) m_pCustomizeCtrl->AddColumn(IDS_MENU_GENERAL_CUSTOMIZE, 100 - 25, CF_JUSTIFY_RIGHT);
	AddOnOffOption(IDS_MENU_GENERAL_CROSSHAIR, m_pMainMenus->GetSmallFont(), 100, &m_bCrosshair);
	AddOnOffOption(IDS_MENU_GENERAL_ALWAYSRUN, m_pMainMenus->GetSmallFont(), 100, &m_bAlwaysRun);

	UpdateData(DFALSE);
	EnableEnhancementControls();
}

// Customize Enhancements... is live only while Port Enhancements is ON
void CMenuGeneral::EnableEnhancementControls()
{
	if (m_pCustomizeCtrl) m_pCustomizeCtrl->Enable(m_bEnhancements);
}

// After a left, right or click on Port Enhancements, take the new value at once so the entry
// under it greys or lights
void CMenuGeneral::OnEnhancementsToggled()
{
	if (!m_pEnhancementsCtrl || GetCurrentItem() != m_pEnhancementsCtrl) return;

	UpdateData(DTRUE);
	EnableEnhancementControls();
}

void CMenuGeneral::OnLeft()
{
	CMenuBase::OnLeft();
	OnEnhancementsToggled();
}

void CMenuGeneral::OnRight()
{
	CMenuBase::OnRight();
	OnEnhancementsToggled();
}

DBOOL CMenuGeneral::OnLButtonDown(int x, int y)
{
	DBOOL bHit = CMenuBase::OnLButtonDown(x, y);
	OnEnhancementsToggled();
	return bHit;
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

	m_bEnhancements = g_pBloodClientShell->IsEnhancementOn("PortEnhancements", DFALSE);
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

	sprintf(strConsole, "+PortEnhancements %f", (float)(m_bEnhancements ? 1 : 0));
	m_pClientDE->RunConsoleString(strConsole);
}

void CMenuGeneral::OnFocus(DBOOL bFocus)
{
	if (bFocus)
	{
		// The settings may have changed under the menu after Build, from the console or autoexec.cfg
		LoadGeneralSettings();
		UpdateData(DFALSE);
		EnableEnhancementControls();
		return;
	}

	UpdateData(DTRUE);

	g_pBloodClientShell->SetShowCrosshair(m_bCrosshair);
	g_pBloodClientShell->SetRunLock(m_bAlwaysRun);

	SaveGeneralSettings();

	// A game in progress takes the change now rather than at the next world
	g_pBloodClientShell->SendEnhancements();
}

DDWORD CMenuGeneral::OnCommand(DDWORD dwCommand, DDWORD dwParam1, DDWORD dwParam2)
{
	if (dwCommand == MENU_CMD_ENHANCEMENTS && m_bEnhancements)
	{
		m_pMainMenus->SetCurrentMenu(MENU_ID_ENHANCEMENTS);
	}
	return 0;
}
