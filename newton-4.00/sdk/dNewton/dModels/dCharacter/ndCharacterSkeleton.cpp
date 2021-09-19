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
#include "ndCharacterSkeleton.h"
#include "ndCharacterRootNode.h"

ndCharacterSkeleton::ndCharacterSkeleton(ndCharacterNode* const node, const dMatrix& matrix, ndCharacterSkeleton* const parent)
	:dNodeHierarchy<ndCharacterSkeleton>()
	,m_transform(matrix)
	,m_node(node)
{
	if (parent)
	{
		Attach(parent);
	}
}

ndCharacterSkeleton* ndCharacterSkeleton::FindNode(ndCharacterNode* const node) const
{
	if (m_node == node)
	{
		return (ndCharacterSkeleton*)this;
	}

	for (dNodeBaseHierarchy* ptr = GetFirst(); ptr && (ptr != this); ptr = ptr->GetNext())
	{
		ndCharacterSkeleton*const bone = (ndCharacterSkeleton*)ptr;
		if (node == bone->m_node)
		{
			return bone;
		}
	}
	return nullptr;
}