/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include "slg/engines/biaspathcpu/biaspathcpu.h"
#include "slg/core/mc.h"
#include "luxrays/core/randomgen.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BiasPathCPU RenderThread
//------------------------------------------------------------------------------

BiasPathCPURenderThread::BiasPathCPURenderThread(BiasPathCPURenderEngine *engine,
		const u_int index, IntersectionDevice *device) :
		CPUTileRenderThread(engine, index, device) {
}

void BiasPathCPURenderThread::SampleGrid(RandomGenerator *rndGen, const u_int size,
		const u_int ix, const u_int iy, float *u0, float *u1) const {
	if (size == 1) {
		*u0 = rndGen->floatValue();
		*u1 = rndGen->floatValue();
	} else {
		const float idim = 1.f / size;
		*u0 = (ix + rndGen->floatValue()) * idim;
		*u1 = (iy + rndGen->floatValue()) * idim;
	}
}

bool BiasPathCPURenderThread::DirectLightSampling(
		const LightSource *light, const float lightPickPdf,
		const float u0, const float u1,
		const float u2, const float u3,
		const Spectrum &pathThrouput, const BSDF &bsdf,
		Spectrum *radiance) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	Vector lightRayDir;
	float distance, directPdfW;
	Spectrum lightRadiance = light->Illuminate(*scene, bsdf.hitPoint.p,
			u0, u1, u2, &lightRayDir, &distance, &directPdfW);

	if (!lightRadiance.Black() && (distance > engine->nearStartLight)) {
		BSDFEvent event;
		float bsdfPdfW;
		Spectrum bsdfEval = bsdf.Evaluate(lightRayDir, &event, &bsdfPdfW);

		const float cosThetaToLight = AbsDot(lightRayDir, bsdf.hitPoint.shadeN);
		const float directLightSamplingPdfW = directPdfW * lightPickPdf;
		const float factor = cosThetaToLight / directLightSamplingPdfW;

		// MIS between direct light sampling and BSDF sampling
		const float weight = PowerHeuristic(directLightSamplingPdfW, bsdfPdfW);

		const Spectrum illumRadiance = (weight * factor) * pathThrouput * lightRadiance * bsdfEval;

		if (illumRadiance.Y() > engine->lowLightThreashold) {
			const float epsilon = Max(MachineEpsilon::E(bsdf.hitPoint.p), MachineEpsilon::E(distance));
			Ray shadowRay(bsdf.hitPoint.p, lightRayDir,
					epsilon,
					distance - epsilon);
			RayHit shadowRayHit;
			BSDF shadowBsdf;
			Spectrum connectionThroughput;
			// Check if the light source is visible
			if (!scene->Intersect(device, false, u3, &shadowRay,
					&shadowRayHit, &shadowBsdf, &connectionThroughput)) {
				*radiance += connectionThroughput * illumRadiance;
				return true;
			}
		}
	}

	return false;
}

bool BiasPathCPURenderThread::DirectLightSamplingONE(
		RandomGenerator *rndGen,
		const Spectrum &pathThrouput, const BSDF &bsdf,
		Spectrum *radiance) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	// Pick a light source to sample
	float lightPickPdf;
	const LightSource *light = scene->SampleAllLights(rndGen->floatValue(), &lightPickPdf);

	return DirectLightSampling(light, lightPickPdf, rndGen->floatValue(), rndGen->floatValue(),
			rndGen->floatValue(), rndGen->floatValue(), pathThrouput, bsdf, radiance);
}

