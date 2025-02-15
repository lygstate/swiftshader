// Copyright 2019 The SwiftShader Authors. All Rights Reserved.
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

#include "ComputeProgram.hpp"
#include "Constants.hpp"

#include "Vulkan/VkDebug.hpp"
#include "Vulkan/VkPipelineLayout.hpp"

#include <queue>

namespace
{
	enum { X, Y, Z };
} // anonymous namespace

namespace sw
{
	ComputeProgram::ComputeProgram(SpirvShader const *shader, vk::PipelineLayout const *pipelineLayout, const vk::DescriptorSet::Bindings &descriptorSets)
		: data(Arg<0>()),
		  shader(shader),
		  pipelineLayout(pipelineLayout),
		  descriptorSets(descriptorSets)
	{
	}

	ComputeProgram::~ComputeProgram()
	{
	}

	void ComputeProgram::generate()
	{
		SpirvRoutine routine(pipelineLayout);
		shader->emitProlog(&routine);
		emit(&routine);
		shader->emitEpilog(&routine);
	}

	void ComputeProgram::setWorkgroupBuiltins(SpirvRoutine* routine, Int workgroupID[3])
	{
		setInputBuiltin(routine, spv::BuiltInNumWorkgroups, [&](const SpirvShader::BuiltinMapping& builtin, Array<SIMD::Float>& value)
		{
			auto numWorkgroups = *Pointer<Int4>(data + OFFSET(Data, numWorkgroups));
			for (uint32_t component = 0; component < builtin.SizeInComponents; component++)
			{
				value[builtin.FirstComponent + component] =
					As<SIMD::Float>(SIMD::Int(Extract(numWorkgroups, component)));
			}
		});

		setInputBuiltin(routine, spv::BuiltInWorkgroupId, [&](const SpirvShader::BuiltinMapping& builtin, Array<SIMD::Float>& value)
		{
			for (uint32_t component = 0; component < builtin.SizeInComponents; component++)
			{
				value[builtin.FirstComponent + component] =
					As<SIMD::Float>(SIMD::Int(workgroupID[component]));
			}
		});

		setInputBuiltin(routine, spv::BuiltInWorkgroupSize, [&](const SpirvShader::BuiltinMapping& builtin, Array<SIMD::Float>& value)
		{
			auto workgroupSize = *Pointer<Int4>(data + OFFSET(Data, workgroupSize));
			for (uint32_t component = 0; component < builtin.SizeInComponents; component++)
			{
				value[builtin.FirstComponent + component] =
					As<SIMD::Float>(SIMD::Int(Extract(workgroupSize, component)));
			}
		});

		setInputBuiltin(routine, spv::BuiltInNumSubgroups, [&](const SpirvShader::BuiltinMapping& builtin, Array<SIMD::Float>& value)
		{
			ASSERT(builtin.SizeInComponents == 1);
			auto subgroupsPerWorkgroup = *Pointer<Int>(data + OFFSET(Data, subgroupsPerWorkgroup));
			value[builtin.FirstComponent] = As<SIMD::Float>(SIMD::Int(subgroupsPerWorkgroup));
		});

		setInputBuiltin(routine, spv::BuiltInSubgroupSize, [&](const SpirvShader::BuiltinMapping& builtin, Array<SIMD::Float>& value)
		{
			ASSERT(builtin.SizeInComponents == 1);
			auto invocationsPerSubgroup = *Pointer<Int>(data + OFFSET(Data, invocationsPerSubgroup));
			value[builtin.FirstComponent] = As<SIMD::Float>(SIMD::Int(invocationsPerSubgroup));
		});

		setInputBuiltin(routine, spv::BuiltInSubgroupLocalInvocationId, [&](const SpirvShader::BuiltinMapping& builtin, Array<SIMD::Float>& value)
		{
			ASSERT(builtin.SizeInComponents == 1);
			value[builtin.FirstComponent] = As<SIMD::Float>(SIMD::Int(0, 1, 2, 3));
		});

		setInputBuiltin(routine, spv::BuiltInDeviceIndex, [&](const SpirvShader::BuiltinMapping& builtin, Array<SIMD::Float>& value)
		{
			ASSERT(builtin.SizeInComponents == 1);
			// Only a single physical device is supported.
			value[builtin.FirstComponent] = As<SIMD::Float>(SIMD::Int(0, 0, 0, 0));
		});
	}

