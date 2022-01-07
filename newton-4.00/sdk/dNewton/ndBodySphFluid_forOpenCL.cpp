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
#include "ndNewtonStdafx.h"
#include "ndWorld.h"
#include "ndBodySphFluid_forOpenCL.h"

ndBodySphFluid_forOpenCL::ndBodySphFluid_forOpenCL()
	:ndBodySphFluid()
	,m_box0(ndFloat32(-1e10f))
	,m_box1(ndFloat32(1e10f))
	,m_locks(1024)
	,m_pairCount(1024)
	,m_pair(1024)
	,m_hashGridMap(1024)
	,m_hashGridMapScratchBuffer(1024)
	,m_gridScans(1024)
	,m_upperDigitsIsValid()
{
	for (ndInt32 i = 0; i < D_MAX_THREADS_COUNT; i++)
	{
		m_partialsGridScans[i].Resize(256);
	}
}

ndBodySphFluid_forOpenCL::ndBodySphFluid_forOpenCL(const ndLoadSaveBase::ndLoadDescriptor& desc)
	:ndBodySphFluid(desc)
	,m_box0(ndFloat32(-1e10f))
	,m_box1(ndFloat32(1e10f))
	,m_hashGridMap()
	,m_hashGridMapScratchBuffer()
{
	// nothing was saved
	dAssert(0);
}

ndBodySphFluid_forOpenCL::~ndBodySphFluid_forOpenCL()
{
}

//void ndBodySphFluid_forOpenCL::Save(const dLoadSaveBase::dSaveDescriptor& desc) const
void ndBodySphFluid_forOpenCL::Save(const ndLoadSaveBase::ndSaveDescriptor&) const
{
	dAssert(0);
	//nd::TiXmlElement* const paramNode = CreateRootElement(rootNode, "ndBodySphFluid_forOpenCL", nodeid);
	//ndBodyParticleSet::Save(paramNode, assetPath, nodeid, shapesCache);
}

void ndBodySphFluid_forOpenCL::CaculateAABB(const ndWorld* const world)
{
	D_TRACKTIME();
	class ndBox
	{
		public:
		ndBox()
			:m_min(ndFloat32(1.0e10f))
			,m_max(ndFloat32(-1.0e10f))
		{
		}
		ndVector m_min;
		ndVector m_max;
	};

	class ndContext
	{
		public:
		ndBox m_boxes[D_MAX_THREADS_COUNT];
		const ndBodySphFluid_forOpenCL* m_fluid;
	};

	class ndCaculateAabb : public ndScene::ndBaseJob
	{
		virtual void Execute()
		{
			D_TRACKTIME();
			ndContext* const context = (ndContext*)m_context;
			const ndBodySphFluid_forOpenCL* const fluid = context->m_fluid;
			const ndArray<ndVector>& posit = fluid->m_posit;

			ndBox box;
			const ndStartEnd startEnd(posit.GetCount(), GetThreadId(), m_owner->GetThreadCount());
			for (ndInt32 i = startEnd.m_start; i < startEnd.m_end; ++i)
			{
				box.m_min = box.m_min.GetMin(posit[i]);
				box.m_max = box.m_max.GetMax(posit[i]);
			}
			context->m_boxes[GetThreadId()] = box;
		}
	};

	ndContext context;
	context.m_fluid = this;
	ndScene* const scene = world->GetScene();
	scene->SubmitJobs<ndCaculateAabb>(&context);

	ndBox box;
	const ndInt32 threadCount = scene->GetThreadCount();
	for (ndInt32 i = 0; i < threadCount; ++i)
	{
		box.m_min = box.m_min.GetMin(context.m_boxes[i].m_min);
		box.m_max = box.m_max.GetMax(context.m_boxes[i].m_max);
	}

	const ndFloat32 gridSize = CalculateGridSize();
	m_box0 = box.m_min - ndVector(gridSize);
	m_box1 = box.m_max + ndVector(gridSize);
}

void ndBodySphFluid_forOpenCL::SortByCenterType(const ndWorld* const world)
{
	D_TRACKTIME();
	class ndKey
	{
		public:
		ndKey(void* const) {}
		ndInt32 GetKey(const ndGridHash& cell) const
		{
			return 1 - ndInt32(cell.m_cellType);
		}
	};

	ndScene* const scene = world->GetScene();
	ndCountingSort<ndGridHash, ndKey, 1>(*scene, m_hashGridMap, m_hashGridMapScratchBuffer);
}

