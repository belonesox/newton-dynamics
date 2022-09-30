/* Copyright (c) <2003-2019> <Newton Game Dynamics>
*
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
*
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely
*/

#include <iostream>
#include "testStdafx.h"


class ndDemoEntityNotify: public ndBodyNotify
{
  public:
  ndDemoEntityNotify()
    :ndBodyNotify(ndVector (0.0f, -10.0f, 0.0f, 0.0f))
  {
    // here we set the application user data that
    // goes with the game engine, for now is just null
    m_applicationUserData = nullptr;
  }

  //virtual void OnApplyExternalForce(ndInt32 threadIndex, ndFloat32 timestep)
  virtual void OnApplyExternalForce(ndInt32, ndFloat32)
  {
    ndBodyDynamic* const dynamicBody = GetBody()->GetAsBodyDynamic();
    if (dynamicBody)
    {
      ndVector massMatrix(dynamicBody->GetMassMatrix());
      ndVector force(ndVector(0.0f, -10.0f, 0.0f, 0.0f).Scale(massMatrix.m_w));
      dynamicBody->SetForce(force);
      dynamicBody->SetTorque(ndVector::m_zero);
    }
  }

  //virtual void OnTransform(ndInt32 threadIndex, const ndMatrix& matrix)
  virtual void OnTransform(ndInt32, const ndMatrix&)
  {
    // apply this transformation matrix to the application user data.
    //dAssert(0);
  }

  void* m_applicationUserData;
};

ndVector FindFloor(const ndWorld& world, const ndVector& origin, ndFloat32 dist)
{
  // shot a vertical ray from a high altitude and collect the intersection parameter.
  ndVector p0(origin);
  ndVector p1(origin - ndVector(0.0f, ndAbs(dist), 0.0f, 0.0f));

  ndRayCastClosestHitCallback rayCaster;
  world.RayCast(rayCaster, p0, p1);
  return (rayCaster.m_param < 1.0f) ? rayCaster.m_contact.m_point : p0;
}

ndBodyDynamic* BuildFloorBox(ndWorld& world)
{
  world.Sync();
  ndShapeInstance box(new ndShapeBox(200.0f, 1.0f, 200.f));
  ndBodyDynamic* const body = new ndBodyDynamic();

  ndMatrix matrix(ndGetIdentityMatrix());
  matrix.m_posit.m_y = -0.5f;

  body->SetNotifyCallback(new ndDemoEntityNotify);
  body->SetMatrix(matrix);
  body->SetCollisionShape(box);
  world.AddBody(body);
  return body;
}

ndBodyDynamic* BuildSphere(ndWorld& world, ndFloat32 mass, const ndVector& origin, const ndFloat32 diameter, ndFloat32 offsetHigh)
{
  ndMatrix matrix(ndGetIdentityMatrix());
  matrix.m_posit = origin;
  matrix.m_posit.m_w = 1.0f;

  world.Sync();

  ndShapeInstance sphere(new ndShapeSphere(diameter * 0.5f));

  ndVector floor(FindFloor(world, matrix.m_posit + ndVector(0.0f, 100.0f, 0.0f, 0.0f), 200.0f));
  matrix.m_posit.m_y = floor.m_y + diameter * 0.5f * 0.99f;

  matrix.m_posit.m_y += offsetHigh;

  ndBodyDynamic* const body = new ndBodyDynamic();
  body->SetNotifyCallback(new ndDemoEntityNotify);
  body->SetMatrix(matrix);
  body->SetCollisionShape(sphere);
  body->SetMassMatrix(mass, sphere);
  world.AddBody(body);
  return body;
}

int main(int, const char*)
{
  ndWorld world;
  world.SetSubSteps(1);

  ndVector size(0.5f, 0.25f, 0.8f, 0.0f);

  ndVector origin(0.0f, 0.0f, 0.0f, 0.0f);
  ndBodyDynamic* bodyFloor = BuildFloorBox(world);
  ndBodyDynamic* bodySphere = BuildSphere(world, 1.0f, origin + ndVector(3.0f, 0.0f, 0.0f, 0.0f), 1.0f, 1.0f);

  ndJointSpherical joint(bodySphere->GetMatrix(), bodySphere, bodyFloor);

  ndFloat32 totalTime = 0;
  for (ndInt32 i = 0; i < 10000; i++)
  {
    if (i == 0) world.AddJoint(&joint);
    if (i == 2) world.RemoveJoint(&joint);

    world.Update(1.0f / 60.0f);
    totalTime += world.GetUpdateTime();
    world.Sync();

  }
  std::cout << "Newton demo completed successfully\n";
  return 0;
}
