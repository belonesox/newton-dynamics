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
#include "ndDemoMesh.h"
#include "ndDemoCamera.h"
#include "ndDebugDisplay.h"
#include "ndPhysicsUtils.h"
#include "ndPhysicsWorld.h"
#include "ndDemoEntityManager.h"

#define D_USE_GEOMETRY_SHADERS 
//#define D_TEST_CALCULATE_CONTACTS

#ifndef D_USE_GEOMETRY_SHADERS 
static glVector3 CalculatePoint(const ndMatrix& matrix, const ndVector& center, ndFloat32 x, ndFloat32 y, ndFloat32 w)
{
	ndVector point(center.m_x + x, center.m_y + y, center.m_z, center.m_w);
	point = matrix.TransformVector1x4(point.Scale(w));
	return glVector3(GLfloat(point.m_x), GLfloat(point.m_y), GLfloat(point.m_z));
}
#endif

static void DrawBox(const ndVector& p0, const ndVector& p1, glVector3 box[12][2])
{
	//ndMeshVector box[12][2];
	box[0][0] = glVector3(GLfloat(p0.m_x), GLfloat(p0.m_y), GLfloat(p0.m_z));
	box[0][1] = glVector3(GLfloat(p1.m_x), GLfloat(p0.m_y), GLfloat(p0.m_z));

	box[1][0] = glVector3(GLfloat(p0.m_x), GLfloat(p1.m_y), GLfloat(p0.m_z));
	box[1][1] = glVector3(GLfloat(p1.m_x), GLfloat(p1.m_y), GLfloat(p0.m_z));

	box[2][0] = glVector3(GLfloat(p0.m_x), GLfloat(p1.m_y), GLfloat(p1.m_z));
	box[2][1] = glVector3(GLfloat(p1.m_x), GLfloat(p1.m_y), GLfloat(p1.m_z));

	box[3][0] = glVector3(GLfloat(p0.m_x), GLfloat(p0.m_y), GLfloat(p1.m_z));
	box[3][1] = glVector3(GLfloat(p1.m_x), GLfloat(p0.m_y), GLfloat(p1.m_z));

	box[4][0] = glVector3(GLfloat(p0.m_x), GLfloat(p0.m_y), GLfloat(p0.m_z));
	box[4][1] = glVector3(GLfloat(p0.m_x), GLfloat(p1.m_y), GLfloat(p0.m_z));

	box[5][0] = glVector3(GLfloat(p1.m_x), GLfloat(p0.m_y), GLfloat(p0.m_z));
	box[5][1] = glVector3(GLfloat(p1.m_x), GLfloat(p1.m_y), GLfloat(p0.m_z));

	box[6][0] = glVector3(GLfloat(p0.m_x), GLfloat(p0.m_y), GLfloat(p1.m_z));
	box[6][1] = glVector3(GLfloat(p0.m_x), GLfloat(p1.m_y), GLfloat(p1.m_z));

	box[7][0] = glVector3(GLfloat(p1.m_x), GLfloat(p0.m_y), GLfloat(p1.m_z));
	box[7][1] = glVector3(GLfloat(p1.m_x), GLfloat(p1.m_y), GLfloat(p1.m_z));

	box[8][0] = glVector3(GLfloat(p0.m_x), GLfloat(p0.m_y), GLfloat(p0.m_z));
	box[8][1] = glVector3(GLfloat(p0.m_x), GLfloat(p0.m_y), GLfloat(p1.m_z));

	box[9][0] = glVector3(GLfloat(p1.m_x), GLfloat(p0.m_y), GLfloat(p0.m_z));
	box[9][1] = glVector3(GLfloat(p1.m_x), GLfloat(p0.m_y), GLfloat(p1.m_z));

	box[10][0] = glVector3(GLfloat(p0.m_x), GLfloat(p1.m_y), GLfloat(p0.m_z));
	box[10][1] = glVector3(GLfloat(p0.m_x), GLfloat(p1.m_y), GLfloat(p1.m_z));

	box[11][0] = glVector3(GLfloat(p1.m_x), GLfloat(p1.m_y), GLfloat(p0.m_z));
	box[11][1] = glVector3(GLfloat(p1.m_x), GLfloat(p1.m_y), GLfloat(p1.m_z));

	glDrawArrays(GL_LINES, 0, 24);
}

