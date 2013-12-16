/*******************************************************************************
 * fpmetric.cpp
 *
 * This module implements the parametric shapetype.
 *
 * This module was written by D.Skarda&T.Bily and modified by R.Suzuki.
 * Ported to POV-Ray 3.5 by Thorsten Froehlich.
 *
 * ---------------------------------------------------------------------------
 * Persistence of Vision Ray Tracer ('POV-Ray') version 3.7.
 * Copyright 1991-2013 Persistence of Vision Raytracer Pty. Ltd.
 *
 * POV-Ray is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * POV-Ray is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * ---------------------------------------------------------------------------
 * POV-Ray is based on the popular DKB raytracer version 2.12.
 * DKBTrace was originally written by David K. Buck.
 * DKBTrace Ver 2.0-2.12 were written by David K. Buck & Aaron A. Collins.
 * ---------------------------------------------------------------------------
 * $File: //depot/public/povray/3.x/source/backend/shape/fpmetric.cpp $
 * $Revision: #1 $
 * $Change: 6069 $
 * $DateTime: 2013/11/06 11:59:40 $
 * $Author: chrisc $
 *******************************************************************************/

#include <limits.h>
#include <algorithm>

// frame.h must always be the first POV file included (pulls in platform config)
#include "backend/frame.h"
#include "backend/scene/objects.h"
#include "backend/scene/threaddata.h"
#include "backend/shape/fpmetric.h"
#include "backend/shape/spheres.h" // TODO - Move sphere intersection function to math code! [trf]
#include "backend/shape/boxes.h" // TODO - Move box intersection function to math code! [trf]
#include "backend/vm/fnpovfpu.h"

// this must be the last file included
#include "base/povdebug.h"