	void ComputeProgram::setSubgroupBuiltins(SpirvRoutine* routine, Int workgroupID[3], SIMD::Int localInvocationIndex, Int subgroupIndex)
	{
		Int4 numWorkgroups = *Pointer<Int4>(data + OFFSET(Data, numWorkgroups));
		Int4 workgroupSize = *Pointer<Int4>(data + OFFSET(Data, workgroupSize));

		// TODO: Fix Int4 swizzles so we can just use workgroupSize.x, workgroupSize.y.
		Int workgroupSizeX = Extract(workgroupSize, X);
		Int workgroupSizeY = Extract(workgroupSize, Y);

		SIMD::Int localInvocationID[3];
		{
			SIMD::Int idx = localInvocationIndex;
			localInvocationID[Z] = idx / SIMD::Int(workgroupSizeX * workgroupSizeY);
			idx -= localInvocationID[Z] * SIMD::Int(workgroupSizeX * workgroupSizeY); // modulo
			localInvocationID[Y] = idx / SIMD::Int(workgroupSizeX);
			idx -= localInvocationID[Y] * SIMD::Int(workgroupSizeX); // modulo
			localInvocationID[X] = idx;
		}

		setInputBuiltin(routine, spv::BuiltInLocalInvocationIndex, [&](const SpirvShader::BuiltinMapping& builtin, Array<SIMD::Float>& value)
		{
			ASSERT(builtin.SizeInComponents == 1);
			value[builtin.FirstComponent] = As<SIMD::Float>(localInvocationIndex);
		});

		setInputBuiltin(routine, spv::BuiltInSubgroupId, [&](const SpirvShader::BuiltinMapping& builtin, Array<SIMD::Float>& value)
		{
			ASSERT(builtin.SizeInComponents == 1);
			value[builtin.FirstComponent] = As<SIMD::Float>(SIMD::Int(subgroupIndex));
		});

		setInputBuiltin(routine, spv::BuiltInLocalInvocationId, [&](const SpirvShader::BuiltinMapping& builtin, Array<SIMD::Float>& value)
		{
			for (uint32_t component = 0; component < builtin.SizeInComponents; component++)
			{
				value[builtin.FirstComponent + component] =
					As<SIMD::Float>(localInvocationID[component]);
			}
		});

		setInputBuiltin(routine, spv::BuiltInGlobalInvocationId, [&](const SpirvShader::BuiltinMapping& builtin, Array<SIMD::Float>& value)
		{
			SIMD::Int wgID = 0;
			wgID = Insert(wgID, workgroupID[X], X);
			wgID = Insert(wgID, workgroupID[Y], Y);
			wgID = Insert(wgID, workgroupID[Z], Z);
			auto localBase = workgroupSize * wgID;
			for (uint32_t component = 0; component < builtin.SizeInComponents; component++)
			{
				auto globalInvocationID = SIMD::Int(Extract(localBase, component)) + localInvocationID[component];
				value[builtin.FirstComponent + component] = As<SIMD::Float>(globalInvocationID);
			}
		});
	}

	void ComputeProgram::emit(SpirvRoutine* routine)
	{
		Int workgroupX = Arg<1>();
		Int workgroupY = Arg<2>();
		Int workgroupZ = Arg<3>();
		Pointer<Byte> workgroupMemory = Arg<4>();
		Int firstSubgroup = Arg<5>();
		Int subgroupCount = Arg<6>();

		routine->descriptorSets = data + OFFSET(Data, descriptorSets);
		routine->descriptorDynamicOffsets = data + OFFSET(Data, descriptorDynamicOffsets);
		routine->pushConstants = data + OFFSET(Data, pushConstants);
		routine->constants = *Pointer<Pointer<Byte>>(data + OFFSET(Data, constants));
		routine->workgroupMemory = workgroupMemory;

		Int invocationsPerWorkgroup = *Pointer<Int>(data + OFFSET(Data, invocationsPerWorkgroup));

		Int workgroupID[3] = {workgroupX, workgroupY, workgroupZ};
		setWorkgroupBuiltins(routine, workgroupID);

		For(Int i = 0, i < subgroupCount, i++)
		{
			auto subgroupIndex = firstSubgroup + i;

			// TODO: Replace SIMD::Int(0, 1, 2, 3) with SIMD-width equivalent
			auto localInvocationIndex = SIMD::Int(subgroupIndex * SIMD::Width) + SIMD::Int(0, 1, 2, 3);

			// Disable lanes where (invocationIDs >= invocationsPerWorkgroup)
			auto activeLaneMask = CmpLT(localInvocationIndex, SIMD::Int(invocationsPerWorkgroup));

			setSubgroupBuiltins(routine, workgroupID, localInvocationIndex, subgroupIndex);

			shader->emit(routine, activeLaneMask, descriptorSets);
		}
	}

