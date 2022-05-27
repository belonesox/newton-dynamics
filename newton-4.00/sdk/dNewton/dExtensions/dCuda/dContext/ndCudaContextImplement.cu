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

#include "ndCudaStdafx.h"
#include "ndCudaUtils.h"
#include "ndCudaContext.h"
#include "ndCudaPrefixScan.cuh"
#include "ndCudaCountingSort.cuh"
#include "ndCudaContextImplement.h"

#define D_CUDA_SCENE_GRID_SIZE		8.0f
#define D_CUDA_SCENE_INV_GRID_SIZE	(1.0f/D_CUDA_SCENE_GRID_SIZE) 

__global__ void ndCudaEndFrame(ndCudaSceneInfo& info, int frameCount)
{
	info.m_frameCount = frameCount;
}

__global__ void ndCudaInitTransforms(ndCudaSceneInfo& info)
{
	int index = threadIdx.x + blockDim.x * blockIdx.x;
	if (index < info.m_bodyArray.m_size)
	{
		const ndCudaBodyProxy* src = info.m_bodyArray.m_array;
		ndCudaSpatialVector* dst0 = info.m_transformBuffer0.m_array;
		ndCudaSpatialVector* dst1 = info.m_transformBuffer1.m_array;
	
		dst0[index].m_linear = src[index].m_posit;
		dst0[index].m_angular = src[index].m_rotation;
		dst1[index].m_linear = src[index].m_posit;
		dst1[index].m_angular = src[index].m_rotation;
	}
}

__global__ void ndCudaInitBodyArrayInternal(ndCudaSceneInfo& info)
{
	__shared__  ndCudaBoundingBox cacheAabb[D_THREADS_PER_BLOCK];

	const unsigned threadId = threadIdx.x;
	const unsigned index = threadId + blockDim.x * blockIdx.x;
	const unsigned bodyCount = info.m_bodyArray.m_size - 1;
	if (index < bodyCount)
	{
		ndCudaBodyProxy* bodyArray = info.m_bodyArray.m_array;
		ndCudaBodyProxy& body = bodyArray[index];

		// calculate shape global Matrix
		body.m_globalSphapeRotation = body.m_localRotation * body.m_rotation;
		ndCudaMatrix3x3 matrix(body.m_globalSphapeRotation.GetMatrix3x3());
		body.m_globalSphapePosition = matrix.RotateVector(body.m_localPosition) + body.m_posit;

		matrix.m_front = matrix.m_front.Scale(body.m_scale.x);
		matrix.m_up = matrix.m_up.Scale(body.m_scale.y);
		matrix.m_right = matrix.m_right.Scale(body.m_scale.z);
		matrix = body.m_alignRotation.GetMatrix3x3() * matrix;

		const ndCudaVector origin(matrix.RotateVector(body.m_obbOrigin) + body.m_globalSphapePosition);
		const ndCudaVector size(matrix.m_front.Abs().Scale(body.m_obbSize.x) + matrix.m_up.Abs().Scale(body.m_obbSize.y) + matrix.m_right.Abs().Scale(body.m_obbSize.z));

		const ndCudaVector padding(1.0f / 16.0f);
		const ndCudaVector minBox(origin - size - padding);
		const ndCudaVector maxBox(origin + size + padding);

		// save aabb and calculate bonding box for this thread block
		body.m_minAabb = minBox;
		body.m_maxAabb = maxBox;
		cacheAabb[threadId].m_min = minBox;
		cacheAabb[threadId].m_max = maxBox;
	}

	const unsigned lastBlock = bodyCount / D_THREADS_PER_BLOCK;
	if (lastBlock == blockIdx.x)
	{
		__syncthreads();
		const unsigned lastId = bodyCount - D_THREADS_PER_BLOCK * lastBlock;
		const ndCudaBoundingBox box(cacheAabb[0]);
		if (threadId >= lastId)
		{
			cacheAabb[threadId] = box;
		}
	}
	__syncthreads();

	ndCudaBoundingBox* bBox = info.m_bodyAabbArray.m_array;
	for (int i = D_THREADS_PER_BLOCK / 2; i; i = i >> 1)
	{
		if (threadId < i)
		{
			cacheAabb[threadId].m_min = cacheAabb[threadId].m_min.Min(cacheAabb[threadId + i].m_min);
			cacheAabb[threadId].m_max = cacheAabb[threadId].m_max.Max(cacheAabb[threadId + i].m_max);
		}
		__syncthreads();
	}

	if (threadId == 0)
	{
		bBox[blockIdx.x].m_min = cacheAabb[0].m_min;
		bBox[blockIdx.x].m_max = cacheAabb[0].m_max;
	}
};

