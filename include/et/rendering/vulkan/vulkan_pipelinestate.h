/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#pragma once

#include <et/rendering/interface/pipelinestate.h>

namespace et
{
	class VulkanState;
	class VulkanNativePipeline;
	class VulkanPipelineStatePrivate;
	class VulkanPipelineState : public PipelineState
	{
	public:
		ET_DECLARE_POINTER(VulkanPipelineState);

	public:
		VulkanPipelineState(VulkanState&);
		~VulkanPipelineState();

		const VulkanNativePipeline& nativePipeline() const;

		void build() override;

	private:
		ET_DECLARE_PIMPL(VulkanPipelineState, 32);
	};
}