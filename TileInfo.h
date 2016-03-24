#ifndef __TILEINFO_H__
#define __TILEINFO_H__

#include <VrLib/BinaryStream.h>

class TileInfo : public vrlib::SerializableObject
{
public:
	TileInfo();
	TileInfo(int xx, int yy, int zz, int ttype);
	TileInfo(const TileInfo& other);
	
	int x, y, z, type;

	void writeObject(vrlib::BinaryStream &writer);
	void readObject(vrlib::BinaryStream &reader);
};

#endif
