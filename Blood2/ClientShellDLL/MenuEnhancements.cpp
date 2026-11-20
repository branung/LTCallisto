// MenuEnhancements.cpp: implementation of the CMenuEnhancements class.
//
// A new screen added to this port: General, Customize Enhancements. Each row is an ON/OFF console
// variable that counts only while General's Port Enhancements is ON, and each defaults to ON, so
// turning the master switch on gives the whole set.
//
//////////////////////////////////////////////////////////////////////

#include "MenuBase.h"
#include "MainMenus.h"
#include "MenuEnhancements.h"
#include "MenuCommands.h"
#include "BloodClientShell.h"
#include "ClientRes.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMenuEnhancements::CMenuEnhancements()
{
	m_bReactions = DTRUE;
	m_bConsistentDamage = DTRUE;
}

CMenuEnhancements::~CMenuEnhancements()
{

}

// Build the menu
void CMenuEnhancements::Build()
{
	// Make sure to call the base class
	CMenuBase::Build();

	CreateTitle("interface\\mainmenus\\options.pcx", IDS_MENU_TITLE_OPTIONS, m_pMainMenus->GetTitlePos());
	SetOptionPos(m_pMainMenus->GetOptionsPos());
	SetItemSpacing(5);

	m_bReactions = g_pBloodClientShell ? g_pBloodClientShell->IsEnhancementOn("EnhanceReactions") : DTRUE;
	m_bConsistentDamage = g_pBloodClientShell ? g_pBloodClientShell->IsEnhancementOn("EnhanceConsistentDamage") : DTRUE;

	AddOnOffOption(IDS_MENU_ENHANCEMENTS_REACTIONS, m_pMainMenus->GetSmallFont(), 100, &m_bReactions);
	AddOnOffOption(IDS_MENU_ENHANCEMENTS_DAMAGE, m_pMainMenus->GetSmallFont(), 100, &m_bConsistentDamage);

	UpdateData(DFALSE);
}

void CMenuEnhancements::OnFocus(DBOOL bFocus)
{
	if (bFocus)
	{
		if (g_pBloodClientShell)
		{
			m_bReactions = g_pBloodClientShell->IsEnhancementOn("EnhanceReactions");
			m_bConsistentDamage = g_pBloodClientShell->IsEnhancementOn("EnhanceConsistentDamage");
		}
		UpdateData(DFALSE);
		return;
	}

	UpdateData(DTRUE);

	if (m_pClientDE)
	{
		// '+' gives each variable VARFLAG_SAVE, so it is written to autoexec.cfg
		char strConsole[64];
		sprintf(strConsole, "+EnhanceReactions %f", (float)(m_bReactions ? 1 : 0));
		m_pClientDE->RunConsoleString(strConsole);
		sprintf(strConsole, "+EnhanceConsistentDamage %f", (float)(m_bConsistentDamage ? 1 : 0));
		m_pClientDE->RunConsoleString(strConsole);
	}

	if (g_pBloodClientShell) g_pBloodClientShell->SendEnhancements();
}

DDWORD CMenuEnhancements::OnCommand(DDWORD dwCommand, DDWORD dwParam1, DDWORD dwParam2)
{
	return 0;
}
