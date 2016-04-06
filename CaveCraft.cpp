#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <GL/glew.h>
#include <winsock2.h>
#include <windows.h>
#include "CaveCraft.h"
#include "BuildThread.h"

#include <glm/gtc/matrix_transform.hpp>


#include <VrLib/texture.h>
#include <VrLib/Log.h>

#include <fstream>
#include <zlib.h>
#include <math.h>
#include <vector>

std::string ip = "localhost";
int serverport = 25565;
int sticklength = 5;

void setStatus(const char* format,...)
{
	char text[2048];
	va_list args;
	va_start (args, format);
	vsprintf_s(text,2048, format, args);
	va_end (args);

	std::ofstream pFile("d:/cavestatus.txt");
	pFile.write(text, strlen(text));
	pFile<<"\n";
	pFile<<"Lopen\n";
	pFile<<"Draaien\n";
	pFile<<"Blok plaatsen\n";
	pFile<<"Blok weghalen\n";
	pFile<<"Blok Kiezen (lang inhouden om af te sluiten)";

	pFile.close();
}



CaveCraft::CaveCraft()
{
	s = 0;
	lastPositionSync = 0;
}

CaveCraft::~CaveCraft()
{

}

inline int& CaveCraft::blockType(int x, int y, int z)
{
	return mBlockType[(x)+sizex*(y)+sizex*sizey*(z)];
}


void CaveCraft::contextInit()
{
   glEnable(GL_DEPTH_TEST);
   glEnable(GL_NORMALIZE);
   glEnable(GL_LIGHTING);
   glEnable(GL_LIGHT0);
   glEnable(GL_COLOR_MATERIAL);
   glShadeModel(GL_SMOOTH);

}

int packetLens[] = {
	131,	// 0x00: login
	1,		// 0x01: ping!
	1,		// 0x02: level incoming!
	1028,	// 0x03: level chunk
	7,		// 0x04: level done
	0,		// 0x05: NOT USED
	8,		// 0x06: change a block
	74,		// 0x07
	10,		// 0x08
	7,		// 0x09
	5,		// 0x0A
	4,		// 0x0B
	2,		// 0x0C
	66,		// 0x0D
};


void CaveCraft::init()
{
	sharedInfo.init();

	sharedInfo->blocks = NULL;
	sharedInfo->position = glm::vec4(0,0,0,1);
	sharedInfo->rotation = 0;
	sharedInfo->inMenu = 0;
	sharedInfo->inMenuDir = false;
	sharedInfo->currentBlockIndex = 1;

	mBlockType = NULL;
	chunks = NULL;

	for(int i = 0; i < 64; i++)
		offset[i] = 0;

	mWand.init("WandPosition");
	mLeftButton.init("LeftButton");
	mRightButton.init("RightButton");
	mKeyF5.init("MiddleButton");
	mKeyPageDown.init("KeyPageDown");
	mKeyPageUp.init("KeyPageUp");

	blockTexture = vrlib::Texture::loadCached("data/cavecraft/terrain.png");
	blockTexture->setNearestFilter();
	loadingTexture = vrlib::Texture::loadCached("data/cavecraft/loading.png");
	loadingTexture->setNearestFilter();
	fallspeed = 0;


	rotationMatrix = glm::mat4();

	if(sharedInfo.isLocal())
	{
		struct sockaddr_in addr;
		struct hostent* host;
		host = gethostbyname(ip.c_str());	
		if(host==NULL)
		{
			printf("Could not look up host '%s', are you connected to the internet?\n", ip.c_str());
			s = 0;
			return;
		}
		addr.sin_family = host->h_addrtype;
		memcpy((char*) &addr.sin_addr.s_addr, host->h_addr_list[0], host->h_length);
		addr.sin_port = htons(serverport);
		memset(addr.sin_zero, 0, 8);

		if ((s = socket(AF_INET,SOCK_STREAM,0)) == -1)
		{
			printf("Cannot create socket, try a reboot");
			closesocket(s);
			s = 0;
			return;
		}

		int rc;
		int siz = sizeof(addr);
		rc = connect(s, (struct sockaddr*) &addr, siz);
		if (rc < 0)
		{
			printf("Could not connect to server %s:%i\n", ip.c_str(), serverport);
			closesocket(s);
			s = 0;
			return;
		}


		char bufje[1024];
		memset(bufje, ' ', 1024);
		bufje[0] = 0x00;
		bufje[1] = 0x07;
		memcpy(bufje+2, "CaveCraft", 9);
		memcpy(bufje+2+64, "c510b42dee237272f48e0478d0e0a065", 32);
		bufje[130] = 0x00;
		send(s, bufje, 131, 0);

		std::string buffer;
		char buf[2048];
		ZeroMemory(buf, 2048);
		rc = -1;
		BYTE lastPacket = 0;

		#ifdef WIN32
			unsigned long l = 1;
			ioctlsocket(s, FIONBIO, &l);
		#else
			int opts;
			opts = fcntl(s,F_GETFL);
			if (opts < 0) {
				Log(0,0,"fcntl(F_GETFL)");
			}
			opts = (opts | O_NONBLOCK);
			if (fcntl(s,F_SETFL,opts) < 0) {
				Log(0,0,"fcntl(F_SETFL)");
			}
		#endif
	}

	clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

	contextInit();

	//buildThread = new vpr::Thread(boost::bind(&CaveCraft::buildThreadFunc, this));

	BuildThread* thread = new BuildThread(this);
	thread->start();

}


glm::vec3 getRotation(glm::mat4& matrix, glm::vec3 p1, glm::vec3 p2)
{
	p1 = glm::vec3(matrix * glm::vec4(p1,1));
	p2 = glm::vec3(matrix * glm::vec4(p2,1));

	glm::vec3 p3 = p1 - p2;

	float xrot = atan2(p3[2],p3[1]);//*57.295779513;
	float yrot = atan2(p3[0],p3[2]);//*57.295779513;
	float zrot = atan2(p3[1],p3[0]);//*57.295779513;

	return glm::vec3(xrot,yrot,zrot);
}