void RenderBodiesAABB(ndDemoEntityManager* const scene)
{
	ndWorld* const world = scene->GetWorld();
	
	GLuint shader = scene->GetShaderCache().m_wireFrame;
	
	ndDemoCamera* const camera = scene->GetCamera();
	const glMatrix viewProjectionMatrix(camera->GetViewMatrix() * camera->GetProjectionMatrix());
	
	glVector4 color;
	color.m_x = 0.0f;
	color.m_y = 0.0f;
	color.m_z = 1.0f;
	color.m_w = 1.0f;
	
	glUseProgram(shader);
	
	ndInt32 shadeColorLocation = glGetUniformLocation(shader, "shadeColor");
	ndInt32 projectionViewModelMatrixLocation = glGetUniformLocation(shader, "projectionViewModelMatrix");
	
	glUniform4fv(shadeColorLocation, 1, &color.m_x);
	glUniformMatrix4fv(projectionViewModelMatrixLocation, 1, false, &viewProjectionMatrix[0][0]);
	
	glVector3 box[12][2];
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, sizeof (glVector3), box);
	
	const ndBodyListView& bodyList = world->GetBodyList();
	for (ndBodyListView::ndNode* bodyNode = bodyList.GetFirst(); bodyNode; bodyNode = bodyNode->GetNext())
	{
		ndVector p0;
		ndVector p1;
		ndBodyKinematic* const body = bodyNode->GetInfo()->GetAsBodyKinematic();
		body->GetAABB(p0, p1);
		DrawBox(p0, p1, box);
	}
	glUseProgram(0);
	glDisableClientState(GL_VERTEX_ARRAY);
}

void RenderWorldScene(ndDemoEntityManager* const scene)
{
	ndWorld* const world = scene->GetWorld();
	GLuint shader = scene->GetShaderCache().m_wireFrame;

	ndDemoCamera* const camera = scene->GetCamera();
	const glMatrix viewProjectionMatrix(camera->GetViewMatrix() * camera->GetProjectionMatrix());

	glVector4 color(ndVector(1.0f, 1.0f, 0.0f, 1.0f));

	glUseProgram(shader);

	ndInt32 shadeColorLocation = glGetUniformLocation(shader, "shadeColor");
	ndInt32 projectionViewModelMatrixLocation = glGetUniformLocation(shader, "projectionViewModelMatrix");

	glUniform4fv(shadeColorLocation, 1, &color[0]);
	glUniformMatrix4fv(projectionViewModelMatrixLocation, 1, false, &viewProjectionMatrix[0][0]);
	glEnableClientState(GL_VERTEX_ARRAY);

	class ndDrawScene: public ndSceneTreeNotiFy
	{
		public: 
		ndDrawScene()
			:ndSceneTreeNotiFy()
		{
			glVertexPointer(3, GL_FLOAT, sizeof(glVector3), m_box);
		}

		virtual void OnDebugNode(const ndBvhNode* const node)
		{
			ndVector p0;
			ndVector p1;
			node->GetAabb(p0, p1);
			DrawBox(p0, p1, m_box);
		}

		glVector3 m_box[12][2];
	};

	ndDrawScene drawBroaphase;
	world->DebugScene(&drawBroaphase);

	glUseProgram(0);
	glDisableClientState(GL_VERTEX_ARRAY);
}

