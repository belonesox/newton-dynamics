/* Copyright (c) <2003-2021> <Julio Jerez, Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 
* 3. This notice may not be removed or altered from any source distribution.
*/

#include "ndCoreStdafx.h"
#include "ndSort.h"
#include "ndStack.h"
#include "ndGoogol.h"
#include "ndSmallDeterminant.h"
#include "ndDelaunayTetrahedralization.h"

ndDelaunayTetrahedralization::ndDelaunayTetrahedralization(const dFloat64* const vertexCloud, dInt32 count, dInt32 strideInByte, dFloat64 distTol)
	:ndConvexHull4d()
{
	ndSetPrecisionDouble precision;
	ndStack<ndBigVector> pool(count + 2);

	ndBigVector* const points = &pool[0];
	dInt32 stride = dInt32 (strideInByte / sizeof (dFloat64));
	for (dInt32 i = 0; i < count; i ++) 
	{
		dFloat64 x = dRoundToFloat (vertexCloud[i * stride + 0]);
		dFloat64 y = dRoundToFloat (vertexCloud[i * stride + 1]);
		dFloat64 z = dRoundToFloat (vertexCloud[i * stride + 2]);
		points[i] = ndBigVector (x, y, z, x * x + y * y + z * z);
	}

	dInt32 oldCount = count;
	BuildHull (&pool[0].m_x, sizeof (ndBigVector), count, distTol);
	if (oldCount > m_count) 
	{
		// the mesh is convex, need to add two steiners point to make tractable
		ndBigVector origin (dFloat64 (0.0f));
		dFloat64 maxW = dFloat64 (-1.0e20f);
		for (dInt32 i = 0; i < count; i++) 
		{
			dFloat64 x = dRoundToFloat(vertexCloud[i * stride + 0]);
			dFloat64 y = dRoundToFloat(vertexCloud[i * stride + 1]);
			dFloat64 z = dRoundToFloat(vertexCloud[i * stride + 2]);
			points[i] = ndBigVector(x, y, z, x * x + y * y + z * z);
			origin += points[i];
			maxW = dMax (points[i].m_w, maxW);
		}
		origin = origin.Scale (dFloat64 (1.0f) / count);
		points[count + 0] = origin;
		points[count + 1] = origin;
		points[count + 0].m_w += dFloat64 (1.0f);
		points[count + 1].m_w -= dFloat64 (1.0f);
 		BuildHull (&pool[0].m_x, sizeof (ndBigVector), count + 2, distTol);
	}

	if (oldCount > m_count) 
	{
		// this is probably a regular convex solid, which will have a zero volume hull
		// add the rest of the points by incremental insertion with small perturbation
		dInt32 hullCount = m_count;
		
		for (dInt32 i = 0; i < count; i ++) 
		{
			bool inHull = false;
			const ndConvexHull4dVector* const hullPoints = &m_points[0];
			for (dInt32 j = 0; j < hullCount; j ++) 
			{
				if (hullPoints[j].m_index == i) 
				{
					inHull = true;
					break;
				}			
			}
			if (!inHull) 
			{
				ndBigVector q (points[i]);
				dInt32 index = AddVertex(q);
				if (index == -1) 
				{
					q.m_x += dFloat64 (1.0e-3f);
					q.m_y += dFloat64 (1.0e-3f);
					q.m_z += dFloat64 (1.0e-3f);
					index = AddVertex(q);
					dAssert (index != -1);
				}
				dAssert (index != -1);
				m_points[index].m_index = i;
			}
		}
	}

	SortVertexArray ();

	ndTempList::FlushFreeList();
}

ndDelaunayTetrahedralization::~ndDelaunayTetrahedralization()
{
}

dInt32 ndDelaunayTetrahedralization::AddVertex (const ndBigVector& vertex)
{
	ndSetPrecisionDouble precision;

	ndBigVector p (vertex);
	dAssert(p.m_w == dFloat32(0.0f));
	p.m_w = p.DotProduct(p).GetScalar();
	dInt32 index = ndConvexHull4d::AddVertex(p);

	return index;
}

void ndDelaunayTetrahedralization::SortVertexArray ()
{
	ndConvexHull4dVector* const points = &m_points[0];
	for (ndNode* node = GetFirst(); node; node = node->GetNext())
	{	
		ndConvexHull4dTetraherum* const tetra = &node->GetInfo();
		for (dInt32 i = 0; i < 4; i ++) 
		{
			ndConvexHull4dTetraherum::ndTetrahedrumFace& face = tetra->m_faces[i];
			for (dInt32 j = 0; j < 4; j ++) 
			{
				dInt32 index = face.m_index[j];
				face.m_index[j] = points[index].m_index;
			}
		}
	}

	class CompareVertex
	{
		public:
		dInt32 Compare(const ndConvexHull4dVector& elementA, const ndConvexHull4dVector& elementB, void* const) const
		{
			if (elementA.m_index < elementB.m_index)
			{
				return -1;
			}
			else if (elementA.m_index > elementB.m_index)
			{
				return 1;
			}
			return 0;
		}
	};
	ndSort<ndConvexHull4dVector, CompareVertex>(points, m_count);
}

void ndDelaunayTetrahedralization::RemoveUpperHull ()
{
	ndSetPrecisionDouble precision;

	ndNode* nextNode = NULL;
	for (ndNode* node = GetFirst(); node; node = nextNode) 
	{
		nextNode = node->GetNext();

		ndConvexHull4dTetraherum* const tetra = &node->GetInfo();
		tetra->SetMark(0);
		dFloat64 w = GetTetraVolume (tetra);
		if (w >= dFloat64 (0.0f)) 
		{
			DeleteFace(node);
		}
	}
}

void ndDelaunayTetrahedralization::DeleteFace (ndNode* const node)
{
	ndConvexHull4dTetraherum* const tetra = &node->GetInfo();
	for (dInt32 i = 0; i < 4; i ++) 
	{
		ndNode* const twinNode = tetra->m_faces[i].m_twin;
		if (twinNode) 
		{
			ndConvexHull4dTetraherum* const twinTetra = &twinNode->GetInfo();
			for (dInt32 j = 0; j < 4; j ++) 
			{
				if (twinTetra->m_faces[j].m_twin == node) 
				{
					twinTetra->m_faces[j].m_twin = NULL;
					break;
				}
			}
		}
	}
	ndConvexHull4d::DeleteFace (node);
}