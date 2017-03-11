/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#pragma once

#include <et/camera/camera.h>
#include <et/rendering/objects/light.h>
#include <et/rendering/base/rendering.h>
#include <et/rendering/base/constantbuffer.h>
#include <et/rendering/base/renderbatch.h>

namespace et
{

struct RenderTarget
{
	Texture::Pointer texture;
	FramebufferOperation loadOperation = FramebufferOperation::Clear;
	FramebufferOperation storeOperation = FramebufferOperation::Store;
	vec4 clearValue = vec4(1.0f);
	bool useDefaultRenderTarget = true;
	bool enabled = false;
};

union RenderSubpass
{
	struct
	{
		uint32_t layer;
		uint32_t level;
	};
	uint64_t hash;

	RenderSubpass() : 
		hash(0) { }

	RenderSubpass(uint32_t aLayer, uint32_t aLevel) : 
		layer(aLayer), level(aLevel) { }

	bool operator < (const RenderSubpass& r) 
		{ return hash < r.hash;  }
};

struct RenderPassBeginInfo
{
	Vector<RenderSubpass> subpasses;

	RenderPassBeginInfo() = default;

	RenderPassBeginInfo(uint32_t l, uint32_t m) : 
		subpasses(1, { l, m }) { }
};

class RenderInterface;
class RenderPass : public Shared
{
public:
	ET_DECLARE_POINTER(RenderPass);

	struct ConstructionInfo
	{
		RenderTarget color[MaxRenderTargets];
		RenderTarget depth;
		Camera::Pointer camera;
		Light::Pointer light;
		float depthBias = 0.0f;
		float depthSlope = 0.0f;
		uint32_t priority = RenderPassPriority::Default;
		std::string name;
	};

	struct Variables
	{
		mat4 viewProjection;
		mat4 projection;
		mat4 view;
		mat4 inverseViewProjection;
		mat4 inverseProjection;
		mat4 inverseView;
		vec4 cameraPosition;
		vec4 cameraDirection;
		vec4 cameraUp;
		vec4 lightPosition;
		mat4 lightProjection;
	};

	static const std::string kPassNameForward;
	static const std::string kPassNameUI;
	static const std::string kPassNameDepth;

public:
	RenderPass(RenderInterface*, const ConstructionInfo&);
	virtual ~RenderPass();

	virtual void begin(const RenderPassBeginInfo& info) = 0;
	virtual void nextSubpass() = 0;
	virtual void pushRenderBatch(const RenderBatch::Pointer&) = 0;
	virtual void end() = 0;

	void executeSingleRenderBatch(const RenderBatch::Pointer&, const RenderPassBeginInfo&);

	const ConstructionInfo& info() const;
	ConstantBuffer& dynamicConstantBuffer();

	void setCamera(const Camera::Pointer& cam);
	void setLightCamera(const Camera::Pointer& cam);

	Camera::Pointer& camera();
	const Camera::Pointer& camera() const;

	void setSharedTexture(MaterialTexture, const Texture::Pointer&, const Sampler::Pointer&);

	uint64_t identifier() const;

protected:
	using SharedTexturesSet = std::map<MaterialTexture, std::pair<Texture::Pointer, Sampler::Pointer>>;
	const SharedTexturesSet& sharedTextures() const { return _sharedTextures; }

private:
	RenderInterface* _renderer = nullptr;
	ConstructionInfo _info;
	ConstantBuffer _dynamicConstantBuffer;
	SharedTexturesSet _sharedTextures;
};

}
