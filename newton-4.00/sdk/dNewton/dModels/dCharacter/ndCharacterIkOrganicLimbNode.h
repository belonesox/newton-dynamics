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

#ifndef __D_CHARACTER_IK_ORGANIC_LIMB_NODE_H__
#define __D_CHARACTER_IK_ORGANIC_LIMB_NODE_H__

#include "ndNewtonStdafx.h"
#include "ndCharacterIkNode.h"
#include "ndJointBallAndSocketActuator.h"

class ndCharacterIkOrganicLimbNode: public ndCharacterIkNode, public ndJointBallAndSocketActuator
{
	public:
	D_CLASS_RELECTION(ndCharacterIkOrganicLimbNode);

	D_NEWTON_API ndCharacterIkOrganicLimbNode(ndBodyDynamic* const body, ndCharacterIkNode* const parent);
	D_NEWTON_API virtual ~ndCharacterIkOrganicLimbNode ();

	virtual ndBodyDynamic* GetBody() const;

	protected:
	ndBodyDynamic* m_body;
};

inline ndBodyDynamic* ndCharacterIkOrganicLimbNode::GetBody() const
{
	return m_body;
}

#endif