void CaveCraft::preFrame(double frameTime, double totalTime)
{
	//vpr::Interval cur_time = mWand->getTimeStamp();
	//vpr::Interval diff_time(cur_time-mLastPreFrameTime);
	//mLastPreFrameTime = cur_time;

	
	if(sharedInfo.isLocal())
	{
		char buf[2048];
		ZeroMemory(buf, 2048);
		int rc = -1;
		BYTE lastPacket = 0;
		//while(sharedInfo->blocks == NULL)
		for(int networkpackets = 0; networkpackets < 5; networkpackets++)
		{
			rc = recv(s, buf, 1, 0);
			if(rc != 1)
				break;
			char packet = buf[0];
			Sleep(1);

			if(packet < 0 || packet > 0x0D || packet == 0x05)
			{
				printf("Invalid packet: %02X, last packet: %02X\n",packet, lastPacket); 
				continue;
			}
			lastPacket = packet;
			int len = packetLens[packet];

			int received = 0;
			int i = 0;
			while(received < len-1 && rc > -1)
			{
				rc = recv(s, buf+received, len-received-1, 0);
				received += rc;
				i++;
			}

			if(packet == 0x03) // level chunk
			{
				int len = (BYTE)buf[1] | ((BYTE)buf[0]<<8);
				gzipdata += std::string(buf+2, len);
				setStatus("Loading level: %i%%\r", buf[1026]);
				Sleep(10);
			}
			else if(packet == 0x04) // finalize level
			{
				std::string buffer;
					sharedInfo->mapSizeX = (BYTE)buf[1] | ((BYTE)buf[0]<<8);
					sharedInfo->mapSizeY = (BYTE)buf[3] | ((BYTE)buf[2]<<8);
					sharedInfo->mapSizeZ = (BYTE)buf[5] | ((BYTE)buf[4]<<8);
					std::ofstream pFile("tmp.txt", std::ios_base::out | std::ios_base::binary);
					pFile.write(gzipdata.c_str(), gzipdata.length());
					pFile.close();
					gzFile file = gzopen("tmp.txt","rb");
					while(!gzeof(file))
					{
						char buf2[1024];
						int rc = gzread(file, buf2, 1024);
						buffer += std::string(buf2, rc);
					}
					gzclose(file);
					buffer = buffer.substr(4);
					sharedInfo->blocks = new unsigned char[sharedInfo->mapSizeX*sharedInfo->mapSizeY*sharedInfo->mapSizeZ];
					for(int i = 0; i < sharedInfo->mapSizeX*sharedInfo->mapSizeY*sharedInfo->mapSizeZ; i++)
						sharedInfo->blocks[i] = buffer[i];
			}
			else if(packet == 0x0D)
			{
				printf("Message: %s\n", buf);
			}
			else if (packet == 0x06) // change a block
			{
				int x = (BYTE)buf[1] | ((BYTE)buf[0]<<8);
				int y = (BYTE)buf[3] | ((BYTE)buf[2]<<8);
				int z = (BYTE)buf[5] | ((BYTE)buf[4]<<8);
				int type = (BYTE)buf[6];					
				printf("BLOCK CHANGE: %i,%i,%i -> %i\n",x,y,z,type);
				sharedInfo->changeTiles.push_back(TileInfo(x, y, z, type));
			}
			else if(packet == 0x07)
			{
				if(buf[0] == -1)
				{
					sharedInfo->position[0] = (float)(((BYTE)buf[66] | ((BYTE)buf[65]<<8))>>5);
					sharedInfo->position[1] = (float)(((BYTE)buf[68] | ((BYTE)buf[67]<<8))>>5);
					sharedInfo->position[2] = (float)(((BYTE)buf[70] | ((BYTE)buf[69]<<8))>>5);
				}
			}
			else if(packet == 0x0D)
			{
				printf("Message: %s\n", buf);
			}

		}
		if(mBlockType == NULL)
			return;



		float collisionHeight = -1.0f;


		int targetHeight = (int)glm::ceil(glm::max(0.0f, glm::min(sizey-1.0f, sharedInfo->position[1])));
		while(targetHeight < sizey && blockType((int)sharedInfo->position[0],targetHeight,(int)sharedInfo->position[2]) != 0)
			targetHeight++;

		while(targetHeight >= 0 && blockType((int)sharedInfo->position[0],targetHeight,(int)sharedInfo->position[2]) == 0)
			targetHeight--;

		if(fabs(sharedInfo->position[1] - (targetHeight+collisionHeight)) < 0.2)
		{
			sharedInfo->position[1] = targetHeight+collisionHeight;
			fallspeed = 0;
		}
		if(sharedInfo->position[1] > targetHeight+collisionHeight)
		{
			if(fallspeed < 0.9)
				fallspeed+=0.01f;
			sharedInfo->position[1]-=fallspeed;
		}
		else
		{
			fallspeed = 0;
			sharedInfo->position[1]+=0.1f;
		}


	}

	if(mBlockType == NULL)
		return;


	if(sharedInfo.isLocal())
	{
		if(sharedInfo->inMenu == 0)
		{
			if(mKeyPageDown.getData() == vrlib::TOGGLE_ON)
			{
				glm::vec3 wandPos = getWandPosition();
				if(wandPos != glm::vec3(-1,-1,-1))
				{
					glm::vec3 diff = (wandPos - glm::vec3(floor(wandPos[0]), floor(wandPos[1]), floor(wandPos[2]))) - glm::vec3(0.5, 0.5, 0.5);
					glm::vec3 newBlockPos = wandPos;

					if(fabs(diff[0]) > fabs(diff[1]) && fabs(diff[0]) > fabs(diff[2]))
						newBlockPos[0] += diff[0] > 0 ? 1 : -1;
					else if(fabs(diff[1]) > fabs(diff[0]) && fabs(diff[1]) > fabs(diff[2]))
						newBlockPos[1] += diff[1] > 0 ? 1 : -1;
					else if(fabs(diff[2]) > fabs(diff[0]) && fabs(diff[2]) > fabs(diff[1]))
						newBlockPos[2] += diff[2] > 0 ? 1 : -1;
				
					printf("Setting Block\n");
					setBlock((int)newBlockPos[0], (int)newBlockPos[1], (int)newBlockPos[2], true);
				}
			}
			if(mKeyPageUp.getData() == vrlib::TOGGLE_ON)
			{
				glm::vec3 wandPos = getWandPosition();
				if(wandPos != glm::vec3(-1,-1,-1))
				{
					printf("Removing block\n");
					setBlock((int)wandPos[0], (int)wandPos[1], (int)wandPos[2], false);
				}
			}
			if(mRightButton.getData() == vrlib::ON)
			{
				glm::mat4 wandMat = mWand.getData();
				glm::vec4 up = wandMat * glm::vec4(0,1,0, 0);
				glm::vec3 rot1 = getRotation(wandMat, glm::vec3(0,0,0), glm::vec3(0,0,1));
				up = glm::rotate(glm::mat4(), glm::radians(-rot1[1]), glm::vec3(0,1,0)) * up;
				float wandH = glm::degrees(atan2(up[0], up[1]));
				if(wandH < 0)
					wandH = -180 - wandH;
				else
					wandH = 180 - wandH;

				sharedInfo->rotation -= wandH*(float)frameTime/1000.0f;
			}
			if(mLeftButton.getData() == vrlib::ON)
			{
				glm::mat4 wand = rotationMatrix * mWand.getData();
				sharedInfo->position += (wand * glm::vec4(0,0,0,1)) - (wand * glm::vec4(0,0,frameTime/1000.0*5,1));
			}

			lastPositionSync += frameTime;
			if(lastPositionSync > 200)
			{
				lastPositionSync = 0;
				int posX = ((int)sharedInfo->position[0])<<5 | (int)((sharedInfo->position[0]-(int)sharedInfo->position[0]) * 32);
				int posY = ((int)sharedInfo->position[1]+3)<<5 | (int)(((sharedInfo->position[1]+0.59375)-(int)(sharedInfo->position[1]+0.59375)) * 32);
				int posZ = ((int)sharedInfo->position[2])<<5 | (int)((sharedInfo->position[2]-(int)sharedInfo->position[2]) * 32);
				char buf2[10];
				buf2[0] = 0x08; // position & orientation
				buf2[1] = (char)0xff;
				buf2[2] = (posX>>8)&0xff;
				buf2[3] = posX&0xff;
				buf2[4] = (posY>>8)&0xff;
				buf2[5] = posY&0xff;
				buf2[6] = (posZ>>8)&0xff;
				buf2[7] = posZ&0xff;
				buf2[8] = (int)((sharedInfo->rotation/360)*255) ;
				buf2[9] = 0;
				send(s, buf2, 10, 0);
			}
		}
		else if(sharedInfo->inMenu == 1)
		{
			if(mLeftButton.getData() == vrlib::TOGGLE_OFF)
			{
				if(selectedIndex != 0)
				{
					sharedInfo->currentBlockIndex = selectedIndex;
					sharedInfo->inMenuDir = false;
				}
			}
		}
		sharedInfo->inMenu += sharedInfo->inMenuDir ? 1 * 0.05f : -1 * 0.05f;
		sharedInfo->inMenu = glm::min(1.0f, glm::max(0.0f, sharedInfo->inMenu));
		
		
		vrlib::DigitalState f5key = mKeyF5.getData();

		if(f5key == vrlib::TOGGLE_ON)
		{
			if(totalTime - F5DownTime < 250)
				stop();

			F5DownTime = totalTime;
		}
		if(f5key == vrlib::TOGGLE_OFF)
		{
			sharedInfo->inMenuDir = !sharedInfo->inMenuDir;
		}
		

	}
}


