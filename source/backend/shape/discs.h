/*******************************************************************************
 * discs.h
 *
 * This module contains all defines, typedefs, and prototypes for DISCS.CPP.
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
 * $File: //depot/povray/smp/source/backend/shape/discs.h $
 * $Revision: #18 $
 * $Change: 6139 $
 * $DateTime: 2013/11/25 21:34:55 $
 * $Author: clipka $
 *******************************************************************************/

#ifndef DISCS_H
#define DISCS_H

namespace pov
{

/*****************************************************************************
* Global preprocessor defines
******************************************************************************/

#define DISC_OBJECT            (BASIC_OBJECT)



/*****************************************************************************
* Global typedefs
******************************************************************************/

class Disc : public ObjectBase
{
	public:
		Vector3d center;  ///< Center of the disc.
		Vector3d normal;  ///< Direction perpendicular to the disc (plane normal).
		DBL d;            ///< The constant part of the plane equation.
		DBL iradius2;     ///< Distance from center to inner circle of the disc.
		DBL oradius2;     ///< Distance from center to outer circle of the disc.

		Disc();
		virtual ~Disc();

		virtual ObjectPtr Copy();

		virtual bool All_Intersections(const Ray&, IStack&, TraceThreadData *);
		virtual bool Inside(const Vector3d&, TraceThreadData *) const;
		virtual void Normal(Vector3d&, Intersection *, TraceThreadData *) const;
		// virtual void UVCoord(Vector2d&, const Intersection *, TraceThreadData *) const; // TODO FIXME - why is there no UV-mapping for this object?
		virtual void Translate(const Vector3d&, const TRANSFORM *);
		virtual void Rotate(const Vector3d&, const TRANSFORM *);
		virtual void Scale(const Vector3d&, const TRANSFORM *);
		virtual void Transform(const TRANSFORM *);
		virtual void Invert();
		virtual void Compute_BBox();

		void Compute_Disc();
	protected:
		bool Intersect(const Ray& ray, DBL *Depth) const;
};

}

#endif