__global__ void ndCudaMergeAabbInternal(ndCudaSceneInfo& info)
{
	__shared__  ndCudaBoundingBox cacheAabb[D_THREADS_PER_BLOCK];

	const ndCudaBoundingBox* bBoxOut = info.m_bodyAabbArray.m_array;

	const unsigned threadId = threadIdx.x;
	const unsigned boxCount = info.m_bodyAabbArray.m_size - 1;
	const unsigned aabbBlocks = boxCount / D_THREADS_PER_BLOCK;
	const unsigned boxLastRow = boxCount - aabbBlocks * D_THREADS_PER_BLOCK;

	cacheAabb[threadId] = bBoxOut[0];
	if (threadId < boxLastRow)
	{
		cacheAabb[threadId] = bBoxOut[aabbBlocks * D_THREADS_PER_BLOCK + threadId];
	}
	__syncthreads();

	unsigned base = 0;
	for (int i = 0; i < aabbBlocks; i++)
	{
		cacheAabb[threadId].m_min = cacheAabb[threadId].m_min.Min(cacheAabb[base + threadId].m_min);
		cacheAabb[threadId].m_max = cacheAabb[threadId].m_max.Min(cacheAabb[base + threadId].m_max);
		base += D_THREADS_PER_BLOCK;
	}

	__syncthreads();
	for (int i = D_THREADS_PER_BLOCK / 2; i; i = i >> 1)
	{
		if (threadId < i)
		{
			cacheAabb[threadId].m_min = cacheAabb[threadId].m_min.Min(cacheAabb[threadId + i].m_min);
			cacheAabb[threadId].m_max = cacheAabb[threadId].m_max.Max(cacheAabb[threadId + i].m_max);
		}
		__syncthreads();
	}

	if (threadIdx.x == 0)
	{
		ndCudaVector minBox((cacheAabb[0].m_min.Scale(D_CUDA_SCENE_INV_GRID_SIZE).Floor()).Scale(D_CUDA_SCENE_GRID_SIZE));
		ndCudaVector maxBox((cacheAabb[0].m_max.Scale(D_CUDA_SCENE_INV_GRID_SIZE).Floor()).Scale(D_CUDA_SCENE_GRID_SIZE) + ndCudaVector(D_CUDA_SCENE_GRID_SIZE));
		minBox.w = 0.0f;
		maxBox.w = 0.0f;
		info.m_worldBox.m_min = minBox;
		info.m_worldBox.m_max = maxBox;
	}
};

__global__ void ndCudaCountAabbInternal(ndCudaSceneInfo& info)
{
	const unsigned blockId = blockIdx.x;
	const unsigned bodyCount = info.m_bodyArray.m_size - 1;
	const unsigned threadId = threadIdx.x;
	const unsigned index = threadId + blockDim.x * blockId;

	unsigned* histogram = info.m_histogram.m_array;
	const ndCudaBodyProxy* bodyArray = info.m_bodyArray.m_array;
	if (index < bodyCount)
	{
		const ndCudaVector minBox(info.m_worldBox.m_min);
		const ndCudaVector bodyBoxMin(bodyArray[index].m_minAabb);
		const ndCudaVector bodyBoxMax(bodyArray[index].m_maxAabb);

		const int x0 = __float2int_rd((bodyBoxMin.x - minBox.x) * D_CUDA_SCENE_INV_GRID_SIZE);
		const int y0 = __float2int_rd((bodyBoxMin.y - minBox.y) * D_CUDA_SCENE_INV_GRID_SIZE);
		const int z0 = __float2int_rd((bodyBoxMin.z - minBox.z) * D_CUDA_SCENE_INV_GRID_SIZE);
		const int x1 = __float2int_rd((bodyBoxMax.x - minBox.x) * D_CUDA_SCENE_INV_GRID_SIZE) + 1;
		const int y1 = __float2int_rd((bodyBoxMax.y - minBox.y) * D_CUDA_SCENE_INV_GRID_SIZE) + 1;
		const int z1 = __float2int_rd((bodyBoxMax.z - minBox.z) * D_CUDA_SCENE_INV_GRID_SIZE) + 1;
		const int count = (z1 - z0) * (y1 - y0) * (x1 - x0);
		histogram[index] = count;
	}
};

