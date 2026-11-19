// *********************************************************************** //
//
// MODULE  : ltlibraryloader.cpp
//
// PURPOSE : Win32 implementation of LTLibraryLoader.
//
// CREATED : 05/21/04
//
// (c) 2004 Monolith Productions, Inc.  All Rights Reserved
//
// *********************************************************************** //

#include "bdefs.h"
#include "syslibraryloader.h"

HLTMODULE LTLibraryLoader::OpenLibrary(const char* pszFileName)
{
	return (HLTMODULE)::LoadLibrary(pszFileName);
}

HLTMODULE LTLibraryLoader::OpenLibraryAsResource(const char* pszFileName)
{
	return (HLTMODULE)::LoadLibraryExA(pszFileName, NULL,
		LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
}

void LTLibraryLoader::CloseLibrary(const HLTMODULE hModule)
{
	::FreeLibrary((HMODULE)hModule);
}

bool LTLibraryLoader::IsLibraryLoaded(const char* pszFileName)
{
	HMODULE hModule = ::GetModuleHandle(pszFileName);
	return !!hModule;
}

HLTMODULE LTLibraryLoader::GetLibraryHandle(const char* pszFileName)
{
	return ::GetModuleHandle(pszFileName);
}

HLTMODULE LTLibraryLoader::GetMainHandle()
{
	return ::GetModuleHandle(NULL);
}

HLTPROC LTLibraryLoader::GetProcAddress(HLTMODULE hModule, const char* pszProcName)
{
	return ::GetProcAddress((HMODULE)hModule, pszProcName);
}