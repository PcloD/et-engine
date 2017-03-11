/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#pragma once

#include <et/rendering/vulkan/vulkan_renderpass.h>
#include <et/rendering/vulkan/vulkan.h>

namespace et
{

struct VulkanRenderBatch
{
	VulkanPipelineState::Pointer pipeline;
	VulkanTextureSet::Pointer textureSet;
	VulkanBuffer::Pointer vertexBuffer;
	VulkanBuffer::Pointer indexBuffer;
	VkIndexType indexBufferFormat = VkIndexType::VK_INDEX_TYPE_MAX_ENUM;
	uint32_t dynamicOffsets[DescriptorSetClass::DynamicDescriptorsCount]{};
	uint32_t startIndex = InvalidIndex;
	uint32_t indexCount = InvalidIndex;
};

struct VulkanRenderPassBeginInfo
{
	VkFramebufferCreateInfo framebufferInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	VkRenderPassBeginInfo beginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
};

class VulkanRenderPassPrivate : public VulkanNativeRenderPass
{
public:
	VulkanRenderPassPrivate(VulkanState& v, VulkanRenderer* r)
		: vulkan(v), renderer(r) { }

	VulkanState& vulkan;
	VulkanRenderer* renderer = nullptr;

	ConstantBufferEntry variablesData;
	Vector<VulkanRenderBatch> batches;
	Vector<VkClearValue> clearValues;
	Map<uint32_t, VulkanRenderPassBeginInfo> framebuffers;
	uint32_t currentBeginInfoIndex = 0;

	std::atomic_bool recording{ false };