#ifdef D_USE_GEOMETRY_SHADERS 
void RenderContactPoints(ndDemoEntityManager* const scene)
{
	ndWorld* const world = scene->GetWorld();
	GLuint shader = scene->GetShaderCache().m_thickPoints;

	ndDemoCamera* const camera = scene->GetCamera();
	const ndMatrix viewMatrix(camera->GetViewMatrix());
	const ndMatrix projectionMatrix(camera->GetProjectionMatrix());

	glVector4 color;
	color.m_x = 255.0f / 255.0f;
	color.m_y = 0.0f;
	color.m_z = 0.0f;
	color.m_w = 1.0f;

	glUseProgram(shader);

	ndInt32 pixelSizeLocation = glGetUniformLocation(shader, "pixelSize");
	ndInt32 pixelColorLocation = glGetUniformLocation(shader, "inputPixelColor");

	ndInt32 viewModelMatrixLocation = glGetUniformLocation(shader, "viewModelMatrix");
	ndInt32 projectionMatrixLocation = glGetUniformLocation(shader, "projectionMatrix");

	glUniform4fv(pixelColorLocation, 1, &color.m_x);

	const glMatrix glViewMatrix(viewMatrix);
	const glMatrix glProjectionMatrix(projectionMatrix);

	glUniformMatrix4fv(viewModelMatrixLocation, 1, false, &glViewMatrix[0][0]);
	glUniformMatrix4fv(projectionMatrixLocation, 1, false, &glProjectionMatrix[0][0]);
	
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	ndFloat32 radius = 4.0f / (ndFloat32)viewport[3];

	glVector4 quad[] =
	{
		ndVector(-radius, -radius, ndFloat32(0.0f), ndFloat32(0.0f)),
		ndVector(radius, -radius, ndFloat32(0.0f), ndFloat32(0.0f)),
		ndVector(-radius,  radius, ndFloat32(0.0f), ndFloat32(0.0f)),
		ndVector(radius,  radius, ndFloat32(0.0f), ndFloat32(0.0f)),
	};

	ndFixSizeArray<glVector3, 1024> pointBuffer;
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(4, GL_FLOAT, sizeof(glVector3), &pointBuffer[0]);

	glUniform4fv(pixelSizeLocation, 4, &quad[0][0]);

	const ndContactArray& contactList = world->GetContactList();
	for (ndInt32 i = 0; i < contactList.GetCount(); ++i)
	{
		const ndContact* const contact = contactList[i];
		if (contact->IsActive())
		{
			const ndContactPointList& contactPoints = contact->GetContactPoints();
			for (ndContactPointList::ndNode* contactPointsNode = contactPoints.GetFirst(); contactPointsNode; contactPointsNode = contactPointsNode->GetNext())
			{
				const ndContactPoint& contactPoint = contactPointsNode->GetInfo();
				pointBuffer.PushBack(contactPoint.m_point);
				if (pointBuffer.GetCount() == pointBuffer.GetCapacity())
				{
					glDrawArrays(GL_POINTS, 0, pointBuffer.GetCount());
					pointBuffer.SetCount(0);
				}
			}

			// test User contact calculation.
			#ifdef D_TEST_CALCULATE_CONTACTS
			ndContactSolver solver;
			ndContactNotify notification(world->GetScene());
			ndFixSizeArray<ndContactPoint, 16> contactOut;
			ndBodyKinematic* bodyA = contact->GetBody0();
			ndBodyKinematic* bodyB = contact->GetBody1();
			const ndShapeInstance& shapeA = bodyA->GetCollisionShape();
			const ndShapeInstance& shapeB = bodyB->GetCollisionShape();

			solver.CalculateContacts(
				&shapeA, bodyA->GetMatrix(), bodyA->GetVelocity(),
				&shapeB, bodyB->GetMatrix(), bodyB->GetVelocity(),
				contactOut, &notification);
			#endif
		}
	}
	glDrawArrays(GL_POINTS, 0, pointBuffer.GetCount());

	glUseProgram(0);
	glDisableClientState(GL_VERTEX_ARRAY);
}

#else

