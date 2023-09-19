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
#include "ndBrainVector.h"
#include "ndBrainSaveLoad.h"

ndBrain::ndBrain()
	:ndArray<ndBrainLayer*>()
{
}

ndBrain::ndBrain(const ndBrain& src)
	:ndArray<ndBrainLayer*>()
{
	const ndArray<ndBrainLayer*>& srcLayers = src;
	for (ndInt32 i = 0; i < srcLayers.GetCount(); ++i)
	{
		ndBrainLayer* const layer = srcLayers[i]->Clone();
		AddLayer(layer);
	}
	CopyFrom(src);
}

ndBrain::~ndBrain()
{
	for (ndInt32 i = GetCount() - 1; i >= 0 ; --i)
	{
		delete (*this)[i];
	}
}

ndInt32 ndBrain::GetInputSize() const
{
	return GetCount() ? (*this)[0]->GetInputSize() : 0;
}

ndInt32 ndBrain::GetOutputSize() const
{
	return GetCount() ? (*this)[GetCount()-1]->GetOutputSize() : 0;
}

void ndBrain::CopyFrom(const ndBrain& src)
{
	const ndArray<ndBrainLayer*>& layers = *this;
	const ndArray<ndBrainLayer*>& srcLayers = src;
	for (ndInt32 i = 0; i < layers.GetCount(); ++i)
	{
		layers[i]->CopyFrom(*srcLayers[i]);
	}
}

void ndBrain::SoftCopy(const ndBrain& src, ndReal blend)
{
	const ndArray<ndBrainLayer*>& layers = *this;
	const ndArray<ndBrainLayer*>& srcLayers = src;
	for (ndInt32 i = 0; i < layers.GetCount(); ++i)
	{
		layers[i]->Blend(*srcLayers[i], blend);
	}
}

ndBrainLayer* ndBrain::AddLayer(ndBrainLayer* const layer)
{
	ndAssert(!GetCount() || ((*this)[GetCount() - 1]->GetOutputSize() == layer->GetInputSize()));
	PushBack(layer);
	return layer;
}

void ndBrain::InitWeights(ndReal weighVariance, ndReal biasVariance)
{
	ndArray<ndBrainLayer*>& layers = *this;
	for (ndInt32 i = layers.GetCount() - 1; i >= 0; --i)
	{
		layers[i]->InitWeights(weighVariance, biasVariance);
	}
}

void ndBrain::InitWeightsXavierMethod()
{
	ndArray<ndBrainLayer*>& layers = *this;
	for (ndInt32 i = layers.GetCount() - 1; i >= 0; --i)
	{
		ndBrainLayer* const layer = layers[i];
		if (layer->HasParameters())
		{
			if (i < (layers.GetCount() - 1))
			{
				const char* const labelMame = layers[i + 1]->GetLabelId();
				if (!strcmp(labelMame, "ndBrainLayerTanhActivation"))
				{
					layer->InitWeightsXavierMethod();
				}
				else
				{
					layer->InitWeights(ndReal(0.1f), ndReal(0.1f));
				}
			}
			else
			{
				layer->InitWeights(ndReal(0.1f), ndReal(0.1f));
			}
		}
	}
}

void ndBrain::MakePrediction(const ndBrainVector& input, ndBrainVector& output)
{
	const ndArray<ndBrainLayer*>& layers = *this;
	ndInt32 maxSize = layers[0]->GetInputSize();
	for (ndInt32 i = 0; i < GetCount(); ++i)
	{
		maxSize = ndMax(maxSize, layers[i]->GetOutputSize());
	}

	ndReal* const memBuffer = ndAlloca(ndReal, maxSize * 2 + 256);
	ndBrainMemVector in(memBuffer, input.GetCount());
	ndBrainMemVector out(memBuffer + maxSize + 128, input.GetCount());

	in.Set(input);
	for (ndInt32 i = 0; i < GetCount(); ++i)
	{
		out.SetSize(layers[i]->GetOutputSize());
		layers[i]->MakePrediction(in, out);
		in.Swap(out);
	}

	ndAssert(in.GetCount() == output.GetCount());
	output.Set(in);
}

