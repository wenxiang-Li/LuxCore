#line 2 "sampler_funcs.cl"

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

//------------------------------------------------------------------------------
// Random Sampler Kernel
//------------------------------------------------------------------------------

#if (PARAM_SAMPLER_TYPE == 0)

#define Sampler_GetSamplePath(index) (Rnd_FloatValue(seed))
#define Sampler_GetSamplePathVertex(depth, index) (Rnd_FloatValue(seed))

__global float *Sampler_GetSampleData(__global Sample *sample, __global float *samplesData) {
	const size_t gid = get_global_id(0);
	return &samplesData[gid * TOTAL_U_SIZE];
}

__global float *Sampler_GetSampleDataPathBase(__global Sample *sample, __global float *sampleData) {
	return sampleData;
}

__global float *Sampler_GetSampleDataPathVertex(__global Sample *sample,
		__global float *sampleDataPathBase, const uint depth) {
	return &sampleDataPathBase[IDX_BSDF_OFFSET + depth * VERTEX_SAMPLE_SIZE];
}

void Sampler_SplatSample(
		Seed *seed,
		__global Sample *sample,
		__global float *sampleData
		FILM_PARAM_DECL
		) {
#if defined(PARAM_USE_FAST_PIXEL_FILTER)
	Film_AddSample(sample->result.pixelX, sample->result.pixelY,
			&sample->result, 1.f
			FILM_PARAM);
#else
	Film_SplatSample(&sample->result, 1.f
			FILM_PARAM);
#endif
}

void Sampler_NextSample(
		Seed *seed,
		__global Sample *sample,
		__global float *sampleData,
		const uint filmWidth, const uint filmHeight
#if defined(PARAM_USE_FAST_PIXEL_FILTER)
		, __global float *pixelFilterDistribution
#endif
		) {
	// Move to the next sample
	const float u0 = Rnd_FloatValue(seed);
	const float u1 = Rnd_FloatValue(seed);

	// TODO: remove sampleData[]
	sampleData[IDX_SCREEN_X] = u0;
	sampleData[IDX_SCREEN_Y] = u1;

	SampleResult_Init(&sample->result);

#if defined(PARAM_USE_FAST_PIXEL_FILTER)
	const float ux = u0 * filmWidth;
	const float uy = u1 * filmHeight;

	const uint pixelX = min((uint)ux, (uint)(filmWidth - 1));
	const uint pixelY = min((uint)uy, (uint)(filmHeight - 1));
	sample->result.pixelX = pixelX;
	sample->result.pixelY = pixelY;

	float uSubPixelX = ux - pixelX;
	float uSubPixelY = uy - pixelY;

	// Sample according the pixel filter distribution
	float distX, distY;
	FilterDistribution_SampleContinuous(pixelFilterDistribution, uSubPixelX, uSubPixelY, &distX, &distY);

	sample->result.filmX = pixelX + .5f + distX;
	sample->result.filmY = pixelY + .5f + distY;
#else
	sample->result.filmX = min(u0 * filmWidth, (float)(filmWidth - 1));
	sample->result.filmY = min(u1 * filmHeight, (float)(filmHeight - 1));
#endif
}

void Sampler_Init(Seed *seed, __global Sample *sample, __global float *sampleData,
		const uint filmWidth, const uint filmHeight
#if defined(PARAM_USE_FAST_PIXEL_FILTER)
		, __global float *pixelFilterDistribution
#endif
		) {
	Sampler_NextSample(seed, sample, sampleData, filmWidth, filmHeight
#if defined(PARAM_USE_FAST_PIXEL_FILTER)
			, pixelFilterDistribution
#endif
			);
}

#endif

//------------------------------------------------------------------------------
// Metropolis Sampler Kernel
//------------------------------------------------------------------------------

#if (PARAM_SAMPLER_TYPE == 1)

#define Sampler_GetSamplePath(index) (sampleDataPathBase[index])
#define Sampler_GetSamplePathVertex(depth, index) (sampleDataPathVertexBase[index])

