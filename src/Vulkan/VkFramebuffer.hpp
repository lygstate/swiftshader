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

#ifndef VK_FRAMEBUFFER_HPP_
#define VK_FRAMEBUFFER_HPP_

#include "VkObject.hpp"

namespace vk
{

class ImageView;
class RenderPass;

class Framebuffer : public Object<Framebuffer, VkFramebuffer>
{
public:
	Framebuffer(const VkFramebufferCreateInfo* pCreateInfo, void* mem);
	void destroy(const VkAllocationCallbacks* pAllocator);

	void clear(const RenderPass* renderPass, uint32_t clearValueCount, const VkClearValue* pClearValues, const VkRect2D& renderArea);
	void clear(const RenderPass* renderPass, const VkClearAttachment& attachment, const VkClearRect& rect);

	static size_t ComputeRequiredAllocationSize(const VkFramebufferCreateInfo* pCreateInfo);
	ImageView *getAttachment(uint32_t index) const;
	void resolve(const RenderPass* renderPass);

private:
	uint32_t    attachmentCount = 0;
	ImageView** attachments = nullptr;
};

static inline Framebuffer* Cast(VkFramebuffer object)
{
	return reinterpret_cast<Framebuffer*>(object.get());
}

} // namespace vk

#endif // VK_FRAMEBUFFER_HPP_
