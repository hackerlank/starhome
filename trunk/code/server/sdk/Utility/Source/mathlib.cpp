//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Math primitives.
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================

/// FIXME: As soon as all references to mathlib.c are gone, include it in here
#include "../stdafx.h"

#include <math.h>
#include <float.h>	// Needed for FLT_EPSILON

//#include "basetypes.h"
#include <memory.h>
#include "dbg.h"

#pragma warning(disable:4244)   // "conversion from 'const int' to 'float', possible loss of data"
#pragma warning(disable:4730)	// "mixing _m64 and floating point expressions may result in incorrect code"

#include "mathlib.h"
#include "amd3dx.h"
#include "vector3d.h"
//#include "Geo.h"
#include "def.h"
static bool s_bMathlibInitialized = false;


#ifdef PARANOID
// User must provide an implementation of Sys_Error()
void Sys_Error (char *error, ...);
#endif

const Vector vec3_origin(0,0,0);
const QAngle vec3_angle(0,0,0);
const Vector vec3_invalid( FLT_MAX, FLT_MAX, FLT_MAX );
const int nanmask = 255<<23;


// Math routines which have alternate versions for MMX/SSE/3DNOW in MATHLIB.C
// void  _VectorMA( const float *start, float scale, const float *direction, float *dest );

#ifdef PFN_VECTORMA
void  _SSE_VectorMA( const float *start, float scale, const float *direction, float *dest );
#endif

//-----------------------------------------------------------------------------
// Macros and constants required by some of the SSE assembly:
//-----------------------------------------------------------------------------

#ifdef WIN32
#define _PS_EXTERN_CONST(Name, Val) \
const __declspec(align(16)) float _ps_##Name[4] = { Val, Val, Val, Val }

#define _PS_EXTERN_CONST_TYPE(Name, Type, Val) \
const __declspec(align(16)) Type _ps_##Name[4] = { Val, Val, Val, Val }; \

#define _EPI32_CONST(Name, Val) \
static const __declspec(align(16)) __int32 _epi32_##Name[4] = { Val, Val, Val, Val }

#define _PS_CONST(Name, Val) \
	static const __declspec(align(16)) float _ps_##Name[4] = { Val, Val, Val, Val }

#else
#define _PS_EXTERN_CONST(Name, Val) \
const __attribute__((aligned(16))) float _ps_##Name[4] = { Val, Val, Val, Val }

#define _PS_EXTERN_CONST_TYPE(Name, Type, Val) \
const __attribute__((aligned(16))) Type _ps_##Name[4] = { Val, Val, Val, Val }; \

#define _EPI32_CONST(Name, Val) \
static const __attribute__((aligned(16))) int32 _epi32_##Name[4] = { Val, Val, Val, Val }

#define _PS_CONST(Name, Val) \
	static const __attribute__((aligned(16))) float _ps_##Name[4] = { Val, Val, Val, Val }

#endif

_PS_EXTERN_CONST(am_0, 0.0f);
_PS_EXTERN_CONST(am_1, 1.0f);
_PS_EXTERN_CONST(am_m1, -1.0f);
_PS_EXTERN_CONST(am_0p5, 0.5f);
_PS_EXTERN_CONST(am_1p5, 1.5f);
_PS_EXTERN_CONST(am_pi, (float)M_PI);
_PS_EXTERN_CONST(am_pi_o_2, (float)(M_PI / 2.0));
_PS_EXTERN_CONST(am_2_o_pi, (float)(2.0 / M_PI));
_PS_EXTERN_CONST(am_pi_o_4, (float)(M_PI / 4.0));
_PS_EXTERN_CONST(am_4_o_pi, (float)(4.0 / M_PI));
_PS_EXTERN_CONST_TYPE(am_sign_mask, int32, 0x80000000);
_PS_EXTERN_CONST_TYPE(am_inv_sign_mask, int32, ~0x80000000);
_PS_EXTERN_CONST_TYPE(am_min_norm_pos,int32, 0x00800000);
_PS_EXTERN_CONST_TYPE(am_mant_mask, int32, 0x7f800000);
_PS_EXTERN_CONST_TYPE(am_inv_mant_mask, int32, ~0x7f800000);

_EPI32_CONST(1, 1);
_EPI32_CONST(2, 2);

_PS_CONST(sincos_p0, 0.15707963267948963959e1f);
_PS_CONST(sincos_p1, -0.64596409750621907082e0f);
_PS_CONST(sincos_p2, 0.7969262624561800806e-1f);
_PS_CONST(sincos_p3, -0.468175413106023168e-2f);

static const uint32 _sincos_masks[]	  = { (uint32)0x0,  (uint32)~0x0 };
static const uint32 _sincos_inv_masks[] = { (uint32)~0x0, (uint32)0x0 };

//-----------------------------------------------------------------------------
// Standard C implementations of optimized routines:
//-----------------------------------------------------------------------------

//float _sqrtf(float _X)
float _sqrtf(float __X)
{
	Assert( s_bMathlibInitialized );
	return sqrtf(__X);
}

float _rsqrtf(float x)
{
	Assert( s_bMathlibInitialized );

	return 1.f / _sqrtf( x );
}

float FASTCALL _VectorNormalize (Vector& vec)
{
	Assert( s_bMathlibInitialized );
	float radius = sqrtf(vec.x*vec.x + vec.y*vec.y + vec.z*vec.z);

	// FLT_EPSILON is added to the radius to eliminate the possibility of divide by zero.
	float iradius = 1.f / ( radius + FLT_EPSILON );

	vec.x *= iradius;
	vec.y *= iradius;
	vec.z *= iradius;

	return radius;
}

// TODO: Add fast C VectorNormalizeFast.
// Perhaps use approximate rsqrt trick, if the accuracy isn't too bad.
void FASTCALL _VectorNormalizeFast (Vector& vec)
{
	Assert( s_bMathlibInitialized );

	// FLT_EPSILON is added to the radius to eliminate the possibility of divide by zero.
	float iradius = 1.f / ( sqrtf(vec.x*vec.x + vec.y*vec.y + vec.z*vec.z) + FLT_EPSILON );

	vec.x *= iradius;
	vec.y *= iradius;
	vec.z *= iradius;

}

float _InvRSquared(const float* v)
{
	Assert( s_bMathlibInitialized );
	float	r2 = DotProduct(v, v);
	return r2 < 1.f ? 1.f : 1/r2;
}

// Math routines done in optimized assembly math package routines

//-----------------------------------------------------------------------------
// 3D Now Implementations of optimized routines:
//-----------------------------------------------------------------------------
float _3DNow_Sqrt(float x)
{
	Assert( s_bMathlibInitialized );
	float	root = 0.f;
#ifdef WIN32
#ifdef X64_WIN
	assert(0);
#else
	_asm
	{
		femms
		movd		mm0, x
		PFRSQRT		(mm1,mm0)
		punpckldq	mm0, mm0
		PFMUL		(mm0, mm1)
		movd		root, mm0
		femms
	}
#endif
#else

 	__asm __volatile__( "femms" );
 	__asm __volatile__
	(
		"pfrsqrt    %y0, %y1 \n\t"
		"punpckldq   %y1, %y1 \n\t"
		"pfmul      %y1, %y0 \n\t"
		: "=y" (root), "=y" (x)
 		:"0" (x)
 	);
 	__asm __volatile__( "femms" );

#endif

	return root;
}

// NJS FIXME: Need to test Recripricol squareroot performance and accuraccy
// on AMD's before using the specialized instruction.
float _3DNow_RSqrt(float x)
{
	Assert( s_bMathlibInitialized );

	return 1.f / _3DNow_Sqrt(x);
}


float FASTCALL _3DNow_VectorNormalize (Vector& vec)
{
	Assert( s_bMathlibInitialized );
	float *v = &vec[0];
	float	radius = 0.f;

	if ( v[0] || v[1] || v[2] )
	{
#ifdef WIN32
#ifdef X64_WIN
	assert(0);
#else
	_asm
		{
			mov			eax, v
			femms
			movq		mm0, QWORD PTR [eax]
			movd		mm1, DWORD PTR [eax+8]
			movq		mm2, mm0
			movq		mm3, mm1
			PFMUL		(mm0, mm0)
			PFMUL		(mm1, mm1)
			PFACC		(mm0, mm0)
			PFADD		(mm1, mm0)
			PFRSQRT		(mm0, mm1)
			punpckldq	mm1, mm1
			PFMUL		(mm1, mm0)
			PFMUL		(mm2, mm0)
			PFMUL		(mm3, mm0)
			movq		QWORD PTR [eax], mm2
			movd		DWORD PTR [eax+8], mm3
			movd		radius, mm1
			femms
		}
#endif
#else
		long long a,c;
    		int b,d;
    		memcpy(&a,&vec[0],sizeof(a));
    		memcpy(&b,&vec[2],sizeof(b));
    		memcpy(&c,&vec[0],sizeof(c));
    		memcpy(&d,&vec[2],sizeof(d));

      		__asm __volatile__( "femms" );
        	__asm __volatile__
        	(
        		"pfmul           %y3, %y3\n\t"
        		"pfmul           %y0, %y0 \n\t"
        		"pfacc           %y3, %y3 \n\t"
        		"pfadd           %y3, %y0 \n\t"
        		"pfrsqrt         %y0, %y3 \n\t"
        		"punpckldq       %y0, %y0 \n\t"
        		"pfmul           %y3, %y0 \n\t"
        		"pfmul           %y3, %y2 \n\t"
        		"pfmul           %y3, %y1 \n\t"
        		: "=y" (radius), "=y" (c), "=y" (d)
        		: "y" (a), "0" (b), "1" (c), "2" (d)
        	);
        	__asm __volatile__( "femms" );

      		memcpy(&vec[0],&c,sizeof(c));
      		memcpy(&vec[2],&d,sizeof(d));
#endif
	}
    return radius;
}

void FASTCALL _3DNow_VectorNormalizeFast (Vector& vec)
{
	_3DNow_VectorNormalize( vec );
}

// JAY: This complains with the latest processor pack
#pragma warning(disable: 4730)

float _3DNow_InvRSquared(const float* v)
{
	Assert( s_bMathlibInitialized );
	float	r2 = 1.f;
#ifdef WIN32
#ifdef X64_WIN
	assert(0);
#else
	_asm { // AMD 3DNow only routine
		mov			eax, v
		femms
		movq		mm0, QWORD PTR [eax]
		movd		mm1, DWORD PTR [eax+8]
		movd		mm2, [r2]
		PFMUL		(mm0, mm0)
		PFMUL		(mm1, mm1)
		PFACC		(mm0, mm0)
		PFADD		(mm1, mm0)
		PFMAX		(mm1, mm2)
		PFRCP		(mm0, mm1)
		movd		[r2], mm0
		femms
	}
#endif
#else
		long long a,c;
    		int b;
    		memcpy(&a,&v[0],sizeof(a));
    		memcpy(&b,&v[2],sizeof(b));
    		memcpy(&c,&v[0],sizeof(c));

      		__asm __volatile__( "femms" );
        	__asm __volatile__
        	(
			"PFMUL          %y2, %y2 \n\t"
                        "PFMUL          %y3, %y3 \n\t"
                        "PFACC          %y2, %y2 \n\t"
                        "PFADD          %y2, %y3 \n\t"
                        "PFMAX          %y3, %y4 \n\t"
                        "PFRCP          %y3, %y2 \n\t"
                        "movq           %y2, %y0 \n\t"
        		: "=y" (r2)
        		: "0" (r2), "y" (a), "y" (b), "y" (c)
        	);
        	__asm __volatile__( "femms" );
#endif

	return r2;
}

//-----------------------------------------------------------------------------
// SSE implementations of optimized routines:
//-----------------------------------------------------------------------------
float _SSE_Sqrt(float x)
{
	Assert( s_bMathlibInitialized );
	float	root = 0.f;
#ifdef WIN32
#ifdef X64_WIN
	assert(0);
#else
	_asm
	{
		sqrtss		xmm0, x
		movss		root, xmm0
	}
#endif
#else
	__asm__ __volatile__(
		"sqrtss %X1, %%xmm0\n\t"
           	"movss %%xmm0, %X0\n\t"
       		: "=X" (root)
		: "X" (x)
		: "%xmm0"
	);
#endif
	return root;
}

//	Single iteration NewtonRaphson reciprocal square root:
// 0.5 * rsqrtps * (3 - x * rsqrtps(x) * rsqrtps(x))
// Very low error, and fine to use in place of 1.f / sqrtf(x).
#if 0
	float _SSE_RSqrtAccurate(float x)
	{
		Assert( s_bMathlibInitialized );

		float rroot;
		_asm
		{
			rsqrtss	xmm0, x
			movss	rroot, xmm0
		}

		return (0.5f * rroot) * (3.f - (x * rroot) * rroot);
	}
#else

	// Intel / Kipps SSE RSqrt.  Significantly faster than above.
	inline float _SSE_RSqrtAccurate(float a)
	{
		float x = 0;
		float half = 0.5f;
		float three = 3.f;

#ifdef WIN32
#ifdef X64_WIN
	assert(0);
#else
		__asm
		{
			movss   xmm3, a;
			movss   xmm1, half;
			movss   xmm2, three;
			rsqrtss xmm0, xmm3;

			mulss   xmm3, xmm0;
			mulss   xmm1, xmm0;
			mulss   xmm3, xmm0;
			subss   xmm2, xmm3;
			mulss   xmm1, xmm2;

			movss   x,    xmm1;
		}
#endif
#else
	 __asm__ __volatile__(
		"movss   %X1, %%xmm3 \n\t"
                "movss   %X2, %%xmm1 \n\t"
                "movss   %X3, %%xmm2 \n\t"
                "rsqrtss %%xmm3, %%xmm0 \n\t"
                "mulss   %%xmm0, %%xmm3 \n\t"
                "mulss   %%xmm0, %%xmm1 \n\t"
                "mulss   %%xmm0, %%xmm3 \n\t"
                "subss   %%xmm3, %%xmm2 \n\t"
                "mulss   %%xmm2, %%xmm1 \n\t"
                "movss   %%xmm1, %X0 \n\t"
		: "=X" (x)
		: "X" (a), "X" (half), "X" (three)
	);
#endif
		return x;
	}
#endif

// Simple SSE rsqrt.  Usually accurate to around 6 (relative) decimal places
// or so, so ok for closed transforms.  (ie, computing lighting normals)
inline float _SSE_RSqrtFast(float x)
{
	Assert( s_bMathlibInitialized );

	float rroot = 0;
#ifdef WIN32
#ifdef X64_WIN
	assert(0);
#else
	_asm
	{
		rsqrtss	xmm0, x
		movss	rroot, xmm0
	}
#endif
#else
	 __asm__ __volatile__(
		"rsqrtss %1, %%xmm0 \n\t"
		"movss %%xmm0, %0 \n\t"
		: "=X" (x)
		: "X" (rroot)
		: "%xmm0"
	);
#endif

	return rroot;
}

float FASTCALL _SSE_VectorNormalize (Vector& vec)
{
	Assert( s_bMathlibInitialized );

	// NOTE: This is necessary to prevent an memory overwrite...
	// sice vec only has 3 floats, we can't "movaps" directly into it.
#ifdef WIN32
	__declspec(align(16)) float result[4];
#else
	__attribute__((aligned(16))) float result[4];
#endif

	float *v = &vec[0];
	float *r = &result[0];

	float	radius = 0.f;
	// Blah, get rid of these comparisons ... in reality, if you have all 3 as zero, it shouldn't
	// be much of a performance win, considering you will very likely miss 3 branch predicts in a row.
	if ( v[0] || v[1] || v[2] )
	{
#ifdef WIN32
#ifdef X64_WIN
	assert(0);
#else
		_asm
		{
			mov			eax, v
			mov			edx, r
	#ifdef ALIGNED_VECTOR
			movaps		xmm4, [eax]			// r4 = vx, vy, vz, X
			movaps		xmm1, xmm4			// r1 = r4
	#else
			movups		xmm4, [eax]			// r4 = vx, vy, vz, X
			movaps		xmm1, xmm4			// r1 = r4
	#endif
			mulps		xmm1, xmm4			// r1 = vx * vx, vy * vy, vz * vz, X
			movhlps		xmm3, xmm1			// r3 = vz * vz, X, X, X
			movaps		xmm2, xmm1			// r2 = r1
			shufps		xmm2, xmm2, 1		// r2 = vy * vy, X, X, X
			addss		xmm1, xmm2			// r1 = (vx * vx) + (vy * vy), X, X, X
			addss		xmm1, xmm3			// r1 = (vx * vx) + (vy * vy) + (vz * vz), X, X, X
			sqrtss		xmm1, xmm1			// r1 = sqrt((vx * vx) + (vy * vy) + (vz * vz)), X, X, X
			movss		radius, xmm1		// radius = sqrt((vx * vx) + (vy * vy) + (vz * vz))
			rcpss		xmm1, xmm1			// r1 = 1/radius, X, X, X
			shufps		xmm1, xmm1, 0		// r1 = 1/radius, 1/radius, 1/radius, X
			mulps		xmm4, xmm1			// r4 = vx * 1/radius, vy * 1/radius, vz * 1/radius, X
			movaps		[edx], xmm4			// v = vx * 1/radius, vy * 1/radius, vz * 1/radius, X
		}
#endif
#else
	#warning "Be operating"
	#ifdef BE_OPERATING		///	Be operating
			__asm__ __volatile__(
		#ifdef ALIGNED_VECTOR
							"movaps          %X2, %%xmm4 \n\t"
							"movaps          %%xmm4, %%xmm1 \n\t"
		#else
							"movups          %X2, %%xmm4 \n\t"
							"movaps          %%xmm4, %%xmm1 \n\t"
		#endif
							"mulps           %%xmm4, %%xmm1 \n\t"
							"movhlps         %%xmm1, %%xmm3 \n\t"
							"movaps          %%xmm1, %%xmm2 \n\t"
							"shufps          $1, %%xmm2, %%xmm2 \n\t"
							"addss           %%xmm2, %%xmm1 \n\t"
							"addss           %%xmm3, %%xmm1 \n\t"
							"sqrtss          %%xmm1, %%xmm1 \n\t"
							"movss           %%xmm1, %X0 \n\t"
							"rcpss           %%xmm1, %%xmm1 \n\t"
							"shufps          $0, %%xmm1, %%xmm1 \n\t"
							"mulps           %%xmm1, %%xmm4 \n\t"
							"movaps          %%xmm4, %X1 \n\t"
							: "=X" (radius), "=X" (result)
							: "X" (*v)
			);
	#endif
#endif
		vec.x = result[0];
		vec.y = result[1];
		vec.z = result[2];

	}

	return radius;
}

void FASTCALL _SSE_VectorNormalizeFast (Vector& vec)
{
	float ool = _SSE_RSqrtAccurate( FLT_EPSILON + vec.x * vec.x + vec.y * vec.y + vec.z * vec.z );

	vec.x *= ool;
	vec.y *= ool;
	vec.z *= ool;
}

