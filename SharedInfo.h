#ifndef __SHAREDINFO_H__
#define __SHAREDINFO_H__


#include <vector>
#include <VrLib/BinaryStream.h>
#include <glm/glm.hpp>

#include "TileInfo.h"


class SharedInfo : public vrlib::SerializableObject
{
public:
	float inMenu;
	bool inMenuDir;
	int currentBlockIndex;
	glm::vec4 position;
	float rotation;

	unsigned char* blocks;
	int mapSizeX;
	int mapSizeY;
	int mapSizeZ;

	std::vector<TileInfo> changeTiles;


	int getEstimatedSize();
	void writeObject(vrlib::BinaryStream &writer);
	void readObject(vrlib::BinaryStream &reader);

};

#endif
