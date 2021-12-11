/* Copyright (c) <2003-2021> <Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software./Users/juliojerez/Desktop/newton-dynamics/applications/demosSandbox/sdkDemos/toolBox/PhysicsUtils.cpp
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely
*/

#include "ndSandboxStdafx.h"
#include "ndDemoMesh.h"
#include "ndDemoEntity.h"
#include "ndPhysicsUtils.h"
#include "ndDemoEntityManager.h"
#include "ndOpenGlUtil.h"
#include "ndDebugDisplay.h"
#include "ndPhysicsWorld.h"
#include "ndDemoEntityManager.h"
#include "ndDemoInstanceEntity.h"
#include "ndHighResolutionTimer.h"

ndVector FindFloor(const ndWorld& world, const ndVector& origin, dFloat32 dist)
{
	ndVector p0(origin);
	ndVector p1(origin - ndVector(0.0f, dAbs(dist), 0.0f, 0.0f));

	ndRayCastClosestHitCallback rayCaster;
	if (world.RayCast(rayCaster, p0, p1))
	{
		return rayCaster.m_contact.m_point;
	}
	return p1;
}

ndBodyKinematic* MousePickBody(ndWorld* const world, const ndVector& origin, const ndVector& end, dFloat32& paramterOut, ndVector& positionOut, ndVector& normalOut)
{
	class ndRayPickingCallback: public ndRayCastClosestHitCallback
	{
		public: 
		ndRayPickingCallback()
			:ndRayCastClosestHitCallback()
		{
		}

		dFloat32 OnRayCastAction(const ndContactPoint& contact, dFloat32 intersetParam)
		{
			if (contact.m_body0->GetInvMass() == dFloat32(0.0f)) 
			{
				return 1.2f;
			}
			return ndRayCastClosestHitCallback::OnRayCastAction(contact, intersetParam);
		}
	};

	ndRayPickingCallback rayCaster;
	if (world->RayCast(rayCaster, origin, end))
	{
		paramterOut = rayCaster.m_param;
		positionOut = rayCaster.m_contact.m_point;
		normalOut = rayCaster.m_contact.m_normal;
		return (ndBodyKinematic* )rayCaster.m_contact.m_body0;
	}

	return nullptr;
}

static void AddShape(ndDemoEntityManager* const scene,
	ndDemoInstanceEntity* const rootEntity, const ndShapeInstance& shape,
	dFloat32 mass, const ndVector& origin, const dFloat32 high, dInt32 count)
{
	ndMatrix matrix(dRollMatrix(90.0f * dDegreeToRad));
	matrix.m_posit = origin;
	matrix.m_posit.m_w = 1.0f;

	ndPhysicsWorld* const world = scene->GetWorld();

	ndVector floor(FindFloor(*world, matrix.m_posit + ndVector(0.0f, 100.0f, 0.0f, 0.0f), 200.0f));
	matrix.m_posit.m_y = floor.m_y + high + 7.0f;

	for (dInt32 i = 0; i < count; i++)
	{
		ndBodyDynamic* const body = new ndBodyDynamic();
		ndDemoEntity* const entity = new ndDemoEntity(matrix, rootEntity);

		body->SetNotifyCallback(new ndDemoEntityNotify(scene, entity));
		body->SetMatrix(matrix);
		body->SetCollisionShape(shape);
		body->SetMassMatrix(mass, shape);
		body->SetAngularDamping(ndVector(dFloat32(0.5f)));

		world->AddBody(body);
		matrix.m_posit.m_y += high * 2.5f;
	}
}

void AddPlanks(ndDemoEntityManager* const scene, const ndVector& origin, dFloat32 mass, dInt32 count)
{
	for (dInt32 i = 0; i < count; i++)
	{
		for (dInt32 j = 0; j < count; j++)
		{
			ndVector posit(origin + ndVector((i - 2)* 5.0f, 0.0f, (j - 2) * 5.0f, 0.0f));
			AddBox(scene, posit, mass, 4.0f, 0.25f, 3.0f);
		}
	}
}

void AddCapsulesStacks(ndDemoEntityManager* const scene, const ndVector& origin, dFloat32 mass, dFloat32 radius0, dFloat32 radius1, dFloat32 high, dInt32 rows_x, dInt32 rows_z, dInt32 columHigh)
{
	ndShapeInstance shape(new ndShapeCapsule(radius0, radius1, high));
	ndDemoMeshIntance* const instanceMesh = new ndDemoMeshIntance("shape", scene->GetShaderCache(), &shape, "marble.tga", "marble.tga", "marble.tga");

	ndDemoInstanceEntity* const rootEntity = new ndDemoInstanceEntity(instanceMesh);
	scene->AddEntity(rootEntity);

	dFloat32 spacing = 2.0f;
	for (dInt32 i = 0; i < rows_x; i++)
	{
		for (dInt32 j = 0; j < rows_z; j++)
		{
			ndVector location((j - rows_x / 2) * spacing, 0.0f, (i - rows_z / 2) * spacing, 0.0f);
			AddShape(scene, rootEntity, shape, mass, location + origin, high, columHigh);
		}
	}

	instanceMesh->Release();
}