bool BiasPathCPURenderThread::DirectLightSamplingALL(
		RandomGenerator *rndGen,
		const Spectrum &pathThrouput, const BSDF &bsdf,
		Spectrum *radiance) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	bool illuminated = false;
	const u_int lightsSize = scene->GetLightCount();
	for (u_int i = 0; i < lightsSize; ++i) {
		const LightSource *light = scene->GetLightByIndex(i);
		const int samples = light->GetSamples();
		const u_int samplesToDo = (samples < 0) ? engine->directLightSamples : ((u_int)samples);

		Spectrum lightRadiance;
		u_int sampleCount = 0;
		for (u_int sampleY = 0; sampleY < samplesToDo; ++sampleY) {
			for (u_int sampleX = 0; sampleX < samplesToDo; ++sampleX) {
				float u0, u1;
				SampleGrid(rndGen, samplesToDo, sampleX, sampleY, &u0, &u1);

				if (DirectLightSampling(light, 1.f, u0, u1,
						rndGen->floatValue(), rndGen->floatValue(),
						pathThrouput, bsdf, &lightRadiance)) {
					illuminated = true;
					++sampleCount;
				}
			}
		}

		if (sampleCount > 0)
			*radiance += lightRadiance / sampleCount;
	}

	return illuminated;
}

bool BiasPathCPURenderThread::DirectHitFiniteLight(
		const bool lastSpecular, const Spectrum &pathThrouput,
		const float distance, const BSDF &bsdf, const float lastPdfW,
		Spectrum *radiance) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	float directPdfA;
	const Spectrum emittedRadiance = bsdf.GetEmittedRadiance(&directPdfA);

	if (emittedRadiance.Y() > engine->lowLightThreashold) {
		float weight;
		if (!lastSpecular) {
			// This PDF used for MIS is correct because lastSpecular is always
			// true when using DirectLightSamplingALL()
			const float lightPickProb = scene->SampleAllLightPdf(bsdf.GetLightSource());
			const float directPdfW = PdfAtoW(directPdfA, distance,
				AbsDot(bsdf.hitPoint.fixedDir, bsdf.hitPoint.shadeN));

			// MIS between BSDF sampling and direct light sampling
			weight = PowerHeuristic(lastPdfW, directPdfW * lightPickProb);
		} else
			weight = 1.f;

		*radiance +=  pathThrouput * weight * emittedRadiance;

		return true;
	}

	return false;
}

bool BiasPathCPURenderThread::DirectHitInfiniteLight(
		const bool lastSpecular, const Spectrum &pathThrouput,
		const Vector &eyeDir, const float lastPdfW, Spectrum *radiance) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	// Infinite light
	bool illuminated = false;
	float directPdfW;
	if (scene->envLight) {
		const Spectrum envRadiance = scene->envLight->GetRadiance(*scene, -eyeDir, &directPdfW);
		if (envRadiance.Y() > engine->lowLightThreashold) {
			if(!lastSpecular) {
				// This PDF used for MIS is correct because lastSpecular is always
				// true when using DirectLightSamplingALL()

				// MIS between BSDF sampling and direct light sampling
				*radiance += PowerHeuristic(lastPdfW, directPdfW) * pathThrouput * envRadiance;
			} else
				*radiance += pathThrouput * envRadiance;
		}

		illuminated = true;
	}

	// Sun light
	if (scene->sunLight) {
		const Spectrum sunRadiance = scene->sunLight->GetRadiance(*scene, -eyeDir, &directPdfW);
		if (sunRadiance.Y() > engine->lowLightThreashold) {
			if(!lastSpecular) {
				// MIS between BSDF sampling and direct light sampling
				*radiance += PowerHeuristic(lastPdfW, directPdfW) * pathThrouput * sunRadiance;
			} else
				*radiance += pathThrouput * sunRadiance;
		}

		illuminated = true;
	}

	return illuminated;
}

