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

#include "ndSandboxStdafx.h"
#include "ndSkyBox.h"
#include "ndTargaToOpenGl.h"
#include "ndDemoMesh.h"
#include "ndDemoCamera.h"
#include "ndLoadFbxMesh.h"
#include "ndPhysicsUtils.h"
#include "ndPhysicsWorld.h"
#include "ndMakeStaticMap.h"
#include "ndAnimationPose.h"
#include "ndDemoEntityManager.h"
#include "ndDemoInstanceEntity.h"
#include "ndAnimationSequenceBase.h"
#include "ndAnimationSequencePlayer.h"

#define D_USE_FORWARD_DYNAMICS

class ndAiQuadrupedTest_2 : public ndModel
{
	public:
	D_CLASS_REFLECTION(ndAiQuadrupedTest_2);

	class ndParamMapper
	{
		public:
		ndParamMapper()
			:m_x0(0.0f)
			,m_scale(0.0f)
		{
		}

		ndParamMapper(ndFloat32 x0, ndFloat32 x1)
			:m_x0(x0 + (x1 - x0) * 0.5f)
			,m_scale((x1 - x0) * 0.5f)
		{
		}

		ndFloat32 Interpolate(const ndFloat32 t)
		{
			return m_x0 + m_scale * t;
		}

		ndFloat32 m_x0;
		ndFloat32 m_scale;
	};

	class ndEffectorInfo
	{
		public:
		ndEffectorInfo()
			:m_basePosition(ndVector::m_wOne)
			,m_effector(nullptr)
			,m_lookAtJoint(nullptr)
			,m_swivel(0.0f)
			,m_x(0.0f)
			,m_y(0.0f)
			,m_z(0.0f)
		{
		}

		ndEffectorInfo(ndIkSwivelPositionEffector* const effector, ndJointHinge* const lookActJoint)
			:m_basePosition(effector->GetPosition())
			,m_effector(effector)
			,m_lookAtJoint(lookActJoint)
			,m_swivel(0.0f)
			,m_x(0.0f)
			,m_y(0.0f)
			,m_z(0.0f)
		{
		}

		ndVector m_basePosition;
		ndIkSwivelPositionEffector* m_effector;
		ndJointHinge* m_lookAtJoint;
		ndReal m_swivel;
		ndReal m_x;
		ndReal m_y;
		ndReal m_z;
		ndParamMapper m_x_mapper;
		ndParamMapper m_y_mapper;
		ndParamMapper m_z_mapper;
		ndParamMapper m_swivel_mapper;
	};
	
