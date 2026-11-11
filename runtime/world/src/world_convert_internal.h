#ifndef __WORLD_CONVERT_INTERNAL_H__
#define __WORLD_CONVERT_INTERNAL_H__

// The converter's own types, shared by its readers (world_read_v56.cpp, world_read_v66.cpp) and the
// v85 writer (world_write_v85.cpp). Only those files include this.

#include "world_convert.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// stricmp is MSVC's spelling of strcasecmp
#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

#include <map>
#include <set>
#include <string>
#include <vector>
#include <algorithm>

namespace worldconv {

// LT1 surface flags
static const uint32 kSurfNonExistent = 1u << 1;	// Preprocessor drops these
static const uint32 kSurfInvisible = 1u << 2;	// Don't draw
static const uint32 kSurfTransparent = 1u << 3;	// Alpha-blended (glass, grates, sky layers)
static const uint32 kSurfSky = 1u << 4;			// Sky portal, not geometry
static const uint32 kSurfLightMap = 1u << 7;
static const uint32 kSurfPanningSky = 1u << 15; // Sky shadow pan overlaid on this surface

// Property type code for a string.
// Used to pick out the sky objects' names
static const uint8 kPropTypeString = 0;


// ePCShader_* codes written into each render section
static const uint8 kShaderGouraud = 1;
static const uint8 kShaderLightmap = 2;			// Base pass: the section's lightmap page
static const uint8 kShaderLightmapTexture = 4;	// Texture pass
// The sky shadow pan's base pass
static const uint8 kShaderSkypan = 5;
static const uint8 kShaderGouraudTranslucent = 12;

// Lightmap atlas pages.
// A v85 section carries one lightmap, so the per poly LT1 lightmaps are packed into pages of fixed width.
static const uint32 kLMPageW = 128;
static const uint32 kLMPageMaxH = 256;

// Jupiter's world info flags
static const uint32 kWifMoveable = 1u << 1;
static const uint32 kWifMainWorld = 1u << 2;
static const uint32 kWifPhysicsBsp = 1u << 4;

// LT1's object light grid, rebulit in the same order with the same rounding
static const double kLightGridCell = 350.0;
static const double kLightGridMargin = 300.0;



class Reader
{
public:
	Reader(const uint8 *pData, uint32 nSize) : m_pData(pData), m_nSize(nSize), m_nPos(0), m_bBad(false) {}

	bool Bad() const { return m_bBad; }
	uint32 Pos() const { return m_nPos; }
	void SeekTo(uint32 nPos) { if (nPos > m_nSize) m_bBad = true; else m_nPos = nPos; }

	const uint8 *Raw(uint32 n)
	{
		if (m_bBad || m_nPos + n > m_nSize || m_nPos + n < m_nPos)
		{
			m_bBad = true;
			return NULL;
		}
		const uint8 *p = m_pData + m_nPos;
		m_nPos += n;
		return p;
	}

	void Skip(uint32 n) { Raw(n); }

	uint8 U8() { const uint8 *p = Raw(1); return p ? p[0] : 0; }
	uint16 U16() { const uint8 *p = Raw(2); uint16 v = 0; if (p) memcpy(&v, p, 2); return v; }
	uint32 U32() { const uint8 *p = Raw(4); uint32 v = 0; if (p) memcpy(&v, p, 4); return v; }
	int32 I32() { const uint8 *p = Raw(4); int32 v = 0; if (p) memcpy(&v, p, 4); return v; }
	float F32() { const uint8 *p = Raw(4); float v = 0; if (p) memcpy(&v, p, 4); return v; }

	void V3(float *pOut)
	{
		const uint8 *p = Raw(12);
		if (p) memcpy(pOut, p, 12);
		else { pOut[0] = pOut[1] = pOut[2] = 0.0f; }
	}

	std::string S16()
	{
		uint16 n = U16();
		const uint8 *p = Raw(n);
		return p ? std::string((const char *)p, n) : std::string();
	}

private:
	const uint8 *m_pData;
	uint32 m_nSize;
	uint32 m_nPos;
	bool m_bBad;
};

struct Surface
{
	float m_O[3], m_P[3], m_Q[3];
	// The lightmap basis. 
	// The basis length is a per-surface luxel scale, so it is never normalised.
	float m_LMU[3], m_LMV[3];
	uint16 m_nTexture;
	uint32 m_nFlags;
	uint16 m_nTexFlags;