void CaveCraft::setBlock(int x, int y, int z, bool setBlock)
{
	if(glm::length(glm::vec4(sharedInfo->position - glm::vec4(x,y,z,sharedInfo->position[3]))) > 5)
	{
		printf("Too far: standing at %i,%i,%i, putting block at %i,%i,%i\n", (int)sharedInfo->position[0], (int)sharedInfo->position[1], (int)sharedInfo->position[2], x,y,z);
		return;
	}


	blockType(x,y,z) = setBlock ? sharedInfo->currentBlockIndex : 0;
	chunks[(x/16)+(z/16)*(int)ceil(sizex/16.0)]->state = cChunk::DIRTY;


	for(int xx = -1; xx <= 1; xx++)
		for(int zz = -1; zz <= 1; zz++)
			chunks[((x+xx)/16)+((z+zz)/16)*(int)ceil(sizex/16.0)]->state = cChunk::DIRTY;


	//octree->setChanged(x,y,z);
	
	printf("Sending a block set on (%i,%i,%i) to %i\n", x,y,z, sharedInfo->currentBlockIndex);
	char bufje[10];
	bufje[0] = 0x05;
	bufje[1] = (x>>8) & 0xff;
	bufje[2] = (x>>0) & 0xff;
	bufje[3] = (y>>8) & 0xff;
	bufje[4] = (y>>0) & 0xff;
	bufje[5] = (z>>8) & 0xff;
	bufje[6] = (z>>0) & 0xff;
	bufje[7] = setBlock ? 0x01 : 0x00;
	bufje[8] = sharedInfo->currentBlockIndex;
	send(s, bufje, 9, 0);
	//Sleep(10);
}


void CaveCraft::latePreFrame()
{
	rotationMatrix = glm::rotate(glm::mat4(), glm::radians(-sharedInfo->rotation), glm::vec3(0,1,0));
	if(sharedInfo->blocks)
	{
		printf("Setting mapdata from sharedinfo\n");
		sizex = sharedInfo->mapSizeX;
		sizey = sharedInfo->mapSizeY;
		sizez = sharedInfo->mapSizeZ;

		chunks = new cChunk*[(int)(ceil(sizex/16.0) * ceil(sizez/16.0))];
		for(int i = 0; i < ceil(sizex / 16.0) * ceil(sizez/16.0); i++)
		{
			int x = i % (int)ceil(sizez/16.0);
			int y = i / (int)ceil(sizez/16.0);
			chunks[i] = new cChunk(x, y, this);
		}


		mBlockType = new int[sharedInfo->mapSizeX * sharedInfo->mapSizeY * sharedInfo->mapSizeZ];
	



		for(int x = 0; x < sharedInfo->mapSizeX; x++)
		{
			for(int y = 0; y < sharedInfo->mapSizeY; y++)
			{
				for(int z = 0; z < sharedInfo->mapSizeZ; z++)
				{
					blockType(z,y,x) = sharedInfo->blocks[x*sharedInfo->mapSizeZ + y*sharedInfo->mapSizeX*sharedInfo->mapSizeZ + z];
				}
			}
		}
		delete sharedInfo->blocks;
		sharedInfo->blocks = NULL;

		printf("Done setting mapdata from sharedinfo\n");
		clearColor = glm::vec4(0.5f, 0.5f, 1.0f, 1.0f);
	}
	if(sharedInfo->changeTiles.size() > 0)
	{
		for(unsigned int i = 0; i < sharedInfo->changeTiles.size(); i++)
		{
			blockType(sharedInfo->changeTiles[i].x,sharedInfo->changeTiles[i].y,sharedInfo->changeTiles[i].z) = sharedInfo->changeTiles[i].type;
			chunks[(sharedInfo->changeTiles[i].x/16) + (sharedInfo->changeTiles[i].z/16) * (int)ceil(sizex/16.0)]->state = cChunk::DIRTY;
		}
		sharedInfo->changeTiles.clear();
	}


}

void CaveCraft::postFrame()
{
}


