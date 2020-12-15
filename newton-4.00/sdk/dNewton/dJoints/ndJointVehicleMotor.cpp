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
#include "ndJointVehicleMotor.h"

ndJointVehicleMotor::ndJointVehicleMotor(ndBodyKinematic* const motor, ndBodyKinematic* const chassis)
	:ndJointBilateralConstraint(3, motor, chassis, motor->GetMatrix())
	,m_speed(dFloat32 (0.0f))
	,m_maxSpeed(dFloat32(50.0f))
	,m_throttle(dFloat32(0.0f))
	,m_engineTorque(dFloat32(100.0f))
{
	m_engineTorque = 10.0f;
}

void ndJointVehicleMotor::AlignMatrix()
{
	dMatrix matrix0;
	dMatrix matrix1;
	CalculateGlobalMatrix(matrix0, matrix1);

	//matrix1.m_posit += matrix1.m_up.Scale(1.0f);

	m_body0->SetMatrix(matrix1);
	m_body0->SetVelocity(m_body1->GetVelocity());

	dVector omega0(m_body0->GetOmega());
	dVector omega1(m_body1->GetOmega());
	dVector omega(
		matrix1.m_front.Scale(matrix1.m_front.DotProduct(omega0).GetScalar()) +
		matrix1.m_up.Scale(matrix1.m_up.DotProduct(omega1).GetScalar()) +
		matrix1.m_right.Scale(matrix1.m_right.DotProduct(omega1).GetScalar()));

	//omega += matrix1.m_front.Scale(-40.0f) - matrix1.m_front.Scale(matrix1.m_front.DotProduct(omega0).GetScalar());
	//omega += matrix1.m_up.Scale(5.0f) - matrix1.m_up.Scale(matrix1.m_up.DotProduct(omega0).GetScalar());
	//m_body0->SetOmega(omega);
}

void ndJointVehicleMotor::SetMaxSpeed(dFloat32 maxSpeed)
{
	dAssert(0);
}

void ndJointVehicleMotor::SetEngineTorque(dFloat32 torque)
{
	dAssert(0);
}

void ndJointVehicleMotor::SetThrottle(dFloat32 param)
{
	dAssert(0);
}


void ndJointVehicleMotor::JacobianDerivative(ndConstraintDescritor& desc)
{
	dMatrix matrix0;
	dMatrix matrix1;
	CalculateGlobalMatrix(matrix0, matrix1);

	// two rows to restrict rotation around around the parent coordinate system
	const dFloat32 angle0 = CalculateAngle(matrix0.m_front, matrix1.m_front, matrix1.m_up);
	AddAngularRowJacobian(desc, matrix1.m_up, angle0);

	const dFloat32 angle1 = CalculateAngle(matrix0.m_front, matrix1.m_front, matrix1.m_right);
	AddAngularRowJacobian(desc, matrix1.m_right, angle1);

	//// save the current joint Omega

	m_throttle = 0.5f;
	AddAngularRowJacobian(desc, matrix1.m_front, dFloat32 (0.0f));

	const dVector& omega0 = m_body0->GetOmega();
	const dVector& omega1 = m_body1->GetOmega();
	ndJacobian& jacobian0 = desc.m_jacobian[desc.m_rowsCount - 1].m_jacobianM0;
	ndJacobian& jacobian1 = desc.m_jacobian[desc.m_rowsCount - 1].m_jacobianM1;
	const dVector relOmega(omega0 * jacobian0.m_angular + omega1 * jacobian1.m_angular);
	m_speed = -relOmega.AddHorizontal().GetScalar();

	dFloat32 relSpeed = dClamp(m_speed - m_throttle * m_maxSpeed, -m_maxSpeed, dFloat32(0.0f));

	SetMotorAcceleration(desc, relSpeed * desc.m_invTimestep);
	SetHighFriction(desc, m_engineTorque);
	SetLowerFriction(desc, -m_engineTorque);
}

