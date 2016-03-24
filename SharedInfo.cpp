#include "SharedInfo.h"



void SharedInfo::writeObject(vrlib::BinaryStream &writer)
{
	writer<<inMenu<<inMenuDir<<currentBlockIndex<<position<<rotation<<(blocks != NULL);
	if(blocks)
	{
		writer<<mapSizeX<<mapSizeY<<mapSizeZ;
		for(int x = 0; x < mapSizeX; x++)
		{
			for(int y = 0; y < mapSizeY; y++)
			{
				for(int z = 0; z < mapSizeZ; z++)
				{
					writer<<blocks[x * mapSizeZ*mapSizeY + y * mapSizeZ + z];
				}
			}
		}
	}
	writer<<changeTiles.size();
	if(changeTiles.size() > 0)
	{
		for(unsigned int i = 0; i < changeTiles.size(); i++)
			writer<<changeTiles[i];
	}
}
void SharedInfo::readObject(vrlib::BinaryStream &reader)
{
	reader>>inMenu>>inMenuDir>>currentBlockIndex>>position>>rotation;
	
	bool read;
	reader>>read;

	if(read)
	{
		reader>>mapSizeX>>mapSizeY>>mapSizeZ;
		if(blocks)
			delete[] blocks;
		blocks = new unsigned char[mapSizeX * mapSizeY * mapSizeZ];

		for(int x = 0; x < mapSizeX; x++)
		{
			for(int y = 0; y < mapSizeY; y++)
			{
				for(int z = 0; z < mapSizeZ; z++)
				{
					reader>>blocks[x * mapSizeZ*mapSizeY + y * mapSizeZ + z];
				}
			}
		}
	}

	size_t len;
	reader>>len;

	changeTiles.resize(len);
	for(size_t i = 0; i < len; i++)
	{
		reader>>changeTiles[i];
	}

}


int SharedInfo::getEstimatedSize()
{
	return blocks ? (1024*1024*6) : 1024;
}