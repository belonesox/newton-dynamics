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

#include "dCoreStdafx.h"
#include "ndNewtonStdafx.h"
#include "ndWorld.h"
#include "ndCharacter.h"
#include "ndBodyDynamic.h"
#include "ndCharacterLimbNode.h"
#include "ndCharacterRootNode.h"
#include "ndCharacterEffectorNode.h"
#include "ndCharacterFowardDynamicNode.h"
#include "ndCharacterInverseDynamicNode.h"

ndCharacter::ndCharacter()
	:ndModel()
	,m_rootNode(nullptr)
{
}

ndCharacter::ndCharacter(const nd::TiXmlNode* const xmlNode)
	:ndModel(xmlNode)
{
}

ndCharacter::~ndCharacter()
{
	if (m_rootNode)
	{
		delete m_rootNode;
	}
}

ndCharacterRootNode* ndCharacter::CreateRoot(ndBodyDynamic* const body)
{
	m_rootNode = new ndCharacterRootNode(this, body);
	return m_rootNode;
}

ndCharacterFowardDynamicNode* ndCharacter::CreateFowardDynamicLimb(const dMatrix& matrixInGlobalScape, ndBodyDynamic* const body, ndCharacterLimbNode* const parent)
{
	ndCharacterFowardDynamicNode* const limb = new ndCharacterFowardDynamicNode(matrixInGlobalScape, body, parent);
	return limb;
}

ndCharacterInverseDynamicNode* ndCharacter::CreateInverseDynamicLimb(const dMatrix& matrixInGlobalScape, ndBodyDynamic* const body, ndCharacterLimbNode* const parent)
{
	ndCharacterInverseDynamicNode* const limb = new ndCharacterInverseDynamicNode(matrixInGlobalScape, body, parent);
	return limb;
}

ndCharacterEffectorNode* ndCharacter::CreateInverseDynamicEffector(const dMatrix& matrixInGlobalScape, ndCharacterLimbNode* const child, ndCharacterLimbNode* const referenceNode)
{
	ndCharacterEffectorNode* const effector = new ndCharacterEffectorNode(matrixInGlobalScape, child, referenceNode);
	return effector;
}

ndCharacter::ndLinearState ndCharacter::CalculateCom() const
{
	dInt32 stack = 1;
	ndCharacterLimbNode* nodePool[32];
	
	nodePool[0] = m_rootNode;

	dFloat32 mass = dFloat32(0.0f);
	dVector com(dVector::m_zero);
	dVector veloc(dVector::m_zero);

	while (stack)
	{
		stack--;
		ndCharacterLimbNode* const node = nodePool[stack];
		ndBodyDynamic* const body = node->GetBody();
		if (body)
		{
			dFloat32 partMass = body->GetMassMatrix().m_w;
			mass += partMass;
			dMatrix bodyMatrix(body->GetMatrix());
			com += bodyMatrix.TransformVector(body->GetCentreOfMass()).Scale (partMass);
			veloc += body->GetVelocity().Scale(partMass);
		}

		for (ndCharacterLimbNode* child = node->GetChild(); child; child = child->GetSibling())
		{
			nodePool[stack] = child;
			stack++;
		}
	}

	
	com = com.Scale(dFloat32(1.0f) / mass);
	veloc = veloc.Scale(dFloat32(1.0f) / mass);
	com.m_w = dFloat32 (1.0f);
	veloc.m_w = dFloat32(0.0f);

	ndLinearState state;
	state.m_mass = mass;
	state.m_centerOfMass = com;
	state.m_centerOfMassVeloc = veloc;
	return state;
}

void ndCharacter::Debug(ndConstraintDebugCallback& context) const
{
	ndBodyDynamic* const hip = m_rootNode->GetBody();
	dMatrix matrix(hip->GetMatrix());
	
	// show character center of mass.
	dFloat32 scale = context.GetScale();
	context.SetScale(scale * 0.5f);

	ndLinearState state(CalculateCom());
	matrix.m_posit = state.m_centerOfMass;
	context.DrawFrame(matrix);

	context.SetScale(scale);
}

void ndCharacter::PostUpdate(ndWorld* const, dFloat32)
{
}

void ndCharacter::Update(ndWorld* const, dFloat32)
{
}