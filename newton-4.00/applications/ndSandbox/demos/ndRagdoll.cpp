/* Copyright (c) <2003-2022> <Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely
*/

#include "ndSandboxStdafx.h"
#include "ndSkyBox.h"
#include "ndDemoMesh.h"
#include "ndDemoCamera.h"
#include "ndLoadFbxMesh.h"
#include "ndPhysicsUtils.h"
#include "ndPhysicsWorld.h"
#include "ndTargaToOpenGl.h"
#include "ndMakeStaticMap.h"
#include "ndDemoEntityManager.h"
#include "ndDemoInstanceEntity.h"

#include "ndAnimationPose.h"
#include "ndAnimationSequence.h"
#include "ndBasicPlayerCapsule.h"
#include "ndAnimationKeyframesTrack.h"
#include "ndAnimationSequencePlayer.h"


class ndRagdollDefinition
{
	public:
	enum dLimbType
	{
		m_root,
		m_hinge,
		m_spherical,
		m_doubleHinge,
		m_effector
	};

	struct ndDampData
	{
		ndDampData()
			:m_spring(0.0f)
			,m_damper(0.25f)
			,m_regularizer(0.1f)
		{
		}

		ndDampData(ndFloat32 spring, ndFloat32 damper, ndFloat32 regularizer)
			:m_spring(spring)
			,m_damper(damper)
			,m_regularizer(regularizer)
		{
		}

		ndFloat32 m_spring;
		ndFloat32 m_damper;
		ndFloat32 m_regularizer;
	};

	struct ndJointLimits
	{
		ndFloat32 m_minTwistAngle;
		ndFloat32 m_maxTwistAngle;
		ndFloat32 m_coneAngle;
	};

	struct ndOffsetFrameMatrix
	{
		ndFloat32 m_pitch;
		ndFloat32 m_yaw;
		ndFloat32 m_roll;
	};

	char m_boneName[32];
	dLimbType m_limbType;
	ndFloat32 m_massWeight;
	ndJointLimits m_jointLimits;
	ndOffsetFrameMatrix m_frameBasics;
	ndDampData m_coneSpringData;
	ndDampData m_twistSpringData;
};

static ndRagdollDefinition jointsDefinition[] =
{
	{ "root", ndRagdollDefinition::m_root, 1.0f, {}, {} },
	{ "lowerback", ndRagdollDefinition::m_spherical, 1.0f, { -15.0f, 15.0f, 30.0f }, { 0.0f, 0.0f, 0.0f } },
	{ "upperback", ndRagdollDefinition::m_spherical, 1.0f,{ -15.0f, 15.0f, 30.0f },{ 0.0f, 0.0f, 0.0f } },
	{ "lowerneck", ndRagdollDefinition::m_spherical, 1.0f,{ -15.0f, 15.0f, 30.0f },{ 0.0f, 0.0f, 0.0f } },
	{ "upperneck", ndRagdollDefinition::m_spherical, 1.0f,{ -60.0f, 60.0f, 30.0f },{ 0.0f, 0.0f, 0.0f } },
	
	{ "lclavicle", ndRagdollDefinition::m_spherical, 1.0f, { -60.0f, 60.0f, 80.0f }, { 0.0f, -60.0f, 0.0f } },
	{ "lhumerus", ndRagdollDefinition::m_hinge, 1.0f, { 0.0f, 120.0f, 0.0f }, { 0.0f, 90.0f, 0.0f } },
	{ "lradius", ndRagdollDefinition::m_doubleHinge, 1.0f, { 0.0f, 0.0f, 60.0f }, { 90.0f, 0.0f, 90.0f } },

	{ "rclavicle", ndRagdollDefinition::m_spherical, 1.0f, { -60.0f, 60.0f, 80.0f }, { 0.0f, 60.0f, 0.0f } },
	{ "rhumerus", ndRagdollDefinition::m_hinge, 1.0f, { 0.0f, 120.0f, 0.0f }, { 0.0f, 90.0f, 0.0f } },
	{ "rradius", ndRagdollDefinition::m_doubleHinge, 1.0f, { 0.0f, 0.0f, 60.0f }, { 90.0f, 0.0f, 90.0f } },

	{ "rhipjoint", ndRagdollDefinition::m_spherical, 1.0f, { -45.0f, 45.0f, 80.0f }, { 0.0f, -60.0f, 0.0f } },
	{ "rfemur", ndRagdollDefinition::m_hinge, 1.0f, { 0.0f, 120.0f, 0.0f }, { 0.0f, 90.0f, 0.0f } },
	{ "rtibia", ndRagdollDefinition::m_doubleHinge, 1.0f, { 0.0f, 0.0f, 60.0f }, { 90.0f, 0.0f, 90.0f } },
		
	{ "lhipjoint", ndRagdollDefinition::m_spherical, 1.0f,{ -45.0f, 45.0f, 80.0f }, { 0.0f, 60.0f, 0.0f } },
	{ "lfemur", ndRagdollDefinition::m_hinge, 1.0f, { 0.0f, 120.0f, 0.0f }, { 0.0f, 90.0f, 0.0f } },
	{ "ltibia", ndRagdollDefinition::m_doubleHinge, 1.0f, { 0.0f, 0.0f, 60.0f }, { 90.0f, 0.0f, 90.0f } },
	
	{ "", ndRagdollDefinition::m_root,{},{} },
};

