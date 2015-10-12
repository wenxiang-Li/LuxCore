/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#ifndef _SLG_FILMSAMPLESPLATTER_H
#define	_SLG_FILMSAMPLESPLATTER_H

#include "slg/film/film.h"
#include "slg/film/filters/filter.h"

namespace slg {

//------------------------------------------------------------------------------
// FilmSampleSplatter
//
// Used to splat samples on a Film using PBRT-like pixel filtering.
//------------------------------------------------------------------------------

class FilmSampleSplatter {
public:
	FilmSampleSplatter(const Filter *flt);
	~FilmSampleSplatter();

	const Filter *GetFilter() const { return filter; }

	// This method must be thread-safe.
	void SplatSample(Film &film, const SampleResult &sampleResult, const float weight) const;

private:
	const Filter *filter;
	FilterLUTs *filterLUTs;
};
		
}

#endif	/* _SLG_FILMSAMPLESPLATTER_H */