void CaveCraft::draw(const glm::mat4 &projectionMatrix, const glm::mat4 &modelviewMatrix)
{
	//glMatrixMode(GL_MODELVIEW);
	//glLoadIdentity();
	
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);

	glm::mat4 wand = mWand.getData();

	glPushMatrix();
	glScalef(scale,scale,scale);
	glRotatef(sharedInfo->rotation, 0,1,0);
	glTranslatef(-sharedInfo->position[0]*scale2, -sharedInfo->position[1]*scale2-3, -sharedInfo->position[2]*scale2);
	//	cFrustum::calculateFrustum();

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GEQUAL, 0.01f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	glDisable(GL_CULL_FACE);
	//glCullFace(GL_BACK);
	//glFrontFace(GL_CCW);


	sharedInfo->position[3] = 1;

	glLineWidth(10);
	glBegin(GL_POINTS);
	for(int i = 0; i < sticklength*scale2*50; i++)
	{
		glm::vec4 p = wand * glm::vec4(0,0,-i/50.0f,1);
		p/=scale;
		p/=scale2;
		p = rotationMatrix*p;
		p += (sharedInfo->position+glm::vec4(0,3,0,0));
		if((int)p[0] < 0 || (int)p[1] < 0 || (int)p[2] < 0 || (int)p[0] >= sharedInfo->mapSizeX || (int)p[1] >= sharedInfo->mapSizeY || (int)p[2] >= sharedInfo->mapSizeZ)
			continue;
		glVertex3f(p[0], p[1], p[2]);
	}
	glEnd();

	blockTexture->bind();
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);

	GLfloat light0_ambient[] = { 0.4f,  0.4f,  0.4f,  1.0f};
	GLfloat light0_diffuse[] = { 1.0f,  1.0f,  1.0f,  1.0f};
	GLfloat light0_specular[] = { 1.0f,  1.0f,  1.0f,  1.0f};
	GLfloat light0_position[] = { sharedInfo->position[0]*scale2, sharedInfo->position[1]*scale2, sharedInfo->position[2]*scale2, 1.0f};

	GLfloat mat_ambient[] = { 0.2f, 0.2f,  0.2f, 1.0f };
	GLfloat mat_diffuse[] = { 0.8f, 0.8f,  0.8f, 1.0f };
	GLfloat mat_specular[] = { 0.0,  0.0,  0.0,  0.0};
	GLfloat mat_shininess[] = { 0.0f };
	//   GLfloat mat_emission[] = { 1.0,  1.0,  1.0,  1.0};
	GLfloat no_mat[] = { 0.0,  0.0,  0.0,  1.0};
	glLightfv(GL_LIGHT1, GL_AMBIENT,  light0_ambient);
	glLightfv(GL_LIGHT1, GL_DIFFUSE,  light0_diffuse);
	glLightfv(GL_LIGHT1, GL_SPECULAR,  light0_specular);
	glLightfv(GL_LIGHT1, GL_POSITION,  light0_position);

	glMaterialfv( GL_FRONT_AND_BACK, GL_AMBIENT, mat_ambient );
	glMaterialfv( GL_FRONT_AND_BACK,  GL_DIFFUSE, mat_diffuse );
	glMaterialfv( GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular );
	glMaterialfv( GL_FRONT_AND_BACK,  GL_SHININESS, mat_shininess );
	glMaterialfv( GL_FRONT_AND_BACK,  GL_EMISSION, no_mat);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT1);
	glDisable(GL_LIGHT0);

	if(mBlockType == NULL)
	{
		glLoadIdentity();
		glEnable(GL_TEXTURE_2D);
		loadingTexture->bind();
		glBegin(GL_QUADS);
			glTexCoord2f(0,1); glVertex3f(-5,-5,-5);
			glTexCoord2f(0,0); glVertex3f(-5,5,-5);
			glTexCoord2f(1,0); glVertex3f(5,5,-5);
			glTexCoord2f(1,1); glVertex3f(5,-5,-5);

			glTexCoord2f(0,0); glVertex3f(-5,-5,-5);
			glTexCoord2f(0,1); glVertex3f(-5,-5,5);
			glTexCoord2f(1,1); glVertex3f(5,-5,5);
			glTexCoord2f(1,0); glVertex3f(5,-5,-5);

			glTexCoord2f(1,1); glVertex3f(-5,-5,-5);
			glTexCoord2f(0,1); glVertex3f(-5,-5,5);
			glTexCoord2f(0,0); glVertex3f(-5,5,5);
			glTexCoord2f(1,0); glVertex3f(-5,5,-5);

			glTexCoord2f(0,1); glVertex3f(5,-5,-5);
			glTexCoord2f(1,1); glVertex3f(5,-5,5);
			glTexCoord2f(1,0); glVertex3f(5,5,5);
			glTexCoord2f(0,0); glVertex3f(5,5,-5);

		glEnd();
	}

	if(chunks == NULL)
		return;

	if(sharedInfo->inMenu < 0.5)
	{
		glEnable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glColor4f(1,1,1,1-(sharedInfo->inMenu*2));

		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_NORMAL_ARRAY);
		for(int i = 0; i < ceil(sizex/16.0) * ceil(sizez/16.0); i++)
			chunks[i]->draw();
		for(int i = 0; i < ceil(sizex/16.0) * ceil(sizez/16.0); i++)
			chunks[i]->drawWater();
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_NORMAL_ARRAY);
		glDisable(GL_CULL_FACE);

		if(sharedInfo->inMenu == 0)
		{
			glm::vec3 p = getWandPosition();
			if(p != glm::vec3(-1,-1,-1))
			{
				glColor4f(1,1,1,0.5);
				glTranslatef(-0.5f, -0.5f, -0.5f);
				glBegin(GL_QUADS);
				drawCube(sharedInfo->currentBlockIndex, (int)p[0], (int)p[1], (int)p[2], 0.01f);
				glEnd();
				glTranslatef(0.5, 0.5, 0.5);
				glColor4f(1,1,1,1);
			}
		}
		glPopMatrix();
	}

	if(sharedInfo->inMenu > 0.5) //in menu+
	{
		glColor4f(1,1,1,(sharedInfo->inMenu-0.5f)*2);
		glPopMatrix();
		selectedIndex = 0;
		for(int i = 1; i < 10000; i++)
		{
			glm::vec4 pp = wand * glm::vec4(0,0, -i/100.0,1);
			if(pp[2] < -3*3)
			{
				int x = (int)((pp[0]/3) + 3);
				int y = (int)((pp[1]/3) + 2);

				if(y >= 0 && y < 4 && x >= 0 && x < 7)
				{
					selectedIndex = 1+4*x+y;
					offset[selectedIndex] = glm::min(offset[selectedIndex]+0.25f, 2.0f);
					break;
				}
			}
			if(pp[0] > 3*4)
			{
				int z = (int)((pp[2]/3) + 3);
				int y = (int)((pp[1]/3) + 2);
				if(y >= 0 && y < 4 && z >= 0 && z < 7)
				{
					selectedIndex = 1+7*4+4*z+y;
					offset[selectedIndex] = glm::min(offset[selectedIndex]+0.25f, 2.0f);
					break;
				}
			}
		}

		for(int i = 0; i < 64; i++)
		{
			if(i != selectedIndex)
				offset[i] = glm::max(0.0f, offset[i]-0.025f);
		}

		int index = 1;
		glBegin(GL_QUADS);
		for(int x = 0; x < 7; x++)
		{
			for(int y = 0; y < 4; y++)
			{
				glPushMatrix();
				glTranslatef(0,0,offset[index]);
				glBegin(GL_QUADS);
				drawCube(index, x-3,y-2,-4);
				glEnd();
				glPopMatrix();

				index++;
			}
		}
		for(int z = 0; z < 6; z++)
		{
			for(int y = 0; y < 4; y++)
			{
				glPushMatrix();
				glTranslatef(-offset[index],0,0);
				glBegin(GL_QUADS);
				drawCube(index, 4,y-2,z-3);
				glEnd();
				glPopMatrix();

				index++;
			}
		}

	}