	void generateDynamicDescriptorSet(RenderPass* pass);
	void loadVariables(Camera::Pointer camera, Light::Pointer light);
};

VulkanRenderPass::VulkanRenderPass(VulkanRenderer* renderer, VulkanState& vulkan, const RenderPass::ConstructionInfo& passInfo)
	: RenderPass(renderer, passInfo)
{
	ET_PIMPL_INIT(VulkanRenderPass, vulkan, renderer);

	ET_ASSERT(!passInfo.name.empty());

	_private->variablesData = dynamicConstantBuffer().staticAllocate(sizeof(Variables));
	_private->batches.reserve(128);
	_private->generateDynamicDescriptorSet(this);

	VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VULKAN_CALL(vkCreateSemaphore(vulkan.device, &semaphoreInfo, nullptr, &_private->semaphore));

	VkCommandBufferAllocateInfo info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	info.commandPool = vulkan.commandPool;
	info.commandBufferCount = 1;
	info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	VULKAN_CALL(vkAllocateCommandBuffers(vulkan.device, &info, &_private->commandBuffer));

	Vector<VkAttachmentReference> colorAttachmentReferences;
	colorAttachmentReferences.reserve(8);

	Vector<VkAttachmentDescription> attachments;
	attachments.reserve(8);

	for (const RenderTarget& target : passInfo.color)
	{
		if (target.enabled)
		{
			colorAttachmentReferences.emplace_back();
			VkAttachmentReference& ref = colorAttachmentReferences.back();
			ref.attachment = static_cast<uint32_t>(colorAttachmentReferences.size() - 1);
			ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			attachments.emplace_back();
			VkAttachmentDescription& attachment = attachments.back();
			attachment.loadOp = vulkan::frameBufferOperationToLoadOperation(target.loadOperation);
			attachment.storeOp = vulkan::frameBufferOperationToStoreOperation(target.storeOperation);
			attachment.samples = VK_SAMPLE_COUNT_1_BIT;
			attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			if (target.useDefaultRenderTarget)
			{
				attachment.format = vulkan.swapchain.surfaceFormat.format;
				attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			}
			else
			{
				ET_ASSERT(target.texture.valid());
				VulkanTexture::Pointer texture = target.texture;
				attachment.format = texture->nativeTexture().format;
				attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}

			if (attachment.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
			{
				const vec4& cl = target.clearValue;
				_private->clearValues.emplace_back();
				_private->clearValues.back().color = { cl.x, cl.y, cl.z, cl.w };
			}
		}
	}

	VkAttachmentReference depthAttachmentReference = { static_cast<uint32_t>(colorAttachmentReferences.size()) };
	if (passInfo.depth.enabled)
	{
		attachments.emplace_back();
		VkAttachmentDescription& attachment = attachments.back();
		attachment.loadOp = vulkan::frameBufferOperationToLoadOperation(passInfo.depth.loadOperation);
		attachment.storeOp = vulkan::frameBufferOperationToStoreOperation(passInfo.depth.storeOperation);
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		if (passInfo.depth.useDefaultRenderTarget)
		{
			attachment.format = vulkan.swapchain.depthFormat;
		}
		else
		{
			ET_ASSERT(passInfo.depth.texture.valid());
			VulkanTexture::Pointer texture = passInfo.depth.texture;
			attachment.format = texture->nativeTexture().format;
		}
		if (attachment.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
		{
			_private->clearValues.emplace_back();
			_private->clearValues.back().depthStencil = { passInfo.depth.clearValue.x };
		}
		depthAttachmentReference.layout = attachment.finalLayout;
	}

	VkSubpassDescription subpassInfo = {};
	subpassInfo.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentReferences.size());
	subpassInfo.pColorAttachments = colorAttachmentReferences.empty() ? nullptr : colorAttachmentReferences.data();
	subpassInfo.pDepthStencilAttachment = passInfo.depth.enabled ? &depthAttachmentReference : nullptr;

	VkRenderPassCreateInfo createInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	createInfo.pAttachments = attachments.data();
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subpassInfo;

	VULKAN_CALL(vkCreateRenderPass(vulkan.device, &createInfo, nullptr, &_private->renderPass));
}

VulkanRenderPass::~VulkanRenderPass()
{
	for (const auto& fb : _private->framebuffers)
		vkDestroyFramebuffer(_private->vulkan.device, fb.second.beginInfo.framebuffer, nullptr);

	vkDestroyRenderPass(_private->vulkan.device, _private->renderPass, nullptr);
	vkFreeCommandBuffers(_private->vulkan.device, _private->vulkan.commandPool, 1, &_private->commandBuffer);
	
	vkFreeDescriptorSets(_private->vulkan.device, _private->vulkan.descriptorPool, 1, &_private->dynamicDescriptorSet);
	vkDestroyDescriptorSetLayout(_private->vulkan.device, _private->dynamicDescriptorSetLayout, nullptr);
	
	dynamicConstantBuffer().free(_private->variablesData);

	ET_PIMPL_FINALIZE(VulkanRenderPass);
}

const VulkanNativeRenderPass& VulkanRenderPass::nativeRenderPass() const
{
	return *(_private);
}

void VulkanRenderPass::begin(const BeginInfo& beginInfo)
{
	ET_ASSERT(_private->recording == false);

	uint32_t rtWidth = _private->vulkan.swapchain.extent.width;
	uint32_t rtHeight = _private->vulkan.swapchain.extent.height;
	uint32_t rtLayers = 1;
	_private->currentBeginInfoIndex = _private->vulkan.swapchain.currentImageIndex;

	ET_ASSERT(_private->currentBeginInfoIndex != InvalidIndex);

	Texture::Pointer texture0 = info().color[0].texture;
	if (!info().color[0].useDefaultRenderTarget && texture0.valid())
	{
		rtWidth = static_cast<uint32_t>(texture0->size().x);
        rtHeight = static_cast<uint32_t>(texture0->size().y);
		_private->currentBeginInfoIndex = beginInfo.layerIndex;
	}
	else if (!info().depth.useDefaultRenderTarget && info().depth.texture.valid())
	{
		rtWidth = static_cast<uint32_t>(info().depth.texture->size().x);
		rtHeight = static_cast<uint32_t>(info().depth.texture->size().y);
		_private->currentBeginInfoIndex = 0;
	}

	VulkanRenderPassBeginInfo& cbi = _private->framebuffers[_private->currentBeginInfoIndex];
	if ((cbi.framebufferInfo.width != rtWidth) || (cbi.framebufferInfo.height != rtHeight))
	{
		vkDestroyFramebuffer(_private->vulkan.device, cbi.beginInfo.framebuffer, nullptr);
		cbi.beginInfo.framebuffer = nullptr;
	}

	if (cbi.beginInfo.framebuffer == nullptr)
	{
		Vector<VkImageView> attachments;
		attachments.reserve(MaxRenderTargets + 1);

		for (const RenderTarget& rt : info().color)
		{
			if (rt.enabled && rt.useDefaultRenderTarget)
			{
				attachments.emplace_back(_private->vulkan.swapchain.currentRenderTarget().colorView);
			}
			else if (rt.enabled)
			{
				ET_ASSERT(rt.texture.valid());
				VulkanTexture::Pointer texture = rt.texture;
				attachments.emplace_back(texture->nativeTexture().layerImageView(beginInfo.layerIndex));
			}
			else
			{
				break;
			}
		}
		
		if (info().depth.enabled && info().depth.useDefaultRenderTarget)
		{
			attachments.emplace_back(_private->vulkan.swapchain.depthBuffer.depthView);
		}
		else if (info().depth.enabled)
		{
			ET_ASSERT(info().depth.texture.valid());
			VulkanTexture::Pointer texture = info().depth.texture;
			attachments.emplace_back(texture->nativeTexture().completeImageView);
		}

		cbi.framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		cbi.framebufferInfo.pAttachments = attachments.data();
		cbi.framebufferInfo.width = rtWidth;
		cbi.framebufferInfo.height = rtHeight;
		cbi.framebufferInfo.layers = rtLayers;
		cbi.framebufferInfo.renderPass = _private->renderPass;

		VULKAN_CALL(vkCreateFramebuffer(_private->vulkan.device, &cbi.framebufferInfo, nullptr, &cbi.beginInfo.framebuffer));
	}

	cbi.beginInfo.clearValueCount = static_cast<uint32_t>(_private->clearValues.size());
	cbi.beginInfo.pClearValues = _private->clearValues.data();
	cbi.beginInfo.renderPass = _private->renderPass;
	cbi.beginInfo.renderArea.extent.width = rtWidth;
	cbi.beginInfo.renderArea.extent.height = rtHeight;

	_private->scissor.extent = cbi.beginInfo.renderArea.extent;
	_private->viewport.width = static_cast<float>(rtWidth);
	_private->viewport.height = static_cast<float>(rtHeight);
	_private->viewport.maxDepth = 1.0f;
	_private->loadVariables(info().camera, info().light);

	_private->recording = true;
}

void VulkanRenderPass::pushRenderBatch(const RenderBatch::Pointer& inBatch)
{
	ET_ASSERT(_private->recording);

	retain();
	MaterialInstance::Pointer material = inBatch->material();
	VulkanProgram::Pointer program = inBatch->material()->configuration(info().name).program;
	VulkanPipelineState::Pointer pipelineState = _private->renderer->acquirePipelineState(VulkanRenderPass::Pointer(this), material, inBatch->vertexStream());
	release();

	if (pipelineState->nativePipeline().pipeline == nullptr)
		return;

	ConstantBufferEntry objectVariables;
	if (program->reflection().objectVariablesBufferSize > 0)
	{
		objectVariables = dynamicConstantBuffer().dynamicAllocate(program->reflection().objectVariablesBufferSize);
		auto var = program->reflection().objectVariables.find(PipelineState::kWorldTransform());
		if (var != program->reflection().objectVariables.end())
		{
			memcpy(objectVariables.data() + var->second.offset, inBatch->transformation().data(), sizeof(inBatch->transformation()));
		}
		var = program->reflection().objectVariables.find(PipelineState::kWorldRotationTransform());
		if (var != program->reflection().objectVariables.end())
		{
			memcpy(objectVariables.data() + var->second.offset, inBatch->rotationTransformation().data(), sizeof(inBatch->rotationTransformation()));
		}
	}
	
	for (const auto& sh : sharedTextures())
	{
		material->setTexture(sh.first, sh.second.first);
		material->setSampler(sh.first, sh.second.second);
	}

	_private->batches.emplace_back();

	VulkanRenderBatch& batch = _private->batches.back();
	batch.textureSet = material->textureSet(info().name);
	batch.dynamicOffsets[0] = objectVariables.offset();
	batch.dynamicOffsets[1] = material->constantBufferData(info().name).offset();
	batch.vertexBuffer = inBatch->vertexStream()->vertexBuffer();
	batch.indexBuffer = inBatch->vertexStream()->indexBuffer();
	batch.indexBufferFormat = vulkan::indexBufferFormat(inBatch->vertexStream()->indexArrayFormat());
	batch.startIndex = inBatch->firstIndex();
	batch.indexCount = inBatch->numIndexes();
	batch.pipeline = pipelineState;
}

void VulkanRenderPass::end()
{
	_private->recording = false;
}

void VulkanRenderPass::recordCommandBuffer()
{
	ET_ASSERT(_private->recording == false);

	_private->renderer->sharedConstantBuffer().flush();
	dynamicConstantBuffer().flush();

	VkCommandBuffer commandBuffer = _private->commandBuffer;

	VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VULKAN_CALL(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

	vkCmdSetScissor(commandBuffer, 0, 1, &_private->scissor);
	vkCmdSetViewport(commandBuffer, 0, 1, &_private->viewport);

	VkPipeline lastPipeline = nullptr;
	VkBuffer lastVertexBuffer = nullptr;
	VkBuffer lastIndexBuffer = nullptr;
	VkIndexType lastIndexType = VkIndexType::VK_INDEX_TYPE_MAX_ENUM;

	VkDescriptorSet descriptorSets[DescriptorSetClass::Count] = {
		_private->dynamicDescriptorSet,
		nullptr,
	};

	VulkanRenderPassBeginInfo& bi = _private->framebuffers.at(_private->currentBeginInfoIndex);
	vkCmdBeginRenderPass(commandBuffer, &bi.beginInfo, VK_SUBPASS_CONTENTS_INLINE);
	for (const auto& batch : _private->batches)
	{
		if (batch.pipeline->nativePipeline().pipeline != lastPipeline)
		{
			lastPipeline = batch.pipeline->nativePipeline().pipeline;
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lastPipeline);
		}

		if (batch.vertexBuffer->nativeBuffer().buffer != lastVertexBuffer)
		{
			VkDeviceSize nullOffset = 0;
			lastVertexBuffer = batch.vertexBuffer->nativeBuffer().buffer;
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &lastVertexBuffer, &nullOffset);
		}

		if ((batch.indexBuffer->nativeBuffer().buffer != lastIndexBuffer) || (batch.indexBufferFormat != lastIndexType))
		{
			lastIndexBuffer = batch.indexBuffer->nativeBuffer().buffer;
			lastIndexType = batch.indexBufferFormat;
			vkCmdBindIndexBuffer(commandBuffer, lastIndexBuffer, 0, lastIndexType);
		}

		descriptorSets[DescriptorSetClass::Textures] = batch.textureSet->nativeSet().descriptorSet;

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, batch.pipeline->nativePipeline().layout, 0,
			DescriptorSetClass::Count, descriptorSets, DescriptorSetClass::DynamicDescriptorsCount, batch.dynamicOffsets);

		vkCmdDrawIndexed(commandBuffer, batch.indexCount, 1, batch.startIndex, 0, 0);
	}
	vkCmdEndRenderPass(commandBuffer);
	VULKAN_CALL(vkEndCommandBuffer(commandBuffer));

