// The LithTech 1.0 world reader (DAT v56), and LT1's network position box

#include "world_convert_internal.h"

using namespace worldconv;

namespace worldconv {


static bool ReadWorldModel(Reader &r, WorldModel &m)
{
	// Each world model is preceded by the offset of the next one and 32 bytes of dummy data
	r.U32();
	r.Skip(32);

	m.m_bSkyObject = false;		// Set by ReadWorld from the object block
	m.m_nLT1Translucent = -1;	// Stamped by world_v56_BuildV85Image
	m.m_nSrcWorldInfoFlags = 0;	// v56's word isn't a bitfield, so it's never read

	r.U32(); // World info flags, not carried
	m.m_sName = r.S16();
	r.U32(); // Next position (LT1 only)

	m.m_nPoints = r.U32();
	m.m_nPlanes = r.U32();
	m.m_nSurfaces = r.U32();
	m.m_nUserPortals = r.U32();
	m.m_nPolies = r.U32();
	m.m_nLeaves = r.U32();
	m.m_nVerts = r.U32();
	// Two sizing hints for the leaf section, which Jupiter never uses
	r.U32();	// totalVisListSize
	r.U32();	// nLeafLists
	m.m_nNodes = r.U32();

	// Total number of leaf node indices, which Jupiter's header has no field for
	r.U32();

	r.V3(m.m_MinBox);
	r.V3(m.m_MaxBox);
	r.V3(m.m_Translation);

	r.U32(); // name length
	uint32 nTextures = r.U32();
	if (r.Bad() || nTextures > 0x10000) return false;

	m.m_Textures.reserve(nTextures);
	for (uint32 i = 0; i < nTextures; ++i)
	{
		// WorldTexture: a null terminated name
		std::string s;
		for (;;)
		{
			uint8 c = r.U8();
			if (r.Bad() || c == 0) break;
			s += (char)c;
		}
		m.m_Textures.push_back(s);
	}

	// Two per poly vertex counts: 
	// The polygon as drawn, then extra edge vertices that close T-junction cracks.
	// Only the first set is drawn since summing them produces lots of z fighting.
	if (r.Bad() || m.m_nPolies > 0x400000) return false;
	m.m_PolyVertCounts.reserve(m.m_nPolies);
	m.m_PolyVertTotals.reserve(m.m_nPolies);
	for (uint32 i = 0; i < m.m_nPolies; ++i)
	{
		uint8 a = r.U8();
		uint8 b = r.U8();
		m.m_PolyVertCounts.push_back((uint32)a);
		m.m_PolyVertTotals.push_back((uint32)a + (uint32)b);
	}

	// Leaves
	// Kept verbatim. The v85 loader walks and skips them.
	if (r.Bad() || m.m_nLeaves > 0x400000) return false;
	m.m_Leaves.resize(m.m_nLeaves);
	for (uint32 i = 0; i < m.m_nLeaves; ++i)
	{
		Leaf &leaf = m.m_Leaves[i];
		leaf.m_nCount = r.U16();
		leaf.m_nAlias = 0;
		if (leaf.m_nCount == 0xFFFF)
		{
			leaf.m_nAlias = r.U16();
		}
		else
		{
			leaf.m_Lists.resize(leaf.m_nCount);
			for (uint32 k = 0; k < leaf.m_nCount; ++k)
			{
				leaf.m_Lists[k].m_nPortalId = r.U16();
				uint16 nSize = r.U16();
				const uint8 *p = r.Raw(nSize);
				if (!p) return false;
				leaf.m_Lists[k].m_Payload.assign(p, p + nSize);
			}
		}
		// LT1 extra leaf data which v85 has no place for
		uint16 nLeafPolies = r.U16();
		r.Skip((uint32)nLeafPolies * 4);
		r.F32();
		if (r.Bad()) return false;
	}

	// Planes are dropped cuz a plane is derived per polygon on the way out
	if (r.Bad() || m.m_nPlanes > 0x400000) return false;
	r.Skip(m.m_nPlanes * 16);

	// Surfaces
	if (r.Bad() || m.m_nSurfaces > 0x400000) return false;
	m.m_bHasVisPrune = false;
	m.m_Surfaces.resize(m.m_nSurfaces);
	for (uint32 i = 0; i < m.m_nSurfaces; ++i)
	{
		Surface &s = m.m_Surfaces[i];
		float dummy[3];
		r.V3(s.m_O); r.V3(s.m_P); r.V3(s.m_Q);
		r.V3(s.m_LMU); r.V3(s.m_LMV);	// Lightmap basis (see Surface)
		r.V3(dummy);					// Color
		s.m_nTexture = r.U16();

		r.U32(); // Plane index

		s.m_nFlags = r.U32();

		// LT1 keeps only the first of these four bytes, which the vis tree builder prunes on
		{
			uint32 nFour = r.U32();
			s.m_nVisPrune = (uint8)(nFour & 0xFF);
			m.m_bHasVisPrune = true;
		}

		uint8 bUseEffects = r.U8();
		if (bUseEffects == 1)
		{
			// Effect name then effect parameters
			s.m_sEffect = r.S16();
			s.m_sEffectParams = r.S16();
		}
		s.m_nTexFlags = r.U16();
		if (r.Bad()) return false;
	}

	// Polygons
	m.m_Polies.resize(m.m_nPolies);
	for (uint32 i = 0; i < m.m_nPolies; ++i)
	{
		Poly &p = m.m_Polies[i];
		// Four fields LT1 never stores
		r.U16(); r.U16();
		r.U32(); r.U32();

		p.m_nSurface = r.U32(); // Bounds checked against nSurfaces

		uint32 nVerts = m.m_PolyVertCounts[i];
		uint32 nOnDisk = m.m_PolyVertTotals[i];
		p.m_Verts.resize(nVerts);
		for (uint32 k = 0; k < nOnDisk; ++k)
		{
			uint16 nVert = r.U16();
			const uint8 *pRGB = r.Raw(3);
			if (!pRGB) return false;
			// Past the drawn set are the T-junction vertices, which are consumed and not kept
			if (k < nVerts)
			{
				p.m_Verts[k].m_nVert = nVert;
				memcpy(p.m_Verts[k].m_RGB, pRGB, 3);
			}
		}
		if (r.Bad()) return false;
	}

	// Nodes. 
	// v85 keeps only the poly index, leaf index, and two child indices.
	if (r.Bad() || m.m_nNodes > 0x400000) return false;
	m.m_Nodes.resize(m.m_nNodes);
	for (uint32 i = 0; i < m.m_nNodes; ++i)
	{
		r.U32(); // Plane index

		m.m_Nodes[i].m_nPoly = r.U32();
		m.m_Nodes[i].m_nLeaf = r.U16();
		m.m_Nodes[i].m_Child0 = r.I32();
		m.m_Nodes[i].m_Child1 = r.I32();

		// A bounding sphere
		r.Skip(16); // centre (float[3]) + radius
	}

	// User portals
	// Carried verbatim into the v85 image
	m.m_UserPortals.resize(m.m_nUserPortals);
	for (uint32 i = 0; i < m.m_nUserPortals; ++i)
	{
		WorldModel::UserPortal &up = m.m_UserPortals[i];
		up.m_sName = r.S16();
		up.m_nUnknown32 = r.U32();
		up.m_nUnknown16 = r.U16();
		r.V3(up.m_Centre);
		r.V3(up.m_Dims);
		if (r.Bad()) return false;
	}

	// Points
	if (r.Bad() || m.m_nPoints > 0x400000) return false;
	m.m_Points.resize((size_t)m.m_nPoints * 3);
	for (uint32 i = 0; i < m.m_nPoints; ++i)
		r.V3(&m.m_Points[(size_t)i * 3]);

	// LT1 polygon block table
	{
		uint32 a = r.U32();
		uint32 b = r.U32();
		uint32 c = r.U32();
		float dummy[3];
		r.V3(dummy); r.V3(dummy);
		if (r.Bad()) return false;
		// Guard the product since a corrupt header would otherwise spin
		double nBlocks = (double)a * (double)b * (double)c;
		if (nBlocks > 4.0 * 1024.0 * 1024.0) return false;
		uint32 nTotal = a * b * c;
		for (uint32 i = 0; i < nTotal; ++i)
		{
			uint16 nSize = r.U16();
			r.U16();
			r.Skip((uint32)nSize * 6);
			if (r.Bad()) return false;
		}
	}

	m.m_nRootNode = r.I32();

	// LT1 tail: 
	// An unidentified word, per poly vectors, then packed lightmaps.
	// The LT1 runtime never reads this.
	r.U32();
	// One lightmap origin per polygon
	for (uint32 i = 0; i < m.m_nPolies; ++i)
		r.V3(m.m_Polies[i].m_LMOrigin);

	// A byte count for the packed lightmap block. Not a number of lightmaps
	uint32 nLightmapBytes = r.U32();
	if (nLightmapBytes > 0)
	{
		for (uint32 i = 0; i < m.m_nPolies; ++i)
		{
			uint32 nSurf = m.m_Polies[i].m_nSurface;
			if (nSurf >= m.m_nSurfaces) continue;
			if (!(m.m_Surfaces[nSurf].m_nFlags & kSurfLightMap)) continue;
			uint8 w = r.U8();
			uint8 h = r.U8();
			WorldModel::LMPage &page = m.m_Lightmaps[i];
			page.m_nW = w;
			page.m_nH = h;
			page.m_Texels.resize((size_t)w * h * 2);
			if (page.m_Texels.size())
			{
				const uint8 *pTexels = r.Raw((uint32)page.m_Texels.size());
				if (pTexels)
					memcpy(&page.m_Texels[0], pTexels, page.m_Texels.size());
			}
			if (r.Bad()) return false;
		}
	}

	return !r.Bad();
}


bool ReadV56(const uint8 *pData, uint32 nSize, SourceWorld &out)
{
	Reader r(pData, nSize);

	if (r.U32() != 56) return false;

	uint32 nObjectDataPos = r.U32();

	// Offset of the world model section. The objects are walked instead of seeking here.
	r.U32();

	uint32 nInfoLen = r.U32();
	const uint8 *pInfo = r.Raw(nInfoLen);
	if (!pInfo) return false;
	out.m_sWorldInfo.assign((const char *)pInfo, nInfoLen);

	// Eight dummy ints. LT1 carries no extents here, they come from the root model's box.
	r.Skip(8 * 4);

	// The object list comes before the world models.
	// Each object's data length covers everything after that field.
	r.SeekTo(nObjectDataPos);
	uint32 nObjects = r.U32();
	if (r.Bad() || nObjects > 0x100000) return false;
	std::set<std::string> SkyObjectNames;
	for (uint32 i = 0; i < nObjects; ++i)
	{
		uint32 nStart = r.Pos();
		uint16 nDataLen = r.U16();
		std::string sType = r.S16();

		// DemoSkyWorldModel is a sky object itself, and SkyPointer names another brush in SkyObjectName
		if (sType == "DemoSkyWorldModel" || sType == "SkyPointer")
		{
			const char *pszWant = (sType == "DemoSkyWorldModel") ? "Name" : "SkyObjectName";
			uint32 nProps = r.U32();
			for (uint32 nProp = 0; nProp < nProps && !r.Bad(); ++nProp)
			{
				std::string sProp = r.S16();
				uint8 nCode = r.U8();
				r.U32(); // Flags
				uint16 nLen = r.U16();
				const uint8 *pVal = r.Raw(nLen);
				// A string property's data is itself length-prefixed
				if (pVal && sProp == pszWant && nCode == kPropTypeString && nLen >= 2)
				{
					uint16 nChars = 0;
					memcpy(&nChars, pVal, 2);
					if ((uint32)nChars + 2 <= (uint32)nLen && nChars > 0)
						SkyObjectNames.insert(std::string((const char *)pVal + 2, nChars));
				}
			}
		}

		r.SeekTo(nStart + 2 + nDataLen);
		if (r.Bad()) return false;
	}

	// The object list is identical between v56 and v85. Carried across verbatim
	out.m_ObjectBlock.assign(pData + nObjectDataPos, pData + r.Pos());

	// LT1 has no world tree. The root world model is first, then a count and the rest.
	out.m_Models.resize(1);
	if (!ReadWorldModel(r, out.m_Models[0])) return false;

	uint32 nMore = r.U32();
	if (r.Bad() || nMore > 0x10000) return false;
	for (uint32 i = 0; i < nMore; ++i)
	{
		out.m_Models.resize(out.m_Models.size() + 1);
		if (!ReadWorldModel(r, out.m_Models.back())) return false;
	}

	// A world model registered as a sky object is shaded differently.
	// Matched with case, as LT1's name table is actually case sensitive and SkyPointer only registers an exact match.
	for (uint32 i = 0; i < out.m_Models.size(); ++i)
	{
		WorldModel &m = out.m_Models[i];
		if (!m.m_sName.empty() && SkyObjectNames.find(m.m_sName) != SkyObjectNames.end())
		{
			m.m_bSkyObject = true;
		}
	}

	return true;
}

}