/*	int errorCode = glGetError();
	if(errorCode != 0)
		printf("Error!: %i\n" ,errorCode);*/

}

void CaveCraft::getCubeTextureIndices(int index, int &topTexture, int &bottomTexture, int &sideTexture)
{
	switch(index)
	{
		case 0: //Air
			return;
		case 1: //Rock
			topTexture = bottomTexture = sideTexture = 1;
			break;
		case 2: //Grass
			topTexture = 0;
			bottomTexture = 2;
			sideTexture = 3;
			break;
		case 3: //Dirt
			topTexture = bottomTexture = sideTexture = 2;
			break;
		case 4: //Cobblestone
			topTexture = bottomTexture = sideTexture = 16;
			break;
		case 5: //Wood
			topTexture = bottomTexture = sideTexture = 4;
			break;
		case 6: //Sapling
			topTexture = bottomTexture = sideTexture = 15;
			break;
		case 7: //Adminium
			topTexture = bottomTexture = sideTexture = 37;
			break;
		case 8: //Water
			topTexture = bottomTexture = sideTexture = 14;
			break;
		case 9: //Stationary water
			topTexture = bottomTexture = sideTexture = 14;
			break;
		case 10: //Lava
			topTexture = bottomTexture = sideTexture = 30;
			break;
		case 11: //Stationary lava
			topTexture = bottomTexture = sideTexture = 30;
			break;
		case 12: //Sand
			topTexture = bottomTexture = sideTexture = 18;
			break;
		case 13: //Gravel
			topTexture = bottomTexture = sideTexture = 19;
			break;
		case 14: //Gold ore
			topTexture = bottomTexture = sideTexture = 32;
			break;
		case 15: //Iron ore
			topTexture = bottomTexture = sideTexture = 33;
			break;
		case 16: //Coal ore
			topTexture = bottomTexture = sideTexture = 34;
			break;
		case 17: //Tree trunk
			topTexture = bottomTexture = 21;
			sideTexture = 20;
			break;
		case 18: //Leaves
			topTexture = bottomTexture = sideTexture = 22;
			break;
		case 19: //Sponge
			topTexture = bottomTexture = sideTexture = 48;
			break;
		case 20: //Glass
			topTexture = bottomTexture = sideTexture = 49;
			break;
		case 21:// through 36: Cloth tiles of various colors
			topTexture = bottomTexture = sideTexture = 64;
			break;
		case 22:
			topTexture = bottomTexture = sideTexture = 65;
			break;
		case 23:
			topTexture = bottomTexture = sideTexture = 66;
			break;
		case 24:
			topTexture = bottomTexture = sideTexture = 67;
			break;
		case 25:
			topTexture = bottomTexture = sideTexture = 68;
			break;
		case 26:
			topTexture = bottomTexture = sideTexture = 69;
			break;
		case 27:
			topTexture = bottomTexture = sideTexture = 70;
			break;
		case 28:
			topTexture = bottomTexture = sideTexture = 71;
			break;
		case 29:
			topTexture = bottomTexture = sideTexture = 72;
			break;
		case 30:
			topTexture = bottomTexture = sideTexture = 73;
			break;
		case 31:
			topTexture = bottomTexture = sideTexture = 74;
			break;
		case 32:
			topTexture = bottomTexture = sideTexture = 75;
			break;
		case 33:
			topTexture = bottomTexture = sideTexture = 76;
			break;
		case 34:
			topTexture = bottomTexture = sideTexture = 77;
			break;
		case 35:
			topTexture = bottomTexture = sideTexture = 78;
			break;
		case 36:
			topTexture = bottomTexture = sideTexture = 79;
			break;
		case 37: //Flower
			topTexture = bottomTexture = sideTexture = 13;
			break;
		case 38: //Rose
			topTexture = bottomTexture = sideTexture = 12;
			break;
		case 39: //Red mushroom
			topTexture = bottomTexture = sideTexture = 28;
			break;
		case 40: //Brown mushroom
			topTexture = bottomTexture = sideTexture = 29;
			break;
		case 41: //Gold
			topTexture = 24;
			sideTexture = 40;
			bottomTexture = 56;
			break;
		case 42: //Iron (honestly!)
		case 43: //Double stone slab
			sideTexture = 5;
			topTexture = bottomTexture = 6;
			break;
		case 44: //Single stone slab
			sideTexture = 5;
			topTexture = bottomTexture = 6;
			break;
		case 45: //Red brick tile
			topTexture = bottomTexture = sideTexture = 7;
			break;
		case 46: //TNT
			topTexture = 9;
			bottomTexture = 10;
			sideTexture = 8;
			break;
		case 47: //Bookshelf
			topTexture = bottomTexture = sideTexture = 35;
			break;
		case 48: //Moss covered cobblestone
			topTexture = bottomTexture = sideTexture = 36;
			break;
		case 49: //Obsidian
			topTexture = bottomTexture = sideTexture = 37;
			break;
		case 50: //Torch
			break;
		case 51: //Fire
			break;
		case 52: //Infinite water source
			topTexture = bottomTexture = sideTexture = 14;
			break;
		default:
			return;
	}
}
void CaveCraft::drawCube(int index, int x, int y, int z, float sInc)
{
	int topTexture = 0;
	int sideTexture = 0;
	int bottomTexture = 0;
	
	getCubeTextureIndices(index, topTexture,bottomTexture, sideTexture);

	float t_tx0 = (topTexture%16)*(1/16.0f);
	float t_tx1 = ((topTexture%16)+1)*(1/16.0f);
	float t_ty0 = (topTexture/16)*(1/16.0f);
	float t_ty1 = ((topTexture/16)+1)*(1/16.0f);

	float s_tx0 = (sideTexture%16)*(1/16.0f);
	float s_tx1 = ((sideTexture%16)+1)*(1/16.0f);
	float s_ty0 = (sideTexture/16)*(1/16.0f);
	float s_ty1 = ((sideTexture/16)+1)*(1/16.0f);

	float b_tx0 = (bottomTexture%16)*(1/16.0f);
	float b_tx1 = ((bottomTexture%16)+1)*(1/16.0f);
	float b_ty0 = (bottomTexture/16)*(1/16.0f);
	float b_ty1 = ((bottomTexture/16)+1)*(1/16.0f);

	glNormal3f(0,1,0);
	glTexCoord2f(b_tx1, b_ty0); glVertex3f(scale2*(x+1+sInc),	scale2*(y-sInc),	scale2*(z-sInc));
	glTexCoord2f(b_tx1, b_ty1); glVertex3f(scale2*(x+1+sInc),	scale2*(y-sInc),	scale2*(z+1+sInc));
	glTexCoord2f(b_tx0, b_ty1); glVertex3f(scale2*(x-sInc),		scale2*(y-sInc),	scale2*(z+1+sInc));
	glTexCoord2f(b_tx0, b_ty0); glVertex3f(scale2*(x-sInc),		scale2*(y-sInc),	scale2*(z-sInc));
	glNormal3f(0,-1,0);
	glTexCoord2f(t_tx0, t_ty0); glVertex3f(scale2*(x-sInc),		scale2*(y+1+sInc),	scale2*(z-sInc));
	glTexCoord2f(t_tx0, t_ty1); glVertex3f(scale2*(x-sInc),		scale2*(y+1+sInc),	scale2*(z+1+sInc));
	glTexCoord2f(t_tx1, t_ty1); glVertex3f(scale2*(x+1+sInc),	scale2*(y+1+sInc),	scale2*(z+1+sInc));
	glTexCoord2f(t_tx1, t_ty0); glVertex3f(scale2*(x+1+sInc),	scale2*(y+1+sInc),	scale2*(z-sInc));
	
	glNormal3f(0,0,-1);
	glTexCoord2f(s_tx0, s_ty1); glVertex3f(scale2*(x-sInc),		scale2*(y-sInc),	scale2*(z-sInc));
	glTexCoord2f(s_tx0, s_ty0); glVertex3f(scale2*(x-sInc),		scale2*(y+1+sInc),	scale2*(z-sInc));
	glTexCoord2f(s_tx1, s_ty0); glVertex3f(scale2*(x+1+sInc),	scale2*(y+1+sInc),	scale2*(z-sInc));
	glTexCoord2f(s_tx1, s_ty1); glVertex3f(scale2*(x+1+sInc),	scale2*(y-sInc),	scale2*(z-sInc));
	
	glNormal3f(0,0,1);
	glTexCoord2f(s_tx1, s_ty1); glVertex3f(scale2*(x+1+sInc),	scale2*(y-sInc),	scale2*(z+1+sInc));
	glTexCoord2f(s_tx1, s_ty0); glVertex3f(scale2*(x+1+sInc),	scale2*(y+1+sInc),	scale2*(z+1+sInc));
	glTexCoord2f(s_tx0, s_ty0); glVertex3f(scale2*(x-sInc),		scale2*(y+1+sInc),	scale2*(z+1+sInc));
	glTexCoord2f(s_tx0, s_ty1); glVertex3f(scale2*(x-sInc),		scale2*(y-sInc),	scale2*(z+1+sInc));
	
	
	glNormal3f(1,0,0);
	glTexCoord2f(s_tx1, s_ty1); glVertex3f(scale2*(x-sInc),		scale2*(y-sInc),	scale2*(z-sInc));
	glTexCoord2f(s_tx0, s_ty1); glVertex3f(scale2*(x-sInc),		scale2*(y-sInc),	scale2*(z+1+sInc));
	glTexCoord2f(s_tx0, s_ty0); glVertex3f(scale2*(x-sInc),		scale2*(y+1+sInc),	scale2*(z+1+sInc));
	glTexCoord2f(s_tx1, s_ty0); glVertex3f(scale2*(x-sInc),		scale2*(y+1+sInc),	scale2*(z-sInc));
	
	glNormal3f(-1,0,0);
	glTexCoord2f(s_tx1, s_ty0); glVertex3f(scale2*(x+1+sInc),	scale2*(y+1+sInc),	scale2*(z-sInc));
	glTexCoord2f(s_tx0, s_ty0); glVertex3f(scale2*(x+1+sInc),	scale2*(y+1+sInc),	scale2*(z+1+sInc));
	glTexCoord2f(s_tx0, s_ty1); glVertex3f(scale2*(x+1+sInc),	scale2*(y-sInc),	scale2*(z+1+sInc));
	glTexCoord2f(s_tx1, s_ty1); glVertex3f(scale2*(x+1+sInc),	scale2*(y-sInc),	scale2*(z-sInc));
	
}