__global__ void ndCudaGenerateGridsInternal(const ndCudaSceneInfo& info)
{
	const unsigned threadId = threadIdx.x;
	const unsigned index = threadId + blockDim.x * blockIdx.x;
	const unsigned bodyCount = info.m_bodyArray.m_size - 1;
	if (index < bodyCount)
	{
		const unsigned* histogram = info.m_histogram.m_array;
		const ndCudaBodyProxy* bodyArray = info.m_bodyArray.m_array;
		ndCudaBodyAabbCell* hashArray = info.m_bodyAabbCell.m_array;

		const ndCudaVector minBox(info.m_worldBox.m_min);
		const ndCudaVector bodyBoxMin(bodyArray[index].m_minAabb);
		const ndCudaVector bodyBoxMax(bodyArray[index].m_maxAabb);

		const int x0 = __float2int_rd((bodyBoxMin.x - minBox.x) * D_CUDA_SCENE_INV_GRID_SIZE);
		const int y0 = __float2int_rd((bodyBoxMin.y - minBox.y) * D_CUDA_SCENE_INV_GRID_SIZE);
		const int z0 = __float2int_rd((bodyBoxMin.z - minBox.z) * D_CUDA_SCENE_INV_GRID_SIZE);
		const int x1 = __float2int_rd((bodyBoxMax.x - minBox.x) * D_CUDA_SCENE_INV_GRID_SIZE) + 1;
		const int y1 = __float2int_rd((bodyBoxMax.y - minBox.y) * D_CUDA_SCENE_INV_GRID_SIZE) + 1;
		const int z1 = __float2int_rd((bodyBoxMax.z - minBox.z) * D_CUDA_SCENE_INV_GRID_SIZE) + 1;

		ndCudaBodyAabbCell hash;

		hash.m_key = 0;
		hash.m_id = index;
		unsigned start = index ? histogram[index - 1] : 0;
		for (int z = z0; z < z1; z++)
		{
			hash.m_z = z;
			for (int y = y0; y < y1; y++)
			{
				hash.m_y = y;
				for (int x = x0; x < x1; x++)
				{
					hash.m_x = x;
					hashArray[start].m_value = hash.m_value;
					start++;
				}
			}
		}
	}
};

__global__ void ndCudaInitBodyArray(ndCudaSceneInfo& info)
{
	const unsigned bodyCount = info.m_bodyArray.m_size - 1;
	const unsigned blocksCount = (bodyCount + D_THREADS_PER_BLOCK - 1) / D_THREADS_PER_BLOCK;
	ndCudaInitBodyArrayInternal << <blocksCount, D_THREADS_PER_BLOCK, 0 >> > (info);
	if (!info.m_frameIsValid)
	{
		printf("skipping frame %d  function %s  line %d\n", info.m_frameCount, __FUNCTION__, __LINE__);
		return;
	}

	ndCudaMergeAabbInternal << <1, D_THREADS_PER_BLOCK, 0 >> > (info);
	if (bodyCount > info.m_histogram.m_capacity)
	{
		info.m_frameIsValid = 0;
		printf("skipping frame %d  function %s  line %d\n", info.m_frameCount, __FUNCTION__, __LINE__);
		info.m_histogram.m_size = bodyCount + 1;
		return;
	}

	info.m_histogram.m_size = bodyCount;
	ndCudaCountAabbInternal << <blocksCount, D_THREADS_PER_BLOCK, 0 >> > (info);
}

__global__ void ndCudaGenerateGrids(ndCudaSceneInfo& info)
{
	if (!info.m_frameIsValid)
	{
		printf("skipping frame %d  function %s  line %d\n", info.m_frameCount, __FUNCTION__, __LINE__);
		return;
	}

	const unsigned bodyCount = info.m_bodyArray.m_size - 1;
	const unsigned cellCount = info.m_histogram.m_array[bodyCount - 1];
	if ((cellCount + D_THREADS_PER_BLOCK) > info.m_bodyAabbCell.m_capacity)
	{
		info.m_frameIsValid = 0;
		info.m_bodyAabbCell.m_size = cellCount + D_THREADS_PER_BLOCK;
		info.m_bodyAabbCellScrath.m_size = cellCount + D_THREADS_PER_BLOCK;
		printf("skipping frame %d  function %s  line %d\n", info.m_frameCount, __FUNCTION__, __LINE__);
		return;
	}

	ndCudaBodyAabbCell cell;
	ndCudaBodyAabbCell* cellArray = info.m_bodyAabbCell.m_array;
	ndCudaBodyAabbCell* cellArrayScrath = info.m_bodyAabbCellScrath.m_array;

	cell.m_value = 0;
	cell.m_x = unsigned(-1);
	cell.m_y = unsigned(-1);
	cell.m_z = unsigned(-1);
	cell.m_id = unsigned(-1);
	cellArray[cellCount].m_value = cell.m_value;
	cellArrayScrath[cellCount].m_value = cell.m_value;

	const unsigned blocksCount = (cellCount + D_COUNTING_SORT_MAX_BLOCK_SIZE - 1) / D_COUNTING_SORT_MAX_BLOCK_SIZE;

	info.m_bodyAabbCell.m_size = cellCount + 1;
	info.m_bodyAabbCellScrath.m_size = cellCount + 1;
	ndCudaGenerateGridsInternal << <blocksCount, D_THREADS_PER_BLOCK, 0 >> > (info);

	//printf("bodyCount(%d)  cellCount(%d) blocksCount(%d)\n", bodyCount, cellCount, blocksCount);
	//printf("function: %s: ", __FUNCTION__);
	//for (int i = 0; i < info.m_histogram.m_size; i++)
	//{
	//	printf("%d ", info.m_histogram.m_array[i]);
	//}
	//printf("\n");
	//printf("function: %s\n: ", __FUNCTION__);
	//printf("bodyCount(%d)  cellCount(%d) blocksCount(%d)\n", bodyCount, cellCount, blocksCount);
	//for (int i = 0; i < info.m_bodyAabbCell.m_size; i++)
	//{
	//	printf("x(%d) y(%d) z(%d) id(%d)\n", cellArray[i].m_x, cellArray[i].m_y, cellArray[i].m_z, cellArray[i].m_id);
	//}
	//printf("\n");

	//unsigned* histogram = info.m_histogram.m_array;
	//const unsigned size = info.m_bodyAabbCell.m_size - 1;
	const unsigned newCapacity = ndCudaCountingSortCalculateScanPrefixSize(info.m_bodyAabbCell.m_size - 1, D_THREADS_PER_BLOCK);
	if (newCapacity > info.m_histogram.m_capacity)
	{
		info.m_frameIsValid = 0;
		printf("skipping frame %d  function %s  line %d\n", info.m_frameCount, __FUNCTION__, __LINE__);
		info.m_histogram.m_size = newCapacity;
	}

	printf("frame %d  size %d\n", info.m_frameCount, info.m_bodyAabbCell.m_size);
}