ndBodyKinematic* CreateBody(ndDemoEntityManager* const scene, const ndShapeInstance& shape, const ndMatrix& origin, dFloat32 mass)
{
	ndPhysicsWorld* const world = scene->GetWorld();

	ndMatrix matrix(origin);
	ndVector floor(FindFloor(*world, matrix.m_posit + ndVector(0.0f, 500.0f, 0.0f, 0.0f), 1000.0f));
	matrix.m_posit.m_y = dMax (floor.m_y + 1.0f, matrix.m_posit.m_y);
	ndDemoMesh* const mesh = new ndDemoMesh("shape", scene->GetShaderCache(), &shape, "marble.tga", "marble.tga", "marble.tga");

	ndBodyDynamic* const body = new ndBodyDynamic();
	ndDemoEntity* const entity = new ndDemoEntity(matrix, nullptr);
	entity->SetMesh(mesh, dGetIdentityMatrix());
	body->SetNotifyCallback(new ndDemoEntityNotify(scene, entity));

	body->SetMatrix(matrix);
	body->SetCollisionShape(shape);
	body->SetMassMatrix(mass, shape);

	world->AddBody(body);
	scene->AddEntity(entity);

	mesh->Release();
	return body;
}

ndBodyKinematic* AddBox(ndDemoEntityManager* const scene, const ndVector& origin, dFloat32 mass, dFloat32 sizex, dFloat32 sizey, dFloat32 sizez)
{
	ndMatrix matrix(dGetIdentityMatrix());
	matrix.m_posit = origin;
	matrix.m_posit.m_w = 1.0f;

	ndShapeInstance shape(new ndShapeBox(sizex, sizey, sizez));
	ndBodyKinematic* const body = CreateBody(scene, shape, matrix, mass);
	return body;
}

ndBodyKinematic* AddSphere(ndDemoEntityManager* const scene, const ndVector& origin, dFloat32 mass, dFloat32 radius)
{
	ndMatrix matrix(dGetIdentityMatrix());
	matrix.m_posit = origin;
	matrix.m_posit.m_w = 1.0f;

	ndShapeInstance shape(new ndShapeSphere(radius));
	ndBodyKinematic* const body = CreateBody(scene, shape, matrix, mass);
	return body;
}

ndBodyKinematic* AddCapsule(ndDemoEntityManager* const scene, const ndVector& origin, dFloat32 mass, dFloat32 radius0, dFloat32 radius1, dFloat32 high)
{
	ndMatrix matrix(dGetIdentityMatrix());
	matrix.m_posit = origin;
	matrix.m_posit.m_w = 1.0f;

	ndShapeInstance shape(new ndShapeCapsule(radius0, radius1, high));
	ndBodyKinematic* const body = CreateBody(scene, shape, matrix, mass);
	return body;
}

ndBodyKinematic* AddConvexHull(ndDemoEntityManager* const scene, const ndVector& origin, dFloat32 mass, dFloat32 radius, dFloat32 high, dInt32 segments)
{
	dInt32 count = 0;
	ndVector points[1024 * 8];
	for (dInt32 i = 0; i < segments; i++)
	{
		dFloat32 angle = dFloat32(2.0f) * dPi * i / segments;
		dFloat32 x = radius * dCos(angle);
		dFloat32 z = radius * dSin(angle);
		points[count++] = ndVector(0.7f * x, -high * 0.5f, 0.7f * z, 0.0f);
		points[count++] = ndVector(0.7f * x,  high * 0.5f, 0.7f * z, 0.0f);
		points[count++] = ndVector(x, -high * 0.25f, z, 0.0f);
		points[count++] = ndVector(x, high * 0.25f, z, 0.0f);
		dAssert(count < dInt32 (sizeof(points) / sizeof(points[0])));
	}

	ndMatrix matrix(dGetIdentityMatrix());
	matrix.m_posit = origin;
	matrix.m_posit.m_w = 1.0f;

	ndShapeInstance shape(new ndShapeConvexHull(count, sizeof(ndVector), 0.0f, &points[0].m_x));
	ndBodyKinematic* const body = CreateBody(scene, shape, matrix, mass);
	return body;
}
