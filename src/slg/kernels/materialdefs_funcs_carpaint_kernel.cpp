#include <string>
namespace slg { namespace ocl {
std::string KernelSource_materialdefs_funcs_carpaint = 
"#line 2 \"materialdefs_funcs_carpaint.cl\"\n"
"\n"
"/***************************************************************************\n"
" * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *\n"
" *                                                                         *\n"
" *   This file is part of LuxRender.                                       *\n"
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
"// CarPaint material\n"
"//\n"
"// LuxRender carpaint material porting.\n"
"//------------------------------------------------------------------------------\n"
"\n"
"#if defined (PARAM_ENABLE_MAT_CARPAINT)\n"
"\n"
"BSDFEvent CarPaintMaterial_GetEventTypes() {\n"
"	return GLOSSY | REFLECT;\n"
"}\n"
"\n"
"bool CarPaintMaterial_IsDelta() {\n"
"	return false;\n"
"}\n"
"\n"
"float3 CarPaintMaterial_Evaluate(\n"
"		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,\n"
"		BSDFEvent *event, float *directPdfW,\n"
"		const float3 kaVal, const float d, const float3 kdVal, \n"
"		const float3 ks1Val, const float m1, const float r1,\n"
"		const float3 ks2Val, const float m2, const float r2,\n"
"		const float3 ks3Val, const float m3, const float r3) {\n"
"	float3 H = normalize(lightDir + eyeDir);\n"
"	if (all(H == 0.f))\n"
"	{\n"
"		if (directPdfW)\n"
"			*directPdfW = 0.f;\n"
"		return BLACK;\n"
"	}\n"
"	if (H.z < 0.f)\n"
"		H = -H;\n"
"\n"
"	float pdf = 0.f;\n"
"	int n = 1; // already counts the diffuse layer\n"
"\n"
"	// Absorption\n"
"	const float cosi = fabs(lightDir.z);\n"
"	const float coso = fabs(eyeDir.z);\n"
"	const float3 alpha = Spectrum_Clamp(kaVal);\n"
"	const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);\n"
"\n"
"	// Diffuse layer\n"
"	float3 result = absorption * Spectrum_Clamp(kdVal) * M_1_PI_F * fabs(lightDir.z);\n"
"\n"
"	// 1st glossy layer\n"
"	const float3 ks1 = Spectrum_Clamp(ks1Val);\n"
"	if (Spectrum_Filter(ks1) > 0.f && m1 > 0.f)\n"
"	{\n"
"		const float rough1 = m1 * m1;\n"
"		result += (SchlickDistribution_D(rough1, H, 0.f) * SchlickDistribution_G(rough1, lightDir, eyeDir) / (4.f * coso)) * (ks1 * FresnelSchlick_Evaluate(r1, dot(eyeDir, H)));\n"
"		pdf += SchlickDistribution_Pdf(rough1, H, 0.f);\n"
"		++n;\n"
"	}\n"
"	const float3 ks2 = Spectrum_Clamp(ks2Val);\n"
"	if (Spectrum_Filter(ks2) > 0.f && m2 > 0.f)\n"
"	{\n"
"		const float rough2 = m2 * m2;\n"
"		result += (SchlickDistribution_D(rough2, H, 0.f) * SchlickDistribution_G(rough2, lightDir, eyeDir) / (4.f * coso)) * (ks2 * FresnelSchlick_Evaluate(r2, dot(eyeDir, H)));\n"
"		pdf += SchlickDistribution_Pdf(rough2, H, 0.f);\n"
"		++n;\n"
"	}\n"
"	const float3 ks3 = Spectrum_Clamp(ks3Val);\n"
"	if (Spectrum_Filter(ks3) > 0.f && m3 > 0.f)\n"
"	{\n"
"		const float rough3 = m3 * m3;\n"
"		result += (SchlickDistribution_D(rough3, H, 0.f) * SchlickDistribution_G(rough3, lightDir, eyeDir) / (4.f * coso)) * (ks3 * FresnelSchlick_Evaluate(r3, dot(eyeDir, H)));\n"
"		pdf += SchlickDistribution_Pdf(rough3, H, 0.f);\n"
"		++n;\n"
"	}\n"
"\n"
"	// Front face: coating+base\n"
"	*event = GLOSSY | REFLECT;\n"
"\n"
"	// Finish pdf computation\n"
"	pdf /= 4.f * fabs(dot(lightDir, H));\n"
"	if (directPdfW)\n"
"		*directPdfW = (pdf + fabs(lightDir.z) * M_1_PI_F) / n;\n"
"\n"
"	return result;\n"
"}\n"
"\n"
"float3 CarPaintMaterial_Sample(\n"
"		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,\n"
"		const float u0, const float u1,\n"
"#if defined(PARAM_HAS_PASSTHROUGH)\n"
"		const float passThroughEvent,\n"
"#endif\n"
"		float *pdfW, float *cosSampledDir, BSDFEvent *event,\n"
"		const BSDFEvent requestedEvent,\n"
"		const float3 kaVal, const float d, const float3 kdVal, \n"
"		const float3 ks1Val, const float m1, const float r1,\n"
"		const float3 ks2Val, const float m2, const float r2,\n"
"		const float3 ks3Val, const float m3, const float r3) {\n"
"	if (!(requestedEvent & (GLOSSY | REFLECT)) ||\n"
"		(fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC))\n"
"		return BLACK;\n"
"\n"
"	// Test presence of components\n"
"	int n = 1; // already count the diffuse layer\n"
"	int sampled = 0; // sampled layer\n"
"	float3 result = BLACK;\n"
"	float pdf = 0.f;\n"
"	bool l1 = false, l2 = false, l3 = false;\n"
"	// 1st glossy layer\n"
"	const float3 ks1 = Spectrum_Clamp(ks1Val);\n"
"	if (Spectrum_Filter(ks1) > 0.f && m1 > 0.f)\n"
"	{\n"
"		l1 = true;\n"
"		++n;\n"
"	}\n"
"	// 2nd glossy layer\n"
"	const float3 ks2 = Spectrum_Clamp(ks2Val);\n"
"	if (Spectrum_Filter(ks2) > 0.f && m2 > 0.f)\n"
"	{\n"
"		l2 = true;\n"
"		++n;\n"
"	}\n"
"	// 3rd glossy layer\n"
"	const float3 ks3 = Spectrum_Clamp(ks3Val);\n"
"	if (Spectrum_Filter(ks3) > 0.f && m3 > 0.f)\n"
"	{\n"
"		l3 = true;\n"
"		++n;\n"
"	}\n"
"\n"
"	float3 wh;\n"
"	float cosWH;\n"
"	if (passThroughEvent < 1.f / n) {\n"
"		// Sample diffuse layer\n"
"		*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, &pdf);\n"
"\n"
"		*cosSampledDir = fabs((*sampledDir).z);\n"
"		if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)\n"
"			return BLACK;\n"
"\n"
"		// Absorption\n"
"		const float cosi = fabs(fixedDir.z);\n"
"		const float coso = fabs((*sampledDir).z);\n"
"		const float3 alpha = Spectrum_Clamp(kaVal);\n"
"		const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);\n"
"\n"
"		// Evaluate base BSDF\n"
"		result = absorption * Spectrum_Clamp(kdVal) * pdf;\n"
"\n"
"		wh = normalize(*sampledDir + fixedDir);\n"
"		if (wh.z < 0.f)\n"
"			wh = -wh;\n"
"		cosWH = fabs(dot(fixedDir, wh));\n"
"	} else if (passThroughEvent < 2.f / n && l1) {\n"
"		// Sample 1st glossy layer\n"
"		sampled = 1;\n"
"		const float rough1 = m1 * m1;\n"
"		float d;\n"
"		SchlickDistribution_SampleH(rough1, 0.f, u0, u1, &wh, &d, &pdf);\n"
"		cosWH = dot(fixedDir, wh);\n"
"		*sampledDir = 2.f * cosWH * wh - fixedDir;\n"
"		*cosSampledDir = fabs((*sampledDir).z);\n"
"		cosWH = fabs(cosWH);\n"
"\n"
"		if (((*sampledDir).z < DEFAULT_COS_EPSILON_STATIC) ||\n"
"			(fixedDir.z * (*sampledDir).z < 0.f))\n"
"			return BLACK;\n"
"\n"
"		pdf /= 4.f * cosWH;\n"
"		if (pdf <= 0.f)\n"
"			return BLACK;\n"
"\n"
"		result = FresnelSchlick_Evaluate(r1, cosWH);\n"
"\n"
"		const float G = SchlickDistribution_G(rough1, fixedDir, *sampledDir);\n"
"		result *= d * G / (4.f * fabs(fixedDir.z));\n"
"	} else if ((passThroughEvent < 2.f / n  ||\n"
"		(!l1 && passThroughEvent < 3.f / n)) && l2) {\n"
"		// Sample 2nd glossy layer\n"
"		sampled = 2;\n"
"		const float rough2 = m2 * m2;\n"
"		float d;\n"
"		SchlickDistribution_SampleH(rough2, 0.f, u0, u1, &wh, &d, &pdf);\n"
"		cosWH = dot(fixedDir, wh);\n"
"		*sampledDir = 2.f * cosWH * wh - fixedDir;\n"
"		*cosSampledDir = fabs((*sampledDir).z);\n"
"		cosWH = fabs(cosWH);\n"
"\n"
"		if (((*sampledDir).z < DEFAULT_COS_EPSILON_STATIC) ||\n"
"			(fixedDir.z * (*sampledDir).z < 0.f))\n"
"			return BLACK;\n"
"\n"
"		pdf /= 4.f * cosWH;\n"
"		if (pdf <= 0.f)\n"
"			return BLACK;\n"
"\n"
"		result = FresnelSchlick_Evaluate(r2, cosWH);\n"
"\n"
"		const float G = SchlickDistribution_G(rough2, fixedDir, *sampledDir);\n"
"		result *= d * G / (4.f * fabs(fixedDir.z));\n"
"	} else if (l3) {\n"
"		// Sample 3rd glossy layer\n"
"		sampled = 3;\n"
"		const float rough3 = m3 * m3;\n"
"		float d;\n"
"		SchlickDistribution_SampleH(rough3, 0.f, u0, u1, &wh, &d, &pdf);\n"
"		cosWH = dot(fixedDir, wh);\n"
"		*sampledDir = 2.f * cosWH * wh - fixedDir;\n"
"		*cosSampledDir = fabs((*sampledDir).z);\n"
"		cosWH = fabs(cosWH);\n"
"\n"
"		if (((*sampledDir).z < DEFAULT_COS_EPSILON_STATIC) ||\n"
"			(fixedDir.z * (*sampledDir).z < 0.f))\n"
"			return BLACK;\n"
"\n"
"		pdf /= 4.f * cosWH;\n"
"		if (pdf <= 0.f)\n"
"			return BLACK;\n"
"\n"
"		result = FresnelSchlick_Evaluate(r3, cosWH);\n"
"\n"
"		const float G = SchlickDistribution_G(rough3, fixedDir, *sampledDir);\n"
"		result *= d * G / (4.f * fabs(fixedDir.z));\n"
"	} else {\n"
"		// Sampling issue\n"
"		return BLACK;\n"
"	}\n"
"	*event = GLOSSY | REFLECT;\n"
"	// Add other components\n"
"	// Diffuse\n"
"	if (sampled != 0) {\n"
"		// Absorption\n"
"		const float cosi = fabs(fixedDir.z);\n"
"		const float coso = fabs((*sampledDir).z);\n"
"		const float3 alpha = Spectrum_Clamp(kaVal);\n"
"		const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);\n"
"\n"
"		const float pdf0 = fabs((*sampledDir).z) * M_1_PI_F;\n"
"		pdf += pdf0;\n"
"		result = absorption * Spectrum_Clamp(kdVal) * pdf0;\n"
"	}\n"
"	// 1st glossy\n"
"	if (l1 && sampled != 1) {\n"
"		const float rough1 = m1 * m1;\n"
"		const float d1 = SchlickDistribution_D(rough1, wh, 0.f);\n"
"		const float pdf1 = SchlickDistribution_Pdf(rough1, wh, 0.f) / (4.f * cosWH);\n"
"		if (pdf1 > 0.f) {\n"
"			result += (d1 *\n"
"				SchlickDistribution_G(rough1, fixedDir, *sampledDir) /\n"
"				(4.f * fabs(fixedDir.z))) *\n"
"				FresnelSchlick_Evaluate(r1, cosWH);\n"
"			pdf += pdf1;\n"
"		}\n"
"	}\n"
"	// 2nd glossy\n"
"	if (l2 && sampled != 2) {\n"
"		const float rough2 = m2 * m2;\n"
"		const float d2 = SchlickDistribution_D(rough2, wh, 0.f);\n"
"		const float pdf2 = SchlickDistribution_Pdf(rough2, wh, 0.f) / (4.f * cosWH);\n"
"		if (pdf2 > 0.f) {\n"
"			result += (d2 *\n"
"				SchlickDistribution_G(rough2, fixedDir, *sampledDir) /\n"
"				(4.f * fabs(fixedDir.z))) *\n"
"				FresnelSchlick_Evaluate(r2, cosWH);\n"
"			pdf += pdf2;\n"
"		}\n"
"	}\n"
"	// 3rd glossy\n"
"	if (l3 && sampled != 3) {\n"
"		const float rough3 = m3 * m3;\n"
"		const float d3 = SchlickDistribution_D(rough3, wh, 0.f);\n"
"		const float pdf3 = SchlickDistribution_Pdf(rough3, wh, 0.f) / (4.f * cosWH);\n"
"		if (pdf3 > 0.f) {\n"
"			result += (d3 *\n"
"				SchlickDistribution_G(rough3, fixedDir, *sampledDir) /\n"
"				(4.f * fabs(fixedDir.z))) *\n"
"				FresnelSchlick_Evaluate(r3, cosWH);\n"
"			pdf += pdf3;\n"
"		}\n"
"	}\n"
"	// Adjust pdf and result\n"
"	*pdfW = pdf / n;\n"
"	return result / *pdfW;\n"
"}\n"
"\n"
"#endif\n"
; } }
