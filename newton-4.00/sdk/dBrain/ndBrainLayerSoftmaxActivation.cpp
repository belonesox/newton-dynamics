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
#include "ndBrainSaveLoad.h"
#include "ndBrainLayerSoftmaxActivation.h"

ndBrainLayerSoftmaxActivation::ndBrainLayerSoftmaxActivation(ndInt32 neurons)
	:ndBrainLayerActivation(neurons)
{
}

ndBrainLayerSoftmaxActivation::ndBrainLayerSoftmaxActivation(const ndBrainLayerSoftmaxActivation& src)
	:ndBrainLayerActivation(src)
{
}

ndBrainLayer* ndBrainLayerSoftmaxActivation::Clone() const
{
	return new ndBrainLayerSoftmaxActivation(*this);
}

const char* ndBrainLayerSoftmaxActivation::GetLabelId() const
{
	return "ndBrainLayerSoftmaxActivation";
}

ndBrainLayer* ndBrainLayerSoftmaxActivation::Load(const ndBrainLoad* const loadSave)
{
	char buffer[1024];
	loadSave->ReadString(buffer);

	loadSave->ReadString(buffer);
	ndInt32 inputs = loadSave->ReadInt();
	ndBrainLayerSoftmaxActivation* const layer = new ndBrainLayerSoftmaxActivation(inputs);
	loadSave->ReadString(buffer);
	return layer;
}

void ndBrainLayerSoftmaxActivation::MakePrediction(const ndBrainVector& input, ndBrainVector& output) const
{
	ndAssert(input.GetCount() == output.GetCount());
	ndBrainFloat max = ndBrainFloat(1.0e-16f);
	for (ndInt32 i = input.GetCount() - 1; i >= 0; --i)
	{
		max = ndMax(input[i], max);
	}

	ndBrainFloat acc = ndBrainFloat(0.0f);
	for (ndInt32 i = input.GetCount() - 1; i >= 0; --i)
	{
		ndBrainFloat in = ndMax((input[i] - max), ndBrainFloat(-30.0f));
		ndAssert(in <= ndBrainFloat(0.0f));
		ndBrainFloat prob = ndBrainFloat(ndExp(in));
		output[i] = prob;
		acc += prob;
	}

	ndAssert(acc > ndBrainFloat(0.0f));
	output.Scale(ndBrainFloat(1.0f) / acc);
	output.FlushToZero();
}

void ndBrainLayerSoftmaxActivation::InputDerivative(const ndBrainVector& output, const ndBrainVector& outputDerivative, ndBrainVector& inputDerivative) const
{
	ndAssert(output.GetCount() == outputDerivative.GetCount());
	ndAssert(output.GetCount() == inputDerivative.GetCount());

	// calculate the output derivative which is a the Jacobian matrix time the input
	//for (ndInt32 i = 0; i < output.GetCount(); ++i)
	//{
	//	ndFloat32 s = output[i];
	//	ndFloat32 acc = (s * (ndFloat32(1.0f) - s)) * outputDerivative[i];
	//	for (ndInt32 j = 0; j < output.GetCount(); ++j)
	//	{
	//		if (i != j)
	//		{
	//			acc -= s * output[j] * outputDerivative[j];
	//		}
	//	}
	//	inputDerivative[i] = ndBrainFloat(acc);
	//}

	// better way to calculate the output derivative which is a the Jacobian matrix time the input
	// y = (O * I - O * transp(O)) * x
	ndBrainFloat s = -outputDerivative.Dot(output);
	inputDerivative.Set(output);
	inputDerivative.Scale(s);
	inputDerivative.MulAdd(output, outputDerivative);

	inputDerivative.FlushToZero();
}