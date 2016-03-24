#pragma comment(lib,"opengl32")

#include "cavecraft.h"
#include <VrLib/Kernel.h>

extern std::string ip;
extern int serverport;


int main(int argc, char* argv[])
{
	vrlib::Kernel* kernel = vrlib::Kernel::getInstance();           // Get the kernel

   CaveCraft* application = new CaveCraft();          // Instantiate an instance of the app
   for( int i = 1; i < argc; ++i )
   {
	   if(strcmp(argv[i], "--config") == 0)
	   {
			i++;
			kernel->loadConfig(argv[i]);
	   }
	   if(strcmp(argv[i], "--ip") == 0 || strcmp(argv[i], "--host") == 0)
	   {
			i++;
			ip = argv[i];
	   }
	   if(strcmp(argv[i], "--port") == 0)
	   {
			i++;
			serverport = atoi(argv[i]);
	   }
   }
  
   kernel->setApp(application);
   kernel->start();
   delete application;

   TerminateProcess(GetCurrentProcess(), 0);

   return 0;
}
