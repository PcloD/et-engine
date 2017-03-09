/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#pragma once

#include <et/imaging/texturedescription.h>
#include <et-ext/rt/raytraceobjects.h>
#include <et-ext/rt/image.h>

namespace et
{
namespace rt
{

struct ET_ALIGNED(16) EmitterInteraction
{
	float4 direction;
	float4 normal;
	float4 color = float4(0.0f);
	float cosTheta = 0.0f;
	float pdf = 0.0f;
};

class Scene;
class Emitter : public Shared
{
public:
	ET_DECLARE_POINTER(Emitter);
	using Collection = Vector<Pointer>;

public:
	virtual ~Emitter() = default;
	virtual EmitterInteraction sample(const Scene&, const float4& position, const float4& normal) const = 0;
	virtual void prepare(const Scene&) { }

	virtual index materialIndex() const { return InvalidIndex; }
};

class UniformEmitter : public Emitter
{
public:
	ET_DECLARE_POINTER(UniformEmitter);

public:
	UniformEmitter(const float4& color);
	EmitterInteraction sample(const Scene&, const float4& position, const float4& normal) const override;

private:
	float4 _color;
};

class EnvironmentEmitter : public Emitter
{
public:
	ET_DECLARE_POINTER(EnvironmentEmitter);

public:
	EnvironmentEmitter(const Image::Pointer&);
	EmitterInteraction sample(const Scene&, const float4& position, const float4& normal) const override;

private:
	Image::Pointer _image;
};

class MeshEmitter : public Emitter
{
public:
	ET_DECLARE_POINTER(MeshEmitter);

public:
	MeshEmitter(index firstTriangle, index numTriangles, index materialIndex);
	void prepare(const Scene&) override;
	EmitterInteraction sample(const Scene&, const float4& position, const float4& normal) const override;

	index materialIndex() const override { return _materialIndex; }

	float4 samplePoint(const Scene&);
	
	float4 evaluate(const Scene&, const float4& position, const float4& direction, float4& nrm, float4& pos, float& pdf);
	float pdf(const float4& position, const float4& direction, const float4& lightPosition, const float4& lightNormal);

private:
	index _firstTriangle = InvalidIndex;
	index _numTriangles = 0;
	index _materialIndex = InvalidIndex;
	float _area = 0.0f;
};

}
}