bool BiasPathCPURenderThread::ContinueTracePath(RandomGenerator *rndGen,
		PathDepthInfo depthInfo, Ray ray,
		Spectrum pathThrouput, BSDFEvent lastBSDFEvent, float lastPdfW,
		Spectrum *radiance) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	BSDF bsdf;
	bool illuminated = false;
	for (;;) {
		RayHit rayHit;
		Spectrum connectionThroughput;
		if (!scene->Intersect(device, false, rndGen->floatValue(),
				&ray, &rayHit, &bsdf, &connectionThroughput)) {
			// Nothing was hit, look for infinitelight
			illuminated |= DirectHitInfiniteLight(lastBSDFEvent & SPECULAR, pathThrouput * connectionThroughput, ray.d,
					lastPdfW, radiance);
			break;
		}
		pathThrouput *= connectionThroughput;

		// Something was hit

		// Check if it is visible in indirect paths
		if (((lastBSDFEvent & DIFFUSE) && !bsdf.IsVisibleIndirectDiffuse()) ||
				((lastBSDFEvent & GLOSSY) && !bsdf.IsVisibleIndirectGlossy()) ||
				((lastBSDFEvent & SPECULAR) && !bsdf.IsVisibleIndirectSpecular()))
			break;

		// Check if it is a light source
		if (bsdf.IsLightSource() && (rayHit.t > engine->nearStartLight)) {
			illuminated |= DirectHitFiniteLight(lastBSDFEvent & SPECULAR, pathThrouput,
					rayHit.t, bsdf, lastPdfW, radiance);
		}

		// Note: pass-through check is done inside SceneIntersect()

		//----------------------------------------------------------------------
		// Direct light sampling
		//----------------------------------------------------------------------

		if (!bsdf.IsDelta())
			illuminated |= DirectLightSamplingONE(rndGen, pathThrouput, bsdf, radiance);

		//----------------------------------------------------------------------
		// Build the next vertex path ray
		//----------------------------------------------------------------------

		Vector sampledDir;
		float cosSampledDir;
		const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
				rndGen->floatValue(),
				rndGen->floatValue(),
				&lastPdfW, &cosSampledDir, &lastBSDFEvent);
		if (bsdfSample.Y() <= engine->lowLightThreashold)
			break;

		// Check if I have to stop because of path depth
		depthInfo.IncDepths(lastBSDFEvent);
		if (!depthInfo.CheckDepths(engine->maxPathDepth))
			break;

		pathThrouput *= bsdfSample * (cosSampledDir / lastPdfW);
		assert (!pathThrouput.IsNaN() && !pathThrouput.IsInf());

		ray = Ray(bsdf.hitPoint.p, sampledDir);
	}

	assert (!radiance->IsNaN() && !radiance->IsInf());

	return illuminated;
}

// NOTE: bsdf.hitPoint.passThroughEvent is modified by this method
bool BiasPathCPURenderThread::SampleComponent(RandomGenerator *rndGen,
		const BSDFEvent requestedEventTypes, const u_int size, BSDF &bsdf,
		Spectrum *radiance) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;

	bool illuminated = false;
	for (u_int sampleY = 0; sampleY < size; ++sampleY) {
		for (u_int sampleX = 0; sampleX < size; ++sampleX) {
			float u0, u1;
			SampleGrid(rndGen, size, sampleX, sampleY, &u0, &u1);
			bsdf.hitPoint.passThroughEvent = rndGen->floatValue();

			Vector sampledDir;
			BSDFEvent event;
			float pdfW, cosSampledDir;
			const Spectrum bsdfSample = bsdf.Sample(&sampledDir, u0, u1,
					&pdfW, &cosSampledDir, &event, requestedEventTypes);
			if (bsdfSample.Y() <= engine->lowLightThreashold)
				continue;

			// Check if I have to stop because of path depth
			PathDepthInfo depthInfo;
			depthInfo.IncDepths(event);
			if (!depthInfo.CheckDepths(engine->maxPathDepth))
				continue;

			const Spectrum continuePathThrouput = bsdfSample * (cosSampledDir / pdfW);
			assert (!continuePathThrouput.IsNaN() && !continuePathThrouput.IsInf());

			Ray continueRay(bsdf.hitPoint.p, sampledDir);
			illuminated |= ContinueTracePath(rndGen, depthInfo, continueRay,
					continuePathThrouput, event, pdfW, radiance);
		}
	}
	*radiance /= size * size;

	return illuminated;
}

