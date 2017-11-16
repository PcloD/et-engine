#include <et>
#include <inputdefines>
#include "importance-sampling.h"
#include "atmosphere.h"
#include "environment.h"
#include "bsdf.h"

#if (WRAP_EQ_TO_CUBEMAP)

	Texture2D<float4> baseColorTexture : DECL_TEXTURE(BaseColor);

#elif (VISUALIZE_CUBEMAP || DIFFUSE_CONVOLUTION || SPECULAR_CONVOLUTION || DOWNSAMPLE_CUBEMAP)

	TextureCube<float4> baseColorTexture : DECL_TEXTURE(BaseColor);

#endif

SamplerState baseColorSampler : DECL_SAMPLER(BaseColor);

cbuffer ObjectVariables : DECL_BUFFER(Object)
{
	row_major float4x4 worldTransform;
	float3 lightColor;
	float4 lightDirection;
};

cbuffer MaterialVariables : DECL_BUFFER(Material)
{
	float4 extraParameters;
};

struct VSOutput 
{
	float4 position : SV_Position;
	float2 texCoord0 : TEXCOORD0;
	float3 direction : TEXCOORD1;
};

#if (VISUALIZE_CUBEMAP)

#include <inputlayout>

VSOutput vertexMain(VSInput vsIn)
{
	VSOutput vsOut;
	vsOut.texCoord0 = vsIn.position.xy * 0.5 + 0.5;
	vsOut.position = float4(vsIn.position.xy, 0.0, 1.0);
	vsOut.direction = mul(vsOut.position, worldTransform).xyz;
	vsOut.position = mul(vsOut.position, worldTransform);
	return vsOut;
}

#else

VSOutput vertexMain(uint vertexIndex : SV_VertexID)
{
    float2 pos = float2((vertexIndex << 1) & 2, vertexIndex & 2) * 2.0 - 1.0;
	
	VSOutput vsOut;
    vsOut.texCoord0 = pos * 0.5 + 0.5;
    vsOut.position = float4(pos, 0.0, 1.0);
	vsOut.direction = mul(vsOut.position, worldTransform).xyz;
	return vsOut;
}

#endif

float areaElement(float x, float y)
{
    return atan2(x * y, sqrt(x * x + y * y + 1.0));
}
 
float texelSolidAngle(float U, float V, float invSize)
{
    float x0 = U - invSize;
    float x1 = U + invSize;
    float y0 = V - invSize;
    float y1 = V + invSize;
    return areaElement(x0, y0) - areaElement(x0, y1) - areaElement(x1, y0) + areaElement(x1, y1);
}

float texelSolidAngle(in float3 d, float invSize)
{
	float u = 0.0;
	float v = 0.0;
	float3 ad = abs(d);
	float maxComponent = max(ad.x, max(ad.y, ad.z));
	if (maxComponent == ad.x)
	{
		u = d.z / maxComponent;
		v = d.y / maxComponent;
	}
	if (maxComponent == ad.y)
	{
		u = d.z / maxComponent;
		v = d.x / maxComponent;
	}
	if (maxComponent == ad.z)
	{
		u = d.y / maxComponent;
		v = d.x / maxComponent;
	}
	return texelSolidAngle(u, v, invSize);
}

const float cScale = 1.0; // 0.0066666;