void ndBodySphFluid_forOpenCL::SortCellBuckects(const ndWorld* const world)
{
	D_TRACKTIME();
	class ndKey_xlow
	{
		public:
		ndKey_xlow(void* const) {}
		ndInt32 GetKey(const ndGridHash& cell) const
		{
			return cell.m_xLow;
		}
	};

	class ndKey_ylow
	{
		public:
		ndKey_ylow(void* const) {}
		ndInt32 GetKey(const ndGridHash& cell) const
		{
			return cell.m_yLow;
		}
	};

	class ndKey_zlow
	{
		public:
		ndKey_zlow(void* const) {}
		ndInt32 GetKey(const ndGridHash& cell) const
		{
			return cell.m_zLow;
		}
	};

	class ndKey_xhigh
	{
		public:
		ndKey_xhigh(void* const) {}
		ndInt32 GetKey(const ndGridHash& cell) const
		{
			return cell.m_xHigh;
		}
	};

	class ndKey_yhigh
	{
		public:
		ndKey_yhigh(void* const) {}
		ndInt32 GetKey(const ndGridHash& cell) const
		{
			return cell.m_yHigh;
		}
	};

	class ndKey_zhigh
	{
		public:
		ndKey_zhigh(void* const) {}
		ndInt32 GetKey(const ndGridHash& cell) const
		{
			return cell.m_zHigh;
		}
	};

	ndScene* const scene = world->GetScene();
	ndCountingSort<ndGridHash, ndKey_xlow, D_RADIX_DIGIT_SIZE>(*scene, m_hashGridMap, m_hashGridMapScratchBuffer);
	if (m_upperDigitsIsValid.m_x)
	{
		ndCountingSort<ndGridHash, ndKey_xhigh, D_RADIX_DIGIT_SIZE>(*scene, m_hashGridMap, m_hashGridMapScratchBuffer);
	}

	ndCountingSort<ndGridHash, ndKey_ylow, D_RADIX_DIGIT_SIZE>(*scene, m_hashGridMap, m_hashGridMapScratchBuffer);
	if (m_upperDigitsIsValid.m_y)
	{
		ndCountingSort<ndGridHash, ndKey_yhigh, D_RADIX_DIGIT_SIZE>(*scene, m_hashGridMap, m_hashGridMapScratchBuffer);
	}

	ndCountingSort<ndGridHash, ndKey_zlow, D_RADIX_DIGIT_SIZE>(*scene, m_hashGridMap, m_hashGridMapScratchBuffer);
	if (m_upperDigitsIsValid.m_z)
	{
		ndCountingSort<ndGridHash, ndKey_zhigh, D_RADIX_DIGIT_SIZE>(*scene, m_hashGridMap, m_hashGridMapScratchBuffer);
	}
}