/*
template <typename SortKeyPredicate_x, typename SortKeyPredicate_y, typename SortKeyPredicate_z, typename SortKeyPredicate_w>
__global__ void ndCudaSortGridArray(ndCudaSceneInfo& info, SortKeyPredicate_x sortKey_x, SortKeyPredicate_y sortKey_y, SortKeyPredicate_z sortKey_z, SortKeyPredicate_w sortKey_w)
{
	if (!info.m_frameIsValid)
	{
		printf("skipping frame %d  function %s  line %d\n", info.m_frameCount, __FUNCTION__, __LINE__);
		return;
	}

	unsigned* histogram = info.m_histogram.m_array;
	const unsigned size = info.m_bodyAabbCell.m_size - 1;

	const unsigned newCapacity = ndCudaCountingSortCalculateScanPrefixSize(size, D_THREADS_PER_BLOCK);
	if (newCapacity > info.m_histogram.m_capacity)
	{
		info.m_frameIsValid = 0;
		printf("skipping frame %d  function %s  line %d\n", info.m_frameCount, __FUNCTION__, __LINE__);
		info.m_histogram.m_size = newCapacity;
		return;
	}

	//printf("function: %s\n: ", __FUNCTION__);
	//for (int i = 0; i < info.m_bodyAabbCell.m_size; i++)
	//{
	//	const ndCudaBodyAabbCell* cellArray = info.m_bodyAabbCell.m_array;
	//	printf("x(%d) y(%d) z(%d) id(%d)\n", cellArray[i].m_x, cellArray[i].m_y, cellArray[i].m_z, cellArray[i].m_id);
	//}
	//printf("\n");
	//return;

	//ndCudaBodyAabbCell* src = info.m_bodyAabbCell.m_array;
	//ndCudaBodyAabbCell* dst = info.m_bodyAabbCellScrath.m_array;
	long long* src = &info.m_bodyAabbCell.m_array->m_value;
	long long* dst = &info.m_bodyAabbCellScrath.m_array->m_value;

	ndCudaCountingSort << <1, 1, 0 >> > (info, src, dst, histogram, size, sortKey_x, D_THREADS_PER_BLOCK, "aaaa");
	//ndCudaCountingSort << <1, 1, 0 >> > (info, dst, src, histogram, size, sortKey_y, D_THREADS_PER_BLOCK);
	//ndCudaCountingSort << <1, 1, 0 >> > (info, src, dst, histogram, size, sortKey_z, D_THREADS_PER_BLOCK);
	//ndCudaCountingSort << <1, 1, 0 >> > (info, dst, src, histogram, size, sortKey_w, D_THREADS_PER_BLOCK);

	//printf("function: %s\n", __FUNCTION__);
	//for (int i = 0; i < info.m_bodyAabbCell.m_size; i++)
	//{
	//	ndCudaBodyAabbCell cell;
	//	//cell.m_value = dst[i];
	//	cell.m_value = src[i];
	//	printf("x(%d) y(%d) z(%d) id(%d)\n", cell.m_x, cell.m_y, cell.m_z, cell.m_id);
	//}
	//printf("\n");
}
*/

