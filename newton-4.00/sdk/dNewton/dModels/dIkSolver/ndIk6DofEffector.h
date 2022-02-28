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

#ifndef __ND_IK_6DOF_EFFECTOR_H__
#define __ND_IK_6DOF_EFFECTOR_H__

#include "ndNewtonStdafx.h"
#include "ndJointBilateralConstraint.h"

class ndIk6DofEffector: public ndJointBilateralConstraint
{
	public:
	D_CLASS_REFLECTION(ndIk6DofEffector);
	D_NEWTON_API ndIk6DofEffector(const ndLoadSaveBase::ndLoadDescriptor& desc);
	D_NEWTON_API ndIk6DofEffector(const ndMatrix& globalPinAndPivot, ndBodyKinematic* const child, ndBodyKinematic* const parent);
	D_NEWTON_API virtual ~ndIk6DofEffector();

	//D_NEWTON_API ndMatrix GetReferenceMatrix() const;
	//D_NEWTON_API void SetTargetMatrix(const ndMatrix& localMatrix);
	//D_NEWTON_API bool IsLinearMode() const;
	//D_NEWTON_API bool IsAngularMode() const;
	//D_NEWTON_API void SetMode(bool linear, bool angular);

	D_NEWTON_API void EnableAxisX(bool state);
	D_NEWTON_API void EnableAxisY(bool state);
	D_NEWTON_API void EnableAxisZ(bool state);

	D_NEWTON_API ndMatrix GetOffsetMatrix() const;
	D_NEWTON_API void SetOffsetMatrix(const ndMatrix& matrix);
	
	//D_NEWTON_API void SetLinearSpringDamper(ndFloat32 regularizer, ndFloat32 springConst, ndFloat32 damperConst);
	//D_NEWTON_API void GetLinearSpringDamper(ndFloat32& regularizer, ndFloat32& springConst, ndFloat32& damperConst) const;
	//D_NEWTON_API void SetAngularSpringDamper(ndFloat32 regularizer, ndFloat32 springConst, ndFloat32 damperConst);
	//D_NEWTON_API void GetAngularSpringDamper(ndFloat32& regularizer, ndFloat32& springConst, ndFloat32& damperConst) const;

	protected:
	D_NEWTON_API void JacobianDerivative(ndConstraintDescritor& desc);
	D_NEWTON_API void Save(const ndLoadSaveBase::ndSaveDescriptor& desc) const;
	D_NEWTON_API void DebugJoint(ndConstraintDebugCallback& debugCallback) const;

	D_NEWTON_API void SubmitLinearAxis(const ndMatrix& matrix0, const ndMatrix& matrix1, ndConstraintDescritor& desc);
	D_NEWTON_API void SubmitAngularAxis(const ndMatrix& matrix0, const ndMatrix& matrix1, ndConstraintDescritor& desc);
	D_NEWTON_API void SubmitAngularAxisCartesianApproximation(const ndMatrix& matrix0, const ndMatrix& matrix1, ndConstraintDescritor& desc);
	
	ndMatrix m_targetFrame;
	//ndFloat32 m_angle;
	//ndFloat32 m_minAngle;
	//ndFloat32 m_maxAngle;
	//ndFloat32 m_angularSpring;
	//ndFloat32 m_angularDamper;
	//ndFloat32 m_angularRegularizer;
	
	ndFloat32 m_linearSpring;
	ndFloat32 m_linearDamper;
	ndFloat32 m_linearRegularizer;
	//bool m_linearMode;
	//bool m_angularMode;
	ndInt8 m_linearAxis;
};


#endif 