void ndBodySphFluid_forOpenCL::CalculateScans(const ndWorld* const world)
{
	D_TRACKTIME();
	class ndContext
	{
		public:
		ndContext(ndBodySphFluid_forOpenCL* const fluid, const ndWorld* const world)
			:m_fluid(fluid)
		{
			memset(m_scan, 0, sizeof(m_scan));

			const ndInt32 threadCount = world->GetThreadCount();
			ndInt32 particleCount = m_fluid->m_hashGridMap.GetCount();

			ndInt32 acc0 = 0;
			ndInt32 stride = particleCount / threadCount;
			const ndGridHash* const hashGridMap = &m_fluid->m_hashGridMap[0];
			for (ndInt32 threadIndex = 0; threadIndex < threadCount; threadIndex++)
			{
				m_scan[threadIndex] = acc0;
				acc0 += stride;
				while (acc0 < particleCount && (hashGridMap[acc0].m_gridHash == hashGridMap[acc0 - 1].m_gridHash))
				{
					acc0++;
				}
			}
			m_scan[threadCount] = particleCount;
		}

		ndBodySphFluid_forOpenCL* m_fluid;
		ndInt32 m_sums[D_MAX_THREADS_COUNT + 1];
		ndInt32 m_scan[D_MAX_THREADS_COUNT + 1];
	};

	class ndCountGridScans : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			ndContext* const context = (ndContext*)m_context;
		
			const ndInt32 threadIndex = GetThreadId();
			ndBodySphFluid_forOpenCL* const fluid = context->m_fluid;
			const ndGridHash* const hashGridMap = &context->m_fluid->m_hashGridMap[0];
				
			const ndInt32 start = context->m_scan[threadIndex];
			const ndInt32 end = context->m_scan[threadIndex + 1];
			ndArray<ndInt32>& gridScans = fluid->m_partialsGridScans[threadIndex];
			ndUnsigned64 gridHash0 = hashGridMap[start].m_gridHash;

			ndInt32 count = 0;
			gridScans.SetCount(0);
			for (ndInt32 i = start; i < end; ++i)
			{
				ndUnsigned64 gridHash = hashGridMap[i].m_gridHash;
				if (gridHash != gridHash0)
				{
					gridScans.PushBack(count);
					count = 0;
					gridHash0 = gridHash;
				}
				count++;
			}
			gridScans.PushBack(count);
		}
	};

	class ndCalculateScans : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			ndContext* const context = (ndContext*)m_context;
			const ndInt32 threadIndex = GetThreadId();
			ndBodySphFluid_forOpenCL* const fluid = context->m_fluid;

			ndArray<ndInt32>& gridScans = fluid->m_gridScans;
			const ndArray<ndInt32>& partialScan = fluid->m_partialsGridScans[threadIndex];
			const ndInt32 base = context->m_sums[threadIndex];
			ndInt32 sum = context->m_scan[threadIndex];
			for (ndInt32 i = 0; i < partialScan.GetCount(); ++i)
			{
				gridScans[base + i] = sum;
				sum += partialScan[i];
			}
		}
	};

	ndContext context(this, world);
	ndScene* const scene = world->GetScene();
	scene->SubmitJobs<ndCountGridScans>(&context);

	ndInt32 scansCount = 0;
	const ndInt32 threadCount = scene->GetThreadCount();
	for (ndInt32 i = 0; i < threadCount; ++i)
	{
		context.m_sums[i] = scansCount;
		scansCount += m_partialsGridScans[i].GetCount();
	}
	context.m_sums[threadCount] = scansCount;
	m_gridScans.SetCount(scansCount + 1);
	scene->SubmitJobs<ndCalculateScans>(&context);
	m_gridScans[scansCount] = context.m_scan[threadCount];
}

