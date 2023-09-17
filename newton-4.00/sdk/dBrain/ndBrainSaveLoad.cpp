/* Copyright (c) <2003-2022> <Julio Jerez, Newton Game Dynamics>
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

#include "ndBrainStdafx.h"
#include "ndBrain.h"
#include "ndBrainSaveLoad.h"
#include "ndBrainLayerLinear.h"
#include "ndBrainLayerReluActivation.h"
#include "ndBrainLayerTanhActivation.h"
#include "ndBrainLayerSoftmaxActivation.h"
#include "ndBrainLayerSigmoidActivation.h"

ndBrain* ndBrainLoad::Load(const char* const pathName)
{
	class Loader : public ndBrainLoad
	{
		public:
		Loader(const char* const pathName)
			:ndBrainLoad()
		{
			m_file = fopen(pathName, "rb");
			ndAssert(m_file);
		}

		~Loader()
		{
			if (m_file)
			{
				fclose(m_file);
			}
		}

		ndInt32 ReadInt() const
		{
			ndInt32 value;
			ndInt32 error = 0;
			error = fscanf(m_file, "%d", &value);
			return value;
		}

		ndFloat32 ReadFloat() const
		{
			ndReal value;
			ndInt32 error = 0;
			error = fscanf(m_file, "%f", &value);
			return value;
		}

		void ReadString(char* const buffer) const
		{
			ndInt32 error = 0;
			error = fscanf(m_file, "%s", buffer);
		}

		FILE* m_file;
	};

	Loader loader(pathName);
	return loader.Load();
}

ndBrain* ndBrainLoad::Load() const
{
	char buffer[1024];
	ReadString(buffer);
	ReadString(buffer);
	ReadString(buffer);
	
	ReadString(buffer);
	ndInt32 layersCount = ReadInt();
	
	ndBrain* const brain = new ndBrain;
	for (ndInt32 i = 0; i < layersCount; ++i)
	{
		char layerType[256];
		ReadString(buffer);
		ReadString(layerType);
	
		ndBrainLayer* layer = nullptr;
		if (!strcmp(layerType, "ndBrainLayerLinear"))
		{
			layer = ndBrainLayerLinear::Load(this);
		}
		else if (!strcmp(layerType, "ndBrainLayerReluActivation"))
		{
			layer = ndBrainLayerReluActivation::Load(this);
		}
		else if (!strcmp(layerType, "ndBrainLayerTanhActivation"))
		{
			layer = ndBrainLayerTanhActivation::Load(this);
		}
		else if (!strcmp(layerType, "ndBrainLayerSigmoidActivation"))
		{
			layer = ndBrainLayerSigmoidActivation::Load(this);
		}
		else if (!strcmp(layerType, "ndBrainLayerSoftmaxActivation"))
		{
			layer = ndBrainLayerSoftmaxActivation::Load(this);
		}
		else
		{
			ndAssert(0);
		}
		ndAssert(layer);
		brain->AddLayer(layer);
	}
	
	return brain;
}

void ndBrainSave::Save(const ndBrain* const brain)
{
	char buffer[1024];
	auto Save = [this, &buffer](const char* const fmt, ...)
	{
		va_list v_args;
		buffer[0] = 0;
		va_start(v_args, fmt);
		vsprintf(buffer, fmt, v_args);
		va_end(v_args);
		WriteData(buffer);
	};
	
	Save("ndBrain version 1.0\n\n");
	Save("layersCount %d\n\n", brain->GetCount());
	
	for (ndInt32 i = 0; i < brain->GetCount(); ++i)
	{
		ndBrainLayer* const layer = (*brain)[i];
		Save("layer %s\n", layer->GetLabelId());
		Save("{\n");
		layer->Save(this);
		Save("}\n\n");
	}
}

void ndBrainSave::Save(const ndBrain* const brain, const char* const pathName)
{
	class SaveAgent: public ndBrainSave
	{
		public:
		SaveAgent(const char* const pathFilename)
			:ndBrainSave()
		{
			m_file = fopen(pathFilename, "wb");
			ndAssert(m_file);
		}

		~SaveAgent()
		{
			if (m_file)
			{
				fclose(m_file);
			}
		}

		void WriteData(const char* const data) const
		{
			ndInt32 error = 0;
			error = fprintf(m_file, "%s", data);
		}

		FILE* m_file;
	};

	SaveAgent saveAgent(pathName);
	saveAgent.Save(brain);
}
