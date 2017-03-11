/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#pragma once

#include <et/rendering/interface/renderpass.h>

namespace et
{

class VulkanState;
class VulkanRenderer;
class VulkanNativeRenderPass;
class VulkanRenderPassPrivate;
class VulkanRenderPass : public RenderPass
{
public:
	ET_DECLARE_POINTER(VulkanRenderPass);

public:
	VulkanRenderPass(VulkanRenderer*, VulkanState&, const RenderPass::ConstructionInfo&);
	~VulkanRenderPass();

	const VulkanNativeRenderPass& nativeRenderPass() const;

	void begin(const RenderPassBeginInfo&) override;
	void nextSubpass() override;
	void pushRenderBatch(const RenderBatch::Pointer&) override;
	void end() override;
	
	void recordCommandBuffer();

private:
	ET_DECLARE_PIMPL(VulkanRenderPass, 384);
};
}
