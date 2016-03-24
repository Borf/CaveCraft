#ifndef __CAVECRAFT_H__
#define __CAVECRAFT_H__

#include <GL/glew.h>
#include <fstream>
#include <vector>
#include <winsock2.h>

#include <VrLib/Application.h>
#include <VrLib/Device.h>
#include <VrLib/ClusterData.h>

#include "SharedInfo.h"

namespace vrlib { class Texture; }
class OctreeNode;

static const float scale = 1.0f;
static const float scale2 = 1.0f;


#define SIZE 512

class CaveCraft : public vrlib::Application
{
public:
	class VertexData
	{
	public:
		float	pos[3];
		float	tex[2];
		float	nor[3];
		VertexData(glm::vec2, glm::vec3, glm::vec3);
		VertexData() {};
		VertexData(VertexData&);
	};


	int sizex;
	int sizey;
	int sizez;

	int*			mBlockType;//[SIZE][SIZE][SIZE];

	class cChunk
	{
	public:
		enum eStatus
		{
			DIRTY,
			BUILDING,
			LOADED,
			DONE,
		} state;

		int topleftX, topleftY;
		CaveCraft* cc;


		GLuint VBO;
		int nVertices;

		GLuint VBOWater;
		int nVerticesWater;


		std::vector<VertexData> vertices;
		std::vector<VertexData> verticesWater;

		cChunk(int, int, CaveCraft* caveCraft);
		void buildVboData();
		void buildVbo();
		void draw();
		void drawWater();

	};

	cChunk** chunks;


//#define	blockType(x, y, z) mBlockType[(x)+sizex*(y)+sizex*sizey*(z)]
	inline int& blockType(int x, int y, int z);
	//std::vector<std::vector<std::vector<int> > > blockType;
	//int			blockData[SIZE][SIZE][SIZE];

	vrlib::Texture*	blockTexture;
	vrlib::Texture*	loadingTexture;

	int selectedIndex;
	glm::mat4 rotationMatrix;
	float fallspeed;
	OctreeNode* octree;

	CaveCraft(void);
	~CaveCraft(void);

	void init();
	void contextInit();

	void preFrame(double frameTime, double totalTime);
	void latePreFrame();
	void draw(const glm::mat4 &projectionMatrix, const glm::mat4 &modelviewMatrix);
	void postFrame();

private:
	double lastQuitKeyTime;
	bool lastQuitKey;
	vrlib::PositionalDevice mWand;
	vrlib::PositionalDevice mHead;

	vrlib::DigitalDevice   mLeftButton;
	vrlib::DigitalDevice   mRightButton;

	vrlib::DigitalDevice   mKeyPageUp;
	vrlib::DigitalDevice   mKeyPageDown;
	vrlib::DigitalDevice   mKeyF5;

	vrlib::ClusterData< SharedInfo >		sharedInfo;
	double					mLastPreFrameTime;
	double					lastPositionSync;
	double					F5DownTime;

	SOCKET s;
	std::string gzipdata;

 //  vpr::Mutex preframeMutex;
public:
	void setBlock(int x, int y, int z, bool setBlock);
	void drawCube(int,int=0,int=0,int=0,float=0);
	bool isTranslucent(int);
	void getCubeTextureIndices(int index, int &topTexture, int &bottomTexture, int &sideTexture);
	float offset[64];
	glm::vec3 getWandPosition();
	void buildThreadFunc();


	//vpr::Thread* buildThread;

};

#endif
