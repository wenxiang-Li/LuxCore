/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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

#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/tonemaps/autolinear.h"
#include "slg/film/imagepipeline/plugins/gammacorrection.h"

using namespace std;
using namespace luxrays;
using namespace slg;

Properties Film::ToProperties(const Properties &cfg) {
	Properties props;
	
	props <<
			cfg.Get(Property("film.width")(640u)) <<
			cfg.Get(Property("film.height")(480u)) <<
			cfg.Get(Property("film.safesave")(true)) <<
			cfg.Get(Property("batch.haltthreshold")(-1.f)) <<
			cfg.Get(Property("batch.haltthreshold.step")(64)) <<
			cfg.Get(Property("batch.haltthreshold.warmup")(64)) <<
			cfg.Get(Property("batch.halttime")(0.0)) <<
			cfg.Get(Property("batch.haltspp")(0u)) <<
			FilmOutputs::ToProperties(cfg);

	// Add also radiance group scales related property
	props << cfg.GetAllProperties("film.radiancescales.");

	return props;
}

bool Film::GetFilmSize(const Properties &cfg,
		u_int *filmFullWidth, u_int *filmFullHeight,
		u_int *filmSubRegion) {
	u_int width = 640;
	if (cfg.IsDefined("image.width")) {
		SLG_LOG("WARNING: deprecated property image.width");
		width = cfg.Get(Property("image.width")(width)).Get<u_int>();
	}
	width = cfg.Get(Property("film.width")(width)).Get<u_int>();

	u_int height = 480;
	if (cfg.IsDefined("image.height")) {
		SLG_LOG("WARNING: deprecated property image.height");
		height = cfg.Get(Property("image.height")(height)).Get<u_int>();
	}
	height = cfg.Get(Property("film.height")(width)).Get<u_int>();

	// Check if I'm rendering a film subregion
	u_int subRegion[4];
	bool subRegionUsed;
	if (cfg.IsDefined("film.subregion")) {
		const Property &prop = cfg.Get(Property("film.subregion")(0, width - 1u, 0, height - 1u));

		subRegion[0] = Max(0u, Min(width - 1, prop.Get<u_int>(0)));
		subRegion[1] = Max(0u, Min(width - 1, Max(subRegion[0] + 1, prop.Get<u_int>(1))));
		subRegion[2] = Max(0u, Min(height - 1, prop.Get<u_int>(2)));
		subRegion[3] = Max(0u, Min(height - 1, Max(subRegion[2] + 1, prop.Get<u_int>(3))));
		subRegionUsed = true;
	} else {
		subRegion[0] = 0;
		subRegion[1] = width - 1;
		subRegion[2] = 0;
		subRegion[3] = height - 1;
		subRegionUsed = false;
	}

	if (filmFullWidth)
		*filmFullWidth = width;
	if (filmFullHeight)
		*filmFullHeight = height;

	if (filmSubRegion) {
		filmSubRegion[0] = subRegion[0];
		filmSubRegion[1] = subRegion[1];
		filmSubRegion[2] = subRegion[2];
		filmSubRegion[3] = subRegion[3];
	}

	return subRegionUsed;
}

Film *Film::FromProperties(const Properties &cfg) {
	//--------------------------------------------------------------------------
	// Create the Film
	//--------------------------------------------------------------------------

	u_int filmFullWidth, filmFullHeight, filmSubRegion[4];
	const bool filmSubRegionUsed = GetFilmSize(cfg, &filmFullWidth, &filmFullHeight, filmSubRegion);

	SLG_LOG("Film resolution: " << filmFullWidth << "x" << filmFullHeight);
	if (filmSubRegionUsed)
		SLG_LOG("Film sub-region: " << filmSubRegion[0] << " " << filmSubRegion[1] << filmSubRegion[2] << " " << filmSubRegion[3]);
	auto_ptr<Film> film(new Film(filmFullWidth, filmFullHeight,
			filmSubRegionUsed ? filmSubRegion : NULL));

	// For compatibility with the past
	if (cfg.IsDefined("film.alphachannel.enable")) {
		SLG_LOG("WARNING: deprecated property film.alphachannel.enable");

		if (cfg.Get(Property("film.alphachannel.enable")(0)).Get<bool>())
			film->AddChannel(Film::ALPHA);
		else
			film->RemoveChannel(Film::ALPHA);
	}

	film->oclEnable = cfg.Get(Property("film.opencl.enable")(true)).Get<bool>();
	film->oclPlatformIndex = cfg.Get(Property("film.opencl.platform")(-1)).Get<int>();
	film->oclDeviceIndex = cfg.Get(Property("film.opencl.device")(-1)).Get<int>();

	//--------------------------------------------------------------------------
	// Add the default image pipeline
	//--------------------------------------------------------------------------

	auto_ptr<ImagePipeline> imagePipeline(new ImagePipeline());
	imagePipeline->AddPlugin(new AutoLinearToneMap());
	imagePipeline->AddPlugin(new GammaCorrectionPlugin(2.2f));

	film->SetImagePipelines(imagePipeline.release());

	//--------------------------------------------------------------------------
	// Add the default output
	//--------------------------------------------------------------------------

	film->Parse(Properties() << 
			Property("film.outputs.0.type")("RGB_IMAGEPIPELINE") <<
			Property("film.outputs.0.filename")("image.png"));

	//--------------------------------------------------------------------------
	// Create the image pipeline, initialize radiance channel scales
	// and film outputs
	//--------------------------------------------------------------------------

	film->Parse(cfg);

	return film.release();
}