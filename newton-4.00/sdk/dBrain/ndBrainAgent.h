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

#ifndef _ND_BRAIN_AGENT_H__
#define _ND_BRAIN_AGENT_H__

#include "ndBrainStdafx.h"
#include "ndBrainMatrix.h"
#include "ndBrainInstance.h"
#include "ndBrainReplayBuffer.h"

class ndBrainAgent: public ndClassAlloc
{
	public: 
	ndBrainAgent();
	virtual ~ndBrainAgent();

	virtual void LearnStep() = 0;
	virtual bool IsTerminal() const = 0;
	virtual ndReal GetReward() const = 0;
	virtual void GetObservation(ndReal* const state) const = 0;
	virtual void GetAction(ndReal* const actions, ndReal exploreProbability) const = 0;
};

#endif 