void BiasPathCPURenderThread::TraceEyePath(RandomGenerator *rndGen, const Ray &ray,
		SampleResult *sampleResult) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	Ray eyeRay = ray;
	RayHit eyeRayHit;
	Spectrum pathThrouput(1.f);
	BSDF bsdf;
	if (!scene->Intersect(device, false, rndGen->floatValue(),
			&eyeRay, &eyeRayHit, &bsdf, &pathThrouput)) {
		// Nothing was hit, look for infinitelight
		Spectrum radiance;
		DirectHitInfiniteLight(true, pathThrouput, eyeRay.d,
				1.f, &radiance);
		sampleResult->emission = radiance;
		sampleResult->radiancePerPixelNormalized += radiance;

		sampleResult->alpha = 0.f;
		sampleResult->depth = std::numeric_limits<float>::infinity();
		sampleResult->position = Point(
				std::numeric_limits<float>::infinity(),
				std::numeric_limits<float>::infinity(),
				std::numeric_limits<float>::infinity());
		sampleResult->geometryNormal = Normal(
				std::numeric_limits<float>::infinity(),
				std::numeric_limits<float>::infinity(),
				std::numeric_limits<float>::infinity());
		sampleResult->shadingNormal = Normal(
				std::numeric_limits<float>::infinity(),
				std::numeric_limits<float>::infinity(),
				std::numeric_limits<float>::infinity());
		sampleResult->materialID = std::numeric_limits<u_int>::max();
		sampleResult->directDiffuse = Spectrum();
		sampleResult->directGlossy = Spectrum();
		sampleResult->indirectDiffuse = Spectrum();
		sampleResult->indirectGlossy = Spectrum();
		sampleResult->indirectSpecular = Spectrum();
		sampleResult->directShadowMask = 0.f;
		sampleResult->indirectShadowMask = 0.f;
	} else {
		// Something was hit
		sampleResult->alpha = 1.f;
		sampleResult->depth = eyeRayHit.t;
		sampleResult->position = bsdf.hitPoint.p;
		sampleResult->geometryNormal = bsdf.hitPoint.geometryN;
		sampleResult->shadingNormal = bsdf.hitPoint.shadeN;
		sampleResult->materialID = bsdf.GetMaterialID();

		// Check if it is a light source
		if (bsdf.IsLightSource() && (eyeRayHit.t > engine->nearStartLight)) {
			Spectrum radiance;
			DirectHitFiniteLight(true, pathThrouput,
					eyeRayHit.t, bsdf, 1.f, &radiance);
			sampleResult->emission = radiance;
			sampleResult->radiancePerPixelNormalized += radiance;
		} else
			sampleResult->emission = Spectrum();

		// Note: pass-through check is done inside SceneIntersect()

		//----------------------------------------------------------------------
		// Direct light sampling
		//----------------------------------------------------------------------

		if (!bsdf.IsDelta()) {
			Spectrum radiance;
			const bool illuminated = (engine->lightSamplingStrategyONE) ?
				DirectLightSamplingONE(rndGen, pathThrouput, bsdf, &radiance) :
				DirectLightSamplingALL(rndGen, pathThrouput, bsdf, &radiance);

			if (illuminated) {
				radiance *= pathThrouput;

				if (bsdf.GetEventTypes() & DIFFUSE) {
					sampleResult->directDiffuse = radiance;
					sampleResult->directGlossy = Spectrum();
				} else {
					sampleResult->directDiffuse = Spectrum();
					sampleResult->directGlossy = radiance;
				}
				sampleResult->radiancePerPixelNormalized += radiance;

				sampleResult->directShadowMask = 0.f;
			} else {
				sampleResult->directDiffuse = Spectrum();
				sampleResult->directGlossy = Spectrum();
				sampleResult->directShadowMask = 1.f;				
			}
		} else {
			sampleResult->directDiffuse = Spectrum();
			sampleResult->directGlossy = Spectrum();
			sampleResult->directShadowMask = 1.f;
		}

		//----------------------------------------------------------------------
		// Split the path
		//----------------------------------------------------------------------

		const BSDFEvent materialEventTypes = bsdf.GetEventTypes();
		int materialSamples = bsdf.GetSamples();
		sampleResult->indirectShadowMask = 1.f;
		sampleResult->indirectDiffuse = Spectrum();
		sampleResult->indirectGlossy = Spectrum();
		sampleResult->indirectSpecular = Spectrum();

		//----------------------------------------------------------------------
		// Sample the diffuse component
		//
		// NOTE: bsdf.hitPoint.passThroughEvent is modified by SampleComponent()
		//----------------------------------------------------------------------

		if ((engine->maxPathDepth.diffuseDepth > 0) && (materialEventTypes & DIFFUSE)) {
			const u_int diffuseSamples = (materialSamples < 0) ? engine->diffuseSamples : ((u_int)materialSamples);

			if (diffuseSamples > 0) {
				Spectrum radiance;
				const bool illuminated = SampleComponent(
						rndGen, DIFFUSE | REFLECT | TRANSMIT,
						diffuseSamples, bsdf, &radiance);

				if (illuminated) {
					radiance *= pathThrouput;
					sampleResult->indirectDiffuse = radiance;
					sampleResult->radiancePerPixelNormalized += radiance;
					sampleResult->indirectShadowMask = 0.f;
				}
			}
		}

		//----------------------------------------------------------------------
		// Sample the glossy component
		//
		// NOTE: bsdf.hitPoint.passThroughEvent is modified by SampleComponent()
		//----------------------------------------------------------------------

		if ((engine->maxPathDepth.glossyDepth > 0) && (materialEventTypes & GLOSSY)) {
			const u_int glossySamples = (materialSamples < 0) ? engine->glossySamples : ((u_int)materialSamples);

			if (glossySamples > 0) {
				Spectrum radiance;
				const bool illuminated = SampleComponent(
						rndGen, GLOSSY | REFLECT | TRANSMIT, 
						glossySamples, bsdf, &radiance);

				if (illuminated) {
					radiance *= pathThrouput;
					sampleResult->indirectGlossy = radiance;
					sampleResult->radiancePerPixelNormalized += radiance;
					sampleResult->indirectShadowMask = 0.f;
				}
			}
		}

		//----------------------------------------------------------------------
		// Sample the specular component
		//
		// NOTE: bsdf.hitPoint.passThroughEvent is modified by SampleComponent()
		//----------------------------------------------------------------------

		if ((engine->maxPathDepth.specularDepth > 0) && (materialEventTypes & SPECULAR)) {
			const u_int speculaSamples = (materialSamples < 0) ? engine->specularSamples : ((u_int)materialSamples);

			if (speculaSamples > 0) {
				Spectrum radiance;
				const bool illuminated = SampleComponent(
						rndGen, SPECULAR | REFLECT | TRANSMIT,
						speculaSamples, bsdf, &radiance);

				if (illuminated) {
					radiance *= pathThrouput;
					sampleResult->indirectSpecular = radiance;
					sampleResult->radiancePerPixelNormalized += radiance;
					sampleResult->indirectShadowMask = 0.f;
				}
			}
		}
	}
}