__global float *Sampler_GetSampleData(__global Sample *sample, __global float *samplesData) {
	const size_t gid = get_global_id(0);
	return &samplesData[gid * (2 * TOTAL_U_SIZE)];
}

__global float *Sampler_GetSampleDataPathBase(__global Sample *sample, __global float *sampleData) {
	return &sampleData[sample->proposed * TOTAL_U_SIZE];
}

__global float *Sampler_GetSampleDataPathVertex(__global Sample *sample,
		__global float *sampleDataPathBase, const uint depth) {
	return &sampleDataPathBase[IDX_BSDF_OFFSET + depth * VERTEX_SAMPLE_SIZE];
}

void LargeStep(Seed *seed, __global float *proposedU) {
	for (int i = 0; i < TOTAL_U_SIZE; ++i)
		proposedU[i] = Rnd_FloatValue(seed);
}

float Mutate(Seed *seed, const float x) {
	const float s1 = 1.f / 512.f;
	const float s2 = 1.f / 16.f;

	const float randomValue = Rnd_FloatValue(seed);

	const float dx = s1 / (s1 / s2 + fabs(2.f * randomValue - 1.f)) -
		s1 / (s1 / s2 + 1.f);

	float mutatedX = x;
	if (randomValue < 0.5f) {
		mutatedX += dx;
		mutatedX = (mutatedX < 1.f) ? mutatedX : (mutatedX - 1.f);
	} else {
		mutatedX -= dx;
		mutatedX = (mutatedX < 0.f) ? (mutatedX + 1.f) : mutatedX;
	}

	return mutatedX;
}

float MutateScaled(Seed *seed, const float x, const float range) {
	const float s1 = 32.f;

	const float randomValue = Rnd_FloatValue(seed);

	const float dx = range / (s1 / (1.f + s1) + (s1 * s1) / (1.f + s1) *
		fabs(2.f * randomValue - 1.f)) - range / s1;

	float mutatedX = x;
	if (randomValue < 0.5f) {
		mutatedX += dx;
		mutatedX = (mutatedX < 1.f) ? mutatedX : (mutatedX - 1.f);
	} else {
		mutatedX -= dx;
		mutatedX = (mutatedX < 0.f) ? (mutatedX + 1.f) : mutatedX;
	}

	return mutatedX;
}

void SmallStep(Seed *seed, __global float *currentU, __global float *proposedU) {
	proposedU[IDX_SCREEN_X] = MutateScaled(seed, currentU[IDX_SCREEN_X],
			PARAM_SAMPLER_METROPOLIS_IMAGE_MUTATION_RANGE);
	proposedU[IDX_SCREEN_Y] = MutateScaled(seed, currentU[IDX_SCREEN_Y],
			PARAM_SAMPLER_METROPOLIS_IMAGE_MUTATION_RANGE);

	for (int i = IDX_SCREEN_Y + 1; i < TOTAL_U_SIZE; ++i)
		proposedU[i] = Mutate(seed, currentU[i]);
}

void Sampler_Init(Seed *seed, __global Sample *sample, __global float *sampleData,
		const uint filmWidth, const uint filmHeight
#if defined(PARAM_USE_FAST_PIXEL_FILTER)
		, __global float *pixelFilterDistribution
#endif
		) {
	sample->totalI = 0.f;
	sample->largeMutationCount = 1.f;

	sample->current = NULL_INDEX;
	sample->proposed = 1;

	sample->smallMutationCount = 0;
	sample->consecutiveRejects = 0;

	sample->weight = 0.f;

	__global float *sampleDataPathBase = Sampler_GetSampleDataPathBase(sample, sampleData);
	LargeStep(seed, sampleDataPathBase);

	SampleResult_Init(&sample->result);

#if defined(PARAM_USE_FAST_PIXEL_FILTER)
	const float ux = sampleDataPathBase[IDX_SCREEN_X] * filmWidth;
	const float uy = sampleDataPathBase[IDX_SCREEN_Y] * filmHeight;

	const uint pixelX = min((uint)ux, (uint)(filmWidth - 1));
	const uint pixelY = min((uint)uy, (uint)(filmHeight - 1));
	sample->result.pixelX = pixelX;
	sample->result.pixelY = pixelY;

	float uSubPixelX = ux - pixelX;
	float uSubPixelY = uy - pixelY;

	// Sample according the pixel filter distribution
	float distX, distY;
	FilterDistribution_SampleContinuous(pixelFilterDistribution, uSubPixelX, uSubPixelY, &distX, &distY);

	sample->result.filmX = pixelX + .5f + distX;
	sample->result.filmY = pixelY + .5f + distY;
#else
	sample->result.filmX = min(sampleDataPathBase[IDX_SCREEN_X] * filmWidth, (float)(filmWidth - 1));
	sample->result.filmY = min(sampleDataPathBase[IDX_SCREEN_Y] * filmHeight, (float)(filmHeight - 1));
#endif
}