	void ComputeProgram::setInputBuiltin(SpirvRoutine* routine, spv::BuiltIn id, std::function<void(const SpirvShader::BuiltinMapping& builtin, Array<SIMD::Float>& value)> cb)
	{
		auto it = shader->inputBuiltins.find(id);
		if (it != shader->inputBuiltins.end())
		{
			const auto& builtin = it->second;
			cb(builtin, routine->getVariable(builtin.Id));
		}
	}

	void ComputeProgram::run(
		vk::DescriptorSet::Bindings const &descriptorSets,
		vk::DescriptorSet::DynamicOffsets const &descriptorDynamicOffsets,
		PushConstantStorage const &pushConstants,
		uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ,
		uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
	{
		auto &modes = shader->getModes();

		auto invocationsPerSubgroup = SIMD::Width;
		auto invocationsPerWorkgroup = modes.WorkgroupSizeX * modes.WorkgroupSizeY * modes.WorkgroupSizeZ;
		auto subgroupsPerWorkgroup = (invocationsPerWorkgroup + invocationsPerSubgroup - 1) / invocationsPerSubgroup;

		// We're sharing a buffer here across all workgroups.
		// We can only do this because we know a single workgroup is in flight
		// at any time.
		std::vector<uint8_t> workgroupMemory(shader->workgroupMemory.size());

		Data data;
		data.descriptorSets = descriptorSets;
		data.descriptorDynamicOffsets = descriptorDynamicOffsets;
		data.numWorkgroups[X] = groupCountX;
		data.numWorkgroups[Y] = groupCountY;
		data.numWorkgroups[Z] = groupCountZ;
		data.numWorkgroups[3] = 0;
		data.workgroupSize[X] = modes.WorkgroupSizeX;
		data.workgroupSize[Y] = modes.WorkgroupSizeY;
		data.workgroupSize[Z] = modes.WorkgroupSizeZ;
		data.workgroupSize[3] = 0;
		data.invocationsPerSubgroup = invocationsPerSubgroup;
		data.invocationsPerWorkgroup = invocationsPerWorkgroup;
		data.subgroupsPerWorkgroup = subgroupsPerWorkgroup;
		data.pushConstants = pushConstants;
		data.constants = &sw::constants;

		for (uint32_t groupZ = baseGroupZ; groupZ < baseGroupZ + groupCountZ; groupZ++)
		{
			for (uint32_t groupY = baseGroupY; groupY < baseGroupY + groupCountY; groupY++)
			{
				for (uint32_t groupX = baseGroupX; groupX < baseGroupX + groupCountX; groupX++)
				{

					// TODO(bclayton): Split work across threads.
					using Coroutine = std::unique_ptr<rr::Stream<SpirvShader::YieldResult>>;
					std::queue<Coroutine> coroutines;

					if (shader->getModes().ContainsControlBarriers)
					{
						// Make a function call per subgroup so each subgroup
						// can yield, bringing all subgroups to the barrier
						// together.
						for(int subgroupIndex = 0; subgroupIndex < subgroupsPerWorkgroup; subgroupIndex++)
						{
							auto coroutine = (*this)(&data, groupX, groupY, groupZ, workgroupMemory.data(), subgroupIndex, 1);
							coroutines.push(std::move(coroutine));
						}
					}
					else
					{
						auto coroutine = (*this)(&data, groupX, groupY, groupZ, workgroupMemory.data(), 0, subgroupsPerWorkgroup);
						coroutines.push(std::move(coroutine));
					}

					while (coroutines.size() > 0)
					{
						auto coroutine = std::move(coroutines.front());
						coroutines.pop();

						SpirvShader::YieldResult result;
						if (coroutine->await(result))
						{
							// TODO: Consider result (when the enum is more than 1 entry).
							coroutines.push(std::move(coroutine));
						}
					}

				} // groupX
			} // groupY
		} // groupZ
	}

} // namespace sw