ndCudaContextImplement::ndCudaContextImplement()
	:m_sceneInfoGpu(nullptr)
	,m_sceneInfoCpu(nullptr)
	,m_histogram()
	,m_bodyBufferGpu()
	,m_bodyAabbCell()
	,m_bodyAabbCellScrath()
	,m_boundingBoxGpu()
	,m_transformBufferCpu0()
	,m_transformBufferCpu1()
	,m_transformBufferGpu0()
	,m_transformBufferGpu1()
	,m_solverMemCpyStream(0)
	,m_solverComputeStream(0)
	,m_timeInMilisecunds(0.0f)
	,m_frameCounter(0)
{
	cudaError_t cudaStatus;
	cudaStatus = cudaStreamCreate(&m_solverMemCpyStream);
	dAssert(cudaStatus == cudaSuccess);

	cudaStatus = cudaStreamCreate(&m_solverComputeStream);
	dAssert(cudaStatus == cudaSuccess);
	
	cudaStatus = cudaMalloc((void**)&m_sceneInfoGpu, sizeof(ndCudaSceneInfo));
	dAssert(cudaStatus == cudaSuccess);
	
	cudaStatus = cudaMallocHost((void**)&m_sceneInfoCpu, sizeof(ndCudaSceneInfo));
	dAssert(cudaStatus == cudaSuccess);
	
	if (cudaStatus != cudaSuccess)
	{
		dAssert(0);
	}

	cudaEventCreate(&m_startTime);
	cudaEventCreate(&m_stopTime);
	
	*m_sceneInfoCpu = ndCudaSceneInfo();
}

ndCudaContextImplement::~ndCudaContextImplement()
{
	cudaError_t cudaStatus;

	cudaEventDestroy(m_stopTime);
	cudaEventDestroy(m_startTime);
	
	cudaStatus = cudaFreeHost(m_sceneInfoCpu);
	dAssert(cudaStatus == cudaSuccess);
	
	cudaStatus = cudaFree(m_sceneInfoGpu);
	dAssert(cudaStatus == cudaSuccess);
	
	cudaStatus = cudaStreamDestroy(m_solverComputeStream);
	dAssert(cudaStatus == cudaSuccess);

	cudaStatus = cudaStreamDestroy(m_solverMemCpyStream);
	dAssert(cudaStatus == cudaSuccess);
	
	if (cudaStatus != cudaSuccess)
	{
		dAssert(0);
	}
}

float ndCudaContextImplement::GetTimeInMilisecunds() const
{
	return m_timeInMilisecunds;
}

void ndCudaContextImplement::SwapBuffers()
{
	m_frameCounter = m_frameCounter + 1;

	ndCudaSceneInfo* const gpuInfo = m_sceneInfoGpu;
	ndCudaEndFrame << < 1, 1, 0, m_solverComputeStream >> > (*gpuInfo, m_frameCounter);
	cudaEventRecord(m_stopTime);

	m_transformBufferCpu0.Swap(m_transformBufferCpu1);
}

void ndCudaContextImplement::Begin()
{
	cudaEventSynchronize(m_stopTime);
	cudaEventElapsedTime(&m_timeInMilisecunds, m_startTime, m_stopTime);
	cudaDeviceSynchronize();

	// get the scene info from the update	
	ndCudaSceneInfo* const gpuInfo = m_sceneInfoGpu;
	ndCudaSceneInfo* const cpuInfo = m_sceneInfoCpu;

	cudaError_t cudaStatus = cudaMemcpyAsync(cpuInfo, gpuInfo, sizeof(ndCudaSceneInfo), cudaMemcpyDeviceToHost, m_solverMemCpyStream);
	dAssert(cudaStatus == cudaSuccess);
	if (cudaStatus != cudaSuccess)
	{
		dAssert(0);
	}

	const int frameCounter = m_frameCounter;
	if (frameCounter)
	{
		ndCudaHostBuffer<ndCudaSpatialVector>& cpuBuffer = m_transformBufferCpu0;
		ndCudaDeviceBuffer<ndCudaSpatialVector>& gpuBuffer = (frameCounter & 1) ? m_transformBufferGpu1 : m_transformBufferGpu0;
		gpuBuffer.WriteData(&cpuBuffer[0], cpuBuffer.GetCount() - 1, m_solverMemCpyStream);
	}

	cudaEventRecord(m_startTime);
}

ndCudaSpatialVector* ndCudaContextImplement::GetTransformBuffer0()
{
	return &m_transformBufferCpu0[0];
}