float _SSE_InvRSquared(const float* v)
{
	float	inv_r2 = 1.f;
#ifdef WIN32
#ifdef X64_WIN
	assert(0);
#else
	_asm { // Intel SSE only routine
		mov			eax, v
		movss		xmm5, inv_r2		// x5 = 1.0, 0, 0, 0
	#ifdef ALIGNED_VECTOR
		movaps		xmm4, [eax]			// x4 = vx, vy, vz, X
	#else
		movups		xmm4, [eax]			// x4 = vx, vy, vz, X
	#endif
		movaps		xmm1, xmm4			// x1 = x4
		mulps		xmm1, xmm4			// x1 = vx * vx, vy * vy, vz * vz, X
		movhlps		xmm3, xmm1			// x3 = vz * vz, X, X, X
		movaps		xmm2, xmm1			// x2 = x1
		shufps		xmm2, xmm2, 1		// x2 = vy * vy, X, X, X
		addss		xmm1, xmm2			// x1 = (vx * vx) + (vy * vy), X, X, X
		addss		xmm1, xmm3			// x1 = (vx * vx) + (vy * vy) + (vz * vz), X, X, X
		maxss		xmm1, xmm5			// x1 = max( 1.0, x1 )
		rcpss		xmm0, xmm1			// x0 = 1 / max( 1.0, x1 )
		movss		inv_r2, xmm0		// inv_r2 = x0
	}
#endif
#else
	#warning "Be operating"
	#ifdef BE_OPERATING		///	Be operating
			__asm__ __volatile__(
		#ifdef ALIGNED_VECTOR
							"movaps          %X1, %%xmm4 \n\t"
		#else
							"movups          %X1, %%xmm4 \n\t"
		#endif
							"movaps          %%xmm4, %%xmm1 \n\t"
							"mulps           %%xmm4, %%xmm1 \n\t"
				"movhlps         %%xmm1, %%xmm3 \n\t"
				"movaps          %%xmm1, %%xmm2 \n\t"
							"shufps          $1, %%xmm2, %%xmm2 \n\t"
							"addss           %%xmm2, %%xmm1 \n\t"
							"addss           %%xmm3, %%xmm1 \n\t"
				"maxss           %%xmm5, %%xmm1 \n\t"
							"rcpss           %%xmm1, %%xmm0 \n\t"
				"movss           %%xmm0, %X0 \n\t"
							: "=X" (inv_r2)
							: "X" (*v), "0" (inv_r2)
			);
	#endif
#endif

	return inv_r2;
}

#ifdef WIN32

void _SSE_SinCos(float x, float* s, float* c)
{

	float t4, t8, t12;
#ifdef X64_WIN
	assert(0);
#else
	__asm
	{
		movss	xmm0, x
		movss	t12, xmm0
		movss	xmm1, _ps_am_inv_sign_mask
		mov		eax, t12
		mulss	xmm0, _ps_am_2_o_pi
		andps	xmm0, xmm1
		and		eax, 0x80000000

		cvttss2si	edx, xmm0
		mov		ecx, edx
		mov		t12, esi
		mov		esi, edx
		add		edx, 0x1
		shl		ecx, (31 - 1)
		shl		edx, (31 - 1)

		movss	xmm4, _ps_am_1
		cvtsi2ss	xmm3, esi
		mov		t8, eax
		and		esi, 0x1

		subss	xmm0, xmm3
		movss	xmm3, _sincos_inv_masks[esi * 4]
		minss	xmm0, xmm4

		subss	xmm4, xmm0

		movss	xmm6, xmm4
		andps	xmm4, xmm3
		and		ecx, 0x80000000
		movss	xmm2, xmm3
		andnps	xmm3, xmm0
		and		edx, 0x80000000
		movss	xmm7, t8
		andps	xmm0, xmm2
		mov		t8, ecx
		mov		t4, edx
		orps	xmm4, xmm3

		mov		eax, s     //mov eax, [esp + 4 + 16]
		mov		edx, c //mov edx, [esp + 4 + 16 + 4]

		andnps	xmm2, xmm6
		orps	xmm0, xmm2

		movss	xmm2, t8
		movss	xmm1, xmm0
		movss	xmm5, xmm4
		xorps	xmm7, xmm2
		movss	xmm3, _ps_sincos_p3
		mulss	xmm0, xmm0
		mulss	xmm4, xmm4
		movss	xmm2, xmm0
		movss	xmm6, xmm4
		orps	xmm1, xmm7
		movss	xmm7, _ps_sincos_p2
		mulss	xmm0, xmm3
		mulss	xmm4, xmm3
		movss	xmm3, _ps_sincos_p1
		addss	xmm0, xmm7
		addss	xmm4, xmm7
		movss	xmm7, _ps_sincos_p0
		mulss	xmm0, xmm2
		mulss	xmm4, xmm6
		addss	xmm0, xmm3
		addss	xmm4, xmm3
		movss	xmm3, t4
		mulss	xmm0, xmm2
		mulss	xmm4, xmm6
		orps	xmm5, xmm3
		mov		esi, t12
		addss	xmm0, xmm7
		addss	xmm4, xmm7
		mulss	xmm0, xmm1
		mulss	xmm4, xmm5

		// use full stores since caller might reload with full loads
		movss	[eax], xmm0
		movss	[edx], xmm4

	}
#endif
}


float _SSE_cos( float x)
{
#ifdef X64_WIN
	assert(0);
#else
	float temp;
	__asm
	{
		movss	xmm0, x
		movss	xmm1, _ps_am_inv_sign_mask
		andps	xmm0, xmm1
		addss	xmm0, _ps_am_pi_o_2
		mulss	xmm0, _ps_am_2_o_pi

		cvttss2si	ecx, xmm0
		movss	xmm5, _ps_am_1
		mov		edx, ecx
		shl		edx, (31 - 1)
		cvtsi2ss	xmm1, ecx
		and		edx, 0x80000000
		and		ecx, 0x1

		subss	xmm0, xmm1
		movss	xmm6, _sincos_masks[ecx * 4]
		minss	xmm0, xmm5

		movss	xmm1, _ps_sincos_p3
		subss	xmm5, xmm0

		andps	xmm5, xmm6
		movss	xmm7, _ps_sincos_p2
		andnps	xmm6, xmm0
		mov		temp, edx
		orps	xmm5, xmm6
		movss	xmm0, xmm5

		mulss	xmm5, xmm5
		movss	xmm4, _ps_sincos_p1
		movss	xmm2, xmm5
		mulss	xmm5, xmm1
		movss	xmm1, _ps_sincos_p0
		addss	xmm5, xmm7
		mulss	xmm5, xmm2
		movss	xmm3, temp
		addss	xmm5, xmm4
		mulss	xmm5, xmm2
		orps	xmm0, xmm3
		addss	xmm5, xmm1
		mulss	xmm0, xmm5

		movss   x,    xmm0

	}
#endif
	return x;
}


//-----------------------------------------------------------------------------
// SSE2 implementations of optimized routines:
//-----------------------------------------------------------------------------
void _SSE2_SinCos(float x, float* s, float* c)  // any x
{
#ifdef X64_WIN
	assert(0);
#else
	__asm
	{
		movss	xmm0, x
		movaps	xmm7, xmm0
		movss	xmm1, _ps_am_inv_sign_mask
		movss	xmm2, _ps_am_sign_mask
		movss	xmm3, _ps_am_2_o_pi
		andps	xmm0, xmm1
		andps	xmm7, xmm2
		mulss	xmm0, xmm3

		pxor	xmm3, xmm3
		movd	xmm5, _epi32_1
		movss	xmm4, _ps_am_1

		cvttps2dq	xmm2, xmm0
		pand	xmm5, xmm2
		movd	xmm1, _epi32_2
		pcmpeqd	xmm5, xmm3
		movd	xmm3, _epi32_1
		cvtdq2ps	xmm6, xmm2
		paddd	xmm3, xmm2
		pand	xmm2, xmm1
		pand	xmm3, xmm1
		subss	xmm0, xmm6
		pslld	xmm2, (31 - 1)
		minss	xmm0, xmm4

		mov		eax, s     // mov eax, [esp + 4 + 16]
		mov		edx, c	   // mov edx, [esp + 4 + 16 + 4]

		subss	xmm4, xmm0
		pslld	xmm3, (31 - 1)

		movaps	xmm6, xmm4
		xorps	xmm2, xmm7
		movaps	xmm7, xmm5
		andps	xmm6, xmm7
		andnps	xmm7, xmm0
		andps	xmm0, xmm5
		andnps	xmm5, xmm4
		movss	xmm4, _ps_sincos_p3
		orps	xmm6, xmm7
		orps	xmm0, xmm5
		movss	xmm5, _ps_sincos_p2

		movaps	xmm1, xmm0
		movaps	xmm7, xmm6
		mulss	xmm0, xmm0
		mulss	xmm6, xmm6
		orps	xmm1, xmm2
		orps	xmm7, xmm3
		movaps	xmm2, xmm0
		movaps	xmm3, xmm6
		mulss	xmm0, xmm4
		mulss	xmm6, xmm4
		movss	xmm4, _ps_sincos_p1
		addss	xmm0, xmm5
		addss	xmm6, xmm5
		movss	xmm5, _ps_sincos_p0
		mulss	xmm0, xmm2
		mulss	xmm6, xmm3
		addss	xmm0, xmm4
		addss	xmm6, xmm4
		mulss	xmm0, xmm2
		mulss	xmm6, xmm3
		addss	xmm0, xmm5
		addss	xmm6, xmm5
		mulss	xmm0, xmm1
		mulss	xmm6, xmm7

		// use full stores since caller might reload with full loads
		movss	[eax], xmm0
		movss	[edx], xmm6

	}
#endif
}

float _SSE2_cos(float x)
{
#ifdef X64_WIN
	assert(0);
#else
	__asm
	{
		movss	xmm0, x
		movss	xmm1, _ps_am_inv_sign_mask
		movss	xmm2, _ps_am_pi_o_2
		movss	xmm3, _ps_am_2_o_pi
		andps	xmm0, xmm1
		addss	xmm0, xmm2
		mulss	xmm0, xmm3

		pxor	xmm3, xmm3
		movd	xmm5, _epi32_1
		movss	xmm4, _ps_am_1
		cvttps2dq	xmm2, xmm0
		pand	xmm5, xmm2
		movd	xmm1, _epi32_2
		pcmpeqd	xmm5, xmm3
		cvtdq2ps	xmm6, xmm2
		pand	xmm2, xmm1
		pslld	xmm2, (31 - 1)

		subss	xmm0, xmm6
		movss	xmm3, _ps_sincos_p3
		minss	xmm0, xmm4
		subss	xmm4, xmm0
		andps	xmm0, xmm5
		andnps	xmm5, xmm4
		orps	xmm0, xmm5

		movaps	xmm1, xmm0
		movss	xmm4, _ps_sincos_p2
		mulss	xmm0, xmm0
		movss	xmm5, _ps_sincos_p1
		orps	xmm1, xmm2
		movaps	xmm7, xmm0
		mulss	xmm0, xmm3
		movss	xmm6, _ps_sincos_p0
		addss	xmm0, xmm4
		mulss	xmm0, xmm7
		addss	xmm0, xmm5
		mulss	xmm0, xmm7
		addss	xmm0, xmm6
		mulss	xmm0, xmm1
		movss   x,    xmm0
	}
#endif
	return x;
}
#else
#warning "_SSE_sincos,_SSE_cos,_SSE2_cos,_SSE_sincos NOT implemented!"
#endif

//-----------------------------------------------------------------------------
// Function pointers selecting the appropriate implementation from above:
//-----------------------------------------------------------------------------
float (*pfSqrt)(float x)  = _sqrtf;
float (*pfRSqrt)(float x) = _rsqrtf;
float (*pfRSqrtFast)(float x) = _rsqrtf;
float (FASTCALL *pfVectorNormalize)(Vector& v) = _VectorNormalize;
void  (FASTCALL *pfVectorNormalizeFast)(Vector& v) = _VectorNormalizeFast;
float (*pfInvRSquared)(const float* v) = _InvRSquared;
void  (*pfFastSinCos)(float x, float* s, float* c) = SinCos;
float (*pfFastCos)(float x) = cosf;


float SinCosTable[SIN_TABLE_SIZE];

void InitSinCosTable()
{
	for( int i = 0; i < SIN_TABLE_SIZE; i++ )
	{
		SinCosTable[i] = sin(i * 2.0 * M_PI / SIN_TABLE_SIZE);
	}
}

#ifdef PARANOID
/*
==================
BOPS_Error

Split out like this for ASM to call.
==================
*/
void BOPS_Error (void)
{
	Sys_Error ("BoxOnPlaneSide:  Bad signbits");
}
#endif


#if	!id386 && !defined(FREEBSD)

/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
int BoxOnPlaneSide (const float *emins, const float *emaxs, const cplane_t *p)
{
	Assert( s_bMathlibInitialized );
	float	dist1, dist2;
	int		sides;

#if 0	// this is done by the BOX_ON_PLANE_SIDE macro before calling this
		// function
// fast axial cases
	if (p->type < 3)
	{
		if (p->dist <= emins[p->type])
			return 1;
		if (p->dist >= emaxs[p->type])
			return 2;
		return 3;
	}
#endif

	// general case
	switch (p->signbits)
	{
	case 0:
		dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		dist2 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		break;
	case 1:
		dist1 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		break;
	case 2:
		dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		dist2 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		break;
	case 3:
		dist1 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		break;
	case 4:
		dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		dist2 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		break;
	case 5:
		dist1 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		break;
	case 6:
		dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		dist2 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		break;
	case 7:
		dist1 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		break;
	default:
		dist1 = dist2 = 0;		// shut up compiler
#ifdef PARANOID
		BOPS_Error ();
#endif
		break;
	}

#if 0
	int		i;
	float	corners[2][3];

	for (i=0 ; i<3 ; i++)
	{
		if (plane->normal[i] < 0)
		{
			corners[0][i] = emins[i];
			corners[1][i] = emaxs[i];
		}
		else
		{
			corners[1][i] = emins[i];
			corners[0][i] = emaxs[i];
		}
	}
	dist = DotProduct (plane->normal, corners[0]) - plane->dist;
	dist2 = DotProduct (plane->normal, corners[1]) - plane->dist;
	sides = 0;
	if (dist1 >= 0)
		sides = 1;
	if (dist2 < 0)
		sides |= 2;

#endif

	sides = 0;
	if (dist1 >= p->dist)
		sides = 1;
	if (dist2 < p->dist)
		sides |= 2;

#ifdef PARANOID
if (sides == 0)
	Sys_Error ("BoxOnPlaneSide: sides==0");
#endif

	return sides;
}

#endif
//#endif

qboolean VectorsEqual( const float *v1, const float *v2 )
{
	Assert( s_bMathlibInitialized );
	return ( ( v1[0] == v2[0] ) &&
		     ( v1[1] == v2[1] ) &&
			 ( v1[2] == v2[2] ) );
}



int LineSphereIntersection(
	const Vector &vSphereCenter,
	const float fSphereRadius,
	const Vector &vLinePt,
	const Vector &vLineDir,
	float *fIntersection1,
	float *fIntersection2)
{
	Assert( s_bMathlibInitialized );
	// Line = P + Vt
	// Sphere = r (assume we've translated to origin)
	// (P + Vt)^2 = r^2
	// VVt^2 + 2PVt + (PP - r^2)
	// Solve as quadratic:  (-b  +/-  sqrt(b^2 - 4ac)) / 2a
	// If (b^2 - 4ac) is < 0 there is no solution.
	// If (b^2 - 4ac) is = 0 there is one solution (a case this function doesn't support).
	// If (b^2 - 4ac) is > 0 there are two solutions.
	Vector P;
	float a, b, c, sqr, insideSqr;


	// Translate sphere to origin.
	P[0] = vLinePt[0] - vSphereCenter[0];
	P[1] = vLinePt[1] - vSphereCenter[1];
	P[2] = vLinePt[2] - vSphereCenter[2];

	a = vLineDir.Dot(vLineDir);
	b = 2.0f * P.Dot(vLineDir);
	c = P.Dot(P) - (fSphereRadius * fSphereRadius);

	insideSqr = b*b - 4*a*c;
	if(insideSqr <= 0.000001f)
		return 0;

	// Ok, two solutions.
	sqr = (float)sqrt(insideSqr);

	*fIntersection1 = (-b + sqr) / (2.0f * a);
	*fIntersection2 = (-b - sqr) / (2.0f * a);

	return 1;
}


//-----------------------------------------------------------------------------
// Purpose: Generates Euler angles given a left-handed orientation matrix. The
//			columns of the matrix contain the forward, left, and up vectors.
// Input  : matrix - Left-handed orientation matrix.
//			angles[PITCH, YAW, ROLL]. Receives right-handed counterclockwise
//				rotations in degrees around Y, Z, and X respectively.
//-----------------------------------------------------------------------------

void MatrixAngles( const matrix3x4_t& matrix, RadianEuler &angles, Vector &position )
{
	MatrixGetColumn( matrix, 3, position );
	MatrixAngles( matrix, angles );
}

void MatrixAngles( const matrix3x4_t &matrix, Quaternion &q, Vector &pos )
{
	// FIXME: make a version that does Matrix to Quaternion directly
	RadianEuler ang;
	MatrixAngles( matrix, ang );
	AngleQuaternion( ang, q );
	MatrixGetColumn( matrix, 3, pos );
}

void MatrixAngles( const matrix3x4_t& matrix, float *angles )
{
	Assert( s_bMathlibInitialized );
	float forward[3];
	float left[3];
	float up[3];

	//
	// Extract the basis vectors from the matrix. Since we only need the Z
	// component of the up vector, we don't get X and Y.
	//
	forward[0] = matrix[0][0];
	forward[1] = matrix[1][0];
	forward[2] = matrix[2][0];
	left[0] = matrix[0][1];
	left[1] = matrix[1][1];
	left[2] = matrix[2][1];
	up[2] = matrix[2][2];

	float xyDist = sqrtf( forward[0] * forward[0] + forward[1] * forward[1] );

	// enough here to get angles?
	if ( xyDist > 0.001f )
	{
		// (yaw)	y = ATAN( forward.y, forward.x );		-- in our space, forward is the X axis
		angles[1] = RAD2DEG( atan2f( forward[1], forward[0] ) );

		// The engine does pitch inverted from this, but we always end up negating it in the DLL
		// UNDONE: Fix the engine to make it consistent
		// (pitch)	x = ATAN( -forward.z, sqrt(forward.x*forward.x+forward.y*forward.y) );
		angles[0] = RAD2DEG( atan2f( -forward[2], xyDist ) );

		// (roll)	z = ATAN( left.z, up.z );
		angles[2] = RAD2DEG( atan2f( left[2], up[2] ) );
	}
	else	// forward is mostly Z, gimbal lock-
	{
		// (yaw)	y = ATAN( -left.x, left.y );			-- forward is mostly z, so use right for yaw
		angles[1] = RAD2DEG( atan2f( -left[0], left[1] ) );

		// The engine does pitch inverted from this, but we always end up negating it in the DLL
		// UNDONE: Fix the engine to make it consistent
		// (pitch)	x = ATAN( -forward.z, sqrt(forward.x*forward.x+forward.y*forward.y) );
		angles[0] = RAD2DEG( atan2f( -forward[2], xyDist ) );

		// Assume no roll in this case as one degree of freedom has been lost (i.e. yaw == roll)
		angles[2] = 0;
	}
}


