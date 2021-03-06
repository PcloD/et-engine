/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#pragma once

#include <et/rendering/base/renderbatch.h>
#include <et/scene3d/elementcontainer.h>

namespace et
{
namespace s3d
{
class RenderableElement : public ElementContainer
{
public:
	ET_DECLARE_POINTER(RenderableElement);
	
public:
	RenderableElement(const std::string& name, BaseElement* parent);

	void addRenderBatch(RenderBatch::Pointer);
	
	Vector<RenderBatch::Pointer>& renderBatches();
	const Vector<RenderBatch::Pointer>& renderBatches() const;

	// Helper function - broadcasts material to all batches
	void setMaterial(const Material::Pointer&);

	virtual RayIntersection intersectsWorldSpaceRay(const ray3d&) { return RayIntersection(); }

private:
	Vector<RenderBatch::Pointer> _renderBatches;
};
}
}
