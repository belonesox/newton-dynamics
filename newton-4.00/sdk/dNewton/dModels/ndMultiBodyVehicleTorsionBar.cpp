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
#include "ndMultiBodyVehicle.h"
#include "ndMultiBodyVehicleMotor.h"
#include "ndMultiBodyVehicleTorsionBar.h"

ndMultiBodyVehicleTorsionBar::ndMultiBodyVehicleTorsionBar(const ndMultiBodyVehicle* const vehicle, ndBodyDynamic* const fixedbody)
	:ndJointBilateralConstraint(1, vehicle->m_chassis, fixedbody, dGetIdentityMatrix())
	,m_springK(dFloat32 (10.0f))
	,m_damperC(dFloat32(1.0f))
	,m_springDamperRegularizer(dFloat32(0.1f))
	,m_axeldCount(0)
{
	const dMatrix worldMatrix (vehicle->m_localFrame * vehicle->m_chassis->GetMatrix());
	CalculateLocalMatrix(worldMatrix, m_localMatrix0, m_localMatrix1);
	SetSolverModel(m_jointkinematicCloseLoop);
}

void ndMultiBodyVehicleTorsionBar::SetTorsionTorque(dFloat32 springK, dFloat32 damperC, dFloat32 springDamperRegularizer)
{
	m_springK = dAbs (springK);
	m_damperC = dAbs(damperC);
	m_springDamperRegularizer = dClamp (springDamperRegularizer, dFloat32 (0.001), dFloat32(0.99f));
}

void ndMultiBodyVehicleTorsionBar::AddAxel(const ndBodyDynamic* const leftTire, const ndBodyDynamic* const rightTire)
{
	if (m_axeldCount < sizeof(m_axles) / sizeof(m_axles[0]))
	{
		m_axles[m_axeldCount].m_leftTire = leftTire;
		m_axles[m_axeldCount].m_rightTire = rightTire;
		m_axeldCount++;
	}
}

void ndMultiBodyVehicleTorsionBar::JacobianDerivative(ndConstraintDescritor& desc)
{
	if (m_axeldCount)
	{
		dAssert(0);
		//if (dAbs(m_gearRatio) > dFloat32(1.0e-2f))
		//{
		//	dMatrix matrix0;
		//	dMatrix matrix1;
		//	
		//	// calculate the position of the pivot point and the Jacobian direction vectors, in global space. 
		//	CalculateGlobalMatrix(matrix0, matrix1);
		//	
		//	AddAngularRowJacobian(desc, matrix0.m_front, dFloat32(0.0f));
		//	
		//	ndJacobian& jacobian0 = desc.m_jacobian[desc.m_rowsCount - 1].m_jacobianM0;
		//	ndJacobian& jacobian1 = desc.m_jacobian[desc.m_rowsCount - 1].m_jacobianM1;
		//
		//	dFloat32 gearRatio = dFloat32(1.0f) / m_gearRatio;
		//	
		//	jacobian0.m_angular = matrix0.m_front;
		//	jacobian1.m_angular = matrix1.m_front.Scale(gearRatio);
		//	
		//	const dVector& omega0 = m_body0->GetOmega();
		//	const dVector& omega1 = m_body1->GetOmega();
		//	
		//	ndMultiBodyVehicleMotor* const rotor = m_chassis->m_motor;
		//	
		//	dFloat32 idleOmega = rotor->m_idleOmega * gearRatio * dFloat32(0.9f);
		//	dFloat32 w0 = omega0.DotProduct(jacobian0.m_angular).GetScalar();
		//	dFloat32 w1 = omega1.DotProduct(jacobian1.m_angular).GetScalar() + idleOmega;
		//	w1 = (gearRatio > dFloat32(0.0f)) ? dMin(w1, dFloat32(0.0f)) : dMax(w1, dFloat32(0.0f));
		//	
		//	const dFloat32 w = (w0 + w1) * dFloat32(0.5f);
		//	SetMotorAcceleration(desc, -w * desc.m_invTimestep);
		//
		//	//SetHighFriction(desc, 1000.0f);
		//	//SetLowerFriction(desc, -1000.0f);
		//}
	}
}

