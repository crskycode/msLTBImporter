// msLTBImporter.h : main header file for the msLTBImporter DLL
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols
#include "msPlugIn.h"
#include "msLib.h"


// CmsLTBImporterApp
// See msLTBImporter.cpp for the implementation of this class
//

class CMsPlugInApp : public CWinApp
{
public:
	CMsPlugInApp();

// Overrides
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};

// cPlugIn
// See msLTBImporter.cpp for the implementation of this class
//

class cPlugIn : public cMsPlugIn
{
	char szTitle[64];

public:
	cPlugIn();
	virtual ~cPlugIn();

public:
	int				GetType(void);
	const char *	GetTitle(void);
	int				Execute(msModel *pModel);
};