void ndBodySphFluid_forOpenCL::CreateGrids(const ndWorld* const world)
{
	//#define D_USE_PARALLEL_CLASSIFY
	D_TRACKTIME();
	class ndGridCounters
	{
		public:
		ndInt32 m_scan[D_MAX_THREADS_COUNT][2];
		ndUpperDidit m_upperDigisIsValid[D_MAX_THREADS_COUNT];
	};

	class ndGridNeighborInfo
	{
		public:
		ndGridNeighborInfo()
		{
			//ndGridHash stepsCode;
			m_neighborDirs[0][0] = ndGridHash(0, 0, 0);
			m_neighborDirs[0][1] = ndGridHash(0, 0, 0);
			m_neighborDirs[0][2] = ndGridHash(0, 0, 0);
			m_neighborDirs[0][3] = ndGridHash(0, 0, 0);
			m_neighborDirs[0][4] = ndGridHash(0, 0, 0);
			m_neighborDirs[0][5] = ndGridHash(0, 0, 0);
			m_neighborDirs[0][6] = ndGridHash(0, 0, 0);
			m_neighborDirs[0][7] = ndGridHash(0, 0, 0);

			m_counter[0] = 1;
			m_isPadd[0][0] = 0;
			m_isPadd[0][1] = 1;
			m_isPadd[0][2] = 1;
			m_isPadd[0][3] = 1;
			m_isPadd[0][4] = 1;
			m_isPadd[0][5] = 1;
			m_isPadd[0][6] = 1;
			m_isPadd[0][7] = 1;

			//ndGridHash stepsCode_x;
			m_neighborDirs[1][0] = ndGridHash(0, 0, 0);
			m_neighborDirs[1][1] = ndGridHash(1, 0, 0);
			m_neighborDirs[1][2] = ndGridHash(0, 0, 0);
			m_neighborDirs[1][3] = ndGridHash(0, 0, 0);
			m_neighborDirs[1][4] = ndGridHash(0, 0, 0);
			m_neighborDirs[1][5] = ndGridHash(0, 0, 0);
			m_neighborDirs[1][6] = ndGridHash(0, 0, 0);
			m_neighborDirs[1][7] = ndGridHash(0, 0, 0);

			m_counter[1] = 2;
			m_isPadd[1][0] = 0;
			m_isPadd[1][1] = 0;
			m_isPadd[1][2] = 1;
			m_isPadd[1][3] = 1;
			m_isPadd[1][4] = 1;
			m_isPadd[1][5] = 1;
			m_isPadd[1][6] = 1;
			m_isPadd[1][7] = 1;

			//ndGridHash stepsCode_y;
			m_neighborDirs[2][0] = ndGridHash(0, 0, 0);
			m_neighborDirs[2][1] = ndGridHash(0, 1, 0);
			m_neighborDirs[2][2] = ndGridHash(0, 0, 0);
			m_neighborDirs[2][3] = ndGridHash(0, 0, 0);
			m_neighborDirs[2][4] = ndGridHash(0, 0, 0);
			m_neighborDirs[2][5] = ndGridHash(0, 0, 0);
			m_neighborDirs[2][6] = ndGridHash(0, 0, 0);
			m_neighborDirs[2][7] = ndGridHash(0, 0, 0);

			m_counter[2] = 2;
			m_isPadd[2][0] = 0;
			m_isPadd[2][1] = 0;
			m_isPadd[2][2] = 1;
			m_isPadd[2][3] = 1;
			m_isPadd[2][4] = 1;
			m_isPadd[2][5] = 1;
			m_isPadd[2][6] = 1;
			m_isPadd[2][7] = 1;

			//ndGridHash stepsCode_xy;
			m_neighborDirs[3][0] = ndGridHash(0, 0, 0);
			m_neighborDirs[3][1] = ndGridHash(1, 0, 0);
			m_neighborDirs[3][2] = ndGridHash(0, 1, 0);
			m_neighborDirs[3][3] = ndGridHash(1, 1, 0);
			m_neighborDirs[3][4] = ndGridHash(0, 0, 0);
			m_neighborDirs[3][5] = ndGridHash(0, 0, 0);
			m_neighborDirs[3][6] = ndGridHash(0, 0, 0);
			m_neighborDirs[3][7] = ndGridHash(0, 0, 0);

			m_counter[3] = 4;
			m_isPadd[3][0] = 0;
			m_isPadd[3][1] = 0;
			m_isPadd[3][2] = 0;
			m_isPadd[3][3] = 0;
			m_isPadd[3][4] = 1;
			m_isPadd[3][5] = 1;
			m_isPadd[3][6] = 1;
			m_isPadd[3][7] = 1;

			//ndGridHash stepsCode_z;
			m_neighborDirs[4][0] = ndGridHash(0, 0, 0);
			m_neighborDirs[4][1] = ndGridHash(0, 0, 1);
			m_neighborDirs[4][2] = ndGridHash(0, 0, 0);
			m_neighborDirs[4][3] = ndGridHash(0, 0, 0);
			m_neighborDirs[4][4] = ndGridHash(0, 0, 0);
			m_neighborDirs[4][5] = ndGridHash(0, 0, 0);
			m_neighborDirs[4][6] = ndGridHash(0, 0, 0);
			m_neighborDirs[4][7] = ndGridHash(0, 0, 0);

			m_counter[4] = 2;
			m_isPadd[4][0] = 0;
			m_isPadd[4][1] = 0;
			m_isPadd[4][2] = 1;
			m_isPadd[4][3] = 1;
			m_isPadd[4][4] = 1;
			m_isPadd[4][5] = 1;
			m_isPadd[4][6] = 1;
			m_isPadd[4][7] = 1;

			//ndGridHash stepsCode_xz;
			m_neighborDirs[5][0] = ndGridHash(0, 0, 0);
			m_neighborDirs[5][1] = ndGridHash(1, 0, 0);
			m_neighborDirs[5][2] = ndGridHash(0, 0, 1);
			m_neighborDirs[5][3] = ndGridHash(1, 0, 1);
			m_neighborDirs[5][4] = ndGridHash(0, 0, 0);
			m_neighborDirs[5][5] = ndGridHash(0, 0, 0);
			m_neighborDirs[5][6] = ndGridHash(0, 0, 0);
			m_neighborDirs[5][7] = ndGridHash(0, 0, 0);

			m_counter[5] = 4;
			m_isPadd[5][0] = 0;
			m_isPadd[5][1] = 0;
			m_isPadd[5][2] = 0;
			m_isPadd[5][3] = 0;
			m_isPadd[5][4] = 1;
			m_isPadd[5][5] = 1;
			m_isPadd[5][6] = 1;
			m_isPadd[5][7] = 1;

			//ndGridHash stepsCode_yz;
			m_neighborDirs[6][0] = ndGridHash(0, 0, 0);
			m_neighborDirs[6][1] = ndGridHash(0, 1, 0);
			m_neighborDirs[6][2] = ndGridHash(0, 0, 1);
			m_neighborDirs[6][3] = ndGridHash(0, 1, 1);
			m_neighborDirs[6][4] = ndGridHash(0, 0, 0);
			m_neighborDirs[6][5] = ndGridHash(0, 0, 0);
			m_neighborDirs[6][6] = ndGridHash(0, 0, 0);
			m_neighborDirs[6][7] = ndGridHash(0, 0, 0);

			m_counter[6] = 4;
			m_isPadd[6][0] = 0;
			m_isPadd[6][1] = 0;
			m_isPadd[6][2] = 0;
			m_isPadd[6][3] = 0;
			m_isPadd[6][4] = 1;
			m_isPadd[6][5] = 1;
			m_isPadd[6][6] = 1;
			m_isPadd[6][7] = 1;

			//ndGridHash stepsCode_xyz;
			m_neighborDirs[7][0] = ndGridHash(0, 0, 0);
			m_neighborDirs[7][1] = ndGridHash(1, 0, 0);
			m_neighborDirs[7][2] = ndGridHash(0, 1, 0);
			m_neighborDirs[7][3] = ndGridHash(1, 1, 0);
			m_neighborDirs[7][4] = ndGridHash(0, 0, 1);
			m_neighborDirs[7][5] = ndGridHash(1, 0, 1);
			m_neighborDirs[7][6] = ndGridHash(0, 1, 1);
			m_neighborDirs[7][7] = ndGridHash(1, 1, 1);

			m_counter[7] = 8;
			m_isPadd[7][0] = 0;
			m_isPadd[7][1] = 0;
			m_isPadd[7][2] = 0;
			m_isPadd[7][3] = 0;
			m_isPadd[7][4] = 0;
			m_isPadd[7][5] = 0;
			m_isPadd[7][6] = 0;
			m_isPadd[7][7] = 0;
		}
		
		ndGridHash m_neighborDirs[8][8];
		ndInt8 m_isPadd[8][8];
		ndInt8 m_counter[8];
	};

	class ndContext
	{
		public:
		ndContext(ndBodySphFluid_forOpenCL* const fluid)
			:m_scan()
			,m_neighbors()
			,m_fluid(fluid)
		{
		}

		ndGridCounters m_scan;
		ndGridNeighborInfo m_neighbors;
		ndBodySphFluid_forOpenCL* m_fluid;
	};

	class ndCreateGrids: public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			ndContext* const context = (ndContext*)m_context;
			ndBodySphFluid_forOpenCL* const fluid = context->m_fluid;
			const ndFloat32 radius = fluid->m_radius;
			const ndFloat32 gridSize = fluid->CalculateGridSize();

			const ndVector origin(fluid->m_box0);
			const ndVector invGridSize(ndFloat32(1.0f) / gridSize);
			const ndVector* const posit = &fluid->m_posit[0];

			ndGridHash* const dst = &fluid->m_hashGridMapScratchBuffer[0];

			const ndVector box0(-radius, -radius, -radius, ndFloat32(0.0f));
			const ndVector box1( radius,  radius,  radius, ndFloat32(0.0f));
			const ndGridNeighborInfo& gridNeighborInfo = context->m_neighbors;

			#ifdef D_USE_PARALLEL_CLASSIFY
			ndInt32* const scan = &context->m_scan.m_scan[GetThreadId()][0];
			scan[0] = 0;
			scan[1] = 0;
			#endif

			ndUpperDidit upperDigisIsValid;
			const ndStartEnd startEnd(fluid->m_posit.GetCount(), GetThreadId(), m_owner->GetThreadCount());
			for (ndInt32 i = startEnd.m_start; i < startEnd.m_end; ++i)
			{
				const ndVector r(posit[i] - origin);
				const ndVector p(r * invGridSize);
				const ndGridHash hashKey(p, i);

				upperDigisIsValid.m_x |= hashKey.m_xHigh;
				upperDigisIsValid.m_y |= hashKey.m_yHigh;
				upperDigisIsValid.m_z |= hashKey.m_zHigh;
				const ndVector p0((r + box0) * invGridSize);
				const ndVector p1((r + box1) * invGridSize);
				ndGridHash box0Hash(p0, i);
				const ndGridHash box1Hash(p1, i);
				const ndGridHash codeHash(box1Hash.m_gridHash - box0Hash.m_gridHash);

				dAssert(codeHash.m_x <= 1);
				dAssert(codeHash.m_y <= 1);
				dAssert(codeHash.m_z <= 1);
				const ndUnsigned32 code = ndUnsigned32(codeHash.m_z * 4 + codeHash.m_y * 2 + codeHash.m_x);

				#ifdef D_USE_PARALLEL_CLASSIFY
				scan[0] += gridNeighborInfo.m_counter[code];
				scan[1] += (8 - gridNeighborInfo.m_counter[code]);
				#endif

				const ndInt8* const padding = &gridNeighborInfo.m_isPadd[code][0];
				const ndGridHash* const neigborgh = &gridNeighborInfo.m_neighborDirs[code][0];
				for (ndInt32 j = 0; j < 8; j++)
				{
					ndGridHash quadrand(box0Hash);
					quadrand.m_cellIsPadd = padding[j];
					quadrand.m_gridHash += neigborgh[j].m_gridHash;
					quadrand.m_cellType = ndGridType(quadrand.m_gridHash == hashKey.m_gridHash);
					dAssert(quadrand.m_cellIsPadd <= 1);
					dAssert(quadrand.m_cellType == ((quadrand.m_gridHash == hashKey.m_gridHash) ? ndHomeGrid : ndAdjacentGrid));
					dst[i * 8 + j] = quadrand;
				}
			}

			fluid->m_upperDigitsIsValid.m_x |= upperDigisIsValid.m_x;
			fluid->m_upperDigitsIsValid.m_y |= upperDigisIsValid.m_y;
			fluid->m_upperDigitsIsValid.m_z |= upperDigisIsValid.m_z;
		}
	};

	class ndCompactGrids : public ndScene::ndBaseJob
	{
		public:
		void Execute()
		{
			D_TRACKTIME();
			ndContext* const context = (ndContext*)m_context;
			ndBodySphFluid_forOpenCL* const fluid = context->m_fluid;
			
			ndArray<ndGridHash>& dst = fluid->m_hashGridMap;
			const ndGridHash* const src = &fluid->m_hashGridMapScratchBuffer[0];
			
			const ndInt32 threadID = GetThreadId();
			const ndInt32 threadCount = m_owner->GetThreadCount();

			ndUpperDidit upperDigisIsValid;
			ndInt32* const scan = &context->m_scan.m_scan[threadID][0];

			const ndStartEnd startEnd(dst.GetCount(), threadID, threadCount);
			for (ndInt32 i = startEnd.m_start; i < startEnd.m_end; ++i)
			{
				const ndGridHash grid(src[i]);
				const ndInt32 key = grid.m_cellIsPadd;
				const ndInt32 index = scan[key];

				upperDigisIsValid.m_x |= grid.m_xHigh;
				upperDigisIsValid.m_y |= grid.m_yHigh;
				upperDigisIsValid.m_z |= grid.m_zHigh;

				dst[index] = grid;
				scan[key] = index + 1;
			}
			context->m_scan.m_upperDigisIsValid[threadID] = upperDigisIsValid;
		}
	};

	dAssert (sizeof(ndGridHash) <= 16);
	ndScene* const scene = world->GetScene();

	ndContext context(this);
	m_hashGridMap.SetCount(m_posit.GetCount() * 8);
	m_hashGridMapScratchBuffer.SetCount(m_posit.GetCount() * 8);
	scene->SubmitJobs<ndCreateGrids>(&context);