void RenderContactPoints(ndDemoEntityManager* const scene)
{
	ndWorld* const world = scene->GetWorld();
	GLuint shader = scene->GetShaderCache().m_wireFrame;

	ndDemoCamera* const camera = scene->GetCamera();
	const ndMatrix viewProjectionMatrix(camera->GetViewMatrix() * camera->GetProjectionMatrix());
	const ndMatrix invViewProjectionMatrix(camera->GetProjectionMatrix().Inverse4x4() * camera->GetViewMatrix().OrthoInverse());

	glVector4 color(ndVector(1.0f, 0.0f, 0.0f, 1.0f));

	glUseProgram(shader);

	ndInt32 pixelColorLocation = glGetUniformLocation(shader, "shadeColor");
	ndInt32 projectionViewModelMatrixLocation = glGetUniformLocation(shader, "projectionViewModelMatrix");
	glUniform4fv(pixelColorLocation, 1, &color[0]);
	const glMatrix viewProjMatrix(viewProjectionMatrix);
	glUniformMatrix4fv(projectionViewModelMatrixLocation, 1, false, &viewProjMatrix[0][0]);

	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	ndFloat32 pizelSize = 8.0f / viewport[2];

	glVector3 pointBuffer[4];
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, sizeof(glVector3), pointBuffer);
	const ndContactArray& contactList = world->GetContactList();
	for (ndInt32 i = 0; i < contactList.GetCount(); ++i)
	{
		const ndContact* const contact = contactList[i];
		if (contact->IsActive())
		{
			const ndContactPointList& contactPoints = contact->GetContactPoints();
			for (ndContactPointList::ndNode* contactPointsNode = contactPoints.GetFirst(); contactPointsNode; contactPointsNode = contactPointsNode->GetNext())
			{
				const ndContactPoint& contactPoint = contactPointsNode->GetInfo();
				ndVector pointInScreenSpace(viewProjectionMatrix.TransformVector1x4(contactPoint.m_point));
				ndFloat32 zDist = pointInScreenSpace.m_w;
				pointInScreenSpace = pointInScreenSpace.Scale(1.0f / zDist);

				pointBuffer[0] = CalculatePoint(invViewProjectionMatrix, pointInScreenSpace, -pizelSize, pizelSize, zDist);
				pointBuffer[1] = CalculatePoint(invViewProjectionMatrix, pointInScreenSpace, -pizelSize, -pizelSize, zDist);
				pointBuffer[2] = CalculatePoint(invViewProjectionMatrix, pointInScreenSpace, pizelSize, pizelSize, zDist);
				pointBuffer[3] = CalculatePoint(invViewProjectionMatrix, pointInScreenSpace, pizelSize, -pizelSize, zDist);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			}
		}
	}

	glUseProgram(0);
	glDisableClientState(GL_VERTEX_ARRAY);
}
#endif

void RenderPolygon(ndDemoEntityManager* const scene, const ndVector* const points, ndInt32 count, const ndVector& color)
{
	GLuint shader = scene->GetShaderCache().m_wireFrame;

	ndDemoCamera* const camera = scene->GetCamera();
	const glMatrix viewProjectionMatrix(camera->GetViewMatrix() * camera->GetProjectionMatrix());

	glUseProgram(shader);

	ndInt32 shadeColorLocation = glGetUniformLocation(shader, "shadeColor");
	ndInt32 projectionViewModelMatrixLocation = glGetUniformLocation(shader, "projectionViewModelMatrix");
	glUniformMatrix4fv(projectionViewModelMatrixLocation, 1, false, &viewProjectionMatrix[0][0]);

	glVector3 line[64];

	glVector4 colorgl(color);
	glUniform4fv(shadeColorLocation, 1, &colorgl[0]);

	ndInt32 i0 = count - 1;
	for (ndInt32 i = 0; i < count; ++i)
	{
		line[i * 2 + 0].m_x = GLfloat(points[i0].m_x);
		line[i * 2 + 0].m_y = GLfloat(points[i0].m_y);
		line[i * 2 + 0].m_z = GLfloat(points[i0].m_z);
		line[i * 2 + 1].m_x = GLfloat(points[i].m_x);
		line[i * 2 + 1].m_y = GLfloat(points[i].m_y);
		line[i * 2 + 1].m_z = GLfloat(points[i].m_z);
		i0 = i;
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, sizeof(glVector3), line);

	glDrawArrays(GL_LINES, 0, count * 2);
	glDisableClientState(GL_VERTEX_ARRAY);
	glUseProgram(0);
}