	ndAiQuadrupedTest_2(ndDemoEntityManager* const scene, const ndMatrix& location)
		:ndModel()
	{
		ndFloat32 mass = 10.0f;
		ndFloat32 radius = 0.25f;
		ndFloat32 limbMass = 0.5f;
		ndFloat32 limbLength = 0.3f;
		ndFloat32 limbRadios = 0.06f;

		ndPhysicsWorld* const world = scene->GetWorld();
		ndBodyKinematic* const torso = AddSphere(scene, location, mass, radius, "smilli.tga");
		m_rootBody = torso->GetAsBodyDynamic();

		ndDemoEntity* const entity = (ndDemoEntity*) torso->GetNotifyCallback()->GetUserData();
		entity->SetMeshMatrix(dYawMatrix(90.0f * ndDegreeToRad) * dPitchMatrix(90.0f * ndDegreeToRad));

		ndMatrix matrix(dRollMatrix(45.0f * ndDegreeToRad));
		matrix.m_posit.m_x = radius * 0.9f;
		matrix.m_posit.m_y = -radius * 0.5f;

		ndFloat32 angles[] = { 300.0f, 240.0f, 120.0f, 60.0f };
		//ndFloat32 angles[] = { 270.0f, 90.0f, 120.0f, 60.0f };

		const ndVector upDir(location.m_up);
		for (ndInt32 i = 0; i < 4; ++i)
		{
			ndMatrix limbPivotLocation(matrix * dYawMatrix(angles[i] * ndDegreeToRad));
			limbPivotLocation.m_posit += torso->GetMatrix().m_posit;
			limbPivotLocation.m_posit.m_w = 1.0f;

			// add leg thigh
			const ndVector thighPivot(limbPivotLocation.m_posit);

			ndFloat32 workSpace = 0.0f;
			ndBodyKinematic* thigh = nullptr;
			{
				ndMatrix bodyMatrix(limbPivotLocation);
				bodyMatrix.m_posit += limbPivotLocation.m_front.Scale(limbLength * 0.5f);
				thigh = AddCapsule(scene, bodyMatrix, limbMass, limbRadios, limbRadios, limbLength);
				thigh->SetMatrix(bodyMatrix);
				ndIkJointSpherical* const ball = new ndIkJointSpherical(limbPivotLocation, thigh, torso);
				world->AddJoint(ball);

				limbPivotLocation.m_posit += limbPivotLocation.m_front.Scale(limbLength);

				workSpace += limbLength;
			}

			// add calf0
			ndBodyKinematic* calf0 = nullptr;
			{
				limbPivotLocation = dRollMatrix(-90.0f * ndDegreeToRad) * limbPivotLocation;

				ndMatrix bodyMatrix(limbPivotLocation);
				bodyMatrix.m_posit += limbPivotLocation.m_front.Scale(limbLength * 0.5f);
				calf0 = AddCapsule(scene, bodyMatrix, limbMass, limbRadios, limbRadios, limbLength);
				calf0->SetMatrix(bodyMatrix);
				
				ndMatrix caffPinAndPivotFrame(dGetIdentityMatrix());
				ndFloat32 sign = angles[i] > 180.0f ? -1.0f : 1.0f;
				caffPinAndPivotFrame.m_front = limbPivotLocation.m_right.Scale (sign);
				caffPinAndPivotFrame.m_up = limbPivotLocation.m_front;
				caffPinAndPivotFrame.m_right = caffPinAndPivotFrame.m_front.CrossProduct(caffPinAndPivotFrame.m_up);
				caffPinAndPivotFrame.m_posit = limbPivotLocation.m_posit;
				ndIkJointHinge* const hinge = new ndIkJointHinge(caffPinAndPivotFrame, calf0, thigh);

				// add joint limit to prevent knee from flipping
				hinge->SetLimitState(true);
				hinge->SetLimits(-70.0f * ndDegreeToRad, 70.0f * ndDegreeToRad);
				world->AddJoint(hinge);

				limbPivotLocation.m_posit += limbPivotLocation.m_front.Scale(limbLength);
				workSpace += limbLength;
			}

			ndBodyKinematic* calf1 = nullptr;
			ndJointHinge* lookActHinge = nullptr;
			{
				ndFloat32 lenght = limbLength * 0.5f;
				limbPivotLocation = dRollMatrix(-45.0f * ndDegreeToRad) * limbPivotLocation;
				ndMatrix bodyMatrix(limbPivotLocation);
				bodyMatrix.m_posit += limbPivotLocation.m_front.Scale(lenght * 0.5f);

				calf1 = AddCapsule(scene, bodyMatrix, limbMass * 0.5f, limbRadios, limbRadios, lenght);
				calf1->SetMatrix(bodyMatrix);

				ndMatrix caffPinAndPivotFrame(dGetIdentityMatrix());
				caffPinAndPivotFrame.m_front = limbPivotLocation.m_right.Scale(-1.0f);
				caffPinAndPivotFrame.m_up = limbPivotLocation.m_front;
				caffPinAndPivotFrame.m_right = caffPinAndPivotFrame.m_front.CrossProduct(caffPinAndPivotFrame.m_up);
				caffPinAndPivotFrame.m_posit = limbPivotLocation.m_posit;

				// add joint limit to prevent knee from flipping
				lookActHinge = new ndJointHinge(caffPinAndPivotFrame, calf0, calf1);
				lookActHinge->SetLimitState(true);
				lookActHinge->SetLimits(-60.0f * ndDegreeToRad, 60.0f * ndDegreeToRad);
				lookActHinge->SetAsSpringDamper(0.001f, 2000.0f, 50.0f);

				world->AddJoint(lookActHinge);

				limbPivotLocation.m_posit += limbPivotLocation.m_front.Scale(lenght);
				workSpace += lenght;
			}

			// add leg effector
			{
				ndBodyKinematic* const targetBody = calf1;

				ndMatrix effectorToeFrame(dGetIdentityMatrix());
				ndMatrix effectorRefFrame(dGetIdentityMatrix());
				effectorRefFrame.m_posit = thighPivot;
				effectorToeFrame.m_posit = limbPivotLocation.m_posit;

				ndMatrix effectorSwivelFrame(dGetIdentityMatrix());
				effectorSwivelFrame.m_front = (effectorToeFrame.m_posit - effectorRefFrame.m_posit).Normalize();
				effectorSwivelFrame.m_up = upDir;
				effectorSwivelFrame.m_right = (effectorSwivelFrame.m_front.CrossProduct(effectorSwivelFrame.m_up)).Normalize();
				effectorSwivelFrame.m_up = effectorSwivelFrame.m_right.CrossProduct(effectorSwivelFrame.m_front);

				ndFloat32 regularizer = 0.001f;
				ndIkSwivelPositionEffector* const effector = new ndIkSwivelPositionEffector(effectorToeFrame, effectorRefFrame, effectorSwivelFrame, targetBody, torso);
				effector->SetLinearSpringDamper(regularizer, 2000.0f, 50.0f);
				effector->SetAngularSpringDamper(regularizer, 2000.0f, 50.0f);
				effector->SetWorkSpaceConstraints(0.0f, workSpace * 0.9f);

				world->AddJoint(effector);

				ndEffectorInfo info(effector, lookActHinge);
				info.m_x_mapper = ndParamMapper(-0.2f, 0.2f);
				info.m_y_mapper = ndParamMapper(-0.2f, 0.2f);
				info.m_z_mapper = ndParamMapper(-0.15f, 0.15f);
				info.m_swivel_mapper = ndParamMapper(-20.0f * ndDegreeToRad, 20.0f * ndDegreeToRad);
				m_effectors.PushBack(info);
			}
		}
	}