class ndActiveRagdollEntityNotify : public ndDemoEntityNotify
{
	public:
	ndActiveRagdollEntityNotify(ndDemoEntityManager* const manager, ndDemoEntity* const entity, ndBodyDynamic* const parentBody)
		:ndDemoEntityNotify(manager, entity, parentBody)
		,m_bindMatrix(ndGetIdentityMatrix())
	{
		if (parentBody)
		{
			ndDemoEntity* const parentEntity = (ndDemoEntity*)(parentBody->GetNotifyCallback()->GetUserData());
			m_bindMatrix = entity->GetParent()->CalculateGlobalMatrix(parentEntity).Inverse();
		}
	}

	void OnTransform(ndInt32, const ndMatrix& matrix)
	{
		if (!m_parentBody)
		{
			const ndMatrix localMatrix(matrix * m_bindMatrix);
			const ndQuaternion rot(localMatrix);
			m_entity->SetMatrix(rot, localMatrix.m_posit);
		}
		else
		{
			const ndMatrix parentMatrix(m_parentBody->GetMatrix());
			const ndMatrix localMatrix(matrix * parentMatrix.Inverse() * m_bindMatrix);
			const ndQuaternion rot(localMatrix);
			m_entity->SetMatrix(rot, localMatrix.m_posit);
		}
	}

	void OnApplyExternalForce(ndInt32 thread, ndFloat32 timestep)
	{
		ndDemoEntityNotify::OnApplyExternalForce(thread, timestep);
		// remember to check and clamp huge angular velocities
	}

	ndMatrix m_bindMatrix;
};


class ndRagdollModel : public ndModel
{
	public:
	ndRagdollModel(ndDemoEntityManager* const scene, ndDemoEntity* const ragdollMesh, const ndMatrix& location)
		:ndModel()
		,m_rootBody(nullptr)
	{
		ndWorld* const world = scene->GetWorld();

		// make a clone of the mesh and add it to the scene
		ndDemoEntity* const entity = (ndDemoEntity*)ragdollMesh->CreateClone();
		scene->AddEntity(entity);

		ndDemoEntity* const rootEntity = (ndDemoEntity*)entity->Find(jointsDefinition[0].m_boneName);
		ndMatrix matrix(rootEntity->CalculateGlobalMatrix() * location);

		// find the floor location 
		ndVector floor(FindFloor(*world, matrix.m_posit + ndVector(0.0f, 100.0f, 0.0f, 0.0f), 200.0f));
		matrix.m_posit.m_y = floor.m_y + 1.5f;

		// add the root body
		ndBodyDynamic* const rootBody = CreateBodyPart(scene, rootEntity, nullptr);

		// set bindimg matrix;
		ndActiveRagdollEntityNotify* const notify = (ndActiveRagdollEntityNotify*)rootBody->GetNotifyCallback();
		notify->m_bindMatrix = rootEntity->GetParent()->CalculateGlobalMatrix().Inverse();

		notify->OnTransform(0, matrix);
		
		ndInt32 stack = 0;
		ndFixSizeArray<ndFloat32, 64> massWeight;
		ndFixSizeArray<ndBodyDynamic*, 64> bodies;
		ndFixSizeArray<ndBodyDynamic*, 32> parentBones;
		ndFixSizeArray<ndDemoEntity*, 32> childEntities;

		m_rootBody = rootBody;
		parentBones.SetCount(32);
		childEntities.SetCount(32);
		
		for (ndDemoEntity* child = rootEntity->GetChild(); child; child = child->GetSibling())
		{
			childEntities[stack] = child;
			parentBones[stack] = rootBody;
			stack++;
		}

		bodies.PushBack(m_rootBody);
		massWeight.PushBack(jointsDefinition[0].m_massWeight);

		while (stack) 
		{
			stack--;
			ndBodyDynamic* parentBone = parentBones[stack];
			ndDemoEntity* const childEntity = childEntities[stack];
			const char* const name = childEntity->GetName().GetStr();
			//ndTrace(("name: %s\n", name));
			for (ndInt32 i = 0; jointsDefinition[i].m_boneName[0]; ++i)
			{
				const ndRagdollDefinition& definition = jointsDefinition[i];
				if (!strcmp(definition.m_boneName, name))
				{
					ndBodyDynamic* const childBody = CreateBodyPart(scene, childEntity, parentBone);
					bodies.PushBack(childBody);
					massWeight.PushBack(jointsDefinition[i].m_massWeight);

					//connect this body part to its parentBody with a ragdoll joint
					ndJointBilateralConstraint* const joint = ConnectBodyParts(childBody, parentBone, definition);
					world->AddJoint(joint);

					parentBone = childBody;
					break;
				}
			}
		
			for (ndDemoEntity* child = childEntity->GetChild(); child; child = child->GetSibling())
			{
				childEntities[stack] = child;
				parentBones[stack] = parentBone;
				stack++;
			}
		}
		
		NormalizeMassDistribution(100.0f, bodies, massWeight);
	}