void VectorTransform (const float *in1, const matrix3x4_t& in2, float *out)
{
	Assert( s_bMathlibInitialized );
	Assert( in1 != out );
	out[0] = DotProduct(in1, in2[0]) + in2[0][3];
	out[1] = DotProduct(in1, in2[1]) + in2[1][3];
	out[2] = DotProduct(in1, in2[2]) + in2[2][3];
}

// SSE Version of VectorTransform
void VectorTransformSSE(const float *in1, const matrix3x4_t& in2, float *out1)
{
	Assert( s_bMathlibInitialized );
	Assert( in1 != out1 );

#ifdef WIN32
#ifdef X64_WIN
	assert(0);
#else
	__asm
	{
		mov eax, in1;
		mov ecx, in2;
		mov edx, out1;

		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
		addss xmm0, [ecx+12]
 		movss [edx], xmm0;
		add ecx, 16;

		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
		addss xmm0, [ecx+12]
		movss [edx+4], xmm0;
		add ecx, 16;

		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
		addss xmm0, [ecx+12]
		movss [edx+8], xmm0;
	}
#endif
#else
#warning "VectorTransformSSE C implementation only"
	out1[0] = DotProduct(in1, in2[0]) + in2[0][3];
	out1[1] = DotProduct(in1, in2[1]) + in2[1][3];
	out1[2] = DotProduct(in1, in2[2]) + in2[2][3];
#endif
}

void VectorITransform (const float *in1, const matrix3x4_t& in2, float *out)
{
	Assert( s_bMathlibInitialized );
	float in1t[3];

	in1t[0] = in1[0] - in2[0][3];
	in1t[1] = in1[1] - in2[1][3];
	in1t[2] = in1[2] - in2[2][3];

	out[0] = in1t[0] * in2[0][0] + in1t[1] * in2[1][0] + in1t[2] * in2[2][0];
	out[1] = in1t[0] * in2[0][1] + in1t[1] * in2[1][1] + in1t[2] * in2[2][1];
	out[2] = in1t[0] * in2[0][2] + in1t[1] * in2[1][2] + in1t[2] * in2[2][2];
}


void VectorRotate( const float *in1, const matrix3x4_t& in2, float *out )
{
	Assert( s_bMathlibInitialized );
	Assert( in1 != out );
	out[0] = DotProduct( in1, in2[0] );
	out[1] = DotProduct( in1, in2[1] );
	out[2] = DotProduct( in1, in2[2] );
}

void VectorRotateSSE( const float *in1, const matrix3x4_t& in2, float *out1 )
{
	Assert( s_bMathlibInitialized );
	Assert( in1 != out1 );

#ifdef WIN32
#ifdef X64_WIN
	assert(0);
#else
	__asm
	{
		mov eax, in1;
		mov ecx, in2;
		mov edx, out1;

		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
 		movss [edx], xmm0;
		add ecx, 16;

		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
		movss [edx+4], xmm0;
		add ecx, 16;

		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
		movss [edx+8], xmm0;
	}
#endif
#else
#warning "VectorRotateSSE C implementation only"
	out1[0] = DotProduct( in1, in2[0] );
	out1[1] = DotProduct( in1, in2[1] );
	out1[2] = DotProduct( in1, in2[2] );

#endif
}


//-----------------------------------------------------------------------------
// NOTE: This actually works, but we need to be sure it actually optimizes anything
// I've actually really only added it to make sure it's in source control
//-----------------------------------------------------------------------------
void TransformAndRotate( const Vector &srcPos, const Vector &srcNorm,
							const matrix3x4_t& mat, Vector &pos, Vector &norm )
{
	const float *pMat = &mat[0][0];
	const float *pPos = &srcPos.x;
	const float *pNormal = &srcNorm.x;
	float *pPosOut = &pos.x;
	float *pNormalOut = &norm.x;

//	Vector pt, nt;
//	pt[0] = DotProduct(vert.m_vecPosition.Base(), mat[0]) + mat[0][3];
//	pt[1] = DotProduct(vert.m_vecPosition.Base(), mat[1]) + mat[1][3];
//	pt[2] = DotProduct(vert.m_vecPosition.Base(), mat[2]) + mat[2][3];
//	nt[0] = DotProduct( vert.m_vecNormal.Base(), mat[0] );
//	nt[1] = DotProduct( vert.m_vecNormal.Base(), mat[1] );
//	nt[2] = DotProduct( vert.m_vecNormal.Base(), mat[2] );

#ifdef WIN32
#ifdef X64_WIN
	assert(0);
#else
	__asm
	{
		mov		eax, DWORD PTR [pMat]
		mov		edx, DWORD PTR [pPos]
		mov		ecx, DWORD PTR [pNormal]
		mov		esi, DWORD PTR [pNormalOut]
		mov		edi, DWORD PTR [pPosOut]

		fld DWORD PTR[eax + 3*4]	; m03
		fld DWORD PTR[eax + 0]		; m00		| m03
		fld DWORD PTR[edx + 0]		; pos.x		| m00		| m03
		fld DWORD PTR[ecx + 0]		; norm.x	| pos.x		| m00		| m03
		fld DWORD PTR[edx + 4]		; pos.y		| norm.x	| pos.x		| m00		| m03
		fld DWORD PTR[eax + 1*4]	; m01		| pos.y		| norm.x	| pos.x		| m00		| m03
		fld DWORD PTR[ecx + 4]		; norm.y	| m01		| pos.y		| norm.x	| pos.x		| m00		| m03

		fxch st(5)					; m00		| m01				| pos.y			| norm.x		| pos.x			| norm.y	| m03
		fmul st(4), st(0)			; m00		| m01				| pos.y			| norm.x 		| pos.x	* m00	| norm.y	| m03
		fmulp st(3), st(0)			; m01		| pos.y				| norm.x * m00	| pos.x	* m00	| norm.y		| m03
		fmul st(1), st(0)			; m01		| pos.y * m01		| norm.x * m00	| pos.x	* m00	| norm.y		| m03
		fmulp st(4), st(0)			; pos.y * m01	| norm.x * m00	| pos.x * m00	| norm.y * m01	| m03

		fld DWORD PTR[eax + 2*4]	; m02		| pos.y * m01		| norm.x * m00	| pos.x * m00		| norm.y * m01	| m03
		fld DWORD PTR[edx + 8]		; pos.z		| m02				| pos.y * m01		| norm.x * m00	| pos.x * m00		| norm.y * m01	| m03
		fld DWORD PTR[ecx + 8]		; norm.z	| pos.z					| m02				| pos.y * m01		| norm.x * m00	| pos.x * m00		| norm.y * m01	| m03

		fxch st(5)					; pos.x * m00	| pos.z							| m02						| pos.y * m01	| norm.x * m00	| norm.z				| norm.y * m01	| m03
		faddp st(3), st(0)			; pos.z			| m02							| pos.x * m00 + pos.y * m01	| norm.x * m00	| norm.z				| norm.y * m01	| m03
		fxch st(3)					; norm.x * m00	| m02							| pos.x * m00 + pos.y * m01	| pos.z			| norm.z				| norm.y * m01	| m03
		faddp st(5), st(0)			; m02			| pos.x * m00 + pos.y * m01		| pos.z						| norm.z		| norm.y * m01 + norm.x * m00		| m03
		fmul  st(2), st(0)			; m02			| pos.x * m00 + pos.y * m01		| pos.z * m02				| norm.z		| norm.y * m01 + norm.x * m00		| m03
		fmulp st(3), st(0)			; pos.x * m00 + pos.y * m01		| pos.z * m02	| norm.z * m02	| norm.y * m01 + norm.x * m00		| m03
		faddp st(4), st(0)			; pos.z * m02	| norm.z * m02	| norm.y * m01 + norm.x * m00		| pos.x * m00 + pos.y * m01	+ m03

		; NOTE: Need two more instructions before we can start using the .z stuff

		fld DWORD PTR[eax + 1*16 + 3*4]	; m13		| pos.z * m02	| norm.z * m02	| norm.y * m01 + norm.x * m00		| pos.x * m00 + pos.y * m01	+ m03
		fld DWORD PTR[eax + 1*16 + 0]	; m10		| m13		| pos.z * m02		| norm.z * m02	| norm.y * m01 + norm.x * m00	| pos.x * m00 + pos.y * m01	+ m03
		fld DWORD PTR[edx + 0]		; pos.x		| m10		| m13				| pos.z * m02	| norm.z * m02		| norm.y * m01 + norm.x * m00		| pos.x * m00 + pos.y * m01	+ m03
		fld DWORD PTR[ecx + 0]		; norm.x	| pos.x		| m10				| m13			| pos.z * m02		| norm.z * m02	| norm.y * m01 + norm.x * m00		| pos.x * m00 + pos.y * m01	+ m03

		fxch st(7)					; pos.x * m00 + pos.y * m01	+ m03 | pos.x		| m10 | m13 | pos.z * m02		| norm.z * m02	| norm.y * m01 + norm.x * m00		| norm.x
		faddp st(4), st(0)			; pos.x		| m10 | m13 | pos.x * m00 + pos.y * m01	+ pos.z * m02 + m03 	| norm.z * m02	| norm.y * m01 + norm.x * m00		| norm.x
		fxch st(5)					; norm.y * m01 + norm.x * m00		| m10 | m13 | pos.x * m00 + pos.y * m01	+ pos.z * m02 + m03 	| norm.z * m02	| pos.x		| norm.x
		faddp st(4), st(0)			; m10 | m13 | pos.x * m00 + pos.y * m01	+ pos.z * m02 + m03 	| norm.x * m00 + norm.y * m01 + norm.z * m02	| pos.x		| norm.x
		fmul st(4), st(0)			; m10 | m13 | pos.x * m00 + pos.y * m01	+ pos.z * m02 + m03 	| norm.x * m00 + norm.y * m01 + norm.z * m02	| pos.x	* m10	| norm.x
		fmulp st(5), st(0)			; m13 | pos.x * m00 + pos.y * m01	+ pos.z * m02 + m03 	| norm.x * m00 + norm.y * m01 + norm.z * m02	| pos.x	* m10	| norm.x * m10
		fxch st(2)					; norm.x * m00 + norm.y * m01 + norm.z * m02 | pos.x * m00 + pos.y * m01	+ pos.z * m02 + m03 	| m13	| pos.x	* m10	| norm.x * m10

		fstp DWORD PTR[esi]			; pos.x * m00 + pos.y * m01	+ pos.z * m02 + m03 	| m13	| pos.x	* m10	| norm.x * m10
		fstp DWORD PTR[edi]			; m13	| pos.x	* m10	| norm.x * m10

		fld DWORD PTR[eax + 1*16 + 1*4]				; m11	| m13	| pos.x	* m10	| norm.x * m10
		fld DWORD PTR[edx + 4]		; pos.y	| m11	| m13		| pos.x	* m10	| norm.x * m10
		fld DWORD PTR[ecx + 4]		; norm.y| pos.y	| m11		| m13	| pos.x	* m10	| norm.x * m10
		fld DWORD PTR[eax + 1*16 + 2*4]				; m12	| norm.y| pos.y		| m11	| m13	| pos.x	* m10	| norm.x * m10
		fld DWORD PTR[edx + 8]		; pos.z	| m12	| norm.y	| pos.y	| m11	| m13	| pos.x	* m10	| norm.x * m10

		fxch st(4)					; m11	| m12	| norm.y	| pos.y		| pos.z		| m13	| pos.x	* m10	| norm.x * m10
		fmul st(3), st(0)			; m11	| m12	| norm.y	| pos.y	* m11	| pos.z		| m13	| pos.x	* m10	| norm.x * m10
		fmulp st(2), st(0)			; m12	| norm.y * m11	| pos.y	* m11	| pos.z		| m13	| pos.x	* m10	| norm.x * m10

		fld DWORD PTR[ecx + 8]		; norm.z	| m12	| norm.y * m11	| pos.y	* m11	| pos.z		| m13	| pos.x	* m10	| norm.x * m10
		fxch st(1)					; m12	| norm.z	| norm.y * m11	| pos.y	* m11	| pos.z		| m13	| pos.x	* m10	| norm.x * m10
		fmul st(4), st(0)			; m12	| norm.z	| norm.y * m11	| pos.y	* m11	| pos.z * m12		| m13	| pos.x	* m10	| norm.x * m10
		fmulp st(1), st(0)			; norm.z * m12		| norm.y * m11	| pos.y	* m11	| pos.z * m12		| m13	| pos.x	* m10	| norm.x * m10

		fld DWORD PTR[eax + 2*16 + 3*4]				; m23	|	norm.z * m12	| norm.y * m11	| pos.y	* m11	| pos.z * m12		| m13	| pos.x	* m10	| norm.x * m10

		fxch st(6)					; pos.x	* m10	| norm.z * m12	| norm.y * m11	| pos.y	* m11	| pos.z * m12	| m13			| m23			| norm.x * m10
		faddp st(3), st(0)			; norm.z * m12	| norm.y * m11	| pos.x	* m10 + pos.y * m11	| pos.z * m12		| m13			| m23			| norm.x * m10
		fxch st(4)					; m13			| norm.y * m11	| pos.x	* m10 + pos.y * m11	| pos.z * m12		| norm.z * m12	| m23			| norm.x * m10
		faddp st(3), st(0)			; norm.y * m11	| pos.x	* m10 + pos.y * m11	| pos.z * m12 + m13	| norm.z * m12	| m23			| norm.x * m10
		faddp st(5), st(0)			; pos.x	* m10 + pos.y * m11	| pos.z * m12 + m13	| norm.z * m12	| m23			| norm.x * m10 + norm.y * m11

		fld DWORD PTR[eax + 2*16 + 0]				; m20		| pos.x	* m10 + pos.y * m11	| pos.z * m12 + m13	| norm.z * m12	| m23			| norm.x * m10 + norm.y * m11
		fld DWORD PTR[edx + 0]		; pos.x		| m20		| pos.x	* m10 + pos.y * m11	| pos.z * m12 + m13	| norm.z * m12	| m23			| norm.x * m10 + norm.y * m11
		fld DWORD PTR[ecx + 0]		; norm.x	| pos.x		| m20		| pos.x	* m10 + pos.y * m11	| pos.z * m12 + m13	| norm.z * m12	| m23			| norm.x * m10 + norm.y * m11

		fxch st(3)					; pos.x	* m10 + pos.y * m11	| pos.x		| m20		| norm.x	| pos.z * m12 + m13	| norm.z * m12	| m23			| norm.x * m10 + norm.y * m11
		faddp st(4), st(0)			; pos.x		| m20		| norm.x	| pos.x	* m10 + pos.y * m11	+ pos.z * m12 + m13	| norm.z * m12	| m23			| norm.x * m10 + norm.y * m11
		fxch st(4)					; norm.z * m12	| m20		| norm.x	| pos.x	* m10 + pos.y * m11	+ pos.z * m12 + m13	| pos.x		| m23			| norm.x * m10 + norm.y * m11
		faddp st(6), st(0)			; m20		| norm.x	| pos.x	* m10 + pos.y * m11	+ pos.z * m12 + m13	| pos.x		| m23			| norm.x * m10 + norm.y * m11 + norm.z * m12
		fmul st(3), st(0)			; m20		| norm.x	| pos.x	* m10 + pos.y * m11	+ pos.z * m12 + m13	| pos.x	* m20	| m23			| norm.x * m10 + norm.y * m11 + norm.z * m12
		fmulp st(1), st(0)			; norm.x * m20	| pos.x	* m10 + pos.y * m11	+ pos.z * m12 + m13	| pos.x	* m20	| m23			| norm.x * m10 + norm.y * m11 + norm.z * m12

		fld DWORD PTR[eax + 2*16 + 1*4]		; m21		| norm.x * m20	| pos.x	* m10 + pos.y * m11	+ pos.z * m12 + m13	| pos.x	* m20	| m23			| norm.x * m10 + norm.y * m11 + norm.z * m12
		fld DWORD PTR[edx + 4]		; pos.y		| m21		| norm.x * m20	| pos.x	* m10 + pos.y * m11	+ pos.z * m12 + m13	| pos.x	* m20	| m23			| norm.x * m10 + norm.y * m11 + norm.z * m12
		fld DWORD PTR[ecx + 4]		; norm.y	| pos.y		| m21		| norm.x * m20	| pos.x	* m10 + pos.y * m11	+ pos.z * m12 + m13	| pos.x	* m20	| m23			| norm.x * m10 + norm.y * m11 + norm.z * m12

		fxch st(4)					; pos.x	* m10 + pos.y * m11	+ pos.z * m12 + m13	| pos.y		| m21		| norm.x * m20	| norm.y	| pos.x	* m20	| m23			| norm.x * m10 + norm.y * m11 + norm.z * m12
		fstp DWORD PTR[edi + 4]		; pos.y		| m21		| norm.x * m20	| norm.y	| pos.x	* m20	| m23			| norm.x * m10 + norm.y * m11 + norm.z * m12
		fxch st(6)					; norm.x * m10 + norm.y * m11 + norm.z * m12		| m21		| norm.x * m20	| norm.y	| pos.x	* m20	| m23			| pos.y
		fstp DWORD PTR[esi + 4]		; m21		| norm.x * m20	| norm.y	| pos.x	* m20	| m23			| pos.y

		fmul st(5), st(0)			; m21			| norm.x * m20	| norm.y		| pos.x	* m20	| m23			| pos.y	* m21
		fmulp st(2), st(0)			; norm.x * m20	| norm.y * m21	| pos.x	* m20	| m23			| pos.y	* m21

		fld DWORD PTR[eax + 2*16 + 2*4]	; m22			| norm.x * m20	| norm.y * m21	| pos.x	* m20	| m23			| pos.y	* m21
		fld DWORD PTR[edx + 8]		; pos.z			| m22			| norm.x * m20	| norm.y * m21	| pos.x	* m20	| m23			| pos.y	* m21
		fld DWORD PTR[ecx + 8]		; norm.z		| pos.z			| m22			| norm.x * m20	| norm.y * m21	| pos.x	* m20	| m23			| pos.y	* m21

		fxch st(5) 					; pos.x	* m20	| pos.z			| m22			| norm.x * m20	| norm.y * m21	| norm.z		| m23			| pos.y	* m21
		faddp st(6), st(0)			; pos.z			| m22			| norm.x * m20	| norm.y * m21	| norm.z		| pos.x	* m20 + m23	| pos.y	* m21
		fxch st(2)					; norm.x * m20 	| m22			| pos.z			| norm.y * m21	| norm.z		| pos.x	* m20 + m23	| pos.y	* m21
		faddp st(3), st(0)			; m22			| pos.z			| norm.x * m20 + norm.y * m21	| norm.z		| pos.x	* m20 + m23	| pos.y	* m21
		fmul st(1), st(0)			; m22			| pos.z * m22	| norm.x * m20 + norm.y * m21	| norm.z		| pos.x	* m20 + m23	| pos.y	* m21
		fmulp st(3), st(0)			; pos.z * m22	| norm.x * m20 + norm.y * m21	| norm.z * m22	| pos.x	* m20 + m23	| pos.y	* m21
		fxch st(4)					; pos.y * m21	| norm.x * m20 + norm.y * m21	| norm.z * m22	| pos.x	* m20 + m23	| pos.z	* m22
		faddp st(3), st(0)			; norm.x * m20 + norm.y * m21	| norm.z * m22	| pos.x	* m20 + pos.y * m21 + m23	| pos.z	* m22

		; STALLS AHOY!!!

		faddp st(1), st(0)			; norm.x * m20 + norm.y * m21 + norm.z * m22	| pos.x	* m20 + pos.y * m21 + m23	| pos.z	* m22
		fxch st(2)					; pos.z	* m22									| pos.x	* m20 + pos.y * m21 + m23	| norm.x * m20 + norm.y * m21 + norm.z * m22
		faddp st(1), st(0)			; pos.x	* m20 + pos.y * m21 + pos.z	* m22 + m23	| norm.x * m20 + norm.y * m21 + norm.z * m22
		fxch st(1)					; norm.x * m20 + norm.y * m21 + norm.z * m22	| pos.x	* m20 + pos.y * m21 + pos.z	* m22 + m23
		fstp DWORD PTR[esi + 8]		; pos.x	* m20 + pos.y * m21 + pos.z	* m22 + m23
		fstp DWORD PTR[edi + 8]
	}
#endif
#else
#warning "TransformAndRotate C implementation only"
	VectorTransform( pPos, mat, pPosOut);
	VectorRotate( pPosOut, mat, pPosOut);
	norm=pos;
	VectorNormalize(norm);

#endif

}

