/* Copyright (c) <2003-2019> <Julio Jerez, Newton Game Dynamics>
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

#include "dCoreStdafx.h"
#include "ndNewtonStdafx.h"
#include "ndWorld.h"
#include "ndBodyDynamic.h"
#include "ndSkeletonList.h"
#include "ndDynamicsUpdateOpencl.h"
#include "ndJointBilateralConstraint.h"

#ifdef _D_NEWTON_OPENCL
#include <CL/cl.h>

//using namespace ndOpencl;

class OpenclSystem
{
	public:
	OpenclSystem(cl_context context, cl_platform_id platform)
		:m_context(context)
		//,device(nullptr)
		//,commandQueue(nullptr)
		//,program(nullptr)
		//,kernel(nullptr)
		//,platformVersion(CL_TARGET_OPENCL_VERSION)
		//,deviceVersion(CL_TARGET_OPENCL_VERSION)
		//,compilerVersion(CL_TARGET_OPENCL_VERSION)
		//,srcA(nullptr)
		//,srcB(nullptr)
		//,dstMem(nullptr)
	{
		cl_int err;
		// get the device
		err = clGetContextInfo(m_context, CL_CONTEXT_DEVICES, sizeof(cl_device_id), &m_device, nullptr);
		dAssert(err == CL_SUCCESS);

		// get vendor driver support
		size_t stringLength = 0;
		err = clGetPlatformInfo(platform, CL_PLATFORM_NAME, 0, nullptr, &stringLength);
		dAssert(err == CL_SUCCESS);
		dAssert(stringLength < sizeof(m_platformName));
		err = clGetPlatformInfo(platform, CL_PLATFORM_NAME, stringLength, m_platformName, nullptr);
		dAssert(err == CL_SUCCESS);

		// get opencl version
		err = clGetDeviceInfo(m_device, CL_DEVICE_VERSION, 0, NULL, &stringLength);
		dAssert(err == CL_SUCCESS);
		dAssert(stringLength < sizeof(m_platformName));
		err = clGetDeviceInfo(m_device, CL_DEVICE_VERSION, stringLength, m_platformName, nullptr);
		dAssert(err == CL_SUCCESS);

		// create command queue
		cl_command_queue_properties properties = CL_QUEUE_PROFILING_ENABLE;
		m_commandQueue = clCreateCommandQueue(m_context, m_device, properties, &err);
		dAssert(err == CL_SUCCESS);

		char programFile[256];
		sprintf(programFile, "%s/CL/solver/solver.cl", CL_KERNEL_PATH);
		m_solverProgram = CompileProgram(programFile);
	}

	~OpenclSystem()
	{
		cl_int err;

		err = clReleaseProgram(m_solverProgram);
		dAssert(err == CL_SUCCESS);

		err = clReleaseCommandQueue(m_commandQueue);
		dAssert(err == CL_SUCCESS);

		err = clReleaseDevice(m_device);
		dAssert(err == CL_SUCCESS);

		err = clReleaseContext(m_context);
		dAssert(err == CL_SUCCESS);
	}

	cl_program CompileProgram(char* const filename)
	{
		FILE* const fp = fopen(filename, "rb");
		if (fp)
		{
			size_t sourceSize;
			fseek(fp, 0, SEEK_END);
			sourceSize = ftell(fp);
			fseek(fp, 0, SEEK_SET);

			char* const source = dAlloca(char, sourceSize * 256);
			fread(source, 1, sourceSize, fp);
			fclose(fp);

			int errorCode;
			cl_program program = clCreateProgramWithSource(m_context, 1, (const char**)&source, &sourceSize, &errorCode);
			dAssert(errorCode == CL_SUCCESS);

			errorCode = clBuildProgram(program, 1, &m_device, "", nullptr, nullptr);
			if (errorCode == CL_BUILD_PROGRAM_FAILURE)
			{
				size_t log_size = 0;
				clGetProgramBuildInfo(program, m_device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);

				char* const build_log = dAlloca(char, log_size * 256);
				clGetProgramBuildInfo(program, m_device, CL_PROGRAM_BUILD_LOG, log_size, build_log, nullptr);
				dTrace((build_log));
			}
			dAssert(errorCode == CL_SUCCESS);

			return program;
		}
		return nullptr;
	}

	static OpenclSystem* Singleton()
	{
		cl_uint numPlatforms = 0;
		cl_int err = clGetPlatformIDs(0, nullptr, &numPlatforms);
		if ((err != CL_SUCCESS) || (numPlatforms == 0))
		{
			return nullptr;
		}

		dAssert(numPlatforms < 16);
		cl_platform_id platforms[16];
		err = clGetPlatformIDs(numPlatforms, &platforms[0], nullptr);
		if (err != CL_SUCCESS)
		{
			return nullptr;
		}

		cl_platform_id bestPlatform = 0;
		for (cl_uint i = 0; i < numPlatforms; i++)
		{
			cl_uint numDevices = 0;
			err = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_GPU, 0, nullptr, &numDevices);
			if (!((err != CL_SUCCESS) || (numDevices == 0)))
			{
				bestPlatform = platforms[i];
			}
		}

		if (bestPlatform == nullptr)
		{
			return nullptr;
		}

		cl_context_properties contextProperties[] = { CL_CONTEXT_PLATFORM, (cl_context_properties)bestPlatform, 0 };

		cl_context context = clCreateContextFromType(contextProperties, CL_DEVICE_TYPE_GPU, nullptr, nullptr, &err);
		if ((CL_SUCCESS != err) || (context == nullptr))
		{
			return nullptr;
		}

		return new OpenclSystem(context, bestPlatform);
	}

	// Regular OpenCL objects:
	cl_context m_context;					// hold the context handler
	cl_device_id m_device;					// hold the selected device handler
	cl_program	m_solverProgram;			// hold the program handler
	cl_command_queue m_commandQueue;		// hold the commands-queue handler

	//cl_kernel        kernel;				// hold the kernel handler
	//cl_mem           srcA;				// hold first source buffer
	//cl_mem           srcB;				// hold second source buffer
	//cl_mem           dstMem;				// hold destination buffer
	char m_platformName[128];
};


void ndDynamicsUpdateOpencl::ndOpenclBodyProxyArray::CopyData(dArray<ndBodyKinematic*>& sourceData)
{
	SetCount(sourceData.GetCount());
	for (dInt32 i = 0; i < GetCount(); i++)
	{
		ndOpenclBodyProxy& data = (*this)[i];
		ndBodyKinematic* const body = sourceData[i];
		data.m_matrix = body->m_matrix;
		data.m_invMass = body->m_invMass;
		data.m_body = body;
	}
}

ndDynamicsUpdateOpencl::ndDynamicsUpdateOpencl(ndWorld* const world)
	:ndDynamicsUpdate(world)
	,m_openCl(nullptr)
{
	m_openCl = OpenclSystem::Singleton();
}

ndDynamicsUpdateOpencl::~ndDynamicsUpdateOpencl()
{
	if (m_openCl)
	{
		delete m_openCl;
	}
}

const char* ndDynamicsUpdateOpencl::GetStringId() const
{
	return m_openCl ? m_openCl->m_platformName : "no opencl support";
}

void ndDynamicsUpdateOpencl::Update()
{
	if (m_openCl)
	{
		//ndDynamicsUpdate::Update();
		GpuUpdate();
	}
}

void ndDynamicsUpdateOpencl::GpuUpdate()
{
	m_timestep = m_world->GetScene()->GetTimestep();

	BuildIsland();
	if (m_islands.GetCount())
	{
		m_bodyArray.CopyData(m_bodyIslandOrder);
		IntegrateUnconstrainedBodies();

		//InitWeights();
		//InitBodyArray();
		//InitJacobianMatrix();
		//CalculateForces();
		IntegrateBodies();
		//DetermineSleepStates();
	}
}

void ndDynamicsUpdateOpencl::BuildIsland()
{
	ndScene* const scene = m_world->GetScene();
	const dArray<ndBodyKinematic*>& bodyArray = scene->GetActiveBodyArray();
	dAssert(bodyArray.GetCount() >= 1);
	if (bodyArray.GetCount() - 1)
	{
		D_TRACKTIME();
		SortJoints();
		SortIslands();
	}
}

void ndDynamicsUpdateOpencl::SortJoints()
{
	D_TRACKTIME();
	ndScene* const scene = m_world->GetScene();

	for (ndSkeletonList::dListNode* node = m_world->GetSkeletonList().GetFirst(); node; node = node->GetNext())
	{
		ndSkeletonContainer* const skeleton = &node->GetInfo();
		skeleton->CheckSleepState();
	}

	const ndJointList& jointList = m_world->GetJointList();
	ndConstraintArray& jointArray = scene->GetActiveContactArray();

	dInt32 index = jointArray.GetCount();
	jointArray.SetCount(index + jointList.GetCount());
	for (ndJointList::dListNode* node = jointList.GetFirst(); node; node = node->GetNext())
	{
		ndJointBilateralConstraint* const joint = node->GetInfo();
		if (joint->IsActive())
		{
			jointArray[index] = joint;
			index++;
		}
	}
	jointArray.SetCount(index);

	for (dInt32 i = jointArray.GetCount() - 1; i >= 0; i--)
	{
		const ndConstraint* const joint = jointArray[i];
		ndBodyKinematic* const body0 = joint->GetBody0();
		ndBodyKinematic* const body1 = joint->GetBody1();
		dAssert(body0->m_solverSleep0 <= 1);
		dAssert(body1->m_solverSleep0 <= 1);

		const dInt32 resting = body0->m_equilibrium & body1->m_equilibrium;
		if (!resting)
		{
			body0->m_solverSleep0 = 0;
			if (body1->GetInvMass() > dFloat32(0.0f))
			{
				body1->m_solverSleep0 = 0;
			}
		}
	}

	for (dInt32 i = jointArray.GetCount() - 1; i >= 0; i--)
	{
		const ndConstraint* const joint = jointArray[i];
		ndBodyKinematic* const body0 = joint->GetBody0();
		ndBodyKinematic* const body1 = joint->GetBody1();
		dAssert(body0->m_solverSleep1 <= 1);
		dAssert(body1->m_solverSleep1 <= 1);

		const dInt32 test = body0->m_solverSleep0 & body1->m_solverSleep0;
		if (!test)
		{
			body0->m_solverSleep1 = 0;
			if (body1->GetInvMass() > dFloat32(0.0f))
			{
				body1->m_solverSleep1 = 0;
			}
		}
	}

	dInt32 currentActive = jointArray.GetCount();
	for (dInt32 i = currentActive - 1; i >= 0; i--)
	{
		ndConstraint* const joint = jointArray[i];
		ndBodyKinematic* const body0 = joint->GetBody0();
		ndBodyKinematic* const body1 = joint->GetBody1();
		const dInt32 test = body0->m_solverSleep1 & body1->m_solverSleep1;
		if (!test)
		{
			const dInt32 resting = (body0->m_equilibrium & body1->m_equilibrium) ? 1 : 0;
			const dInt32 rows = joint->GetRowsCount();
			joint->m_rowCount = rows;

			body0->m_bodyIsConstrained = 1;
			body0->m_resting = body0->m_resting & resting;

			if (body1->GetInvMass() > dFloat32(0.0f))
			{
				body1->m_bodyIsConstrained = 1;
				body1->m_resting = body1->m_resting & resting;

				ndBodyKinematic* root0 = FindRootAndSplit(body0);
				ndBodyKinematic* root1 = FindRootAndSplit(body1);
				if (root0 != root1)
				{
					if (root0->m_rank > root1->m_rank)
					{
						dSwap(root0, root1);
					}
					root0->m_islandParent = root1;
					if (root0->m_rank == root1->m_rank)
					{
						root1->m_rank += 1;
						dAssert(root1->m_rank <= 6);
					}
				}

				const dInt32 sleep = body0->m_islandSleep & body1->m_islandSleep;
				if (!sleep)
				{
					dAssert(root1->m_islandParent == root1);
					root1->m_islandSleep = 0;
				}
			}
			else
			{
				if (!body0->m_islandSleep)
				{
					ndBodyKinematic* const root = FindRootAndSplit(body0);
					root->m_islandSleep = 0;
				}
			}
		}
		else
		{
			currentActive--;
			jointArray[i] = jointArray[currentActive];
		}
	}

	dAssert(currentActive <= jointArray.GetCount());
	jointArray.SetCount(currentActive);
	if (!jointArray.GetCount())
	{
		m_activeJointCount = 0;
		return;
	}

	dInt32 jointCountSpans[128];
	m_leftHandSide.SetCount(jointArray.GetCount() + 32);
	ndConstraint** const sortBuffer = (ndConstraint**)&m_leftHandSide[0];
	memset(jointCountSpans, 0, sizeof(jointCountSpans));

	dInt32 activeJointCount = 0;
	for (dInt32 i = 0; i < jointArray.GetCount(); i++)
	{
		ndConstraint* const joint = jointArray[i];
		sortBuffer[i] = joint;

		const ndBodyKinematic* const body0 = joint->GetBody0();
		const ndBodyKinematic* const body1 = joint->GetBody1();
		const dInt32 resting = (body0->m_resting & body1->m_resting) ? 1 : 0;
		activeJointCount += (1 - resting);

		const ndSortKey key(resting, joint->m_rowCount);
		dAssert(key.m_value >= 0);
		dAssert(key.m_value < sizeof(jointCountSpans) / sizeof(jointCountSpans[0]));
		jointCountSpans[key.m_value] ++;
	}

	dInt32 acc = 0;
	for (dInt32 i = 0; i < sizeof(jointCountSpans) / sizeof(jointCountSpans[0]); i++)
	{
		const dInt32 val = jointCountSpans[i];
		jointCountSpans[i] = acc;
		acc += val;
	}

	m_activeJointCount = activeJointCount;
	for (dInt32 i = 0; i < jointArray.GetCount(); i++)
	{
		ndConstraint* const joint = sortBuffer[i];
		const ndBodyKinematic* const body0 = joint->GetBody0();
		const ndBodyKinematic* const body1 = joint->GetBody1();
		const dInt32 resting = (body0->m_resting & body1->m_resting) ? 1 : 0;

		const ndSortKey key(resting, joint->m_rowCount);
		dAssert(key.m_value >= 0);
		dAssert(key.m_value < sizeof(jointCountSpans) / sizeof(jointCountSpans[0]));

		const dInt32 entry = jointCountSpans[key.m_value];
		jointArray[entry] = joint;
		jointCountSpans[key.m_value] = entry + 1;
	}

	dInt32 rowCount = 0;
	for (dInt32 i = 0; i < jointArray.GetCount(); i++)
	{
		ndConstraint* const joint = jointArray[i];
		joint->m_rowStart = rowCount;
		rowCount += joint->m_rowCount;
	}

	m_leftHandSide.SetCount(rowCount);
	m_rightHandSide.SetCount(rowCount);
}

dInt32 ndDynamicsUpdateOpencl::CompareIslands(const ndIsland* const islandA, const ndIsland* const islandB, void* const)
{
	dUnsigned32 keyA = islandA->m_count * 2 + islandA->m_root->m_bodyIsConstrained;
	dUnsigned32 keyB = islandB->m_count * 2 + islandB->m_root->m_bodyIsConstrained;;
	if (keyA < keyB)
	{
		return 1;
	}
	else if (keyA > keyB)
	{
		return -1;
	}
	return 0;
}

void ndDynamicsUpdateOpencl::SortIslands()
{
	D_TRACKTIME();

	ndScene* const scene = m_world->GetScene();
	const dArray<ndBodyKinematic*>& bodyArray = scene->GetActiveBodyArray();
	m_internalForces.SetCount(bodyArray.GetCount());

	dInt32 count = 0;
	ndBodyIndexPair* const buffer0 = (ndBodyIndexPair*)&m_internalForces[0];
	for (dInt32 i = bodyArray.GetCount() - 2; i >= 0; i--)
	{
		ndBodyKinematic* const body = bodyArray[i];
		if (!(body->m_resting & body->m_islandSleep))
		{
			buffer0[count].m_body = body;
			if (body->GetInvMass() > dFloat32(0.0f))
			{
				ndBodyKinematic* root = body->m_islandParent;
				while (root != root->m_islandParent)
				{
					root = root->m_islandParent;
				}

				buffer0[count].m_root = root;
				if (root->m_rank != -1)
				{
					root->m_rank = -1;
				}
			}
			else
			{
				buffer0[count].m_root = body;
				body->m_rank = -1;
			}
			count++;
		}
	}

	m_islands.SetCount(0);
	m_bodyIslandOrder.SetCount(count);
	m_unConstrainedBodyCount = 0;
	if (count)
	{
		// sort using counting sort o(n)
		dInt32 scans[2];
		scans[0] = 0;
		scans[1] = 0;
		for (dInt32 i = 0; i < count; i++)
		{
			dInt32 j = 1 - buffer0[i].m_root->m_bodyIsConstrained;
			scans[j] ++;
		}
		scans[1] = scans[0];
		scans[0] = 0;
		ndBodyIndexPair* const buffer2 = buffer0 + count;
		for (dInt32 i = 0; i < count; i++)
		{
			const dInt32 key = 1 - buffer0[i].m_root->m_bodyIsConstrained;
			const dInt32 j = scans[key];
			buffer2[j] = buffer0[i];
			scans[key] = j + 1;
		}

		const ndBodyIndexPair* const buffer1 = buffer0 + count;
		for (dInt32 i = 0; i < count; i++)
		{
			dAssert((i == count - 1) || (buffer1[i].m_root->m_bodyIsConstrained >= buffer1[i + 1].m_root->m_bodyIsConstrained));

			m_bodyIslandOrder[i] = buffer1[i].m_body;
			if (buffer1[i].m_root->m_rank == -1)
			{
				buffer1[i].m_root->m_rank = 0;
				ndIsland island(buffer1[i].m_root);
				m_islands.PushBack(island);
			}
			buffer1[i].m_root->m_rank += 1;
		}

		dInt32 start = 0;
		dInt32 unConstrainedCount = 0;
		for (dInt32 i = 0; i < m_islands.GetCount(); i++)
		{
			ndIsland& island = m_islands[i];
			island.m_start = start;
			island.m_count = island.m_root->m_rank;
			start += island.m_count;
			unConstrainedCount += island.m_root->m_bodyIsConstrained ? 0 : 1;
		}

		m_unConstrainedBodyCount = unConstrainedCount;
		dSort(&m_islands[0], m_islands.GetCount(), CompareIslands);
	}
}

void ndDynamicsUpdateOpencl::IntegrateUnconstrainedBodies()
{
	class ndIntegrateUnconstrainedBodies : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			ndWorld* const world = m_owner->GetWorld();
			ndDynamicsUpdateOpencl* const me = (ndDynamicsUpdateOpencl*)world->m_solver;
			dArray<ndBodyKinematic*>& bodyArray = me->m_bodyIslandOrder;

			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = m_owner->GetThreadCount();
			const dInt32 bodyCount = me->m_unConstrainedBodyCount;
			const dInt32 base = bodyArray.GetCount() - bodyCount;
			const dInt32 step = bodyCount / threadCount;
			const dInt32 start = threadIndex * step;
			const dInt32 count = ((threadIndex + 1) < threadCount) ? step : bodyCount - start;
			const dFloat32 timestep = m_timestep;

			for (dInt32 i = 0; i < count; i++)
			{
				ndBodyKinematic* const body = bodyArray[base + start + i]->GetAsBodyKinematic();
				dAssert(body);
				body->UpdateInvInertiaMatrix();
				body->AddDampingAcceleration(m_timestep);
				body->IntegrateExternalForce(timestep);
			}
		}
	};

	if (m_unConstrainedBodyCount)
	{
		D_TRACKTIME();
		ndScene* const scene = m_world->GetScene();
		scene->SubmitJobs<ndIntegrateUnconstrainedBodies>();
	}
}

void ndDynamicsUpdateOpencl::IntegrateBodies()
{
	D_TRACKTIME();
	class ndIntegrateBodies : public ndScene::ndBaseJob
	{
		public:
		virtual void Execute()
		{
			D_TRACKTIME();
			ndWorld* const world = m_owner->GetWorld();
			ndDynamicsUpdateOpencl* const me = (ndDynamicsUpdateOpencl*)world->m_solver;
			dArray<ndBodyKinematic*>& bodyArray = me->m_bodyIslandOrder;

			const dInt32 threadIndex = GetThreadId();
			const dInt32 threadCount = m_owner->GetThreadCount();
			const dInt32 bodyCount = bodyArray.GetCount();
			const dInt32 step = bodyCount / threadCount;
			const dInt32 start = threadIndex * step;
			const dInt32 count = ((threadIndex + 1) < threadCount) ? step : bodyCount - start;

			const dFloat32 timestep = m_timestep;
			const dVector invTime(me->m_invTimestep);
			for (dInt32 i = 0; i < count; i++)
			{
				ndBodyDynamic* const dynBody = bodyArray[start + i]->GetAsBodyDynamic();

				// the initial velocity and angular velocity were stored in m_accel and dynBody->m_alpha for memory saving
				if (dynBody)
				{
					if (!dynBody->m_equilibrium)
					{
						dynBody->m_accel = invTime * (dynBody->m_veloc - dynBody->m_accel);
						dynBody->m_alpha = invTime * (dynBody->m_omega - dynBody->m_alpha);
						dynBody->IntegrateVelocity(timestep);
					}
				}
				else
				{
					ndBodyKinematic* const kinBody = bodyArray[start + i]->GetAsBodyKinematic();
					dAssert(kinBody);
					if (!kinBody->m_equilibrium)
					{
						kinBody->IntegrateVelocity(timestep);
					}
				}
			}
		}
	};

	ndScene* const scene = m_world->GetScene();
	scene->SubmitJobs<ndIntegrateBodies>();
}
#endif