ndCudaSpatialVector* ndCudaContextImplement::GetTransformBuffer1()
{
	return &m_transformBufferCpu1[0];
}

void ndCudaContextImplement::ResizeBuffers(int cpuBodyCount)
{
	const int gpuBodyCount = D_THREADS_PER_BLOCK * ((cpuBodyCount + D_THREADS_PER_BLOCK - 1) / D_THREADS_PER_BLOCK);
	
	ndCudaDeviceBuffer<unsigned>& histogramGpu = m_histogram;
	ndCudaDeviceBuffer<ndCudaBodyProxy>& bodyBufferGpu = m_bodyBufferGpu;
	ndCudaDeviceBuffer<ndCudaBoundingBox>& boundingBoxGpu = m_boundingBoxGpu;
	ndCudaDeviceBuffer<ndCudaBodyAabbCell>& bodyAabbCellGpu0 = m_bodyAabbCell;
	ndCudaDeviceBuffer<ndCudaBodyAabbCell>& bodyAabbCellGpu1 = m_bodyAabbCellScrath;
	ndCudaHostBuffer<ndCudaSpatialVector>& transformBufferCpu0 = m_transformBufferCpu0;
	ndCudaHostBuffer<ndCudaSpatialVector>& transformBufferCpu1 = m_transformBufferCpu1;
	ndCudaDeviceBuffer<ndCudaSpatialVector>& transformBufferGpu0 = m_transformBufferGpu0;
	ndCudaDeviceBuffer<ndCudaSpatialVector>& transformBufferGpu1 = m_transformBufferGpu1;
	
	histogramGpu.SetCount(cpuBodyCount);
	bodyBufferGpu.SetCount(cpuBodyCount);
	bodyAabbCellGpu0.SetCount(cpuBodyCount);
	bodyAabbCellGpu1.SetCount(cpuBodyCount);
	transformBufferGpu0.SetCount(cpuBodyCount);
	transformBufferGpu1.SetCount(cpuBodyCount);
	transformBufferCpu0.SetCount(cpuBodyCount);
	transformBufferCpu1.SetCount(cpuBodyCount);
	boundingBoxGpu.SetCount(gpuBodyCount / D_THREADS_PER_BLOCK);
}

void ndCudaContextImplement::LoadBodyData(const ndCudaBodyProxy* const src, int cpuBodyCount)
{
	cudaDeviceSynchronize();
		
	ndCudaSceneInfo info;
	info.m_histogram = ndCudaBuffer<unsigned>(m_histogram);
	info.m_bodyArray = ndCudaBuffer<ndCudaBodyProxy>(m_bodyBufferGpu);
	info.m_bodyAabbArray = ndCudaBuffer<ndCudaBoundingBox>(m_boundingBoxGpu);
	info.m_bodyAabbCell = ndCudaBuffer<ndCudaBodyAabbCell>(m_bodyAabbCell);
	info.m_bodyAabbCellScrath = ndCudaBuffer<ndCudaBodyAabbCell>(m_bodyAabbCellScrath);
	info.m_transformBuffer0 = ndCudaBuffer<ndCudaSpatialVector>(m_transformBufferGpu0);
	info.m_transformBuffer1 = ndCudaBuffer<ndCudaSpatialVector>(m_transformBufferGpu0);
	
	*m_sceneInfoCpu = info;
	cudaError_t cudaStatus = cudaMemcpy(m_sceneInfoGpu, &info, sizeof(ndCudaSceneInfo), cudaMemcpyHostToDevice);
	dAssert(cudaStatus == cudaSuccess);

	const int blocksCount = (cpuBodyCount + D_THREADS_PER_BLOCK - 1) / D_THREADS_PER_BLOCK;
	m_bodyBufferGpu.ReadData(src, cpuBodyCount);
	ndCudaInitTransforms << <blocksCount, D_THREADS_PER_BLOCK, 0, 0 >> > (*m_sceneInfoCpu);
	
	cudaDeviceSynchronize();
	if (cudaStatus != cudaSuccess)
	{
		dAssert(0);
	}
}

