/* Copyright (c) <2003-2021> <Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely
*/

#ifndef __D_MULTIBODY_VEHICLE_TIRE_JOINT_H__
#define __D_MULTIBODY_VEHICLE_TIRE_JOINT_H__

#include "ndNewtonStdafx.h"
#include "ndJointWheel.h"


class ndMultiBodyVehicleTireJoint: public ndJointWheel
{
	public:
	D_CLASS_RELECTION(ndMultiBodyVehicleTireJoint);
	D_NEWTON_API ndMultiBodyVehicleTireJoint(const dMatrix& pinAndPivotFrame, ndBodyKinematic* const child, ndBodyKinematic* const parent, const ndWheelDescriptor& desc);
	D_NEWTON_API virtual ~ndMultiBodyVehicleTireJoint();

	protected:
	D_NEWTON_API void JacobianDerivative(ndConstraintDescritor& desc);
};

#endif 