bool CaveCraft::isTranslucent(int index)
{
	return index == 0 || index == 8 || index == 9 || index == 18 || index == 37 || index == 38 || index == 39 || index == 40 || index == 20;
}

glm::vec3 CaveCraft::getWandPosition()
{
	sharedInfo->position[3] = 1;
	glm::mat4 wand = mWand.getData();


	for(int i = 0; i < sticklength*scale2*50; i++)
	{
		glm::vec4 p = wand * glm::vec4(0,0,-i/50.0f,1);
		p/=scale;
		p/=scale2;
		p = rotationMatrix*p;
		p += (sharedInfo->position+glm::vec4(0,3,0,0));
		p += glm::vec4(.5, .5, .5, 0);

		if((int)p[0] < 0 || (int)p[1] < 0 || (int)p[2] < 0 || (int)p[0] >= sharedInfo->mapSizeX || (int)p[1] >= sharedInfo->mapSizeY || (int)p[2] >= sharedInfo->mapSizeZ)
			continue;
		if(blockType((int)glm::floor(p[0]), (int)glm::floor(p[1]), (int)glm::floor(p[2])) != 0)
		{
			return glm::vec3(p[0], p[1], p[2]);
		}
	}
	return glm::vec3(-1,-1,-1);
}

void CaveCraft::buildThreadFunc()
{
	while(true)
	{
		if(mBlockType && chunks && sharedInfo->blocks == NULL)
		{
			cChunk* closest = NULL;

			for(int i = 0; i < ceil(sharedInfo->mapSizeX/16.0) * ceil(sharedInfo->mapSizeZ/16.0); i++)
			{
				if(chunks[i]->state == cChunk::DIRTY)
				{
					if(closest == NULL)
						closest = chunks[i];
					float dx1 = closest->topleftX*16 - sharedInfo->position[0];
					float dy1 = closest->topleftY*16 - sharedInfo->position[2];

					float dx2 = chunks[i]->topleftX*16 - sharedInfo->position[0];
					float dy2 = chunks[i]->topleftY*16 - sharedInfo->position[2];
					
					if(dx1*dx1+dy1*dy1 > dx2*dx2+dy2*dy2)
						closest = chunks[i];
				}
			}
			if(closest != NULL)
				closest->buildVboData();

		}
		Sleep(10);
	}
}



