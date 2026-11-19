#include "world_convert_internal.h"

using namespace worldconv;

namespace worldconv {


// LithTech 2.x worlds (DAT v66)
// Only the reader is new. Everything downstream is shared with v56 apart from the lightmap UV rule, which m_dLMGrid selects.
// The layout differs from v56's: twelve header counts, the spare word before the name, three surface vectors instead of six,
// 14-byte nodes, a uint32 leaf polygon count, points with a normal, and no packed lightmap block.

static std::string ToLowerV66(const std::string &s)
{
	std::string out(s);
	for (size_t i = 0; i < out.size(); ++i)
		if (out[i] >= 'A' && out[i] <= 'Z') out[i] = (char)(out[i] - 'A' + 'a');
	return out;
}

// LT2's lightmap basis.
// The row whose second vector has the strictly greatest dot with the plane normal is picked, then U = v1 x n and V = n x U.
static const double kLT2LMPlanes[6][2][3] =
{
	{ { 0.0, 0.0, -1.0 }, { 0.0, 1.0, 0.0 } },
	{ { 0.0, 0.0, 1.0 }, { 0.0, -1.0, 0.0 } },
	{ { 0.0, 1.0, 0.0 }, { 0.0, 0.0, 1.0 } },
	{ { 0.0, -1.0, 0.0 }, { 0.0, 0.0, -1.0 } },
	{ { 0.0, -1.0, 0.0 }, { 1.0, 0.0, 0.0 } },
	{ { 0.0, -1.0, 0.0 }, { -1.0, 0.0, 0.0 } },
};

static void LT2LightmapBasis(const float *pNormal, double U[3], double V[3])
{
	const double n[3] = { (double)pNormal[0], (double)pNormal[1], (double)pNormal[2] };
	double dBest = -2.0;
	int nBest = 0;
	for (int i = 0; i < 6; ++i)
	{
		const double *v2 = kLT2LMPlanes[i][1];
		double d = v2[0] * n[0] + v2[1] * n[1] + v2[2] * n[2];
		if (d > dBest) { dBest = d; nBest = i; }
	}
	const double *a = kLT2LMPlanes[nBest][0];
	U[0] = a[1] * n[2] - a[2] * n[1];
	U[1] = a[2] * n[0] - a[0] * n[2];
	U[2] = a[0] * n[1] - a[1] * n[0];
	V[0] = n[1] * U[2] - n[2] * U[1];
	V[1] = n[2] * U[0] - n[0] * U[2];
	V[2] = n[0] * U[1] - n[1] * U[0];
}

static bool ReadWorldModelV66(Reader &r, WorldModel &m)
{
	uint32 nNextPos = r.U32();
	r.Skip(32); // Dummy block, 8 dwords
	m.m_nSrcWorldInfoFlags = r.U32(); // A real Jupiter world info bitfield
	r.U32(); // Spare, which v56 puts after the name
	m.m_sName = r.S16();
	m.m_bSkyObject = false;
	m.m_nLT1Translucent = -1; // Stamped by StampWholeModelTranslucency
	m.m_bHasVisPrune = false;
	m.m_dLMGrid = 0.0; // Set by AttachLightmapsV66

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

	// Normal and distance. The normals are kept for the lightmap basis.
	std::vector<float> PlaneNormals;
	for (uint32 i = 0; i < m.m_nPlanes && !r.Bad(); ++i)
	{
		float n[3];
		r.V3(n);
		r.F32();
		PlaneNormals.push_back(n[0]);
		PlaneNormals.push_back(n[1]);
		PlaneNormals.push_back(n[2]);
	}
	if (r.Bad()) return false;

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
		uint32 nPlane = r.U32();
		s.m_nFlags = r.U32();
		r.U8(); r.U8(); r.U8(); r.U8();
		if (r.U8() == 1) // The fifth byte gates the effect strings
		{
			s.m_sEffect = r.S16();
			s.m_sEffectParams = r.S16();
		}
		s.m_nTexFlags = r.U16(); // After the effect strings, where v56 has it before
		if (nPlane < m.m_nPlanes)
		{
			s.m_bLMBasisMissing = false;
			LT2LightmapBasis(&PlaneNormals[(size_t)nPlane * 3], s.m_LMUd, s.m_LMVd);
		}
		else
		{
			s.m_bLMBasisMissing = true;
		}
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

	// The PBlockTable, a spatial grid nothing in LT2's rendering or collision reads back.
	// It is not the object light grid, which LT2 builds at load from the lights.
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


// Decodes exactly nTexels texels of an LT2 lightmap blob to RGB888, or fails.
// The blob is u16 RGB555 words, and bit 15 means a u8 run count follows.
static bool DecodeLM555(const uint8 *p, uint32 n, uint32 nTexels, std::vector<uint8> &out)
{
	out.clear();
	uint32 i = 0, nGot = 0;
	while (i + 2 <= n)
	{
		uint32 word = (uint32)p[i] | ((uint32)p[i + 1] << 8);
		i += 2;
		uint32 nRun = 1;
		if (word & 0x8000)
		{
			if (i >= n) return false;
			nRun = p[i];
			i += 1;
		}
		if (nRun == 0) return false;
		uint32 rr = (word >> 10) & 0x1F, gg = (word >> 5) & 0x1F, bb = word & 0x1F;
		uint8 px[3] = { (uint8)((rr << 3) | (rr >> 2)), (uint8)((gg << 3) | (gg >> 2)), (uint8)((bb << 3) | (bb >> 2)) };
		for (uint32 k = 0; k < nRun; ++k)
			out.insert(out.end(), px, px + 3);
		nGot += nRun;
	}
	return i == n && nGot == nTexels;
}

// The lightmaps, from the render data section: frame 0 of animation 0, which is the base lighting in every retail world.
// The switchable light animations after it are not baked.
// Pages are staged, and kept only if the whole of frame 0 reads cleanly.
static void AttachLightmapsV66(const uint8 *pData, uint32 nSize, uint32 nPos, float fGrid, SourceWorld &out)
{
	if (!(fGrid > 0.0f))
		return;
	for (size_t i = 0; i < out.m_Models.size(); ++i)
		out.m_Models[i].m_dLMGrid = (double)fGrid;

	if ((uint64)nPos + 20 > (uint64)nSize)
		return;
	Reader r(pData, nSize);
	r.SeekTo(nPos + 16);
	uint32 nAnims = r.U32();
	if (r.Bad() || nAnims < 1)
		return;
	uint16 nNameLen = r.U16();
	r.Skip(nNameLen);
	uint32 nShadow = r.U32();
	uint32 nFrames = r.U32();
	uint32 nRefs = r.U32();
	if (r.Bad() || nShadow != 0 || nFrames < 1)
		return;
	if ((uint64)r.Pos() + (uint64)nRefs * 4 > (uint64)nSize)
		return;
	// A model index and a polygon index for each blob
	std::vector<uint16> Refs((size_t)nRefs * 2);
	for (size_t k = 0; k < Refs.size(); ++k)
		Refs[k] = r.U16();

	std::map<std::pair<uint32, uint32>, WorldModel::LMPage> Staged;
	std::vector<uint8> rgb;
	for (uint32 k = 0; k < nRefs; ++k)
	{
		uint32 nBlob = r.U32();
		if (r.Bad() || (uint64)r.Pos() + nBlob > (uint64)nSize)
			return;
		const uint8 *pBlob = pData + r.Pos();
		r.Skip(nBlob);
		uint32 mi = Refs[(size_t)k * 2], pi = Refs[(size_t)k * 2 + 1];
		if (mi >= out.m_Models.size() || pi >= out.m_Models[mi].m_Polies.size())
			continue;
		const Poly &p = out.m_Models[mi].m_Polies[pi];
		uint32 w = p.m_nLMW, h = p.m_nLMH;
		if (w < 1 || h < 1 || w > 255 || h > 255)
			continue;
		if (DecodeLM555(pBlob, nBlob, w * h, rgb))
		{
			WorldModel::LMPage &page = Staged[std::make_pair(mi, pi)];
			page.m_nW = (uint8)w;
			page.m_nH = (uint8)h;
			page.m_Texels = rgb;
		}
	}
	for (std::map<std::pair<uint32, uint32>, WorldModel::LMPage>::const_iterator it = Staged.begin(); it != Staged.end(); ++it)
		out.m_Models[it->first.first].m_Lightmaps[it->first.second] = it->second;
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

	float fLMGrid = r.F32(); // Lightmap grid size
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

	AttachLightmapsV66(pData, nSize, nRenderDataPos, fLMGrid, out);
	return true;
}

}


LTRESULT world_v66_BuildV85Image(const uint8 *pSrc, uint32 nSrcSize, uint8 **ppImage, uint32 *pImageSize, WorldV56TexDimsFn pTexDims, void *pUser, WorldV56WarnFn pWarn, void *pWarnUser, uint32 nMaxBlockVerts, bool bLT25Lights)
{
	if (!pSrc || !ppImage || !pImageSize) return LT_INVALIDPARAMS;

	*ppImage = NULL;
	*pImageSize = 0;

	// A v66 world carries no LT1 position box, so drop any an earlier v56 world left
	ClearLT1PositionBox();

	SourceWorld src;
	if (!ReadV66(pSrc, nSrcSize, src)) return LT_INVALIDFILE;
	src.m_bSourceWorldInfoFlags = true;
	src.m_bLT25Lights = bLT25Lights;

	// LT2 sorts a world model as LT1 does, from polygon 0, and never reads SURF_TRANSPARENT on the main world
	StampWholeModelTranslucency(src);

	return BuildV85ImageFromSource(src, ppImage, pImageSize, pTexDims, pUser, pWarn, pWarnUser, nMaxBlockVerts);
}

bool world_v66_IsV66(const uint8 *pData, uint32 nSize)
{
	if (!pData || nSize < 4) return false;
	uint32 nVersion = 0;
	memcpy(&nVersion, pData, 4);
	return nVersion == 66;
}