void ndCudaContextImplement::ValidateContextBuffers()
{
	ndCudaSceneInfo* const sceneInfo = m_sceneInfoCpu;
	if (!sceneInfo->m_frameIsValid)
	{
		cudaDeviceSynchronize();
		sceneInfo->m_frameIsValid = 1;

		if (sceneInfo->m_histogram.m_size > sceneInfo->m_histogram.m_capacity)
		{
			m_histogram.SetCount(sceneInfo->m_histogram.m_size);
			sceneInfo->m_histogram = ndCudaBuffer<unsigned>(m_histogram);
		}

		if (sceneInfo->m_bodyAabbCell.m_size > sceneInfo->m_bodyAabbCell.m_capacity)
		{
			m_bodyAabbCell.SetCount(sceneInfo->m_bodyAabbCell.m_size);
			m_bodyAabbCellScrath.SetCount(sceneInfo->m_bodyAabbCell.m_size);
			sceneInfo->m_bodyAabbCell = ndCudaBuffer<ndCudaBodyAabbCell>(m_bodyAabbCell);
			sceneInfo->m_bodyAabbCellScrath = ndCudaBuffer<ndCudaBodyAabbCell>(m_bodyAabbCellScrath);
		}

		cudaError_t cudaStatus = cudaMemcpy(m_sceneInfoGpu, sceneInfo, sizeof(ndCudaSceneInfo), cudaMemcpyHostToDevice);
		dAssert(cudaStatus == cudaSuccess);
		if (cudaStatus != cudaSuccess)
		{
			dAssert(0);
		}
		cudaDeviceSynchronize();
	}
}