// rotate by the inverse of the matrix
void VectorIRotate( const float *in1, const matrix3x4_t& in2, float *out )
{
	Assert( s_bMathlibInitialized );
	Assert( in1 != out );
	out[0] = in1[0]*in2[0][0] + in1[1]*in2[1][0] + in1[2]*in2[2][0];
	out[1] = in1[0]*in2[0][1] + in1[1]*in2[1][1] + in1[2]*in2[2][1];
	out[2] = in1[0]*in2[0][2] + in1[1]*in2[1][2] + in1[2]*in2[2][2];
}

QAngle TransformAnglesToLocalSpace( const QAngle &angles, matrix3x4_t &parentMatrix )
{
	matrix3x4_t angToWorld, worldToParent, localMatrix;
	MatrixInvert( parentMatrix, worldToParent );
	AngleMatrix( angles, angToWorld );
	ConcatTransforms( worldToParent, angToWorld, localMatrix );

	QAngle out;
	MatrixAngles( localMatrix, out );
	return out;
}

QAngle TransformAnglesToWorldSpace( const QAngle &angles, matrix3x4_t &parentMatrix )
{
	matrix3x4_t angToParent, angToWorld;
	AngleMatrix( angles, angToParent );
	ConcatTransforms( parentMatrix, angToParent, angToWorld );
	QAngle out;
	MatrixAngles( angToWorld, out );
	return out;
}

void MatrixCopy( const matrix3x4_t& in, matrix3x4_t& out )
{
	Assert( s_bMathlibInitialized );
	memcpy( out.Base(), in.Base(), sizeof( float ) * 3 * 4 );
}

void MatrixInvert( const matrix3x4_t& in, matrix3x4_t& out )
{
	Assert( s_bMathlibInitialized );
	if ( &in == &out )
	{
		matrix3x4_t in2;
		MatrixCopy( in, in2 );
		MatrixInvert( in2, out );
		return;
	}
	float tmp[3];

	// I'm guessing this only works on a 3x4 orthonormal matrix
	out[0][0] = in[0][0];
	out[0][1] = in[1][0];
	out[0][2] = in[2][0];

	out[1][0] = in[0][1];
	out[1][1] = in[1][1];
	out[1][2] = in[2][1];

	out[2][0] = in[0][2];
	out[2][1] = in[1][2];
	out[2][2] = in[2][2];

	tmp[0] = in[0][3];
	tmp[1] = in[1][3];
	tmp[2] = in[2][3];

	out[0][3] = -DotProduct( tmp, out[0] );
	out[1][3] = -DotProduct( tmp, out[1] );
	out[2][3] = -DotProduct( tmp, out[2] );
}

void MatrixGetColumn( const matrix3x4_t& in, int column, Vector &out )
{
	out.x = in[0][column];
	out.y = in[1][column];
	out.z = in[2][column];
}

void MatrixSetColumn( const Vector &in, int column, matrix3x4_t& out )
{
	out[0][column] = in.x;
	out[1][column] = in.y;
	out[2][column] = in.z;
}


int VectorCompare (const float *v1, const float *v2)
{
	Assert( s_bMathlibInitialized );
	int		i;

	for (i=0 ; i<3 ; i++)
		if (v1[i] != v2[i])
			return 0;

	return 1;
}

#if WIN32
#ifndef X64_WIN
_declspec(naked)
#endif
#ifndef PFN_VECTORMA
void VectorMA( const float *start, float scale, const float *direction, float *dest )
#else
void _VectorMA( const float *start, float scale, const float *direction, float *dest )
#endif
{
#ifdef X64_WIN
	assert(0);
#else
	Assert( s_bMathlibInitialized );
	_asm {
		mov	eax, DWORD PTR [esp+0x04]	; *start, s0..s2
		mov ecx, DWORD PTR [esp+0x0c]	; *direction, d0..d2
		mov edx, DWORD PTR [esp+0x10]	; *dest
		fld DWORD PTR [esp+0x08]		; sc
		fld	DWORD PTR [eax]				; s0		| sc
		fld DWORD PTR [ecx]				; d0		| s0	| sc
		fld DWORD PTR [eax+4]			; s1		| d0	| s0	| sc
		fld DWORD PTR [ecx+4]			; d1		| s1	| d0	| s0	| sc
		fld DWORD PTR [eax+8]			; s2		| d1	| s1	| d0	| s0	| sc
		fxch st(4)						; s0		| d1	| s1	| d0	| s2	| sc
		fld DWORD PTR [ecx+8]			; d2		| s0	| d1	| s1	| d0	| s2	| sc
		fxch st(4)						; d0		| s0	| d1	| s1	| d2	| s2	| sc
		fmul st,st(6)					; d0*sc		| s0	| d1	| s1	| d2	| s2	| sc
		fxch st(2)						; d1		| s0	| d0*sc	| s1	| d2	| s2	| sc
		fmul st,st(6)					; d1*sc		| s0	| d0*sc	| s1	| d2	| s2	| sc
		fxch st(4)						; d2 		| s0	| d0*sc	| s1	| d1*sc | s2	| sc
		fmulp st(6),st					; s0		| d0*sc	| s1	| d1*sc	| s2	| d2*sc
		faddp st(1),st					; s0+d0*sc	| s1	| d1*sc	| s2	| d2*sc
		fstp DWORD PTR [edx]			; s1		| d1*sc	| s2	| d2*sc
		faddp st(1),st					; s1+d1*sc	| s2	| d2*sc
		fstp DWORD PTR [edx+4]			; s2		| d2*sc
		faddp st(1),st					; s2+d2*sc
		fstp DWORD PTR [edx+8]			; [Empty stack]
		ret
	}
#endif
}
#else
#ifndef PFN_VECTORMA
void VectorMA( const float *start, float scale, const float *direction, float *dest )
#else
void _VectorMA( const float *start, float scale, const float *direction, float *dest )
#endif
{
	Assert( s_bMathlibInitialized );
	dest[0] = start[0] + scale*direction[0];
	dest[1] = start[1] + scale*direction[1];
	dest[2] = start[2] + scale*direction[2];
}
#endif

#ifdef WIN32
void
#ifndef X64_WIN
_declspec(naked)
#endif
_SSE_VectorMA( const float *start, float scale, const float *direction, float *dest )
{
#ifdef X64_WIN
	assert(0);
#else
	// FIXME: This don't work!! It will overwrite memory in the write to dest
	Assert(0);

	Assert( s_bMathlibInitialized );
	_asm {  // Intel SSE only routine
		mov	eax, DWORD PTR [esp+0x04]	; *start, s0..s2
		mov ecx, DWORD PTR [esp+0x0c]	; *direction, d0..d2
		mov edx, DWORD PTR [esp+0x10]	; *dest
		movss	xmm2, [esp+0x08]		; x2 = scale, 0, 0, 0
#ifdef ALIGNED_VECTOR
		movaps	xmm3, [ecx]				; x3 = dir0,dir1,dir2,X
		pshufd	xmm2, xmm2, 0			; x2 = scale, scale, scale, scale
		movaps	xmm1, [eax]				; x1 = start1, start2, start3, X
		mulps	xmm3, xmm2				; x3 *= x2
		addps	xmm3, xmm1				; x3 += x1
		movaps	[edx], xmm3				; *dest = x3
#else
		movups	xmm3, [ecx]				; x3 = dir0,dir1,dir2,X
		pshufd	xmm2, xmm2, 0			; x2 = scale, scale, scale, scale
		movups	xmm1, [eax]				; x1 = start1, start2, start3, X
		mulps	xmm3, xmm2				; x3 *= x2
		addps	xmm3, xmm1				; x3 += x1
		movups	[edx], xmm3				; *dest = x3
#endif
	}
#endif
}
#endif

#ifdef OLD_DOTPRODUCT

_declspec(naked)
vec_t DotProduct (const vec_t *a, const vec_t *c)
{
	Assert( s_bMathlibInitialized );
	_asm {
		mov	eax, DWORD PTR [esp+4]	; *a
		mov ecx, DWORD PTR [esp+8]	; *c
		fld DWORD PTR [eax]				; a0
		fmul DWORD PTR [ecx]			; a0*c0
		fld DWORD PTR [eax+4]			; a1	| a0*c0
		fmul DWORD PTR [ecx+4]			; a1*c1	| a0*c0
		fld DWORD PTR [eax+8]			; a2	| a1*c1 | a0*c0
		fmul DWORD PTR [ecx+8]			; a2*c2	| a1*c1 | a0*c0
		fxch st(2)						; a0*c0 | a1*c1 | a2*c2
		faddp st(1), st					; a1*c1+a0*c0	| a2*c2
		faddp st(1), st					; a2*c2+a0*c0+a1*c1
		ret
	}
}

#endif


// SSE DotProduct -- it's a smidgen faster than the asm DotProduct...
//   Should be validated too!  :)
//   NJS: (Nov 1 2002) -NOT- faster.  may time a couple cycles faster in a single function like
//   this, but when inlined, and instruction scheduled, the C version is faster.
//   Verified this via VTune

/*
vec_t DotProduct (const vec_t *a, const vec_t *c)
{
	vec_t temp;

	__asm
	{
		mov eax, a;
		mov ecx, c;
		mov edx, DWORD PTR [temp]
		movss xmm0, [eax];
		mulss xmm0, [ecx];
		movss xmm1, [eax+4];
		mulss xmm1, [ecx+4];
		movss xmm2, [eax+8];
		mulss xmm2, [ecx+8];
		addss xmm0, xmm1;
		addss xmm0, xmm2;
		movss [edx], xmm0;
		fld DWORD PTR [edx];
		ret
	}
}
*/

void CrossProduct (const float* v1, const float* v2, float* cross)
{
	Assert( s_bMathlibInitialized );
	Assert( v1 != cross );
	Assert( v2 != cross );
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}







int Q_log2(int val)
{
	int answer=0;
	while (val>>=1)
		answer++;
	return answer;
}

void VectorMatrix( const Vector &forward, matrix3x4_t& matrix)
{
	Assert( s_bMathlibInitialized );
	Vector right, left, up, tmp;

	if (forward[0] == 0 && forward[1] == 0)
	{
		left[0] = -1;
		left[1] = 0;
		left[2] = 0;
		up[0] = -forward[2];
		up[1] = 0;
		up[2] = 0;
	}
	else
	{
		tmp[0] = 0; tmp[1] = 0; tmp[2] = 1.0;
		CrossProduct( forward, tmp, right );
		VectorNormalize( right );
		CrossProduct( right, forward, up );
		VectorScale( right, -1, left );
		VectorNormalize( up );
	}
	MatrixSetColumn( forward, 0, matrix );
	MatrixSetColumn( left, 1, matrix );
	MatrixSetColumn( up, 2, matrix );
}


void VectorVectors( const Vector &forward, Vector &right, Vector &up )
{
	Assert( s_bMathlibInitialized );
	Vector tmp;

	if (forward[0] == 0 && forward[1] == 0)
	{
		right[0] = 1;
		right[1] = 0;
		right[2] = 0;
		up[0] = -forward[2];
		up[1] = 0;
		up[2] = 0;
	}
	else
	{
		tmp[0] = 0; tmp[1] = 0; tmp[2] = 1.0;
		CrossProduct( forward, tmp, right );
		VectorNormalize( right );
		CrossProduct( right, forward, up );
		VectorNormalize( up );
	}
}

