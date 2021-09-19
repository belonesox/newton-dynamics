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

#ifndef __D_CHARACTER_POSE_H__
#define __D_CHARACTER_POSE_H__

#include "ndNewtonStdafx.h"

class ndCharacterRootNode;
class ndCharacterNode;

//class ndCharaterKeyFramePose
//{
//	public:
//	ndCharaterKeyFramePose();
//	ndCharaterKeyFramePose(ndCharacterNode* const node, const dMatrix& matrix);
//
//	dVector m_position;
//	//dQuaternion m_rotation;
//	dVector m_euler;
//};
//
//inline ndCharaterKeyFramePose::ndCharaterKeyFramePose()
//	:m_position(dVector::m_wOne)
//	,m_euler(dVector::m_zero)
//	,m_node(nullptr)
//{
//}

//inline ndCharaterKeyFramePose::ndCharaterKeyFramePose(ndCharacterNode* const node, const dMatrix& matrix)
//	:m_position(matrix.m_posit)
//	,m_euler(dVector::m_zero)
//	,m_node(node)
//{
//	dAssert(0);
//}

class ndCharacterSkeleton: public dNodeHierarchy<ndCharacterSkeleton>
{
	public: 
	D_NEWTON_API ndCharacterSkeleton* FindNode(ndCharacterNode* const node) const;

	const dMatrix& GetTransform() const;

	protected:
	ndCharacterSkeleton(ndCharacterNode* const node, const dMatrix& matrix, ndCharacterSkeleton* const parent);

	ndCharacterSkeleton* CreateClone() const
	{
		dAssert(0);
		return nullptr;
	}

	dMatrix m_transform;
	ndCharacterNode* m_node;

	friend class ndCharacter;
};

inline const dMatrix& ndCharacterSkeleton::GetTransform() const
{
	return m_transform;
}

#endif