void BiasPathCPURenderThread::RenderPixelSample(RandomGenerator *rndGen,
		const FilterDistribution &filterDistribution,
		const u_int x, const u_int y,
		const u_int xOffset, const u_int yOffset,
		const u_int sampleX, const u_int sampleY) {
	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;

	float u0, u1;
	SampleGrid(rndGen, engine->aaSamples, sampleX, sampleY, &u0, &u1);

	// Sample according the pixel filter distribution
	filterDistribution.SampleContinuous(u0, u1, &u0, &u1);

	SampleResult sampleResult(Film::RADIANCE_PER_PIXEL_NORMALIZED | Film::ALPHA | Film::DEPTH |
		Film::POSITION | Film::GEOMETRY_NORMAL | Film::SHADING_NORMAL | Film::MATERIAL_ID |
		Film::DIRECT_DIFFUSE | Film::DIRECT_GLOSSY | Film::EMISSION | Film::INDIRECT_DIFFUSE |
		Film::INDIRECT_GLOSSY | Film::INDIRECT_SPECULAR | Film::DIRECT_SHADOW_MASK |
		Film::INDIRECT_SHADOW_MASK);
	sampleResult.filmX = xOffset + x + .5f + u0;
	sampleResult.filmY = yOffset + y + .5f + u1;
	Ray eyeRay;
	engine->renderConfig->scene->camera->GenerateRay(sampleResult.filmX, sampleResult.filmY,
			&eyeRay, rndGen->floatValue(), rndGen->floatValue());

	// Trace the path
	TraceEyePath(rndGen, eyeRay, &sampleResult);

	// Clamping
	if (engine->clampValueEnabled)
		sampleResult.radiancePerPixelNormalized = sampleResult.radiancePerPixelNormalized.Clamp(0.f, engine->clampMaxValue);

	tileFilm->AddSampleCount(1.0);
	tileFilm->AddSample(x, y, sampleResult);
}