	ndAiQuadrupedTest_2(const ndLoadSaveBase::ndLoadDescriptor& desc)
		:ndModel(ndLoadSaveBase::ndLoadDescriptor(desc))
	{
		dAssert(0);
	}

	~ndAiQuadrupedTest_2()	
	{
	}

	//void Save(const ndLoadSaveBase::ndSaveDescriptor& desc) const
	void Save(const ndLoadSaveBase::ndSaveDescriptor&) const
	{
		dAssert(0);
	}

	void Debug(ndConstraintDebugCallback& context) const
	{
		ndVector upVector(m_rootBody->GetMatrix().m_up);
		//for (ndInt32 i = 0; i < m_effectors.GetCount(); ++i)
		for (ndInt32 i = 0; i < 1; ++i)
		{
			const ndEffectorInfo& info = m_effectors[i];
			ndJointBilateralConstraint* const effector = info.m_effector;
			effector->DebugJoint(context);

			//ndMatrix lookAtMatrix0;
			//ndMatrix lookAtMatrix1;
			//ndJointBilateralConstraint* const lookAtJoint = info.m_lookAtJoint;
			//lookAtJoint->DebugJoint(context);
			//info.m_lookAtJoint->CalculateGlobalMatrix(lookAtMatrix0, lookAtMatrix1);
			//ndVector updir(lookAtMatrix1.m_posit + upVector.Scale(-1.0f));
			//context.DrawLine(lookAtMatrix1.m_posit, updir, ndVector(0.0f, 0.0f, 0.0f, 1.0f));

			//ndMatrix swivelMatrix0;
			//ndMatrix swivelMatrix1;
			//info.m_effector->CalculateSwivelMatrices(swivelMatrix0, swivelMatrix1);
			//
			//ndVector posit1(swivelMatrix1.m_posit);
			//posit1.m_y += 1.0f;
			//context.DrawLine(swivelMatrix1.m_posit, posit1, ndVector(0.0f, 0.0f, 0.0f, 1.0f));
		}
	}

	void PostUpdate(ndWorld* const world, ndFloat32 timestep)
	{
		ndModel::PostUpdate(world, timestep);
	}

	void PostTransformUpdate(ndWorld* const world, ndFloat32 timestep)
	{
		ndModel::PostTransformUpdate(world, timestep);
	}