// LT1's network position box from the last world converted (world_convert.h)
static bool  s_bLT1PosBox = false;
static float s_LT1PosMin[3], s_LT1PosMax[3];

// The world file's last 24 bytes - min then max - each padded by 200 as LT1's loader does
void worldconv::SetLT1PositionBox(const uint8 *pSrc, uint32 nSrcSize)
{
	s_bLT1PosBox = false;
	if (!pSrc || nSrcSize < 24) return;
	float f[6];
	memcpy(f, pSrc + nSrcSize - 24, sizeof(f));
	for (int i = 0; i < 3; ++i)
	{
		s_LT1PosMin[i] = f[i] - 200.0f;
		s_LT1PosMax[i] = f[3 + i] + 200.0f;
		if (!(s_LT1PosMax[i] > s_LT1PosMin[i])) return;
	}
	s_bLT1PosBox = true;
}

bool world_v56_LT1EncodePosition(uint16 nOut[3], const LTVector &vPos)
{
	if (!s_bLT1PosBox) return false;
	const float v[3] = { vPos.x, vPos.y, vPos.z };
	for (int i = 0; i < 3; ++i)
	{
		// Float throughout, as LT1's math rounded like a float, and LT1 stored the reciprocal as a float
		const float fInv = 1.0f / (s_LT1PosMax[i] - s_LT1PosMin[i]);
		const float fPercent = (v[i] - s_LT1PosMin[i]) * fInv;
		nOut[i] = (uint16)(uint32)(fPercent * 65536.0f);   // Truncated, no clamp
	}
	return true;
}