	~ndRagdollModel()
	{
	}

	void NormalizeMassDistribution(ndFloat32 mass, const ndFixSizeArray<ndBodyDynamic*, 64>& bodyArray, const ndFixSizeArray<ndFloat32, 64>& massWeight) const
	{
		ndFloat32 volume = 0.0f;
		for (ndInt32 i = 0; i < bodyArray.GetCount(); ++i)
		{
			volume += bodyArray[i]->GetCollisionShape().GetVolume() * massWeight[i];
		}
		ndFloat32 density = mass / volume;

		for (ndInt32 i = 0; i < bodyArray.GetCount(); ++i)
		{
			ndBodyDynamic* const body = bodyArray[i];
			ndFloat32 scale = density * body->GetCollisionShape().GetVolume() * massWeight[i];
			ndVector inertia(body->GetMassMatrix().Scale (scale));
			body->SetMassMatrix(inertia);
		}
	}
	
	ndBodyDynamic* CreateBodyPart(ndDemoEntityManager* const scene, ndDemoEntity* const entityPart, ndBodyDynamic* const parentBone)
	{
		ndShapeInstance* const shape = entityPart->CreateCollisionFromChildren();
		ndAssert(shape);

		// create the rigid body that will make this body
		ndMatrix matrix(entityPart->CalculateGlobalMatrix());

		ndBodyDynamic* const body = new ndBodyDynamic();
		body->SetMatrix(matrix);
		body->SetCollisionShape(*shape);
		body->SetMassMatrix(1.0f, *shape);
		body->SetNotifyCallback(new ndActiveRagdollEntityNotify(scene, entityPart, parentBone));

		scene->GetWorld()->AddBody(body);
		delete shape;
		return body;
	}

