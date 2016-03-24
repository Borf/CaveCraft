#include "TileInfo.h"


TileInfo::TileInfo()
{
	x = 0;
	y = 0; 
	z = 0; 
	type = -1;
}

TileInfo::TileInfo(int xx, int yy, int zz, int ttype)
{
	x = xx;
	y = yy;
	z = zz;
	type = ttype;
}

TileInfo::TileInfo(const TileInfo& other)
{
	x = other.x;
	y = other.y;
	z = other.z;
	type = other.type;
}

void TileInfo::writeObject(vrlib::BinaryStream &writer)
{
	writer<<x<<y<<z<<type;
}

void TileInfo::readObject(vrlib::BinaryStream &reader)
{
	reader>>x>>y>>z>>type;
}