void ndCudaContextImplement::InitBodyArray()
{
	//auto CompactMovingBodies = ndMakeObject::ndFunction([this, &scans](int threadIndex, int threadCount)
	//{
	//	const ndArray<ndBodyKinematic*>& activeBodyArray = GetActiveBodyArray();
	//	ndBodyKinematic** const sceneBodyArray = &m_sceneBodyArray[0];
	//
	//	const ndArray<ndBodyKinematic*>& view = m_bodyList.m_view;
	//	int* const scan = &scans[threadIndex][0];
	//
	//	const ndStartEnd startEnd(view.GetCount(), threadIndex, threadCount);
	//	for (int i = startEnd.m_start; i < startEnd.m_end; ++i)
	//	{
	//		ndBodyKinematic* const body = activeBodyArray[i];
	//		const int key = body->m_sceneEquilibrium;
	//		const int index = scan[key];
	//		sceneBodyArray[index] = body;
	//		scan[key] ++;
	//	}
	//});

	//ParallelExecute(BuildBodyArray);
	//int sum = 0;
	//int threadCount = GetThreadCount();
	//for (int j = 0; j < 2; j++)
	//{
	//	for (int i = 0; i < threadCount; ++i)
	//	{
	//		const int count = scans[i][j];
	//		scans[i][j] = sum;
	//		sum += count;
	//	}
	//}
	//
	//int movingBodyCount = scans[0][1] - scans[0][0];
	//m_sceneBodyArray.SetCount(m_bodyList.GetCount());
	//if (movingBodyCount)
	//{
	//	ParallelExecute(CompactMovingBodies);
	//}
	//
	//m_sceneBodyArray.SetCount(movingBodyCount);
	//
	//ndBodyKinematic* const sentinelBody = m_sentinelBody;
	//sentinelBody->PrepareStep(GetActiveBodyArray().GetCount() - 1);
	//
	//sentinelBody->m_isStatic = 1;
	//sentinelBody->m_autoSleep = 1;
	//sentinelBody->m_equilibrium = 1;
	//sentinelBody->m_equilibrium0 = 1;
	//sentinelBody->m_isJointFence0 = 1;
	//sentinelBody->m_isJointFence1 = 1;
	//sentinelBody->m_isConstrained = 0;
	//sentinelBody->m_sceneEquilibrium = 1;
	//sentinelBody->m_weigh = ndFloat32(0.0f);

#if 0


	auto CalculateBodyPairsCount = [] __device__(cuSceneInfo & info)
	{
		__shared__  unsigned cacheBuffer[D_THREADS_PER_BLOCK / 2 + D_THREADS_PER_BLOCK];

		const unsigned blockId = blockIdx.x;
		const unsigned cellCount = info.m_bodyAabbCell.m_size - 1;
		const unsigned blocks = (cellCount + blockDim.x - 1) / blockDim.x;
		if (blockId < blocks)
		{
			const unsigned threadId = threadIdx.x;
			const unsigned threadId1 = D_THREADS_PER_BLOCK / 2 + threadId;
			int index = threadId + blockDim.x * blockIdx.x;

			cacheBuffer[threadId] = 0;
			cacheBuffer[threadId1] = 0;
			if (index < cellCount)
			{
				const cuBodyAabbCell* hashArray = info.m_bodyAabbCell.m_array;

				unsigned count = 0;
				const cuBodyAabbCell& cell = hashArray[index];

				for (int i = index + 1; cell.m_key == hashArray[i].m_key; i++)
				{
					count++;
				}
				cacheBuffer[threadId1] = count;
			}

			__syncthreads();
			for (int i = 1; i < D_THREADS_PER_BLOCK; i = i << 1)
			{
				int sum = cacheBuffer[threadId1] + cacheBuffer[threadId1 - i];
				__syncthreads();
				cacheBuffer[threadId1] = sum;
				__syncthreads();
			}

			if (index < cellCount)
			{
				unsigned* scan = info.m_histogram.m_array;
				const unsigned prefixScanSuperBlockAlign = D_PREFIX_SCAN_PASSES * D_THREADS_PER_BLOCK;
				const unsigned offset = (cellCount + prefixScanSuperBlockAlign) & (-prefixScanSuperBlockAlign);
				scan[offset + index] = cacheBuffer[threadId1];
			}
		}
	};
#endif


	auto GetItemsCount = [] __device__(const ndCudaSceneInfo & info)
	{
		return info.m_bodyAabbCell.m_size - 1;
	};

	auto GetSrcBuffer = [] __device__(const ndCudaSceneInfo &info)
	{
		return &info.m_bodyAabbCell.m_array[0].m_value;
	};

	auto GetDstBuffer = [] __device__(const ndCudaSceneInfo & info)
	{
		return &info.m_bodyAabbCellScrath.m_array[0].m_value;
	};


	auto GetSortKey_x = [] __device__(long long value)
	{
		ndCudaBodyAabbCell item;
		item.m_value = value;
		const unsigned key = item.m_key;
		return key & 0xff;
	};

	//auto SortKey_y = [] __device__(long long value)
	//{
	//	ndCudaBodyAabbCell item;
	//	item.m_value = value;
	//	const unsigned key = item.m_key;
	//	return (key>>8) & 0xff;
	//};
	//
	//auto SortKey_z = [] __device__(long long value)
	//{
	//	ndCudaBodyAabbCell item;
	//	item.m_value = value;
	//	const unsigned key = item.m_key;
	//	return (key >> 16) & 0xff;
	//};
	//
	//auto SortKey_w = [] __device__(long long value)
	//{
	//	ndCudaBodyAabbCell item;
	//	item.m_value = value;
	//	const unsigned key = item.m_key;
	//	return (key >> 24) & 0xff;
	//};


	ndCudaSceneInfo* const infoGpu = m_sceneInfoGpu;

#if 0
	//int threads = m_context->m_bodyBufferGpu.GetCount() - 1;
	//int bodyBlocksCount = (threads + D_THREADS_PER_BLOCK - 1) / D_THREADS_PER_BLOCK;
	//CudaInitBodyArray << <bodyBlocksCount, D_THREADS_PER_BLOCK, 0, stream >> > (InitBodyArray, *infoGpu);
	//CudaMergeAabb << <1, D_THREADS_PER_BLOCK, 0, stream >> > (MergeAabb, *infoGpu);
	//CudaCountAabb << <bodyBlocksCount, D_THREADS_PER_BLOCK, 0, stream >> > (CountAabb, *infoGpu);
	//CudaPrefixScan(m_context, D_THREADS_PER_BLOCK);
	//CudaValidateGridBuffer << <1, 1, 0, stream >> > (ValidateGridArray, *infoGpu);
	//CudaGenerateGridHash << <bodyBlocksCount, D_THREADS_PER_BLOCK, 0, stream >> > (GenerateHashGrids, *infoGpu);
	//CudaBodyAabbCellSortBuffer(m_context);
	//dAssert(SanityCheckSortCells());
	//int cellsBlocksCount = (m_context->m_bodyAabbCell.m_capacity + D_THREADS_PER_BLOCK - 1) / D_THREADS_PER_BLOCK;
	//dAssert(cellsBlocksCount > 0);
	//CudaCalculateBodyPairsCount << <cellsBlocksCount, D_THREADS_PER_BLOCK, 0, stream >> > (CalculateBodyPairsCount, *infoGpu);
	//dAssert(SanityCheckPrefix());
#endif

	//ndCudaScene << <1, 1, 0, m_solverComputeStream >> > (*infoGpu);
	//ndCudaSortGridArray << <1, 1, 0, m_solverComputeStream >> > (*infoGpu, SortKey_x, SortKey_y, SortKey_z, SortKey_w);

	ndCudaInitBodyArray << <1, 1, 0, m_solverComputeStream >> > (*infoGpu);
	ndCudaHillisSteelePrefixScan << <1, 1, 0, m_solverComputeStream >> > (*infoGpu);
	ndCudaGenerateGrids << <1, 1, 0, m_solverComputeStream >> > (*infoGpu);

	//__global__ void ndCudaCountingSort(ndCudaSceneInfo& info, GetInfoPredicate GetInfo, GetKeyPredicate GetKey)
	long long dommyType = 0;
	ndCudaCountingSort << <1, 1, 0, m_solverComputeStream >> > (*infoGpu, dommyType, GetSrcBuffer, GetDstBuffer, GetItemsCount, GetSortKey_x, 256);

}