#ifdef D_USE_PARALLEL_CLASSIFY
	ndInt32 sum = 0;
	const ndInt32 threadCount = scene->GetThreadCount();
	for (ndInt32 i = 0; i < 2; ++i)
	{
		for (ndInt32 j = 0; j < threadCount; ++j)
		{
			ndInt32 partialSum = context.m_scan.m_scan[j][i];
			context.m_scan.m_scan[j][i] = sum;
			sum += partialSum;
		}
	}

	ndInt32 gridCount = context.m_scan.m_scan[0][1] - context.m_scan.m_scan[0][0];
	scene->SubmitJobs<ndCompactGrids>(&context);

	ndUpperDidit upperDigits;
	for (ndInt32 j = 0; j < threadCount; ++j)
	{
		upperDigits.m_x |= context.m_scan.m_upperDigisIsValid[j].m_x;
		upperDigits.m_y |= context.m_scan.m_upperDigisIsValid[j].m_y;
		upperDigits.m_z |= context.m_scan.m_upperDigisIsValid[j].m_z;
	}
#else
	ndInt32 gridCount = 0;
	ndUpperDidit upperDigits;
	{
		D_TRACKTIME();
		for (ndInt32 i = 0; i < m_hashGridMapScratchBuffer.GetCount(); ++i)
		{
			const ndGridHash cell(m_hashGridMapScratchBuffer[i]);
			upperDigits.m_x |= cell.m_xHigh;
			upperDigits.m_y |= cell.m_yHigh;
			upperDigits.m_z |= cell.m_zHigh;

			m_hashGridMap[gridCount] = cell;
			gridCount += (1 - ndInt32 (cell.m_cellIsPadd));
		}
	}
