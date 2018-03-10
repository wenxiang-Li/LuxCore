#include <string>
namespace slg { namespace ocl {
std::string KernelSource_sampler_tilepath_funcs = 
"#line 2 \"sampler_tilepath_funcs.cl\"\n"
"\n"
"/***************************************************************************\n"
" * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *\n"
" *                                                                         *\n"
" *   This file is part of LuxCoreRender.                                   *\n"
" *                                                                         *\n"
" * Licensed under the Apache License, Version 2.0 (the \"License\");         *\n"
" * you may not use this file except in compliance with the License.        *\n"
" * You may obtain a copy of the License at                                 *\n"
" *                                                                         *\n"
" *     http://www.apache.org/licenses/LICENSE-2.0                          *\n"
" *                                                                         *\n"
" * Unless required by applicable law or agreed to in writing, software     *\n"
" * distributed under the License is distributed on an \"AS IS\" BASIS,       *\n"
" * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*\n"
" * See the License for the specific language governing permissions and     *\n"
" * limitations under the License.                                          *\n"
" ***************************************************************************/\n"
"\n"
"//------------------------------------------------------------------------------\n"
"// Morton related functions\n"
"//------------------------------------------------------------------------------\n"
"\n"
"#if defined(RENDER_ENGINE_RTPATHOCL)\n"
"\n"
"// Morton decode from https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/\n"
"\n"
"// Inverse of Part1By1 - \"delete\" all odd-indexed bits\n"
"\n"
"OPENCL_FORCE_INLINE uint Compact1By1(uint x) {\n"
"	x &= 0x55555555;					// x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0\n"
"	x = (x ^ (x >> 1)) & 0x33333333;	// x = --fe --dc --ba --98 --76 --54 --32 --10\n"
"	x = (x ^ (x >> 2)) & 0x0f0f0f0f;	// x = ---- fedc ---- ba98 ---- 7654 ---- 3210\n"
"	x = (x ^ (x >> 4)) & 0x00ff00ff;	// x = ---- ---- fedc ba98 ---- ---- 7654 3210\n"
"	x = (x ^ (x >> 8)) & 0x0000ffff;	// x = ---- ---- ---- ---- fedc ba98 7654 3210\n"
"	return x;\n"
"}\n"
"\n"
"// Inverse of Part1By2 - \"delete\" all bits not at positions divisible by 3\n"
"\n"
"OPENCL_FORCE_INLINE uint Compact1By2(uint x) {\n"
"	x &= 0x09249249;					// x = ---- 9--8 --7- -6-- 5--4 --3- -2-- 1--0\n"
"	x = (x ^ (x >> 2)) & 0x030c30c3;	// x = ---- --98 ---- 76-- --54 ---- 32-- --10\n"
"	x = (x ^ (x >> 4)) & 0x0300f00f;	// x = ---- --98 ---- ---- 7654 ---- ---- 3210\n"
"	x = (x ^ (x >> 8)) & 0xff0000ff;	// x = ---- --98 ---- ---- ---- ---- 7654 3210\n"
"	x = (x ^ (x >> 16)) & 0x000003ff;	// x = ---- ---- ---- ---- ---- --98 7654 3210\n"
"	return x;\n"
"}\n"
"\n"
"OPENCL_FORCE_INLINE uint DecodeMorton2X(const uint code) {\n"
"	return Compact1By1(code >> 0);\n"
"}\n"
"\n"
"OPENCL_FORCE_INLINE uint DecodeMorton2Y(const uint code) {\n"
"	return Compact1By1(code >> 1);\n"
"}\n"
"\n"
"#endif\n"
"\n"
"//------------------------------------------------------------------------------\n"
"// TilePath Sampler Kernel\n"
"//------------------------------------------------------------------------------\n"
"\n"
"#if (PARAM_SAMPLER_TYPE == 3)\n"
"\n"
"OPENCL_FORCE_INLINE __global float *Sampler_GetSampleData(__global Sample *sample, __global float *samplesData) {\n"
"	const size_t gid = get_global_id(0);\n"
"	return &samplesData[gid * TOTAL_U_SIZE];\n"
"}\n"
"\n"
"OPENCL_FORCE_INLINE __global float *Sampler_GetSampleDataPathBase(__global Sample *sample, __global float *sampleData) {\n"
"	return sampleData;\n"
"}\n"
"\n"
"OPENCL_FORCE_INLINE __global float *Sampler_GetSampleDataPathVertex(__global Sample *sample,\n"
"		__global float *sampleDataPathBase, const uint depth) {\n"
"	return &sampleDataPathBase[IDX_BSDF_OFFSET + depth * VERTEX_SAMPLE_SIZE];\n"
"}\n"
"\n"
"OPENCL_FORCE_INLINE float Sampler_GetSamplePath(Seed *seed, __global Sample *sample,\n"
"		__global float *sampleDataPathBase, const uint index) {\n"
"	switch (index) {\n"
"		case IDX_SCREEN_X:\n"
"			return sampleDataPathBase[IDX_SCREEN_X];\n"
"		case IDX_SCREEN_Y:\n"
"			return sampleDataPathBase[IDX_SCREEN_Y];\n"
"		default:\n"
"#if defined(RENDER_ENGINE_RTPATHOCL)\n"
"			return Rnd_FloatValue(seed);\n"
"#else\n"
"			return SobolSequence_GetSample(sample, index);\n"
"#endif\n"
"	}\n"
"}\n"
"\n"
"OPENCL_FORCE_INLINE float Sampler_GetSamplePathVertex(Seed *seed, __global Sample *sample,\n"
"		__global float *sampleDataPathVertexBase,\n"
"		const uint depth, const uint index) {\n"
"	// The depth used here is counted from the first hit point of the path\n"
"	// vertex (so it is effectively depth - 1)\n"
"#if defined(RENDER_ENGINE_RTPATHOCL)\n"
"	return Rnd_FloatValue(seed);\n"
"#else\n"
"	if (depth < SOBOL_MAX_DEPTH)\n"
"		return SobolSequence_GetSample(sample, IDX_BSDF_OFFSET + depth * VERTEX_SAMPLE_SIZE + index);\n"
"	else\n"
"		return Rnd_FloatValue(seed);\n"
"#endif\n"
"}\n"
"\n"
"OPENCL_FORCE_NOT_INLINE void Sampler_SplatSample(\n"
"		Seed *seed,\n"
"		__global SamplerSharedData *samplerSharedData,\n"
"		__global Sample *sample, __global float *sampleData\n"
"		FILM_PARAM_DECL\n"
"		) {\n"
"	const size_t gid = get_global_id(0);\n"
"\n"
"#if defined(RENDER_ENGINE_RTPATHOCL)\n"
"	// Check if I'm in preview phase\n"
"	if (sample->pass < PARAM_RTPATHOCL_PREVIEW_RESOLUTION_REDUCTION_STEP) {\n"
"		// I have to copy the current pixel to fill the assigned square\n"
"		for (uint y = 0; y < PARAM_RTPATHOCL_PREVIEW_RESOLUTION_REDUCTION; ++y) {\n"
"			for (uint x = 0; x < PARAM_RTPATHOCL_PREVIEW_RESOLUTION_REDUCTION; ++x) {\n"
"				// The sample weight is very low so this value is rapidly replaced\n"
"				// during normal rendering\n"
"				const uint px = sample->result.pixelX + x;\n"
"				const uint py = sample->result.pixelY + y;\n"
"				// px and py are unsigned so there is no need to check if they are >= 0\n"
"				if ((px < filmWidth) && (py < filmHeight)) {\n"
"					Film_AddSample(px, py,\n"
"							&sample->result, .001f\n"
"							FILM_PARAM);\n"
"				}\n"
"			}\n"
"		}\n"
"	} else\n"
"#endif\n"
"		Film_AddSample(sample->result.pixelX, sample->result.pixelY,\n"
"				&sample->result, 1.f\n"
"				FILM_PARAM);\n"
"}\n"
"\n"
"OPENCL_FORCE_NOT_INLINE void Sampler_NextSample(\n"
"		Seed *seed,\n"
"		__global SamplerSharedData *samplerSharedData,\n"
"		__global Sample *sample, __global float *sampleData,\n"
"#if defined(PARAM_FILM_CHANNELS_HAS_CONVERGENCE)\n"
"		__global float *filmConvergence,\n"
"#endif\n"
"		const uint filmWidth, const uint filmHeight,\n"
"		const uint filmSubRegion0, const uint filmSubRegion1,\n"
"		const uint filmSubRegion2, const uint filmSubRegion3) {\n"
"	// Sampler_NextSample() is not used in TILEPATHSAMPLER\n"
"}\n"
"\n"
"OPENCL_FORCE_NOT_INLINE bool Sampler_Init(Seed *seed, __global SamplerSharedData *samplerSharedData,\n"
"		__global Sample *sample, __global float *sampleData,\n"
"#if defined(PARAM_FILM_CHANNELS_HAS_CONVERGENCE)\n"
"		__global float *filmConvergence,\n"
"#endif\n"
"		const uint filmWidth, const uint filmHeight,\n"
"		const uint filmSubRegion0, const uint filmSubRegion1,\n"
"		const uint filmSubRegion2, const uint filmSubRegion3,\n"
"		// cameraFilmWidth/cameraFilmHeight and filmWidth/filmHeight are usually\n"
"		// the same. They are different when doing tile rendering\n"
"		const uint cameraFilmWidth, const uint cameraFilmHeight,\n"
"		const uint tileStartX, const uint tileStartY,\n"
"		const uint tileWidth, const uint tileHeight,\n"
"		const uint tilePass, const uint aaSamples) {\n"
"	const size_t gid = get_global_id(0);\n"
"\n"
"#if defined(RENDER_ENGINE_RTPATHOCL)\n"
"	// 1 thread for each pixel\n"
"\n"
"	uint pixelX, pixelY;\n"
"	if (tilePass < PARAM_RTPATHOCL_PREVIEW_RESOLUTION_REDUCTION_STEP) {\n"
"		const uint samplesPerRow = filmWidth / PARAM_RTPATHOCL_PREVIEW_RESOLUTION_REDUCTION;\n"
"		const uint subPixelX = gid % samplesPerRow;\n"
"		const uint subPixelY = gid / samplesPerRow;\n"
"\n"
"		pixelX = subPixelX * PARAM_RTPATHOCL_PREVIEW_RESOLUTION_REDUCTION;\n"
"		pixelY = subPixelY * PARAM_RTPATHOCL_PREVIEW_RESOLUTION_REDUCTION;\n"
"	} else {\n"
"		const uint samplesPerRow = filmWidth / PARAM_RTPATHOCL_RESOLUTION_REDUCTION;\n"
"		const uint subPixelX = gid % samplesPerRow;\n"
"		const uint subPixelY = gid / samplesPerRow;\n"
"\n"
"		pixelX = subPixelX * PARAM_RTPATHOCL_RESOLUTION_REDUCTION;\n"
"		pixelY = subPixelY * PARAM_RTPATHOCL_RESOLUTION_REDUCTION;\n"
"\n"
"		const uint pixelsCount = PARAM_RTPATHOCL_RESOLUTION_REDUCTION;\n"
"		const uint pixelsCount2 = pixelsCount * pixelsCount;\n"
"\n"
"		// Rendering according a Morton curve\n"
"		const uint pixelIndex = tilePass % pixelsCount2;\n"
"		const uint mortonX = DecodeMorton2X(pixelIndex);\n"
"		const uint mortonY = DecodeMorton2Y(pixelIndex);\n"
"\n"
"		pixelX += mortonX;\n"
"		pixelY += mortonY;\n"
"	}\n"
"\n"
"	if ((pixelX >= tileWidth) || (pixelY >= tileHeight))\n"
"		return false;\n"
"\n"
"	sample->pass = tilePass;\n"
"\n"
"	sampleData[IDX_SCREEN_X] = pixelX + Rnd_FloatValue(seed);\n"
"	sampleData[IDX_SCREEN_Y] = pixelY + Rnd_FloatValue(seed);\n"
"#else\n"
"	// aaSamples * aaSamples threads for each pixel\n"
"\n"
"	const uint aaSamples2 = aaSamples * aaSamples;\n"
"\n"
"	if (gid >= filmWidth * filmHeight * aaSamples2)\n"
"		return false;\n"
"\n"
"	const uint pixelIndex = gid / aaSamples2;\n"
"\n"
"	const uint pixelX = pixelIndex % filmWidth;\n"
"	const uint pixelY = pixelIndex / filmWidth;\n"
"	if ((pixelX >= tileWidth) || (pixelY >= tileHeight))\n"
"		return false;\n"
"\n"
"	const uint sharedDataIndex = (tileStartX + pixelX - filmSubRegion0) +\n"
"			(tileStartY + pixelY - filmSubRegion2) * (filmSubRegion1 - filmSubRegion0 + 1);\n"
"	// Limit the number of pass skipped\n"
"	sample->rngPass = samplerSharedData[sharedDataIndex].rngPass;\n"
"	sample->rng0 = samplerSharedData[sharedDataIndex].rng0;\n"
"	sample->rng1 = samplerSharedData[sharedDataIndex].rng1;\n"
"\n"
"	sample->pass = tilePass * aaSamples2 + gid % aaSamples2;\n"
"	\n"
"	sampleData[IDX_SCREEN_X] = pixelX + SobolSequence_GetSample(sample, IDX_SCREEN_X);\n"
"	sampleData[IDX_SCREEN_Y] = pixelY + SobolSequence_GetSample(sample, IDX_SCREEN_Y);\n"
"#endif\n"
"\n"
"	return true;\n"
"}\n"
"\n"
"#endif\n"
; } }
