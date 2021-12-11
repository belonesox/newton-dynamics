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

/****************************************************************************
*
*  Visual C++ 6.0 created by: Julio Jerez
*
****************************************************************************/
#ifndef __ND_POLYGONSOUP_BUILDER_H__
#define __ND_POLYGONSOUP_BUILDER_H__

#include "ndCoreStdafx.h"
#include "ndTypes.h"
#include "ndPlane.h"
#include "ndArray.h"
#include "ndVector.h"
#include "ndMatrix.h"

class ndAdjacentdFace
{
	public:
	ndPlane m_normal;
	dInt32 m_count;
	dInt32 *m_index;
	dInt64 m_edgeMap[256];
};

class ndPolygonSoupBuilder: public ndClassAlloc 
{
	class dgFaceMap;
	class dgFaceInfo;
	class dgFaceBucket;
	class dgPolySoupFilterAllocator;
	public:

	D_CORE_API ndPolygonSoupBuilder ();
	D_CORE_API ndPolygonSoupBuilder (const ndPolygonSoupBuilder& sopurce);
	D_CORE_API ~ndPolygonSoupBuilder ();

	D_CORE_API void Begin();
	D_CORE_API void End(bool optimize);
	D_CORE_API void AddFace(const dFloat32* const vertex, dInt32 strideInBytes, dInt32 vertexCount, const dInt32 faceId);
	D_CORE_API void AddFaceIndirect(const dFloat32* const vertex, dInt32 strideInBytes, dInt32 faceId, const dInt32* const indexArray, dInt32 indexCount);

	D_CORE_API void SavePLY(const char* const fileName) const;

	private:
	void Optimize(dInt32 faceId, const dgFaceBucket& faceBucket, const ndPolygonSoupBuilder& source);

	void Finalize();
	void OptimizeByIndividualFaces();
	void FinalizeAndOptimize(dInt32 id);
	dInt32 FilterFace (dInt32 count, dInt32* const indexArray);
	dInt32 AddConvexFace (dInt32 count, dInt32* const indexArray, dInt32* const  facesArray);
	void PackArray();

	public:
	class ndVertexArray: public ndArray<ndBigVector>
	{	
		public:
		ndVertexArray()
			:ndArray<ndBigVector>()
		{
		}

		ndVertexArray(dInt32 count)
			:ndArray<ndBigVector>(count)
		{
		}
	};

	class ndIndexArray: public ndArray<dInt32>
	{
		public:
		ndIndexArray()
			:ndArray<dInt32>()
		{
		}

		ndIndexArray(dInt32 count)
			:ndArray<dInt32>(count)
		{
		}
	};

	ndIndexArray m_faceVertexCount;
	ndIndexArray m_vertexIndex;
	ndIndexArray m_normalIndex;
	ndVertexArray m_vertexPoints;
	ndVertexArray m_normalPoints;
	dInt32 m_run;
};

#endif