	void Update(ndWorld* const world, ndFloat32 timestep)
	{
		ndModel::Update(world, timestep);

		ndVector upVector(m_rootBody->GetMatrix().m_up);
		for (ndInt32 i = 0; i < m_effectors.GetCount(); ++i)
		{
			ndEffectorInfo& info = m_effectors[i];
			ndVector posit(info.m_basePosition);
			posit.m_x += info.m_x_mapper.Interpolate(info.m_x);
			posit.m_y += info.m_y_mapper.Interpolate(info.m_y);
			posit.m_z += info.m_z_mapper.Interpolate(info.m_z);
			info.m_effector->SetPosition(posit);

			ndMatrix swivelMatrix0;
			ndMatrix swivelMatrix1;
			info.m_effector->CalculateSwivelMatrices(swivelMatrix0, swivelMatrix1);
			const ndFloat32 angle = info.m_effector->CalculateAngle(upVector, swivelMatrix1[1], swivelMatrix1[0]);
			info.m_effector->SetSwivelAngle(info.m_swivel_mapper.Interpolate(info.m_swivel) - angle);

			// calculate lookAt angle
			ndMatrix lookAtMatrix0;
			ndMatrix lookAtMatrix1;
			info.m_lookAtJoint->CalculateGlobalMatrix(lookAtMatrix0, lookAtMatrix1);
			const ndFloat32 lookAngle = info.m_lookAtJoint->CalculateAngle(upVector.Scale(-1.0f), lookAtMatrix0[1], lookAtMatrix0[0]);
			info.m_lookAtJoint->SetOffsetAngle(lookAngle);
		}
	}

	void ApplyControls(ndDemoEntityManager* const scene)
	{
		ndVector color(1.0f, 1.0f, 0.0f, 0.0f);
		scene->Print(color, "Control panel");

		ndEffectorInfo& info = m_effectors[0];

		bool change = false;
		ImGui::Text("position x");
		change = change | ImGui::SliderFloat("##x", &info.m_x, -1.0f, 1.0f);
		ImGui::Text("position y");
		change = change | ImGui::SliderFloat("##y", &info.m_y, -1.0f, 1.0f);
		ImGui::Text("position z");
		change = change | ImGui::SliderFloat("##z", &info.m_z, -1.0f, 1.0f);

		ImGui::Text("swivel");
		change = change | ImGui::SliderFloat("##swivel", &info.m_swivel, -1.0f, 1.0f);
		
		if (change)
		{
			m_rootBody->SetSleepState(false);

			for (ndInt32 i = 1; i < m_effectors.GetCount(); ++i)
			{
				m_effectors[i].m_x = info.m_x;
				m_effectors[i].m_y = info.m_y;
				m_effectors[i].m_z = info.m_z;
				m_effectors[i].m_swivel = info.m_swivel;
			}
		}
	}

	static void ControlPanel(ndDemoEntityManager* const scene, void* const context)
	{
		ndAiQuadrupedTest_2* const me = (ndAiQuadrupedTest_2*)context;
		me->ApplyControls(scene);
	}
	
	ndBodyDynamic* m_rootBody;
	ndFixSizeArray<ndEffectorInfo, 4> m_effectors;
};
D_CLASS_REFLECTION_IMPLEMENT_LOADER(ndAiQuadrupedTest_2);


void ndQuadrupedTest_2(ndDemoEntityManager* const scene)
{
	// build a floor
	BuildFloorBox(scene, dGetIdentityMatrix());
	//BuildFlatPlane(scene, true);

	ndVector origin1(0.0f, 0.0f, 0.0f, 0.0f);
	ndWorld* const world = scene->GetWorld();
	ndMatrix matrix(dYawMatrix(-0.0f * ndDegreeToRad));
	
	ndAiQuadrupedTest_2* const aiBot_1 = new ndAiQuadrupedTest_2(scene, matrix);
	scene->SetSelectedModel(aiBot_1);
	world->AddModel(aiBot_1);
	scene->Set2DDisplayRenderFunction(ndAiQuadrupedTest_2::ControlPanel, nullptr, aiBot_1);
	//world->AddJoint(new ndJointFix6dof(aiBot_1->m_rootBody->GetMatrix(), aiBot_1->m_rootBody, world->GetSentinelBody()));

	matrix.m_posit.m_x -= 4.0f;
	matrix.m_posit.m_y += 1.5f;
	matrix.m_posit.m_z += 0.25f;
	ndQuaternion rotation(ndVector(0.0f, 1.0f, 0.0f, 0.0f), 0.0f * ndDegreeToRad);
	scene->SetCameraMatrix(rotation, matrix.m_posit);
}