void Sampler_SplatSample(
		Seed *seed,
		__global Sample *sample,
		__global float *sampleData
		FILM_PARAM_DECL
		) {
	//--------------------------------------------------------------------------
	// Accept/Reject the sample
	//--------------------------------------------------------------------------

	uint current = sample->current;
	uint proposed = sample->proposed;

	if (current == NULL_INDEX) {
		// It is the very first sample, I have still to initialize the current
		// sample

#if defined(PARAM_USE_FAST_PIXEL_FILTER)
		Film_AddSample(sample->result.pixelX, sample->result.pixelY,
				&sample->result, 1.f
				FILM_PARAM);
#else
		Film_SplatSample(&sample->result, 1.f
					FILM_PARAM);
#endif

		sample->currentResult = sample->result;
		sample->totalI = SampleResult_Radiance_Y(&sample->result);

		current = proposed;
		proposed ^= 1;
	} else {
		const float currentI = SampleResult_Radiance_Y(&sample->currentResult);

		float proposedI = SampleResult_Radiance_Y(&sample->result);
		proposedI = (isnan(proposedI) || isinf(proposedI)) ? 0.f : proposedI;

		float totalI = sample->totalI;
		uint largeMutationCount = sample->largeMutationCount;
		uint smallMutationCount = sample->smallMutationCount;
		if (smallMutationCount == 0) {
			// It is a large mutation
			totalI += proposedI;
			largeMutationCount += 1;

			sample->totalI = totalI;
			sample->largeMutationCount = largeMutationCount;
		}

		const float meanI = (totalI > 0.f) ? (totalI / largeMutationCount) : 1.f;

		// Calculate accept probability from old and new image sample
		uint consecutiveRejects = sample->consecutiveRejects;

		float accProb;
		if ((currentI > 0.f) && (consecutiveRejects < PARAM_SAMPLER_METROPOLIS_MAX_CONSECUTIVE_REJECT))
			accProb = min(1.f, proposedI / currentI);
		else
			accProb = 1.f;

		const float newWeight = accProb + ((smallMutationCount == 0) ? 1.f : 0.f);
		float weight = sample->weight;
		weight += 1.f - accProb;

		const float rndVal = Rnd_FloatValue(seed);

		/*if (get_global_id(0) == 0)
			printf(\"[%d] Current: (%f, %f, %f) [%f] Proposed: (%f, %f, %f) [%f] accProb: %f <%f>\\n\",
					consecutiveRejects,
					currentL.r, currentL.g, currentL.b, weight,
					proposedL.r, proposedL.g, proposedL.b, newWeight,
					accProb, rndVal);*/

		__global SampleResult *contrib;
		float norm;

		if ((accProb == 1.f) || (rndVal < accProb)) {
			/*if (get_global_id(0) == 0)
				printf(\"\\t\\tACCEPTED !\\n\");*/

			// Add accumulated contribution of previous reference sample
			norm = weight / (currentI / meanI + PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE);
			contrib = &sample->currentResult;

			current ^= 1;
			proposed ^= 1;
			consecutiveRejects = 0;

			weight = newWeight;
		} else {
			/*if (get_global_id(0) == 0)
				printf(\"\\t\\tREJECTED !\\n\");*/

			// Add contribution of new sample before rejecting it
			norm = newWeight / (proposedI / meanI + PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE);
			contrib = &sample->result;

			++consecutiveRejects;
		}

		if (norm > 0.f) {
			/*if (get_global_id(0) == 0)
				printf(\"\\t\\tContrib: (%f, %f, %f) [%f] consecutiveRejects: %d\\n\",
						contrib.r, contrib.g, contrib.b, norm, consecutiveRejects);*/

#if defined(PARAM_USE_FAST_PIXEL_FILTER)
			Film_AddSample(contrib->pixelX, contrib->pixelY,
					contrib, norm
					FILM_PARAM);
#else
			Film_SplatSample(contrib, norm
					FILM_PARAM);
#endif
		}

		// Check if it is an accepted mutation
		if (consecutiveRejects == 0) {
			// I can now (after Film_SplatSample()) overwrite sample->currentResult and sample->result
			sample->currentResult = sample->result;
		}

		sample->weight = weight;
		sample->consecutiveRejects = consecutiveRejects;
	}

	sample->current = current;
	sample->proposed = proposed;
}

