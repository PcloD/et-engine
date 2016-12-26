/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#include <et-ext/rt/bsdf.h>

namespace et
{

namespace rt
{

BSDFSample::BSDFSample(const float4& _wi, const float4& _n, const Material& mat,
	const float4& uv, Direction _d) : Wi(_wi), n(_n), IdotN(_wi.dot(_n)), alpha(mat.roughness), dir(_d)
{
	switch (mat.cls)
	{
	case Material::Class::Diffuse:
	{
		cls = BSDFSample::Class::Diffuse;
		Wo = randomVectorOnHemisphere(n, uniformDistribution);
		color = mat.diffuse;
		break;
	}

	case Material::Class::Conductor:
	{
		cls = BSDFSample::Class::Reflection;
		Wo = computeReflectionVector(Wi, n, alpha);
		fresnel = fresnelShlickApproximation(1.0f, IdotN);
		color = mat.specular;
		break;
	}

	case Material::Class::Dielectric:
	{
		eta = mat.ior;
		if (eta > 1.0f) // refractive
		{
			if (IdotN < 0.0f)
			{
				eta = 1.0f / eta;
			}
			else
			{
				n *= -1.0f;
				IdotN = -IdotN;
			}

			float sinTheta = 1.0f - sqr(eta) * (1.0f - sqr(IdotN));
			fresnel = (sinTheta > 0.0f) ? fresnelShlickApproximation(mat.metallness, IdotN) : 1.0f;
			if (fastRandomFloat() <= fresnel)
			{
				cls = BSDFSample::Class::Reflection;
				Wo = computeReflectionVector(Wi, n, alpha);
				color = mat.specular;
			}
			else
			{
				cls = BSDFSample::Class::Transmittance;
				Wo = computeRefractionVector(Wi, n, eta, alpha, sinTheta, IdotN);
				color = mat.diffuse;
			}
		}
		else // non-refractive material
		{
			fresnel = fresnelShlickApproximation(mat.metallness, IdotN);
			if (fastRandomFloat() <= fresnel)
			{
				cls = BSDFSample::Class::Reflection;
				Wo = computeReflectionVector(Wi, n, alpha);
				color = mat.specular;
			}
			else
			{
				cls = BSDFSample::Class::Diffuse;
				Wo = computeDiffuseVector(Wi, n, alpha);
				color = mat.diffuse;
			}
		}
		break;
	}

	default:
		ET_FAIL("Invalid material class");
	}

	OdotN = Wo.dot(n);
	cosTheta = std::abs((dir == BSDFSample::Direction::Backward ? OdotN : IdotN));
}

BSDFSample::BSDFSample(const float4& _wi, const float4& _wo, const float4& _n,
	const Material& mat, const float4& uv, Direction _d)
	: Wi(_wi)
	, Wo(_wo)
	, n(_n)
	, IdotN(_wi.dot(_n))
	, OdotN(_wo.dot(_n))
	, alpha(mat.roughness)
	, dir(_d)
{
	switch (mat.cls)
	{
	case Material::Class::Diffuse:
	{
		cls = BSDFSample::Class::Diffuse;
		color = mat.diffuse;
		break;
	}

	case Material::Class::Conductor:
	{
		cls = BSDFSample::Class::Reflection;
		fresnel = fresnelShlickApproximation(mat.metallness, IdotN);
		color = mat.specular;
		break;
	}

	case Material::Class::Dielectric:
	{
		eta = mat.ior;
		if (eta > 1.0f) // refractive
		{
			if (IdotN < 0.0f)
				eta = 1.0f / eta;

			float refractionK = 1.0f - sqr(eta) * (1.0f - sqr(IdotN));
			fresnel = (refractionK > 0.0f) ? fresnelShlickApproximation(mat.metallness, IdotN) : 1.0f;
			if (OdotN <= 0.0f)
			{
				cls = BSDFSample::Class::Transmittance;
				color = mat.diffuse;
			}
			else
			{
				cls = BSDFSample::Class::Reflection;
				color = mat.specular;
			}
		}
		else // non-refractive material
		{
			fresnel = fresnelShlickApproximation(mat.metallness, IdotN);
			if (fastRandomFloat() <= fresnel)
			{
				cls = BSDFSample::Class::Reflection;
				color = mat.specular;
			}
			else
			{
				cls = BSDFSample::Class::Diffuse;
				color = mat.diffuse;
			}
		}
		break;
	}

	default:
		ET_FAIL("Invalid material class");
	}

	cosTheta = std::abs((dir == BSDFSample::Direction::Backward ? OdotN : IdotN));
}

inline float G_ggx(float t, float alpha)
{
	float tanThetaSquared = (1.0f - t * t) / (t * t);
	return 2.0f / (1.0f + std::sqrt(1.0f + alpha * alpha * tanThetaSquared));
}

inline float normalDistribution(float alphaSquared, float cosTheta)
{
	float denom = sqr(cosTheta) * (alphaSquared - 1.0f) + 1.0f;
	return ((denom > 0.0f) && (cosTheta > 0.0f)) ? alphaSquared / (PI * denom * denom) : 0.0f;
}

float BSDFSample::bsdf()
{
	if (cls == Class::Diffuse)
		return 1.0f / PI;

	if (cls == Class::Reflection)
	{
		ET_ASSERT(h.dotSelf() > 0.0f);
		float NdotO = n.dot(Wo);
		float NdotI = -n.dot(Wi);
		float HdotO = h.dot(Wo);
		float HdotI = -h.dot(Wi);
		float NdotH = n.dot(h);
		float ndf = normalDistribution(alpha * alpha, NdotH);
		float g1 = G_ggx(NdotI, alpha) * float(HdotI / NdotI > 0.0f);
		float g2 = G_ggx(NdotO, alpha) * float(HdotO / NdotO > 0.0f);
		float g = g1 * g2;
		return (ndf * g * fresnel) / (4.0f * NdotI * NdotO);
	}
	else if (cls == BSDFSample::Class::Transmittance)
	{
		float HdotO = h.dot(Wo);
		float HdotI = h.dot(Wi);
		float g1 = G_ggx(IdotN, alpha) * float(HdotI / IdotN > 0.0f);
		float g2 = G_ggx(OdotN, alpha) * float(HdotO / OdotN > 0.0f);
		float g = g1 * g2;
		float NdotH = n.dot(h);
		float etaSq = eta * eta;
		float ndf = normalDistribution(alpha * alpha, NdotH);
		float denom = sqr(HdotI + eta * HdotO);

		return ((1.0f - fresnel) * ndf * g * etaSq * HdotI * HdotO) / (IdotN * denom + Constants::epsilon);
	}

	ET_FAIL("Invalid material class");
	return 0.0f;
}

float BSDFSample::pdf()
{
	if (cls == Class::Diffuse)
		return 1.0f / DOUBLE_PI;

	if (cls == Class::Reflection)
	{
		h = Wo - Wi;
		h.normalize();
		float cTh = std::min(1.0f, n.dot(h));
		float sTh = std::sqrt(1.0f - cTh * cTh);
		return normalDistribution(alpha * alpha, cTh) * cTh * sTh;
	}
	else if (cls == BSDFSample::Class::Transmittance)
	{
		h = Wi + Wo * eta;
		h.normalize();
		h *= 2.0f * float(h.dot(n) > 0.0f) - 1.0f;

		float HdotO = h.dot(Wo);
		float HdotI = h.dot(Wi);
		float rSq = sqr(alpha);

		float NdotH = n.dot(h);
		float etaSq = eta * eta;
		float ndf = normalDistribution(rSq, NdotH);
		return ndf * etaSq * HdotO / sqr(HdotI + eta * HdotO);
	}

	ET_FAIL("Invalid material class");
	return 0.0f;
}

float4 BSDFSample::evaluate()
{
	float pdfValue = pdf();
	//return float4(pdfValue);

	if (pdfValue == 0.0f)
		return float4(0.0f);

	float bsdfValue = bsdf();
	// return float4(bsdfValue);

	return color * (cosTheta * bsdfValue / pdfValue);
}

float4 BSDFSample::combinedEvaluate()
{
	if (cls == Class::Diffuse)
		return color * (2.0f * cosTheta);

	if (cls == Class::Reflection)
	{
		h = Wo - Wi;
		h.normalize();
		ET_ASSERT(h.dotSelf() > 0.0f);
		float HdotO = h.dot(Wo);
		float NdotH = n.dot(h);
		float HdotIdivIdotN = h.dot(Wi) / IdotN;
		float g1 = G_ggx(-IdotN, alpha) * float(HdotIdivIdotN > 0.0f);
		float g2 = G_ggx(OdotN, alpha) * float(HdotO / OdotN > 0.0f);
		return color * (g1 * g2 * fresnel * HdotIdivIdotN / NdotH);
	}

	if (cls == BSDFSample::Class::Transmittance)
	{
		h = Wi + Wo * eta;
		h.normalize();
		h *= 2.0f * float(h.dot(n) > 0.0f) - 1.0f;
		float HdotO = h.dot(Wo);
		float HdotI = h.dot(Wi);
		float HdotIdivIdotN = HdotI / IdotN;
		float g1 = G_ggx(IdotN, alpha) * float(HdotIdivIdotN > 0.0f);
		float g2 = G_ggx(OdotN, alpha) * float(HdotO / OdotN > 0.0f);
		return color * (cosTheta * (1.0f - fresnel) * g1 * g2 * HdotIdivIdotN * HdotO / HdotO);
	}

	ET_FAIL("Invalid material class");
	return float4(0.0f);
}

}
}
