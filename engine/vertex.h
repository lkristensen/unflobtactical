#ifndef UFOATTACK_VERTEX_INCLUDED
#define UFOATTACK_VERTEX_INCLUDED

#include <vector>
#include "../grinliz/glvector.h"
#include "../grinliz/glmatrix.h"
#include "fixedgeom.h"

class Texture;


struct Vertex
{
	enum {
		POS_OFFSET = 0,
		NORMAL_OFFSET = 12,
		TEXTURE_OFFSET = 24
	};

	grinliz::Vector3F	pos;
	grinliz::Vector3F	normal;
	grinliz::Vector2F	tex;

	bool Equal( const Vertex& v ) const {
		if (    pos == v.pos 
			 && normal == v.normal
			 && tex == v.tex )
		{
			return true;
		}
		return false;
	}
};


struct VertexX
{
	enum {
		POS_OFFSET = 0,
		NORMAL_OFFSET = 12,
		TEXTURE_OFFSET = 24
	};

	void From( const Vertex& v )
	{
		pos.x = v.pos.x;
		pos.y = v.pos.y;
		pos.z = v.pos.z;

		normal.x = v.normal.x;
		normal.y = v.normal.y;
		normal.z = v.normal.z;

		tex.x = v.tex.x;
		tex.y = v.tex.y;
	}

	Vector3X	pos;
	Vector3X	normal;
	Vector2X	tex;

	bool Equal( const VertexX& v ) const {
		if (    pos == v.pos 
			 && normal == v.normal
			 && tex == v.tex )
		{
			return true;
		}
		return false;
	}
};

#endif //  UFOATTACK_VERTEX_INCLUDED