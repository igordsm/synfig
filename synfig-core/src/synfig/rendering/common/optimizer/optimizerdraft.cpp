/* === S Y N F I G ========================================================= */
/*!	\file synfig/rendering/common/optimizer/optimizerdraft.cpp
**	\brief Draft Optimizers
**
**	$Id$
**
**	\legal
**	......... ... 2017-2018 Ivan Mahonin
**
**	This package is free software; you can redistribute it and/or
**	modify it under the terms of the GNU General Public License as
**	published by the Free Software Foundation; either version 2 of
**	the License, or (at your option) any later version.
**
**	This package is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**	General Public License for more details.
**	\endlegal
*/
/* ========================================================================= */

/* === H E A D E R S ======================================================= */

#ifdef USING_PCH
#	include "pch.h"
#else
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#endif

#include "optimizerdraft.h"

#include "../task/taskcontour.h"
#include "../task/taskblur.h"
#include "../task/tasklayer.h"
#include "../task/tasktransformation.h"

#endif

using namespace synfig;
using namespace rendering;

/* === M A C R O S ========================================================= */

/* === G L O B A L S ======================================================= */

/* === P R O C E D U R E S ================================================= */

/* === M E T H O D S ======================================================= */


// OptimizerDraft

OptimizerDraft::OptimizerDraft()
{
	category_id = CATEGORY_ID_BEGIN;
	for_task = true;
}


// OptimizerDraftLowRes

OptimizerDraftLowRes::OptimizerDraftLowRes(Real scale): scale(scale)
{
	category_id = CATEGORY_ID_COORDS;
	depends_from = CATEGORY_BEGIN;
	for_root_task = true;
	for_task = false;
	deep_first = true;
}

void
OptimizerDraftLowRes::run(const RunParams &params) const
{
	if (params.ref_task && !params.parent)
	{
		Task::Handle sub_task = params.ref_task->clone();

		TaskTransformationAffine::Handle affine = new TaskTransformationAffine();
		affine->sub_task() = sub_task;
		affine->supersample = Vector(1.0/scale, 1.0/scale);
		affine->interpolation = Color::INTERPOLATION_NEAREST;
		affine->sub_task() = sub_task;

		affine->target_surface.swap( sub_task->target_surface );

		apply(params, affine);
	}
}


// OptimizerDraftTransformation

void
OptimizerDraftTransformation::run(const RunParams &params) const
{
	if (TaskTransformation::Handle transformation = TaskTransformation::Handle::cast_dynamic(params.ref_task))
	{
		if ( ( transformation->interpolation != Color::INTERPOLATION_NEAREST
		    && transformation->interpolation != Color::INTERPOLATION_LINEAR )
		  || approximate_greater_lp(transformation->supersample[0], 1.0)
		  || approximate_greater_lp(transformation->supersample[1], 1.0) )
		{
			transformation = TaskTransformation::Handle::cast_dynamic( transformation->clone() );
			transformation->interpolation = Color::INTERPOLATION_LINEAR;
			transformation->supersample[0] = std::min(transformation->supersample[0], 1.0);
			transformation->supersample[1] = std::min(transformation->supersample[1], 1.0);
			apply(params, transformation);
		}
	}
}


// OptimizerDraftContour

OptimizerDraftContour::OptimizerDraftContour(Real detail, bool antialias):
	detail(detail), antialias(antialias) { }

void
OptimizerDraftContour::run(const RunParams &params) const
{
	if (TaskContour::Handle contour = TaskContour::Handle::cast_dynamic(params.ref_task))
	{
		if ( approximate_less_lp(contour->detail, detail)
		 || ( !antialias
		   && contour->contour
		   && contour->contour->antialias
		   && contour->allow_antialias ))
		{
			contour = TaskContour::Handle::cast_dynamic(contour->clone());
			if (contour->detail < detail) contour->detail = detail;
			if (!antialias) contour->allow_antialias = false;
			apply(params, contour);
		}
	}
}


// OptimizerDraftBlur

void
OptimizerDraftBlur::run(const RunParams &params) const
{
	if (TaskBlur::Handle blur = TaskBlur::Handle::cast_dynamic(params.ref_task))
	{
		if (blur->blur.type != Blur::BOX && blur->blur.type != Blur::CROSS)
		{
			blur = TaskBlur::Handle::cast_dynamic(blur->clone());
			blur->blur.type = Blur::BOX;
			apply(params, blur);
		}
	}
}


// OptimizerDraftLayerRemove

OptimizerDraftLayerRemove::OptimizerDraftLayerRemove(const String &layername):
	layername(layername) { }

void
OptimizerDraftLayerRemove::run(const RunParams &params) const
{
	if (TaskLayer::Handle layer = TaskLayer::Handle::cast_dynamic(params.ref_task))
		if (layer->layer && layer->layer->get_name() == layername)
			apply(params, Task::Handle());
}


// OptimizerDraftLayerSkip

OptimizerDraftLayerSkip::OptimizerDraftLayerSkip(const String &layername):
	layername(layername)
	{ mode |= MODE_REPEAT_LAST; }

void
OptimizerDraftLayerSkip::run(const RunParams &params) const
{
	if (TaskLayer::Handle layer = TaskLayer::Handle::cast_dynamic(params.ref_task))
		if (layer->layer && layer->layer->get_name() == layername)
			apply(params, layer->sub_task());
}


/* === E N T R Y P O I N T ================================================= */