void BiasPathCPURenderThread::RenderFunc() {
	//SLG_LOG("[BiasPathCPURenderEngine::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	BiasPathCPURenderEngine *engine = (BiasPathCPURenderEngine *)renderEngine;
	RandomGenerator *rndGen = new RandomGenerator(engine->seedBase + threadIndex);
	Film *film = engine->film;
	const Filter *filter = film->GetFilter();
	const u_int filmWidth = film->GetWidth();
	const u_int filmHeight = film->GetHeight();
	const FilterDistribution filterDistribution(filter, 64);

	//--------------------------------------------------------------------------
	// Extract the tile to render
	//--------------------------------------------------------------------------

	TileRepository::Tile *tile = NULL;
	bool interruptionRequested = boost::this_thread::interruption_requested();
	while (engine->NextTile(&tile, tileFilm) && !interruptionRequested) {
		// Render the tile
		tileFilm->Reset();
		const u_int tileWidth = Min(engine->tileRepository->tileSize, filmWidth - tile->xStart);
		const u_int tileHeight = Min(engine->tileRepository->tileSize, filmHeight - tile->yStart);
		//SLG_LOG("[BiasPathCPURenderEngine::" << threadIndex << "] Tile: "
		//		"(" << tile->xStart << ", " << tile->yStart << ") => " <<
		//		"(" << tileWidth << ", " << tileHeight << ")");

		for (u_int y = 0; y < tileHeight && !interruptionRequested; ++y) {
			for (u_int x = 0; x < tileWidth && !interruptionRequested; ++x) {
				if (tile->sampleIndex >= 0) {
					const u_int sampleX = tile->sampleIndex % engine->aaSamples;
					const u_int sampleY = tile->sampleIndex / engine->aaSamples;
					RenderPixelSample(rndGen, filterDistribution, x, y,
							tile->xStart, tile->yStart, sampleX, sampleY);
				} else {
					for (u_int sampleY = 0; sampleY < engine->aaSamples; ++sampleY) {
						for (u_int sampleX = 0; sampleX < engine->aaSamples; ++sampleX) {
							RenderPixelSample(rndGen, filterDistribution, x, y,
									tile->xStart, tile->yStart, sampleX, sampleY);
						}
					}
				}

				interruptionRequested = boost::this_thread::interruption_requested();
#ifdef WIN32
				// Work around Windows bad scheduling
				renderThread->yield();
#endif
			}
		}
	}

	delete rndGen;

	//SLG_LOG("[BiasPathCPURenderEngine::" << threadIndex << "] Rendering thread halted");
}
