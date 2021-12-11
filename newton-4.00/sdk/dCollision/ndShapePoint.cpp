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

#include "ndCoreStdafx.h"
#include "ndCollisionStdafx.h"
#include "ndContact.h"
#include "ndShapePoint.h"
#include "ndContactSolver.h"

ndShapePoint::ndShapePoint()
	:ndShapeConvex(m_pointCollision)
{
}

ndShapePoint::ndShapePoint(const ndLoadSaveBase::dLoadDescriptor&)
	:ndShapeConvex(m_pointCollision)
{
	dAssert(0);
}

ndShapePoint::~ndShapePoint()
{
}

void ndShapePoint::MassProperties()
{
	dAssert(0);
	//m_centerOfMass = ndVector::m_zero;
	//m_crossInertia = ndVector::m_zero;
	//dFloat32 volume = dFloat32(4.0f * dPi / 3.0f) * m_radius *  m_radius * m_radius;
	//dFloat32 II = dFloat32(2.0f / 5.0f) * m_radius *  m_radius;
	//m_inertia = ndVector(II, II, II, dFloat32(0.0f));
	//m_centerOfMass.m_w = volume;
}

void ndShapePoint::CalculateAabb(const ndMatrix&, ndVector &, ndVector &) const
{
	dAssert(0);
	//ndVector size(m_radius);
	//p0 = (matrix[3] - size) & ndVector::m_triplexMask;
	//p1 = (matrix[3] + size) & ndVector::m_triplexMask;
}

ndVector ndShapePoint::SupportVertexSpecialProjectPoint(const ndVector&, const ndVector&) const
{
	dAssert(0);
	//return dir.Scale(m_radius - D_PENETRATION_TOL);
	return ndVector::m_zero;
}

ndVector ndShapePoint::SupportVertexSpecial(const ndVector&, dFloat32, dInt32* const) const
{
	return ndVector::m_zero;
}

ndVector ndShapePoint::SupportVertex(const ndVector&, dInt32* const) const
{
	dAssert(0);
	//dAssert(dir.m_w == dFloat32(0.0f));
	//dAssert(dAbs(dir.DotProduct(dir).GetScalar() - dFloat32(1.0f)) < dFloat32(1.0e-3f));
	//dAssert(dir.m_w == 0.0f);
	//return dir.Scale(m_radius);
	return ndVector::m_zero;
}

dInt32 ndShapePoint::CalculatePlaneIntersection(const ndVector& normal, const ndVector& point, ndVector* const contactsOut) const
{
	dAssert(normal.m_w == 0.0f);
	dAssert(normal.DotProduct(normal).GetScalar() > dFloat32(0.999f));
	contactsOut[0] = normal * normal.DotProduct(point);
	return 1;
}

dFloat32 ndShapePoint::RayCast(ndRayCastNotify&, const ndVector&, const ndVector&, dFloat32, const ndBody* const, ndContactPoint&) const
{
	dAssert(0);
	return dFloat32 (1.2f);
	//dFloat32 t = dRayCastSphere(localP0, localP1, ndVector::m_zero, m_radius);
	//if (t < maxT) 
	//{
	//	ndVector contact(localP0 + (localP1 - localP0).Scale(t));
	//	dAssert(contact.m_w == dFloat32(0.0f));
	//	//contactOut.m_normal = contact.Scale (dgRsqrt (contact.DotProduct(contact).GetScalar()));
	//	contactOut.m_normal = contact.Normalize();
	//	//contactOut.m_userId = SetUserDataID();
	//}
	//return t;
}

ndShapeInfo ndShapePoint::GetShapeInfo() const
{
	ndShapeInfo info(ndShapeConvex::GetShapeInfo());
	info.m_point.m_noUsed = dFloat32 (0.0f);
	return info;
}

void ndShapePoint::DebugShape(const ndMatrix&, ndShapeDebugNotify&) const
{
	dAssert(0);
	//ndVector tmpVectex[1024 * 2];
	//
	//ndVector p0(dFloat32(1.0f), dFloat32(0.0f), dFloat32(0.0f), dFloat32(0.0f));
	//ndVector p1(-dFloat32(1.0f), dFloat32(0.0f), dFloat32(0.0f), dFloat32(0.0f));
	//ndVector p2(dFloat32(0.0f), dFloat32(1.0f), dFloat32(0.0f), dFloat32(0.0f));
	//ndVector p3(dFloat32(0.0f), -dFloat32(1.0f), dFloat32(0.0f), dFloat32(0.0f));
	//ndVector p4(dFloat32(0.0f), dFloat32(0.0f), dFloat32(1.0f), dFloat32(0.0f));
	//ndVector p5(dFloat32(0.0f), dFloat32(0.0f), -dFloat32(1.0f), dFloat32(0.0f));
	//
	//dInt32 index = 3;
	//dInt32 count = 0;
	//TesselateTriangle(index, p4, p0, p2, count, tmpVectex);
	//TesselateTriangle(index, p4, p2, p1, count, tmpVectex);
	//TesselateTriangle(index, p4, p1, p3, count, tmpVectex);
	//TesselateTriangle(index, p4, p3, p0, count, tmpVectex);
	//TesselateTriangle(index, p5, p2, p0, count, tmpVectex);
	//TesselateTriangle(index, p5, p1, p2, count, tmpVectex);
	//TesselateTriangle(index, p5, p3, p1, count, tmpVectex);
	//TesselateTriangle(index, p5, p0, p3, count, tmpVectex);
	//
	//for (dInt32 i = 0; i < count; i++) 
	//{
	//	tmpVectex[i] = matrix.TransformVector(tmpVectex[i].Scale(m_radius)) & ndVector::m_triplexMask;
	//}
	//
	//for (dInt32 i = 0; i < count; i += 3) 
	//{
	//	debugCallback.DrawPolygon(3, &tmpVectex[i]);
	//}
}

void ndShapePoint::Save(const ndLoadSaveBase::ndSaveDescriptor&) const
{
	dAssert(0);
}