void Sampler_NextSample(
		Seed *seed,
		__global Sample *sample,
		__global float *sampleData,
		const uint filmWidth, const uint filmHeight
#if defined(PARAM_USE_FAST_PIXEL_FILTER)
		, __global float *pixelFilterDistribution
#endif
		) {
	//--------------------------------------------------------------------------
	// Mutate the sample
	//--------------------------------------------------------------------------

	__global float *proposedU = &sampleData[sample->proposed * TOTAL_U_SIZE];
	if (Rnd_FloatValue(seed) < PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE) {
		LargeStep(seed, proposedU);
		sample->smallMutationCount = 0;
	} else {
		__global float *currentU = &sampleData[sample->current * TOTAL_U_SIZE];

		SmallStep(seed, currentU, proposedU);
		sample->smallMutationCount += 1;
	}

	SampleResult_Init(&sample->result);

#if defined(PARAM_USE_FAST_PIXEL_FILTER)
	const float ux = proposedU[IDX_SCREEN_X] * filmWidth;
	const float uy = proposedU[IDX_SCREEN_Y] * filmHeight;

	const uint pixelX = min((uint)ux, (uint)(filmWidth - 1));
	const uint pixelY = min((uint)uy, (uint)(filmHeight - 1));
	sample->result.pixelX = pixelX;
	sample->result.pixelY = pixelY;

	float uSubPixelX = ux - pixelX;
	float uSubPixelY = uy - pixelY;

	// Sample according the pixel filter distribution
	float distX, distY;
	FilterDistribution_SampleContinuous(pixelFilterDistribution, uSubPixelX, uSubPixelY, &distX, &distY);

	sample->result.filmX = pixelX + .5f + distX;
	sample->result.filmY = pixelY + .5f + distY;
#else
	sample->result.filmX = min(proposedU[IDX_SCREEN_X] * filmWidth, (float)(filmWidth - 1));
	sample->result.filmY = min(proposedU[IDX_SCREEN_Y] * filmHeight, (float)(filmHeight - 1));
#endif
}

#endif

//------------------------------------------------------------------------------
// Sobol Sampler Kernel
//------------------------------------------------------------------------------

#if (PARAM_SAMPLER_TYPE == 2)

uint SobolSampler_SobolDimension(const uint index, const uint dimension) {
	const uint offset = dimension * SOBOL_BITS;
	uint result = 0;
	uint i = index;

	for (uint j = 0; i; i >>= 1, j++) {
		if (i & 1)
			result ^= SOBOL_DIRECTIONS[offset + j];
	}

	return result;
}

float SobolSampler_GetSample(__global Sample *sample, const uint index) {
	const uint pass = sample->pass;

	const uint iResult = SobolSampler_SobolDimension(pass, index);
	const float fResult = iResult * (1.f / 0xffffffffu);

	// Cranley-Patterson rotation to reduce visible regular patterns
	const float shift = (index & 1) ? PARAM_SAMPLER_SOBOL_RNG0 : PARAM_SAMPLER_SOBOL_RNG1;
	const float val = fResult + shift;

	return val - floor(val);
}

