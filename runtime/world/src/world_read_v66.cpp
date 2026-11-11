#include "world_convert_internal.h"

using namespace worldconv;

namespace worldconv {


// LithTech 2.x worlds (DAT v66)
// Only the reader is new. Everything downstream runs over a v66 model unchanged, as it does over a v56 one.
// The layout differs from v56's: twelve header counts, the spare word before the name, three surface vectors instead of six,
// 14-byte nodes, a uint32 leaf polygon count, points with a normal, and no packed lightmap block.

static std::string ToLowerV66(const std::string &s)
{
	std::string out(s);
	for (size_t i = 0; i < out.size(); ++i)
		if (out[i] >= 'A' && out[i] <= 'Z') out[i] = (char)(out[i] - 'A' + 'a');
	return out;
}

static bool ReadWorldModelV66(Reader &r, WorldModel &m)
{
	uint32 nNextPos = r.U32();
	r.Skip(32); // Dummy block, 8 dwords
	m.m_nSrcWorldInfoFlags = r.U32(); // A real Jupiter world info bitfield
	r.U32(); // Spare, which v56 puts after the name
	m.m_sName = r.S16();
	m.m_bSkyObject = false;
	m.m_nLT1Translucent = -1; // Decided per polygon
	m.m_bHasVisPrune = false;

	m.m_nPoints = r.U32();
	m.m_nPlanes = r.U32();
	m.m_nSurfaces = r.U32();
	m.m_nUserPortals = r.U32();
	m.m_nPolies = r.U32();
	m.m_nLeaves = r.U32();
	m.m_nVerts = r.U32();
	r.U32(); // Total vis list size
	r.U32(); // Leaf list count
	m.m_nNodes = r.U32();
	r.U32(); // Total leaf polies
	r.U32(); // The twelfth count, unidentified

	r.V3(m.m_MinBox);
	r.V3(m.m_MaxBox);
	r.V3(m.m_Translation);
	if (r.Bad() || m.m_nPolies > 0x400000 || m.m_nLeaves > 0x400000 || m.m_nSurfaces > 0x400000 ||
		m.m_nNodes > 0x400000 || m.m_nPoints > 0x400000 || m.m_nPlanes > 0x400000) return false;

	uint32 nTexBytes = r.U32();
	uint32 nTextures = r.U32();
	const uint8 *pTex = r.Raw(nTexBytes);
	if (!pTex) return false;
	// NUL separated names, as many as the count says, so an empty name keeps its slot
	{
		uint32 nStart = 0;
		for (uint32 i = 0; i < nTexBytes && m.m_Textures.size() < nTextures; ++i)
		{
			if (pTex[i] != 0) continue;
			m.m_Textures.push_back(std::string((const char *)pTex + nStart, i - nStart));
			nStart = i + 1;
		}
	}

	// Per poly vertex counts: drawn, then the T-junction extras
	m.m_PolyVertCounts.resize(m.m_nPolies);
	m.m_PolyVertTotals.resize(m.m_nPolies);
	for (uint32 i = 0; i < m.m_nPolies; ++i)
	{
		uint8 a = r.U8();
		uint8 b = r.U8();
		m.m_PolyVertCounts[i] = a;
		m.m_PolyVertTotals[i] = (uint32)a + b;
	}
	if (r.Bad()) return false;

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
		uint32 nPolys = r.U32(); // v56 reads a uint16 here
		if (nPolys > 0x400000) return false;
		r.Skip(nPolys * 4);
		r.Skip(4);
		if (r.Bad()) return false;
	}

	r.Skip(m.m_nPlanes * 16); // Normal and distance