float4 fragmentMain(VSOutput fsIn) : SV_Target0
{
#if (VISUALIZE_CUBEMAP)	

	float phi = PI * (fsIn.texCoord0.x * 2.0 - 1.0);
	float theta = PI * (0.5 - fsIn.texCoord0.y);
	float sinTheta = sin(theta);
	float cosTheta = cos(theta);
	float sinPhi = sin(phi);
	float cosPhi = cos(phi);
	float3 sampleDirection = float3(cosPhi * cosTheta, sinTheta, sinPhi * cosTheta);
	return cScale * baseColorTexture.SampleLevel(baseColorSampler, sampleDirection, extraParameters.x);

#elif (WRAP_EQ_TO_CUBEMAP)

	float3 d = normalize(fsIn.direction);        	
	float u = atan2(d.z, d.x) * 0.5 / PI + 0.5;
	float v = asin(d.y) / PI + 0.5;
	return baseColorTexture.SampleLevel(baseColorSampler, float2(u, v), 0.0);

#elif (ATMOSPHERE)

	float3 d = normalize(fsIn.direction);        	
	return float4(sampleAtmosphere(d, lightDirection.xyz, lightColor), 1.0);

#elif (DOWNSAMPLE_CUBEMAP)
	
	return baseColorTexture.SampleLevel(baseColorSampler, fsIn.direction, extraParameters.x - 1.0);

#elif (DIFFUSE_CONVOLUTION)

	float3 n = normalize(fsIn.direction);

	const float3 t0[6] = {
		float3(1.0, 0.0, 0.0),
		float3(-1.0, 0.0, 0.0),
		float3(0.0, 1.0, 0.0),
		float3(0.0, -1.0, 0.0),
		float3(0.0, 0.0, 1.0),
		float3(0.0, 0.0, -1.0),
	};
	
	const float3 t1[6] = {
		float3(0.0, 1.0, 0.0),
		float3(0.0, 0.0, 1.0),
		float3(1.0, 0.0, 0.0),
	};
	
	const float3 t2[6] = {
		float3(0.0, 0.0, 1.0),
		float3(1.0, 0.0, 0.0),
		float3(0.0, 1.0, 0.0),
	};
	
	const uint sampledLevel = 3;

	uint level0Width = 0;
	uint level0Height = 0;
	baseColorTexture.GetDimensions(level0Width, level0Height);
	
	uint w = level0Width >> sampledLevel;
	uint h = level0Height >> sampledLevel;
	uint numSamples = w * h * 6;
	float invFaceSize = 1.0 / float(min(w, h));

	float passedSamples = 0.0;
	float3 integralResult = 0.0;
	for (uint face = 0; face < 6; ++face)
	{
		float3 a0 = t0[face];
		float3 a1 = t1[face / 2];
		float3 a2 = t2[face / 2];
		
		for (uint y = 0; y < h; ++y)
		{
			float v = (float(y) / float(h - 1)) * 2.0 - 1.0;
			for (uint x = 0; x < w; ++x)
			{
				float u = (float(x) / float(w - 1)) * 2.0 - 1.0;
				float3 direction = normalize(a0 + a1 * u + a2 * v);
				float cs = dot(n, direction);
				if (cs >= 0.0)
				{
					float solidAngle = texelSolidAngle(u, v, invFaceSize);
					float3 smp = baseColorTexture.SampleLevel(baseColorSampler, direction, sampledLevel).xyz;
					integralResult += cs * solidAngle * smp;
				}
			}
		}	
	}
	float3 result = integralResult * (1.03 / PI);

	return float4(result, 1.0);

#elif (SPECULAR_CONVOLUTION)
	
	uint level0Width = 0;
	uint level0Height = 0;
	baseColorTexture.GetDimensions(level0Width, level0Height);
	float invFaceSize = 1.0 / float(min(level0Width, level0Height));

	const uint samples = 2048;
	float invSamples = 1.0 / float(samples);

	float3 n = normalize(fsIn.direction);
	float3 v = n;
	
	float3 up = abs(n.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
	float3 tX = normalize(cross(up, n));
	float3 tY = cross(n, tX);

	float roughness = clamp(extraParameters.x / 8.0, MIN_ROUGHNESS, 1.0);
	roughness = roughness * roughness;

	float3 result = 0.0;
	float weight = 0.0;
	for (uint i = 0; i < samples; ++i)
	{
		float2 Xi = hammersley(i, samples);
		float3 h = importanceSampleGGX(Xi, roughness);	

		h = tX * h.x + tY * h.y + n * h.z;
		float3 l = 2.0 * dot(v, h) * h - v;
		float LdotN = dot(l, n);
		if (LdotN > 0.0)
		{
			float NdotH = dot(n, h);
			float invPdf = 4.0 * dot(l, h) / (ggxDistribution(NdotH, roughness) * NdotH);
			float sampleSolidAngle = invSamples * invPdf;
			float cubemapSolidAngle = texelSolidAngle(l, invFaceSize);
			float sampledLevel = clamp(2.0 + 0.5 * log2(1.0 + sampleSolidAngle / cubemapSolidAngle), 0.0, 8.0);
			result += LdotN * baseColorTexture.SampleLevel(baseColorSampler, l, sampledLevel).xyz;
			weight += LdotN;
		}
	}
	return float4(result / weight, 1.0);

#else

	return 1.0;

#endif
}