#endif
	m_hashGridMap.SetCount(gridCount);
	m_upperDigitsIsValid = upperDigits;
}

void ndBodySphFluid_forOpenCL::SortGrids(const ndWorld* const world)
{
	D_TRACKTIME();
	SortByCenterType(world);
	SortCellBuckects(world);

	#ifdef _DEBUG
	for (ndInt32 i = 0; i < (m_hashGridMap.GetCount() - 1); ++i)
	{
		const ndGridHash& entry0 = m_hashGridMap[i + 0];
		const ndGridHash& entry1 = m_hashGridMap[i + 1];
		ndUnsigned64 gridHashA = entry0.m_gridHash;
		ndUnsigned64 gridHashB = entry1.m_gridHash;
		dAssert(gridHashA <= gridHashB);
	}
	#endif
}

//void ndBodySphFluid_forOpenCL::CalculateAccelerations(const ndWorld* const world)
void ndBodySphFluid_forOpenCL::CalculateAccelerations(const ndWorld* const)
{
	D_TRACKTIME();
	//class ndCalculateDensity: public ndScene::ndBaseJob
	//{
	//	public:
	//	class ndContext
	//	{
	//		public:
	//		ndContext(ndBodySphFluid_forOpenCL* const fluid)
	//			:m_fluid(fluid)
	//		{
	//		}
	//
	//		ndBodySphFluid_forOpenCL* m_fluid;
	//	};
	//
	//	void AddPair(ndInt32 particle0, ndInt32 particle1)
	//	{
	//
	//	}
	//	
	//	virtual void Execute()
	//	{
	//		D_TRACKTIME();
	//		ndWorld* const world = m_owner->GetWorld();
	//		ndBodySphFluid_forOpenCL* const fluid = ((ndContext*)m_context)->m_fluid;
	//
	//		const ndArray<ndGridHash>& hashGridMap = fluid->m_hashGridMap;
	//		const ndArray<ndInt32>& gridScans = fluid->m_gridScans;
	//
	//		const ndStartEnd startEnd(gridScans.GetCount() - 1, GetThreadId(), world->GetThreadCount());
	//		for (ndInt32 i = startEnd.m_start; i < startEnd.m_end; ++i)
	//		{
	//			const ndInt32 start = gridScans[i];
	//			const ndInt32 count1 = gridScans[i + 1] - start;
	//			const ndInt32 count0 = count1 - 1;
	//			for (ndInt32 j = 0; (j < count0) && (hashGridMap[start + j].m_cellType == ndHomeGrid); ++j)
	//			{
	//				ndGridHash hash0 = hashGridMap[start + j];
	//				ndInt32 k = j + 1;
	//				for (; k < count1 && (hashGridMap[start + k].m_cellType == ndHomeGrid); k++)
	//				{
	//					ndGridHash hash1 = hashGridMap[start + k];
	//					if (hash0.m_particleIndex < hash1.m_particleIndex)
	//					{
	//						//dTrace(("%d %d\n", hash0.m_particleIndex, hash1.m_particleIndex));
	//						CalculateDentity(hash0.m_particleIndex, hash1.m_particleIndex);
	//					}
	//				}
	//
	//				for (; k < count1; k++)
	//				{
	//					ndGridHash hash1 = hashGridMap[start + k];
	//					//dTrace(("%d %d\n", hash0.m_particleIndex, hash1.m_particleIndex));
	//					CalculateDentity(hash0.m_particleIndex, hash1.m_particleIndex);
	//				}
	//			}
	//		}
	//		//dTrace(("xxxx"));
	//	}
	//};
	//
	//ndScene* const scene = world->GetScene();
	////m_particlesPairs.SetCount(0);
	////ndBodySphFluidCreatePair::ndContext context(this);
	//
	//ndCalculateDensity::ndContext densityContext(this);
	//scene->SubmitJobs<ndCalculateDensity>(&densityContext);
}

