/* Copyright (c) <2003-2016> <Julio Jerez, Newton Game Dynamics>
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

#include "dgNewtonPluginStdafx.h"
#include "dgSolver.h"

#include "dgBody.h"
#include "dgWorld.h"
#include "dgConstraint.h"
#include "dgDynamicBody.h"
#include "dgWorldDynamicUpdate.h"
#include "dgWorldDynamicsParallelSolver.h"

dgSolver::dgSolver(dgWorld* const world, dgMemoryAllocator* const allocator)
	:dgParallelBodySolver(allocator)
	,m_soaOne(1.0f)
	,m_soaZero(0.0f)
	,m_zero(0.0f)
	,m_negOne(-1.0f)
	,m_massMatrix(allocator)
{
	m_world = world;
}

dgSolver::~dgSolver()
{
}

#ifdef D_USE_SOA_SOLVER
void dgSolver::CalculateJointForces(const dgBodyCluster& cluster, dgBodyInfo* const bodyArray, dgJointInfo* const jointArray, dgFloat32 timestep)
{
	m_cluster = &cluster;
	m_bodyArray = bodyArray;
	m_jointArray = jointArray;
	m_timestep = timestep;
	m_invTimestep = (timestep > dgFloat32(0.0f)) ? dgFloat32(1.0f) / timestep : dgFloat32(0.0f);

	m_invStepRK = dgFloat32 (0.25f);
	m_timestepRK = m_timestep * m_invStepRK;
	m_invTimestepRK = m_invTimestep * dgFloat32 (4.0f);

	m_threadCounts = m_world->GetThreadCount();
	m_solverPasses = m_world->GetSolverIterations();

	dgInt32 mask = -dgInt32(DG_SOA_WORD_GROUP_SIZE - 1);
	m_jointCount = ((m_cluster->m_jointCount + DG_SOA_WORD_GROUP_SIZE - 1) & mask) / DG_SOA_WORD_GROUP_SIZE;

	m_bodyProxyArray = dgAlloca(dgBodyProxy, cluster.m_bodyCount);
	m_bodyJacobiansPairs = dgAlloca(dgBodyJacobianPair, cluster.m_jointCount * 2);
	m_soaRowStart = dgAlloca(dgInt32, cluster.m_jointCount / DG_SOA_WORD_GROUP_SIZE + 1);

	InitWeights();
	InitBodyArray();
	InitJacobianMatrix();
	CalculateForces();
}

void dgSolver::InitWeights()
{
	const dgJointInfo* const jointArray = m_jointArray;
	const dgInt32 jointCount = m_cluster->m_jointCount;
	dgBodyProxy* const weight = m_bodyProxyArray;
	memset(m_bodyProxyArray, 0, m_cluster->m_bodyCount * sizeof(dgBodyProxy));
	for (dgInt32 i = 0; i < jointCount; i++) {
		const dgJointInfo* const jointInfo = &jointArray[i];
		const dgInt32 m0 = jointInfo->m_m0;
		const dgInt32 m1 = jointInfo->m_m1;
		weight[m0].m_weight += dgFloat32(1.0f);
		weight[m1].m_weight += dgFloat32(1.0f);
	}
	m_bodyProxyArray[0].m_weight = dgFloat32(1.0f);

	dgFloat32 extraPasses = dgFloat32(0.0f);
	const dgInt32 bodyCount = m_cluster->m_bodyCount;
	for (dgInt32 i = 1; i < bodyCount; i++) {
		extraPasses = dgMax(weight[i].m_weight, extraPasses);
	}
	const dgInt32 conectivity = 7;
	m_solverPasses += 2 * dgInt32(extraPasses) / conectivity + 1;
}

void dgSolver::InitBodyArray()
{
	for (dgInt32 i = 0; i < m_threadCounts; i++) {
		m_world->QueueJob(InitBodyArrayKernel, this, NULL, "dgSolver::InitBodyArray");
	}
	m_world->SynchronizationBarrier();
	m_bodyProxyArray->m_invWeight = dgFloat32 (1.0f);
}

void dgSolver::InitBodyArrayKernel(void* const context, void* const, dgInt32 threadID)
{
	dgSolver* const me = (dgSolver*)context;
	me->InitBodyArray(threadID);
}

void dgSolver::InitBodyArray(dgInt32 threadID)
{
	const dgBodyInfo* const bodyArray = m_bodyArray;
	dgBodyProxy* const bodyProxyArray = m_bodyProxyArray;

	const dgInt32 step = m_threadCounts;;
	const dgInt32 bodyCount = m_cluster->m_bodyCount;
	for (dgInt32 i = threadID; i < bodyCount; i += step) {
		const dgBodyInfo* const bodyInfo = &bodyArray[i];
		dgBody* const body = (dgDynamicBody*)bodyInfo->m_body;
		body->AddDampingAcceleration(m_timestep);
		body->CalcInvInertiaMatrix();

		body->m_accel = body->m_veloc;
		body->m_alpha = body->m_omega;

		const dgFloat32 w = bodyProxyArray[i].m_weight ? bodyProxyArray[i].m_weight : dgFloat32(1.0f);
		bodyProxyArray[i].m_weight = w;
		bodyProxyArray[i].m_invWeight = dgFloat32(1.0f) / w;
	}
}

void dgSolver::InitInternalForcesKernel(void* const context, void* const, dgInt32 threadID)
{
	dgSolver* const me = (dgSolver*)context;
	me->InitInternalForces(threadID);
}

void dgSolver::InitInternalForces(dgInt32 threadID)
{
	const dgBodyProxy* const bodyProxyArray = m_bodyProxyArray;
	const dgBodyJacobianPair* const bodyJacobiansPairs = m_bodyJacobiansPairs;
	dgSoaFloat* const internalForces = (dgSoaFloat*)&m_world->GetSolverMemory().m_internalForcesBuffer[0];
	const dgRightHandSide* const rightHandSide = &m_world->GetSolverMemory().m_righHandSizeBuffer[0];
	const dgSoaFloat* const leftHandSide = (dgSoaFloat*) &m_world->GetSolverMemory().m_leftHandSizeBuffer[0].m_Jt.m_jacobianM0;

	const dgInt32 step = m_threadCounts;;
	const dgInt32 bodyCount = m_cluster->m_bodyCount;
	for (dgInt32 i = threadID; i < bodyCount; i += step) {
		dgSoaFloat forceAcc (m_soaZero);

		const dgBodyProxy* const startJoints = &bodyProxyArray[i];
		const dgInt32 jointsCount = dgInt32(startJoints->m_weight);
		const dgBodyJacobianPair* const jointsStart = &bodyJacobiansPairs[startJoints->m_jointStart];

		for (dgInt32 j = 0; j < jointsCount; j++) {
			const dgInt32 rowsCount = (jointsStart[j].m_rowCount >> 1) * 2;	
			const dgFloat32 preconditioner = jointsStart[j].m_preconditioner;
			const dgSoaFloat* const lhs = &leftHandSide[jointsStart[j].m_rowStart];
			const dgRightHandSide* const rhs = &rightHandSide[jointsStart[j].m_righHandStart];
			for (dgInt32 k = 0; k < rowsCount; k += 2) {
				forceAcc = forceAcc.MulAdd(lhs[(k + 0) * 4], dgSoaFloat(rhs[k + 0].m_force * preconditioner));
				forceAcc = forceAcc.MulAdd(lhs[(k + 1) * 4], dgSoaFloat(rhs[k + 1].m_force * preconditioner));
			}
			if (jointsStart[j].m_rowCount & 1) {
				const dgInt32 k = jointsStart[j].m_rowCount - 1;
				forceAcc = forceAcc.MulAdd(lhs[k * 4], dgSoaFloat(rhs[k].m_force * preconditioner));
			}
		}
		internalForces[i] = forceAcc;
	}
}

DG_INLINE void dgSolver::SortWorkGroup(dgInt32 base) const
{
	dgJointInfo* const jointArray = m_jointArray;
	for (dgInt32 i = 1; i < DG_WORK_GROUP_SIZE; i++) {
		dgInt32 index = base + i;
		const dgJointInfo tmp(jointArray[index]);
		for (; (index > base) && (jointArray[index - 1].m_pairCount < tmp.m_pairCount); index--) {
			jointArray[index] = jointArray[index - 1];
		}
		jointArray[index] = tmp;
	}
}


void dgSolver::InitJacobianMatrix()
{
	m_jacobianMatrixRowAtomicIndex = 0;
	for (dgInt32 i = 0; i < m_threadCounts; i++) {
		m_world->QueueJob(InitJacobianMatrixKernel, this, NULL, "dgSolver::InitJacobianMatrix");
	}
	m_world->SynchronizationBarrier();

	dgBodyProxy* const bodyProxyArray = m_bodyProxyArray;
	dgBodyJacobianPair* const bodyJacobiansPairs = m_bodyJacobiansPairs;
	const dgInt32 entryCount = m_cluster->m_jointCount * 2;
	dgSort(bodyJacobiansPairs, entryCount, CompareBodyJointsPairs);
	for (dgInt32 i = entryCount - 1; i >= 0; i--) {
		dgInt32 index = bodyJacobiansPairs[i].m_bodyIndex;
		bodyProxyArray[index].m_jointStart = i;
	}

	for (dgInt32 i = 0; i < m_threadCounts; i++) {
		m_world->QueueJob(InitInternalForcesKernel, this, NULL, "dgSolver::InitInternalForces");
	}
	m_world->SynchronizationBarrier();

	dgJacobian* const internalForces = &m_world->GetSolverMemory().m_internalForcesBuffer[0];
	internalForces[0].m_linear = m_zero;
	internalForces[0].m_angular = m_zero;

	dgJointInfo* const jointArray = m_jointArray;
	dgSort(jointArray, m_cluster->m_jointCount, CompareJointInfos);

	const dgInt32 jointCount = m_jointCount * DG_SOA_WORD_GROUP_SIZE;
	for (dgInt32 i = m_cluster->m_jointCount; i < jointCount; i++) {
		memset(&jointArray[i], 0, sizeof(dgJointInfo));
	}

	dgInt32 size = 0;
	for (dgInt32 i = 0; i < jointCount; i += DG_WORK_GROUP_SIZE) {
		const dgConstraint* const joint1 = jointArray[i + DG_WORK_GROUP_SIZE - 1].m_joint;
		if (joint1) {
			if (!(joint1->GetBody0()->m_resting & joint1->GetBody1()->m_resting)) {
				const dgConstraint* const joint0 = jointArray[i].m_joint;
				if (joint0->GetBody0()->m_resting & joint0->GetBody1()->m_resting) {
					SortWorkGroup(i);
				}
			}
		} else {
			SortWorkGroup(i);
		}
		size += jointArray[i].m_pairCount;
	}
	m_massMatrix.ResizeIfNecessary(size);

	m_soaRowsCount = 0;
	for (dgInt32 i = 0; i < m_threadCounts; i++) {
		m_world->QueueJob(TransposeMassMatrixKernel, this, NULL, "dgSolver::TransposeMassMatrix");
	}
	m_world->SynchronizationBarrier();
}

dgInt32 dgSolver::CompareBodyJointsPairs(const dgBodyJacobianPair* const pairA, const dgBodyJacobianPair* const pairB, void* notUsed)
{
	if (pairA->m_bodyIndex < pairB->m_bodyIndex) {
		return -1;
	}
	else if (pairA->m_bodyIndex > pairB->m_bodyIndex) {
		return 1;
	}
	return 0;
}

dgInt32 dgSolver::CompareJointInfos(const dgJointInfo* const infoA, const dgJointInfo* const infoB, void* notUsed)
{
	const dgInt32 restingA = (infoA->m_joint->GetBody0()->m_resting & infoA->m_joint->GetBody1()->m_resting) ? 1 : 0;
	const dgInt32 restingB = (infoB->m_joint->GetBody0()->m_resting & infoB->m_joint->GetBody1()->m_resting) ? 1 : 0;

	const dgInt32 countA = (restingA << 24) + infoA->m_pairCount;
	const dgInt32 countB = (restingB << 24) + infoB->m_pairCount;

	if (countA < countB) {
		return 1;
	}
	if (countA > countB) {
		return -1;
	}
	return 0;
}

void dgSolver::InitJacobianMatrixKernel(void* const context, void* const, dgInt32 threadID)
{
	dgSolver* const me = (dgSolver*)context;
	me->InitJacobianMatrix(threadID);
}

void dgSolver::TransposeMassMatrixKernel(void* const context, void* const, dgInt32 threadID)
{
	dgSolver* const me = (dgSolver*)context;
	me->TransposeMassMatrix(threadID);
}

void dgSolver::InitJacobianMatrix(dgInt32 threadID)
{
	dgLeftHandSide* const leftHandSide = &m_world->GetSolverMemory().m_leftHandSizeBuffer[0];
	dgRightHandSide* const rightHandSide = &m_world->GetSolverMemory().m_righHandSizeBuffer[0];
	dgBodyJacobianPair* const bodyJacobiansPairs = m_bodyJacobiansPairs;

	dgContraintDescritor constraintParams;
	constraintParams.m_world = m_world;
	constraintParams.m_threadIndex = threadID;
	constraintParams.m_timestep = m_timestep;
	constraintParams.m_invTimestep = m_invTimestep;

	const dgInt32 step = m_threadCounts;
	const dgInt32 jointCount = m_cluster->m_jointCount;
	for (dgInt32 i = threadID; i < jointCount; i += step) {
		dgJointInfo* const jointInfo = &m_jointArray[i];
		dgConstraint* const constraint = jointInfo->m_joint;
		dgAssert(jointInfo->m_m0 >= 0);
		dgAssert(jointInfo->m_m1 >= 0);
		dgAssert(jointInfo->m_m0 != jointInfo->m_m1);
		const dgInt32 rowBase = dgAtomicExchangeAndAdd(&m_jacobianMatrixRowAtomicIndex, jointInfo->m_pairCount);
		m_world->GetJacobianDerivatives(constraintParams, jointInfo, constraint, leftHandSide, rightHandSide, rowBase);
		BuildJacobianMatrix(jointInfo, leftHandSide, rightHandSide);

		bodyJacobiansPairs[i * 2 + 0].m_bodyIndex = jointInfo->m_m0;
		bodyJacobiansPairs[i * 2 + 0].m_rowCount = jointInfo->m_pairCount;
		bodyJacobiansPairs[i * 2 + 0].m_rowStart = jointInfo->m_pairStart * 4;
		bodyJacobiansPairs[i * 2 + 0].m_righHandStart = jointInfo->m_pairStart;
		bodyJacobiansPairs[i * 2 + 0].m_preconditioner = jointInfo->m_preconditioner0;

		bodyJacobiansPairs[i * 2 + 1].m_bodyIndex = jointInfo->m_m1;
		bodyJacobiansPairs[i * 2 + 1].m_rowCount = jointInfo->m_pairCount;
		bodyJacobiansPairs[i * 2 + 1].m_rowStart = jointInfo->m_pairStart * 4 + 1;
		bodyJacobiansPairs[i * 2 + 1].m_righHandStart = jointInfo->m_pairStart;
		bodyJacobiansPairs[i * 2 + 1].m_preconditioner = jointInfo->m_preconditioner1;
	}
}

DG_INLINE void dgSolver::TransposeRow(dgSoaMatrixElement* const row, const dgJointInfo* const jointInfoArray, dgInt32 index)
{
	const dgLeftHandSide* const leftHandSide = &m_world->GetSolverMemory().m_leftHandSizeBuffer[0];
	const dgRightHandSide* const rightHandSide = &m_world->GetSolverMemory().m_righHandSizeBuffer[0];
	if (jointInfoArray[0].m_pairCount == jointInfoArray[DG_SOA_WORD_GROUP_SIZE - 1].m_pairCount) {
		for (dgInt32 i = 0; i < DG_SOA_WORD_GROUP_SIZE; i++) {
			const dgJointInfo* const jointInfo = &jointInfoArray[i];
			const dgLeftHandSide* const lhs = &leftHandSide[jointInfo->m_pairStart + index];
			const dgRightHandSide* const rhs = &rightHandSide[jointInfo->m_pairStart + index];

			row->m_Jt.m_jacobianM0.m_linear.m_x[i] = lhs->m_Jt.m_jacobianM0.m_linear.m_x;
			row->m_Jt.m_jacobianM0.m_linear.m_y[i] = lhs->m_Jt.m_jacobianM0.m_linear.m_y;
			row->m_Jt.m_jacobianM0.m_linear.m_z[i] = lhs->m_Jt.m_jacobianM0.m_linear.m_z;
			row->m_Jt.m_jacobianM0.m_angular.m_x[i] = lhs->m_Jt.m_jacobianM0.m_angular.m_x;
			row->m_Jt.m_jacobianM0.m_angular.m_y[i] = lhs->m_Jt.m_jacobianM0.m_angular.m_y;
			row->m_Jt.m_jacobianM0.m_angular.m_z[i] = lhs->m_Jt.m_jacobianM0.m_angular.m_z;
			row->m_Jt.m_jacobianM1.m_linear.m_x[i] = lhs->m_Jt.m_jacobianM1.m_linear.m_x;
			row->m_Jt.m_jacobianM1.m_linear.m_y[i] = lhs->m_Jt.m_jacobianM1.m_linear.m_y;
			row->m_Jt.m_jacobianM1.m_linear.m_z[i] = lhs->m_Jt.m_jacobianM1.m_linear.m_z;
			row->m_Jt.m_jacobianM1.m_angular.m_x[i] = lhs->m_Jt.m_jacobianM1.m_angular.m_x;
			row->m_Jt.m_jacobianM1.m_angular.m_y[i] = lhs->m_Jt.m_jacobianM1.m_angular.m_y;
			row->m_Jt.m_jacobianM1.m_angular.m_z[i] = lhs->m_Jt.m_jacobianM1.m_angular.m_z;

			row->m_JMinv.m_jacobianM0.m_linear.m_x[i] = lhs->m_JMinv.m_jacobianM0.m_linear.m_x;
			row->m_JMinv.m_jacobianM0.m_linear.m_y[i] = lhs->m_JMinv.m_jacobianM0.m_linear.m_y;
			row->m_JMinv.m_jacobianM0.m_linear.m_z[i] = lhs->m_JMinv.m_jacobianM0.m_linear.m_z;
			row->m_JMinv.m_jacobianM0.m_angular.m_x[i] = lhs->m_JMinv.m_jacobianM0.m_angular.m_x;
			row->m_JMinv.m_jacobianM0.m_angular.m_y[i] = lhs->m_JMinv.m_jacobianM0.m_angular.m_y;
			row->m_JMinv.m_jacobianM0.m_angular.m_z[i] = lhs->m_JMinv.m_jacobianM0.m_angular.m_z;
			row->m_JMinv.m_jacobianM1.m_linear.m_x[i] = lhs->m_JMinv.m_jacobianM1.m_linear.m_x;
			row->m_JMinv.m_jacobianM1.m_linear.m_y[i] = lhs->m_JMinv.m_jacobianM1.m_linear.m_y;
			row->m_JMinv.m_jacobianM1.m_linear.m_z[i] = lhs->m_JMinv.m_jacobianM1.m_linear.m_z;
			row->m_JMinv.m_jacobianM1.m_angular.m_x[i] = lhs->m_JMinv.m_jacobianM1.m_angular.m_x;
			row->m_JMinv.m_jacobianM1.m_angular.m_y[i] = lhs->m_JMinv.m_jacobianM1.m_angular.m_y;
			row->m_JMinv.m_jacobianM1.m_angular.m_z[i] = lhs->m_JMinv.m_jacobianM1.m_angular.m_z;

			row->m_force[i] = rhs->m_force;
			row->m_diagDamp[i] = rhs->m_diagDamp;
			row->m_invJinvMJt[i] = rhs->m_invJinvMJt;
			row->m_coordenateAccel[i] = rhs->m_coordenateAccel;
			row->m_normalForceIndex.m_i[i] = rhs->m_normalForceIndex;
			row->m_lowerBoundFrictionCoefficent[i] = rhs->m_lowerBoundFrictionCoefficent;
			row->m_upperBoundFrictionCoefficent[i] = rhs->m_upperBoundFrictionCoefficent;
		}
	} else {
		memset(row, 0, sizeof (dgSoaMatrixElement));
		for (dgInt32 i = 0; i < DG_SOA_WORD_GROUP_SIZE; i++) {
			if (index < jointInfoArray[i].m_pairCount) {
				const dgJointInfo* const jointInfo = &jointInfoArray[i];
				const dgLeftHandSide* const lhs = &leftHandSide[jointInfo->m_pairStart + index];
				const dgRightHandSide* const rhs = &rightHandSide[jointInfo->m_pairStart + index];

				row->m_Jt.m_jacobianM0.m_linear.m_x[i] = lhs->m_Jt.m_jacobianM0.m_linear.m_x;
				row->m_Jt.m_jacobianM0.m_linear.m_y[i] = lhs->m_Jt.m_jacobianM0.m_linear.m_y;
				row->m_Jt.m_jacobianM0.m_linear.m_z[i] = lhs->m_Jt.m_jacobianM0.m_linear.m_z;
				row->m_Jt.m_jacobianM0.m_angular.m_x[i] = lhs->m_Jt.m_jacobianM0.m_angular.m_x;
				row->m_Jt.m_jacobianM0.m_angular.m_y[i] = lhs->m_Jt.m_jacobianM0.m_angular.m_y;
				row->m_Jt.m_jacobianM0.m_angular.m_z[i] = lhs->m_Jt.m_jacobianM0.m_angular.m_z;
				row->m_Jt.m_jacobianM1.m_linear.m_x[i] = lhs->m_Jt.m_jacobianM1.m_linear.m_x;
				row->m_Jt.m_jacobianM1.m_linear.m_y[i] = lhs->m_Jt.m_jacobianM1.m_linear.m_y;
				row->m_Jt.m_jacobianM1.m_linear.m_z[i] = lhs->m_Jt.m_jacobianM1.m_linear.m_z;
				row->m_Jt.m_jacobianM1.m_angular.m_x[i] = lhs->m_Jt.m_jacobianM1.m_angular.m_x;
				row->m_Jt.m_jacobianM1.m_angular.m_y[i] = lhs->m_Jt.m_jacobianM1.m_angular.m_y;
				row->m_Jt.m_jacobianM1.m_angular.m_z[i] = lhs->m_Jt.m_jacobianM1.m_angular.m_z;

				row->m_JMinv.m_jacobianM0.m_linear.m_x[i] = lhs->m_JMinv.m_jacobianM0.m_linear.m_x;
				row->m_JMinv.m_jacobianM0.m_linear.m_y[i] = lhs->m_JMinv.m_jacobianM0.m_linear.m_y;
				row->m_JMinv.m_jacobianM0.m_linear.m_z[i] = lhs->m_JMinv.m_jacobianM0.m_linear.m_z;
				row->m_JMinv.m_jacobianM0.m_angular.m_x[i] = lhs->m_JMinv.m_jacobianM0.m_angular.m_x;
				row->m_JMinv.m_jacobianM0.m_angular.m_y[i] = lhs->m_JMinv.m_jacobianM0.m_angular.m_y;
				row->m_JMinv.m_jacobianM0.m_angular.m_z[i] = lhs->m_JMinv.m_jacobianM0.m_angular.m_z;
				row->m_JMinv.m_jacobianM1.m_linear.m_x[i] = lhs->m_JMinv.m_jacobianM1.m_linear.m_x;
				row->m_JMinv.m_jacobianM1.m_linear.m_y[i] = lhs->m_JMinv.m_jacobianM1.m_linear.m_y;
				row->m_JMinv.m_jacobianM1.m_linear.m_z[i] = lhs->m_JMinv.m_jacobianM1.m_linear.m_z;
				row->m_JMinv.m_jacobianM1.m_angular.m_x[i] = lhs->m_JMinv.m_jacobianM1.m_angular.m_x;
				row->m_JMinv.m_jacobianM1.m_angular.m_y[i] = lhs->m_JMinv.m_jacobianM1.m_angular.m_y;
				row->m_JMinv.m_jacobianM1.m_angular.m_z[i] = lhs->m_JMinv.m_jacobianM1.m_angular.m_z;

				row->m_force[i] = rhs->m_force;
				row->m_diagDamp[i] = rhs->m_diagDamp;
				row->m_invJinvMJt[i] = rhs->m_invJinvMJt;
				row->m_coordenateAccel[i] = rhs->m_coordenateAccel;
				row->m_normalForceIndex.m_i[i] = rhs->m_normalForceIndex;
				row->m_lowerBoundFrictionCoefficent[i] = rhs->m_lowerBoundFrictionCoefficent;
				row->m_upperBoundFrictionCoefficent[i] = rhs->m_upperBoundFrictionCoefficent;
			} else {
				row->m_normalForceIndex.m_i[i] = DG_INDEPENDENT_ROW;
			}
		}
	}
}

void dgSolver::TransposeMassMatrix(dgInt32 threadID)
{
	const dgJointInfo* const jointInfoArray = m_jointArray;
	dgSoaMatrixElement* const massMatrixArray = &m_massMatrix[0];

	const dgInt32 step = m_threadCounts;
	const dgInt32 jointCount = m_jointCount;
	for (dgInt32 i = threadID; i < jointCount; i += step) {
		const dgInt32 index = i * DG_SOA_WORD_GROUP_SIZE;
		const dgInt32 rowCount = jointInfoArray[index].m_pairCount;
		const dgInt32 rowSoaStart = dgAtomicExchangeAndAdd(&m_soaRowsCount, rowCount);
		m_soaRowStart[i] = rowSoaStart;
		for (dgInt32 j = 0; j < rowCount; j ++) {
			dgSoaMatrixElement* const row = &massMatrixArray[rowSoaStart + j];
			TransposeRow (row, &jointInfoArray[index], j);
		}
	}
}

DG_INLINE void dgSolver::BuildJacobianMatrix(dgJointInfo* const jointInfo, dgLeftHandSide* const leftHandSide, dgRightHandSide* const rightHandSide)
{
	const dgInt32 m0 = jointInfo->m_m0;
	const dgInt32 m1 = jointInfo->m_m1;
	const dgInt32 index = jointInfo->m_pairStart;
	const dgInt32 count = jointInfo->m_pairCount;
	const dgDynamicBody* const body0 = (dgDynamicBody*)m_bodyArray[m0].m_body;
	const dgDynamicBody* const body1 = (dgDynamicBody*)m_bodyArray[m1].m_body;
	const bool isBilateral = jointInfo->m_joint->IsBilateral();

	const dgMatrix invInertia0 = body0->m_invWorldInertiaMatrix;
	const dgMatrix invInertia1 = body1->m_invWorldInertiaMatrix;
	const dgVector invMass0(body0->m_invMass[3]);
	const dgVector invMass1(body1->m_invMass[3]);

	dgSoaFloat force0(m_soaZero);
	if (body0->IsRTTIType(dgBody::m_dynamicBodyRTTI)) {
		force0 = dgSoaFloat(body0->m_externalForce, body0->m_externalTorque);
	}

	dgSoaFloat force1(m_soaZero);
	if (body1->IsRTTIType(dgBody::m_dynamicBodyRTTI)) {
		force1 = dgSoaFloat(body1->m_externalForce, body1->m_externalTorque);
	}

	jointInfo->m_preconditioner0 = dgFloat32(1.0f);
	jointInfo->m_preconditioner1 = dgFloat32(1.0f);
	if ((invMass0.GetScalar() > dgFloat32(0.0f)) && (invMass1.GetScalar() > dgFloat32(0.0f)) && !(body0->GetSkeleton() && body1->GetSkeleton())) {
		const dgFloat32 mass0 = body0->GetMass().m_w;
		const dgFloat32 mass1 = body1->GetMass().m_w;
		if (mass0 > (DG_DIAGONAL_PRECONDITIONER * mass1)) {
			jointInfo->m_preconditioner0 = mass0 / (mass1 * DG_DIAGONAL_PRECONDITIONER);
		} else if (mass1 > (DG_DIAGONAL_PRECONDITIONER * mass0)) {
			jointInfo->m_preconditioner1 = mass1 / (mass0 * DG_DIAGONAL_PRECONDITIONER);
		}
	}

	const dgFloat32 forceImpulseScale = dgFloat32(1.0f);
	const dgSoaFloat weight0(m_bodyProxyArray[m0].m_weight * jointInfo->m_preconditioner0);
	const dgSoaFloat weight1(m_bodyProxyArray[m1].m_weight * jointInfo->m_preconditioner0);
	for (dgInt32 i = 0; i < count; i++) {
		dgLeftHandSide* const row = &leftHandSide[index + i];
		dgRightHandSide* const rhs = &rightHandSide[index + i];

		row->m_JMinv.m_jacobianM0.m_linear = row->m_Jt.m_jacobianM0.m_linear * invMass0;
		row->m_JMinv.m_jacobianM0.m_angular = invInertia0.RotateVector(row->m_Jt.m_jacobianM0.m_angular);
		row->m_JMinv.m_jacobianM1.m_linear = row->m_Jt.m_jacobianM1.m_linear * invMass1;
		row->m_JMinv.m_jacobianM1.m_angular = invInertia1.RotateVector(row->m_Jt.m_jacobianM1.m_angular);

		const dgSoaFloat& JMinvM0 = (dgSoaFloat&)row->m_JMinv.m_jacobianM0;
		const dgSoaFloat& JMinvM1 = (dgSoaFloat&)row->m_JMinv.m_jacobianM1;
		const dgSoaFloat tmpAccel((JMinvM0 * force0).MulAdd(JMinvM1, force1));

		dgFloat32 extenalAcceleration = -tmpAccel.AddHorizontal();
		rhs->m_deltaAccel = extenalAcceleration * forceImpulseScale;
		rhs->m_coordenateAccel += extenalAcceleration * forceImpulseScale;
		dgAssert(rhs->m_jointFeebackForce);
		const dgFloat32 force = rhs->m_jointFeebackForce->m_force * forceImpulseScale;
		rhs->m_force = isBilateral ? dgClamp(force, rhs->m_lowerBoundFrictionCoefficent, rhs->m_upperBoundFrictionCoefficent) : force;
		rhs->m_maxImpact = dgFloat32(0.0f);

		const dgSoaFloat& JtM0 = (dgSoaFloat&)row->m_Jt.m_jacobianM0;
		const dgSoaFloat& JtM1 = (dgSoaFloat&)row->m_Jt.m_jacobianM1;
		const dgSoaFloat tmpDiag((weight0 * JMinvM0 * JtM0).MulAdd(weight1, JMinvM1 * JtM1));

		dgFloat32 diag = tmpDiag.AddHorizontal();
		dgAssert(diag > dgFloat32(0.0f));
		rhs->m_diagDamp = diag * rhs->m_stiffness;
		diag *= (dgFloat32(1.0f) + rhs->m_stiffness);
		//rhs->m_jinvMJt = diag;
		rhs->m_invJinvMJt = dgFloat32(1.0f) / diag;
	}
}

void dgSolver::CalculateJointsAccelerationKernel(void* const context, void* const, dgInt32 threadID)
{
	dgSolver* const me = (dgSolver*)context;
	me->CalculateJointsAcceleration(threadID);
}

void dgSolver::CalculateJointsForceKernel(void* const context, void* const, dgInt32 threadID)
{
	dgSolver* const me = (dgSolver*)context;
	me->CalculateJointsForce(threadID);
}

void dgSolver::CalculateBodyForceKernel(void* const context, void* const, dgInt32 threadID)
{
	dgSolver* const me = (dgSolver*)context;
	me->CalculateBodyForce(threadID);
}

void dgSolver::IntegrateBodiesVelocityKernel(void* const context, void* const worldContext, dgInt32 threadID)
{
	dgSolver* const me = (dgSolver*)context;
	me->IntegrateBodiesVelocity(threadID);
}

void dgSolver::CalculateBodiesAccelerationKernel(void* const context, void* const, dgInt32 threadID)
{
	dgSolver* const me = (dgSolver*)context;
	me->CalculateBodiesAcceleration(threadID);
}

void dgSolver::UpdateForceFeedbackKernel(void* const context, void* const, dgInt32 threadID)
{
	dgSolver* const me = (dgSolver*)context;
	me->UpdateForceFeedback(threadID);
}

void dgSolver::UpdateKinematicFeedbackKernel(void* const context, void* const, dgInt32 threadID)
{
	dgSolver* const me = (dgSolver*)context;
	me->UpdateKinematicFeedback(threadID);
}

void dgSolver::UpdateRowAccelerationKernel(void* const context, void* const, dgInt32 threadID)
{
	dgSolver* const me = (dgSolver*)context;
	me->UpdateRowAcceleration(threadID);
}

void dgSolver::CalculateJointsAcceleration()
{
	for (dgInt32 i = 0; i < m_threadCounts; i++) {
		m_world->QueueJob(CalculateJointsAccelerationKernel, this, NULL, "dgSolver::CalculateJointsAcceleration");
	}
	m_world->SynchronizationBarrier();
	m_firstPassCoef = dgFloat32(1.0f);

	for (dgInt32 i = 0; i < m_threadCounts; i++) {
		m_world->QueueJob(UpdateRowAccelerationKernel, this, NULL, "dgSolver::UpdateRowAcceleration");
	}
	m_world->SynchronizationBarrier();
}

void dgSolver::CalculateBodiesAcceleration()
{
	for (dgInt32 i = 0; i < m_threadCounts; i++) {
		m_world->QueueJob(CalculateBodiesAccelerationKernel, this, NULL, "dgSolver::CalculateBodiesAcceleration");
	}
	m_world->SynchronizationBarrier();
}

void dgSolver::CalculateJointsForce()
{
	for (dgInt32 i = 0; i < m_threadCounts; i++) {
		m_world->QueueJob(CalculateJointsForceKernel, this, NULL, "dgSolver::CalculateJointsForce");
	}
	m_world->SynchronizationBarrier();
}

void dgSolver::CalculateBodyForce()
{
	for (dgInt32 i = 0; i < m_threadCounts; i++) {
		m_world->QueueJob(CalculateBodyForceKernel, this, NULL, "dgSolver::CalculateBodyForce");
	}
	m_world->SynchronizationBarrier();

	dgJacobian* const internalForces = &m_world->GetSolverMemory().m_internalForcesBuffer[0];
	internalForces[0].m_linear = m_zero;
	internalForces[0].m_angular = m_zero;
}

void dgSolver::IntegrateBodiesVelocity()
{
	for (dgInt32 i = 0; i < m_threadCounts; i++) {
		m_world->QueueJob(IntegrateBodiesVelocityKernel, this, NULL, "dgSolver::IntegrateBodiesVelocity");
	}
	m_world->SynchronizationBarrier();
}


void dgSolver::UpdateForceFeedback()
{
	for (dgInt32 i = 0; i < m_threadCounts; i++) {
		m_world->QueueJob(UpdateForceFeedbackKernel, this, NULL, "dgSolver::UpdateForceFeedback");
	}
	m_world->SynchronizationBarrier();
}

void dgSolver::UpdateKinematicFeedback()
{
	for (dgInt32 i = 0; i < m_threadCounts; i++) {
		m_world->QueueJob(UpdateKinematicFeedbackKernel, this, NULL, "dgSolver::UpdateKinematicFeedback");
	}
	m_world->SynchronizationBarrier();
}

void dgSolver::CalculateJointsAcceleration(dgInt32 threadID)
{
	dgJointAccelerationDecriptor joindDesc;
	joindDesc.m_timeStep = m_timestepRK;
	joindDesc.m_invTimeStep = m_invTimestepRK;
	joindDesc.m_firstPassCoefFlag = m_firstPassCoef;
	dgRightHandSide* const rightHandSide = &m_world->GetSolverMemory().m_righHandSizeBuffer[0];
	const dgLeftHandSide* const leftHandSide = &m_world->GetSolverMemory().m_leftHandSizeBuffer[0];

	const dgInt32 step = m_threadCounts;
	const dgInt32 jointCount = m_cluster->m_jointCount;
	for (dgInt32 i = threadID; i < jointCount; i += step) {
		dgJointInfo* const jointInfo = &m_jointArray[i];
		dgConstraint* const constraint = jointInfo->m_joint;
		const dgInt32 pairStart = jointInfo->m_pairStart;
		joindDesc.m_rowsCount = jointInfo->m_pairCount;
		joindDesc.m_leftHandSide = &leftHandSide[pairStart];
		joindDesc.m_rightHandSide = &rightHandSide[pairStart];

		constraint->JointAccelerations(&joindDesc);
	}
}

DG_INLINE dgFloat32 dgSolver::CalculateJointForce(const dgJointInfo* const jointInfo, dgSoaMatrixElement* const massMatrix, const dgSoaFloat* const internalForces) const
{
	dgSoaVector6 forceM0;
	dgSoaVector6 forceM1;
	dgSoaFloat preconditioner0;
	dgSoaFloat preconditioner1;
	dgSoaFloat accNorm(m_soaZero);
	dgSoaFloat normalForce[DG_CONSTRAINT_MAX_ROWS + 1];
	const dgBodyProxy* const bodyProxyArray = m_bodyProxyArray;

	for (dgInt32 i = 0; i < DG_SOA_WORD_GROUP_SIZE; i++) {
		const dgInt32 m0 = jointInfo[i].m_m0;
		const dgInt32 m1 = jointInfo[i].m_m1;

		forceM0.m_linear.m_x[i] = internalForces[m0][0];
		forceM0.m_linear.m_y[i] = internalForces[m0][1];
		forceM0.m_linear.m_z[i] = internalForces[m0][2];
		forceM0.m_angular.m_x[i] = internalForces[m0][4];
		forceM0.m_angular.m_y[i] = internalForces[m0][5];
		forceM0.m_angular.m_z[i] = internalForces[m0][6];

		forceM1.m_linear.m_x[i] = internalForces[m1][0];
		forceM1.m_linear.m_y[i] = internalForces[m1][1];
		forceM1.m_linear.m_z[i] = internalForces[m1][2];
		forceM1.m_angular.m_x[i] = internalForces[m1][4];
		forceM1.m_angular.m_y[i] = internalForces[m1][5];
		forceM1.m_angular.m_z[i] = internalForces[m1][6];

		preconditioner0[i] = jointInfo[i].m_preconditioner0 * bodyProxyArray[m0].m_weight;
		preconditioner1[i] = jointInfo[i].m_preconditioner1 * bodyProxyArray[m1].m_weight;
	}

	forceM0.m_linear.m_x = forceM0.m_linear.m_x * preconditioner0;
	forceM0.m_linear.m_y = forceM0.m_linear.m_y * preconditioner0;
	forceM0.m_linear.m_z = forceM0.m_linear.m_z * preconditioner0;
	forceM0.m_angular.m_x = forceM0.m_angular.m_x * preconditioner0;
	forceM0.m_angular.m_y = forceM0.m_angular.m_y * preconditioner0;
	forceM0.m_angular.m_z = forceM0.m_angular.m_z * preconditioner0;

	forceM1.m_linear.m_x = forceM1.m_linear.m_x * preconditioner1;
	forceM1.m_linear.m_y = forceM1.m_linear.m_y * preconditioner1;
	forceM1.m_linear.m_z = forceM1.m_linear.m_z * preconditioner1;
	forceM1.m_angular.m_x = forceM1.m_angular.m_x * preconditioner1;
	forceM1.m_angular.m_y = forceM1.m_angular.m_y * preconditioner1;
	forceM1.m_angular.m_z = forceM1.m_angular.m_z * preconditioner1;

	const dgInt32 rowsCount = jointInfo->m_pairCount;
	normalForce[0] = m_soaOne;
	for (dgInt32 j = 0; j < rowsCount; j++) {
		dgSoaMatrixElement* const row = &massMatrix[j];

		dgSoaFloat a;
		a = row->m_coordenateAccel.MulSub(row->m_JMinv.m_jacobianM0.m_linear.m_x, forceM0.m_linear.m_x);
		a = a.MulSub(row->m_JMinv.m_jacobianM0.m_linear.m_y, forceM0.m_linear.m_y);
		a = a.MulSub(row->m_JMinv.m_jacobianM0.m_linear.m_z, forceM0.m_linear.m_z);
		a = a.MulSub(row->m_JMinv.m_jacobianM0.m_angular.m_x, forceM0.m_angular.m_x);
		a = a.MulSub(row->m_JMinv.m_jacobianM0.m_angular.m_y, forceM0.m_angular.m_y);
		a = a.MulSub(row->m_JMinv.m_jacobianM0.m_angular.m_z, forceM0.m_angular.m_z);

		a = a.MulSub(row->m_JMinv.m_jacobianM1.m_linear.m_x, forceM1.m_linear.m_x);
		a = a.MulSub(row->m_JMinv.m_jacobianM1.m_linear.m_y, forceM1.m_linear.m_y);
		a = a.MulSub(row->m_JMinv.m_jacobianM1.m_linear.m_z, forceM1.m_linear.m_z);
		a = a.MulSub(row->m_JMinv.m_jacobianM1.m_angular.m_x, forceM1.m_angular.m_x);
		a = a.MulSub(row->m_JMinv.m_jacobianM1.m_angular.m_y, forceM1.m_angular.m_y);
		a = a.MulSub(row->m_JMinv.m_jacobianM1.m_angular.m_z, forceM1.m_angular.m_z);
		a = a.MulSub(row->m_force, row->m_diagDamp);

		dgSoaFloat f(row->m_force.MulAdd(row->m_invJinvMJt,  a));

		dgSoaFloat frictionNormal;
		for (dgInt32 k = 0; k < DG_SOA_WORD_GROUP_SIZE; k++) {
			dgAssert(row->m_normalForceIndex.m_i[k] >= -1);
			dgAssert(row->m_normalForceIndex.m_i[k] <= rowsCount);
			const dgInt32 frictionIndex = dgInt32(row->m_normalForceIndex.m_i[k] + 1);
			frictionNormal[k] = normalForce[frictionIndex][k];
		}

		dgSoaFloat lowerFrictionForce(frictionNormal * row->m_lowerBoundFrictionCoefficent);
		dgSoaFloat upperFrictionForce(frictionNormal * row->m_upperBoundFrictionCoefficent);

		a = a.AndNot((f > upperFrictionForce) | (f < lowerFrictionForce));
		f = f.GetMax(lowerFrictionForce).GetMin(upperFrictionForce);

		accNorm = accNorm + a * a;
		dgSoaFloat deltaForce(f - row->m_force);

		row->m_force = f;
		normalForce[j + 1] = f;

		dgSoaFloat deltaForce0(deltaForce * preconditioner0);
		dgSoaFloat deltaForce1(deltaForce * preconditioner1);

		forceM0.m_linear.m_x = forceM0.m_linear.m_x.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_x, deltaForce0);
		forceM0.m_linear.m_y = forceM0.m_linear.m_y.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_y, deltaForce0);
		forceM0.m_linear.m_z = forceM0.m_linear.m_z.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_z, deltaForce0);
		forceM0.m_angular.m_x = forceM0.m_angular.m_x.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_x, deltaForce0);
		forceM0.m_angular.m_y = forceM0.m_angular.m_y.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_y, deltaForce0);
		forceM0.m_angular.m_z = forceM0.m_angular.m_z.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_z, deltaForce0);

		forceM1.m_linear.m_x = forceM1.m_linear.m_x.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_x, deltaForce1);
		forceM1.m_linear.m_y = forceM1.m_linear.m_y.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_y, deltaForce1);
		forceM1.m_linear.m_z = forceM1.m_linear.m_z.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_z, deltaForce1);
		forceM1.m_angular.m_x = forceM1.m_angular.m_x.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_x, deltaForce1);
		forceM1.m_angular.m_y = forceM1.m_angular.m_y.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_y, deltaForce1);
		forceM1.m_angular.m_z = forceM1.m_angular.m_z.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_z, deltaForce1);
	}

	const dgFloat32 tol = dgFloat32(0.5f);
	const dgFloat32 tol2 = tol * tol;
	dgSoaFloat maxAccel(accNorm);

	for (dgInt32 i = 0; (i < 4) && (maxAccel.AddHorizontal() > tol2); i++) {
		maxAccel = m_soaZero;
		for (dgInt32 j = 0; j < rowsCount; j++) {
			dgSoaMatrixElement* const row = &massMatrix[j];

			dgSoaFloat a;
			a = row->m_coordenateAccel.MulSub(row->m_JMinv.m_jacobianM0.m_linear.m_x, forceM0.m_linear.m_x);
			a = a.MulSub(row->m_JMinv.m_jacobianM0.m_linear.m_y, forceM0.m_linear.m_y);
			a = a.MulSub(row->m_JMinv.m_jacobianM0.m_linear.m_z, forceM0.m_linear.m_z);
			a = a.MulSub(row->m_JMinv.m_jacobianM0.m_angular.m_x, forceM0.m_angular.m_x);
			a = a.MulSub(row->m_JMinv.m_jacobianM0.m_angular.m_y, forceM0.m_angular.m_y);
			a = a.MulSub(row->m_JMinv.m_jacobianM0.m_angular.m_z, forceM0.m_angular.m_z);

			a = a.MulSub(row->m_JMinv.m_jacobianM1.m_linear.m_x, forceM1.m_linear.m_x);
			a = a.MulSub(row->m_JMinv.m_jacobianM1.m_linear.m_y, forceM1.m_linear.m_y);
			a = a.MulSub(row->m_JMinv.m_jacobianM1.m_linear.m_z, forceM1.m_linear.m_z);
			a = a.MulSub(row->m_JMinv.m_jacobianM1.m_angular.m_x, forceM1.m_angular.m_x);
			a = a.MulSub(row->m_JMinv.m_jacobianM1.m_angular.m_y, forceM1.m_angular.m_y);
			a = a.MulSub(row->m_JMinv.m_jacobianM1.m_angular.m_z, forceM1.m_angular.m_z);
			a = a.MulSub(row->m_force, row->m_diagDamp);

			dgSoaFloat f(row->m_force.MulAdd(row->m_invJinvMJt, a));

			dgSoaFloat frictionNormal;
			for (dgInt32 k = 0; k < DG_SOA_WORD_GROUP_SIZE; k++) {
				dgAssert(row->m_normalForceIndex.m_i[k] >= -1);
				dgAssert(row->m_normalForceIndex.m_i[k] <= rowsCount);
				const dgInt32 frictionIndex = dgInt32 (row->m_normalForceIndex.m_i[k] + 1);
				frictionNormal[k] = normalForce[frictionIndex][k];
			}

			dgSoaFloat lowerFrictionForce(frictionNormal * row->m_lowerBoundFrictionCoefficent);
			dgSoaFloat upperFrictionForce(frictionNormal * row->m_upperBoundFrictionCoefficent);

			a = a.AndNot((f > upperFrictionForce) | (f < lowerFrictionForce));
			f = f.GetMax(lowerFrictionForce).GetMin(upperFrictionForce);
			maxAccel = maxAccel + a * a;

			dgSoaFloat deltaForce(f - row->m_force);

			row->m_force = f;
			normalForce[j + 1] = f;

			dgSoaFloat deltaForce0(deltaForce * preconditioner0);
			dgSoaFloat deltaForce1(deltaForce * preconditioner1);

			forceM0.m_linear.m_x = forceM0.m_linear.m_x.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_x, deltaForce0);
			forceM0.m_linear.m_y = forceM0.m_linear.m_y.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_y, deltaForce0);
			forceM0.m_linear.m_z = forceM0.m_linear.m_z.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_z, deltaForce0);
			forceM0.m_angular.m_x = forceM0.m_angular.m_x.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_x, deltaForce0);
			forceM0.m_angular.m_y = forceM0.m_angular.m_y.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_y, deltaForce0);
			forceM0.m_angular.m_z = forceM0.m_angular.m_z.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_z, deltaForce0);

			forceM1.m_linear.m_x = forceM1.m_linear.m_x.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_x, deltaForce1);
			forceM1.m_linear.m_y = forceM1.m_linear.m_y.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_y, deltaForce1);
			forceM1.m_linear.m_z = forceM1.m_linear.m_z.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_z, deltaForce1);
			forceM1.m_angular.m_x = forceM1.m_angular.m_x.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_x, deltaForce1);
			forceM1.m_angular.m_y = forceM1.m_angular.m_y.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_y, deltaForce1);
			forceM1.m_angular.m_z = forceM1.m_angular.m_z.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_z, deltaForce1);
		}
	}

	return accNorm.AddHorizontal();
}

void dgSolver::CalculateJointsForce(dgInt32 threadID)
{
	const dgInt32* const soaRowStart = m_soaRowStart;
	const dgBodyInfo* const bodyArray = m_bodyArray;
	dgSoaMatrixElement* const massMatrix = &m_massMatrix[0];
	dgRightHandSide* const rightHandSide = &m_world->GetSolverMemory().m_righHandSizeBuffer[0];
	dgSoaFloat* const internalForces = (dgSoaFloat*)&m_world->GetSolverMemory().m_internalForcesBuffer[0];
	dgFloat32 accNorm = dgFloat32(0.0f);

	const dgInt32 step = m_threadCounts;
	const dgInt32 jointCount = m_jointCount;
	for (dgInt32 i = threadID; i < jointCount; i += step) {
		const dgInt32 rowStart = soaRowStart[i];
		dgJointInfo* const jointInfo = &m_jointArray[i * DG_SOA_WORD_GROUP_SIZE];

		bool isSleeping = true;
		dgFloat32 accel2 = dgFloat32(0.0f);
		for (dgInt32 j = 0; (j < DG_WORK_GROUP_SIZE) && isSleeping; j++) {
			const dgInt32 m0 = jointInfo[j].m_m0;
			const dgInt32 m1 = jointInfo[j].m_m1;
			const dgBody* const body0 = bodyArray[m0].m_body;
			const dgBody* const body1 = bodyArray[m1].m_body;
			isSleeping &= body0->m_resting;
			isSleeping &= body1->m_resting;
		}
		if (!isSleeping) {
			accel2 = CalculateJointForce(jointInfo, &massMatrix[rowStart], internalForces);
			accNorm += accel2;

			for (dgInt32 j = 0; j < DG_SOA_WORD_GROUP_SIZE; j++) {
				const dgJointInfo* const joint = &jointInfo[j];
				if (joint->m_joint) {
					dgInt32 const rowCount = joint->m_pairCount;
					dgInt32 const rowStartBase = joint->m_pairStart;
					for (dgInt32 k = 0; k < rowCount; k++) {
						const dgSoaMatrixElement* const row = &massMatrix[rowStart + k];
						rightHandSide[k + rowStartBase].m_force = row->m_force[j];
						rightHandSide[k + rowStartBase].m_maxImpact = dgMax(dgAbs(row->m_force[j]), rightHandSide[k + rowStartBase].m_maxImpact);
					}
				}
			}
		}
	}
	m_accelNorm[threadID] = accNorm;
}

void dgSolver::UpdateRowAcceleration(dgInt32 threadID)
{
	dgSoaMatrixElement* const massMatrix = &m_massMatrix[0];
	const dgRightHandSide* const rightHandSide = &m_world->GetSolverMemory().m_righHandSizeBuffer[0];

	const dgInt32* const soaRowStart = m_soaRowStart;
	const dgJointInfo* const jointInfoArray = m_jointArray;

	const dgInt32 step = m_threadCounts;
	const dgInt32 jointCount = m_jointCount;
	for (dgInt32 i = threadID; i < jointCount; i += step) {
		const dgJointInfo* const jointInfoBase = &jointInfoArray[i * DG_SOA_WORD_GROUP_SIZE];

		const dgInt32 rowStart = soaRowStart[i];
		for (dgInt32 j = 0; j < DG_SOA_WORD_GROUP_SIZE; j++) {
			const dgJointInfo* const jointInfo = &jointInfoBase[j];
			if (jointInfo->m_joint) {
				dgInt32 const rowCount = jointInfo->m_pairCount;
				dgInt32 const rowStartBase = jointInfo->m_pairStart;
				for (dgInt32 k = 0; k < rowCount; k++) {
					dgSoaMatrixElement* const row = &massMatrix[rowStart + k];
					row->m_coordenateAccel[j] = rightHandSide[k + rowStartBase].m_coordenateAccel;
				}
			}
		}
	}
}

void dgSolver::CalculateBodyForce(dgInt32 threadID)
{
	const dgBodyProxy* const bodyProxyArray = m_bodyProxyArray;
	const dgBodyJacobianPair* const bodyJacobiansPairs = m_bodyJacobiansPairs;
	dgSoaFloat* const internalForces = (dgSoaFloat*)&m_world->GetSolverMemory().m_internalForcesBuffer[0];
	const dgRightHandSide* const rightHandSide = &m_world->GetSolverMemory().m_righHandSizeBuffer[0];
	const dgSoaFloat* const leftHandSide = (dgSoaFloat*)&m_world->GetSolverMemory().m_leftHandSizeBuffer[0].m_Jt.m_jacobianM0;

	const dgInt32 step = m_threadCounts;;
	const dgInt32 bodyCount = m_cluster->m_bodyCount;
	for (dgInt32 i = threadID; i < bodyCount; i += step) {
		dgSoaFloat forceAcc (m_soaZero);

		const dgBodyProxy* const startJoints = &bodyProxyArray[i];
		const dgInt32 jointsCount = dgInt32(startJoints->m_weight);
		const dgBodyJacobianPair* const jointsStart = &bodyJacobiansPairs[startJoints->m_jointStart];

		for (dgInt32 j = 0; j < jointsCount; j++) {
			const dgInt32 rowsCount = (jointsStart[j].m_rowCount >> 1) * 2;	
			const dgSoaFloat* const lhs = &leftHandSide[jointsStart[j].m_rowStart];
			const dgRightHandSide* const rhs = &rightHandSide[jointsStart[j].m_righHandStart];
			for (dgInt32 k = 0; k < rowsCount; k += 2) {
				forceAcc = forceAcc.MulAdd(lhs[(k + 0) * 4], dgSoaFloat(rhs[k + 0].m_force));
				forceAcc = forceAcc.MulAdd(lhs[(k + 1) * 4], dgSoaFloat(rhs[k + 1].m_force));
			}
			if (jointsStart[j].m_rowCount & 1) {
				const dgInt32 k = jointsStart[j].m_rowCount - 1;
				forceAcc = forceAcc.MulAdd(lhs[k * 4], dgSoaFloat(rhs[k].m_force));
			}
		}
		internalForces[i] = forceAcc * dgSoaFloat(startJoints->m_invWeight);
	}
}

void dgSolver::IntegrateBodiesVelocity(dgInt32 threadID)
{
	dgVector speedFreeze2(m_world->m_freezeSpeed2 * dgFloat32(0.1f));
	dgVector freezeOmega2(m_world->m_freezeOmega2 * dgFloat32(0.1f));

	dgVector timestep4(m_timestepRK);
	const dgBodyProxy* const bodyProxyArray = m_bodyProxyArray;
	dgJacobian* const internalForces = &m_world->GetSolverMemory().m_internalForcesBuffer[0];

	const dgInt32 step = m_threadCounts;;
	const dgInt32 bodyCount = m_cluster->m_bodyCount;
	for (dgInt32 i = threadID; i < bodyCount; i += step) {
		dgDynamicBody* const body = (dgDynamicBody*)m_bodyArray[i].m_body;
		dgAssert(body->m_index == i);

		if (body->IsRTTIType(dgBody::m_dynamicBodyRTTI)) {
			const dgVector w(bodyProxyArray[i].m_weight);
			const dgJacobian& forceAndTorque = internalForces[i];
			const dgVector force(body->m_externalForce + forceAndTorque.m_linear * w);
			const dgVector torque(body->m_externalTorque + forceAndTorque.m_angular * w);

			const dgVector velocStep((force.Scale(body->m_invMass.m_w)) * timestep4);
			const dgVector omegaStep((body->m_invWorldInertiaMatrix.RotateVector(torque)) * timestep4);

			if (!body->m_resting) {
				body->m_veloc += velocStep;
				body->m_omega += omegaStep;
			} else {
				const dgVector velocStep2(velocStep.DotProduct(velocStep));
				const dgVector omegaStep2(omegaStep.DotProduct(omegaStep));
				const dgVector test(((velocStep2 > speedFreeze2) | (omegaStep2 > speedFreeze2)) & m_negOne);
				const dgInt32 equilibrium = test.GetSignMask() ? 0 : 1;
				body->m_resting &= equilibrium;
			}
			dgAssert(body->m_veloc.m_w == dgFloat32(0.0f));
			dgAssert(body->m_omega.m_w == dgFloat32(0.0f));
		}
	}
}

void dgSolver::CalculateBodiesAcceleration(dgInt32 threadID)
{
	dgVector invTime(m_invTimestep);
	dgFloat32 maxAccNorm2 = DG_SOLVER_MAX_ERROR * DG_SOLVER_MAX_ERROR;

	const dgInt32 step = m_threadCounts;;
	const dgInt32 bodyCount = m_cluster->m_bodyCount;
	for (dgInt32 i = threadID; i < bodyCount; i += step) {
		dgDynamicBody* const body = (dgDynamicBody*)m_bodyArray[i].m_body;
		m_world->CalculateNetAcceleration(body, invTime, maxAccNorm2);
	}
}

void dgSolver::UpdateForceFeedback(dgInt32 threadID)
{
	const dgRightHandSide* const rightHandSide = &m_world->GetSolverMemory().m_righHandSizeBuffer[0];
	dgInt32 hasJointFeeback = 0;

	const dgInt32 step = m_threadCounts;
	const dgInt32 jointCount = m_cluster->m_jointCount;
	for (dgInt32 i = threadID; i < jointCount; i += step) {
		dgJointInfo* const jointInfo = &m_jointArray[i];
		dgConstraint* const constraint = jointInfo->m_joint;
		const dgInt32 first = jointInfo->m_pairStart;
		const dgInt32 count = jointInfo->m_pairCount;

		for (dgInt32 j = 0; j < count; j++) {
			const dgRightHandSide* const rhs = &rightHandSide[j + first];
			dgAssert(dgCheckFloat(rhs->m_force));
			rhs->m_jointFeebackForce->m_force = rhs->m_force;
			rhs->m_jointFeebackForce->m_impact = rhs->m_maxImpact * m_timestepRK;
		}
		hasJointFeeback |= (constraint->GetUpdateFeedbackFunction() ? 1 : 0);
	}
	m_hasJointFeeback[threadID] = hasJointFeeback;
}


void dgSolver::UpdateKinematicFeedback(dgInt32 threadID)
{
	const dgInt32 step = m_threadCounts;
	const dgInt32 jointCount = m_cluster->m_jointCount;
	for (dgInt32 i = threadID; i < jointCount; i += step) {
		dgJointInfo* const jointInfo = &m_jointArray[i];
		if (jointInfo->m_joint->GetUpdateFeedbackFunction()) {
			jointInfo->m_joint->GetUpdateFeedbackFunction()(*jointInfo->m_joint, m_timestep, threadID);
		}
	}
}

void dgSolver::CalculateForces()
{
	m_firstPassCoef = dgFloat32(0.0f);
	const dgInt32 passes = m_solverPasses;
	const dgInt32 threadCounts = m_world->GetThreadCount();

	for (dgInt32 step = 0; step < 4; step++) {
		CalculateJointsAcceleration();
		dgFloat32 accNorm = DG_SOLVER_MAX_ERROR * dgFloat32(2.0f);
		for (dgInt32 k = 0; (k < passes) && (accNorm > DG_SOLVER_MAX_ERROR); k++) {
			CalculateJointsForce();
			CalculateBodyForce();
			accNorm = dgFloat32(0.0f);
			for (dgInt32 i = 0; i < threadCounts; i++) {
				accNorm = dgMax(accNorm, m_accelNorm[i]);
			}
		}
		IntegrateBodiesVelocity();
	}

	UpdateForceFeedback();

	dgInt32 hasJointFeeback = 0;
	for (dgInt32 i = 0; i < DG_MAX_THREADS_HIVE_COUNT; i++) {
		hasJointFeeback |= m_hasJointFeeback[i];
	}
	CalculateBodiesAcceleration();

	if (hasJointFeeback) {
		UpdateKinematicFeedback();
	}
}

#else
void dgSolver::CalculateJointForces(const dgBodyCluster& cluster, dgBodyInfo* const bodyArray, dgJointInfo* const jointArray, dgFloat32 timestep)
{
	dgAssert(0);
}

#endif

