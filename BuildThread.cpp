#include <winsock2.h>
#include "BuildThread.h"
#include "CaveCraft.h"

BuildThread::BuildThread( CaveCraft* caveCraft ) : Thread("BuildThread")
{
	this->caveCraft = caveCraft;
}

int BuildThread::run()
{
	caveCraft->buildThreadFunc();
	return 1;
}
