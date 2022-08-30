#include "stdafx.h"
#include "exportMeshNode.h"

exportMeshNode* exportMeshNode::ImportAsfSkeleton(const char* const asfName, const char* const amcName)
{
	FILE* const fp = fopen(asfName, "rt");

	char token[256];
	auto ReadToken = [fp, &token]()
	{
		fscanf(fp, "%s", token);
	};

	auto ReadFloat = [fp]()
	{
		float value;
		fscanf(fp, "%f", &value);
		return value;
	};

	auto ReadInt = [fp]()
	{
		int value;
		fscanf(fp, "%d", &value);
		return value;
	};

	std::map<std::string, exportMeshNode*> map;

	exportMeshNode* entity = nullptr;
	float scale = 6.0f;
	float distScale = 1.0f;
	float angleScale = 1.0f;
	if (fp)
	{
		while (!feof(fp))
		{
			ReadToken();
			if (!strcmp(token, ":units"))
			{
				for (int i = 0; i < 3; i++)
				{
					ReadToken();
					if (!strcmp(token, "length"))
					{
						distScale = ReadFloat();
					}
					else if (!strcmp(token, "angle"))
					{
						ReadToken();
						if (!strcmp(token, "deg"))
						{
							angleScale = 3.14159265f / 180.0f;
						}
					}
					else
					{
						ReadToken();
					}
				}
			}
			else if (!strcmp(token, ":root"))
			{
				entity = new exportMeshNode();
				entity->m_name = "root";
				map[entity->m_name] = entity;
			}
			else if (!strcmp(token, "begin"))
			{
				ReadToken();
				ReadToken();
				ReadToken();
				ReadToken();
				exportMeshNode* const bone = new exportMeshNode();
				bone->m_name = token;
				map[bone->m_name] = bone;

				ReadToken();
				_ASSERT(!strcmp(token, "direction"));
				exportVector dir;
				dir.m_x = ReadFloat();
				dir.m_y = ReadFloat();
				dir.m_z = ReadFloat();
				dir.m_w = 0.0f;

				ReadToken();
				_ASSERT(!strcmp(token, "length"));
				float length = ReadFloat();
				if (dir.DotProduct(dir) > 1.e-5f)
				{
					//dir = dir.Normalize().Scale(length * distScale);
					dir = dir.Normalize().Scale(length * scale);
				}
				dir.m_w = 1.0f;

				ReadToken();
				_ASSERT(!strcmp(token, "axis"));
				exportVector euler;
				euler.m_x = ReadFloat();
				euler.m_y = ReadFloat();
				euler.m_z = ReadFloat();
				euler.m_w = 0.0f;
				bone->m_eulers = euler.Scale(angleScale);
				//if (bone->m_name != "ltibia") bone->m_eulers = euler.Scale(0.0f);

				bone->m_matrix.m_posit = dir;
			}
			else if (!strcmp(token, ":hierarchy"))
			{
				break;
			}
		}

		while (!feof(fp))
		{
			ReadToken();
			if (!strcmp(token, "begin"))
			{
			}
			else if (!strcmp(token, "end"))
			{
				break;
			}
			else
			{
				exportMeshNode* const parent = map[token];
				for (;1;)
				{
					int ch = getc(fp);
					if (ch == '\n')
					{
						break;
					}
					ungetc(ch, fp);
					ReadToken();
					exportMeshNode* const child = map[token];
					child->m_parent = parent;
					parent->m_children.push_back(child);
				}
			}
		}
		fclose(fp);

		int stack = 1;
		exportMeshNode* stackPool[128];
		exportMatrix parentMatrixPool[128];
		stackPool[0] = entity;
		parentMatrixPool[0] = exportMatrix();
		while (stack)
		{
			stack--;
			exportMeshNode* const bone = stackPool[stack];
			exportMatrix pitch(ndPitchMatrix(bone->m_eulers.m_x));
			exportMatrix yaw(ndYawMatrix(bone->m_eulers.m_y));
			exportMatrix roll(ndRollMatrix(bone->m_eulers.m_z));
			exportMatrix globalMatrix(pitch * yaw * roll);
			globalMatrix.m_posit = bone->m_matrix.m_posit + parentMatrixPool[stack].m_posit;
			globalMatrix.m_posit.m_w = 1.0f;
			bone->m_matrix = globalMatrix;
			for (std::list<exportMeshNode*>::const_iterator iter = bone->m_children.begin();
				iter != bone->m_children.end(); iter++)
			{
				stackPool[stack] = *iter;
				parentMatrixPool[stack] = globalMatrix;
				stack++;
			}
		}

		stack = 1;
		stackPool[0] = entity;
		parentMatrixPool[0] = exportMatrix();
		while (stack)
		{
			stack--;
			exportMeshNode* const bone = stackPool[stack];
			exportMatrix globalMatrix(bone->m_matrix);
			exportMatrix localMatrix(globalMatrix * parentMatrixPool[stack].Inverse());
			bone->m_eulers = localMatrix.CalcPitchYawRoll();
			bone->m_matrix = localMatrix;
			for (std::list<exportMeshNode*>::const_iterator iter = bone->m_children.begin();
				iter != bone->m_children.end(); iter++)
			{
				stackPool[stack] = *iter;
				parentMatrixPool[stack] = globalMatrix;
				stack++;
			}
		}
	}

	if (entity && amcName)
	{
		entity->ImportAmcAnimation(amcName);
	}

	return entity;
}

void exportMeshNode::ImportAmcAnimation(const char* const amcName)
{

}