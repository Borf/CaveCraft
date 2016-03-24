#ifndef __BUILDTHREAD_H__
#define __BUILDTHREAD_H__

#include <VrLib/util/thread.h>

class CaveCraft;

class BuildThread : public vrlib::Thread
{
	CaveCraft* caveCraft;
public:
	BuildThread(CaveCraft* caveCraft);

	virtual int run();

	
};




#endif