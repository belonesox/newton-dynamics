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


#ifndef _N_CONFIG_H_
#define _N_CONFIG_H_


#ifdef D_CLASS_REFLECTION
	#undef D_CLASS_REFLECTION
#endif

#ifdef D_MSV_NEWTON_ALIGN_16
	#undef D_MSV_NEWTON_ALIGN_16
#endif

#ifdef D_MSV_NEWTON_ALIGN_32
	#undef D_MSV_NEWTON_ALIGN_32
#endif

#ifdef D_CORE_API 
	#undef D_CORE_API 
#endif

#ifdef D_COLLISION_API
	#undef D_COLLISION_API
#endif

#ifdef D_NEWTON_API
	#undef D_NEWTON_API
#endif

#ifdef D_OPERATOR_NEW_AND_DELETE
	#undef D_OPERATOR_NEW_AND_DELETE
#endif

#define D_CORE_API 
#define D_NEWTON_API
#define D_COLLISION_API
#define D_OPERATOR_NEW_AND_DELETE

#ifndef D_CLASS_REFLECTION
	#define D_BASE_CLASS_REFLECTION(x)
	#define D_CLASS_REFLECTION(x,y)
#endif

#ifndef D_MSV_NEWTON_ALIGN_16
	#define D_MSV_NEWTON_ALIGN_16
#endif

#ifndef D_GCC_NEWTON_ALIGN_16
	#define D_GCC_NEWTON_ALIGN_16
#endif

#ifndef D_MSV_NEWTON_ALIGN_32
	#define D_MSV_NEWTON_ALIGN_32
#endif

#ifndef D_GCC_NEWTON_ALIGN_32
	#define D_GCC_NEWTON_ALIGN_32
#endif

void dGetWorkingFileName(const char* const name, char* const outPathName);
#endif 