void RenderBodyFrame(ndDemoEntityManager* const scene)
{
	ndWorld* const world = scene->GetWorld();
	GLuint shader = scene->GetShaderCache().m_wireFrame;
	
	ndDemoCamera* const camera = scene->GetCamera();
	const glMatrix viewProjectionMatrix(camera->GetViewMatrix() * camera->GetProjectionMatrix());
	
	glUseProgram(shader);
	
	ndInt32 shadeColorLocation = glGetUniformLocation(shader, "shadeColor");
	ndInt32 projectionViewModelMatrixLocation = glGetUniformLocation(shader, "projectionViewModelMatrix");
	glUniformMatrix4fv(projectionViewModelMatrixLocation, 1, false, &viewProjectionMatrix[0][0]);
	
	glVector3 line[2];
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, sizeof(glVector3), line);
	
	const ndBodyListView& bodyList = world->GetBodyList();
	for (ndBodyListView::ndNode* bodyNode = bodyList.GetFirst(); bodyNode; bodyNode = bodyNode->GetNext())
	{
		ndBodyKinematic* const body = bodyNode->GetInfo()->GetAsBodyKinematic();
	
		ndMatrix matrix(body->GetMatrix());
		ndVector o(matrix.m_posit);
		line[0].m_x = GLfloat(o.m_x);
		line[0].m_y = GLfloat(o.m_y);
		line[0].m_z = GLfloat(o.m_z);
	
		ndVector x(o + matrix.RotateVector(ndVector(0.5f, 0.0f, 0.0f, 0.0f)));
		line[1].m_x = GLfloat(x.m_x);
		line[1].m_y = GLfloat(x.m_y);
		line[1].m_z = GLfloat(x.m_z);
		glVector4 color (ndVector (1.0f, 0.0f, 0.0f, 0.0f));
		glUniform4fv(shadeColorLocation, 1, &color[0]);
		glDrawArrays(GL_LINES, 0, 2);
	
		x = o + matrix.RotateVector(ndVector(0.0f, 0.5f, 0.0f, 0.0f));
		line[1].m_x = GLfloat(x.m_x);
		line[1].m_y = GLfloat(x.m_y);
		line[1].m_z = GLfloat(x.m_z);
		color = glVector4(ndVector(0.0f, 1.0f, 0.0f, 0.0f));
		glUniform4fv(shadeColorLocation, 1, &color[0]);
		glDrawArrays(GL_LINES, 0, 2);
	
		x = o + matrix.RotateVector(ndVector(0.0f, 0.0f, 0.5f, 0.0f));
		line[1].m_x = GLfloat(x.m_x);
		line[1].m_y = GLfloat(x.m_y);
		line[1].m_z = GLfloat(x.m_z);
		color = glVector4(ndVector(0.0f, 0.0f, 1.0f, 0.0f));
		glUniform4fv(shadeColorLocation, 1, &color[0]);
		glDrawArrays(GL_LINES, 0, 2);
	}
	
	glDisableClientState(GL_VERTEX_ARRAY);
	glUseProgram(0);
}

void RenderCenterOfMass(ndDemoEntityManager* const scene)
{
	ndWorld* const world = scene->GetWorld();
	GLuint shader = scene->GetShaderCache().m_wireFrame;
	
	ndDemoCamera* const camera = scene->GetCamera();
	const glMatrix viewProjectionMatrix(camera->GetViewMatrix() * camera->GetProjectionMatrix());
	
	glUseProgram(shader);
	
	ndInt32 shadeColorLocation = glGetUniformLocation(shader, "shadeColor");
	ndInt32 projectionViewModelMatrixLocation = glGetUniformLocation(shader, "projectionViewModelMatrix");
	glUniformMatrix4fv(projectionViewModelMatrixLocation, 1, false, &viewProjectionMatrix[0][0]);
	
	glVector3 line[2];
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, sizeof(glVector3), line);
	
	const ndBodyListView& bodyList = world->GetBodyList();
	for (ndBodyListView::ndNode* bodyNode = bodyList.GetFirst(); bodyNode; bodyNode = bodyNode->GetNext())
	{
		ndBodyKinematic* const body = bodyNode->GetInfo()->GetAsBodyKinematic();
	
		ndMatrix matrix(body->GetMatrix());
		ndVector com(body->GetCentreOfMass());
		
		ndVector o(matrix.TransformVector(com));
		line[0].m_x = GLfloat(o.m_x);
		line[0].m_y = GLfloat(o.m_y);
		line[0].m_z = GLfloat(o.m_z);
	
		ndVector x(o + matrix.RotateVector(ndVector(0.5f, 0.0f, 0.0f, 0.0f)));
		line[1].m_x = GLfloat(x.m_x);
		line[1].m_y = GLfloat(x.m_y);
		line[1].m_z = GLfloat(x.m_z);
		glVector4 color (ndVector (1.0f, 0.0f, 0.0f, 0.0f));
		glUniform4fv(shadeColorLocation, 1, &color[0]);
		glDrawArrays(GL_LINES, 0, 2);
	
		x = o + matrix.RotateVector(ndVector(0.0f, 0.5f, 0.0f, 0.0f));
		line[1].m_x = GLfloat(x.m_x);
		line[1].m_y = GLfloat(x.m_y);
		line[1].m_z = GLfloat(x.m_z);
		color = glVector4(ndVector(0.0f, 1.0f, 0.0f, 0.0f));
		glUniform4fv(shadeColorLocation, 1, &color[0]);
		glDrawArrays(GL_LINES, 0, 2);
	
		x = o + matrix.RotateVector(ndVector(0.0f, 0.0f, 0.5f, 0.0f));
		line[1].m_x = GLfloat(x.m_x);
		line[1].m_y = GLfloat(x.m_y);
		line[1].m_z = GLfloat(x.m_z);
		color = glVector4(ndVector (0.0f, 0.0f, 1.0f, 0.0f));
		glUniform4fv(shadeColorLocation, 1, &color[0]);
		glDrawArrays(GL_LINES, 0, 2);
	}
	
	glDisableClientState(GL_VERTEX_ARRAY);
	glUseProgram(0);
}