	// Byte 0 of the field after the flags, which LT1's vis tree builder prunes on
	uint8 m_nVisPrune;

	// The surface effect
	// Empty when the surface carries none
	std::string m_sEffect;
	std::string m_sEffectParams;
};

struct PolyVert
{
	uint16 m_nVert;
	uint8 m_RGB[3];
};

struct Poly
{
	uint32 m_nSurface;
	std::vector<PolyVert> m_Verts;
	// The world-space origin the lightmap basis measures from
	float m_LMOrigin[3];

	// The polygon's own lightmap grid
	// Zero on a v56 polygon
	uint16 m_nLMW, m_nLMH;
};

struct LeafList
{
	uint16 m_nPortalId;
	std::vector<uint8> m_Payload;
};

struct Leaf
{
	uint16 m_nCount;
	uint16 m_nAlias;
	std::vector<LeafList> m_Lists;
};

struct Node
{
	uint32 m_nPoly;
	uint16 m_nLeaf;
	int32 m_Child0, m_Child1;
};

struct WorldModel
{
	// Set by the v56 reader
	bool m_bHasVisPrune;

	std::string m_sName;

	uint32 m_nPoints, m_nPlanes, m_nSurfaces, m_nUserPortals;
	uint32 m_nPolies, m_nLeaves, m_nVerts, m_nNodes;

	float m_MinBox[3], m_MaxBox[3], m_Translation[3];

	// The source's own world info word
	uint32 m_nSrcWorldInfoFlags;

	// A user portal, with every field LT1 keeps carried verbatim
	struct UserPortal
	{
		std::string m_sName;
		uint32 m_nUnknown32;	// LT1 reads this into a scratch and drops it
		uint16 m_nUnknown16;	// portal+4 at runtime (unidentified)
		float m_Centre[3];
		float m_Dims[3];
	};

	std::vector<UserPortal> m_UserPortals;
	std::vector<std::string> m_Textures;
	std::vector<uint32> m_PolyVertCounts;	// drawn
	std::vector<uint32> m_PolyVertTotals;	// drawn + T-junction extras
	std::vector<Leaf> m_Leaves;
	std::vector<Surface> m_Surfaces;
	std::vector<Poly> m_Polies;
	std::vector<Node> m_Nodes;
	std::vector<float> m_Points; // 3 floats per point
	int32 m_nRootNode;

	// Per poly lightmap pages keyed by polygon index, as they sit in the file's packed lightmap block
	struct LMPage
	{
		uint8 m_nW, m_nH;
		std::vector<uint8> m_Texels;
	};
	std::map<uint32, LMPage> m_Lightmaps;

	// True when the object block registers this model as a sky object. 
	// Set by ReadWorld
	bool m_bSkyObject;

	// LT1's whole model translucency verdict: 1 blended, 0 opaque, -1 decide per polygon
	int m_nLT1Translucent;
};

struct SourceWorld
{
	std::string m_sWorldInfo;
	std::vector<WorldModel> m_Models;
	std::vector<uint8> m_ObjectBlock;

	// True when each model's m_nSrcWorldInfoFlags is used as is (v66), false when the flags are synthesized from the model index (v56)
	bool m_bSourceWorldInfoFlags;

	SourceWorld() : m_bSourceWorldInfoFlags(false) {}
};


// The readers
bool ReadV56(const uint8 *pData, uint32 nSize, SourceWorld &out);
bool ReadV66(const uint8 *pData, uint32 nSize, SourceWorld &out);

// LT1's network position box, kept by world_read_v56.cpp
void SetLT1PositionBox(const uint8 *pSrc, uint32 nSrcSize);
void ClearLT1PositionBox();

// The shared v85 synthesis
void StampWholeModelTranslucency(SourceWorld &src);
LTRESULT BuildV85ImageFromSource(const SourceWorld &src, uint8 **ppImage, uint32 *pImageSize, WorldV56TexDimsFn pTexDims, void *pUser, WorldV56WarnFn pWarn, void *pWarnUser, uint32 nMaxBlockVerts);

}

#endif // __WORLD_CONVERT_INTERNAL_H__