void ndBodySphFluid_forOpenCL::BuildPairs(const ndWorld* const world)
{
	D_TRACKTIME();
	class ndAddPairs : public ndScene::ndBaseJob
	{
		public:
		class ndPairInfo
		{
			public:
			ndPairInfo(ndBodySphFluid_forOpenCL* const fluid)
				:m_locks(fluid->m_locks)
				,m_posit(fluid->m_posit)
				,m_pairCount(fluid->m_pairCount)
				,m_pair(fluid->m_pair)
				,m_dist2(fluid->CalculateGridSize() * fluid->CalculateGridSize())
			{
			}

			ndArray<ndSpinLock>& m_locks;
			const ndArray<ndVector>& m_posit;
			ndArray<ndInt8>& m_pairCount;
			ndArray<ndParticlePair>& m_pair;
			ndFloat32 m_dist2;
		};

		void AddPair(ndPairInfo& info, ndInt32 particle0, ndInt32 particle1)
		{
			ndVector p1p0(info.m_posit[particle0] - info.m_posit[particle1]);
			ndFloat32 dist2(p1p0.DotProduct(p1p0).GetScalar());
			if (dist2 <= info.m_dist2)
			{
				{
					ndSpinLock lock(info.m_locks[particle0]);
					ndInt8 count = info.m_pairCount[particle0];
					if (count < 32)
					{
						info.m_pair[particle0].m_m1[count] = particle1;
						info.m_pairCount[particle0] = count + 1;
					}
				}

				{
					ndSpinLock lock(info.m_locks[particle1]);
					ndInt8 count = info.m_pairCount[particle1];
					if (count < 32)
					{
						info.m_pair[particle1].m_m1[count] = particle0;
						info.m_pairCount[particle1] = count + 1;
					}
				}
			}
		}

		void proccessCell(const ndArray<ndGridHash>& hashGridMap, ndPairInfo& info, const ndInt32 start, const ndInt32 count)
		{
			//D_TRACKTIME();
			const ndInt32 count0 = count - 1;
			for (ndInt32 i = 0; i < count0; ++i)
			{
				const ndGridHash hash0 = hashGridMap[start + i];
				bool test0 = (hash0.m_cellType == ndHomeGrid);
				for (ndInt32 j = i; j < count; ++j)
				{
					const ndGridHash hash1 = hashGridMap[start + j];
					bool test1 = (hash1.m_cellType == ndHomeGrid);
					bool test2 = (hash0.m_particleIndex < hash1.m_particleIndex);
					bool test = test0 & (test1 | test2);
					if (test)
					{
						AddPair(info, hash0.m_particleIndex, hash1.m_particleIndex);
					}
				}
			}
		}

		virtual void Execute()
		{
			D_TRACKTIME();
			ndWorld* const world = m_owner->GetWorld();
			ndBodySphFluid_forOpenCL* const fluid = (ndBodySphFluid_forOpenCL*)m_context;
			
			const ndArray<ndGridHash>& hashGridMap = fluid->m_hashGridMap;
			const ndArray<ndInt32>& gridScans = fluid->m_gridScans;

			ndPairInfo info(fluid);
						
			const ndStartEnd startEnd(gridScans.GetCount() - 1, GetThreadId(), world->GetThreadCount());
			for (ndInt32 i = startEnd.m_start; i < startEnd.m_end; ++i)
			{
				const ndInt32 start = gridScans[i];
				const ndInt32 count = gridScans[i + 1] - start;
				proccessCell(hashGridMap, info, start, count);
			}
		}
	};

	ndInt32 countReset = m_locks.GetCount();
	m_pair.SetCount(m_posit.GetCount());
	m_locks.SetCount(m_posit.GetCount());
	m_pairCount.SetCount(m_posit.GetCount());
	for (ndInt32 i = countReset; i < m_locks.GetCount(); ++i)
	{
		m_locks[i].Unlock();
	}
	for (ndInt32 i = 0; i < m_pairCount.GetCount(); ++i)
	{
		m_pairCount[i] = 0;
	}
	
	ndScene* const scene = world->GetScene();
	scene->SubmitJobs<ndAddPairs>(this);
}

void ndBodySphFluid_forOpenCL::Update(const ndWorld* const world, ndFloat32)
{
	// do the scene management 
	CaculateAABB(world);
	CreateGrids(world);
	SortGrids(world);
	CalculateScans(world);
	BuildPairs(world);

	// do the physics
	//CalculateAccelerations(world);
}