	_private->batches.clear();
}

void VulkanRenderPassPrivate::generateDynamicDescriptorSet(RenderPass* pass)
{
	VulkanBuffer::Pointer db = pass->dynamicConstantBuffer().buffer();
	VulkanBuffer::Pointer sb = renderer->sharedConstantBuffer().buffer();
	VkDescriptorBufferInfo passBufferInfo = { db->nativeBuffer().buffer, variablesData.offset(), sizeof(RenderPass::Variables) };
	VkDescriptorBufferInfo objectBufferInfo = { db->nativeBuffer().buffer, 0, VK_WHOLE_SIZE }; // TODO : calculate offset and size
	VkDescriptorBufferInfo materialBufferInfo = { sb->nativeBuffer().buffer, 0, VK_WHOLE_SIZE }; // TODO : calculate offset and size
	VkDescriptorSetLayoutBinding bindings[] = { { }, {  }, {  } };
	VkWriteDescriptorSet writeSets[] = { { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET }, { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET }, { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET } };
	{
		bindings[0] = { ObjectVariablesBufferIndex, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1 };
		bindings[0].stageFlags = VK_SHADER_STAGE_ALL;
		writeSets[0].descriptorCount = bindings[0].descriptorCount;
		writeSets[0].descriptorType = bindings[0].descriptorType;
		writeSets[0].dstBinding = bindings[0].binding;
		writeSets[0].pBufferInfo = &objectBufferInfo;

		bindings[1] = { MaterialVariablesBufferIndex, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1 };
		bindings[1].stageFlags = VK_SHADER_STAGE_ALL;
		writeSets[1].descriptorCount = bindings[1].descriptorCount;
		writeSets[1].descriptorType = bindings[1].descriptorType;
		writeSets[1].dstBinding = bindings[1].binding;
		writeSets[1].pBufferInfo = &materialBufferInfo;

		bindings[2] = { PassVariablesBufferIndex, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 };
		bindings[2].stageFlags = VK_SHADER_STAGE_ALL;
		writeSets[2].descriptorCount = bindings[2].descriptorCount;
		writeSets[2].descriptorType = bindings[2].descriptorType;
		writeSets[2].dstBinding = bindings[2].binding;
		writeSets[2].pBufferInfo = &passBufferInfo;
	}

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	descriptorSetLayoutCreateInfo.bindingCount = sizeof(bindings) / sizeof(bindings[0]);
	descriptorSetLayoutCreateInfo.pBindings = bindings;
	VULKAN_CALL(vkCreateDescriptorSetLayout(vulkan.device, &descriptorSetLayoutCreateInfo, nullptr, &dynamicDescriptorSetLayout));

	VkDescriptorSetAllocateInfo descriptorAllocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	descriptorAllocInfo.pSetLayouts = &dynamicDescriptorSetLayout;
	descriptorAllocInfo.descriptorPool = vulkan.descriptorPool;
	descriptorAllocInfo.descriptorSetCount = 1;
	VULKAN_CALL(vkAllocateDescriptorSets(vulkan.device, &descriptorAllocInfo, &dynamicDescriptorSet));

	for (VkWriteDescriptorSet& wd : writeSets)
		wd.dstSet = dynamicDescriptorSet;

	uint32_t writeSetsCount = static_cast<uint32_t>(sizeof(writeSets) / sizeof(writeSets[0]));
	vkUpdateDescriptorSets(vulkan.device, writeSetsCount, writeSets, 0, nullptr);
}

void VulkanRenderPassPrivate::loadVariables(Camera::Pointer camera, Light::Pointer light)
{
	RenderPass::Variables* vptr = reinterpret_cast<RenderPass::Variables*>(variablesData.data());
	if (camera.valid())
	{
		vptr->viewProjection = camera->viewProjectionMatrix();
		vptr->projection = camera->projectionMatrix();
		vptr->view = camera->viewMatrix();
		vptr->inverseViewProjection = camera->inverseViewProjectionMatrix();
		vptr->inverseProjection = camera->inverseProjectionMatrix();
		vptr->inverseView = camera->inverseViewMatrix();
		vptr->cameraPosition = vec4(camera->position());
		vptr->cameraDirection = vec4(camera->direction());
		vptr->cameraUp = vec4(camera->up());
	}

	if (light.valid())
	{
		vptr->lightPosition = light->type() == Light::Type::Directional ? vec4(-light->direction(), 0.0f) : vec4(light->position());
		vptr->lightProjection = light->viewProjectionMatrix() * lightProjectionMatrix;
	}
}

}