CaveCraft::cChunk::cChunk(int x, int y, CaveCraft* caveCraft)
{
	this->topleftX = x;
	this->topleftY = y;
	this->cc = caveCraft;
	VBO = 0;
	VBOWater = 0;
	nVertices = -1;
	nVerticesWater = -1;
	state = DIRTY;
}




int offsets[6][3] = { { -1, 0, 0 }, { 1, 0, 0 }, { 0, -1, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 } };
void CaveCraft::cChunk::buildVboData()
{
	state = BUILDING;
	vertices.clear();
	for(int x = 16*topleftX; x < 16*topleftX+16; x++)
	{
		for(int y = 0; y < cc->sizey; y++)
		{
			for(int z = 16*topleftY; z < 16*topleftY+16; z++)
			{
				if(x<0 || y<0 || z<0 || x>=cc->sizex || y>=cc->sizey || z>=cc->sizez)
					continue;
				if(cc->isTranslucent(cc->blockType(x,y,z)))
				{
					for(int i = 0; i < 6; i++)
					{
						int xx = offsets[i][0];
						int yy = offsets[i][1];
						int zz = offsets[i][2];

						if(((xx==0?0:1) + (yy==0?0:1) + (zz==0?0:1)) != 1)
							continue;
						int xxx = x+xx;
						int yyy = y+yy;
						int zzz = z+zz;
						if(xxx<0 || yyy<0 || zzz<0 || xxx>=cc->sizex || yyy>=cc->sizey || zzz>=cc->sizez)
							continue;
						int blockType = cc->blockType(xxx,yyy,zzz);

						if(blockType != 0 && blockType != 8 && blockType != 9 && blockType != 10 && blockType != 11)
						{
							glm::vec3 normal = glm::vec3(xxx,yyy,zzz) - glm::vec3(x,y,z);
							normal = glm::normalize(normal);
							glm::vec3 center = glm::vec3(x,y,z)+normal*0.5f;

							int sideTexture = 0, topTexture = 0, bottomTexture = 0;
							cc->getCubeTextureIndices(blockType, topTexture, bottomTexture, sideTexture); 

							float oneSize = 1/16.0;

							float t_tx0 = (topTexture%16)*oneSize;
							float t_tx1 = ((topTexture%16)+1)*oneSize;
							float t_ty0 = (topTexture/16)*oneSize;
							float t_ty1 = ((topTexture/16)+1)*oneSize;

							float s_tx0 = (sideTexture%16)*oneSize;
							float s_tx1 = ((sideTexture%16)+1)*oneSize;
							float s_ty0 = (sideTexture/16)*oneSize;
							float s_ty1 = ((sideTexture/16)+1)*oneSize;

							float b_tx0 = (bottomTexture%16)*oneSize;
							float b_tx1 = ((bottomTexture%16)+1)*oneSize;
							float b_ty0 = (bottomTexture/16)*oneSize;
							float b_ty1 = ((bottomTexture/16)+1)*oneSize;

							normal = glm::vec3(-normal[0], -normal[1], -normal[2]);

							VertexData vv[4];

							bool reverse = false;
							if(normal[0] != 0)
							{
								vv[0] = (VertexData(glm::vec2(s_tx1, s_ty0), (center+glm::vec3(0,0.5,0.5))*scale2, normal));
								vv[1] = (VertexData(glm::vec2(s_tx0, s_ty0), (center+glm::vec3(0,0.5,-0.5))*scale2, normal));
								vv[2] = (VertexData(glm::vec2(s_tx0, s_ty1), (center+glm::vec3(0,-0.5,-0.5))*scale2, normal));
								vv[3] = (VertexData(glm::vec2(s_tx1, s_ty1), (center+glm::vec3(0,-0.5,0.5))*scale2, normal));
								reverse = normal[0] > 0;
							}
							else if(normal[1] != 0)
							{
								vv[0] = (VertexData(glm::vec2(t_tx1, t_ty0), (center+glm::vec3(0.5,0,0.5))*scale2, normal));
								vv[1] = (VertexData(glm::vec2(t_tx0, t_ty0), (center+glm::vec3(0.5,0,-0.5))*scale2, normal));
								vv[2] = (VertexData(glm::vec2(t_tx0, t_ty1), (center+glm::vec3(-0.5,0,-0.5))*scale2, normal));
								vv[3] = (VertexData(glm::vec2(t_tx1, t_ty1), (center+glm::vec3(-0.5,0,0.5))*scale2, normal));
								reverse = normal[1] < 0;
							}
							else if(normal[2] != 0)
							{
								vv[0] = (VertexData(glm::vec2(s_tx1, s_ty0), (center+glm::vec3(0.5,0.5,0))*scale2, normal));
								vv[1] = (VertexData(glm::vec2(s_tx1, s_ty1), (center+glm::vec3(0.5,-0.5,0))*scale2, normal));
								vv[2] = (VertexData(glm::vec2(s_tx0, s_ty1), (center+glm::vec3(-0.5,-0.5,0))*scale2, normal));
								vv[3] = (VertexData(glm::vec2(s_tx0, s_ty0), (center+glm::vec3(-0.5,0.5,0))*scale2, normal));
								reverse = normal[2] > 0;
							}
							if(reverse)
								std::reverse(vv, vv+4);
							vertices.insert(vertices.end(), vv, vv+4);
						}
					}
				}
			}
		}
	}

	/*nVertices = tmpVertices.size();
	vertices = new VertexData[tmpVertices.size()];
	for(int i = 0; i < nVertices; i++)
		vertices[i] = tmpVertices[i];*/




	///water
	verticesWater.clear();
	for(int x = 16*topleftX; x < 16*topleftX+16; x++)
	{
		for(int y = 0; y < cc->sizey; y++)
		{
			for(int z = 16*topleftY; z < 16*topleftY+16; z++)
			{
				if(x<0 || y<0 || z<0 || x>=cc->sizex || y>=cc->sizey || z>=cc->sizez)
					continue;

				if(cc->blockType(x,y,z) == 0)
				{
					for(int i = 0; i < 6; i++)
					{
						int xx = offsets[i][0];
						int yy = offsets[i][1];
						int zz = offsets[i][2];

						if(((xx==0?0:1) + (yy==0?0:1) + (zz==0?0:1)) != 1)
							continue;
						int xxx = x+xx;
						int yyy = y+yy;
						int zzz = z+zz;
						if(xxx<0 || yyy<0 || zzz<0 || xxx>=cc->sizex || yyy>=cc->sizey || zzz>=cc->sizez)
							continue;
						int blockType = cc->blockType(xxx,yyy,zzz);

						if(blockType == 8 || blockType == 9)
						{
							glm::vec3 normal = glm::vec3(xxx,yyy,zzz) - glm::vec3(x,y,z);
							normal = glm::normalize(normal);
							glm::vec3 center = glm::vec3(x,y,z)+normal*0.5f;

							int sideTexture = 0, topTexture = 0, bottomTexture = 0;
							cc->getCubeTextureIndices(blockType, topTexture, bottomTexture, sideTexture); 

							float oneSize = 1/16.0;

							float t_tx0 = (topTexture%16)*oneSize;
							float t_tx1 = ((topTexture%16)+1)*oneSize;
							float t_ty0 = (topTexture/16)*oneSize;
							float t_ty1 = ((topTexture/16)+1)*oneSize;

							float s_tx0 = (sideTexture%16)*oneSize;
							float s_tx1 = ((sideTexture%16)+1)*oneSize;
							float s_ty0 = (sideTexture/16)*oneSize;
							float s_ty1 = ((sideTexture/16)+1)*oneSize;

							float b_tx0 = (bottomTexture%16)*oneSize;
							float b_tx1 = ((bottomTexture%16)+1)*oneSize;
							float b_ty0 = (bottomTexture/16)*oneSize;
							float b_ty1 = ((bottomTexture/16)+1)*oneSize;

							normal = glm::vec3(-normal[0], -normal[1], -normal[2]);

							VertexData vv[4];

							bool reverse = false;
							if(normal[0] != 0)
							{
								vv[0] = (VertexData(glm::vec2(s_tx1, s_ty0), (center+glm::vec3(0,0.5,0.5))*scale2, normal));
								vv[1] = (VertexData(glm::vec2(s_tx0, s_ty0), (center+glm::vec3(0,0.5,-0.5))*scale2, normal));
								vv[2] = (VertexData(glm::vec2(s_tx0, s_ty1), (center+glm::vec3(0,-0.5,-0.5))*scale2, normal));
								vv[3] = (VertexData(glm::vec2(s_tx1, s_ty1), (center+glm::vec3(0,-0.5,0.5))*scale2, normal));
								reverse = normal[0] > 0;
							}
							else if(normal[1] != 0)
							{
								vv[0] = (VertexData(glm::vec2(t_tx1, t_ty0), (center+glm::vec3(0.5,0,0.5))*scale2, normal));
								vv[1] = (VertexData(glm::vec2(t_tx0, t_ty0), (center+glm::vec3(0.5,0,-0.5))*scale2, normal));
								vv[2] = (VertexData(glm::vec2(t_tx0, t_ty1), (center+glm::vec3(-0.5,0,-0.5))*scale2, normal));
								vv[3] = (VertexData(glm::vec2(t_tx1, t_ty1), (center+glm::vec3(-0.5,0,0.5))*scale2, normal));
								reverse = normal[1] < 0;
							}
							else if(normal[2] != 0)
							{
								vv[0] = (VertexData(glm::vec2(s_tx1, s_ty0), (center+glm::vec3(0.5,0.5,0))*scale2, normal));
								vv[1] = (VertexData(glm::vec2(s_tx1, s_ty1), (center+glm::vec3(0.5,-0.5,0))*scale2, normal));
								vv[2] = (VertexData(glm::vec2(s_tx0, s_ty1), (center+glm::vec3(-0.5,-0.5,0))*scale2, normal));
								vv[3] = (VertexData(glm::vec2(s_tx0, s_ty0), (center+glm::vec3(-0.5,0.5,0))*scale2, normal));
								reverse = normal[2] > 0;
							}
							if(reverse)
								std::reverse(vv, vv+4);
							verticesWater.insert(verticesWater.end(), vv, vv+4);
						}
					}
				}
			}
		}
	}

/*	nVerticesWater = tmpVertices.size();
	verticesWater = new VertexData[tmpVertices.size()];
	for(int i = 0; i < nVerticesWater; i++)
		verticesWater[i] = tmpVertices[i];*/

	fprintf(stderr, ".");
	state = LOADED;
}