	m.m_Surfaces.resize(m.m_nSurfaces);
	for (uint32 i = 0; i < m.m_nSurfaces; ++i)
	{
		Surface &s = m.m_Surfaces[i];
		r.V3(s.m_O);
		r.V3(s.m_P);
		r.V3(s.m_Q);
		// No LT1 lightmap basis in v66
		for (int k = 0; k < 3; ++k) s.m_LMU[k] = s.m_LMV[k] = 0.0f;
		s.m_nVisPrune = 0;
		s.m_nTexture = r.U16();
		r.U32(); // Plane index
		s.m_nFlags = r.U32();
		r.U8(); r.U8(); r.U8(); r.U8();
		if (r.U8() == 1) // The fifth byte gates the effect strings
		{
			s.m_sEffect = r.S16();
			s.m_sEffectParams = r.S16();
		}
		s.m_nTexFlags = r.U16(); // After the effect strings, where v56 has it before
		if (r.Bad()) return false;
	}

	m.m_Polies.resize(m.m_nPolies);
	for (uint32 i = 0; i < m.m_nPolies; ++i)
	{
		Poly &poly = m.m_Polies[i];
		r.V3(poly.m_LMOrigin); // The lightmap reference point
		poly.m_nLMW = r.U16();
		poly.m_nLMH = r.U16();
		uint16 nExtra = r.U16();
		r.Skip((uint32)nExtra * 4);
		poly.m_nSurface = r.U32();
		uint32 nDrawn = m.m_PolyVertCounts[i];
		for (uint32 k = 0; k < m.m_PolyVertTotals[i]; ++k)
		{
			uint16 nVert = r.U16();
			const uint8 *pRGB = r.Raw(3);
			if (!pRGB) return false;
			// Only the drawn polygon is kept, as in ReadWorldModel
			if (k < nDrawn)
			{
				PolyVert pv;
				pv.m_nVert = nVert;
				memcpy(pv.m_RGB, pRGB, 3);
				poly.m_Verts.push_back(pv);
			}
		}
		if (r.Bad()) return false;
	}

	m.m_Nodes.resize(m.m_nNodes);
	for (uint32 i = 0; i < m.m_nNodes; ++i)
	{
		Node &n = m.m_Nodes[i];
		n.m_nPoly = r.U32();
		n.m_nLeaf = r.U16();
		n.m_Child0 = r.I32();
		n.m_Child1 = r.I32();
	}

	// The same user portal record as v56, carried the same way
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

	// Position and normal, 24 bytes each, where v56 stores the position only
	m.m_Points.resize((size_t)m.m_nPoints * 3);
	for (uint32 i = 0; i < m.m_nPoints; ++i)
	{
		r.V3(&m.m_Points[(size_t)i * 3]);
		r.Skip(12);
	}

	// A spatial block table, which nothing in LT2's rendering or collision reads back
	{
		uint32 gx = r.U32(), gy = r.U32(), gz = r.U32();
		r.Skip(24);
		uint32 nCells = gx * gy * gz;
		if (gx && gy && nCells / gx / gy != gz) return false; // Overflow guard
		for (uint32 i = 0; i < nCells; ++i)
		{
			uint16 n = r.U16();
			r.U16();
			r.Skip((uint32)n * 6);
			if (r.Bad()) return false;
		}
	}

	m.m_nRootNode = r.I32();
	r.U32(); // Terrain section count

	if (r.Bad()) return false;

	// Seek rather than trust the body's length, as the engine does: some models carry more after it
	r.SeekTo(nNextPos);
	return !r.Bad();
}


// Steps over the packed world tree: one bit per node, LSB first within each byte, four children for each set bit.
// Counting instead of recursing is enough for a skip, since the bit total and the end byte don't depend on traversal order.
static bool SkipWorldTreeV66(Reader &r, uint32 nNodes)
{
	const uint32 kChildren = 4;

	uint32 nSeen = 1;
	uint8 nCur = 0;
	uint32 nBit = 8; // Forces a byte read on entry
	uint32 nPending = 1;
	uint32 nGuard = 0;

	while (nPending > 0)
	{
		if (nSeen > nNodes + kChildren || ++nGuard > (1u << 24)) return false;

		if (nBit == 8)
		{
			nCur = r.U8();
			if (r.Bad()) return false;
			nBit = 0;
		}
		bool bSubdivide = ((nCur >> nBit) & 1) != 0;
		++nBit;
		--nPending;

		if (bSubdivide)
		{
			nSeen += kChildren;
			nPending += kChildren;
		}
	}

	return nSeen == nNodes;
}


