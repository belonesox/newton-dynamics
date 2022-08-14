/* Copyright (c) <2003-2021> <Julio Jerez, Newton Game Dynamics>
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

#include "ndDynamicsUpdateAvx2.h"

#define D_AVX_WORK_GROUP			8 
#define D_AVX_DEFAULT_BUFFER_SIZE	1024

#ifdef D_NEWTON_USE_DOUBLE
	D_MSV_NEWTON_ALIGN_32
	class ndAvxFloat
	{
		public:
		inline ndAvxFloat()
		{
		}

		inline ndAvxFloat(const ndFloat32 val)
			:m_low(_mm256_set1_pd(val))
			,m_high(_mm256_set1_pd(val))
		{
		}

		inline ndAvxFloat(const ndInt32 val)
			:m_low(_mm256_castsi256_pd(_mm256_set1_epi64x(ndInt64(val))))
			,m_high(_mm256_castsi256_pd(_mm256_set1_epi64x(ndInt64(val))))
		{
		}
				
		inline ndAvxFloat(const __m256d low, const __m256d high)
			:m_low(low)
			,m_high(high)
		{
		}

		inline ndAvxFloat(const ndAvxFloat& copy)
			:m_low(copy.m_low)
			,m_high(copy.m_high)
		{
		}

		#ifdef D_USE_VECTOR_AVX
			inline ndAvxFloat(const ndVector& low, const ndVector& high)
				:m_low(low.m_type)
				,m_high(high.m_type)
			{
			}
		#else
			inline ndAvxFloat(const ndVector& low, const ndVector& high)
				:m_low(_mm256_set_m128d(low.m_typeHigh, low.m_typeLow))
				,m_high(_mm256_set_m128d(high.m_typeHigh, high.m_typeLow))
			{
			}
		#endif

		inline ndAvxFloat(const ndAvxFloat* const baseAddr, const ndAvxFloat& index)
			:m_low(_mm256_i64gather_pd(&(*baseAddr)[0], index.m_lowInt, 8))
			,m_high(_mm256_i64gather_pd(&(*baseAddr)[0], index.m_highInt, 8))
		{
		}

		inline ndFloat32& operator[] (ndInt32 i)
		{
			ndAssert(i >= 0);
			ndAssert(i < D_AVX_WORK_GROUP);
			ndFloat32* const ptr = (ndFloat32*)&m_low;
			return ptr[i];
		}

		inline const ndFloat32& operator[] (ndInt32 i) const
		{
			ndAssert(i >= 0);
			ndAssert(i < D_AVX_WORK_GROUP);
			const ndFloat32* const ptr = (ndFloat32*)&m_low;
			return ptr[i];
		}

		inline ndAvxFloat operator+ (const ndAvxFloat& A) const
		{
			return ndAvxFloat(_mm256_add_pd(m_low, A.m_low), _mm256_add_pd(m_high, A.m_high));
		}

		inline ndAvxFloat operator- (const ndAvxFloat& A) const
		{
			return ndAvxFloat(_mm256_sub_pd(m_low, A.m_low), _mm256_sub_pd(m_high, A.m_high));
		}

		inline ndAvxFloat operator* (const ndAvxFloat& A) const
		{
			return ndAvxFloat(_mm256_mul_pd(m_low, A.m_low), _mm256_mul_pd(m_high, A.m_high));
		}

		inline ndAvxFloat MulAdd(const ndAvxFloat& A, const ndAvxFloat& B) const
		{
			return ndAvxFloat(_mm256_fmadd_pd(A.m_low, B.m_low, m_low), _mm256_fmadd_pd(A.m_high, B.m_high, m_high));
		}

		inline ndAvxFloat MulSub(const ndAvxFloat& A, const ndAvxFloat& B) const
		{
			return ndAvxFloat(_mm256_fnmadd_pd(A.m_low, B.m_low, m_low), _mm256_fnmadd_pd(A.m_high, B.m_high, m_high));
		}

		inline ndAvxFloat operator> (const ndAvxFloat& A) const
		{
			return ndAvxFloat(_mm256_cmp_pd(m_low, A.m_low, _CMP_GT_OQ), _mm256_cmp_pd(m_high, A.m_high, _CMP_GT_OQ));
		}

		inline ndAvxFloat operator< (const ndAvxFloat& A) const
		{
			return ndAvxFloat(_mm256_cmp_pd(m_low, A.m_low, _CMP_LT_OQ), _mm256_cmp_pd(m_high, A.m_high, _CMP_LT_OQ));
		}

		inline ndAvxFloat operator| (const ndAvxFloat& A) const
		{
			return ndAvxFloat(_mm256_or_pd(m_low, A.m_low), _mm256_or_pd(m_high, A.m_high));
		}

		inline ndAvxFloat operator& (const ndAvxFloat& A) const
		{
			return ndAvxFloat(_mm256_and_pd(m_low, A.m_low), _mm256_and_pd(m_high, A.m_high));
		}

		inline ndAvxFloat GetMin(const ndAvxFloat& A) const
		{
			return ndAvxFloat(_mm256_min_pd(m_low, A.m_low), _mm256_min_pd(m_high, A.m_high));
		}

		inline ndAvxFloat GetMax(const ndAvxFloat& A) const
		{
			return ndAvxFloat(_mm256_max_pd(m_low, A.m_low), _mm256_max_pd(m_high, A.m_high));
		}

		inline ndAvxFloat Select(const ndAvxFloat& data, const ndAvxFloat& mask) const
		{
			// (((b ^ a) & mask)^a)
			//return  _mm_or_ps (_mm_and_ps (mask.m_type, data.m_type), _mm_andnot_ps(mask.m_type, m_type));
			//return  _mm256_xor_ps(m_type, _mm256_and_ps(mask.m_type, _mm256_xor_ps(m_type, data.m_type)));
			__m256d low (_mm256_xor_pd(m_low, _mm256_and_pd(mask.m_low, _mm256_xor_pd(m_low, data.m_low))));
			__m256d high(_mm256_xor_pd(m_high, _mm256_and_pd(mask.m_high, _mm256_xor_pd(m_high, data.m_high))));
			return ndAvxFloat(low, high);
		}

		inline ndVector GetLow() const
		{
			return m_vector8.m_linear;
		}

		inline ndVector GetHigh() const
		{
			return m_vector8.m_angular;
		}

		inline ndFloat32 GetMax() const
		{
			__m256d tmp0(_mm256_max_pd(m_low, m_high));
			__m128d tmp1(_mm_max_pd(_mm256_castpd256_pd128(tmp0), _mm256_extractf128_pd(tmp0, 1)));
			__m128d tmp2(_mm_max_pd(tmp1, _mm_unpackhi_pd(tmp1, tmp1)));
			return _mm_cvtsd_f64(tmp2);
		}

		inline ndFloat32 AddHorizontal() const
		{
			__m256d tmp0(_mm256_add_pd(m_low, m_high));
			__m128d tmp1(_mm_add_pd(_mm256_castpd256_pd128(tmp0), _mm256_extractf128_pd(tmp0, 1)));
			__m128d tmp2(_mm_hadd_pd(tmp1, tmp1));
			return _mm_cvtsd_f64(tmp2);
		}

		static inline void FlushRegisters()
		{
			_mm256_zeroall();
		}

		union
		{
			struct
			{
				__m256d m_low;
				__m256d m_high;
			};
			struct
			{
				__m256i m_lowInt;
				__m256i m_highInt;
			};
			ndJacobian m_vector8;
			ndInt64 m_int[D_AVX_WORK_GROUP];
		};

		static ndAvxFloat m_one;
		static ndAvxFloat m_zero;
		static ndAvxFloat m_mask;
		static ndAvxFloat m_ordinals;
	} D_GCC_NEWTON_ALIGN_32;

#else
	D_MSV_NEWTON_ALIGN_32
	class ndAvxFloat
	{
		public:
		inline ndAvxFloat()
		{
		}

		inline ndAvxFloat(const ndFloat32 val)
			:m_type(_mm256_set1_ps(val))
		{
		}

		inline ndAvxFloat(const ndInt32 val)
			:m_type(_mm256_castsi256_ps(_mm256_set1_epi32(val)))
		{
		}

		inline ndAvxFloat(const __m256 type)
			: m_type(type)
		{
		}

		inline ndAvxFloat(const ndAvxFloat& copy)
			: m_type(copy.m_type)
		{
		}

		inline ndAvxFloat(const ndVector& low, const ndVector& high)
			#ifdef D_SCALAR_VECTOR_CLASS
			:m_type(_mm256_set_m128(_mm_set_ps(low.m_w, low.m_y, low.m_z, low.m_x), _mm_set_ps(high.m_w, high.m_y, high.m_z, high.m_x)))
			#else
			:m_type(_mm256_set_m128(high.m_type, low.m_type))
			#endif
		{
		}

		inline ndAvxFloat(const ndAvxFloat* const baseAddr, const ndAvxFloat& index)
			: m_type(_mm256_i32gather_ps(&(*baseAddr)[0], index.m_typeInt, 4))
		{
		}

		inline ndFloat32& operator[] (ndInt32 i)
		{
			ndAssert(i >= 0);
			ndAssert(i < D_AVX_WORK_GROUP);
			ndFloat32* const ptr = (ndFloat32*)&m_type;
			return ptr[i];
		}

		inline const ndFloat32& operator[] (ndInt32 i) const
		{
			ndAssert(i >= 0);
			ndAssert(i < D_AVX_WORK_GROUP);
			const ndFloat32* const ptr = (ndFloat32*)&m_type;
			return ptr[i];
		}

		inline ndAvxFloat operator+ (const ndAvxFloat& A) const
		{
			return _mm256_add_ps(m_type, A.m_type);
		}

		inline ndAvxFloat operator- (const ndAvxFloat& A) const
		{
			return _mm256_sub_ps(m_type, A.m_type);
		}

		inline ndAvxFloat operator* (const ndAvxFloat& A) const
		{
			return _mm256_mul_ps(m_type, A.m_type);
		}

		inline ndAvxFloat MulAdd(const ndAvxFloat& A, const ndAvxFloat& B) const
		{
			return _mm256_fmadd_ps(A.m_type, B.m_type, m_type);
		}

		inline ndAvxFloat MulSub(const ndAvxFloat& A, const ndAvxFloat& B) const
		{
			return _mm256_fnmadd_ps(A.m_type, B.m_type, m_type);
		}

		inline ndAvxFloat operator> (const ndAvxFloat& A) const
		{
			return _mm256_cmp_ps(m_type, A.m_type, _CMP_GT_OQ);
		}

		inline ndAvxFloat operator< (const ndAvxFloat& A) const
		{
			return _mm256_cmp_ps(m_type, A.m_type, _CMP_LT_OQ);
		}

		inline ndAvxFloat operator| (const ndAvxFloat& A) const
		{
			return _mm256_or_ps(m_type, A.m_type);
		}

		inline ndAvxFloat operator& (const ndAvxFloat& A) const
		{
			return _mm256_and_ps(m_type, A.m_type);
		}

		inline ndAvxFloat GetMin(const ndAvxFloat& A) const
		{
			return _mm256_min_ps(m_type, A.m_type);
		}

		inline ndAvxFloat GetMax(const ndAvxFloat& A) const
		{
			return _mm256_max_ps(m_type, A.m_type);
		}

		inline ndAvxFloat Select(const ndAvxFloat& data, const ndAvxFloat& mask) const
		{
			// (((b ^ a) & mask)^a)
			//return  _mm_or_ps (_mm_and_ps (mask.m_type, data.m_type), _mm_andnot_ps(mask.m_type, m_type));
			return  _mm256_xor_ps(m_type, _mm256_and_ps(mask.m_type, _mm256_xor_ps(m_type, data.m_type)));
		}

		inline ndVector GetLow() const
		{
			return _mm256_castps256_ps128(m_type);
		}

		inline ndVector GetHigh() const
		{
			return _mm256_extractf128_ps(m_type, 1);
		}

		inline ndFloat32 GetMax() const
		{
			__m128 tmp0(_mm_max_ps(_mm256_castps256_ps128(m_type), _mm256_extractf128_ps(m_type, 1)));
			__m128 tmp1(_mm_max_ps(tmp0, _mm_shuffle_ps(tmp0, tmp0, PERMUTE_MASK(1, 0, 3, 2))));
			__m128 tmp2(_mm_max_ps(tmp1, _mm_shuffle_ps(tmp1, tmp1, PERMUTE_MASK(2, 3, 0, 1))));
			return _mm_cvtss_f32(tmp2);
		}

		inline ndFloat32 AddHorizontal() const
		{
			__m128 tmp0(_mm_add_ps(_mm256_castps256_ps128(m_type), _mm256_extractf128_ps(m_type, 1)));
			__m128 tmp1(_mm_add_ps(tmp0, _mm_shuffle_ps(tmp0, tmp0, PERMUTE_MASK(1, 0, 3, 2))));
			__m128 tmp2(_mm_add_ps(tmp1, _mm_shuffle_ps(tmp1, tmp1, PERMUTE_MASK(2, 3, 0, 1))));
			return _mm_cvtss_f32(tmp2);
		}

		static inline void FlushRegisters()
		{
			_mm256_zeroall();
		}

		union
		{
			__m256 m_type;
			__m256i m_typeInt;
			ndJacobian m_vector8;
			ndInt32 m_int[D_AVX_WORK_GROUP];
		};

		static ndAvxFloat m_one;
		static ndAvxFloat m_zero;
		static ndAvxFloat m_mask;
		static ndAvxFloat m_ordinals;
	} D_GCC_NEWTON_ALIGN_32;
#endif

ndAvxFloat ndAvxFloat::m_one(ndFloat32(1.0f));
ndAvxFloat ndAvxFloat::m_zero(ndFloat32 (0.0f));
ndAvxFloat ndAvxFloat::m_ordinals(ndVector(0, 1, 2, 3), ndVector(4, 5, 6, 7));
ndAvxFloat ndAvxFloat::m_mask(ndVector(-1, -1, -1, -1), ndVector(-1, -1, -1, -1));

D_MSV_NEWTON_ALIGN_32
class ndAvxVector3
{
	public:
	ndAvxFloat m_x;
	ndAvxFloat m_y;
	ndAvxFloat m_z;
} D_GCC_NEWTON_ALIGN_32;

D_MSV_NEWTON_ALIGN_32
class ndAvxVector6
{
	public:
	ndAvxVector3 m_linear;
	ndAvxVector3 m_angular;
} D_GCC_NEWTON_ALIGN_32;

D_MSV_NEWTON_ALIGN_32
class ndOpenclJacobianPair
{
	public:
	ndAvxVector6 m_jacobianM0;
	ndAvxVector6 m_jacobianM1;
}D_GCC_NEWTON_ALIGN_32;

D_MSV_NEWTON_ALIGN_32
class ndSoaMatrixElement
{
	public:
	ndOpenclJacobianPair m_Jt;
	ndOpenclJacobianPair m_JMinv;

	ndAvxFloat m_force;
	ndAvxFloat m_diagDamp;
	ndAvxFloat m_invJinvMJt;
	ndAvxFloat m_coordenateAccel;
	ndAvxFloat m_normalForceIndex;
	ndAvxFloat m_lowerBoundFrictionCoefficent;
	ndAvxFloat m_upperBoundFrictionCoefficent;
} D_GCC_NEWTON_ALIGN_32;

class ndAvxMatrixArray : public ndArray<ndSoaMatrixElement>
{
};

ndDynamicsUpdateAvx2::ndDynamicsUpdateAvx2(ndWorld* const world)
	:ndDynamicsUpdate(world)
	,m_groupType(D_AVX_DEFAULT_BUFFER_SIZE)
	,m_jointMask(D_AVX_DEFAULT_BUFFER_SIZE)
	,m_avxJointRows(D_AVX_DEFAULT_BUFFER_SIZE)
	,m_avxMassMatrixArray(new ndAvxMatrixArray)
{
}

ndDynamicsUpdateAvx2::~ndDynamicsUpdateAvx2()
{
	Clear();
	m_jointMask.Resize(D_AVX_DEFAULT_BUFFER_SIZE);
	m_groupType.Resize(D_AVX_DEFAULT_BUFFER_SIZE);
	m_avxJointRows.Resize(D_AVX_DEFAULT_BUFFER_SIZE);
	delete m_avxMassMatrixArray;
}

const char* ndDynamicsUpdateAvx2::GetStringId() const
{
	return "avx2";
}

void ndDynamicsUpdateAvx2::DetermineSleepStates()
{
	D_TRACKTIME();
	auto CalculateSleepState = ndMakeObject::ndFunction([this](ndInt32 threadIndex, ndInt32 threadCount)
	{
		D_TRACKTIME_NAMED(CalculateSleepState);
		ndScene* const scene = m_world->GetScene();
		const ndArray<ndInt32>& bodyIndex = GetJointForceIndexBuffer();
		const ndJointBodyPairIndex* const jointBodyPairIndexBuffer = &GetJointBodyPairIndexBuffer()[0];
		ndConstraint** const jointArray = &scene->GetActiveContactArray()[0];
		ndBodyKinematic** const bodyArray = &scene->GetActiveBodyArray()[0];

		const ndVector zero(ndVector::m_zero);
		const ndStartEnd startEnd(bodyIndex.GetCount() - 1, threadIndex, threadCount);
		for (ndInt32 i = startEnd.m_start; i < startEnd.m_end; ++i)
		{
			const ndInt32 index = bodyIndex[i];
			ndBodyKinematic* const body = bodyArray[jointBodyPairIndexBuffer[index].m_body];
			ndAssert(body->m_isStatic <= 1);
			ndAssert(body->m_index == jointBodyPairIndexBuffer[index].m_body);
			const ndInt32 mask = ndInt32(body->m_isStatic) - 1;
			const ndInt32 count = mask & (bodyIndex[i + 1] - index);
			if (count)
			{
				ndUnsigned8 equilibrium = body->m_isJointFence0;
				if (equilibrium & body->m_autoSleep)
				{
					for (ndInt32 j = 0; j < count; ++j)
					{
						const ndJointBodyPairIndex& scan = jointBodyPairIndexBuffer[index + j];
						ndConstraint* const joint = jointArray[scan.m_joint >> 1];
						ndBodyKinematic* const body1 = (joint->GetBody0() == body) ? joint->GetBody1() : joint->GetBody0();
						ndAssert(body1 != body);
						equilibrium = equilibrium & body1->m_isJointFence0;
					}
				}
				body->m_equilibrium = equilibrium & body->m_autoSleep;
				if (body->m_equilibrium)
				{
					body->m_veloc = zero;
					body->m_omega = zero;
				}
			}
		}
	});

	ndScene* const scene = m_world->GetScene();
	if (scene->GetActiveContactArray().GetCount())
	{
		scene->ParallelExecute(CalculateSleepState);
	}
}

void ndDynamicsUpdateAvx2::SortJoints()
{
	D_TRACKTIME();
	SortJointsScan();
	if (!m_activeJointCount)
	{
		return;
	}

	ndScene* const scene = m_world->GetScene();
	ndArray<ndConstraint*>& jointArray = scene->GetActiveContactArray();

	#ifdef _DEBUG
		for (ndInt32 i = 1; i < m_activeJointCount; ++i)
		{
			ndConstraint* const joint0 = jointArray[i - 1];
			ndConstraint* const joint1 = jointArray[i - 0];
			ndAssert(!joint0->m_resting);
			ndAssert(!joint1->m_resting);
			ndAssert(joint0->m_rowCount >= joint1->m_rowCount);
			ndAssert(!(joint0->GetBody0()->m_equilibrium0 & joint0->GetBody1()->m_equilibrium0));
			ndAssert(!(joint1->GetBody0()->m_equilibrium0 & joint1->GetBody1()->m_equilibrium0));
		}

		for (ndInt32 i = m_activeJointCount + 1; i < jointArray.GetCount(); ++i)
		{
			ndConstraint* const joint0 = jointArray[i - 1];
			ndConstraint* const joint1 = jointArray[i - 0];
			ndAssert(joint0->m_resting);
			ndAssert(joint1->m_resting);
			ndAssert(joint0->m_rowCount >= joint1->m_rowCount);
			ndAssert(joint0->GetBody0()->m_equilibrium0 & joint0->GetBody1()->m_equilibrium0);
			ndAssert(joint1->GetBody0()->m_equilibrium0 & joint1->GetBody1()->m_equilibrium0);
		}
	#endif

	const ndInt32 mask = -ndInt32(D_AVX_WORK_GROUP);
	const ndInt32 jointCount = jointArray.GetCount();
	const ndInt32 soaJointCount = (jointCount + D_AVX_WORK_GROUP - 1) & mask;
	ndAssert(jointArray.GetCapacity() > soaJointCount);
	ndConstraint** const jointArrayPtr = &jointArray[0];
	for (ndInt32 i = jointCount; i < soaJointCount; ++i)
	{
		jointArrayPtr[i] = nullptr;
	}

	if (m_activeJointCount - jointArray.GetCount())
	{
		const ndInt32 base = m_activeJointCount & mask;
		const ndInt32 count = jointArrayPtr[base + D_AVX_WORK_GROUP - 1] ? D_AVX_WORK_GROUP : jointArray.GetCount() - base;
		ndAssert(count <= D_AVX_WORK_GROUP);
		ndConstraint** const array = &jointArrayPtr[base];
		for (ndInt32 j = 1; j < count; ++j)
		{
			ndInt32 slot = j;
			ndConstraint* const joint = array[slot];
			for (; (slot > 0) && (array[slot - 1]->m_rowCount < joint->m_rowCount); slot--)
			{
				array[slot] = array[slot - 1];
			}
			array[slot] = joint;
		}
	}

	const ndInt32 soaJointCountBatches = soaJointCount / D_AVX_WORK_GROUP;
	m_jointMask.SetCount(soaJointCountBatches);
	m_groupType.SetCount(soaJointCountBatches);
	m_avxJointRows.SetCount(soaJointCountBatches);
	
	ndInt32 rowsCount = 0;
	ndInt32 soaJointRowCount = 0;
	auto SetRowStarts = ndMakeObject::ndFunction([this, &jointArray, &rowsCount, &soaJointRowCount](ndInt32 threadIndex, ndInt32 threadCount)
	{
		D_TRACKTIME_NAMED(SetRowStarts);
		auto SetRowsCount = [this, &jointArray, &rowsCount]()
		{
			ndInt32 rowCount = 1;
			const ndInt32 count = jointArray.GetCount();
			for (ndInt32 i = 0; i < count; ++i)
			{
				ndConstraint* const joint = jointArray[i];
				joint->m_rowStart = rowCount;
				rowCount += joint->m_rowCount;
			}
			rowsCount = rowCount;
		};

		auto SetSoaRowsCount = [this, &jointArray, &soaJointRowCount]()
		{
			ndInt32 rowCount = 0;
			ndArray<ndInt32>& soaJointRows = m_avxJointRows;
			const ndInt32 count = soaJointRows.GetCount();
			for (ndInt32 i = 0; i < count; ++i)
			{
				const ndConstraint* const joint = jointArray[i * D_AVX_WORK_GROUP];
				soaJointRows[i] = rowCount;
				rowCount += joint->m_rowCount;
			}
			soaJointRowCount = rowCount;
		};

		if (threadCount == 1)
		{
			SetRowsCount();
			SetSoaRowsCount();
		}
		else if (threadIndex == 0)
		{
			SetRowsCount();
		}
		else if (threadIndex == (threadCount - 1))
		{
			SetSoaRowsCount();
		}
	});
	scene->ParallelExecute(SetRowStarts);

	m_leftHandSide.SetCount(rowsCount);
	m_rightHandSide.SetCount(rowsCount);
	m_avxMassMatrixArray->SetCount(soaJointRowCount);

	#ifdef _DEBUG
		ndAssert(m_activeJointCount <= jointArray.GetCount());
		const ndInt32 maxRowCount = m_leftHandSide.GetCount();
		for (ndInt32 i = 0; i < jointArray.GetCount(); ++i)
		{
			ndConstraint* const joint = jointArray[i];
			ndAssert(joint->m_rowStart < m_leftHandSide.GetCount());
			ndAssert((joint->m_rowStart + joint->m_rowCount) <= maxRowCount);
		}

		for (ndInt32 i = 0; i < jointCount; i += D_AVX_WORK_GROUP)
		{
			const ndInt32 count = jointArrayPtr[i + D_AVX_WORK_GROUP - 1] ? D_AVX_WORK_GROUP : jointCount - i;
			for (ndInt32 j = 1; j < count; ++j)
			{
				ndConstraint* const joint0 = jointArrayPtr[i + j - 1];
				ndConstraint* const joint1 = jointArrayPtr[i + j - 0];
				ndAssert(joint0->m_rowCount >= joint1->m_rowCount);
			}
		}
	#endif
	SortBodyJointScan();
}

void ndDynamicsUpdateAvx2::SortIslands()
{
	D_TRACKTIME();
	ndScene* const scene = m_world->GetScene();
	const ndArray<ndBodyKinematic*>& bodyArray = scene->GetActiveBodyArray();
	ndArray<ndBodyKinematic*>& activeBodyArray = GetBodyIslandOrder();
	GetInternalForces().SetCount(bodyArray.GetCount());
	activeBodyArray.SetCount(bodyArray.GetCount());

	ndInt32 histogram[D_MAX_THREADS_COUNT][3];
	auto Scan0 = ndMakeObject::ndFunction([this, &bodyArray, &histogram](ndInt32 threadIndex, ndInt32 threadCount)
	{
		D_TRACKTIME_NAMED(Scan0);
		ndInt32* const hist = &histogram[threadIndex][0];
		hist[0] = 0;
		hist[1] = 0;
		hist[2] = 0;

		ndInt32 map[4];
		map[0] = 0;
		map[1] = 1;
		map[2] = 2;
		map[3] = 2;
		const ndStartEnd startEnd(bodyArray.GetCount(), threadIndex, threadCount);
		for (ndInt32 i = startEnd.m_start; i < startEnd.m_end; ++i)
		{
			ndBodyKinematic* const body = bodyArray[i];
			ndInt32 key = map[body->m_equilibrium0 * 2 + 1 - body->m_isConstrained];
			ndAssert(key < 3);
			hist[key] = hist[key] + 1;
		}
	});

	auto Sort0 = ndMakeObject::ndFunction([this, &bodyArray, &activeBodyArray, &histogram](ndInt32 threadIndex, ndInt32 threadCount)
	{
		D_TRACKTIME_NAMED(Sort0);
		ndInt32* const hist = &histogram[threadIndex][0];
		const ndStartEnd startEnd(bodyArray.GetCount(), threadIndex, threadCount);

		ndInt32 map[4];
		map[0] = 0;
		map[1] = 1;
		map[2] = 2;
		map[3] = 2;
		for (ndInt32 i = startEnd.m_start; i < startEnd.m_end; ++i)
		{
			ndBodyKinematic* const body = bodyArray[i];
			ndInt32 key = map[body->m_equilibrium0 * 2 + 1 - body->m_isConstrained];
			ndAssert(key < 3);
			const ndInt32 entry = hist[key];
			activeBodyArray[entry] = body;
			hist[key] = entry + 1;
		}
	});

	scene->ParallelExecute(Scan0);

	ndInt32 scan[3];
	scan[0] = 0;
	scan[1] = 0;
	scan[2] = 0;
	const ndInt32 threadCount = scene->GetThreadCount();

	ndInt32 sum = 0;
	for (ndInt32 i = 0; i < 3; ++i)
	{
		for (ndInt32 j = 0; j < threadCount; ++j)
		{
			ndInt32 partialSum = histogram[j][i];
			histogram[j][i] = sum;
			sum += partialSum;
		}
		scan[i] = sum;
	}

	scene->ParallelExecute(Sort0);
	activeBodyArray.SetCount(scan[1]);
	m_unConstrainedBodyCount = scan[1] - scan[0];
}

void ndDynamicsUpdateAvx2::BuildIsland()
{
	m_unConstrainedBodyCount = 0;
	GetBodyIslandOrder().SetCount(0);
	ndScene* const scene = m_world->GetScene();
	const ndArray<ndBodyKinematic*>& bodyArray = scene->GetActiveBodyArray();
	ndAssert(bodyArray.GetCount() >= 1);
	if (bodyArray.GetCount() - 1)
	{
		D_TRACKTIME();
		SortJoints();
		SortIslands();
	}
}

void ndDynamicsUpdateAvx2::IntegrateUnconstrainedBodies()
{
	ndScene* const scene = m_world->GetScene();
	auto IntegrateUnconstrainedBodies = ndMakeObject::ndFunction([this, &scene](ndInt32 threadIndex, ndInt32 threadCount)
	{
		D_TRACKTIME_NAMED(IntegrateUnconstrainedBodies);
		ndArray<ndBodyKinematic*>& bodyArray = GetBodyIslandOrder();

		const ndFloat32 timestep = scene->GetTimestep();
		const ndInt32 base = bodyArray.GetCount() - GetUnconstrainedBodyCount();

		const ndStartEnd startEnd(GetUnconstrainedBodyCount(), threadIndex, threadCount);
		for (ndInt32 i = startEnd.m_start; i < startEnd.m_end; ++i)
		{
			ndBodyKinematic* const body = bodyArray[base + i];
			ndAssert(body);
			body->UpdateInvInertiaMatrix();
			body->AddDampingAcceleration(timestep);
			body->IntegrateExternalForce(timestep);
		}
	});

	if (GetUnconstrainedBodyCount())
	{
		D_TRACKTIME();
		scene->ParallelExecute(IntegrateUnconstrainedBodies);
	}
}

void ndDynamicsUpdateAvx2::IntegrateBodies()
{
	D_TRACKTIME();
	ndScene* const scene = m_world->GetScene();
	const ndVector invTime(m_invTimestep);
	const ndFloat32 timestep = scene->GetTimestep();

	auto IntegrateBodies = ndMakeObject::ndFunction([this, timestep, invTime](ndInt32 threadIndex, ndInt32 threadCount)
	{
		D_TRACKTIME_NAMED(IntegrateBodies);
		const ndWorld* const world = m_world;
		const ndArray<ndBodyKinematic*>& bodyArray = GetBodyIslandOrder();
		const ndStartEnd startEnd(bodyArray.GetCount(), threadIndex, threadCount);

		const ndFloat32 speedFreeze2 = world->m_freezeSpeed2;
		const ndFloat32 accelFreeze2 = world->m_freezeAccel2;
		for (ndInt32 i = startEnd.m_start; i < startEnd.m_end; ++i)
		{
			ndBodyKinematic* const body = bodyArray[i];
			if (!body->m_equilibrium)
			{
				body->m_accel = invTime * (body->m_veloc - body->m_accel);
				body->m_alpha = invTime * (body->m_omega - body->m_alpha);
				body->IntegrateVelocity(timestep);
			}
			body->EvaluateSleepState(speedFreeze2, accelFreeze2);
		}
	});
	scene->ParallelExecute(IntegrateBodies);
}

void ndDynamicsUpdateAvx2::InitWeights()
{
	D_TRACKTIME();
	ndScene* const scene = m_world->GetScene();
	m_invTimestep = ndFloat32(1.0f) / m_timestep;
	m_invStepRK = ndFloat32(0.25f);
	m_timestepRK = m_timestep * m_invStepRK;
	m_invTimestepRK = m_invTimestep * ndFloat32(4.0f);

	const ndArray<ndBodyKinematic*>& bodyArray = scene->GetActiveBodyArray();
	const ndInt32 bodyCount = bodyArray.GetCount();
	GetInternalForces().SetCount(bodyCount);

	ndInt32 extraPassesArray[D_MAX_THREADS_COUNT];

	auto InitWeights = ndMakeObject::ndFunction([this, &bodyArray, &extraPassesArray](ndInt32 threadIndex, ndInt32 threadCount)
	{
		D_TRACKTIME_NAMED(InitWeights);
		const ndArray<ndInt32>& jointForceIndexBuffer = GetJointForceIndexBuffer();
		const ndArray<ndJointBodyPairIndex>& jointBodyPairIndex = GetJointBodyPairIndexBuffer();

		ndInt32 maxExtraPasses = 1;
		const ndStartEnd startEnd(jointForceIndexBuffer.GetCount() - 1, threadIndex, threadCount);
		for (ndInt32 i = startEnd.m_start; i < startEnd.m_end; ++i)
		{
			const ndInt32 index = jointForceIndexBuffer[i];
			const ndJointBodyPairIndex& scan = jointBodyPairIndex[index];
			ndBodyKinematic* const body = bodyArray[scan.m_body];
			ndAssert(body->m_index == scan.m_body);
			ndAssert(body->m_isConstrained <= 1);
			const ndInt32 count = jointForceIndexBuffer[i + 1] - index - 1;
			const ndInt32 mask = -ndInt32(body->m_isConstrained & ~body->m_isStatic);
			const ndInt32 weigh = 1 + (mask & count);
			ndAssert(weigh >= 0);
			if (weigh)
			{
				body->m_weigh = ndFloat32(weigh);
			}
			maxExtraPasses = ndMax(weigh, maxExtraPasses);
		}
		extraPassesArray[threadIndex] = maxExtraPasses;
	});

	if (scene->GetActiveContactArray().GetCount())
	{

		scene->ParallelExecute(InitWeights);

		ndInt32 extraPasses = 0;
		const ndInt32 threadCount = scene->GetThreadCount();
		for (ndInt32 i = 0; i < threadCount; ++i)
		{
			extraPasses = ndMax(extraPasses, extraPassesArray[i]);
		}

		const ndInt32 conectivity = 7;
		m_solverPasses = m_world->GetSolverIterations() + 2 * extraPasses / conectivity + 2;
	}
}

void ndDynamicsUpdateAvx2::InitBodyArray()
{
	D_TRACKTIME();
	ndScene* const scene = m_world->GetScene();
	const ndFloat32 timestep = scene->GetTimestep();

	auto InitBodyArray = ndMakeObject::ndFunction([this, timestep](ndInt32 threadIndex, ndInt32 threadCount)
	{
		D_TRACKTIME_NAMED(InitBodyArray);
		const ndArray<ndBodyKinematic*>& bodyArray = GetBodyIslandOrder();
		const ndStartEnd startEnd(bodyArray.GetCount() - GetUnconstrainedBodyCount(), threadIndex, threadCount);
		for (ndInt32 i = startEnd.m_start; i < startEnd.m_end; ++i)
		{
			ndBodyKinematic* const body = bodyArray[i];
			ndAssert(body);
			ndAssert(body->m_isConstrained | body->m_isStatic);

			body->UpdateInvInertiaMatrix();
			body->AddDampingAcceleration(timestep);
			const ndVector angularMomentum(body->CalculateAngularMomentum());
			body->m_gyroTorque = body->m_omega.CrossProduct(angularMomentum);
			body->m_gyroAlpha = body->m_invWorldInertiaMatrix.RotateVector(body->m_gyroTorque);

			body->m_accel = body->m_veloc;
			body->m_alpha = body->m_omega;
			body->m_gyroRotation = body->m_rotation;
		}
	});
	scene->ParallelExecute(InitBodyArray);
}

void ndDynamicsUpdateAvx2::GetJacobianDerivatives(ndConstraint* const joint)
{
	ndConstraintDescritor constraintParam;
	ndAssert(joint->GetRowsCount() <= D_CONSTRAINT_MAX_ROWS);
	for (ndInt32 i = joint->GetRowsCount() - 1; i >= 0; i--)
	{
		constraintParam.m_forceBounds[i].m_low = D_MIN_BOUND;
		constraintParam.m_forceBounds[i].m_upper = D_MAX_BOUND;
		constraintParam.m_forceBounds[i].m_jointForce = nullptr;
		constraintParam.m_forceBounds[i].m_normalIndex = D_INDEPENDENT_ROW;
	}

	constraintParam.m_rowsCount = 0;
	constraintParam.m_timestep = m_timestep;
	constraintParam.m_invTimestep = m_invTimestep;
	joint->JacobianDerivative(constraintParam);
	const ndInt32 dof = constraintParam.m_rowsCount;
	ndAssert(dof <= joint->m_rowCount);

	if (joint->GetAsContact())
	{
		ndContact* const contactJoint = joint->GetAsContact();
		contactJoint->m_isInSkeletonLoop = 0;
		ndSkeletonContainer* const skeleton0 = contactJoint->GetBody0()->GetSkeleton();
		ndSkeletonContainer* const skeleton1 = contactJoint->GetBody1()->GetSkeleton();
		if (skeleton0 && (skeleton0 == skeleton1))
		{
			if (contactJoint->IsSkeletonSelftCollision())
			{
				contactJoint->m_isInSkeletonLoop = 1;
				skeleton0->AddCloseLoopJoint(contactJoint);
			}
		}
		else if (contactJoint->IsSkeletonIntraCollision())
		{
			if (skeleton0 && !skeleton1)
			{
				contactJoint->m_isInSkeletonLoop = 1;
				skeleton0->AddCloseLoopJoint(contactJoint);
			}
			else if (skeleton1 && !skeleton0)
			{
				contactJoint->m_isInSkeletonLoop = 1;
				skeleton1->AddCloseLoopJoint(contactJoint);
			}
		}
	}
	else
	{
		ndJointBilateralConstraint* const bilareral = joint->GetAsBilateral();
		ndAssert(bilareral);
		if (!bilareral->m_isInSkeleton && (bilareral->GetSolverModel() == m_jointkinematicAttachment))
		{
			ndSkeletonContainer* const skeleton0 = bilareral->m_body0->GetSkeleton();
			ndSkeletonContainer* const skeleton1 = bilareral->m_body1->GetSkeleton();
			if (skeleton0 || skeleton1)
			{
				if (skeleton0 && !skeleton1)
				{
					bilareral->m_isInSkeletonLoop = 1;
					skeleton0->AddCloseLoopJoint(bilareral);
				}
				else if (skeleton1 && !skeleton0)
				{
					bilareral->m_isInSkeletonLoop = 1;
					skeleton1->AddCloseLoopJoint(bilareral);
				}
			}
		}
	}

	joint->m_rowCount = dof;
	const ndInt32 baseIndex = joint->m_rowStart;
	for (ndInt32 i = 0; i < dof; ++i)
	{
		ndAssert(constraintParam.m_forceBounds[i].m_jointForce);

		ndLeftHandSide* const row = &m_leftHandSide[baseIndex + i];
		ndRightHandSide* const rhs = &m_rightHandSide[baseIndex + i];

		row->m_Jt = constraintParam.m_jacobian[i];
		rhs->m_diagDamp = ndFloat32(0.0f);
		rhs->m_diagonalRegularizer = ndMax(constraintParam.m_diagonalRegularizer[i], ndFloat32(1.0e-5f));

		rhs->m_coordenateAccel = constraintParam.m_jointAccel[i];
		rhs->m_restitution = constraintParam.m_restitution[i];
		rhs->m_penetration = constraintParam.m_penetration[i];
		rhs->m_penetrationStiffness = constraintParam.m_penetrationStiffness[i];
		rhs->m_lowerBoundFrictionCoefficent = constraintParam.m_forceBounds[i].m_low;
		rhs->m_upperBoundFrictionCoefficent = constraintParam.m_forceBounds[i].m_upper;
		rhs->m_jointFeebackForce = constraintParam.m_forceBounds[i].m_jointForce;

		ndAssert(constraintParam.m_forceBounds[i].m_normalIndex >= -1);
		rhs->m_normalForceIndex = constraintParam.m_forceBounds[i].m_normalIndex;
	}
}

void ndDynamicsUpdateAvx2::InitJacobianMatrix()
{
	ndScene* const scene = m_world->GetScene();
	ndBodyKinematic** const bodyArray = &scene->GetActiveBodyArray()[0];
	ndArray<ndConstraint*>& jointArray = scene->GetActiveContactArray();

	auto InitJacobianMatrix = ndMakeObject::ndFunction([this, &jointArray](ndInt32 threadIndex, ndInt32 threadCount)
	{
		D_TRACKTIME_NAMED(InitJacobianMatrix);
		ndAvxFloat* const internalForces = (ndAvxFloat*)&GetTempInternalForces()[0];
		auto BuildJacobianMatrix = [this, &internalForces](ndConstraint* const joint, ndInt32 jointIndex)
		{
			ndAssert(joint->GetBody0());
			ndAssert(joint->GetBody1());
			const ndBodyKinematic* const body0 = joint->GetBody0();
			const ndBodyKinematic* const body1 = joint->GetBody1();

			ndAvxFloat force0(body0->GetForce(), body0->GetTorque());
			ndAvxFloat force1(body1->GetForce(), body1->GetTorque());

			const ndInt32 index = joint->m_rowStart;
			const ndInt32 count = joint->m_rowCount;

			const bool isBilateral = joint->IsBilateral();

			const ndMatrix& invInertia0 = body0->m_invWorldInertiaMatrix;
			const ndMatrix& invInertia1 = body1->m_invWorldInertiaMatrix;
			const ndVector invMass0(body0->m_invMass[3]);
			const ndVector invMass1(body1->m_invMass[3]);

			#ifdef D_JOINT_PRECONDITIONER
			joint->m_preconditioner0 = ndFloat32(1.0f);
			joint->m_preconditioner1 = ndFloat32(1.0f);

			const bool test = !((body0->m_isStatic | body1->m_isStatic) || (body0->GetSkeleton() && body1->GetSkeleton()));
			ndAssert(test == ((invMass0.GetScalar() > ndFloat32(0.0f)) && (invMass1.GetScalar() > ndFloat32(0.0f)) && !(body0->GetSkeleton() && body1->GetSkeleton())));
			if (test)
			{
				const ndFloat32 mass0 = body0->GetMassMatrix().m_w;
				const ndFloat32 mass1 = body1->GetMassMatrix().m_w;
				if (mass0 > (D_DIAGONAL_PRECONDITIONER * mass1))
				{
					joint->m_preconditioner0 = mass0 / (mass1 * D_DIAGONAL_PRECONDITIONER);
				}
				else if (mass1 > (D_DIAGONAL_PRECONDITIONER * mass0))
				{
					joint->m_preconditioner1 = mass1 / (mass0 * D_DIAGONAL_PRECONDITIONER);
				}
			}
			#endif

			ndAvxFloat forceAcc0(ndAvxFloat::m_zero);
			ndAvxFloat forceAcc1(ndAvxFloat::m_zero);

			#ifdef D_JOINT_PRECONDITIONER
			const ndAvxFloat weigh0(body0->m_weigh * joint->m_preconditioner0);
			const ndAvxFloat weigh1(body1->m_weigh * joint->m_preconditioner1);

			const ndFloat32 preconditioner0 = joint->m_preconditioner0;
			const ndFloat32 preconditioner1 = joint->m_preconditioner1;
			#else
			const ndAvxFloat weigh0(body0->m_weigh);
			const ndAvxFloat weigh1(body1->m_weigh);
			#endif

			for (ndInt32 i = 0; i < count; ++i)
			{
				ndLeftHandSide* const row = &m_leftHandSide[index + i];
				ndRightHandSide* const rhs = &m_rightHandSide[index + i];

				row->m_JMinv.m_jacobianM0.m_linear = row->m_Jt.m_jacobianM0.m_linear * invMass0;
				row->m_JMinv.m_jacobianM0.m_angular = invInertia0.RotateVector(row->m_Jt.m_jacobianM0.m_angular);
				row->m_JMinv.m_jacobianM1.m_linear = row->m_Jt.m_jacobianM1.m_linear * invMass1;
				row->m_JMinv.m_jacobianM1.m_angular = invInertia1.RotateVector(row->m_Jt.m_jacobianM1.m_angular);

				const ndAvxFloat& JMinvM0 = (ndAvxFloat&)row->m_JMinv.m_jacobianM0;
				const ndAvxFloat& JMinvM1 = (ndAvxFloat&)row->m_JMinv.m_jacobianM1;

				const ndAvxFloat tmpAccel((JMinvM0 * force0).MulAdd(JMinvM1, force1));

				ndFloat32 extenalAcceleration = -tmpAccel.AddHorizontal();
				rhs->m_deltaAccel = extenalAcceleration;
				rhs->m_coordenateAccel += extenalAcceleration;
				ndAssert(rhs->m_jointFeebackForce);
				const ndFloat32 force = rhs->m_jointFeebackForce->GetInitialGuess();

				rhs->m_force = isBilateral ? ndClamp(force, rhs->m_lowerBoundFrictionCoefficent, rhs->m_upperBoundFrictionCoefficent) : force;
				rhs->m_maxImpact = ndFloat32(0.0f);

				const ndAvxFloat& JtM0 = (ndAvxFloat&)row->m_Jt.m_jacobianM0;
				const ndAvxFloat& JtM1 = (ndAvxFloat&)row->m_Jt.m_jacobianM1;
				const ndAvxFloat tmpDiag(weigh0 * JMinvM0 * JtM0 + weigh1 * JMinvM1 * JtM1);

				ndFloat32 diag = tmpDiag.AddHorizontal();
				ndAssert(diag > ndFloat32(0.0f));
				rhs->m_diagDamp = diag * rhs->m_diagonalRegularizer;

				diag *= (ndFloat32(1.0f) + rhs->m_diagonalRegularizer);
				rhs->m_invJinvMJt = ndFloat32(1.0f) / diag;

				#ifdef D_JOINT_PRECONDITIONER
				forceAcc0 = forceAcc0.MulAdd(JtM0, ndAvxFloat(rhs->m_force * preconditioner0));
				forceAcc1 = forceAcc1.MulAdd(JtM1, ndAvxFloat(rhs->m_force * preconditioner1));
				#else
				forceAcc0 = forceAcc0.MulAdd(JtM0, ndAvxFloat(rhs->m_force));
				forceAcc1 = forceAcc1.MulAdd(JtM1, ndAvxFloat(rhs->m_force));
				#endif
			}

			const ndInt32 index0 = jointIndex * 2 + 0;
			ndAvxFloat& outBody0 = internalForces[index0];
			outBody0 = forceAcc0;

			const ndInt32 index1 = jointIndex * 2 + 1;
			ndAvxFloat& outBody1 = internalForces[index1];
			outBody1 = forceAcc1;
		};

		const ndInt32 jointCount = jointArray.GetCount();
		for (ndInt32 i = threadIndex; i < jointCount; i += threadCount)
		{
			ndConstraint* const joint = jointArray[i];
			GetJacobianDerivatives(joint);
			BuildJacobianMatrix(joint, i);
		}
	});

	auto InitJacobianAccumulatePartialForces = ndMakeObject::ndFunction([this, &bodyArray](ndInt32 threadIndex, ndInt32 threadCount)
	{
		D_TRACKTIME_NAMED(InitJacobianAccumulatePartialForces);
		const ndVector zero(ndVector::m_zero);
		ndJacobian* const internalForces = &GetInternalForces()[0];
		const ndArray<ndInt32>& bodyIndex = GetJointForceIndexBuffer();

		const ndJacobian* const jointInternalForces = &GetTempInternalForces()[0];
		const ndJointBodyPairIndex* const jointBodyPairIndexBuffer = &GetJointBodyPairIndexBuffer()[0];

		const ndStartEnd startEnd(bodyIndex.GetCount() - 1, threadIndex, threadCount);
		for (ndInt32 i = startEnd.m_start; i < startEnd.m_end; ++i)
		{
			ndVector force(zero);
			ndVector torque(zero);

			const ndInt32 index = bodyIndex[i];
			const ndJointBodyPairIndex& scan = jointBodyPairIndexBuffer[index];
			ndBodyKinematic* const body = bodyArray[scan.m_body];

			ndAssert(body->m_isStatic <= 1);
			ndAssert(body->m_index == scan.m_body);
			const ndInt32 mask = ndInt32(body->m_isStatic) - 1;
			const ndInt32 count = mask & (bodyIndex[i + 1] - index);

			for (ndInt32 j = 0; j < count; ++j)
			{
				const ndInt32 jointIndex = jointBodyPairIndexBuffer[index + j].m_joint;
				force += jointInternalForces[jointIndex].m_linear;
				torque += jointInternalForces[jointIndex].m_angular;
			}
			internalForces[i].m_linear = force;
			internalForces[i].m_angular = torque;
		}
	});

	auto TransposeMassMatrix = ndMakeObject::ndFunction([this, &jointArray](ndInt32 threadIndex, ndInt32 threadCount)
	{
		D_TRACKTIME_NAMED(TransposeMassMatrix);
		const ndInt32 jointCount = jointArray.GetCount();

		const ndLeftHandSide* const leftHandSide = &GetLeftHandSide()[0];
		const ndRightHandSide* const rightHandSide = &GetRightHandSide()[0];
		ndAvxMatrixArray& massMatrix = *m_avxMassMatrixArray;

		const ndAvxFloat zero(ndAvxFloat::m_zero);
		const ndAvxFloat ordinals(ndAvxFloat::m_ordinals);
		const ndInt32 mask = -ndInt32(D_AVX_WORK_GROUP);
		const ndInt32 soaJointCount = ((jointCount + D_AVX_WORK_GROUP - 1) & mask) / D_AVX_WORK_GROUP;

		ndInt8* const groupType = &m_groupType[0];
		ndAvxFloat* const jointMask = (ndAvxFloat*)&m_jointMask[0];
		const ndInt32* const soaJointRows = &m_avxJointRows[0];

		ndConstraint** const jointsPtr = &jointArray[0];
		for (ndInt32 i = threadIndex; i < soaJointCount; i += threadCount)
		{
			const ndInt32 index = i * D_AVX_WORK_GROUP;
			ndInt32 maxRow = 0;
			ndInt32 minRow = 255;
			ndAvxFloat selectMask(-1);
			for (ndInt32 j = 0; j < D_AVX_WORK_GROUP; ++j)
			{
				ndConstraint* const joint = jointsPtr[index + j];
				if (joint)
				{
					const ndInt32 maxMask = (maxRow - joint->m_rowCount) >> 8;
					const ndInt32 minMask = (minRow - joint->m_rowCount) >> 8;
					maxRow = ( maxMask & joint->m_rowCount) | (~maxMask & maxRow);
					minRow = (~minMask & joint->m_rowCount) | ( minMask & minRow);
					if (!joint->m_rowCount)
					{
						selectMask[j] = ndFloat32(0.0f);
					}
				}
				else
				{
					minRow = 0;
					selectMask[j] = ndFloat32(0.0f);
				}
			}
			ndAssert(maxRow >= 0);
			ndAssert(minRow < 255);
			jointMask[i] = selectMask;

			const ndInt8 isUniformGroup = (maxRow == minRow) & (maxRow > 0);
			groupType[i] = isUniformGroup;

			const ndInt32 soaRowBase = soaJointRows[i];
			if (isUniformGroup)
			{
				const ndConstraint* const joint0 = jointsPtr[index + 0];
				const ndConstraint* const joint1 = jointsPtr[index + 1];
				const ndConstraint* const joint2 = jointsPtr[index + 2];
				const ndConstraint* const joint3 = jointsPtr[index + 3];
				const ndConstraint* const joint4 = jointsPtr[index + 4];
				const ndConstraint* const joint5 = jointsPtr[index + 5];
				const ndConstraint* const joint6 = jointsPtr[index + 6];
				const ndConstraint* const joint7 = jointsPtr[index + 7];

				const ndInt32 rowCount = joint0->m_rowCount;
				for (ndInt32 j = 0; j < rowCount; ++j)
				{
					ndVector tmp[D_AVX_WORK_GROUP];
					const ndLeftHandSide* const row0 = &leftHandSide[joint0->m_rowStart + j];
					const ndLeftHandSide* const row1 = &leftHandSide[joint1->m_rowStart + j];
					const ndLeftHandSide* const row2 = &leftHandSide[joint2->m_rowStart + j];
					const ndLeftHandSide* const row3 = &leftHandSide[joint3->m_rowStart + j];
					const ndLeftHandSide* const row4 = &leftHandSide[joint4->m_rowStart + j];
					const ndLeftHandSide* const row5 = &leftHandSide[joint5->m_rowStart + j];
					const ndLeftHandSide* const row6 = &leftHandSide[joint6->m_rowStart + j];
					const ndLeftHandSide* const row7 = &leftHandSide[joint7->m_rowStart + j];
					ndSoaMatrixElement& row = massMatrix[soaRowBase + j];

					ndVector::Transpose4x4(tmp[0], tmp[1], tmp[2], tmp[3],
						row0->m_Jt.m_jacobianM0.m_linear,
						row1->m_Jt.m_jacobianM0.m_linear,
						row2->m_Jt.m_jacobianM0.m_linear,
						row3->m_Jt.m_jacobianM0.m_linear);
					ndVector::Transpose4x4(tmp[4], tmp[5], tmp[6], tmp[7],
						row4->m_Jt.m_jacobianM0.m_linear,
						row5->m_Jt.m_jacobianM0.m_linear,
						row6->m_Jt.m_jacobianM0.m_linear,
						row7->m_Jt.m_jacobianM0.m_linear);
					row.m_Jt.m_jacobianM0.m_linear.m_x = ndAvxFloat(tmp[0], tmp[4]);
					row.m_Jt.m_jacobianM0.m_linear.m_y = ndAvxFloat(tmp[1], tmp[5]);
					row.m_Jt.m_jacobianM0.m_linear.m_z = ndAvxFloat(tmp[2], tmp[6]);
					ndVector::Transpose4x4(tmp[0], tmp[1], tmp[2], tmp[3],
						row0->m_Jt.m_jacobianM0.m_angular,
						row1->m_Jt.m_jacobianM0.m_angular,
						row2->m_Jt.m_jacobianM0.m_angular,
						row3->m_Jt.m_jacobianM0.m_angular);
					ndVector::Transpose4x4(tmp[4], tmp[5], tmp[6], tmp[7],
						row4->m_Jt.m_jacobianM0.m_angular,
						row5->m_Jt.m_jacobianM0.m_angular,
						row6->m_Jt.m_jacobianM0.m_angular,
						row7->m_Jt.m_jacobianM0.m_angular);
					row.m_Jt.m_jacobianM0.m_angular.m_x = ndAvxFloat(tmp[0], tmp[4]);
					row.m_Jt.m_jacobianM0.m_angular.m_y = ndAvxFloat(tmp[1], tmp[5]);
					row.m_Jt.m_jacobianM0.m_angular.m_z = ndAvxFloat(tmp[2], tmp[6]);

					ndVector::Transpose4x4(tmp[0], tmp[1], tmp[2], tmp[3],
						row0->m_Jt.m_jacobianM1.m_linear,
						row1->m_Jt.m_jacobianM1.m_linear,
						row2->m_Jt.m_jacobianM1.m_linear,
						row3->m_Jt.m_jacobianM1.m_linear);
					ndVector::Transpose4x4(tmp[4], tmp[5], tmp[6], tmp[7],
						row4->m_Jt.m_jacobianM1.m_linear,
						row5->m_Jt.m_jacobianM1.m_linear,
						row6->m_Jt.m_jacobianM1.m_linear,
						row7->m_Jt.m_jacobianM1.m_linear);
					row.m_Jt.m_jacobianM1.m_linear.m_x = ndAvxFloat(tmp[0], tmp[4]);
					row.m_Jt.m_jacobianM1.m_linear.m_y = ndAvxFloat(tmp[1], tmp[5]);
					row.m_Jt.m_jacobianM1.m_linear.m_z = ndAvxFloat(tmp[2], tmp[6]);
					ndVector::Transpose4x4(tmp[0], tmp[1], tmp[2], tmp[3],
						row0->m_Jt.m_jacobianM1.m_angular,
						row1->m_Jt.m_jacobianM1.m_angular,
						row2->m_Jt.m_jacobianM1.m_angular,
						row3->m_Jt.m_jacobianM1.m_angular);
					ndVector::Transpose4x4(tmp[4], tmp[5], tmp[6], tmp[7],
						row4->m_Jt.m_jacobianM1.m_angular,
						row5->m_Jt.m_jacobianM1.m_angular,
						row6->m_Jt.m_jacobianM1.m_angular,
						row7->m_Jt.m_jacobianM1.m_angular);
					row.m_Jt.m_jacobianM1.m_angular.m_x = ndAvxFloat(tmp[0], tmp[4]);
					row.m_Jt.m_jacobianM1.m_angular.m_y = ndAvxFloat(tmp[1], tmp[5]);
					row.m_Jt.m_jacobianM1.m_angular.m_z = ndAvxFloat(tmp[2], tmp[6]);

					ndVector::Transpose4x4(tmp[0], tmp[1], tmp[2], tmp[3],
						row0->m_JMinv.m_jacobianM0.m_linear,
						row1->m_JMinv.m_jacobianM0.m_linear,
						row2->m_JMinv.m_jacobianM0.m_linear,
						row3->m_JMinv.m_jacobianM0.m_linear);
					ndVector::Transpose4x4(tmp[4], tmp[5], tmp[6], tmp[7],
						row4->m_JMinv.m_jacobianM0.m_linear,
						row5->m_JMinv.m_jacobianM0.m_linear,
						row6->m_JMinv.m_jacobianM0.m_linear,
						row7->m_JMinv.m_jacobianM0.m_linear);
					row.m_JMinv.m_jacobianM0.m_linear.m_x = ndAvxFloat(tmp[0], tmp[4]);
					row.m_JMinv.m_jacobianM0.m_linear.m_y = ndAvxFloat(tmp[1], tmp[5]);
					row.m_JMinv.m_jacobianM0.m_linear.m_z = ndAvxFloat(tmp[2], tmp[6]);
					ndVector::Transpose4x4(tmp[0], tmp[1], tmp[2], tmp[3],
						row0->m_JMinv.m_jacobianM0.m_angular,
						row1->m_JMinv.m_jacobianM0.m_angular,
						row2->m_JMinv.m_jacobianM0.m_angular,
						row3->m_JMinv.m_jacobianM0.m_angular);
					ndVector::Transpose4x4(tmp[4], tmp[5], tmp[6], tmp[7],
						row4->m_JMinv.m_jacobianM0.m_angular,
						row5->m_JMinv.m_jacobianM0.m_angular,
						row6->m_JMinv.m_jacobianM0.m_angular,
						row7->m_JMinv.m_jacobianM0.m_angular);
					row.m_JMinv.m_jacobianM0.m_angular.m_x = ndAvxFloat(tmp[0], tmp[4]);
					row.m_JMinv.m_jacobianM0.m_angular.m_y = ndAvxFloat(tmp[1], tmp[5]);
					row.m_JMinv.m_jacobianM0.m_angular.m_z = ndAvxFloat(tmp[2], tmp[6]);

					ndVector::Transpose4x4(tmp[0], tmp[1], tmp[2], tmp[3],
						row0->m_JMinv.m_jacobianM1.m_linear,
						row1->m_JMinv.m_jacobianM1.m_linear,
						row2->m_JMinv.m_jacobianM1.m_linear,
						row3->m_JMinv.m_jacobianM1.m_linear);
					ndVector::Transpose4x4(tmp[4], tmp[5], tmp[6], tmp[7],
						row4->m_JMinv.m_jacobianM1.m_linear,
						row5->m_JMinv.m_jacobianM1.m_linear,
						row6->m_JMinv.m_jacobianM1.m_linear,
						row7->m_JMinv.m_jacobianM1.m_linear);
					row.m_JMinv.m_jacobianM1.m_linear.m_x = ndAvxFloat(tmp[0], tmp[4]);
					row.m_JMinv.m_jacobianM1.m_linear.m_y = ndAvxFloat(tmp[1], tmp[5]);
					row.m_JMinv.m_jacobianM1.m_linear.m_z = ndAvxFloat(tmp[2], tmp[6]);
					ndVector::Transpose4x4(tmp[0], tmp[1], tmp[2], tmp[3],
						row0->m_JMinv.m_jacobianM1.m_angular,
						row1->m_JMinv.m_jacobianM1.m_angular,
						row2->m_JMinv.m_jacobianM1.m_angular,
						row3->m_JMinv.m_jacobianM1.m_angular);
					ndVector::Transpose4x4(tmp[4], tmp[5], tmp[6], tmp[7],
						row4->m_JMinv.m_jacobianM1.m_angular,
						row5->m_JMinv.m_jacobianM1.m_angular,
						row6->m_JMinv.m_jacobianM1.m_angular,
						row7->m_JMinv.m_jacobianM1.m_angular);
					row.m_JMinv.m_jacobianM1.m_angular.m_x = ndAvxFloat(tmp[0], tmp[4]);
					row.m_JMinv.m_jacobianM1.m_angular.m_y = ndAvxFloat(tmp[1], tmp[5]);
					row.m_JMinv.m_jacobianM1.m_angular.m_z = ndAvxFloat(tmp[2], tmp[6]);

					#ifdef D_NEWTON_USE_DOUBLE
					ndInt64* const normalIndex = (ndInt64*)&row.m_normalForceIndex[0];
					#else
					ndInt32* const normalIndex = (ndInt32*)&row.m_normalForceIndex[0];
					#endif
					for (ndInt32 k = 0; k < D_AVX_WORK_GROUP; ++k)
					{
						const ndConstraint* const soaJoint = jointsPtr[index + k];
						const ndRightHandSide* const rhs = &rightHandSide[soaJoint->m_rowStart + j];
						row.m_force[k] = rhs->m_force;
						row.m_diagDamp[k] = rhs->m_diagDamp;
						row.m_invJinvMJt[k] = rhs->m_invJinvMJt;
						row.m_coordenateAccel[k] = rhs->m_coordenateAccel;
						normalIndex[k] = (rhs->m_normalForceIndex + 1) * D_AVX_WORK_GROUP + k;
						row.m_lowerBoundFrictionCoefficent[k] = rhs->m_lowerBoundFrictionCoefficent;
						row.m_upperBoundFrictionCoefficent[k] = rhs->m_upperBoundFrictionCoefficent;
					}
				}
			}
			else
			{
				const ndConstraint* const firstJoint = jointsPtr[index];
				for (ndInt32 j = 0; j < firstJoint->m_rowCount; ++j)
				{
					ndSoaMatrixElement& row = massMatrix[soaRowBase + j];
					row.m_Jt.m_jacobianM0.m_linear.m_x = zero;
					row.m_Jt.m_jacobianM0.m_linear.m_y = zero;
					row.m_Jt.m_jacobianM0.m_linear.m_z = zero;
					row.m_Jt.m_jacobianM0.m_angular.m_x = zero;
					row.m_Jt.m_jacobianM0.m_angular.m_y = zero;
					row.m_Jt.m_jacobianM0.m_angular.m_z = zero;
					row.m_Jt.m_jacobianM1.m_linear.m_x = zero;
					row.m_Jt.m_jacobianM1.m_linear.m_y = zero;
					row.m_Jt.m_jacobianM1.m_linear.m_z = zero;
					row.m_Jt.m_jacobianM1.m_angular.m_x = zero;
					row.m_Jt.m_jacobianM1.m_angular.m_y = zero;
					row.m_Jt.m_jacobianM1.m_angular.m_z = zero;

					row.m_JMinv.m_jacobianM0.m_linear.m_x = zero;
					row.m_JMinv.m_jacobianM0.m_linear.m_y = zero;
					row.m_JMinv.m_jacobianM0.m_linear.m_z = zero;
					row.m_JMinv.m_jacobianM0.m_angular.m_x = zero;
					row.m_JMinv.m_jacobianM0.m_angular.m_y = zero;
					row.m_JMinv.m_jacobianM0.m_angular.m_z = zero;
					row.m_JMinv.m_jacobianM1.m_linear.m_x = zero;
					row.m_JMinv.m_jacobianM1.m_linear.m_y = zero;
					row.m_JMinv.m_jacobianM1.m_linear.m_z = zero;
					row.m_JMinv.m_jacobianM1.m_angular.m_x = zero;
					row.m_JMinv.m_jacobianM1.m_angular.m_y = zero;
					row.m_JMinv.m_jacobianM1.m_angular.m_z = zero;

					row.m_force = zero;
					row.m_diagDamp = zero;
					row.m_invJinvMJt = zero;
					row.m_coordenateAccel = zero;
					row.m_normalForceIndex = ordinals;
					row.m_lowerBoundFrictionCoefficent = zero;
					row.m_upperBoundFrictionCoefficent = zero;
				}

				for (ndInt32 j = 0; j < D_AVX_WORK_GROUP; ++j)
				{
					const ndConstraint* const joint = jointsPtr[index + j];
					if (joint)
					{
						for (ndInt32 k = 0; k < joint->m_rowCount; ++k)
						{
							ndSoaMatrixElement& row = massMatrix[soaRowBase + k];
							const ndLeftHandSide* const lhs = &leftHandSide[joint->m_rowStart + k];

							row.m_Jt.m_jacobianM0.m_linear.m_x[j] = lhs->m_Jt.m_jacobianM0.m_linear.m_x;
							row.m_Jt.m_jacobianM0.m_linear.m_y[j] = lhs->m_Jt.m_jacobianM0.m_linear.m_y;
							row.m_Jt.m_jacobianM0.m_linear.m_z[j] = lhs->m_Jt.m_jacobianM0.m_linear.m_z;
							row.m_Jt.m_jacobianM0.m_angular.m_x[j] = lhs->m_Jt.m_jacobianM0.m_angular.m_x;
							row.m_Jt.m_jacobianM0.m_angular.m_y[j] = lhs->m_Jt.m_jacobianM0.m_angular.m_y;
							row.m_Jt.m_jacobianM0.m_angular.m_z[j] = lhs->m_Jt.m_jacobianM0.m_angular.m_z;
							row.m_Jt.m_jacobianM1.m_linear.m_x[j] = lhs->m_Jt.m_jacobianM1.m_linear.m_x;
							row.m_Jt.m_jacobianM1.m_linear.m_y[j] = lhs->m_Jt.m_jacobianM1.m_linear.m_y;
							row.m_Jt.m_jacobianM1.m_linear.m_z[j] = lhs->m_Jt.m_jacobianM1.m_linear.m_z;
							row.m_Jt.m_jacobianM1.m_angular.m_x[j] = lhs->m_Jt.m_jacobianM1.m_angular.m_x;
							row.m_Jt.m_jacobianM1.m_angular.m_y[j] = lhs->m_Jt.m_jacobianM1.m_angular.m_y;
							row.m_Jt.m_jacobianM1.m_angular.m_z[j] = lhs->m_Jt.m_jacobianM1.m_angular.m_z;

							row.m_JMinv.m_jacobianM0.m_linear.m_x[j] = lhs->m_JMinv.m_jacobianM0.m_linear.m_x;
							row.m_JMinv.m_jacobianM0.m_linear.m_y[j] = lhs->m_JMinv.m_jacobianM0.m_linear.m_y;
							row.m_JMinv.m_jacobianM0.m_linear.m_z[j] = lhs->m_JMinv.m_jacobianM0.m_linear.m_z;
							row.m_JMinv.m_jacobianM0.m_angular.m_x[j] = lhs->m_JMinv.m_jacobianM0.m_angular.m_x;
							row.m_JMinv.m_jacobianM0.m_angular.m_y[j] = lhs->m_JMinv.m_jacobianM0.m_angular.m_y;
							row.m_JMinv.m_jacobianM0.m_angular.m_z[j] = lhs->m_JMinv.m_jacobianM0.m_angular.m_z;
							row.m_JMinv.m_jacobianM1.m_linear.m_x[j] = lhs->m_JMinv.m_jacobianM1.m_linear.m_x;
							row.m_JMinv.m_jacobianM1.m_linear.m_y[j] = lhs->m_JMinv.m_jacobianM1.m_linear.m_y;
							row.m_JMinv.m_jacobianM1.m_linear.m_z[j] = lhs->m_JMinv.m_jacobianM1.m_linear.m_z;
							row.m_JMinv.m_jacobianM1.m_angular.m_x[j] = lhs->m_JMinv.m_jacobianM1.m_angular.m_x;
							row.m_JMinv.m_jacobianM1.m_angular.m_y[j] = lhs->m_JMinv.m_jacobianM1.m_angular.m_y;
							row.m_JMinv.m_jacobianM1.m_angular.m_z[j] = lhs->m_JMinv.m_jacobianM1.m_angular.m_z;

							const ndRightHandSide* const rhs = &rightHandSide[joint->m_rowStart + k];
							row.m_force[j] = rhs->m_force;
							row.m_diagDamp[j] = rhs->m_diagDamp;
							row.m_invJinvMJt[j] = rhs->m_invJinvMJt;
							row.m_coordenateAccel[j] = rhs->m_coordenateAccel;

							#ifdef D_NEWTON_USE_DOUBLE
							ndInt64* const normalIndex = (ndInt64*)&row.m_normalForceIndex[0];
							#else
							ndInt32* const normalIndex = (ndInt32*)&row.m_normalForceIndex[0];
							#endif
							normalIndex[j] = (rhs->m_normalForceIndex + 1) * D_AVX_WORK_GROUP + j;
							row.m_lowerBoundFrictionCoefficent[j] = rhs->m_lowerBoundFrictionCoefficent;
							row.m_upperBoundFrictionCoefficent[j] = rhs->m_upperBoundFrictionCoefficent;
						}
					}
				}
			}
		}
	});

	if (scene->GetActiveContactArray().GetCount())
	{
		D_TRACKTIME();
		m_rightHandSide[0].m_force = ndFloat32(1.0f);

		scene->ParallelExecute(InitJacobianMatrix);
		scene->ParallelExecute(InitJacobianAccumulatePartialForces);
		scene->ParallelExecute(TransposeMassMatrix);
	}
}

void ndDynamicsUpdateAvx2::UpdateForceFeedback()
{
	D_TRACKTIME();
	ndScene* const scene = m_world->GetScene();
	const ndArray<ndConstraint*>& jointArray = scene->GetActiveContactArray();

	auto UpdateForceFeedback = ndMakeObject::ndFunction([this, &jointArray](ndInt32 threadIndex, ndInt32 threadCount)
	{
		D_TRACKTIME_NAMED(UpdateForceFeedback);
		ndArray<ndRightHandSide>& rightHandSide = m_rightHandSide;
		const ndArray<ndLeftHandSide>& leftHandSide = m_leftHandSide;

		ndAvxFloat zero(ndFloat32(0.0f));
		const ndFloat32 timestepRK = GetTimestepRK();
		const ndStartEnd startEnd(jointArray.GetCount(), threadIndex, threadCount);
		for (ndInt32 i = startEnd.m_start; i < startEnd.m_end; ++i)
		{
			ndConstraint* const joint = jointArray[i];
			const ndInt32 rows = joint->m_rowCount;
			const ndInt32 first = joint->m_rowStart;

			for (ndInt32 j = 0; j < rows; ++j)
			{
				const ndRightHandSide* const rhs = &rightHandSide[j + first];
				ndAssert(dCheckFloat(rhs->m_force));
				rhs->m_jointFeebackForce->Push(rhs->m_force);
				rhs->m_jointFeebackForce->m_force = rhs->m_force;
				rhs->m_jointFeebackForce->m_impact = rhs->m_maxImpact * timestepRK;
			}

			if (joint->GetAsBilateral())
			{
				ndAvxFloat force0(zero);
				ndAvxFloat force1(zero);

				for (ndInt32 j = 0; j < rows; ++j)
				{
					const ndRightHandSide* const rhs = &rightHandSide[j + first];
					const ndLeftHandSide* const lhs = &leftHandSide[j + first];
					const ndAvxFloat f(rhs->m_force);
					force0 = force0.MulAdd((ndAvxFloat&)lhs->m_Jt.m_jacobianM0, f);
					force1 = force1.MulAdd((ndAvxFloat&)lhs->m_Jt.m_jacobianM1, f);
				}
				ndJointBilateralConstraint* const bilateral = (ndJointBilateralConstraint*)joint;
				bilateral->m_forceBody0 = force0.GetLow();
				bilateral->m_torqueBody0 = force0.GetHigh();
				bilateral->m_forceBody1 = force1.GetLow();
				bilateral->m_torqueBody1 = force1.GetHigh();
			}
		}
	});

	scene->ParallelExecute(UpdateForceFeedback);
}

void ndDynamicsUpdateAvx2::InitSkeletons()
{
	D_TRACKTIME();
	ndScene* const scene = m_world->GetScene();
	const ndArray<ndSkeletonContainer*>& activeSkeletons = m_world->m_activeSkeletons;

	auto InitSkeletons = ndMakeObject::ndFunction([this, &activeSkeletons](ndInt32 threadIndex, ndInt32 threadCount)
	{
		D_TRACKTIME_NAMED(InitSkeletons);
		ndArray<ndRightHandSide>& rightHandSide = m_rightHandSide;
		const ndArray<ndLeftHandSide>& leftHandSide = m_leftHandSide;

		for (ndInt32 i = threadIndex; i < activeSkeletons.GetCount(); i += threadCount)
		{
			ndSkeletonContainer* const skeleton = activeSkeletons[i];
			skeleton->InitMassMatrix(&leftHandSide[0], &rightHandSide[0]);
		}
	});

	if (activeSkeletons.GetCount())
	{
		scene->ParallelExecute(InitSkeletons);
	}
}

void ndDynamicsUpdateAvx2::UpdateSkeletons()
{
	D_TRACKTIME();
	ndScene* const scene = m_world->GetScene();
	const ndArray<ndSkeletonContainer*>& activeSkeletons = m_world->m_activeSkeletons;
	const ndBodyKinematic** const bodyArray = (const ndBodyKinematic**)(&scene->GetActiveBodyArray()[0]);

	auto UpdateSkeletons = ndMakeObject::ndFunction([this, &bodyArray, &activeSkeletons](ndInt32 threadIndex, ndInt32 threadCount)
	{
		D_TRACKTIME_NAMED(UpdateSkeletons);
		ndJacobian* const internalForces = &GetInternalForces()[0];
		for (ndInt32 i = threadIndex; i < activeSkeletons.GetCount(); i += threadCount)
		{
			ndSkeletonContainer* const skeleton = activeSkeletons[i];
			skeleton->CalculateReactionForces(internalForces);
		}
	});

	if (activeSkeletons.GetCount())
	{
		scene->ParallelExecute(UpdateSkeletons);
	}
}

void ndDynamicsUpdateAvx2::CalculateJointsAcceleration()
{
	D_TRACKTIME();
	ndScene* const scene = m_world->GetScene();
	const ndArray<ndConstraint*>& jointArray = scene->GetActiveContactArray();

	auto CalculateJointsAcceleration = ndMakeObject::ndFunction([this, &jointArray](ndInt32 threadIndex, ndInt32 threadCount)
	{
		D_TRACKTIME_NAMED(CalculateJointsAcceleration);
		ndJointAccelerationDecriptor joindDesc;
		joindDesc.m_timestep = m_timestepRK;
		joindDesc.m_invTimestep = m_invTimestepRK;
		joindDesc.m_firstPassCoefFlag = m_firstPassCoef;
		ndArray<ndLeftHandSide>& leftHandSide = m_leftHandSide;
		ndArray<ndRightHandSide>& rightHandSide = m_rightHandSide;

		const ndStartEnd startEnd(jointArray.GetCount(), threadIndex, threadCount);
		for (ndInt32 i = startEnd.m_start; i < startEnd.m_end; ++i)
		{
			ndConstraint* const joint = jointArray[i];
			const ndInt32 pairStart = joint->m_rowStart;
			joindDesc.m_rowsCount = joint->m_rowCount;
			joindDesc.m_leftHandSide = &leftHandSide[pairStart];
			joindDesc.m_rightHandSide = &rightHandSide[pairStart];
			joint->JointAccelerations(&joindDesc);
		}
	});

	auto UpdateAcceleration = ndMakeObject::ndFunction([this, &jointArray](ndInt32 threadIndex, ndInt32 threadCount)
	{
		D_TRACKTIME_NAMED(UpdateAcceleration);
		const ndArray<ndRightHandSide>& rightHandSide = m_rightHandSide;

		const ndInt32 jointCount = jointArray.GetCount();
		const ndInt32 mask = -ndInt32(D_AVX_WORK_GROUP);
		const ndInt32* const soaJointRows = &m_avxJointRows[0];
		const ndInt32 soaJointCountBatches = ((jointCount + D_AVX_WORK_GROUP - 1) & mask) / D_AVX_WORK_GROUP;
		const ndInt8* const groupType = &m_groupType[0];

		const ndConstraint* const * jointArrayPtr = &jointArray[0];
		ndAvxMatrixArray& massMatrix = *m_avxMassMatrixArray;
		for (ndInt32 i = threadIndex; i < soaJointCountBatches; i += threadCount)
		{
			if (groupType[i])
			{
				const ndInt32 soaRowStartBase = soaJointRows[i];
				const ndConstraint* const* jointGroup = &jointArrayPtr[i * D_AVX_WORK_GROUP];
				const ndConstraint* const firstJoint = jointGroup[0];
				const ndInt32 rowCount = firstJoint->m_rowCount;
				for (ndInt32 j = 0; j < D_AVX_WORK_GROUP; ++j)
				{
					const ndConstraint* const Joint = jointGroup[j];
					const ndInt32 base = Joint->m_rowStart;
					for (ndInt32 k = 0; k < rowCount; ++k)
					{
						ndSoaMatrixElement* const row = &massMatrix[soaRowStartBase + k];
						row->m_coordenateAccel[j] = rightHandSide[base + k].m_coordenateAccel;
					}
				}
			}
			else
			{
				const ndInt32 soaRowStartBase = soaJointRows[i];
				const ndConstraint* const * jointGroup = &jointArrayPtr[i * D_AVX_WORK_GROUP];
				for (ndInt32 j = 0; j < D_AVX_WORK_GROUP; ++j)
				{
					const ndConstraint* const Joint = jointGroup[j];
					if (Joint)
					{
						const ndInt32 base = Joint->m_rowStart;
						const ndInt32 rowCount = Joint->m_rowCount;
						for (ndInt32 k = 0; k < rowCount; ++k)
						{
							ndSoaMatrixElement* const row = &massMatrix[soaRowStartBase + k];
							row->m_coordenateAccel[j] = rightHandSide[base + k].m_coordenateAccel;
						}
					}
				}
			}
		}
	});

	scene->ParallelExecute(CalculateJointsAcceleration);

	m_firstPassCoef = ndFloat32(1.0f);
	scene->ParallelExecute(UpdateAcceleration);
}

void ndDynamicsUpdateAvx2::IntegrateBodiesVelocity()
{
	D_TRACKTIME();
	ndScene* const scene = m_world->GetScene();
	auto IntegrateBodiesVelocity = ndMakeObject::ndFunction([this](ndInt32 threadIndex, ndInt32 threadCount)
	{
		D_TRACKTIME_NAMED(IntegrateBodiesVelocity);
		ndArray<ndBodyKinematic*>& bodyArray = GetBodyIslandOrder();
		const ndArray<ndJacobian>& internalForces = GetInternalForces();

		const ndVector timestep4(GetTimestepRK());
		const ndVector speedFreeze2(m_world->m_freezeSpeed2 * ndFloat32(0.1f));

		const ndStartEnd startEnd(bodyArray.GetCount() - GetUnconstrainedBodyCount(), threadIndex, threadCount);
		for (ndInt32 i = startEnd.m_start; i < startEnd.m_end; ++i)
		{
			ndBodyKinematic* const body = bodyArray[i];

			ndAssert(body);
			ndAssert(body->GetAsBodyDynamic());
			ndAssert(body->m_isConstrained);
			const ndInt32 index = body->m_index;
			const ndJacobian& forceAndTorque = internalForces[index];
			const ndVector force(body->GetForce() + forceAndTorque.m_linear);
			const ndVector torque(body->GetTorque() + forceAndTorque.m_angular - body->GetGyroTorque());
			const ndJacobian velocStep(body->IntegrateForceAndToque(force, torque, timestep4));

			if (!body->m_equilibrium0)
			{
				body->m_veloc += velocStep.m_linear;
				body->m_omega += velocStep.m_angular;
				body->IntegrateGyroSubstep(timestep4);
			}
			else
			{
				const ndVector velocStep2(velocStep.m_linear.DotProduct(velocStep.m_linear));
				const ndVector omegaStep2(velocStep.m_angular.DotProduct(velocStep.m_angular));
				const ndVector test(((velocStep2 > speedFreeze2) | (omegaStep2 > speedFreeze2)) & ndVector::m_negOne);
				const ndInt8 equilibrium = test.GetSignMask() ? 0 : 1;
				body->m_equilibrium0 = equilibrium;
			}
			ndAssert(body->m_veloc.m_w == ndFloat32(0.0f));
			ndAssert(body->m_omega.m_w == ndFloat32(0.0f));
		}
	});

	scene->ParallelExecute(IntegrateBodiesVelocity);
}

void ndDynamicsUpdateAvx2::CalculateJointsForce()
{
	D_TRACKTIME();
	const ndInt32 passes = m_solverPasses;
	ndScene* const scene = m_world->GetScene();

	ndArray<ndBodyKinematic*>& bodyArray = scene->GetActiveBodyArray();
	ndArray<ndConstraint*>& jointArray = scene->GetActiveContactArray();

	auto CalculateJointsForce = ndMakeObject::ndFunction([this, &jointArray](ndInt32 threadIndex, ndInt32 threadCount)
	{
		D_TRACKTIME_NAMED(CalculateJointsForce);
		const ndInt32 jointCount = jointArray.GetCount();
		ndJacobian* const jointPartialForces = &GetTempInternalForces()[0];

		const ndInt32* const soaJointRows = &m_avxJointRows[0];
		ndAvxMatrixArray& soaMassMatrixArray = *m_avxMassMatrixArray;
		ndSoaMatrixElement* const soaMassMatrix = &soaMassMatrixArray[0];

		auto JointForce = [this, &jointArray, jointPartialForces](ndInt32 group, ndSoaMatrixElement* const massMatrix)
		{
			ndAvxVector6 forceM0;
			ndAvxVector6 forceM1;
			#ifdef D_JOINT_PRECONDITIONER
			ndAvxFloat weight0;
			ndAvxFloat weight1;
			#endif
			ndAvxFloat preconditioner0;
			ndAvxFloat preconditioner1;
			ndAvxFloat normalForce[D_CONSTRAINT_MAX_ROWS + 1];

			const ndInt32 block = group * D_AVX_WORK_GROUP;
			ndConstraint** const jointGroup = &jointArray[block];

			ndAvxFloat zero(ndFloat32(0.0f));
			const ndInt8 isUniformGruop = m_groupType[group];
			if (isUniformGruop)
			{
				for (ndInt32 i = 0; i < D_AVX_WORK_GROUP; ++i)
				{
					const ndConstraint* const joint = jointGroup[i];
					const ndBodyKinematic* const body0 = joint->GetBody0();
					const ndBodyKinematic* const body1 = joint->GetBody1();

					const ndInt32 m0 = body0->m_index;
					const ndInt32 m1 = body1->m_index;

					#ifdef D_JOINT_PRECONDITIONER
					weight0[i] = body0->m_weigh;
					weight1[i] = body1->m_weigh;
					preconditioner0[i] = joint->m_preconditioner0;
					preconditioner1[i] = joint->m_preconditioner1;
					#else	
					preconditioner0[i] = body0->m_weigh;
					preconditioner1[i] = body1->m_weigh;
					#endif

					forceM0.m_linear.m_x[i] = m_internalForces[m0].m_linear.m_x;
					forceM0.m_linear.m_y[i] = m_internalForces[m0].m_linear.m_y;
					forceM0.m_linear.m_z[i] = m_internalForces[m0].m_linear.m_z;
					forceM0.m_angular.m_x[i] = m_internalForces[m0].m_angular.m_x;
					forceM0.m_angular.m_y[i] = m_internalForces[m0].m_angular.m_y;
					forceM0.m_angular.m_z[i] = m_internalForces[m0].m_angular.m_z;

					forceM1.m_linear.m_x[i] = m_internalForces[m1].m_linear.m_x;
					forceM1.m_linear.m_y[i] = m_internalForces[m1].m_linear.m_y;
					forceM1.m_linear.m_z[i] = m_internalForces[m1].m_linear.m_z;
					forceM1.m_angular.m_x[i] = m_internalForces[m1].m_angular.m_x;
					forceM1.m_angular.m_y[i] = m_internalForces[m1].m_angular.m_y;
					forceM1.m_angular.m_z[i] = m_internalForces[m1].m_angular.m_z;
				}
			}
			else
			{
				preconditioner0 = zero;
				preconditioner1 = zero;
				#ifdef D_JOINT_PRECONDITIONER
				weight0 = zero;
				weight1 = zero;
				#endif
				forceM0.m_linear.m_x = zero;
				forceM0.m_linear.m_y = zero;
				forceM0.m_linear.m_z = zero;
				forceM0.m_angular.m_x = zero;
				forceM0.m_angular.m_y = zero;
				forceM0.m_angular.m_z = zero;

				forceM1.m_linear.m_x = zero;
				forceM1.m_linear.m_y = zero;
				forceM1.m_linear.m_z = zero;
				forceM1.m_angular.m_x = zero;
				forceM1.m_angular.m_y = zero;
				forceM1.m_angular.m_z = zero;
				for (ndInt32 i = 0; i < D_AVX_WORK_GROUP; ++i)
				{
					const ndConstraint* const joint = jointGroup[i];
					if (joint && joint->m_rowCount)
					{
						const ndBodyKinematic* const body0 = joint->GetBody0();
						const ndBodyKinematic* const body1 = joint->GetBody1();

						const ndInt32 m0 = body0->m_index;
						const ndInt32 m1 = body1->m_index;
						#ifdef D_JOINT_PRECONDITIONER
						weight0[i] = body0->m_weigh;
						weight1[i] = body1->m_weigh;
						preconditioner0[i] = joint->m_preconditioner0;
						preconditioner1[i] = joint->m_preconditioner1;
						#else
						preconditioner0[i] = body0->m_weigh;
						preconditioner1[i] = body1->m_weigh;
						#endif				

						forceM0.m_linear.m_x[i] = m_internalForces[m0].m_linear.m_x;
						forceM0.m_linear.m_y[i] = m_internalForces[m0].m_linear.m_y;
						forceM0.m_linear.m_z[i] = m_internalForces[m0].m_linear.m_z;
						forceM0.m_angular.m_x[i] = m_internalForces[m0].m_angular.m_x;
						forceM0.m_angular.m_y[i] = m_internalForces[m0].m_angular.m_y;
						forceM0.m_angular.m_z[i] = m_internalForces[m0].m_angular.m_z;

						forceM1.m_linear.m_x[i] = m_internalForces[m1].m_linear.m_x;
						forceM1.m_linear.m_y[i] = m_internalForces[m1].m_linear.m_y;
						forceM1.m_linear.m_z[i] = m_internalForces[m1].m_linear.m_z;
						forceM1.m_angular.m_x[i] = m_internalForces[m1].m_angular.m_x;
						forceM1.m_angular.m_y[i] = m_internalForces[m1].m_angular.m_y;
						forceM1.m_angular.m_z[i] = m_internalForces[m1].m_angular.m_z;
					}
				}
			}

			#ifdef D_JOINT_PRECONDITIONER
			forceM0.m_linear.m_x = forceM0.m_linear.m_x * preconditioner0;
			forceM0.m_linear.m_y = forceM0.m_linear.m_y * preconditioner0;
			forceM0.m_linear.m_z = forceM0.m_linear.m_z * preconditioner0;
			forceM0.m_angular.m_x = forceM0.m_angular.m_x * preconditioner0;
			forceM0.m_angular.m_y = forceM0.m_angular.m_y * preconditioner0;
			forceM0.m_angular.m_z = forceM0.m_angular.m_z * preconditioner0;

			forceM1.m_linear.m_x = forceM1.m_linear.m_x * preconditioner1;
			forceM1.m_linear.m_y = forceM1.m_linear.m_y * preconditioner1;
			forceM1.m_linear.m_z = forceM1.m_linear.m_z * preconditioner1;
			forceM1.m_angular.m_x = forceM1.m_angular.m_x * preconditioner1;
			forceM1.m_angular.m_y = forceM1.m_angular.m_y * preconditioner1;
			forceM1.m_angular.m_z = forceM1.m_angular.m_z * preconditioner1;

			preconditioner0 = preconditioner0 * weight0;
			preconditioner1 = preconditioner1 * weight1;
			#endif

			#ifdef D_USE_EARLY_OUT_JOINT
			ndAvxFloat accNorm(zero);
			#endif

			normalForce[0] = ndAvxFloat (ndFloat32 (1.0f));
			const ndInt32 rowsCount = jointGroup[0]->m_rowCount;

			for (ndInt32 j = 0; j < rowsCount; ++j)
			{
				ndSoaMatrixElement* const row = &massMatrix[j];

				ndAvxFloat a0(row->m_JMinv.m_jacobianM0.m_linear.m_x * forceM0.m_linear.m_x);
				ndAvxFloat a1(row->m_JMinv.m_jacobianM1.m_linear.m_x * forceM1.m_linear.m_x);
				a0 = a0.MulAdd(row->m_JMinv.m_jacobianM0.m_angular.m_x, forceM0.m_angular.m_x);
				a1 = a1.MulAdd(row->m_JMinv.m_jacobianM1.m_angular.m_x, forceM1.m_angular.m_x);

				a0 = a0.MulAdd(row->m_JMinv.m_jacobianM0.m_linear.m_y, forceM0.m_linear.m_y);
				a1 = a1.MulAdd(row->m_JMinv.m_jacobianM1.m_linear.m_y, forceM1.m_linear.m_y);
				a0 = a0.MulAdd(row->m_JMinv.m_jacobianM0.m_angular.m_y, forceM0.m_angular.m_y);
				a1 = a1.MulAdd(row->m_JMinv.m_jacobianM1.m_angular.m_y, forceM1.m_angular.m_y);

				a0 = a0.MulAdd(row->m_JMinv.m_jacobianM0.m_linear.m_z, forceM0.m_linear.m_z);
				a1 = a1.MulAdd(row->m_JMinv.m_jacobianM1.m_linear.m_z, forceM1.m_linear.m_z);
				a0 = a0.MulAdd(row->m_JMinv.m_jacobianM0.m_angular.m_z, forceM0.m_angular.m_z);
				a1 = a1.MulAdd(row->m_JMinv.m_jacobianM1.m_angular.m_z, forceM1.m_angular.m_z);

				ndAvxFloat a(a0 + a1);
				a = row->m_coordenateAccel.MulSub(row->m_force, row->m_diagDamp) - a;
				ndAvxFloat f(row->m_force.MulAdd(row->m_invJinvMJt, a));

				const ndAvxFloat frictionNormal(normalForce, row->m_normalForceIndex);
				const ndAvxFloat lowerFrictionForce(frictionNormal * row->m_lowerBoundFrictionCoefficent);
				const ndAvxFloat upperFrictionForce(frictionNormal * row->m_upperBoundFrictionCoefficent);

				#ifdef D_USE_EARLY_OUT_JOINT
				a = a & (f < upperFrictionForce) & (f > lowerFrictionForce);
				accNorm = accNorm.MulAdd(a, a);
				#endif

				f = f.GetMax(lowerFrictionForce).GetMin(upperFrictionForce);
				normalForce[j + 1] = f;

				const ndAvxFloat deltaForce(f - row->m_force);
				const ndAvxFloat deltaForce0(deltaForce * preconditioner0);
				const ndAvxFloat deltaForce1(deltaForce * preconditioner1);
				forceM0.m_linear.m_x = forceM0.m_linear.m_x.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_x, deltaForce0);
				forceM0.m_linear.m_y = forceM0.m_linear.m_y.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_y, deltaForce0);
				forceM0.m_linear.m_z = forceM0.m_linear.m_z.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_z, deltaForce0);
				forceM0.m_angular.m_x = forceM0.m_angular.m_x.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_x, deltaForce0);
				forceM0.m_angular.m_y = forceM0.m_angular.m_y.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_y, deltaForce0);
				forceM0.m_angular.m_z = forceM0.m_angular.m_z.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_z, deltaForce0);

				forceM1.m_linear.m_x = forceM1.m_linear.m_x.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_x, deltaForce1);
				forceM1.m_linear.m_y = forceM1.m_linear.m_y.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_y, deltaForce1);
				forceM1.m_linear.m_z = forceM1.m_linear.m_z.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_z, deltaForce1);
				forceM1.m_angular.m_x = forceM1.m_angular.m_x.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_x, deltaForce1);
				forceM1.m_angular.m_y = forceM1.m_angular.m_y.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_y, deltaForce1);
				forceM1.m_angular.m_z = forceM1.m_angular.m_z.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_z, deltaForce1);
			}

			const ndFloat32 tol = ndFloat32(0.125f);
			const ndFloat32 tol2 = tol * tol;

			#ifdef D_USE_EARLY_OUT_JOINT
			ndAvxFloat maxAccel(accNorm);
			for (ndInt32 k = 0; (k < 4) && (maxAccel.GetMax() > tol2); ++k)
			#else
			for (ndInt32 k = 0; k < 4; ++k)
			#endif
			{
				#ifdef D_USE_EARLY_OUT_JOINT
				maxAccel = zero;
				#endif

				for (ndInt32 j = 0; j < rowsCount; ++j)
				{
					ndSoaMatrixElement* const row = &massMatrix[j];

					ndAvxFloat a0(row->m_JMinv.m_jacobianM0.m_linear.m_x * forceM0.m_linear.m_x);
					ndAvxFloat a1(row->m_JMinv.m_jacobianM1.m_linear.m_x * forceM1.m_linear.m_x);
					a0 = a0.MulAdd(row->m_JMinv.m_jacobianM0.m_angular.m_x, forceM0.m_angular.m_x);
					a1 = a1.MulAdd(row->m_JMinv.m_jacobianM1.m_angular.m_x, forceM1.m_angular.m_x);

					a0 = a0.MulAdd(row->m_JMinv.m_jacobianM0.m_linear.m_y, forceM0.m_linear.m_y);
					a1 = a1.MulAdd(row->m_JMinv.m_jacobianM1.m_linear.m_y, forceM1.m_linear.m_y);
					a0 = a0.MulAdd(row->m_JMinv.m_jacobianM0.m_angular.m_y, forceM0.m_angular.m_y);
					a1 = a1.MulAdd(row->m_JMinv.m_jacobianM1.m_angular.m_y, forceM1.m_angular.m_y);

					a0 = a0.MulAdd(row->m_JMinv.m_jacobianM0.m_linear.m_z, forceM0.m_linear.m_z);
					a1 = a1.MulAdd(row->m_JMinv.m_jacobianM1.m_linear.m_z, forceM1.m_linear.m_z);
					a0 = a0.MulAdd(row->m_JMinv.m_jacobianM0.m_angular.m_z, forceM0.m_angular.m_z);
					a1 = a1.MulAdd(row->m_JMinv.m_jacobianM1.m_angular.m_z, forceM1.m_angular.m_z);

					ndAvxFloat a(a0 + a1);
					const ndAvxFloat force(normalForce[j + 1]);
					a = row->m_coordenateAccel.MulSub(force, row->m_diagDamp) - a;
					ndAvxFloat f(force.MulAdd(row->m_invJinvMJt, a));

					const ndAvxFloat frictionNormal(normalForce, row->m_normalForceIndex);
					const ndAvxFloat lowerFrictionForce(frictionNormal * row->m_lowerBoundFrictionCoefficent);
					const ndAvxFloat upperFrictionForce(frictionNormal * row->m_upperBoundFrictionCoefficent);

					#ifdef D_USE_EARLY_OUT_JOINT
					a = a & (f < upperFrictionForce) & (f > lowerFrictionForce);
					maxAccel = maxAccel.MulAdd(a, a);
					#endif

					f = f.GetMax(lowerFrictionForce).GetMin(upperFrictionForce);
					normalForce[j + 1] = f;

					const ndAvxFloat deltaForce(f - force);
					const ndAvxFloat deltaForce0(deltaForce * preconditioner0);
					const ndAvxFloat deltaForce1(deltaForce * preconditioner1);

					forceM0.m_linear.m_x = forceM0.m_linear.m_x.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_x, deltaForce0);
					forceM0.m_linear.m_y = forceM0.m_linear.m_y.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_y, deltaForce0);
					forceM0.m_linear.m_z = forceM0.m_linear.m_z.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_z, deltaForce0);
					forceM0.m_angular.m_x = forceM0.m_angular.m_x.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_x, deltaForce0);
					forceM0.m_angular.m_y = forceM0.m_angular.m_y.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_y, deltaForce0);
					forceM0.m_angular.m_z = forceM0.m_angular.m_z.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_z, deltaForce0);

					forceM1.m_linear.m_x = forceM1.m_linear.m_x.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_x, deltaForce1);
					forceM1.m_linear.m_y = forceM1.m_linear.m_y.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_y, deltaForce1);
					forceM1.m_linear.m_z = forceM1.m_linear.m_z.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_z, deltaForce1);
					forceM1.m_angular.m_x = forceM1.m_angular.m_x.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_x, deltaForce1);
					forceM1.m_angular.m_y = forceM1.m_angular.m_y.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_y, deltaForce1);
					forceM1.m_angular.m_z = forceM1.m_angular.m_z.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_z, deltaForce1);
				}
			}

			ndAvxFloat mask(ndAvxFloat::m_mask);
			for (ndInt32 i = 0; i < D_AVX_WORK_GROUP; ++i)
			{
				const ndConstraint* const joint = jointGroup[i];
				if (joint && joint->m_rowCount)
				{
					const ndBodyKinematic* const body0 = joint->GetBody0();
					const ndBodyKinematic* const body1 = joint->GetBody1();
					ndAssert(body0);
					ndAssert(body1);
					const ndInt32 resting = body0->m_equilibrium0 & body1->m_equilibrium0;
					if (resting)
					{
						mask[i] = ndFloat32(0.0f);
					}
				}
			}

			forceM0.m_linear.m_x = zero;
			forceM0.m_linear.m_y = zero;
			forceM0.m_linear.m_z = zero;
			forceM0.m_angular.m_x = zero;
			forceM0.m_angular.m_y = zero;
			forceM0.m_angular.m_z = zero;

			forceM1.m_linear.m_x = zero;
			forceM1.m_linear.m_y = zero;
			forceM1.m_linear.m_z = zero;
			forceM1.m_angular.m_x = zero;
			forceM1.m_angular.m_y = zero;
			forceM1.m_angular.m_z = zero;
			for (ndInt32 i = 0; i < rowsCount; ++i)
			{
				ndSoaMatrixElement* const row = &massMatrix[i];
				const ndAvxFloat force(row->m_force.Select(normalForce[i + 1], mask));
				row->m_force = force;

				forceM0.m_linear.m_x = forceM0.m_linear.m_x.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_x, force);
				forceM0.m_linear.m_y = forceM0.m_linear.m_y.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_y, force);
				forceM0.m_linear.m_z = forceM0.m_linear.m_z.MulAdd(row->m_Jt.m_jacobianM0.m_linear.m_z, force);
				forceM0.m_angular.m_x = forceM0.m_angular.m_x.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_x, force);
				forceM0.m_angular.m_y = forceM0.m_angular.m_y.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_y, force);
				forceM0.m_angular.m_z = forceM0.m_angular.m_z.MulAdd(row->m_Jt.m_jacobianM0.m_angular.m_z, force);

				forceM1.m_linear.m_x = forceM1.m_linear.m_x.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_x, force);
				forceM1.m_linear.m_y = forceM1.m_linear.m_y.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_y, force);
				forceM1.m_linear.m_z = forceM1.m_linear.m_z.MulAdd(row->m_Jt.m_jacobianM1.m_linear.m_z, force);
				forceM1.m_angular.m_x = forceM1.m_angular.m_x.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_x, force);
				forceM1.m_angular.m_y = forceM1.m_angular.m_y.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_y, force);
				forceM1.m_angular.m_z = forceM1.m_angular.m_z.MulAdd(row->m_Jt.m_jacobianM1.m_angular.m_z, force);
			}

			ndAvxFloat force0[8];
			ndAvxFloat force1[8];
			ndVector::Transpose4x4(
				force0[0].m_vector8.m_linear,
				force0[1].m_vector8.m_linear,
				force0[2].m_vector8.m_linear,
				force0[3].m_vector8.m_linear,
				forceM0.m_linear.m_x.m_vector8.m_linear,
				forceM0.m_linear.m_y.m_vector8.m_linear,
				forceM0.m_linear.m_z.m_vector8.m_linear, ndVector::m_zero);
			ndVector::Transpose4x4(
				force0[4].m_vector8.m_linear,
				force0[5].m_vector8.m_linear,
				force0[6].m_vector8.m_linear,
				force0[7].m_vector8.m_linear,
				forceM0.m_linear.m_x.m_vector8.m_angular,
				forceM0.m_linear.m_y.m_vector8.m_angular,
				forceM0.m_linear.m_z.m_vector8.m_angular, ndVector::m_zero);
			ndVector::Transpose4x4(
				force0[0].m_vector8.m_angular,
				force0[1].m_vector8.m_angular,
				force0[2].m_vector8.m_angular,
				force0[3].m_vector8.m_angular,
				forceM0.m_angular.m_x.m_vector8.m_linear,
				forceM0.m_angular.m_y.m_vector8.m_linear,
				forceM0.m_angular.m_z.m_vector8.m_linear, ndVector::m_zero);
			ndVector::Transpose4x4(
				force0[4].m_vector8.m_angular,
				force0[5].m_vector8.m_angular,
				force0[6].m_vector8.m_angular,
				force0[7].m_vector8.m_angular,
				forceM0.m_angular.m_x.m_vector8.m_angular,
				forceM0.m_angular.m_y.m_vector8.m_angular,
				forceM0.m_angular.m_z.m_vector8.m_angular, ndVector::m_zero);

			ndVector::Transpose4x4(
				force1[0].m_vector8.m_linear,
				force1[1].m_vector8.m_linear,
				force1[2].m_vector8.m_linear,
				force1[3].m_vector8.m_linear,
				forceM1.m_linear.m_x.m_vector8.m_linear,
				forceM1.m_linear.m_y.m_vector8.m_linear,
				forceM1.m_linear.m_z.m_vector8.m_linear, ndVector::m_zero);
			ndVector::Transpose4x4(
				force1[4].m_vector8.m_linear,
				force1[5].m_vector8.m_linear,
				force1[6].m_vector8.m_linear,
				force1[7].m_vector8.m_linear,
				forceM1.m_linear.m_x.m_vector8.m_angular,
				forceM1.m_linear.m_y.m_vector8.m_angular,
				forceM1.m_linear.m_z.m_vector8.m_angular, ndVector::m_zero);
			ndVector::Transpose4x4(
				force1[0].m_vector8.m_angular,
				force1[1].m_vector8.m_angular,
				force1[2].m_vector8.m_angular,
				force1[3].m_vector8.m_angular,
				forceM1.m_angular.m_x.m_vector8.m_linear,
				forceM1.m_angular.m_y.m_vector8.m_linear,
				forceM1.m_angular.m_z.m_vector8.m_linear, ndVector::m_zero);
			ndVector::Transpose4x4(
				force1[4].m_vector8.m_angular,
				force1[5].m_vector8.m_angular,
				force1[6].m_vector8.m_angular,
				force1[7].m_vector8.m_angular,
				forceM1.m_angular.m_x.m_vector8.m_angular,
				forceM1.m_angular.m_y.m_vector8.m_angular,
				forceM1.m_angular.m_z.m_vector8.m_angular, ndVector::m_zero);

			ndRightHandSide* const rightHandSide = &m_rightHandSide[0];
			for (ndInt32 i = 0; i < D_AVX_WORK_GROUP; ++i)
			{
				const ndConstraint* const joint = jointGroup[i];
				if (joint)
				{
					const ndInt32 rowCount = joint->m_rowCount;
					const ndInt32 rowStartBase = joint->m_rowStart;
					for (ndInt32 j = 0; j < rowCount; ++j)
					{
						const ndSoaMatrixElement* const row = &massMatrix[j];
						rightHandSide[j + rowStartBase].m_force = row->m_force[i];
						rightHandSide[j + rowStartBase].m_maxImpact = ndMax(ndAbs(row->m_force[i]), rightHandSide[j + rowStartBase].m_maxImpact);
					}

					const ndInt32 index0 = (block + i) * 2 + 0;
					ndAvxFloat& outBody0 = (ndAvxFloat&)jointPartialForces[index0];
					outBody0 = force0[i];

					const ndInt32 index1 = (block + i) * 2 + 1;
					ndAvxFloat& outBody1 = (ndAvxFloat&)jointPartialForces[index1];
					outBody1 = force1[i];
				}
			}
		};

		const ndInt32 mask = -ndInt32(D_AVX_WORK_GROUP);
		const ndInt32 soaJointCount = ((jointCount + D_AVX_WORK_GROUP - 1) & mask) / D_AVX_WORK_GROUP;

		for (ndInt32 i = threadIndex; i < soaJointCount; i += threadCount)
		{
			JointForce(i, &soaMassMatrix[soaJointRows[i]]);
		}
	});

	auto ApplyJacobianAccumulatePartialForces = ndMakeObject::ndFunction([this, &bodyArray](ndInt32 threadIndex, ndInt32 threadCount)
	{
		D_TRACKTIME_NAMED(ApplyJacobianAccumulatePartialForces);
		const ndAvxFloat zero(ndAvxFloat::m_zero);
		const ndInt32* const bodyIndex = &GetJointForceIndexBuffer()[0];
		ndAvxFloat* const internalForces = (ndAvxFloat*)&GetInternalForces()[0];
		const ndAvxFloat* const jointInternalForces = (ndAvxFloat*)&GetTempInternalForces()[0];
		const ndJointBodyPairIndex* const jointBodyPairIndexBuffer = &GetJointBodyPairIndexBuffer()[0];

		const ndStartEnd startEnd(bodyArray.GetCount(), threadIndex, threadCount);
		for (ndInt32 i = startEnd.m_start; i < startEnd.m_end; ++i)
		{
			ndAvxFloat force(zero);
			ndAvxFloat torque(zero);
			const ndBodyKinematic* const body = bodyArray[i];

			const ndInt32 startIndex = bodyIndex[i];
			const ndInt32 mask = body->m_isStatic - 1;
			const ndInt32 count = mask & (bodyIndex[i + 1] - startIndex);
			for (ndInt32 j = 0; j < count; ++j)
			{
				const ndInt32 index = jointBodyPairIndexBuffer[startIndex + j].m_joint;
				force = force + jointInternalForces[index];
			}
			internalForces[i] = force;
		}
	});

	for (ndInt32 i = 0; i < passes; ++i)
	{
		scene->ParallelExecute(CalculateJointsForce);
		scene->ParallelExecute(ApplyJacobianAccumulatePartialForces);
	}
}

void ndDynamicsUpdateAvx2::CalculateForces()
{
	D_TRACKTIME();
	if (m_world->GetScene()->GetActiveContactArray().GetCount())
	{
		m_firstPassCoef = ndFloat32(0.0f);

		InitSkeletons();
		for (ndInt32 step = 0; step < 4; step++)
		{
			CalculateJointsAcceleration();
			CalculateJointsForce();
			UpdateSkeletons();
			IntegrateBodiesVelocity();
		}
		
		UpdateForceFeedback();
	}
}

void ndDynamicsUpdateAvx2::Update()
{
	D_TRACKTIME();
	m_timestep = m_world->GetScene()->GetTimestep();

	BuildIsland();
	IntegrateUnconstrainedBodies();
	InitWeights();
	InitBodyArray();
	InitJacobianMatrix();
	CalculateForces();
	IntegrateBodies();
	DetermineSleepStates();
}