#ifdef D_USE_GEOMETRY_SHADERS 
void RenderParticles(ndDemoEntityManager* const scene)
{
	ndWorld* const world = scene->GetWorld();
	GLuint shader = scene->GetShaderCache().m_spriteSpheres;

	ndDemoCamera* const camera = scene->GetCamera();
	const ndMatrix viewMatrix(camera->GetViewMatrix());
	const ndMatrix projectionMatrix(camera->GetProjectionMatrix());

	glVector4 color;
	color.m_x = 50.0f / 255.0f;
	color.m_y = 100.0f / 255.0f;
	color.m_z = 200.0f / 255.0f;
	color.m_w = 1.0f;

	glUseProgram(shader);

	ndInt32 shadeColorLocation = glGetUniformLocation(shader, "shadeColor");
	ndInt32 viewModelMatrixLocation = glGetUniformLocation(shader, "viewModelMatrix");

	ndInt32 projectionMatrixLocation = glGetUniformLocation(shader, "projectionMatrix");

	ndInt32 quadLocation = glGetUniformLocation(shader, "quadSize");
	ndInt32 uvSizeLocation = glGetUniformLocation(shader, "uvSize");
	ndInt32 radiusLocation = glGetUniformLocation(shader, "spriteRadius");

	glUniform4fv(shadeColorLocation, 1, &color.m_x);

	const glMatrix glViewMatrix(viewMatrix);
	glUniformMatrix4fv(viewModelMatrixLocation, 1, false, &glViewMatrix[0][0]);

	const glMatrix glProjectionMatrix(projectionMatrix);
	glUniformMatrix4fv(projectionMatrixLocation, 1, false, &glProjectionMatrix[0][0]);
	
	const ndBodyList& particles = world->GetParticleList();
	for (ndBodyList::ndNode* particleNode = particles.GetFirst(); particleNode; particleNode = particleNode->GetNext())
	{
		ndBodyParticleSet* const particle = particleNode->GetInfo()->GetAsBodyParticleSet();
		const ndArray<ndVector>& positions = particle->GetPositions();
		if (positions.GetCount())
		{
			glEnableClientState(GL_VERTEX_ARRAY);
			//glVertexPointer(3, GL_FLOAT, sizeof(glVector3), &pointBuffer[0]);
			glVertexPointer(4, GL_FLOAT, 0, &positions[0]);

			ndFloat32 radius = 1.0f * particle->GetParticleRadius();

			glVector4 quadUV[] =
			{
				ndVector(-1.0f, -1.0f, 0.0f, 0.0f),
				ndVector(1.0f, -1.0f, 0.0f, 0.0f),
				ndVector(-1.0f, 1.0f, 0.0f, 0.0f),
				ndVector(1.0f, 1.0f, 0.0f, 0.0f),
			};

			glVector4 quad[] =
			{
				ndVector(-radius, -radius, ndFloat32(0.0f), ndFloat32(0.0f)),
				ndVector(radius, -radius, ndFloat32(0.0f), ndFloat32(0.0f)),
				ndVector(-radius,  radius, ndFloat32(0.0f), ndFloat32(0.0f)),
				ndVector(radius,  radius, ndFloat32(0.0f), ndFloat32(0.0f)),
			};

			glVector4 spriteRadius(radius, radius, radius, 0.0f);
			glUniform4fv(radiusLocation, 1, &spriteRadius.m_x);
			glUniform4fv(quadLocation, 4, &quad[0].m_x);
			glUniform4fv(uvSizeLocation, 4, &quadUV[0][0]);
			glDrawArrays(GL_POINTS, 0, positions.GetCount());
		}
	}
	
	glUseProgram(0);
	glDisableClientState(GL_VERTEX_ARRAY);
}

