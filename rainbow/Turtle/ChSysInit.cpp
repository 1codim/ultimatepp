#include "Local.h"

#ifdef GUI_TURTLE

NAMESPACE_UPP

void ChSysInit()
{
	CtrlImg::Reset();
	CtrlsImg::Reset();
	ChReset();
}

void ChHostSkin()
{
	ChSysInit();
}

END_UPP_NAMESPACE

#endif
