/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#pragma once

#include <et/scene3d/drawer/common.h>

namespace et
{
namespace s3d
{

class CubemapProcessor : public Object, public FlagsHolder
{
public:
	ET_DECLARE_POINTER(CubemapProcessor);

public:
	CubemapProcessor();

	const std::string& sourceTextureName() const;
	const Texture::Pointer& convolvedDiffuseCubemap() const;
	const Texture::Pointer& convolvedSpecularCubemap() const;
	const Texture::Pointer& brdfLookupTexture() const;
	const vec4* environmentSphericalHarmonics() const;

	void processAtmosphere();
	void processEquiretangularTexture(const Texture::Pointer&);
	void process(RenderInterface::Pointer&, DrawerOptions&, const Light::Pointer&);

private:
	void validate(RenderInterface::Pointer& renderer);
	void drawDebug(RenderInterface::Pointer&, const DrawerOptions&);

private:
	enum CubemapType : uint32_t
	{
		Source,
		Downsampled,
		Specular,
		Diffuse,
		Count
	};

	enum : uint32_t
	{
		/*
		 * Flags
		 */
		CubemapProcessed = 1 << 0,
		BRDFLookupProcessed = 1 << 1,
		CubemapAtmosphere = 1 << 2,

		/*
		 * Constants
		 */
		CubemapLevels = 8
	};

private:
	Texture::Pointer _lookup;
	Texture::Pointer _tex[CubemapType::Count];
	Sampler::Pointer _eqMapSampler;
	Material::Pointer _wrapMaterial;
	Material::Pointer _atmosphereMaterial;
	Material::Pointer _processingMaterial;
	Material::Pointer _shMaterial;
	Material::Pointer _downsampleMaterial;
	Material::Pointer _lookupGeneratorMaterial;

	Texture::Pointer _shValues;
	Buffer::Pointer _shValuesBuffer;
	Compute::Pointer _shConvolute;

	RenderPass::Pointer _lookupPass;
	RenderPass::Pointer _lookupDebugPass;
	RenderPass::Pointer _downsamplePass;
	RenderPass::Pointer _cubemapDebugPass;
	RenderPass::Pointer _diffuseConvolvePass;
	RenderPass::Pointer _specularConvolvePass;
	RenderBatch::Pointer _lookupDebugBatch;
	RenderBatch::Pointer _cubemapDebugBatch;
	RenderBatch::Pointer _shDebugBatch;
	RenderBatch::Pointer _diffuseConvolveBatch;
	RenderBatch::Pointer _specularConvolveBatch;
	CubemapProjectionMatrixArray _projections;
	RenderPassBeginInfo _oneLevelCubemapBeginInfo;
	RenderPassBeginInfo _wholeCubemapBeginInfo;

	vec4 _environmentSphericalHarmonics[9]{};
	std::string _sourceTextureName;
	int32_t _grabHarmonicsFrame = -1;
};

}
}