#else

void RenderParticles(ndDemoEntityManager* const scene)
{
	ndWorld* const world = scene->GetWorld();
	GLuint shader = scene->GetShaderCache().m_wireFrame;

	ndDemoCamera* const camera = scene->GetCamera();
	const ndMatrix viewMatrix(camera->GetViewMatrix());
	const ndMatrix projectionMatrix(camera->GetProjectionMatrix());
	const ndMatrix viewProjectionMatrix(viewMatrix * projectionMatrix);

	glVector4 color;
	color.m_x = 50.0f / 255.0f;
	color.m_y = 100.0f / 255.0f;
	color.m_z = 200.0f / 255.0f;
	color.m_w = 1.0f;

	glUseProgram(shader);

	ndInt32 shadeColorLocation = glGetUniformLocation(shader, "shadeColor");
	ndInt32 projectionViewModelMatrixLocation = glGetUniformLocation(shader, "projectionViewModelMatrix");

	glUniform4fv(shadeColorLocation, 1, &color.m_x);
	const glMatrix viewProjMatrix(viewProjectionMatrix);
	glUniformMatrix4fv(projectionViewModelMatrixLocation, 1, false, &viewProjMatrix[0][0]);

	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	static ndArray<glVector3> pointBuffer;

	const ndBodyParticleSetList& particles = world->GetParticleList();
	for (ndBodyParticleSetList::ndNode* particleNode = particles.GetFirst(); particleNode; particleNode = particleNode->GetNext())
	{
		ndBodyParticleSet* const particle = particleNode->GetInfo();
		const ndArray<ndVector>& positions = particle->GetPositions();

		pointBuffer.SetCount(6 * positions.GetCount());
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, &pointBuffer[0]);

		{
			//D_TRACKTIME();
			ndFloat32 radius = particle->GetParticleRadius();

			//radius *= 16.0f;
			//radius *= 0.7f;
			radius *= 0.5f;
			ndVector quad[] =
			{
				ndVector(-radius,  radius, ndFloat32(0.0f), ndFloat32(0.0f)),
				ndVector(-radius, -radius, ndFloat32(0.0f), ndFloat32(0.0f)),
				ndVector(radius,  radius, ndFloat32(0.0f), ndFloat32(0.0f)),
				ndVector(radius,  radius, ndFloat32(0.0f), ndFloat32(0.0f)),
				ndVector(-radius, -radius, ndFloat32(0.0f), ndFloat32(0.0f)),
				ndVector(radius, -radius, ndFloat32(0.0f), ndFloat32(0.0f)),
			};

			for (ndInt32 i = 0; i < positions.GetCount(); ++i)
			{
				const ndVector p(viewMatrix.TransformVector(positions[i]));

				ndInt32 j = i * 6;
				pointBuffer[j + 0] = viewMatrix.UntransformVector(p + quad[0]);
				pointBuffer[j + 1] = viewMatrix.UntransformVector(p + quad[1]);
				pointBuffer[j + 2] = viewMatrix.UntransformVector(p + quad[2]);
				pointBuffer[j + 3] = viewMatrix.UntransformVector(p + quad[3]);
				pointBuffer[j + 4] = viewMatrix.UntransformVector(p + quad[4]);
				pointBuffer[j + 5] = viewMatrix.UntransformVector(p + quad[5]);
			}
		}
		glDrawArrays(GL_TRIANGLES, 0, pointBuffer.GetCount());
	}

	glUseProgram(0);
	glDisableClientState(GL_VERTEX_ARRAY);
}
#endif