	ndJointBilateralConstraint* ConnectBodyParts(ndBodyDynamic* const childBody, ndBodyDynamic* const parentBody, const ndRagdollDefinition& definition)
	{
		ndMatrix matrix(childBody->GetMatrix());
		ndRagdollDefinition::ndOffsetFrameMatrix frameAngle(definition.m_frameBasics);
		ndMatrix pinAndPivotInGlobalSpace(ndPitchMatrix(frameAngle.m_pitch * ndDegreeToRad) * ndYawMatrix(frameAngle.m_yaw * ndDegreeToRad) * ndRollMatrix(frameAngle.m_roll * ndDegreeToRad) * matrix);

		ndRagdollDefinition::ndJointLimits jointLimits(definition.m_jointLimits);

		switch (definition.m_limbType)
		{
			case ndRagdollDefinition::m_spherical:
			{
				ndJointSpherical* const joint = new ndJointSpherical(pinAndPivotInGlobalSpace, childBody, parentBody);
				joint->SetConeLimit(jointLimits.m_coneAngle * ndDegreeToRad);
				joint->SetTwistLimits(jointLimits.m_minTwistAngle * ndDegreeToRad, jointLimits.m_maxTwistAngle * ndDegreeToRad);
				joint->SetAsSpringDamper(definition.m_coneSpringData.m_regularizer, definition.m_coneSpringData.m_spring, definition.m_coneSpringData.m_damper);
				return joint;
			}

			case ndRagdollDefinition::m_hinge:
			{
				ndJointHinge* const joint = new ndJointHinge(pinAndPivotInGlobalSpace, childBody, parentBody);
				joint->SetLimitState(true);
				joint->SetLimits(jointLimits.m_minTwistAngle * ndDegreeToRad, jointLimits.m_maxTwistAngle * ndDegreeToRad);
				joint->SetAsSpringDamper(definition.m_coneSpringData.m_regularizer, definition.m_coneSpringData.m_spring, definition.m_coneSpringData.m_damper);
				return joint;
			}
			
			case ndRagdollDefinition::m_doubleHinge:
			{
				ndJointDoubleHinge* const joint = new ndJointDoubleHinge(pinAndPivotInGlobalSpace, childBody, parentBody);
				joint->SetLimits0(-30.0f * ndDegreeToRad, 30.0f * ndDegreeToRad);
				joint->SetLimits1(-45.0f * ndDegreeToRad, 45.0f * ndDegreeToRad);
				joint->SetAsSpringDamper0(definition.m_coneSpringData.m_regularizer, definition.m_coneSpringData.m_spring, definition.m_coneSpringData.m_damper);
				joint->SetAsSpringDamper1(definition.m_coneSpringData.m_regularizer, definition.m_coneSpringData.m_spring, definition.m_coneSpringData.m_damper);
				return joint;
			}

			default:
				ndAssert(0);
		}
		return nullptr;
	}

	//void Update(ndWorld* const world, ndFloat32 timestep) 
	void Update(ndWorld* const, ndFloat32)
	{
	}
	
	ndBodyDynamic* m_rootBody;
};

void ndRagdoll (ndDemoEntityManager* const scene)
{
	// build a floor
	BuildFloorBox(scene, ndGetIdentityMatrix());

	ndVector origin1(0.0f, 0.0f, 0.0f, 1.0f);
	fbxDemoEntity* const ragdollMesh = scene->LoadFbxMesh("walker.fbx");

	ndMatrix matrix(ndGetIdentityMatrix());
	matrix.m_posit.m_y = 0.5f;
	ndMatrix playerMatrix(matrix);
	ndRagdollModel* const ragdoll = new ndRagdollModel(scene, ragdollMesh, matrix);
	scene->SetSelectedModel(ragdoll);
	scene->GetWorld()->AddModel(ragdoll);
	//scene->GetWorld()->AddJoint(new ndJointFix6dof(ragdoll->m_rootBody->GetMatrix(), ragdoll->m_rootBody, scene->GetWorld()->GetSentinelBody()));

	matrix.m_posit.m_x += 1.4f;
	//TestPlayerCapsuleInteraction(scene, matrix);

	matrix.m_posit.m_x += 2.0f;
	matrix.m_posit.m_y += 2.0f;
	//ndBodyKinematic* const reckingBall = AddSphere(scene, matrix.m_posit, 25.0f, 0.25f);
	//reckingBall->SetVelocity(ndVector(-5.0f, 0.0f, 0.0f, 0.0f));

	matrix.m_posit.m_x += 2.0f;
	matrix.m_posit.m_z -= 2.0f;
	//scene->GetWorld()->AddModel(new ndRagdollModel(scene, ragdollMesh, matrix));

	matrix.m_posit.m_z = 2.0f;
	//scene->GetWorld()->AddModel(new ndRagdollModel(scene, ragdollMesh, matrix));
	delete ragdollMesh;

	origin1.m_x += 20.0f;
	//AddCapsulesStacks(scene, origin1, 10.0f, 0.25f, 0.25f, 0.5f, 10, 10, 7);

	ndFloat32 angle = ndFloat32(90.0f * ndDegreeToRad);
	playerMatrix = ndYawMatrix(angle) * playerMatrix;
	ndVector origin(playerMatrix.m_posit + playerMatrix.m_front.Scale (-5.0f));
	origin.m_y += 1.0f;
	origin.m_z -= 2.0f;
	scene->SetCameraMatrix(playerMatrix, origin);

	//ndLoadSave loadScene;
	//loadScene.SaveModel("xxxxxx", ragdoll);
}