void ndBrain::CalculateInputGradient(const ndBrainVector& input, ndBrainVector& inputGradients)
{
	ndFixSizeArray<ndInt32, 256> prefixScan;
	const ndArray<ndBrainLayer*>& layers = *this;

	ndInt32 maxSize = 0;
	ndInt32 sizeAcc = (layers[0]->GetInputSize() + 7) & -8;

	prefixScan.PushBack(0);
	for (ndInt32 i = 0; i < GetCount(); ++i)
	{
		prefixScan.PushBack(sizeAcc);
		sizeAcc += (layers[i]->GetOutputSize() + 7) & -8;
		maxSize = ndMax(maxSize, layers[i]->GetOutputSize());
	}
	prefixScan.PushBack(sizeAcc);

	const ndReal* const memBuffer = ndAlloca(ndReal, sizeAcc + 8);
	const ndReal* const gradientBuffer = ndAlloca(ndReal, maxSize * 2 + 256);

	ndBrainMemVector in0(memBuffer, input.GetCount());
	in0.Set(input);
	for (ndInt32 i = 0; i < GetCount(); ++i)
	{
		ndBrainMemVector in(memBuffer + prefixScan[i + 0], layers[i]->GetInputSize());
		ndBrainMemVector out(memBuffer + prefixScan[i + 1], layers[i]->GetOutputSize());
		layers[i]->MakePrediction(in, out);
	}
	
	ndBrainMemVector gradientIn(gradientBuffer, GetOutputSize());
	ndBrainMemVector gradientOut(gradientBuffer + maxSize + 128, GetOutputSize());
	gradientOut.Set(ndReal(1.0f));
	for (ndInt32 i = layers.GetCount() - 1; i >= 0; --i)
	{
		const ndBrainLayer* const layer = layers[i];
		gradientIn.SetSize(layer->GetInputSize());
		const ndBrainMemVector out(memBuffer + prefixScan[i + 1], layer->GetOutputSize());
		layer->InputDerivative(out, gradientOut, gradientIn);
		gradientIn.Swap(gradientOut);
	}
	inputGradients.Set(gradientOut);
}

//void ndBrain::CalculateInputGradientLoss(const ndBrainVector& input, const ndBrainVector& groundTruth, ndBrainVector& inputGradients)
//{
//	const ndArray<ndBrainLayer*>& layers = (*this);
//	ndAssert(layers.GetCount());
//	ndAssert(input.GetCount() == GetInputSize());
//	ndAssert(groundTruth.GetCount() == GetOutputSize());
//	ndAssert(inputGradients.GetCount() == GetInputSize());
//	
//	ndInt32 capacity = m_offsets[m_offsets.GetCount() - 1];
//	ndReal* const zBuff = ndAlloca(ndReal, capacity);
//	ndReal* const gBuff = ndAlloca(ndReal, capacity);
//	ndReal* const gradientBuffer = ndAlloca(ndReal, capacity);
//	ndReal* const hidden_zBuffer = ndAlloca(ndReal, capacity);
//	
//	ndBrainMemVector gradient(gradientBuffer, capacity);
//	ndBrainMemVector hidden_z(hidden_zBuffer, m_offsets[m_offsets.GetCount() - 1]);
//	
//	gradient.SetCount(groundTruth.GetCount());
//	MakePrediction(input, gradient, hidden_z);
//	gradient.Sub(groundTruth);
//	for (ndInt32 i = layers.GetCount() - 1; i >= 0; --i)
//	{
//		const ndBrainLayer* const layer = layers[i];
//		ndAssert(layer->m_weights.GetRows() == layer->GetOutputSize());
//		ndAssert(layer->m_weights.GetColumns() == layer->GetInputSize());
//	
//		ndBrainMemVector g(gBuff, layer->GetOutputSize());
//		ndBrainMemVector outGradient(zBuff, layer->GetInputSize());
//		ndBrainMemVector z(&hidden_z[m_offsets[i + 1]], layer->GetOutputSize());
//	
//		layer->ActivationDerivative(z, g);
//		g.Mul(gradient);
//		layer->m_weights.TransposeMul(g, outGradient);
//		
//		gradient.SetCount(outGradient.GetCount());
//		gradient.Set(outGradient);
//	}
//	ndAssert(inputGradients.GetCount() == gradient.GetCount());
//	inputGradients.Set(gradient);
//}