class ndJoindDebug : public ndConstraintDebugCallback
{
	public:
	ndJoindDebug(ndDemoEntityManager* const scene)
	{
		ndDemoCamera* const camera = scene->GetCamera();
		const glMatrix viewProjectionMatrix(camera->GetViewMatrix() * camera->GetProjectionMatrix());
		m_shader = scene->GetShaderCache().m_wireFrame;

		glUseProgram(m_shader);

		m_shadeColorLocation = glGetUniformLocation(m_shader, "shadeColor");
		m_projectionViewModelMatrixLocation = glGetUniformLocation(m_shader, "projectionViewModelMatrix");
		glUniformMatrix4fv(m_projectionViewModelMatrixLocation, 1, false, &viewProjectionMatrix[0][0]);

		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, sizeof(glVector3), m_line);
	}

	~ndJoindDebug()
	{
		glDisableClientState(GL_VERTEX_ARRAY);
		glUseProgram(0);
	}

	void DrawPoint(const ndVector& point, const ndVector& color, ndFloat32 thickness)
	{
		m_line[0].m_x = GLfloat(point.m_x);
		m_line[0].m_y = GLfloat(point.m_y);
		m_line[0].m_z = GLfloat(point.m_z);
		glVector4 c(color);

		glPointSize(GLfloat(thickness));
		glUniform4fv(m_shadeColorLocation, 1, &c[0]);
		glDrawArrays(GL_POINTS, 0, 1);
		glPointSize(1);
	}

	void DrawLine(const ndVector& p0, const ndVector& p1, const ndVector& color, ndFloat32 thickness)
	{
		m_line[0].m_x = GLfloat(p0.m_x);
		m_line[0].m_y = GLfloat(p0.m_y);
		m_line[0].m_z = GLfloat(p0.m_z);
		m_line[1].m_x = GLfloat(p1.m_x);
		m_line[1].m_y = GLfloat(p1.m_y);
		m_line[1].m_z = GLfloat(p1.m_z);
		glVector4 c(color);

		glLineWidth(GLfloat(thickness));
		glUniform4fv(m_shadeColorLocation, 1, &c[0]);
		glDrawArrays(GL_LINES, 0, 2);
		glLineWidth(1);
	}

	GLuint m_shader;
	ndInt32 m_shadeColorLocation;
	ndInt32 m_projectionViewModelMatrixLocation;

	glVector3 m_line[2];
};

void RenderJointsDebugInfo(ndDemoEntityManager* const scene)
{
	ndJoindDebug debugJoint(scene);
	debugJoint.SetScale(0.2f);
	ndWorld* const workd = scene->GetWorld();
	const ndJointList& jointList = workd->GetJointList();
	for (ndJointList::ndNode* jointNode = jointList.GetFirst(); jointNode; jointNode = jointNode->GetNext())
	{
		ndJointBilateralConstraint* const joint = *jointNode->GetInfo();
		joint->DebugJoint(debugJoint);
	}
}

void RenderModelsDebugInfo(ndDemoEntityManager* const scene)
{
	ndJoindDebug debugJoint(scene);
	debugJoint.SetScale(0.2f);
	ndWorld* const workd = scene->GetWorld();
	const ndModelList& modelList = workd->GetModelList();
	for (ndModelList::ndNode* modelNode = modelList.GetFirst(); modelNode; modelNode = modelNode->GetNext())
	{
		ndModel* const model = *modelNode->GetInfo();
		model->Debug(debugJoint);
	}
}

void RenderNormalForces(ndDemoEntityManager* const scene, ndFloat32 scale)
{
	ndWorld* const world = scene->GetWorld();

	ndJoindDebug debugJoint(scene);

	const ndVector color(1.0f, 1.0f, 0.0f, 1.0f);
	const ndContactArray& contactList = world->GetContactList();
	for (ndInt32 i = 0; i < contactList.GetCount(); ++i)
	{
		const ndContact* const contact = contactList[i];
		if (contact->IsActive())
		{
			const ndContactPointList& contactPoints = contact->GetContactPoints();
			for (ndContactPointList::ndNode* contactPointsNode = contactPoints.GetFirst(); contactPointsNode; contactPointsNode = contactPointsNode->GetNext())
			{
				const ndContactMaterial& contactPoint = contactPointsNode->GetInfo();
				const ndVector origin(contactPoint.m_point);
				const ndVector normal(contactPoint.m_normal);
				const ndVector dest(origin + normal.Scale(contactPoint.m_normal_Force.m_force * scale));
				debugJoint.DrawLine(origin, dest, color, 1.0f);
			}
		}
	}
}