bool ReadV66(const uint8 *pData, uint32 nSize, SourceWorld &out)
{
	Reader r(pData, nSize);
	if (r.U32() != 66) return false;
	uint32 nObjectDataPos = r.U32();
	uint32 nRenderDataPos = r.U32();
	// Eight dummy dwords, which v56 carries after the info string instead
	r.Skip(8 * 4);
	uint32 nInfoLen = r.U32();
	const uint8 *pInfo = r.Raw(nInfoLen);
	if (!pInfo) return false;
	out.m_sWorldInfo.assign((const char *)pInfo, nInfoLen);

	r.F32(); // Lightmap grid size
	r.Skip(12 + 12); // World extents
	r.Skip(12 + 12); // The world tree's own box
	uint32 nNodes = r.U32();
	r.U32(); // Dummy terrain depth
	if (r.Bad() || nNodes == 0 || nNodes >= (1u << 22)) return false;
	if (!SkipWorldTreeV66(r, nNodes)) return false;

	// The world models come before the object block, and must end exactly at it
	uint32 nModels = r.U32();
	if (r.Bad() || nModels > 0x10000) return false;
	out.m_Models.resize(nModels);
	for (uint32 i = 0; i < nModels; ++i)
		if (!ReadWorldModelV66(r, out.m_Models[i])) return false;
	if (r.Pos() != nObjectDataPos) return false;

	if (nObjectDataPos > nRenderDataPos || nRenderDataPos > nSize) return false;
	out.m_ObjectBlock.assign(pData + nObjectDataPos, pData + nRenderDataPos);

	// Sky objects, from the object block, as ReadV56 finds them but matched without case
	r.SeekTo(nObjectDataPos);
	uint32 nObjects = r.U32();
	std::set<std::string> SkyObjectNames;
	for (uint32 i = 0; i < nObjects && !r.Bad(); ++i)
	{
		uint32 nStart = r.Pos();
		uint16 nDataLen = r.U16();
		std::string sType = r.S16();
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
				if (pVal && sProp == pszWant && nCode == kPropTypeString && nLen >= 2)
				{
					uint16 nChars = 0;
					memcpy(&nChars, pVal, 2);
					if ((uint32)nChars + 2 <= (uint32)nLen && nChars > 0)
						SkyObjectNames.insert(ToLowerV66(std::string((const char *)pVal + 2, nChars)));
				}
			}
		}
		r.SeekTo(nStart + 2 + nDataLen);
	}
	for (uint32 i = 0; i < out.m_Models.size(); ++i)
	{
		WorldModel &m = out.m_Models[i];
		if (!m.m_sName.empty() && SkyObjectNames.find(ToLowerV66(m.m_sName)) != SkyObjectNames.end())
			m.m_bSkyObject = true;
	}
	return true;
}

}


LTRESULT world_v66_BuildV85Image(const uint8 *pSrc, uint32 nSrcSize, uint8 **ppImage, uint32 *pImageSize, WorldV56TexDimsFn pTexDims, void *pUser, WorldV56WarnFn pWarn, void *pWarnUser, uint32 nMaxBlockVerts)
{
	if (!pSrc || !ppImage || !pImageSize) return LT_INVALIDPARAMS;

	*ppImage = NULL;
	*pImageSize = 0;

	// A v66 world carries no LT1 position box, so drop any an earlier v56 world left
	ClearLT1PositionBox();

	SourceWorld src;
	if (!ReadV66(pSrc, nSrcSize, src)) return LT_INVALIDFILE;
	src.m_bSourceWorldInfoFlags = true;

	return BuildV85ImageFromSource(src, ppImage, pImageSize, pTexDims, pUser, pWarn, pWarnUser, nMaxBlockVerts);
}

bool world_v66_IsV66(const uint8 *pData, uint32 nSize)
{
	if (!pData || nSize < 4) return false;
	uint32 nVersion = 0;
	memcpy(&nVersion, pData, 4);
	return nVersion == 66;
}
