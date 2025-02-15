// Copyright 2018 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "VkRenderPass.hpp"
#include <cstring>

namespace
{
	void MarkFirstUse(int& attachment, int subpass)
	{
		if (attachment == -1)
			attachment = subpass;
	}
}

namespace vk
{

RenderPass::RenderPass(const VkRenderPassCreateInfo* pCreateInfo, void* mem) :
	attachmentCount(pCreateInfo->attachmentCount),
	subpassCount(pCreateInfo->subpassCount),
	dependencyCount(pCreateInfo->dependencyCount)
{
	char* hostMemory = reinterpret_cast<char*>(mem);

	// subpassCount must be greater than 0
	ASSERT(pCreateInfo->subpassCount > 0);

	size_t subpassesSize = pCreateInfo->subpassCount * sizeof(VkSubpassDescription);
	subpasses = reinterpret_cast<VkSubpassDescription*>(hostMemory);
	memcpy(subpasses, pCreateInfo->pSubpasses, subpassesSize);
	hostMemory += subpassesSize;

	if(pCreateInfo->attachmentCount > 0)
	{
		size_t attachmentSize = pCreateInfo->attachmentCount * sizeof(VkAttachmentDescription);
		attachments = reinterpret_cast<VkAttachmentDescription*>(hostMemory);
		memcpy(attachments, pCreateInfo->pAttachments, attachmentSize);
		hostMemory += attachmentSize;

		size_t firstUseSize = pCreateInfo->attachmentCount * sizeof(int);
		attachmentFirstUse = reinterpret_cast<int*>(hostMemory);
		for (auto i = 0u; i < pCreateInfo->attachmentCount; i++)
			attachmentFirstUse[i] = -1;
		hostMemory += firstUseSize;
	}

	// Deep copy subpasses
	for(uint32_t i = 0; i < pCreateInfo->subpassCount; ++i)
	{
		const auto& subpass = pCreateInfo->pSubpasses[i];
		subpasses[i].pInputAttachments = nullptr;
		subpasses[i].pColorAttachments = nullptr;
		subpasses[i].pResolveAttachments = nullptr;
		subpasses[i].pDepthStencilAttachment = nullptr;
		subpasses[i].pPreserveAttachments = nullptr;

		if(subpass.inputAttachmentCount > 0)
		{
			size_t inputAttachmentsSize = subpass.inputAttachmentCount * sizeof(VkAttachmentReference);
			subpasses[i].pInputAttachments = reinterpret_cast<VkAttachmentReference*>(hostMemory);
			memcpy(const_cast<VkAttachmentReference*>(subpasses[i].pInputAttachments),
			       pCreateInfo->pSubpasses[i].pInputAttachments, inputAttachmentsSize);
			hostMemory += inputAttachmentsSize;

			for (auto j = 0u; j < subpasses[i].inputAttachmentCount; j++)
			{
				if (subpass.pInputAttachments[j].attachment != VK_ATTACHMENT_UNUSED)
					MarkFirstUse(attachmentFirstUse[subpass.pInputAttachments[j].attachment], i);
			}
		}

		if(subpass.colorAttachmentCount > 0)
		{
			size_t colorAttachmentsSize = subpass.colorAttachmentCount * sizeof(VkAttachmentReference);
			subpasses[i].pColorAttachments = reinterpret_cast<VkAttachmentReference*>(hostMemory);
			memcpy(const_cast<VkAttachmentReference*>(subpasses[i].pColorAttachments),
			       subpass.pColorAttachments, colorAttachmentsSize);
			hostMemory += colorAttachmentsSize;

			if(subpass.pResolveAttachments != nullptr)
			{
				subpasses[i].pResolveAttachments = reinterpret_cast<VkAttachmentReference*>(hostMemory);
				memcpy(const_cast<VkAttachmentReference*>(subpasses[i].pResolveAttachments),
				       subpass.pResolveAttachments, colorAttachmentsSize);
				hostMemory += colorAttachmentsSize;
			}

			for (auto j = 0u; j < subpasses[i].colorAttachmentCount; j++)
			{
				if (subpass.pColorAttachments[j].attachment != VK_ATTACHMENT_UNUSED)
					MarkFirstUse(attachmentFirstUse[subpass.pColorAttachments[j].attachment], i);
				if (subpass.pResolveAttachments &&
					subpass.pResolveAttachments[j].attachment != VK_ATTACHMENT_UNUSED)
					MarkFirstUse(attachmentFirstUse[subpass.pResolveAttachments[j].attachment], i);
			}
		}

		if(subpass.pDepthStencilAttachment != nullptr)
		{
			subpasses[i].pDepthStencilAttachment = reinterpret_cast<VkAttachmentReference*>(hostMemory);
			memcpy(const_cast<VkAttachmentReference*>(subpasses[i].pDepthStencilAttachment),
				subpass.pDepthStencilAttachment, sizeof(VkAttachmentReference));
			hostMemory += sizeof(VkAttachmentReference);

			if (subpass.pDepthStencilAttachment->attachment != VK_ATTACHMENT_UNUSED)
				MarkFirstUse(attachmentFirstUse[subpass.pDepthStencilAttachment->attachment], i);
		}

		if(subpass.preserveAttachmentCount > 0)
		{
			size_t preserveAttachmentSize = subpass.preserveAttachmentCount * sizeof(uint32_t);
			subpasses[i].pPreserveAttachments = reinterpret_cast<uint32_t*>(hostMemory);
			memcpy(const_cast<uint32_t*>(subpasses[i].pPreserveAttachments),
			       pCreateInfo->pSubpasses[i].pPreserveAttachments, preserveAttachmentSize);
			hostMemory += preserveAttachmentSize;

			for (auto j = 0u; j < subpasses[i].preserveAttachmentCount; j++)
			{
				if (subpass.pPreserveAttachments[j] != VK_ATTACHMENT_UNUSED)
					MarkFirstUse(attachmentFirstUse[subpass.pPreserveAttachments[j]], i);
			}
		}
	}

	if(pCreateInfo->dependencyCount > 0)
	{
		size_t dependenciesSize = pCreateInfo->dependencyCount * sizeof(VkSubpassDependency);
		dependencies = reinterpret_cast<VkSubpassDependency*>(hostMemory);
		memcpy(dependencies, pCreateInfo->pDependencies, dependenciesSize);
	}
}

void RenderPass::destroy(const VkAllocationCallbacks* pAllocator)
{
	vk::deallocate(subpasses, pAllocator); // attachments and dependencies are in the same allocation
}

size_t RenderPass::ComputeRequiredAllocationSize(const VkRenderPassCreateInfo* pCreateInfo)
{
	size_t attachmentSize = pCreateInfo->attachmentCount * sizeof(VkAttachmentDescription)
			+ pCreateInfo->attachmentCount * sizeof(int);
	size_t subpassesSize = 0;
	for(uint32_t i = 0; i < pCreateInfo->subpassCount; ++i)
	{
		const auto& subpass = pCreateInfo->pSubpasses[i];
		uint32_t nbAttachments = subpass.inputAttachmentCount + subpass.colorAttachmentCount;
		if(subpass.pResolveAttachments != nullptr)
		{
			nbAttachments += subpass.colorAttachmentCount;
		}
		if(subpass.pDepthStencilAttachment != nullptr)
		{
			nbAttachments += 1;
		}
		subpassesSize += sizeof(VkSubpassDescription) +
		                 sizeof(VkAttachmentReference) * nbAttachments +
		                 sizeof(uint32_t) * subpass.preserveAttachmentCount;
	}
	size_t dependenciesSize = pCreateInfo->dependencyCount * sizeof(VkSubpassDependency);

	return attachmentSize + subpassesSize + dependenciesSize;
}

void RenderPass::getRenderAreaGranularity(VkExtent2D* pGranularity) const
{
	pGranularity->width = 1;
	pGranularity->height = 1;
}

void RenderPass::begin()
{
	currentSubpass = 0;
}

void RenderPass::nextSubpass()
{
	++currentSubpass;
	ASSERT(currentSubpass < subpassCount);
}

void RenderPass::end()
{
	currentSubpass = 0;
}

} // namespace vk