#define Sampler_GetSamplePath(index) (SobolSampler_GetSample(sample, index))
#define Sampler_GetSamplePathVertex(depth, index) ((depth > PARAM_SAMPLER_SOBOL_MAXDEPTH) ? \
	Rnd_FloatValue(seed) : \
	SobolSampler_GetSample(sample, IDX_BSDF_OFFSET + (depth - 1) * VERTEX_SAMPLE_SIZE + index))

__global float *Sampler_GetSampleData(__global Sample *sample, __global float *samplesData) {
	const size_t gid = get_global_id(0);
	return &samplesData[gid * TOTAL_U_SIZE];
}

__global float *Sampler_GetSampleDataPathBase(__global Sample *sample, __global float *sampleData) {
	return sampleData;
}

__global float *Sampler_GetSampleDataPathVertex(__global Sample *sample,
		__global float *sampleDataPathBase, const uint depth) {
	return &sampleDataPathBase[IDX_BSDF_OFFSET + depth * VERTEX_SAMPLE_SIZE];
}

void Sampler_SplatSample(
		Seed *seed,
		__global Sample *sample,
		__global float *sampleData
		FILM_PARAM_DECL
		) {
#if defined(PARAM_USE_FAST_PIXEL_FILTER)
	Film_AddSample(sample->result.pixelX, sample->result.pixelY,
			&sample->result, 1.f
			FILM_PARAM);
#else
	Film_SplatSample(&sample->result, 1.f
			FILM_PARAM);
#endif
}

void Sampler_NextSample(
		Seed *seed,
		__global Sample *sample,
		__global float *sampleData,
		const uint filmWidth, const uint filmHeight
#if defined(PARAM_USE_FAST_PIXEL_FILTER)
		, __global float *pixelFilterDistribution
#endif
		) {
	// Move to the next sample
	sample->pass += get_global_size(0);

	const float u0 = Sampler_GetSamplePath(IDX_SCREEN_X);
	const float u1 = Sampler_GetSamplePath(IDX_SCREEN_Y);

	sampleData[IDX_SCREEN_X] = u0;
	sampleData[IDX_SCREEN_Y] = u1;

	SampleResult_Init(&sample->result);

#if defined(PARAM_USE_FAST_PIXEL_FILTER)
	const float ux = u0 * filmWidth;
	const float uy = u1 * filmHeight;

	const uint pixelX = min((uint)ux, (uint)(filmWidth - 1));
	const uint pixelY = min((uint)uy, (uint)(filmHeight - 1));
	sample->result.pixelX = pixelX;
	sample->result.pixelY = pixelY;

	float uSubPixelX = ux - pixelX;
	float uSubPixelY = uy - pixelY;

	// Sample according the pixel filter distribution
	float distX, distY;
	FilterDistribution_SampleContinuous(pixelFilterDistribution, uSubPixelX, uSubPixelY, &distX, &distY);

	sample->result.filmX = pixelX + .5f + distX;
	sample->result.filmY = pixelY + .5f + distY;
#else
	sample->result.filmX = min(u0 * filmWidth, (float)(filmWidth - 1));
	sample->result.filmY = min(u1 * filmHeight, (float)(filmHeight - 1));
#endif
}

void Sampler_Init(Seed *seed, __global Sample *sample, __global float *sampleData,
		const uint filmWidth, const uint filmHeight
#if defined(PARAM_USE_FAST_PIXEL_FILTER)
		, __global float *pixelFilterDistribution
#endif
		) {
	sample->pass = PARAM_SAMPLER_SOBOL_STARTOFFSET + get_global_id(0);

	Sampler_NextSample(seed, sample, sampleData, filmWidth, filmHeight
#if defined(PARAM_USE_FAST_PIXEL_FILTER)
			, pixelFilterDistribution
#endif
			);
}

#endif

