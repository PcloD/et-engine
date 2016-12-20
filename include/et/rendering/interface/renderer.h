/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#pragma once

#include <et/app/context.h>
#include <et/core/objectscache.h>
#include <et/imaging/texturedescription.h>
#include <et/rendering/rendercontextparams.h>
#include <et/rendering/base/materiallibrary.h>
#include <et/rendering/interface/buffer.h>
#include <et/rendering/interface/renderpass.h>
#include <et/rendering/interface/pipelinestate.h>
#include <et/rendering/interface/sampler.h>

namespace et
{
class RenderContext;
class RenderInterface : public Shared
{
public:
	ET_DECLARE_POINTER(RenderInterface);

public:
	RenderInterface(RenderContext* rc)
		: _rc(rc) { }

	virtual ~RenderInterface() = default;

	RenderContext* rc() const
		{ return _rc; }

	MaterialLibrary& sharedMaterialLibrary()
		{ return _sharedMaterialLibrary; }

	ConstantBuffer& sharedConstantBuffer() 
		{ return _sharedConstantBuffer; }

	virtual RenderingAPI api() const = 0;

	virtual void init(const RenderContextParameters& params) = 0;
	virtual void shutdown() = 0;

	virtual void resize(const vec2i&) = 0;

	virtual void begin() = 0;
	virtual void present() = 0;

	virtual RenderPass::Pointer allocateRenderPass(const RenderPass::ConstructionInfo&) = 0;
	virtual void submitRenderPass(RenderPass::Pointer) = 0;

	/*
	 * Buffers
	 */
	virtual Buffer::Pointer createBuffer(const std::string&, const Buffer::Description&) = 0;

	Buffer::Pointer createDataBuffer(const std::string&, uint32_t size);
	Buffer::Pointer createDataBuffer(const std::string&, const BinaryDataStorage&);
	Buffer::Pointer createIndexBuffer(const std::string&, const IndexArray::Pointer&, Buffer::Location);
	Buffer::Pointer createVertexBuffer(const std::string&, const VertexStorage::Pointer&, Buffer::Location);

	/*
	 * Textures
	 */
	virtual Texture::Pointer createTexture(TextureDescription::Pointer) = 0;
	virtual TextureSet::Pointer createTextureSet(const TextureSet::Description&) = 0;

	Texture::Pointer loadTexture(const std::string& fileName, ObjectsCache& cache);
	Texture::Pointer defaultTexture();
	
	/*
	 * Programs
	 */
	virtual Program::Pointer createProgram(const std::string& source) = 0;

	/*
	 * Pipeline state
	 */
	virtual PipelineState::Pointer acquirePipelineState(const RenderPass::Pointer&, const Material::Pointer&, const VertexStream::Pointer&) = 0;

	/*
	 * Sampler
	 */
	virtual Sampler::Pointer createSampler(const Sampler::Description&) = 0;
	Sampler::Pointer defaultSampler();

protected:
	void initInternalStructures();
	void shutdownInternalStructures();

private:
	RenderContext* _rc = nullptr;
	MaterialLibrary _sharedMaterialLibrary;
	ConstantBuffer _sharedConstantBuffer;
	Texture::Pointer _defaultTexture;
	Sampler::Pointer _defaultSampler;
};

inline Texture::Pointer RenderInterface::loadTexture(const std::string& fileName, ObjectsCache& cache)
{
	LoadableObject::Collection existingObjects = cache.findObjects(fileName);
	if (existingObjects.empty())
	{
		TextureDescription::Pointer desc = TextureDescription::Pointer::create();
		if (desc->load(fileName))
		{
			Texture::Pointer texture = createTexture(desc);
			texture->setOrigin(fileName);
			cache.manage(texture, ObjectLoader::Pointer());
			return texture;
		}
	}
	else 
	{
		return existingObjects.front();
	}

	log::error("Unable to load texture from %s", fileName.c_str());
	return defaultTexture();
}

inline Texture::Pointer RenderInterface::defaultTexture()
{
	if (_defaultTexture.invalid())
	{
		const uint32_t colors[] = { 0xFF000000, 0xFFFFFFFF };
		uint32_t numColors = static_cast<uint32_t>(sizeof(colors) / sizeof(colors[0]));
		
		TextureDescription::Pointer desc = TextureDescription::Pointer::create();
		desc->size = vec2i(16);
		desc->format = TextureFormat::RGBA8;
		desc->data.resize(4 * static_cast<uint32_t>(desc->size.square()));
		
		uint32_t* data = reinterpret_cast<uint32_t*>(desc->data.data());
		for (uint32_t p = 0, e = static_cast<uint32_t>(desc->size.square()); p < e; ++p)
		{
			uint32_t ux = static_cast<uint32_t>(desc->size.x);
			uint32_t colorIndex = ((p / ux) + (p % ux)) % numColors;
			data[p] = colors[colorIndex];
		}

		_defaultTexture = createTexture(desc);
	}
	return _defaultTexture;
}

inline Sampler::Pointer RenderInterface::defaultSampler()
{
	if (_defaultSampler.invalid())
	{
		Sampler::Description desc;
		_defaultSampler = createSampler(desc);
	}

	return _defaultSampler;
}

inline void RenderInterface::initInternalStructures()
{
	_sharedConstantBuffer.init(this);
	_sharedMaterialLibrary.init(this);
	defaultTexture();
	defaultSampler();
}

inline void RenderInterface::shutdownInternalStructures()
{
	_sharedMaterialLibrary.shutdown();
	_sharedConstantBuffer.shutdown();
	_defaultSampler.reset(nullptr);
	_defaultTexture.reset(nullptr);
}

inline Buffer::Pointer RenderInterface::createDataBuffer(const std::string& name, uint32_t size)
{
	Buffer::Description desc;
	desc.size = size;
	desc.location = Buffer::Location::Host;
	desc.usage = Buffer::Usage::Constant;
	return createBuffer(name, desc);
}

inline Buffer::Pointer RenderInterface::createDataBuffer(const std::string& name, const BinaryDataStorage& data)
{
	Buffer::Description desc;
	desc.size = data.size();
	desc.location = Buffer::Location::Host;
	desc.usage = Buffer::Usage::Constant;
	desc.initialData = BinaryDataStorage(data.data(), data.size());
	return createBuffer(name, desc);
}

inline Buffer::Pointer RenderInterface::createVertexBuffer(const std::string& name, const VertexStorage::Pointer& vs, Buffer::Location location)
{
	Buffer::Description desc;
	desc.size = vs->data().size();
	desc.location = location;
	desc.usage = Buffer::Usage::Vertex;
	desc.initialData = BinaryDataStorage(vs->data().data(), vs->data().size());
	return createBuffer(name, desc);
}

inline Buffer::Pointer RenderInterface::createIndexBuffer(const std::string& name, const IndexArray::Pointer& ia , Buffer::Location location)
{
	Buffer::Description desc;
	desc.size = ia->dataSize();
	desc.location = location;
	desc.usage = Buffer::Usage::Index;
	desc.initialData = BinaryDataStorage(ia->data(), ia->dataSize());
	return createBuffer(name, desc);
}

}
