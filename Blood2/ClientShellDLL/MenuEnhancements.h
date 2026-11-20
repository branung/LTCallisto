// MenuEnhancements.h: interface for the CMenuEnhancements class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MENUENHANCEMENTS_H__INCLUDED_)
#define AFX_MENUENHANCEMENTS_H__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "MenuBase.h"

class CMenuEnhancements : public CMenuBase
{
public:
	CMenuEnhancements();
	virtual ~CMenuEnhancements();

	// Build the menu
	void	Build();

	void	OnFocus(DBOOL bFocus);

protected:
	DBOOL	m_bReactions;			// EnhanceReactions console variable
	DBOOL	m_bConsistentDamage;	// EnhanceConsistentDamage console variable

	DDWORD	OnCommand(DDWORD dwCommand, DDWORD dwParam1, DDWORD dwParam2);
};

#endif // !defined(AFX_MENUENHANCEMENTS_H__INCLUDED_)