void VectorAngles( const float *forward, float *angles )
{
	Assert( s_bMathlibInitialized );
	float	tmp, yaw, pitch;

	if (forward[1] == 0 && forward[0] == 0)
	{
		yaw = 0;
		if (forward[2] > 0)
			pitch = 270;
		else
			pitch = 90;
	}
	else
	{
		yaw = (atan2(forward[1], forward[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		tmp = sqrt (forward[0]*forward[0] + forward[1]*forward[1]);
		pitch = (atan2(-forward[2], tmp) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	angles[0] = pitch;
	angles[1] = yaw;
	angles[2] = 0;
}


/*
================
R_ConcatRotations
================
*/
void ConcatRotations (const float in1[3][3], const float in2[3][3], float out[3][3])
{
	Assert( s_bMathlibInitialized );
	Assert( in1 != out );
	Assert( in2 != out );
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
				in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
				in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
				in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
				in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
				in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
				in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
				in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
				in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
				in1[2][2] * in2[2][2];
}



/*******************************************************************************
* 函数名称 : ConcatTransforms
* 功能描述 : 矩阵相乘R_ConcatTransforms
* 参　　数 : [in] const matrix3x4_t& in1
* 参　　数 : [in] const matrix3x4_t& in2
* 参　　数 : [out] matrix3x4_t& out
* 返 回 值 : voidConcatTransforms
* 作　　者 : david
* 设计日期 : 2011年1月11日
* 注    意 : * 修改日期     修改人    修改内容
*******************************************************************************/
inline void ConcatTransforms (const matrix3x4_t& in1, const matrix3x4_t& in2, matrix3x4_t& out)
{
	Assert( s_bMathlibInitialized );
	if ( &in1 == &out )
	{
		matrix3x4_t in1b;
		MatrixCopy( in1, in1b );
		ConcatTransforms( in1b, in2, out );
		return;
	}
	if ( &in2 == &out )
	{
		matrix3x4_t in2b;
		MatrixCopy( in2, in2b );
		ConcatTransforms( in1, in2b, out );
		return;
	}

#if !defined(X64_WIN) && !defined(FREEBSD)

	if(MathLib_SSEEnabled())
	{
		_asm
		{
			mov edx, in2; // 这时保存的是pIn2
			movups xmm4, [edx]; //pIn2的第1行
			movups xmm5, [edx+16]; //pIn2的第2行
			movups xmm6, [edx+32]; //pIn2的第3行
			//movups xmm7, [edx+48]; //pIn2的第4行

			mov eax, in1; // 这时保存的是pIn1
			mov edx, out;

			mov ecx, 3; // 循环3次

LOOPIT: // 开始循环
			movss xmm0, [eax]; //xmm0 = pIn1->x
			shufps xmm0, xmm0, 0; //洗牌xmm0 = pIn1->x, pIn1->x, pIn1->x, pIn1->x
			mulps xmm0, xmm4;

			movss xmm1, [eax+4]; //xmm1 = pIn1->y
			shufps xmm1, xmm1, 0; //洗牌xmm1 = pIn1->y, pIn1->y, pIn1->y, pIn1->y
			mulps xmm1, xmm5;

			movss xmm2, [eax+8]; //xmm2 = pIn1->z
			shufps xmm2, xmm2, 0; //洗牌xmm2 = pIn1->z, pIn1->z, pIn1->z, pIn1->z
			mulps xmm2, xmm6;

			// 		movss xmm3, [eax+12]; //xmm3 = pIn1->w
			// 		shufps xmm3, xmm3, 0; //洗牌xmm3 = pIn1->w, pIn1->w, pIn1->w, pIn1->w
			// 		mulps xmm3, xmm7;

			addps xmm0, xmm1;
			//		addps xmm2, xmm3;
			addps xmm0, xmm2; //最终结果行保存在xmm0

			movups [edx], xmm0; //将结果保存到pOut中

			fld [eax+12];     in1[4]
			fadd [edx+12];	  in1[4] + out[4]
			fstp [edx+12];

			add edx, 16;
			add eax, 16; //作为变址用

loop LOOPIT;
			emms;
		}
	}
	else
	{
	 	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
	 				in1[0][2] * in2[2][0];
	 	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
	 				in1[0][2] * in2[2][1];
	 	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
	 				in1[0][2] * in2[2][2];
	 	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] +
	 				in1[0][2] * in2[2][3] + in1[0][3];
	 	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
	 				in1[1][2] * in2[2][0];
	 	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
	 				in1[1][2] * in2[2][1];
	 	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
	 				in1[1][2] * in2[2][2];
	 	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] +
	 				in1[1][2] * in2[2][3] + in1[1][3];
	 	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
	 				in1[2][2] * in2[2][0];
	 	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
	 				in1[2][2] * in2[2][1];
	 	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
	 				in1[2][2] * in2[2][2];
	 	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] +
	 				in1[2][2] * in2[2][3] + in1[2][3];
	}
#else
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
				in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
				in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
				in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] +
				in1[0][2] * in2[2][3] + in1[0][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
				in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
				in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
				in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] +
				in1[1][2] * in2[2][3] + in1[1][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
				in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
				in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
				in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] +
				in1[2][2] * in2[2][3] + in1[2][3];
#endif
}


/*
===================
FloorDivMod

Returns mathematically correct (floor-based) quotient and remainder for
numer and denom, both of which should contain no fractional part. The
quotient must fit in 32 bits.
====================
*/

void FloorDivMod (double numer, double denom, int *quotient,
		int *rem)
{
	Assert( s_bMathlibInitialized );
	int		q, r;
	double	x;

#ifdef PARANOID
	if (denom <= 0.0)
		Sys_Error ("FloorDivMod: bad denominator %d\n", denom);

//	if ((floor(numer) != numer) || (floor(denom) != denom))
//		Sys_Error ("FloorDivMod: non-integer numer or denom %f %f\n",
//				numer, denom);
#endif

	if (numer >= 0.0)
	{

		x = floor(numer / denom);
		q = (int)x;
		r = Floor2Int(numer - (x * denom));
	}
	else
	{
	//
	// perform operations with positive values, and fix mod to make floor-based
	//
		x = floor(-numer / denom);
		q = -(int)x;
		r = Floor2Int(-numer - (x * denom));
		if (r != 0)
		{
			q--;
			r = (int)denom - r;
		}
	}

	*quotient = q;
	*rem = r;
}


/*
===================
GreatestCommonDivisor
====================
*/
int GreatestCommonDivisor (int i1, int i2)
{
	Assert( s_bMathlibInitialized );
	if (i1 > i2)
	{
		if (i2 == 0)
			return (i1);
		return GreatestCommonDivisor (i2, i1 % i2);
	}
	else
	{
		if (i1 == 0)
			return (i2);
		return GreatestCommonDivisor (i1, i2 % i1);
	}
}


bool IsDenormal( const float &val )
{
	const int x = *reinterpret_cast <const int *> (&val); // needs 32-bit int
	const int abs_mantissa = x & 0x007FFFFF;
	const int biased_exponent = x & 0x7F800000;

	return  ( biased_exponent == 0 && abs_mantissa != 0 );
}


// ************************************** SPECIAL NOTE *******************************************************
// IFF you find this code to be PROBLEMATIC, PLEASE "#if 0" this and send email to "tkback@valvesoftware.com
// Note what machine this happened on and BUILD a debug version of the binary and see if it catches the problem.
// ***********************************************************************************************************

#if 0

#ifdef __cplusplus
extern "C" {
#endif

// This code is an TEST OPTIMIZATION for float -> long.  DISABLE this (use MSVCRT ver) if it causes problems.
//

// WARNING!!!  This is an experimental replacement for the compiler intrinsic "_ftol()'
// This code is called whenever there is a float to int conversion in c code.
// Try this 80x87 specific FPU conversion trick!  (This normally does a ROUND during FISTP by default)
// If positive, we subtract 0.5 before doing a round() which equals floor() for positive values
// If negative, we add 0.5 before doing a round() which equals ceil() for negative values.
// Together, this simulates "chop to zero" even when the FPU is in normal rounding mode.

// The following two values should be "cache line aligned" ie. align 16.
const unsigned long FPU_HALF    = 0x3EFFFFFFL;
const unsigned long	FPU_MAGIC	= 0x59C00000L;

long int
__declspec( naked )
_ftol()
{
#ifdef _DEBUG

// Turn off "Debug Multitreaded C Runtime Lib use in release mode" to avoid this.(why debug?)
//#ifdef NDEBUG
//#error Both _DEBUG and NDEBUG set!
//#endif

	Assert( s_bMathlibInitialized );
	// Make sure that we are in 64-bit precision(53-bit mantissa) FPU mode. _ftol() needs 64bit.
	_asm {
		fnstcw	word ptr [esp-4]
		and		word ptr [esp-4], 0x0300
		cmp		word ptr [esp-4], 0x0200	; Precision bits.  Must be 0x0200
		je		short good_precision
		int		3				; WRONG Precision set on FPU!!!  s/b >32bit Internally.
good_precision:
	}
#endif

	// Start the float to long conversion
	_asm {								; farg
		ftst
		fnstsw ax
		sahf
		jbe short negative

		fsub DWORD PTR [FPU_HALF]		; Subtract 0.5 from positive float, this changes round() to floor()!
		fadd DWORD PTR [FPU_MAGIC]		; Adjust the mantissa to have the "int" in the low bits.
		fstp QWORD PTR [esp-0Ch]		; Write out the floating point value in 64 bit double format
		mov	eax, DWORD PTR [esp-0Ch]	; Grab the low 32 bits of the mantissa which have our "int".
		ret

negative:
		fadd DWORD PTR [FPU_HALF]		; Add 0.5 to Negative float, this changes round() to floor()!
		fadd DWORD PTR [FPU_MAGIC]		; Adjust the mantissa to have the "int" in the low bits.
		fstp QWORD PTR [esp-0Ch]		; Write out the floating point value in 64 bit double format
		mov	eax, DWORD PTR [esp-0Ch]	; Grab the low 32 bits of the mantissa which have our "int".
		ret

	}
}

#ifdef __cplusplus
}
#endif

#endif

int SignbitsForPlane (cplane_t *out)
{
	Assert( s_bMathlibInitialized );
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}


/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
#if 1 || !id386 || defined(FREEBSD)
#ifndef X64_WIN
int BoxOnPlaneSide (const float *emins, const float *emaxs, const cplane_t *p)
{
	Assert( s_bMathlibInitialized );
	float	dist1, dist2;
	int		sides;

// fast axial cases
	if (p->type < 3)
	{
		if (p->dist <= emins[p->type])
			return 1;
		if (p->dist >= emaxs[p->type])
			return 2;
		return 3;
	}

// general case
	switch (p->signbits)
	{
	case 0:
dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
dist2 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		break;
	case 1:
dist1 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		break;
	case 2:
dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
dist2 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		break;
	case 3:
dist1 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		break;
	case 4:
dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
dist2 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		break;
	case 5:
dist1 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		break;
	case 6:
dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
dist2 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		break;
	case 7:
dist1 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		break;
	default:
		dist1 = dist2 = 0;		// shut up compiler
		Assert( 0 );
		break;
	}

	sides = 0;
	if (dist1 >= p->dist)
		sides = 1;
	if (dist2 < p->dist)
		sides |= 2;

	Assert( sides != 0 );

	return sides;
}
#endif
#else
#pragma warning( disable: 4035 )

__declspec( naked ) int BoxOnPlaneSide (const float *emins, const float *emaxs, const cplane_t *p)
{
	Assert( s_bMathlibInitialized );
	static int bops_initialized;
	static int Ljmptab[8];

	__asm {

		push ebx

		cmp bops_initialized, 1
		je  initialized
		mov bops_initialized, 1

		mov Ljmptab[0*4], offset Lcase0
		mov Ljmptab[1*4], offset Lcase1
		mov Ljmptab[2*4], offset Lcase2
		mov Ljmptab[3*4], offset Lcase3
		mov Ljmptab[4*4], offset Lcase4
		mov Ljmptab[5*4], offset Lcase5
		mov Ljmptab[6*4], offset Lcase6
		mov Ljmptab[7*4], offset Lcase7

initialized:

		mov edx,ds:dword ptr[4+12+esp]
		mov ecx,ds:dword ptr[4+4+esp]
		xor eax,eax
		mov ebx,ds:dword ptr[4+8+esp]
		mov al,ds:byte ptr[17+edx]
		cmp al,8
		jge Lerror
		fld ds:dword ptr[0+edx]
		fld st(0)
		jmp dword ptr[Ljmptab+eax*4]
Lcase0:
		fmul ds:dword ptr[ebx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ebx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase1:
		fmul ds:dword ptr[ecx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ebx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase2:
		fmul ds:dword ptr[ebx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ecx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase3:
		fmul ds:dword ptr[ecx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ecx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase4:
		fmul ds:dword ptr[ebx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ebx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase5:
		fmul ds:dword ptr[ecx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ebx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase6:
		fmul ds:dword ptr[ebx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ecx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase7:
		fmul ds:dword ptr[ecx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ecx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
LSetSides:
		faddp st(2),st(0)
		fcomp ds:dword ptr[12+edx]
		xor ecx,ecx
		fnstsw ax
		fcomp ds:dword ptr[12+edx]
		and ah,1
		xor ah,1
		add cl,ah
		fnstsw ax
		and ah,1
		add ah,ah
		add cl,ah
		pop ebx
		mov eax,ecx
		ret
Lerror:
		int 3
	}
}
#pragma warning( default: 4035 )
#endif







//-----------------------------------------------------------------------------
// VectorMA routines
//-----------------------------------------------------------------------------

#if WIN32
#ifndef X64_WIN
_declspec(naked)
#endif
	#if PFN_VECTORMA
void _VectorMA( const Vector &start, float scale, const Vector &direction, Vector &dest )
    #else
void VectorMA( const Vector &start, float scale, const Vector &direction, Vector &dest )
	#endif
{
	Assert( s_bMathlibInitialized );
#ifdef X64_WIN
	assert(0);
#else
	_asm {
		mov	eax, DWORD PTR [esp+0x04]	; *start, s0..s2
		mov ecx, DWORD PTR [esp+0x0c]	; *direction, d0..d2
		mov edx, DWORD PTR [esp+0x10]	; *dest
		fld DWORD PTR [esp+0x08]		; sc
		fld	DWORD PTR [eax]				; s0		| sc
		fld DWORD PTR [ecx]				; d0		| s0	| sc
		fld DWORD PTR [eax+4]			; s1		| d0	| s0	| sc
		fld DWORD PTR [ecx+4]			; d1		| s1	| d0	| s0	| sc
		fld DWORD PTR [eax+8]			; s2		| d1	| s1	| d0	| s0	| sc
		fxch st(4)						; s0		| d1	| s1	| d0	| s2	| sc
		fld DWORD PTR [ecx+8]			; d2		| s0	| d1	| s1	| d0	| s2	| sc
		fxch st(4)						; d0		| s0	| d1	| s1	| d2	| s2	| sc
		fmul st,st(6)					; d0*sc		| s0	| d1	| s1	| d2	| s2	| sc
		fxch st(2)						; d1		| s0	| d0*sc	| s1	| d2	| s2	| sc
		fmul st,st(6)					; d1*sc		| s0	| d0*sc	| s1	| d2	| s2	| sc
		fxch st(4)						; d2 		| s0	| d0*sc	| s1	| d1*sc | s2	| sc
		fmulp st(6),st					; s0		| d0*sc	| s1	| d1*sc	| s2	| d2*sc
		faddp st(1),st					; s0+d0*sc	| s1	| d1*sc	| s2	| d2*sc
		fstp DWORD PTR [edx]			; s1		| d1*sc	| s2	| d2*sc
		faddp st(1),st					; s1+d1*sc	| s2	| d2*sc
		fstp DWORD PTR [edx+4]			; s2		| d2*sc
		faddp st(1),st					; s2+d2*sc
		fstp DWORD PTR [edx+8]			; [Empty stack]
		ret
	}
#endif
}
#else
	#if PFN_VECTORMA
void _VectorMA( const Vector &start, float scale, const Vector &direction, Vector &dest )
    #else
void VectorMA( const Vector &start, float scale, const Vector &direction, Vector &dest )
	#endif
{
	Assert( s_bMathlibInitialized );
	dest[0] = start[0] + scale*direction[0];
	dest[1] = start[1] + scale*direction[1];
	dest[2] = start[2] + scale*direction[2];
}
#endif

#ifdef WIN32
#if PFN_VECTORMA
void
_declspec(naked)
_SSE_VectorMA( const Vector &start, float scale, const Vector &direction, Vector &dest )
{
	// FIXME: This don't work!! It will overwrite memory in the write to dest
	Assert(0);
	Assert( s_bMathlibInitialized );
	_asm
	{
		// Intel SSE only routine
		mov	eax, DWORD PTR [esp+0x04]	; *start, s0..s2
		mov ecx, DWORD PTR [esp+0x0c]	; *direction, d0..d2
		mov edx, DWORD PTR [esp+0x10]	; *dest
		movss	xmm2, [esp+0x08]		; x2 = scale, 0, 0, 0
#ifdef ALIGNED_VECTOR
		movaps	xmm3, [ecx]				; x3 = dir0,dir1,dir2,X
		pshufd	xmm2, xmm2, 0			; x2 = scale, scale, scale, scale
		movaps	xmm1, [eax]				; x1 = start1, start2, start3, X
		mulps	xmm3, xmm2				; x3 *= x2
		addps	xmm3, xmm1				; x3 += x1
		movaps	[edx], xmm3				; *dest = x3
#else
		movups	xmm3, [ecx]				; x3 = dir0,dir1,dir2,X
		pshufd	xmm2, xmm2, 0			; x2 = scale, scale, scale, scale
		movups	xmm1, [eax]				; x1 = start1, start2, start3, X
		mulps	xmm3, xmm2				; x3 *= x2
		addps	xmm3, xmm1				; x3 += x1
		movups	[edx], xmm3				; *dest = x3
#endif
	}
}

float (*pfVectorMA)(Vector& v) = _VectorMA;
#endif
#endif

//-----------------------------------------------------------------------------
// Euler QAngle -> Basis Vectors
//-----------------------------------------------------------------------------

void AngleVectors (const QAngle &angles, Vector *forward)
{
	Assert( s_bMathlibInitialized );
	Assert( forward );

	float	sp, sy, cp, cy;

#ifdef WIN32
	float y,p;
	y = XMScalarModAngle(DEG2RAD( angles[YAW] ));
	p = XMScalarModAngle(DEG2RAD( angles[PITCH] ));

	XMScalarSinCosEst(&sy, &cy, y);
	XMScalarSinCosEst(&sp, &cp, p);
#else
	SinCos( DEG2RAD( angles[YAW] ), &sy, &cy );
	SinCos( DEG2RAD( angles[PITCH] ), &sp, &cp );
#endif

	forward->x = cp*cy;
	forward->y = cp*sy;
	forward->z = -sp;
}

void AngleVectors (const QAngle &angles, Vector *forward, Vector *right, Vector *up)
{
	Assert( s_bMathlibInitialized );
	float sr, sp, sy, cr, cp, cy;

#ifdef	WIN32
	float y,p,r;
	y = XMScalarModAngle(DEG2RAD( angles[YAW] ));
	p = XMScalarModAngle(DEG2RAD( angles[PITCH] ));
	r = XMScalarModAngle(DEG2RAD( angles[ROLL] ));

	XMScalarSinCosEst(&sy, &cy, y);
	XMScalarSinCosEst(&sp, &cp, p );
	XMScalarSinCosEst(&sr, &cr, r );
#else
 	SinCos( DEG2RAD( angles[YAW] ), &sy, &cy );
 	SinCos( DEG2RAD( angles[PITCH] ), &sp, &cp );
 	SinCos( DEG2RAD( angles[ROLL] ), &sr, &cr );
#endif


	if (forward)
	{
		forward->x = cp*cy;
		forward->y = cp*sy;
		forward->z = -sp;
	}

	if (right)
	{
		right->x = (-1*sr*sp*cy+-1*cr*-sy);
		right->y = (-1*sr*sp*sy+-1*cr*cy);
		right->z = -1*sr*cp;
	}

	if (up)
	{
		up->x = (cr*sp*cy+-sr*-sy);
		up->y = (cr*sp*sy+-sr*cy);
		up->z = cr*cp;
	}
}

//-----------------------------------------------------------------------------
// Euler QAngle -> Basis Vectors transposed
//-----------------------------------------------------------------------------

void AngleVectorsTranspose (const QAngle &angles, Vector *forward, Vector *right, Vector *up)
{
	Assert( s_bMathlibInitialized );
	float sr, sp, sy, cr, cp, cy;

#ifdef WIN32
	float y,p,r;
	y = XMScalarModAngle(DEG2RAD( angles[YAW] ));
	p = XMScalarModAngle(DEG2RAD( angles[PITCH] ));
	r = XMScalarModAngle(DEG2RAD( angles[ROLL] ));

	XMScalarSinCosEst(&sy, &cy, y);
	XMScalarSinCosEst(&sp, &cp, p);
	XMScalarSinCosEst(&sr, &cr, r);
#else
 	SinCos( DEG2RAD( angles[YAW] ), &sy, &cy );
 	SinCos( DEG2RAD( angles[PITCH] ), &sp, &cp );
 	SinCos( DEG2RAD( angles[ROLL] ), &sr, &cr );
#endif

	if (forward)
	{
		forward->x	= cp*cy;
		forward->y	= (sr*sp*cy+cr*-sy);
		forward->z	= (cr*sp*cy+-sr*-sy);
	}

	if (right)
	{
		right->x	= cp*sy;
		right->y	= (sr*sp*sy+cr*cy);
		right->z	= (cr*sp*sy+-sr*cy);
	}

	if (up)
	{
		up->x		= -sp;
		up->y		= sr*cp;
		up->z		= cr*cp;
	}
}

//-----------------------------------------------------------------------------
// Forward direction vector -> Euler angles
//-----------------------------------------------------------------------------

void VectorAngles( const Vector& forward, QAngle &angles )
{
	Assert( s_bMathlibInitialized );
	float	tmp, yaw, pitch;

	if (forward[1] == 0 && forward[0] == 0)
	{
		yaw = 0;
		if (forward[2] > 0)
			pitch = 270;
		else
			pitch = 90;
	}
	else
	{
		yaw = (atan2(forward[1], forward[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		tmp = FastSqrt (forward[0]*forward[0] + forward[1]*forward[1]);
		pitch = (atan2(-forward[2], tmp) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	angles[0] = pitch;
	angles[1] = yaw;
	angles[2] = 0;
}


/*******************************************************************************
* 函数名称 : SetIdentityMatrix
* 功能描述 : 设置单位矩阵
* 参　　数 : [out] matrix3x4_t& matrix: 要被设置的矩阵
* 返 回 值 : void
* 作　　者 : david
* 设计日期 : 2011年1月11日
* 注    意 : * 修改日期     修改人    修改内容
*******************************************************************************/
void SetIdentityMatrix( matrix3x4_t& matrix )
{
	memset( matrix.Base(), 0, sizeof(float)*3*4 );
	matrix[0][0] = 1.0;
	matrix[1][1] = 1.0;
	matrix[2][2] = 1.0;
}

//-----------------------------------------------------------------------------
// Purpose: converts engine euler angles into a matrix
// Input  : vec3_t angles - PITCH, YAW, ROLL
// Output : *matrix - left-handed column matrix
//			the basis vectors for the rotations will be in the columns as follows:
//			matrix[][0] is forward
//			matrix[][1] is left
//			matrix[][2] is up
//-----------------------------------------------------------------------------
void AngleMatrix( RadianEuler const &angles, const Vector &position, matrix3x4_t& matrix )
{
	AngleMatrix( angles, matrix );
	MatrixSetColumn( position, 3, matrix );
}

void AngleMatrix( const RadianEuler& angles, matrix3x4_t& matrix )
{
	QAngle quakeEuler( RAD2DEG( angles.y ), RAD2DEG( angles.z ), RAD2DEG( angles.x ) );

	AngleMatrix( quakeEuler, matrix );
}


void AngleMatrix( const QAngle &angles, const Vector &position, matrix3x4_t& matrix )
{
	AngleMatrix( angles, matrix );
	MatrixSetColumn( position, 3, matrix );
}

/*******************************************************************************
* 函数名称 : AngleMatrix
* 功能描述 : 欧拉角 转换成 矩阵形式
* 参　　数 : [in] const QAngle &angles: 欧拉角
* 参　　数 : [out] matrix3x4_t& matrix: 转换后的矩阵形式
* 返 回 值 : void
* 作　　者 : david
* 设计日期 : 2011年1月11日
* 注    意 : * 修改日期     修改人    修改内容
*******************************************************************************/
void AngleMatrix( const QAngle &angles, matrix3x4_t& matrix )
{
	Assert( s_bMathlibInitialized );
	float		sr, sp, sy, cr, cp, cy;

#ifdef WIN32
	float y,p,r;
	y = XMScalarModAngle(DEG2RAD( angles[YAW] ));
	p = XMScalarModAngle(DEG2RAD( angles[PITCH] ));
	r = XMScalarModAngle(DEG2RAD( angles[ROLL] ));

	XMScalarSinCosEst(&sy, &cy, y);
	XMScalarSinCosEst(&sp, &cp, p);
	XMScalarSinCosEst(&sr, &cr, r);
#else
  	SinCos( DEG2RAD( angles[YAW] ), &sy, &cy );
  	SinCos( DEG2RAD( angles[PITCH] ), &sp, &cp );
  	SinCos( DEG2RAD( angles[ROLL] ), &sr, &cr );
#endif
	// matrix = (YAW * PITCH) * ROLL
 	matrix[0][0] = cp*cy;
	matrix[1][0] = cp*sy;
	matrix[2][0] = -sp;
	matrix[0][1] = sr*sp*cy+cr*-sy;
	matrix[1][1] = sr*sp*sy+cr*cy;
	matrix[2][1] = sr*cp;
	matrix[0][2] = (cr*sp*cy+-sr*-sy);
	matrix[1][2] = (cr*sp*sy+-sr*cy);
	matrix[2][2] = cr*cp;
	matrix[0][3] = 0.f;
	matrix[1][3] = 0.f;
	matrix[2][3] = 0.f;
}

void AngleIMatrix( const RadianEuler& angles, matrix3x4_t& matrix )
{
	QAngle quakeEuler( RAD2DEG( angles.y ), RAD2DEG( angles.z ), RAD2DEG( angles.x ) );

	AngleIMatrix( quakeEuler, matrix );
}

void AngleIMatrix (const QAngle& angles, matrix3x4_t& matrix )
{
	Assert( s_bMathlibInitialized );
	float		sr, sp, sy, cr, cp, cy;

#ifdef WIN32
	float y,p,r;
	y = XMScalarModAngle(DEG2RAD( angles[YAW] ));
	p = XMScalarModAngle(DEG2RAD( angles[PITCH] ));
	r = XMScalarModAngle(DEG2RAD( angles[ROLL] ));

	XMScalarSinCosEst(&sy, &cy, y);
	XMScalarSinCosEst(&sp, &cp, p);
	XMScalarSinCosEst(&sr, &cr, r);
#else
 	SinCos( DEG2RAD( angles[YAW] ), &sy, &cy );
 	SinCos( DEG2RAD( angles[PITCH] ), &sp, &cp );
 	SinCos( DEG2RAD( angles[ROLL] ), &sr, &cr );
#endif

	// matrix = (YAW * PITCH) * ROLL
	matrix[0][0] = cp*cy;
	matrix[0][1] = cp*sy;
	matrix[0][2] = -sp;
	matrix[1][0] = sr*sp*cy+cr*-sy;
	matrix[1][1] = sr*sp*sy+cr*cy;
	matrix[1][2] = sr*cp;
	matrix[2][0] = (cr*sp*cy+-sr*-sy);
	matrix[2][1] = (cr*sp*sy+-sr*cy);
	matrix[2][2] = cr*cp;
	matrix[0][3] = 0.f;
	matrix[1][3] = 0.f;
	matrix[2][3] = 0.f;
}

void AngleIMatrix (const QAngle &angles, const Vector &position, matrix3x4_t &mat )
{
	AngleIMatrix( angles, mat );

	Vector vecTranslation;
	VectorRotate( position, mat, vecTranslation );
	vecTranslation *= -1.0f;
	MatrixSetColumn( vecTranslation, 3, mat );
}


//-----------------------------------------------------------------------------
// Intersects a sphere against an axis aligned box
//-----------------------------------------------------------------------------
bool SphereToAABBIntersection( const Vector& sphCenter, float sphRadius,
			   				   const Vector& boxMin, const Vector& boxMax )
{
	Assert( s_bMathlibInitialized );
	int		i;
	float	s;
	float	dist = 0;

	for( i = 0; i < 3; i++ )
	{
		if( sphCenter[i] < boxMin[i] )
		{
			s = sphCenter[i] - boxMin[i];
			dist += s * s;
		}
		else if( sphCenter[i] > boxMax[i] )
		{
			s = sphCenter[i] - boxMax[i];
			dist += s * s;
		}
	}

	return ( dist <= ( sphRadius * sphRadius ) );
}


//-----------------------------------------------------------------------------
// Bounding box construction methods
//-----------------------------------------------------------------------------

void ClearBounds (Vector& mins, Vector& maxs)
{
	Assert( s_bMathlibInitialized );
	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;
}

void AddPointToBounds (const Vector& v, Vector& mins, Vector& maxs)
{
	Assert( s_bMathlibInitialized );
	int		i;
	vec_t	val;

	for (i=0 ; i<3 ; i++)
	{
		val = v[i];
		if (val < mins[i])
			mins[i] = val;
		if (val > maxs[i])
			maxs[i] = val;
	}
}



//-----------------------------------------------------------------------------
// Gamma conversion support
//-----------------------------------------------------------------------------
static uint8	texgammatable[256];	// palette is sent through this to convert to screen gamma

static float	texturetolinear[256];	// texture (0..255) to linear (0..1)
static int		lineartotexture[1024];	// linear (0..1) to texture (0..255)
static int		lineartoscreen[1024];	// linear (0..1) to gamma corrected vertex light (0..255)

// build a lightmap texture to combine with surface texture, adjust for src*dst+dst*src, ramp reprogramming, etc
float			lineartovertex[4096];	// linear (0..4) to screen corrected vertex space (0..1?)
static int		lineartolightmap[4096];	// linear (0..4) to screen corrected texture value (0..255)

float	power2_n[256];			// 2**(index - 128)

void BuildExponentTable( )
{
	for( int i = 0; i < 256; i++ )
	{
		power2_n[i] = pow( 2.0f, i - 128 ) / 255.0f;
	}
}

void BuildGammaTable( float gamma, float texGamma, float brightness, int overbright )
{
	int		i, inf;
	float	g1, g3;

	// Con_Printf("BuildGammaTable %.1f %.1f %.1f\n", g, v_lightgamma.GetFloat(), v_texgamma.GetFloat() );

	float g = gamma;
	if (g > 3.0)
	{
		g = 3.0;
	}

	g = 1.0 / g;
	g1 = texGamma * g;

	if (brightness <= 0.0)
	{
		g3 = 0.125;
	}
	else if (brightness > 1.0)
	{
		g3 = 0.05;
	}
	else
	{
		g3 = 0.125 - (brightness * brightness) * 0.075;
	}

	for (i=0 ; i<256 ; i++)
	{
		inf = 255 * pow ( i/255.f, g1 );
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		texgammatable[i] = inf;
	}

	for (i=0 ; i<1024 ; i++)
	{
		float f;

		f = i / 1023.0;

		// scale up
		if (brightness > 1.0)
			f = f * brightness;

		// shift up
		if (f <= g3)
			f = (f / g3) * 0.125;
		else
			f = 0.125 + ((f - g3) / (1.0 - g3)) * 0.875;

		// convert linear space to desired gamma space
		inf = 255 * pow ( f, g );

		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		lineartoscreen[i] = inf;
	}

	/*
	for (i=0 ; i<1024 ; i++)
	{
		// convert from screen gamma space to linear space
		lineargammatable[i] = 1023 * pow ( i/1023.0, v_gamma.GetFloat() );
		// convert from linear gamma space to screen space
		screengammatable[i] = 1023 * pow ( i/1023.0, 1.0 / v_gamma.GetFloat() );
	}
	*/

	for (i=0 ; i<256 ; i++)
	{
		// convert from nonlinear texture space (0..255) to linear space (0..1)
		texturetolinear[i] =  pow( i / 255.f, texGamma );
	}

	for (i=0 ; i<1024 ; i++)
	{
		// convert from linear space (0..1) to nonlinear texture space (0..255)
		lineartotexture[i] =  pow( i / 1023.0, 1.0 / texGamma ) * 255;
	}

	BuildExponentTable();

#if 0
	for (i=0 ; i<256 ; i++)
	{
		float f;

		// convert from nonlinear lightmap space (0..255) to linear space (0..4)
		// f =  (i / 255.0) * sqrt( 4 );
		f =  i * (2.0 / 255.0);
		f = f * f;

		texlighttolinear[i] = f;
	}
#endif

	{
		float f;
		float overbrightFactor = 1.0f;

		// Can't do overbright without texcombine
		// UNDONE: Add GAMMA ramp to rectify this
		if ( overbright == 2 )
		{
			overbrightFactor = 0.5;
		}
		else if ( overbright == 4 )
		{
			overbrightFactor = 0.25;
		}

		for (i=0 ; i<4096 ; i++)
		{
			// convert from linear 0..4 (x1024) to screen corrected vertex space (0..1?)
			f = pow ( i/1024.0, 1.0 / gamma );

			lineartovertex[i] = f * overbrightFactor;
			if (lineartovertex[i] > 1)
				lineartovertex[i] = 1;

			lineartolightmap[i] = f * 255 * overbrightFactor;
			if (lineartolightmap[i] > 255)
				lineartolightmap[i] = 255;
		}
	}
}


// convert texture to linear 0..1 value
float TextureToLinear( int c )
{
	Assert( s_bMathlibInitialized );
	if (c < 0)
		return 0;
	if (c > 255)
		return 1.0;

	return texturetolinear[c];
}

// convert texture to linear 0..1 value
int LinearToTexture( float f )
{
	Assert( s_bMathlibInitialized );
	int i;
	i = f * 1023;	// assume 0..1 range
	if (i < 0)
		i = 0;
	if (i > 1023)
		i = 1023;

	return lineartotexture[i];
}


int LinearToLightmap( float f )
{
	Assert( s_bMathlibInitialized );
	int i;
	i = f * 1024;	// assume 0..4 range
	if (i < 0)
		i = 0;
	else if (i > 4095)
		i = 4095;

	return lineartolightmap[i];
}

// converts 0..1 linear value to screen gamma (0..255)
int LinearToScreenGamma( float f )
{
	Assert( s_bMathlibInitialized );
	int i;
	i = f * 1023;	// assume 0..1 range
	if (i < 0)
		i = 0;
	if (i > 1023)
		i = 1023;

	return lineartoscreen[i];
}

// solve a x^2 + b x + c = 0
bool SolveQuadratic( float a, float b, float c, float &root1, float &root2 )
{
	Assert( s_bMathlibInitialized );
	if (a == 0)
	{
		if (b != 0)
		{
			// no x^2 component, it's a linear system
			root1 = root2 = -c / b;
			return true;
		}
		if (c == 0)
		{
			// all zero's
			root1 = root2 = 0;
			return true;
		}
		return false;
	}

	float tmp = b * b - 4.0f * a * c;

	if (tmp < 0)
	{
		// imaginary number, bah, no solution.
		return false;
	}

	tmp = sqrt( tmp );
	root1 = (-b + tmp) / (2.0f * a);
	root2 = (-b - tmp) / (2.0f * a);
	return true;
}

// Rotate a vector around the Z axis (YAW)
void VectorYawRotate( const Vector &in, float flYaw, Vector &out)
{
	Assert( s_bMathlibInitialized );
	if (&in == &out )
	{
		Vector tmp = in;
		VectorYawRotate( tmp, flYaw, out );
		return;
	}

	float sy, cy;

#ifdef WIN32
	float y;
	y = XMScalarModAngle(DEG2RAD(flYaw));
	XMScalarSinCosEst(&sy, &cy, y);
#else
	SinCos( DEG2RAD(flYaw), &sy, &cy );
#endif

	out.x = in.x * cy - in.y * sy;
	out.y = in.x * sy + in.y * cy;
	out.z = in.z;
}



float Bias( float x, float biasAmt )
{
	// WARNING: not thread safe
	static float lastAmt = -1;
	static float lastExponent = 0;
	if( lastAmt != biasAmt )
	{
		lastExponent = log( biasAmt ) * -1.4427f; // (-1.4427 = 1 / log(0.5))
	}
	return pow( x, lastExponent );
}


float Gain( float x, float biasAmt )
{
	// WARNING: not thread safe
	if( x < 0.5 )
		return 0.5f * Bias( 2*x, 1-biasAmt );
	else
		return 1 - 0.5f * Bias( 2 - 2*x, 1-biasAmt );
}


float SmoothCurve( float x )
{
	return (1 - cos( x * M_PI )) * 0.5f;
}


inline float MovePeak( float x, float flPeakPos )
{
	// Todo: make this higher-order?
	if( x < flPeakPos )
		return x * 0.5f / flPeakPos;
	else
		return 0.5 + 0.5 * (x - flPeakPos) / (1 - flPeakPos);
}


float SmoothCurve_Tweak( float x, float flPeakPos, float flPeakSharpness )
{
	float flMovedPeak = MovePeak( x, flPeakPos );
	float flSharpened = Gain( flMovedPeak, flPeakSharpness );
	return SmoothCurve( flSharpened );
}

//-----------------------------------------------------------------------------
// make sure quaternions are within 180 degrees of one another, if not, reverse q
//-----------------------------------------------------------------------------

void QuaternionAlign( const Quaternion &p, const Quaternion &q, Quaternion &qt )
{
	Assert( s_bMathlibInitialized );

	// FIXME: can this be done with a quat dot product?

	int i;
	// decide if one of the quaternions is backwards
	float a = 0;
	float b = 0;
	for (i = 0; i < 4; i++)
	{
		a += (p[i]-q[i])*(p[i]-q[i]);
		b += (p[i]+q[i])*(p[i]+q[i]);
	}
	if (a > b)
	{
		for (i = 0; i < 4; i++)
		{
			qt[i] = -q[i];
		}
	}
	else if (&qt != &q)
	{
		for (i = 0; i < 4; i++)
		{
			qt[i] = q[i];
		}
	}
}


//-----------------------------------------------------------------------------
// Do a piecewise addition of the quaternion elements. This actually makes little
// mathematical sense, but it's a cheap way to simulate a slerp.
//-----------------------------------------------------------------------------

void QuaternionBlend( const Quaternion &p, const Quaternion &q, float t, Quaternion &qt )
{
	Assert( s_bMathlibInitialized );

	// decide if one of the quaternions is backwards
	Quaternion q2;
	QuaternionAlign( p, q, q2 );

	QuaternionBlendNoAlign( p, q2, t, qt );
}


void QuaternionBlendNoAlign( const Quaternion &p, const Quaternion &q, float t, Quaternion &qt )
{
	Assert( s_bMathlibInitialized );
	float sclp, sclq;
	int i;

	// 0.0 returns p, 1.0 return q.
	sclp = 1.0f - t;
	sclq = t;
	for (i = 0; i < 4; i++) {
		qt[i] = sclp * p[i] + sclq * q[i];
	}
	QuaternionNormalize( qt );
}

//-----------------------------------------------------------------------------
// Quaternion sphereical linear interpolation
//-----------------------------------------------------------------------------

void QuaternionSlerp( const Quaternion &p, const Quaternion &q, float t, Quaternion &qt )
{
	Quaternion q2;
	// 0.0 returns p, 1.0 return q.

	// decide if one of the quaternions is backwards
	QuaternionAlign( p, q, q2 );

	QuaternionSlerpNoAlign( p, q2, t, qt );
}


void QuaternionSlerpNoAlign( const Quaternion &p, const Quaternion &q, float t, Quaternion &qt )
{
	Assert( s_bMathlibInitialized );
	float omega, cosom, sinom, sclp, sclq;
	int i;

	// 0.0 returns p, 1.0 return q.

	cosom = p[0]*q[0] + p[1]*q[1] + p[2]*q[2] + p[3]*q[3];

	if ((1.0f + cosom) > 0.000001f) {
		if ((1.0f - cosom) > 0.000001f) {
			omega = acos( cosom );
			sinom = sin( omega );
			sclp = sin( (1.0f - t)*omega) / sinom;
			sclq = sin( t*omega ) / sinom;
		}
		else {
			// TODO: add short circuit for cosom == 1.0f?
			sclp = 1.0f - t;
			sclq = t;
		}
		for (i = 0; i < 4; i++) {
			qt[i] = sclp * p[i] + sclq * q[i];
		}
	}
	else {
		Assert( &qt != &q );

		qt[0] = -q[1];
		qt[1] = q[0];
		qt[2] = -q[3];
		qt[3] = q[2];
		sclp = sin( (1.0f - t) * (0.5f * M_PI));
		sclq = sin( t * (0.5f * M_PI));
		for (i = 0; i < 3; i++) {
			qt[i] = sclp * p[i] + sclq * qt[i];
		}
	}

	Assert( qt.IsValid() );
}


//-----------------------------------------------------------------------------
// Make sure the quaternion is of unit length
//-----------------------------------------------------------------------------

float QuaternionNormalize( Quaternion &q )
{
	Assert( s_bMathlibInitialized );
	float radius, iradius;

	Assert( q.IsValid() );

	radius = q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3];

	if ( radius ) // > FLT_EPSILON && radius < 1.0f - 4*FLT_EPSILON)
	{
		radius = sqrt(radius);
		iradius = 1.0f/radius;
		q[3] *= iradius;
		q[2] *= iradius;
		q[1] *= iradius;
		q[0] *= iradius;
	}
	return radius;
}

void QuaternionScale( const Quaternion &p, float t, Quaternion &q )
{
	Assert( s_bMathlibInitialized );

#if 0
	Quaternion p0;
	Quaternion q;
	p0.Init( 0.0, 0.0, 0.0, 1.0 );

	// slerp in "reverse order" so that p doesn't get realigned
	QuaternionSlerp( p, p0, 1.0 - fabs( t ), q );
	if (t < 0.0)
	{
		q.w = -q.w;
	}
#else
	float r;

	// FIXME: nick, this isn't overly sensitive to accuracy, and it may be faster to
	// use the cos part (w) of the quaternion (sin(omega)*N,cos(omega)) to figure the new scale.
	float sinom = sqrt( DotProduct( &p.x, &p.x ) );
	sinom = MIN( sinom, 1.f );

	float sinsom = sin( asin( sinom ) * t );

	t = sinsom / (sinom + FLT_EPSILON);
	VectorScale( &p.x, t, &q.x );

	// rescale rotation
	r = 1.0f - sinsom * sinsom;

	// Assert( r >= 0 );
	if (r < 0.0f)
		r = 0.0f;
	r = sqrt( r );

	// keep sign of rotation
	if (p.w < 0)
		q.w = -r;
	else
		q.w = r;
#endif

	Assert( q.IsValid() );

	return;
}


void QuaternionAdd( const Quaternion &p, const Quaternion &q, Quaternion &qt )
{
	Assert( s_bMathlibInitialized );
	Assert( p.IsValid() );
	Assert( q.IsValid() );

	// decide if one of the quaternions is backwards
	Quaternion q2;
	QuaternionAlign( p, q, q2 );

	// is this right???
	qt[0] = p[0] + q2[0];
	qt[1] = p[1] + q2[1];
	qt[2] = p[2] + q2[2];
	qt[3] = p[3] + q2[3];

	return;
}


float QuaternionDotProduct( const Quaternion &p, const Quaternion &q )
{
	Assert( s_bMathlibInitialized );
	Assert( p.IsValid() );
	Assert( q.IsValid() );

	return p.x * q.x + p.y * q.y + p.z * q.z + p.w * q.w;
}


// qt = p * q
void QuaternionMult( const Quaternion &p, const Quaternion &q, Quaternion &qt )
{
	Assert( s_bMathlibInitialized );
	Assert( p.IsValid() );
	Assert( q.IsValid() );

	if (&p == &qt)
	{
		Quaternion p2 = p;
		QuaternionMult( p2, q, qt );
		return;
	}

	// decide if one of the quaternions is backwards
	Quaternion q2;
	QuaternionAlign( p, q, q2 );

	qt.x =  p.x * q2.w + p.y * q2.z - p.z * q2.y + p.w * q2.x;
	qt.y = -p.x * q2.z + p.y * q2.w + p.z * q2.x + p.w * q2.y;
	qt.z =  p.x * q2.y - p.y * q2.x + p.z * q2.w + p.w * q2.z;
	qt.w = -p.x * q2.x - p.y * q2.y - p.z * q2.z + p.w * q2.w;
}


void QuaternionMatrix( const Quaternion &q, const Vector &pos, matrix3x4_t& matrix )
{
	QuaternionMatrix( q, matrix );

	matrix[0][3] = pos.x;
	matrix[1][3] = pos.y;
	matrix[2][3] = pos.z;
}

void QuaternionMatrix( const Quaternion &q, matrix3x4_t& matrix )
{
	Assert( s_bMathlibInitialized );
	// Original code
	// This should produce the same code as below with optimization, but looking at the assmebly,
	// it doesn't.  There are 7 extra multiplies in the release build of this, go figure.
#if 1
	matrix[0][0] = 1.0 - 2.0 * q.y * q.y - 2.0 * q.z * q.z;
	matrix[1][0] = 2.0 * q.x * q.y + 2.0 * q.w * q.z;
	matrix[2][0] = 2.0 * q.x * q.z - 2.0 * q.w * q.y;

	matrix[0][1] = 2.0f * q.x * q.y - 2.0f * q.w * q.z;
	matrix[1][1] = 1.0f - 2.0f * q.x * q.x - 2.0f * q.z * q.z;
	matrix[2][1] = 2.0f * q.y * q.z + 2.0f * q.w * q.x;

	matrix[0][2] = 2.0f * q.x * q.z + 2.0f * q.w * q.y;
	matrix[1][2] = 2.0f * q.y * q.z - 2.0f * q.w * q.x;
	matrix[2][2] = 1.0f - 2.0f * q.x * q.x - 2.0f * q.y * q.y;
#else
	float wx, wy, wz, xx, yy, yz, xy, xz, zz, x2, y2, z2;

	// precalculate common multiplitcations
	x2 = q.x + q.x;
	y2 = q.y + q.y;
	z2 = q.z + q.z;
	xx = q.x * x2;
	xy = q.x * y2;
	xz = q.x * z2;
	yy = q.y * y2;
	yz = q.y * z2;
	zz = q.z * z2;
	wx = q.w * x2;
	wy = q.w * y2;
	wz = q.w * z2;

	matrix[0][0] = 1.0 - (yy + zz);
	matrix[0][1] = xy - wz;
	matrix[0][2] = xz + wy;

	matrix[1][0] = xy + wz;
	matrix[1][1] = 1.0 - (xx + zz);
	matrix[1][2] = yz - wx;

	matrix[2][0] = xz - wy;
	matrix[2][1] = yz + wx;
	matrix[2][2] = 1.0 - (xx + yy);
#endif
}

void QuaternionMatrixMulScal( const Quaternion &q, Vector &scal ,matrix3x4_t& matrix )
{

	Assert( s_bMathlibInitialized );

	matrix[0][0] = (1.0 - 2.0 * q.y * q.y - 2.0 * q.z * q.z) * scal.x;
	matrix[1][0] = (2.0 * q.x * q.y + 2.0 * q.w * q.z) * scal.x;
	matrix[2][0] = (2.0 * q.x * q.z - 2.0 * q.w * q.y) * scal.x;

	matrix[0][1] = (2.0f * q.x * q.y - 2.0f * q.w * q.z) * scal.y;
	matrix[1][1] = (1.0f - 2.0f * q.x * q.x - 2.0f * q.z * q.z) * scal.y;
	matrix[2][1] = (2.0f * q.y * q.z + 2.0f * q.w * q.x) * scal.y;

	matrix[0][2] = (2.0f * q.x * q.z + 2.0f * q.w * q.y) * scal.z;
	matrix[1][2] = (2.0f * q.y * q.z - 2.0f * q.w * q.x) * scal.z;
	matrix[2][2] = (1.0f - 2.0f * q.x * q.x - 2.0f * q.y * q.y) * scal.z;

}
//-----------------------------------------------------------------------------
// Purpose: Converts engine-format euler angles to a quaternion
// Input  : angles - Right-handed Euler angles in degrees as follows:
//				[0]: PITCH: Clockwise rotation around the Y axis.
//				[1]: YAW:	Counterclockwise rotation around the Z axis.
//				[2]: ROLL:	Counterclockwise rotation around the X axis.
//			*outQuat - quaternion of form (i,j,k,real)
//-----------------------------------------------------------------------------
void AngleQuaternion( const QAngle &angles, Quaternion &outQuat )
{
	RadianEuler radians;
	radians.Init( DEG2RAD( angles.z ), DEG2RAD( angles.x ), DEG2RAD( angles.y ) );

	AngleQuaternion( radians, outQuat );
}

// inline void AngleQuaternionEst(const QAngle &angles, Quaternion &outQuat)
// {
//
// 	__m128 angle = _mm_set_ps(angles.z,angles.x,angles.y,0.0f);
// 	__m128 half = _mm_set_ps1(0.00872664f);
// 	__m128 mark = _mm_set_ps(1,-1,-1,1);
// 	angle = _mm_mul_ps(angle, half);
// 	angle = XMVectorModAngles(angle);
// 	__m128 pSin;
// 	__m128 pCos;
// 	XMVectorSinCosEst(&pSin,&pCos,angle);
//
// 	__m128 s1,s2,s3,c1,c2,c3;
// 	s1 = _mm_shuffle_ps(pSin,pSin,_MM_SHUFFLE(3, 2, 1, 3));
// 	s2 = _mm_shuffle_ps(pCos,pSin,_MM_SHUFFLE(2, 1, 3, 2));
// 	s3 = _mm_shuffle_ps(pCos,pSin,_MM_SHUFFLE(1, 1, 3, 2));
// 	s3 = _mm_shuffle_ps(s3,s3,_MM_SHUFFLE(3, 1, 0, 3));
// 	c1 = _mm_shuffle_ps(pCos,pCos,_MM_SHUFFLE(3, 2, 1, 3));
// 	c2 = _mm_shuffle_ps(pSin,pCos,_MM_SHUFFLE(2, 1, 3, 2));
// 	c3 = _mm_shuffle_ps(pSin,pCos,_MM_SHUFFLE(1, 1, 3, 2));
// 	c3 = _mm_shuffle_ps(c3,c3,_MM_SHUFFLE(3, 1, 0, 3));
//
// 	s1 = _mm_mul_ps(s1,s2);
// 	s1 = _mm_mul_ps(s1,s3);
// 	c1 = _mm_mul_ps(c1,c2);
// 	c1 = _mm_mul_ps(c1,c3);
// 	c1 = _mm_mul_ps(c1,mark);
//
// 	s1 = _mm_add_ps(s1,c1);
//
// 	outQuat.Init(-s1.m128_f32[2],s1.m128_f32[0],s1.m128_f32[1],s1.m128_f32[3]);
// }

//-----------------------------------------------------------------------------
// Purpose: Converts a quaternion into engine angles
// Input  : *quaternion - q3 + q0.i + q1.j + q2.k
//			*outAngles - PITCH, YAW, ROLL
//-----------------------------------------------------------------------------
void QuaternionAngles( const Quaternion &q, QAngle &angles )
{
	Assert( s_bMathlibInitialized );
	Assert( q.IsValid() );

#if 1
	// FIXME: doing it this way calculates too much data, needs to do an optimized version...
	matrix3x4_t matrix;
	QuaternionMatrix( q, matrix );
	MatrixAngles( matrix, angles );
#else
	float m11, m12, m13, m23, m33;

	m11 = ( 2.0f * q.w * q.w ) + ( 2.0f * q.x * q.x ) - 1.0f;
	m12 = ( 2.0f * q.x * q.y ) + ( 2.0f * q.w * q.z );
	m13 = ( 2.0f * q.x * q.z ) - ( 2.0f * q.w * q.y );
	m23 = ( 2.0f * q.y * q.z ) + ( 2.0f * q.w * q.x );
	m33 = ( 2.0f * q.w * q.w ) + ( 2.0f * q.z * q.z ) - 1.0f;

	// FIXME: this code has a singularity near PITCH +-90
	angles[YAW] = RAD2DEG( atan2(m12, m11) );
	angles[PITCH] = RAD2DEG( asin(-m13) );
	angles[ROLL] = RAD2DEG( atan2(m23, m33) );
#endif

	Assert( angles.IsValid() );
}

//-----------------------------------------------------------------------------
// Purpose: Converts a quaternion to an axis / angle in degrees
//			(exponential map)
//-----------------------------------------------------------------------------
void QuaternionAxisAngle( const Quaternion &q, Vector &axis, float &angle )
{
	angle = RAD2DEG(2 * acos(q.w));
	if ( angle > 180 )
	{
		angle -= 360;
	}
	axis.x = q.x;
	axis.y = q.y;
	axis.z = q.z;
	VectorNormalize( axis );
}

//-----------------------------------------------------------------------------
// Purpose: Converts an exponential map (ang/axis) to a quaternion
//-----------------------------------------------------------------------------
void AxisAngleQuaternion( const Vector &axis, float angle, Quaternion &q )
{
	float sa, ca;

#ifdef WIN32
	float y;
	y = XMScalarModAngle(DEG2RAD(angle) * 0.5f);
	XMScalarSinCosEst(&sa, &ca ,y);
#else
	SinCos( DEG2RAD(angle) * 0.5f, &sa, &ca );
#endif

	q.x = axis.x * sa;
	q.y = axis.y * sa;
	q.z = axis.z * sa;
	q.w = ca;
}


//-----------------------------------------------------------------------------
// Purpose: Converts radian-euler axis aligned angles to a quaternion
// Input  : *pfAngles - Right-handed Euler angles in radians
//			*outQuat - quaternion of form (i,j,k,real)
//-----------------------------------------------------------------------------
#ifdef WIN32
__forceinline void AngleQuaternion( RadianEuler const &angles, Quaternion &outQuat )
#else
void AngleQuaternion( RadianEuler const &angles, Quaternion &outQuat )
#endif
{
	Assert( s_bMathlibInitialized );
//	Assert( angles.IsValid() );

#ifdef WIN32
	__m128 angle = _mm_set_ps(angles.x,angles.y,angles.z,0.0f);
	__m128 half = _mm_set_ps1(0.5f);
	__m128 mark = _mm_set_ps(1,-1,-1,1);
	angle = _mm_mul_ps(angle, half);
	angle = XMVectorModAngles(angle);
	__m128 pSin;
	__m128 pCos;
	XMVectorSinCosEst(&pSin,&pCos,angle);

	__m128 s1,s2,s3,c1,c2,c3;
	s1 = _mm_shuffle_ps(pSin,pSin,_MM_SHUFFLE(3, 2, 1, 3));
	s2 = _mm_shuffle_ps(pCos,pSin,_MM_SHUFFLE(2, 1, 3, 2));
	s3 = _mm_shuffle_ps(pCos,pSin,_MM_SHUFFLE(1, 1, 3, 2));
	s3 = _mm_shuffle_ps(s3,s3,_MM_SHUFFLE(3, 1, 0, 3));
	c1 = _mm_shuffle_ps(pCos,pCos,_MM_SHUFFLE(3, 2, 1, 3));
	c2 = _mm_shuffle_ps(pSin,pCos,_MM_SHUFFLE(2, 1, 3, 2));
	c3 = _mm_shuffle_ps(pSin,pCos,_MM_SHUFFLE(1, 1, 3, 2));
	c3 = _mm_shuffle_ps(c3,c3,_MM_SHUFFLE(3, 1, 0, 3));

	s1 = _mm_mul_ps(s1,s2);
	s1 = _mm_mul_ps(s1,s3);
	c1 = _mm_mul_ps(c1,c2);
	c1 = _mm_mul_ps(c1,c3);
	c1 = _mm_mul_ps(c1,mark);

	s1 = _mm_add_ps(s1,c1);

	outQuat.Init(-s1.m128_f32[2],s1.m128_f32[0],s1.m128_f32[1],s1.m128_f32[3]);
#else
	float sr, sp, sy, cr, cp, cy;

	SinCos( angles.z * 0.5f, &sy, &cy );
	SinCos( angles.y * 0.5f, &sp, &cp );
	SinCos( angles.x * 0.5f, &sr, &cr );

	// 	float y,p,r;
	// 	y = XMScalarModAngle(angles.z * 0.5f);
	// 	p = XMScalarModAngle(angles.y * 0.5f);
	// 	r = XMScalarModAngle(angles.x * 0.5f);
	//
	// 	XMScalarSinCosEst(&sy, &cy, y);
	// 	XMScalarSinCosEst(&sp, &cp, p);
	// 	XMScalarSinCosEst(&sr, &cr, r);

	// NJS: for some reason VC6 wasn't recognizing the common subexpressions:
	float srXcp = sr * cp,
		crXsp = cr * sp;
	outQuat.x = srXcp*cy-crXsp*sy; // X
	outQuat.y = crXsp*cy+srXcp*sy; // Y

	float crXcp = cr * cp,
		srXsp = sr * sp;
	outQuat.z = crXcp*sy-srXsp*cy; // Z
	outQuat.w = crXcp*cy+srXsp*sy; // W (real component)
#endif
//	Assert( outQuat.IsValid() );
}

//-----------------------------------------------------------------------------
// Purpose: Converts a quaternion into engine angles
// Input  : *quaternion - q3 + q0.i + q1.j + q2.k
//			*outAngles - PITCH, YAW, ROLL
//-----------------------------------------------------------------------------
void QuaternionAngles( const Quaternion &q, RadianEuler &angles )
{
	Assert( s_bMathlibInitialized );
	Assert( q.IsValid() );

	// FIXME: doing it this way calculates too much data, needs to do an optimized version...
	matrix3x4_t matrix;
	QuaternionMatrix( q, matrix );
	MatrixAngles( matrix, angles );

	Assert( angles.IsValid() );
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  :
//-----------------------------------------------------------------------------

void Catmull_Rom_Spline(
	const Vector &p1,
	const Vector &p2,
	const Vector &p3,
	const Vector &p4,
	float t,
	Vector& output )
{
	Assert( s_bMathlibInitialized );
	float tSqr = t*t*0.5f;
	float tSqrSqr = t*tSqr;
	t *= 0.5f;

	Assert( &output != &p1 );
	Assert( &output != &p2 );
	Assert( &output != &p3 );
	Assert( &output != &p4 );

	output.Init();

	Vector a, b, c, d;

	// matrix row 1
	VectorScale( p1, -tSqrSqr, a );		// 0.5 t^3 * [ (-1*p1) + ( 3*p2) + (-3*p3) + p4 ]
	VectorScale( p2, tSqrSqr*3, b );
	VectorScale( p3, tSqrSqr*-3, c );
	VectorScale( p4, tSqrSqr, d );

	VectorAdd( a, output, output );
	VectorAdd( b, output, output );
	VectorAdd( c, output, output );
	VectorAdd( d, output, output );

	// matrix row 2
	VectorScale( p1, tSqr*2,  a );		// 0.5 t^2 * [ ( 2*p1) + (-5*p2) + ( 4*p3) - p4 ]
	VectorScale( p2, tSqr*-5, b );
	VectorScale( p3, tSqr*4,  c );
	VectorScale( p4, -tSqr,    d );

	VectorAdd( a, output, output );
	VectorAdd( b, output, output );
	VectorAdd( c, output, output );
	VectorAdd( d, output, output );

	// matrix row 3
	VectorScale( p1, -t, a );			// 0.5 t * [ (-1*p1) + p3 ]
	VectorScale( p3, t,  b );

	VectorAdd( a, output, output );
	VectorAdd( b, output, output );

	// matrix row 4
	VectorAdd( p2, output, output );	// p2
}

void Catmull_Rom_Spline_Tangent(
	const Vector &p1,
	const Vector &p2,
	const Vector &p3,
	const Vector &p4,
	float t,
	Vector& output )
{
	Assert( s_bMathlibInitialized );
	float tOne = 3*t*t*0.5f;
	float tTwo = 2*t*0.5f;
	float tThree = 0.5;

	Assert( &output != &p1 );
	Assert( &output != &p2 );
	Assert( &output != &p3 );
	Assert( &output != &p4 );

	output.Init();

	Vector a, b, c, d;

	// matrix row 1
	VectorScale( p1, -tOne, a );		// 0.5 t^3 * [ (-1*p1) + ( 3*p2) + (-3*p3) + p4 ]
	VectorScale( p2, tOne*3, b );
	VectorScale( p3, tOne*-3, c );
	VectorScale( p4, tOne, d );

	VectorAdd( a, output, output );
	VectorAdd( b, output, output );
	VectorAdd( c, output, output );
	VectorAdd( d, output, output );

	// matrix row 2
	VectorScale( p1, tTwo*2,  a );		// 0.5 t^2 * [ ( 2*p1) + (-5*p2) + ( 4*p3) - p4 ]
	VectorScale( p2, tTwo*-5, b );
	VectorScale( p3, tTwo*4,  c );
	VectorScale( p4, -tTwo,    d );

	VectorAdd( a, output, output );
	VectorAdd( b, output, output );
	VectorAdd( c, output, output );
	VectorAdd( d, output, output );

	// matrix row 3
	VectorScale( p1, -tThree, a );			// 0.5 t * [ (-1*p1) + p3 ]
	VectorScale( p3, tThree,  b );

	VectorAdd( a, output, output );
	VectorAdd( b, output, output );
}

void Catmull_Rom_Spline_Normalize(
	const Vector &p1,
	const Vector &p2,
	const Vector &p3,
	const Vector &p4,
	float t,
	Vector& output )
{
	// Normalize p2->p1 and p3->p4 to be the same length as p2->p3
	float dt = (p3 - p2).Length();

	Vector p1n = (p1 - p2);
	Vector p4n = (p4 - p3);

	VectorNormalize( p1n );
	VectorNormalize( p4n );

	p1n = p2 + p1n * dt;
	p4n = p3 + p4n * dt;

	Catmull_Rom_Spline( p1n, p2, p3, p4n, t, output );
}


void Catmull_Rom_Spline_NormalizeX(
	const Vector &p1,
	const Vector &p2,
	const Vector &p3,
	const Vector &p4,
	float t,
	Vector& output )
{
	// Normalize p2.x->p1.x and p3.x->p4.x to be the same length as p2.x->p3.x
	float dt = p3.x - p2.x;

	Vector p1n = p1;
	Vector p4n = p4;

	if (dt != 0.0)
	{
		if (p1.x != p2.x)
		{
			p1n = p2 - (p2 - p1) * (dt / (p2.x - p1.x));
		}
		if (p4.x != p3.x)
		{
			p4n = p3 + (p4 - p3) * (dt / (p4.x - p3.x));
		}
	}

	Catmull_Rom_Spline( p1n, p2, p3, p4n, t, output );
}


//-----------------------------------------------------------------------------
// Purpose: basic hermite spline.  t = 0 returns p1, t = 1 returns p2,
//			d1 and d2 are used to entry and exit slope of curve
// Input  :
//-----------------------------------------------------------------------------

void Hermite_Spline(
	const Vector &p1,
	const Vector &p2,
	const Vector &d1,
	const Vector &d2,
	float t,
	Vector& output )
{
	Assert( s_bMathlibInitialized );
	float tSqr = t*t;
	float tCube = t*tSqr;

	Assert( &output != &p1 );
	Assert( &output != &p2 );
	Assert( &output != &d1 );
	Assert( &output != &d2 );

	VectorScale( p1, 2*tCube-3*tSqr+1, output );
	VectorMA( output, -2*tCube+3*tSqr, p2, output );
	VectorMA( output, tCube-2*tSqr+t, d1, output );
	VectorMA( output, tCube-tSqr, d2, output );
}

float Hermite_Spline(
	float p1,
	float p2,
	float d1,
	float d2,
	float t )
{
	Assert( s_bMathlibInitialized );
	float output;
	float tSqr = t*t;
	float tCube = t*tSqr;

	output = p1 * (2*tCube-3*tSqr+1);
	output += p2 * (-2*tCube+3*tSqr);
	output += d1 * (tCube-2*tSqr+t);
	output += d2 * (tCube-tSqr);

	return output;
}



//-----------------------------------------------------------------------------
// Purpose: simple three data point hermite spline.
//			t = 0 returns p1, t = 1 returns p2,
//			slopes are generated from the p0->p1 and p1->p2 segments
//			this is reasonable C1 method when there's no "p3" data yet.
// Input  :
//-----------------------------------------------------------------------------
void Hermite_Spline(
	const Vector &p0,
	const Vector &p1,
	const Vector &p2,
	float t,
	Vector& output )
{
	Hermite_Spline( p1, p2, p1 - p0, p2 - p1, t, output );
	return;
}


float Hermite_Spline(
	float p0,
	float p1,
	float p2,
	float t )
{
	return Hermite_Spline( p1, p2, p1 - p0, p2 - p1, t );
}

//-----------------------------------------------------------------------------
// Transforms a AABB into another space; which will inherently grow the box.
//-----------------------------------------------------------------------------
void TransformAABB( const matrix3x4_t& transform, const Vector &vecMinsIn, const Vector &vecMaxsIn, Vector &vecMinsOut, Vector &vecMaxsOut )
{
	Vector localCenter = (vecMinsIn + vecMaxsIn) * 0.5;
	Vector localExtents = vecMaxsIn - localCenter;
	Vector worldCenter;
	VectorTransform( localCenter, transform, worldCenter );
	Vector worldExtents;
	worldExtents.x = DotProductAbs( localExtents, transform[0] );
	worldExtents.y = DotProductAbs( localExtents, transform[1] );
	worldExtents.z = DotProductAbs( localExtents, transform[2] );
	vecMinsOut = worldCenter - worldExtents;
	vecMaxsOut = worldCenter + worldExtents;
}

//-----------------------------------------------------------------------------
// Uses the inverse transform of in1
//-----------------------------------------------------------------------------
void ITransformAABB( const matrix3x4_t& transform, const Vector &vecMinsIn, const Vector &vecMaxsIn, Vector &vecMinsOut, Vector &vecMaxsOut )
{
	Vector worldCenter = (vecMinsIn + vecMaxsIn) * 0.5;
	Vector worldExtents = vecMaxsIn - worldCenter;
	Vector localCenter;
	VectorITransform( worldCenter, transform, localCenter );
	Vector localExtents;
	localExtents.x =	FloatMakePositive( worldExtents.x * transform[0][0] ) +
						FloatMakePositive( worldExtents.y * transform[1][0] ) +
						FloatMakePositive( worldExtents.z * transform[2][0] );
	localExtents.y =	FloatMakePositive( worldExtents.x * transform[0][1] ) +
						FloatMakePositive( worldExtents.y * transform[1][1] ) +
						FloatMakePositive( worldExtents.z * transform[2][1] );
	localExtents.z =	FloatMakePositive( worldExtents.x * transform[0][2] ) +
						FloatMakePositive( worldExtents.y * transform[1][2] ) +
						FloatMakePositive( worldExtents.z * transform[2][2] );

	vecMinsOut = localCenter - localExtents;
	vecMaxsOut = localCenter + localExtents;
}

float CalcDistanceToAABB( const Vector &mins, const Vector &maxs, const Vector &point )
{
	Assert( s_bMathlibInitialized );

	Vector delta;
	delta.Init();
	for ( int i = 0; i < 3; i++ )
	{
		if ( point[i] < mins[i] )
		{
			delta[i] = mins[i] - point[i];
		}
		else if ( point[i] > maxs[i] )
		{
			delta[i] = point[i] - maxs[i];
		}
	}
	return delta.Length();
}

void CalcClosestPointOnAABB( const Vector &mins, const Vector &maxs, const Vector &point, Vector &closestOut )
{
	for ( int i = 0; i < 3; i++ )
	{
		if ( point[i] < mins[i] )
		{
			closestOut[i] = mins[i];
		}
		else if ( point[i] > maxs[i] )
		{
			closestOut[i] = maxs[i];
		}
		else
		{
			closestOut[i] = point[i];
		}
	}
}

float CalcClosestPointToLineT( const Vector &P, const Vector &vLineA, const Vector &vLineB, Vector &vDir )
{
	Assert( s_bMathlibInitialized );
	VectorSubtract( vLineB, vLineA, vDir );

	// D dot [P - (A + D*t)] = 0
	// t = ( DP - DA) / DD
	float div = vDir.Dot( vDir );
	if( div < 0.00001f )
	{
		return 0;
	}
	else
	{
		return (vDir.Dot( P ) - vDir.Dot( vLineA )) / div;
	}
}

void CalcClosestPointOnLine( const Vector &P, const Vector &vLineA, const Vector &vLineB, Vector &vClosest, float *outT )
{
	assert( s_bMathlibInitialized );
	Vector vDir;
	float t = CalcClosestPointToLineT( P, vLineA, vLineB, vDir );
	if ( outT ) *outT = t;
	vClosest.MulAdd( vLineA, vDir, t );
}


float CalcDistanceToLine( const Vector &P, const Vector &vLineA, const Vector &vLineB, float *outT )
{
	Assert( s_bMathlibInitialized );
	Vector vClosest;
	CalcClosestPointOnLine( P, vLineA, vLineB, vClosest, outT );
	return (P - vClosest).Length();
}

void CalcClosestPointOnLineSegment( const Vector &P, const Vector &vLineA, const Vector &vLineB, Vector &vClosest, float *outT )
{
	Vector vDir;
	float t = CalcClosestPointToLineT( P, vLineA, vLineB, vDir );
	t = clamp( t, 0, 1 );
	if ( outT ) *outT = t;
	vClosest.MulAdd( vLineA, vDir, t );
}


float CalcDistanceToLineSegment( const Vector &P, const Vector &vLineA, const Vector &vLineB, float *outT )
{
	Assert( s_bMathlibInitialized );
	Vector vClosest;
	CalcClosestPointOnLineSegment( P, vLineA, vLineB, vClosest, outT );
	return (P - vClosest).Length();
}

float CalcClosestPointToLineT2D( Vector2D const &P, Vector2D const &vLineA, Vector2D const &vLineB, Vector2D &vDir )
{
	Assert( s_bMathlibInitialized );
	Vector2DSubtract( vLineB, vLineA, vDir );

	// D dot [P - (A + D*t)] = 0
	// t = (DP - DA) / DD
	float div = vDir.Dot( vDir );
	if( div < 0.00001f )
	{
		return 0;
	}
	else
	{
		return (vDir.Dot( P ) - vDir.Dot( vLineA )) / div;
	}
}

void CalcClosestPointOnLine2D( Vector2D const &P, Vector2D const &vLineA, Vector2D const &vLineB, Vector2D &vClosest, float *outT )
{
	assert( s_bMathlibInitialized );
	Vector2D vDir;
	float t = CalcClosestPointToLineT2D( P, vLineA, vLineB, vDir );
	if ( outT ) *outT = t;
	vClosest.MulAdd( vLineA, vDir, t );
}

float CalcDistanceToLine2D( Vector2D const &P, Vector2D const &vLineA, Vector2D const &vLineB, float *outT )
{
	Assert( s_bMathlibInitialized );
	Vector2D vClosest;
	CalcClosestPointOnLine2D( P, vLineA, vLineB, vClosest, outT );
	return (P - vClosest).Length();
}

void CalcClosestPointOnLineSegment2D( Vector2D const &P, Vector2D const &vLineA, Vector2D const &vLineB, Vector2D &vClosest, float *outT )
{
	Vector2D vDir;
	float t = CalcClosestPointToLineT2D( P, vLineA, vLineB, vDir );
	t = clamp( t, 0, 1 );
	if ( outT ) *outT = t;
	vClosest.MulAdd( vLineA, vDir, t );
}

float CalcDistanceToLineSegment2D( Vector2D const &P, Vector2D const &vLineA, Vector2D const &vLineB, float *outT )
{
	Assert( s_bMathlibInitialized );
	Vector2D vClosest;
	CalcClosestPointOnLineSegment2D( P, vLineA, vLineB, vClosest, outT );
	return (P - vClosest).Length();
}

#pragma optimize( "", off )

// stuff from windows.h
//typedef unsigned int DWORD;
#ifndef EXCEPTION_EXECUTE_HANDLER
#define EXCEPTION_EXECUTE_HANDLER       1
#endif


#pragma optimize( "", on )

static bool s_b3DNowEnabled = false;
static bool s_bMMXEnabled = false;
static bool s_bSSEEnabled = false;
static bool s_bSSE2Enabled = false;

void MathLib_Init( float gamma, float texGamma, float brightness, int overbright, bool bAllow3DNow, bool bAllowSSE, bool bAllowSSE2, bool bAllowMMX )
{
	// FIXME: Hook SSE into VectorAligned + Vector4DAligned


	// Grab the processor information:
	const CPUInformation& pi = GetCPUInformation();

	// Select the default generic routines.
	pfSqrt = _sqrtf;
	pfRSqrt = _rsqrtf;
	pfRSqrtFast = _rsqrtf;
	pfVectorNormalize = _VectorNormalize;
	pfVectorNormalizeFast = _VectorNormalizeFast;
	pfInvRSquared = _InvRSquared;
	pfFastSinCos = SinCos;
	pfFastCos = cosf;


	if ( bAllowMMX && pi.m_bMMX )
	{
		// Select the MMX specific routines if available
		// (MMX routines were used by SW span fillers - not currently used for HW)
		s_bMMXEnabled = true;
	}
	else
	{
		s_bMMXEnabled = false;
	}

	// SSE Generally performs better than 3DNow when present, so this is placed
	// first to allow SSE to override these settings.
	if ( bAllow3DNow && pi.m_b3DNow )
	{
		s_b3DNowEnabled = true;

		// Select the 3DNow specific routines if available;
		pfVectorNormalize = _3DNow_VectorNormalize;
		pfVectorNormalizeFast = _3DNow_VectorNormalizeFast;
		pfInvRSquared = _3DNow_InvRSquared;
		pfSqrt = _3DNow_Sqrt;
		pfRSqrt = _3DNow_RSqrt;
		pfRSqrtFast = _3DNow_RSqrt;
	}
	else
	{
		s_b3DNowEnabled = false;
	}

	if ( bAllowSSE && pi.m_bSSE )
	{
		s_bSSEEnabled = true;

		// Select the SSE specific routines if available
		pfVectorNormalize = _VectorNormalize;
		pfVectorNormalizeFast = _SSE_VectorNormalizeFast;
		pfInvRSquared = _SSE_InvRSquared;
		pfSqrt = _SSE_Sqrt;
		pfRSqrt = _SSE_RSqrtAccurate;
		pfRSqrtFast = _SSE_RSqrtFast;
#ifdef WIN32
		pfFastSinCos = _SSE_SinCos;
		pfFastCos = _SSE_cos;
#endif
	}
	else
	{
		s_bSSEEnabled = false;
	}

	if ( bAllowSSE2 && pi.m_bSSE2 )
	{
		s_bSSE2Enabled = true;
#ifdef WIN32
		pfFastSinCos = _SSE2_SinCos;
		pfFastCos = _SSE2_cos;
#endif
	} else
	{
		s_bSSE2Enabled = false;
	}

	s_bMathlibInitialized = true;

	InitSinCosTable();
	BuildGammaTable( gamma, texGamma, brightness, overbright );
}

bool MathLib_3DNowEnabled( void )
{
	Assert( s_bMathlibInitialized );
	return s_b3DNowEnabled;
}

bool MathLib_MMXEnabled( void )
{
	Assert( s_bMathlibInitialized );
	return s_bMMXEnabled;
}

bool MathLib_SSEEnabled( void )
{
	Assert( s_bMathlibInitialized );
	return s_bSSEEnabled;
}

bool MathLib_SSE2Enabled( void )
{
	Assert( s_bMathlibInitialized );
	return s_bSSE2Enabled;
}

float Approach( float target, float value, float speed )
{
	float delta = target - value;

	if ( delta > speed )
		value += speed;
	else if ( delta < -speed )
		value -= speed;
	else
		value = target;

	return value;
}

// BUGBUG: Why doesn't this call angle diff?!?!?
float ApproachAngle( float target, float value, float speed )
{
	target = anglemod( target );
	value = anglemod( value );

	float delta = target - value;

	// Speed is assumed to be positive
	if ( speed < 0 )
		speed = -speed;

	if ( delta < -180 )
		delta += 360;
	else if ( delta > 180 )
		delta -= 360;

	if ( delta > speed )
		value += speed;
	else if ( delta < -speed )
		value -= speed;
	else
		value = target;

	return value;
}


// BUGBUG: Why do we need both of these?
float AngleDiff( float destAngle, float srcAngle )
{
	float delta;

	delta = destAngle - srcAngle;
	if ( destAngle > srcAngle )
	{
		while ( delta >= 180 )
			delta -= 360;
	}
	else
	{
		while ( delta <= -180 )
			delta += 360;
	}
	return delta;
}


float AngleDistance( float next, float cur )
{
	float delta = next - cur;

	if ( delta < -180 )
		delta += 360;
	else if ( delta > 180 )
		delta -= 360;

	return delta;
}


float AngleNormalize( float angle )
{
	while (angle > 180)
	{
		angle -= 360;
	}
	while (angle < -180)
	{
		angle += 360;
	}
	return angle;
}

void RotationDeltaAxisAngle( const QAngle &srcAngles, const QAngle &destAngles, Vector &deltaAxis, float &deltaAngle )
{
	Quaternion srcQuat, destQuat, srcQuatInv, out;
	AngleQuaternion( srcAngles, srcQuat );
	AngleQuaternion( destAngles, destQuat );
	QuaternionScale( srcQuat, -1, srcQuatInv );
	QuaternionMult( srcQuatInv, destQuat, out );

	QuaternionNormalize( out );
	QuaternionAxisAngle( out, deltaAxis, deltaAngle );
}

//-----------------------------------------------------------------------------
// Purpose: Computes a triangle normal
//-----------------------------------------------------------------------------
void ComputeTrianglePlane( const Vector& v1, const Vector& v2, const Vector& v3, Vector& normal, float& intercept )
{
	Vector e1, e2;
	VectorSubtract( v2, v1, e1 );
	VectorSubtract( v3, v1, e2 );
	CrossProduct( e1, e2, normal );
	VectorNormalize( normal );
	intercept = DotProduct( normal, v1 );
}

float GetVectorAngle( Vector & v1, Vector & v2 )
{
	float fAngle;
	float x = v1.x - v2.x;
	float y = v1.y - v2.y;

	fAngle = atan2f( y, x );
	if( fAngle < 0.0f )
		fAngle += 2 * M_PI;

	return fAngle;
}

void MoveVectorByAngle( Vector & v1, Vector & v2, float fAngle, float fStep )
{
	float sin, cos;

#ifdef WIN32
	float angle;
	angle = XMScalarModAngle(fAngle);
	XMScalarSinCosEst(&sin, &cos, angle);
#else
	FastSinCos( fAngle, &sin, &cos );
#endif

	v1.x = v2.x + cos * fStep;
	v1.y = v2.y + sin * fStep;
	v1.z = v2.z;
}
//by jinsheng
float FlReduceRotation(float fl)
{
	while (fl >= G_2PI)
		fl -= G_2PI;
	while (fl <= 0)
		fl += G_2PI;
	return fl;
}
BOOL RotateangleLogic(float &DestAngle, float &CurAngle, float MaxChangeAngle )
{
	if ( fabs(DestAngle - CurAngle) < 0.01)
		return FALSE;

	float angle = DestAngle;

	float rotangle = fabs(angle - CurAngle);

	if ( rotangle > G_PI )  // -pI to PI
	{
		//m_angle  = angle;
		if ( angle > G_PI )
			angle  -= G_2PI;

		if ( CurAngle > G_PI )
			CurAngle  -= G_2PI;

		rotangle = fabs(angle - CurAngle);

		if ( rotangle > MaxChangeAngle)
		{
			if (angle > CurAngle)
			{
				CurAngle += MaxChangeAngle;

			}
			else
			{
				CurAngle -= MaxChangeAngle;

			}
		}
		else
		{
			CurAngle  = angle;
		}


	}
	else if (rotangle == G_PI)
	{
		CurAngle += MaxChangeAngle;

	}
	else   // 0--2PI
	{
		if ( rotangle > MaxChangeAngle)
		{
			if (angle > CurAngle)
			{
				CurAngle += MaxChangeAngle;

			}
			else
			{
				CurAngle -= MaxChangeAngle;

			}
		}
		else
		{
			CurAngle  = angle;
		}


	}

	CurAngle = FlReduceRotation( CurAngle );

	return TRUE;
}
