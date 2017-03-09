#include <et>
#include <inputdefines>
#include <inputlayout>

cbuffer MaterialVariables : CONSTANT_LOCATION(b, MaterialVariablesBufferIndex, VariablesSetIndex)
{
	float4 diffuseReflectance;
	float4 specularReflectance;
	float4 emissiveColor;
	float roughnessScale;
	float metallnessScale;
};

cbuffer ObjectVariables : CONSTANT_LOCATION(b, ObjectVariablesBufferIndex, VariablesSetIndex)
{
	row_major float4x4 worldTransform;
	row_major float4x4 worldRotationTransform;
};

Texture2D<float4> baseColorTexture : CONSTANT_LOCATION(t, BaseColorTextureBinding, TexturesSetIndex);
SamplerState baseColorSampler : CONSTANT_LOCATION(s, BaseColorSamplerBinding, TexturesSetIndex);

Texture2D<float4> normalTexture : CONSTANT_LOCATION(t, NormalTextureBinding, TexturesSetIndex);
SamplerState normalSampler : CONSTANT_LOCATION(s, NormalSamplerBinding, TexturesSetIndex);;

Texture2D<float4> shadowTexture : CONSTANT_LOCATION(t, ShadowTextureBinding, TexturesSetIndex);
SamplerState shadowSampler : CONSTANT_LOCATION(s, ShadowSamplerBinding, TexturesSetIndex);;

struct VSOutput 
{
	float4 position : SV_Position;
	float3 toLight : POSITION1;
	float3 toCamera : POSITION2;
	float2 texCoord0 : TEXCOORD0;
	float4 lightCoord : TEXCOORD1;
	float3 invTransformT : TANGENT;
	float3 invTransformB : BINORMAL;
	float3 invTransformN : NORMAL;
};

VSOutput vertexMain(VSInput vsIn)
{
	float4 transformedPosition = mul(float4(vsIn.position, 1.0), worldTransform);

	float3 tNormal = normalize(mul(float4(vsIn.normal, 0.0), worldRotationTransform).xyz);
	float3 tTangent = normalize(mul(float4(vsIn.tangent, 0.0), worldRotationTransform).xyz);
	float3 tBiTangent = cross(tNormal, tTangent);

	VSOutput vsOut;
	vsOut.toCamera = (cameraPosition - transformedPosition).xyz;
	vsOut.toLight = (lightPosition - transformedPosition * lightPosition.w).xyz;
	vsOut.texCoord0 = vsIn.texCoord0;
	vsOut.lightCoord = mul(transformedPosition, lightProjection);
	vsOut.position = mul(transformedPosition, viewProjection);
	vsOut.invTransformT = float3(tTangent.x, tBiTangent.x, tNormal.x);
	vsOut.invTransformB = float3(tTangent.y, tBiTangent.y, tNormal.y);
	vsOut.invTransformN = float3(tTangent.z, tBiTangent.z, tNormal.z);
	return vsOut;
}

#include "srgb.h"
#include "bsdf.h"
#include "environment.h"
#include "importance-sampling.h"

float sampleShadow(float3 tc)
{
	float shadowSample = 0.5 + 0.5 * shadowTexture.Sample(shadowSampler, tc.xy).x;
	return float(tc.z <= shadowSample);
}

float4 fragmentMain(VSOutput fsIn) : SV_Target0
{
	float4 normalSample = normalTexture.Sample(normalSampler, fsIn.texCoord0);
	float4 baseColorSample = baseColorTexture.Sample(baseColorSampler, fsIn.texCoord0);

	float3 baseColor = srgbToLinear(baseColorSample.xyz);
	Surface surface = buildSurface(baseColor, normalSample.w * metallnessScale, baseColorSample.w * roughnessScale);

	float3 tsNormal = normalize(normalSample.xyz - 0.5);

	float3 wsNormal;
	wsNormal.x = dot(fsIn.invTransformT, tsNormal);
	wsNormal.y = dot(fsIn.invTransformB, tsNormal);
	wsNormal.z = dot(fsIn.invTransformN, tsNormal);
	wsNormal = normalize(wsNormal);

	float3 wsLight = normalize(fsIn.toLight);
	float3 wsView = normalize(fsIn.toCamera);
	float3 wsReflected = -reflect(wsView, wsNormal);

	BSDF bsdf = buildBSDF(wsNormal, wsLight, wsView);

	float3 indirectDiffuse = importanceSampledDiffuse(wsNormal, wsView, surface);
	float3 indirectSpecular = importanceSampledSpecular(wsNormal, wsView, surface);

	float3 directDiffuse = computeDirectDiffuse(surface, bsdf);
	float3 directSpecular = computeDirectSpecular(surface, bsdf);
	float3 directTerm = directDiffuse + directSpecular;

	float3 result = directTerm + float3(0.0, 1.0, 0.0) * indirectSpecular; // directTerm + indirectSpecular + indirectDiffuse; 
	
	return float4(result, 1.0);
}