bool world_v56_LT1DecodePosition(LTVector &vOut, const uint16 nIn[3])
{
	if (!s_bLT1PosBox) return false;
	float v[3];
	for (int i = 0; i < 3; ++i)
	{
		const float fPercent = (float)nIn[i] / 65536.0f;
		v[i] = s_LT1PosMin[i] + (s_LT1PosMax[i] - s_LT1PosMin[i]) * fPercent;
	}
	vOut.Init(v[0], v[1], v[2]);
	return true;
}

void worldconv::ClearLT1PositionBox()
{
	s_bLT1PosBox = false;
}


LTRESULT world_v56_BuildV85Image(const uint8 *pSrc, uint32 nSrcSize, uint8 **ppImage, uint32 *pImageSize, WorldV56TexDimsFn pTexDims, void *pUser, WorldV56WarnFn pWarn, void *pWarnUser, uint32 nMaxBlockVerts)
{
	if (!pSrc || !ppImage || !pImageSize) return LT_INVALIDPARAMS;

	*ppImage = NULL;
	*pImageSize = 0;

	// LT1's position box, which the converted image doesn't carry
	SetLT1PositionBox(pSrc, nSrcSize);

	SourceWorld src;
	if (!ReadV56(pSrc, nSrcSize, src)) return LT_INVALIDFILE;
	if (src.m_Models.empty()) return LT_INVALIDFILE;

	StampWholeModelTranslucency(src);

	return BuildV85ImageFromSource(src, ppImage, pImageSize, pTexDims, pUser, pWarn, pWarnUser, nMaxBlockVerts);
}

bool world_v56_IsV56(const uint8 *pData, uint32 nSize)
{
	if (!pData || nSize < 4) return false;
	uint32 nVersion = 0;
	memcpy(&nVersion, pData, 4);
	return nVersion == 56;
}