void CaveCraft::cChunk::draw()
{
	if(state == LOADED)
		buildVbo();

	if(nVertices > 0 && VBO != 0)
	{
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glVertexPointer(3,		GL_FLOAT, sizeof(VertexData),(GLvoid*)0);
		glTexCoordPointer(2,	GL_FLOAT, sizeof(VertexData),(GLvoid*) (sizeof(float)*3));
		glNormalPointer(		GL_FLOAT, sizeof(VertexData),(GLvoid*) (sizeof(float)*5));
		glDrawArrays( GL_QUADS, 0, nVertices);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
}

void CaveCraft::cChunk::drawWater()
{
	if(nVerticesWater > 0 && VBOWater != 0)
	{
		glBindBuffer(GL_ARRAY_BUFFER, VBOWater);
		glVertexPointer(3,		GL_FLOAT, sizeof(VertexData),(GLvoid*)0);
		glTexCoordPointer(2,	GL_FLOAT, sizeof(VertexData),(GLvoid*) (sizeof(float)*3));
		glNormalPointer(		GL_FLOAT, sizeof(VertexData),(GLvoid*) (sizeof(float)*5));
		glDrawArrays( GL_QUADS, 0, nVerticesWater);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
}


void CaveCraft::cChunk::buildVbo()
{
	if(VBO == 0)
	{
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &VBOWater);
	}

	nVertices = vertices.size();
	VertexData* v = new VertexData[vertices.size()];
	for(int i = 0; i < nVertices; i++)
		v[i] = vertices[i];

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, (nVertices*2 + nVertices*3 + nVertices*3)  *sizeof(float), &(v[0]), GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	delete[] v;


	nVerticesWater = verticesWater.size();
	v = new VertexData[verticesWater.size()];
	for(int i = 0; i < nVerticesWater; i++)
		v[i] = verticesWater[i];

	glBindBuffer(GL_ARRAY_BUFFER, VBOWater);
	glBufferData(GL_ARRAY_BUFFER, (nVerticesWater*2 + nVerticesWater*3 + nVerticesWater*3)  *sizeof(float), &(v[0]), GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	delete[] v;

	state = DONE;
}

CaveCraft::VertexData::VertexData( glm::vec2 t, glm::vec3 p, glm::vec3 n)
{
	pos[0] = p[0];
	pos[1] = p[1];
	pos[2] = p[2];

	nor[0] = n[0];
	nor[1] = n[1];
	nor[2] = n[2];

	tex[0] = t[0];
	tex[1] = t[1];
}

CaveCraft::VertexData::VertexData( VertexData& other)
{
	pos[0] = other.pos[0];
	pos[1] = other.pos[1];
	pos[2] = other.pos[2];

	nor[0] = other.nor[0];
	nor[1] = other.nor[1];
	nor[2] = other.nor[2];

	tex[0] = other.tex[0];
	tex[1] = other.tex[1];
}
