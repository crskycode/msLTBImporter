// msLTBImporter.h : main header file for the msLTBImporter DLL
//

#pragma once

#include "msPlugIn.h"
#include "msLib.h"


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