namespace pov
{

/*****************************************************************************
 * Local preprocessor defines
 ******************************************************************************/

const int Max_intNumber = 10000000;
#define close(x, y) (fabs(x-y) < EPSILON ? 1 : 0)

const int INDEX_U = 0;
const int INDEX_V = 1;

/* Side hit. */
const int SIDE_X_0 = 1;
const int SIDE_X_1 = 2;
const int SIDE_Y_0 = 3;
const int SIDE_Y_1 = 4;
const int SIDE_Z_0 = 5;
const int SIDE_Z_1 = 6;

const int OK_X     =   1;
const int OK_Y     =   2;
const int OK_Z     =   4;
const int OK_R     =   8;
const int OK_S     =  16;
const int OK_T     =  32;
const int OK_U     =  64;
const int OK_V     = 128;


/*****************************************************************************
 *
 * FUNCTION
 *
 *   All_Parametric_Intersections
 *
 * INPUT
 *
 * OUTPUT
 *
 * RETURNS
 *
 * AUTHOR
 *
 * DESCRIPTION
 *
 *   -
 *
 * CHANGES
 *
 *   -
 *
 ******************************************************************************/

bool Parametric::All_Intersections(const Ray& ray, IStack& Depth_Stack, TraceThreadData *Thread)
{
	VECTOR P, D, IPoint;
	UV_VECT low_vect, hi_vect, uv;
	Ray New_Ray;
	DBL XRayMin, XRayMax, YRayMin, YRayMax, ZRayMin, ZRayMax, TPotRes, TLen;
	DBL Depth1, Depth2, temp, Len, TResult = HUGE_VAL;
	DBL low, hi, len;
	int MaxPrecompX, MaxPrecompY, MaxPrecompZ;
	int split, i = 0, Side1, Side2;
	int parX, parY;
	int i_flg;
	DBL Intervals_Low[2][32];
	DBL Intervals_Hi[2][32];
	int SectorNum[32];

	Thread->Stats()[Ray_Par_Bound_Tests]++;

	if(container_shape)
	{
		if(Trans != NULL)
		{
			MInvTransPoint(*New_Ray.Origin, *ray.Origin, Trans);
			MInvTransDirection(*New_Ray.Direction, *ray.Direction, Trans);
			len = New_Ray.Direction.length();
			New_Ray.Direction /= len;
			i_flg = Sphere::Intersect(New_Ray, container.sphere.center,
			                          (container.sphere.radius) * (container.sphere.radius),
			                          &Depth1, &Depth2);
			Depth1 = Depth1 / len;
			Depth2 = Depth2 / len;
		}
		else
		{
			i_flg = Sphere::Intersect(ray, container.sphere.center,
			                          (container.sphere.radius) * (container.sphere.radius), &Depth1, &Depth2);
		}
		Thread->Stats()[Ray_Sphere_Tests]--;
		if(i_flg)
			Thread->Stats()[Ray_Sphere_Tests_Succeeded]--;
	}
	else
	{
		i_flg = Box::Intersect(ray, Trans, container.box.corner1, container.box.corner2,
		                       &Depth1, &Depth2, &Side1, &Side2);
	}

	if(!i_flg)
		return false;

	Thread->Stats()[Ray_Par_Bound_Tests_Succeeded]++;
	Thread->Stats()[Ray_Parametric_Tests]++;

	if (Trans != NULL)
	{
		MInvTransPoint(P, *ray.Origin, Trans);
		MInvTransDirection(D, *ray.Direction, Trans);
	}
	else
	{
		P[X] = ray.Origin[X];
		P[Y] = ray.Origin[Y];
		P[Z] = ray.Origin[Z];
		D[X] = ray.Direction[X];
		D[Y] = ray.Direction[Y];
		D[Z] = ray.Direction[Z];
	}

	if (Depth1 == Depth2)
		Depth1 = 0;

	if ((Depth1 += 4 * accuracy) > Depth2)
		return false;

	Intervals_Low[INDEX_U][0] = umin;
	Intervals_Hi[INDEX_U][0] = umax;

	Intervals_Low[INDEX_V][0] = vmin;
	Intervals_Hi[INDEX_V][0] = vmax;
	/* Fri 09-27-1996 0. */
	SectorNum[0] = 1;

	MaxPrecompX = MaxPrecompY = MaxPrecompZ = 0;
	if (PData != NULL)
	{
		if (((PData->flags) & OK_X) != 0)
			MaxPrecompX = 1 << (PData->depth);
		if (((PData->flags) & OK_Y) != 0)
			MaxPrecompY = 1 << (PData->depth);
		if (((PData->flags) & OK_Z) != 0)
			MaxPrecompZ = 1 << (PData->depth);
	}
	/* 0 */
	while (i >= 0)
	{
		low_vect[U] = Intervals_Low[INDEX_U][i];
		hi_vect[U] = Intervals_Hi[INDEX_U][i];
		Len = hi_vect[U] - low_vect[U];
		split = INDEX_U;

		low_vect[V] = Intervals_Low[INDEX_V][i];
		hi_vect[V] = Intervals_Hi[INDEX_V][i];
		temp = hi_vect[V] - low_vect[V];
		if (temp > Len)
		{
			Len = temp;
			split = INDEX_V;
		}
		parX = parY = 0;
		TLen = 0;

		/* X */
		if (SectorNum[i] < MaxPrecompX)
		{
			low = PData->Low[0][SectorNum[i]];
			hi = PData->Hi[0][SectorNum[i]];
		}
		else
			Evaluate_Function_Interval_UV(Thread->functionContext, *(Function[0]), accuracy, low_vect, hi_vect, max_gradient, low, hi);
		/* fabs(D[X] *(T2-T1)) is not OK with new method */

		if (close(D[0], 0))
		{
			parX = 1;
			if ((hi < P[0]) || (low > P[0]))
			{
				i--;
				continue;
			}
		}
		else
		{
			XRayMin = (hi - P[0]) / D[0];
			XRayMax = (low - P[0]) / D[0];
			if (XRayMin > XRayMax)
			{
				temp = XRayMin;
				XRayMin = XRayMax;
				XRayMax = temp;
			}

			if ((XRayMin > Depth2) || (XRayMax < Depth1))
			{
				i--;
				continue;
			}

			if ((TPotRes = XRayMin) > TResult)
			{
				i--;
				continue;
			}

			TLen = XRayMax - XRayMin;
		}

		/* Y */
		if (SectorNum[i] < MaxPrecompY)
		{
			low = PData->Low[1][SectorNum[i]];
			hi = PData->Hi[1][SectorNum[i]];
		}
		else
			Evaluate_Function_Interval_UV(Thread->functionContext, *(Function[1]), accuracy, low_vect, hi_vect, max_gradient, low, hi);

		if (close(D[1], 0))
		{
			parY = 1;
			if ((hi < P[1]) || (low > P[1]))
			{
				i--;
				continue;
			}
		}
		else
		{
			YRayMin = (hi - P[1]) / D[1];
			YRayMax = (low - P[1]) / D[1];
			if (YRayMin > YRayMax)
			{
				temp = YRayMin;
				YRayMin = YRayMax;
				YRayMax = temp;
			}
			if ((YRayMin > Depth2) || (YRayMax < Depth1))
			{
				i--;
				continue;
			}
			if ((TPotRes = YRayMin) > TResult)
			{
				i--;
				continue;
			}
			if (parX == 0)
			{
				if ((YRayMin > XRayMax) || (YRayMax < XRayMin))
				{
					i--;
					continue;
				}
			}
			if ((temp = YRayMax - YRayMin) > TLen)
				TLen = temp;
		}

		/* Z */
		if ((SectorNum[i] < MaxPrecompZ) && (0 < SectorNum[i]))
		{
			low = PData->Low[2][SectorNum[i]];
			hi = PData->Hi[2][SectorNum[i]];
		}
		else
			Evaluate_Function_Interval_UV(Thread->functionContext, *(Function[2]), accuracy, low_vect, hi_vect, max_gradient, low, hi);

		if (close(D[2], 0))
		{
			if ((hi < P[2]) || (low > P[2]))
			{
				i--;
				continue;
			}
		}
		else
		{
			ZRayMin = (hi - P[2]) / D[2];
			ZRayMax = (low - P[2]) / D[2];
			if (ZRayMin > ZRayMax)
			{
				temp = ZRayMin;
				ZRayMin = ZRayMax;
				ZRayMax = temp;
			}
			if ((ZRayMin > Depth2) || (ZRayMax < Depth1))
			{
				i--;
				continue;
			}
			if ((TPotRes = ZRayMin) > TResult)
			{
				i--;
				continue;
			}
			if (parX == 0)
			{
				if ((ZRayMin > XRayMax) || (ZRayMax < XRayMin))
				{
					i--;
					continue;
				}
			}
			if (parY == 0)
			{
				if ((ZRayMin > YRayMax) || (ZRayMax < YRayMin))
				{
					i--;
					continue;
				}
			}
			if ((temp = ZRayMax - ZRayMin) > TLen)
				TLen = temp;
		}

		if (Len > TLen)
			Len = TLen;
		if (Len < accuracy)
		{
			if ((TResult > TPotRes) && (TPotRes > Depth1))
			{
				TResult = TPotRes;
				Assign_UV_Vect(uv, low_vect);
			}
			i--;
		}
		else
		{
			/* 1 copy */
			if ((SectorNum[i] *= 2) >= Max_intNumber)
				SectorNum[i] = Max_intNumber;
			SectorNum[i + 1] = SectorNum[i];
			SectorNum[i]++;
			i++;
			Intervals_Low[INDEX_U][i] = low_vect[U];
			Intervals_Hi[INDEX_U][i] = hi_vect[U];

			Intervals_Low[INDEX_V][i] = low_vect[V];
			Intervals_Hi[INDEX_V][i] = hi_vect[V];

			/* 2 split */
			temp = (Intervals_Hi[split][i] + Intervals_Low[split][i]) / 2.0;
			Intervals_Hi[split][i] = temp;
			Intervals_Low[split][i - 1] = temp;
		}
	}

	if (TResult < Depth2)
	{
		Thread->Stats()[Ray_Parametric_Tests_Succeeded]++;
		VScale(IPoint, *ray.Direction, TResult);
		VAddEq(IPoint, *ray.Origin);

		if (Clip.empty() || Point_In_Clip(Vector3d(IPoint), Clip, Thread))
		{
			/*
			  compute_param_normal( Par, UResult, VResult , &N); 
			  push_normal_entry( TResult ,IPoint, N, reinterpret_cast<ObjectPtr>(Object), Depth_Stack);
			*/
			Depth_Stack->push(Intersection(TResult, Vector3d(IPoint), Vector2d(uv), this));

			return true;
		}
	}

	return false;
}
/* 0 */


/*****************************************************************************
 *
 * FUNCTION
 *
 *   Inside_Parametric
 *
 * INPUT
 *
 * OUTPUT
 *
 * RETURNS
 *
 * AUTHOR
 *
 * DESCRIPTION
 *
 *   -
 *
 * CHANGES
 *
 *   -
 *
 ******************************************************************************/

bool Parametric::Inside(const Vector3d&, TraceThreadData *Thread) const
{
	return false;
}


/*****************************************************************************
 *
 * FUNCTION
 *
 *   Parametric_Normal
 *
 * INPUT
 *
 * OUTPUT
 *
 * RETURNS
 *
 * AUTHOR
 *
 * DESCRIPTION
 *
 *   -
 *
 * CHANGES
 *
 *   -
 *
 ******************************************************************************/

void Parametric::Normal(Vector3d& Result, Intersection *Inter, TraceThreadData *Thread) const
{
	VECTOR RU, RV;
	UV_VECT uv_vect;

	uv_vect[U] = Inter->Iuv[U];
	uv_vect[V] = Inter->Iuv[V];
	RU[X] = RV[X] = -Evaluate_Function_UV(Thread->functionContext, *(Function[X]), uv_vect);
	RU[Y] = RV[Y] = -Evaluate_Function_UV(Thread->functionContext, *(Function[Y]), uv_vect);
	RU[Z] = RV[Z] = -Evaluate_Function_UV(Thread->functionContext, *(Function[Z]), uv_vect);

	uv_vect[U] += accuracy;
	RU[X] += Evaluate_Function_UV(Thread->functionContext, *(Function[X]), uv_vect);
	RU[Y] += Evaluate_Function_UV(Thread->functionContext, *(Function[Y]), uv_vect);
	RU[Z] += Evaluate_Function_UV(Thread->functionContext, *(Function[Z]), uv_vect);

	uv_vect[U] = Inter->Iuv[U];
	uv_vect[V] += accuracy;
	RV[X] += Evaluate_Function_UV(Thread->functionContext, *(Function[X]), uv_vect);
	RV[Y] += Evaluate_Function_UV(Thread->functionContext, *(Function[Y]), uv_vect);
	RV[Z] += Evaluate_Function_UV(Thread->functionContext, *(Function[Z]), uv_vect);

	Result = cross(Vector3d(RU), Vector3d(RV));
	if (Trans != NULL)
		MTransNormal(*Result, *Result, Trans);
	Result.normalize();
}


/*****************************************************************************
 *
 * FUNCTION
 *
 *   Compute_Parametric_BBox
 *
 * INPUT
 *
 * OUTPUT
 *
 * RETURNS
 *
 * AUTHOR
 *
 * DESCRIPTION
 *
 *   -
 *
 * CHANGES
 *
 *   -
 *
 ******************************************************************************/

void Parametric::Compute_BBox()
{
	if(container_shape != 0)
	{
		Make_BBox(BBox,
		          container.sphere.center[X] - container.sphere.radius,
		          container.sphere.center[Y] - container.sphere.radius,
		          container.sphere.center[Z] - container.sphere.radius,
		          container.sphere.radius * 2,
		          container.sphere.radius * 2,
		          container.sphere.radius * 2);
	}
	else
	{
		BBox.lowerLeft = BBoxVector3d(container.box.corner1);
		BBox.size = BBoxVector3d(Vector3d(container.box.corner2) - Vector3d(container.box.corner1));
	}

	if(Trans != NULL)
	{
		Recompute_BBox(&BBox, Trans);
	}
}


/*****************************************************************************
 *
 * FUNCTION
 *
 *   Translate_Parametric
 *
 * INPUT
 *
 * OUTPUT
 *
 * RETURNS
 *
 * AUTHOR
 *
 * DESCRIPTION
 *
 *   -
 *
 * CHANGES
 *
 *   -
 *
 ******************************************************************************/

void Parametric::Translate(const Vector3d&, const TRANSFORM* tr)
{
	Transform(tr);
}


/*****************************************************************************
 *
 * FUNCTION
 *
 *   Rotate_Parametric
 *
 * INPUT
 *
 * OUTPUT
 *
 * RETURNS
 *
 * AUTHOR
 *
 * DESCRIPTION
 *
 *   -
 *
 * CHANGES
 *
 *   -
 *
 ******************************************************************************/

void Parametric::Rotate(const Vector3d&, const TRANSFORM* tr)
{
	Transform(tr);
}


/*****************************************************************************
 *
 * FUNCTION
 *
 *   Scale_Parametric
 *
 * INPUT
 *
 * OUTPUT
 *
 * RETURNS
 *
 * AUTHOR
 *
 * DESCRIPTION
 *
 *   -
 *
 * CHANGES
 *
 *   -
 *
 ******************************************************************************/

void Parametric::Scale(const Vector3d&, const TRANSFORM* tr)
{
	Transform(tr);
}


/*****************************************************************************
 *
 * FUNCTION
 *
 *   Transform_Parametric
 *
 * INPUT
 *
 * OUTPUT
 *
 * RETURNS
 *
 * AUTHOR
 *
 * DESCRIPTION
 *
 *   -
 *
 * CHANGES
 *
 *   -
 *
 ******************************************************************************/

void Parametric::Transform(const TRANSFORM* tr)
{
	if(Trans == NULL)
		Trans = Create_Transform();
	Compose_Transforms(Trans, tr);
	Compute_BBox();
}


/*****************************************************************************
 *
 * FUNCTION
 *
 *   Invert_Parametric
 *
 * INPUT
 *
 * OUTPUT
 *
 * RETURNS
 *
 * AUTHOR
 *
 * DESCRIPTION
 *
 *   -
 *
 * CHANGES
 *
 *   -
 *
 ******************************************************************************/

void Parametric::Invert()
{
}


/*****************************************************************************
 *
 * FUNCTION
 *
 *   Copy_Parametric
 *
 * INPUT
 *
 * OUTPUT
 *
 * RETURNS
 *
 * AUTHOR
 *
 * DESCRIPTION
 *
 *   -
 *
 * CHANGES
 *
 *   -
 *
 ******************************************************************************/

ObjectPtr Parametric::Copy()
{
	Parametric *New = new Parametric();
	Destroy_Transform(New->Trans);
	*New = *this;

	New->Function[0] = Parser::Copy_Function(vm, Function[0]);
	New->Function[1] = Parser::Copy_Function(vm, Function[1]);
	New->Function[2] = Parser::Copy_Function(vm, Function[2]);
	New->Trans = Copy_Transform(Trans);
	New->PData = Copy_PrecompParVal();

	return (New);
}


/*****************************************************************************
 *
 * FUNCTION
 *
 *   Destroy_Parametric
 *
 * INPUT
 *
 * OUTPUT
 *
 * RETURNS
 *
 * AUTHOR
 *
 * DESCRIPTION
 *
 *   -
 *
 * CHANGES
 *
 *   -
 *
 ******************************************************************************/

Parametric::~Parametric()
{
	Destroy_Transform(Trans);
	Parser::Destroy_Function(vm, Function[0]);
	Parser::Destroy_Function(vm, Function[1]);
	Parser::Destroy_Function(vm, Function[2]);
	Destroy_PrecompParVal();
}


/*****************************************************************************
 *
 * FUNCTION
 *
 *   Create_Parametric
 *
 * INPUT
 *
 * OUTPUT
 *
 * RETURNS
 *
 * AUTHOR
 *
 * DESCRIPTION
 *
 *   -
 *
 * CHANGES
 *
 *   -
 *
 ******************************************************************************/

Parametric::Parametric() : ObjectBase(PARAMETRIC_OBJECT)
{
	Make_Vector(container.box.corner1, -1.0, -1.0, -1.0);
	Make_Vector(container.box.corner2, 1.0, 1.0, 1.0);

	Make_BBox(BBox, -1.0, -1.0, -1.0, 2.0, 2.0, 2.0);

	Trans = Create_Transform();

	Function[0] = NULL;
	Function[1] = NULL;
	Function[2] = NULL;
	accuracy = 0.001;
	max_gradient = 1;
	Inverted = false;
	PData = NULL;
	container_shape = 0;
}


/*****************************************************************************
 *
 * FUNCTION
 *
 *   Parametric_UVCoord
 *
 * INPUT
 *
 *   As for all UVCoord methods object and interstcion structure
 *
 * OUTPUT
 *
 *   As for all UVCoord methods 2D vector with UV coordinates
 *
 * RETURNS
 *
 * AUTHOR
 *
 *   Wlodzimierz ABX Skiba
 *
 * DESCRIPTION
 *
 * CHANGES
 *
 *   -
 *
******************************************************************************/

void Parametric::UVCoord(Vector2d& Result, const Intersection *inter, TraceThreadData *Thread) const
{
	Result = inter->Iuv;
}


/*****************************************************************************
 *
 * FUNCTION
 *
 *   Precomp_Par_Int
 *
 * INPUT
 *
 * OUTPUT
 *
 * RETURNS
 *
 * AUTHOR
 *
 * DESCRIPTION
 *
 *   -
 *
 * CHANGES
 *
 *   -
 *
 ******************************************************************************/

void Parametric::Precomp_Par_Int(int depth, DBL umin, DBL vmin, DBL umax, DBL vmax, FPUContext *ctx)
{
	int j;

	if(depth >= PrecompLastDepth)
	{
		for(j = 0; j < 3; j++)
		{
			if(PData->flags & (1 << j))
			{
				UV_VECT low,hi;

				low[U] = umin;
				hi[U] = umax;
				low[V] = vmin;
				hi[V] = vmax;

				Evaluate_Function_Interval_UV(ctx,
				                              *Function[j],
				                              accuracy,
				                              low,
				                              hi,
				                              max_gradient,
				                              PData->Low[j][depth],
				                              PData->Hi[j][depth]);
			}
		}
	}
	else                    /* split */
	{
		if(umax - umin < vmax - vmin)
		{
			Precomp_Par_Int(2 * depth, umin, vmin, umax, (vmin + vmax) / 2.0, ctx);
			Precomp_Par_Int(2 * depth + 1, umin, (vmin + vmax) / 2.0, umax, vmax, ctx);
		}
		else
		{
			Precomp_Par_Int(2 * depth, umin, vmin, (umin + umax) / 2.0, vmax, ctx);
			Precomp_Par_Int(2 * depth + 1, (umin + umax) / 2.0, vmin, umax, vmax, ctx);
		}
		for(j = 0; j < 3; j++)
		{
			if(PData->flags & (1 << j))
			{
				if(PData->Hi[j][2 * depth] > PData->Hi[j][2 * depth + 1])
					PData->Hi[j][depth] = PData->Hi[j][2 * depth];
				else
					PData->Hi[j][depth] = PData->Hi[j][2 * depth + 1];
				if(PData->Low[j][2 * depth] < PData->Low[j][2 * depth + 1])
					PData->Low[j][depth] = PData->Low[j][2 * depth];
				else
					PData->Low[j][depth] = PData->Low[j][2 * depth + 1];
			}
		}
	}
}


/*****************************************************************************
 *
 * FUNCTION
 *
 *   Precompute_Parametric_Values
 *
 * INPUT
 *
 * OUTPUT
 *
 * RETURNS
 *
 * AUTHOR
 *
 * DESCRIPTION
 *
 *   -
 *
 * CHANGES
 *
 *   -
 *
 ******************************************************************************/

void Parametric::Precompute_Parametric_Values(char flags, int depth, FPUContext *ctx)
{
	DBL * Last;
	const char* es = "precompute";
	int nmb;

	if ((depth < 1) || (depth > 20))
		throw POV_EXCEPTION_STRING("Precompute: invalid depth");
	nmb = 1 << depth;

	PData = reinterpret_cast<PRECOMP_PAR_DATA *>(POV_MALLOC(sizeof(PRECOMP_PAR_DATA), es));
	if (PData == NULL)
		throw POV_EXCEPTION_STRING("Cannot allocate memory for parametric precomputation data.");
	PData->flags = flags;
	PData->depth = depth;
	PData->use = 1;

	if (flags & OK_X)
	{
		PData->Low[0] = reinterpret_cast<DBL *>(POV_MALLOC(sizeof(DBL) * nmb, es));
		Last = PData->Hi[0] = reinterpret_cast<DBL *>(POV_MALLOC(sizeof(DBL) * nmb, es));
	}
	if (flags & OK_Y)
	{
		PData->Low[1] = reinterpret_cast<DBL *>(POV_MALLOC(sizeof(DBL) * nmb, es));
		Last = PData->Hi[1] = reinterpret_cast<DBL *>(POV_MALLOC(sizeof(DBL) * nmb, es));
	}
	if (flags & OK_Z)
	{
		PData->Low[2] = reinterpret_cast<DBL *>(POV_MALLOC(sizeof(DBL) * nmb, es));
		Last = PData->Hi[2] = reinterpret_cast<DBL *>(POV_MALLOC(sizeof(DBL) * nmb, es));
	}
	if (Last == NULL)
		throw POV_EXCEPTION_STRING("Cannot allocate memory for parametric precomputation data.");

	PrecompLastDepth = 1 << (depth - 1);
	Precomp_Par_Int(1, umin, vmin, umax, vmax, ctx);
}


/*****************************************************************************
 *
 * FUNCTION
 *
 *   Copy_PrecompParVal
 *
 * INPUT
 *
 * OUTPUT
 *
 * RETURNS
 *
 * AUTHOR
 *
 * DESCRIPTION
 *
 *   -
 *
 * CHANGES
 *
 *   -
 *
 ******************************************************************************/

PRECOMP_PAR_DATA* Parametric::Copy_PrecompParVal()
{
	if (PData == NULL)
		return NULL;

	(PData->use)++;

	return (PData);
}


/*****************************************************************************
 *
 * FUNCTION
 *
 *   Destroy_PrecompParVal
 *
 * INPUT
 *
 * OUTPUT
 *
 * RETURNS
 *
 * AUTHOR
 *
 * DESCRIPTION
 *
 *   -
 *
 * CHANGES
 *
 *   -
 *
 ******************************************************************************/

void Parametric::Destroy_PrecompParVal()
{
	if (PData == NULL)
		return;

	PData->use--;
	if (PData->use == 0)
	{
		if (PData->flags & OK_X)
		{
			POV_FREE(PData->Low[0]);
			POV_FREE(PData->Hi[0]);
		}
		if (PData->flags & OK_Y)
		{
			POV_FREE(PData->Low[1]);
			POV_FREE(PData->Hi[1]);
		}
		if (PData->flags & OK_Z)
		{
			POV_FREE(PData->Low[2]);
			POV_FREE(PData->Hi[2]);
		}
		POV_FREE(PData);
	}
}


/*****************************************************************************
 *
 * FUNCTION
 *
 *   Evaluate_Function
 *
 * INPUT
 *
 * OUTPUT
 *
 * RETURNS
 *
 * AUTHOR
 *
 * DESCRIPTION
 *
 *   -
 *
 * CHANGES
 *   
 *   -
 *
 ******************************************************************************/

DBL Parametric::Evaluate_Function_UV(FPUContext *ctx, FUNCTION funct, const UV_VECT fnvec)
{
	ctx->SetLocal(U, fnvec[U]);
	ctx->SetLocal(V, fnvec[V]);

	return POVFPU_Run(ctx, funct);
}

/*****************************************************************************
 *
 * FUNCTION
 *
 *   Interval
 *
 * INPUT
 *
 * OUTPUT
 *
 * RETURNS
 *
 * AUTHOR
 *   
 *   Ron Parker
 *
 * DESCRIPTION
 *
 *   Assume that the function attains its maximum gradient over the whole
 *   range and determine its minimum and maximum (really lower and upper
 *   bounds, not strict maxima and minima) accordingly.
 *
 * CHANGES
 *   
 *   -
 *
 ******************************************************************************/

void Parametric::Interval(DBL dx, DBL a, DBL b, DBL max_gradient, DBL *Min, DBL *Max)
{
	DBL dy = fabs(a-b);
	DBL ofs = max_gradient*(dx-dy/max_gradient)/2;
	if ( ofs < 0 ) {
		ofs=0;
	}
	*Max = max(a,b)+ofs;
	*Min = min(a,b)-ofs;
}


/*****************************************************************************
 *
 * FUNCTION
 *
 *   Evaluate_Function_Interval
 *
 * INPUT
 *
 * OUTPUT
 *
 * RETURNS
 *
 * AUTHOR
 *
 * DESCRIPTION
 *
 *   -
 *
 * CHANGES
 *   
 *   -
 *
 ******************************************************************************/

void Parametric::Evaluate_Function_Interval_UV(FPUContext *ctx, FUNCTION funct, DBL threshold, const UV_VECT fnvec_low, const UV_VECT fnvec_hi, DBL max_gradient, DBL& low, DBL& hi)
{
	DBL f_0_0, f_0_1, f_1_0, f_1_1;
	DBL f_0_min, f_0_max;
	DBL f_1_min, f_1_max;
	DBL junk;

	/* Calculate the values at each corner */
	ctx->SetLocal(U, fnvec_low[U]);
	ctx->SetLocal(V, fnvec_low[V]);

	f_0_0 = POVFPU_Run(ctx, funct) - threshold;

	ctx->SetLocal(U, fnvec_low[U]);
	ctx->SetLocal(V, fnvec_hi[V]);

	f_0_1 = POVFPU_Run(ctx, funct) - threshold;

	ctx->SetLocal(U, fnvec_hi[U]);
	ctx->SetLocal(V, fnvec_low[V]);

	f_1_0 = POVFPU_Run(ctx, funct) - threshold;

	ctx->SetLocal(U, fnvec_hi[U]);
	ctx->SetLocal(V, fnvec_hi[V]);

	f_1_1 = POVFPU_Run(ctx, funct) - threshold;

	/* Determine a min and a max along the left edge of the patch */
	Interval( fnvec_hi[V]-fnvec_low[V], f_0_0, f_0_1, max_gradient, &f_0_min, &f_0_max);

	/* Determine a min and a max along the right edge of the patch */
	Interval( fnvec_hi[V]-fnvec_low[V], f_1_0, f_1_1, max_gradient, &f_1_min, &f_1_max);

	/* Assume that the upper bounds of both edges are attained at the same
	 u coordinate and determine what an upper bound along that line would
	 be if it existed.  That's the worst-case maximum value we can reach. */
	Interval( fnvec_hi[U]-fnvec_low[U], f_0_max, f_1_max, max_gradient, &junk, &hi);

	/* same as above to get a lower bound from the two edge lower bounds */
	Interval( fnvec_hi[U]-fnvec_low[U], f_0_min, f_1_min, max_gradient, &low, &junk);

	/*
	char str[200];
	int its=0;
	its++;
	if (its>20) Error("Boom!");
	sprintf( str, "%lf     %lf %lf %lf %lf   %lf %lf\n%lf %lf %lf %lf    %lf %lf    %lf %lf    %lf %lf\n",
		max_gradient,
		fnvec_low[U], fnvec_low[V],
		fnvec_hi[U], fnvec_hi[V],
		fnvec_hi[U]-fnvec_low[U],
		fnvec_hi[V]-fnvec_low[V],

		f_0_0,f_0_1,f_1_0,f_1_1,
		f_0_min,f_0_max,
		f_1_min,f_1_max,
		low,hi);
	Warning( 0, str );
	*/
	/* f_min and f_max now contain the maximum interval. */
}

}
