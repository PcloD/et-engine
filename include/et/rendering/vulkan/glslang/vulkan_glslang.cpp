/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#include <external/glslang/glslang/Public/ShaderLang.h>
#include <external/glslang/SPIRV/Logger.h>
#include <external/glslang/SPIRV/GlslangToSpv.h>
#include <external/glslang/OGLCompilersDLL/InitializeDll.h>
#include <external/glslang/glslang/MachineIndependent/localintermediate.h>
#include <external/glslang/glslang/MachineIndependent/reflection.h>
#include <external/spirvcross/spirv_cross.hpp>
#include <external/spirvcross/spirv_glsl.hpp>
#include <external/spirvcross/spirv_msl.hpp>
#include "vulkan_glslang.h"
#include <fstream>

#define ET_PREPROCESS_HLSL				0
#define ET_COMPILE_TEST_HLSL			0
#define ET_CROSS_COMPILE_SHADERS_TEST	0

#if (ET_PLATFORM_WIN && ET_COMPILE_TEST_HLSL)
#	include <d3dcompiler.h>
#	include <wrl/client.h>
#	pragma comment(lib, "d3dcompiler.lib")
#endif

namespace glslang
{
extern const TBuiltInResource DefaultTBuiltInResource;
}

namespace et
{

void buildProgramInputLayout(const glslang::TProgram&, Program::Reflection& reflection);
void buildProgramReflection(const glslang::TProgram&, Program::Reflection& reflection, ProgramStage stage);
void dumpSource(const std::string&);
void crossCompile(const std::vector<uint32_t>&);

class VertexShaderAttribLocationTraverser : public glslang::TIntermTraverser
{
	void visitSymbol(glslang::TIntermSymbol* symbol) override
	{
		glslang::TQualifier& qualifier = symbol->getQualifier();
		if ((qualifier.storage == glslang::TStorageQualifier::EvqVaryingIn) && qualifier.hasLocation())
		{
			VertexAttributeUsage usage = semanticToVertexAttributeUsage(symbol->getQualifier().semanticName);
			if ((usage != VertexAttributeUsage::Unknown) && (static_cast<int>(usage) != qualifier.layoutLocation))
				qualifier.layoutLocation = static_cast<int>(usage);
		}
	}
};

bool buildProgram(glslang::TProgram& program, EShMessages messages)
{
	if (!program.link(messages))
	{
		log::error("Failed to link program:\n%s", program.getInfoLog());
		debug::debugBreak();
		return false;
	}

	if (!program.mapIO())
	{
		log::error("Failed to map program's IO:\n%s", program.getInfoLog());
		debug::debugBreak();
		return false;
	}

	if (program.buildReflection() == false)
	{
		log::error("Failed to build reflection:\n%s", program.getInfoLog());
		debug::debugBreak();
		return false;
	}

	return true;
}

bool performDX11CompileTest(const std::string& preprocessedVertexShader, const std::string& preprocessedFragmentShader)
{
#if (ET_PLATFORM_WIN && ET_COMPILE_TEST_HLSL)
	Microsoft::WRL::ComPtr<ID3DBlob> vertexBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> vertexErrors = nullptr;
	HRESULT vResult = D3DCompile(preprocessedVertexShader.c_str(), preprocessedVertexShader.length(),
		nullptr, nullptr, nullptr, "vertexMain", "vs_5_1",
		D3DCOMPILE_DEBUG | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL0 | D3DCOMPILE_WARNINGS_ARE_ERRORS,
		0, vertexBlob.GetAddressOf(), vertexErrors.GetAddressOf());

	if (FAILED(vResult))
	{
		log::error("Compile test of HLSL vertex shader failed");
		if (vertexErrors)
		{
			std::string errorString(reinterpret_cast<const char*>(vertexErrors->GetBufferPointer()), vertexErrors->GetBufferSize());
			log::error("Errors: %s", errorString.c_str());
			dumpSource(preprocessedVertexShader);
		}
		return false;
	}

	Microsoft::WRL::ComPtr<ID3DBlob> fragmentBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> fragmentErrors = nullptr;
	HRESULT fResult = D3DCompile(preprocessedFragmentShader.c_str(), preprocessedFragmentShader.length(),
		nullptr, nullptr, nullptr, "fragmentMain", "ps_5_1",
		D3DCOMPILE_DEBUG | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL0 | D3DCOMPILE_WARNINGS_ARE_ERRORS,
		0, fragmentBlob.GetAddressOf(), fragmentErrors.GetAddressOf());
	if (FAILED(fResult))
	{
		log::error("Compile test of HLSL fragment shader failed");
		if (fragmentErrors)
		{
			std::string errorString(reinterpret_cast<const char*>(fragmentErrors->GetBufferPointer()), fragmentErrors->GetBufferSize());
			log::error("Errors: %s", errorString.c_str());
			dumpSource(preprocessedFragmentShader);
		}
		return false;
	}
#endif

	return true;
}

EShLanguage shLanguageFromStage(ProgramStage stage)
{
	switch (stage)
	{
	case ProgramStage::Vertex:
		return EShLanguage::EShLangVertex;
	case ProgramStage::Fragment:
		return EShLanguage::EShLangFragment;
	case ProgramStage::Compute:
		return EShLanguage::EShLangCompute;
	default:
		ET_FAIL("Invalid stage speicified");
	}
}

bool generateSPIRFromHLSL(const std::string& source, SPIRProgramStageMap& stages, Program::Reflection& reflection)
{
	EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules | EShMsgReadHlsl);
	const char* rawSource[] = { source.c_str() };
	const char* shaderName[] = { "shader_source" };
	for (auto& stage : stages)
	{
		EShLanguage language = shLanguageFromStage(stage.first);
		const char* entryName = vulkan::programStageEntryName(stage.first);

		glslang::TShader shader(language);
		shader.setStringsWithLengthsAndNames(rawSource, nullptr, shaderName, 1);
		shader.setAutoMapBindings(true);
		shader.setEntryPoint(entryName);

#	if (ET_PREPROCESS_HLSL)
		std::string preprocessedSource;
		preprocessedSource.reserve(source.size());
		glslang::TShader::ForbidIncluder forbidIncluder;
		if (!shader.preprocess(&glslang::DefaultTBuiltInResource, 0, EProfile::ECoreProfile, false, true, messages, &preprocessedSource, forbidIncluder))
		{
			dumpSource(rawSource[0]);
			log::error("Failed to preprocess shader with entry %s:\n%s", entryName, shader.getInfoLog());
#		if (ET_DEBUG)
			debug::debugBreak();
#		endif
			return false;
		}
		const char* preprocessedRawSource[] = { preprocessedSource.c_str() };
		shader.setStringsWithLengthsAndNames(preprocessedRawSource, nullptr, shaderName, 1);
#	endif

		if (!shader.parse(&glslang::DefaultTBuiltInResource, 0, true, messages))
		{
			dumpSource(rawSource[0]);
			log::error("Failed to parse shader with entry %s:\n%s", entryName, shader.getInfoLog());
#		if (ET_DEBUG)
			debug::debugBreak();
#		endif
			return false;
		}

		glslang::TProgram program;
		program.addShader(&shader);

		if (!buildProgram(program, messages))
			return false;

		if (stage.first == ProgramStage::Vertex)
			buildProgramInputLayout(program, reflection);

		buildProgramReflection(program, reflection, stage.first);

		glslang::TIntermediate* intermediate = program.getIntermediate(language);
		if (intermediate == nullptr)
		{
			log::error("Failed to get binary for %s:\n%s", entryName, program.getInfoLog());
			debug::debugBreak();
			return false;
		}

		if (stage.first == ProgramStage::Vertex)
		{
			VertexShaderAttribLocationTraverser attribFixup;
			intermediate->getTreeRoot()->traverse(&attribFixup);
		}
		
		{
			stage.second.reserve(10240);
			spv::SpvBuildLogger logger;
			glslang::GlslangToSpv(*intermediate, stage.second, &logger, nullptr);
			std::string allMessages = logger.getAllMessages();
			if (!allMessages.empty())
				log::info("HLSL to SPV:\n%s", allMessages.c_str());

#		if (ET_CROSS_COMPILE_SHADERS_TEST)
			crossCompile(stage.second);
#		endif
		}
	}
	return true;
}

void initGlslangResources()
{
	glslang::InitProcess();
	glslang::InitializeProcess();
}

void cleanupGlslangResources()
{
	glslang::FinalizeProcess();
	glslang::DetachProcess();
}

void buildProgramInputLayout(const glslang::TProgram& program, Program::Reflection& reflection)
{
	reflection.inputLayout.clear();
	int attribs = program.getNumLiveAttributes();
	for (int attrib = 0; attrib < attribs; ++attrib)
	{
		const glslang::TType* attribType = program.getAttributeTType(attrib);
		const char* semanticName = attribType->getQualifier().semanticName;
		ET_ASSERT(semanticName != nullptr);

		VertexAttributeUsage usage = semanticToVertexAttributeUsage(semanticName);
		if (usage != VertexAttributeUsage::Unknown)
		{
			if (attribType->getBasicType() == glslang::TBasicType::EbtFloat)
			{
				switch (attribType->getVectorSize())
				{
				case 1:
					reflection.inputLayout.push_back(usage, DataType::Float);
					break;
				case 2:
					reflection.inputLayout.push_back(usage, DataType::Vec2);
					break;
				case 3:
					reflection.inputLayout.push_back(usage, DataType::Vec3);
					break;
				case 4:
					reflection.inputLayout.push_back(usage, DataType::Vec4);
					break;
				default:
					ET_ASSERT(!"Invalid vector size for attribute");
				}
			}
			else if ((attribType->getBasicType() == glslang::TBasicType::EbtInt) || (attribType->getBasicType() == glslang::TBasicType::EbtUint))
			{
				switch (attribType->getVectorSize())
				{
				case 1:
					reflection.inputLayout.push_back(usage, DataType::Int);
					break;
				case 2:
					reflection.inputLayout.push_back(usage, DataType::IntVec2);
					break;
				case 3:
					reflection.inputLayout.push_back(usage, DataType::IntVec3);
					break;
				case 4:
					reflection.inputLayout.push_back(usage, DataType::IntVec4);
					break;
				default:
					ET_ASSERT(!"Invalid vector size for attribute");
				}
			}
			else
			{
				ET_ASSERT(!"Unsupported vertex attribute type");
			}
		}
		else
		{
			ET_ASSERT(!"Unsupported vertex attribute usage");
		}
	}
}

void buildProgramReflection(const glslang::TProgram& program, Program::Reflection& reflection, ProgramStage stage)
{
	static const String& kObjectVariables = "ObjectVariables";
	static const String& kMaterialVariables = "MaterialVariables";

	int blocks = program.getNumLiveUniformBlocks();
	for (int block = 0; block < blocks; ++block)
	{
		String blockName(program.getUniformBlockName(block));
		int blockSize = program.getUniformBlockSize(block);

		if (blockName == kObjectVariables)
		{
			reflection.objectVariablesBufferSize = blockSize;
		}
		else if (blockName == kMaterialVariables)
		{
			reflection.materialVariablesBufferSize = blockSize;
		}
		else
		{
			log::error("Unknown uniform block: %s", blockName.c_str());
		}
	}

	auto& textureSet = reflection.textures[stage].textures;
	auto& samplerSet = reflection.textures[stage].samplers;
	auto& imageSet = reflection.textures[stage].images;

	int uniforms = program.getNumLiveUniformVariables();
	for (int u = 0; u < uniforms; ++u)
	{
		std::string uniformName(program.getUniformName(u));
		int uniformBlockIndex = program.getUniformBlockIndex(u);

		String blockName(program.getUniformBlockName(uniformBlockIndex));
		if (blockName.empty())
		{
			const glslang::TType* type = program.getUniformTType(u);
			if (type->getBasicType() == glslang::TBasicType::EbtSampler)
			{
				const glslang::TQualifier& qualifier = type->getQualifier();
				const glslang::TSampler& sampler = type->getSampler();
				ET_ASSERT(qualifier.hasBinding());
				ET_ASSERT(qualifier.hasSet());
				if (sampler.isImage())
				{
					ET_ASSERT(qualifier.layoutBinding < StorageBuffer_max);
					imageSet.emplace(uniformName, qualifier.layoutBinding);
				}
				else if (sampler.isTexture())
				{
					ET_ASSERT(qualifier.layoutBinding < MaterialTexture_max);
					textureSet.emplace(uniformName, qualifier.layoutBinding);
				}
				else if (sampler.isPureSampler())
				{
					ET_ASSERT(qualifier.layoutBinding >= MaterialSamplerBindingOffset);
					ET_ASSERT(qualifier.layoutBinding < MaterialTexture_max + MaterialSamplerBindingOffset);
					samplerSet.emplace(uniformName, qualifier.layoutBinding);
				}
				else
				{
					log::warning("Unsupported sampler type in shader: %s", sampler.getString().c_str());
				}
			}
			else
			{
				log::warning("Unsupported uniform type in shader: %s", type->getBasicTypeString().c_str());
			}
		}
		else
		{
			int uniformOffset = program.getUniformBufferOffset(u);
			if (blockName == kObjectVariables)
			{
				ObjectVariable varId = stringToObjectVariable(uniformName);
				ET_ASSERT(varId != ObjectVariable::max);
				reflection.objectVariables[static_cast<uint32_t>(varId)].offset = static_cast<uint32_t>(uniformOffset);
				reflection.objectVariables[static_cast<uint32_t>(varId)].enabled = 1;
			}
			else if (blockName == kMaterialVariables)
			{
				MaterialVariable varId = stringToMaterialVariable(uniformName);
				ET_ASSERT(varId != MaterialVariable::max);
				reflection.materialVariables[static_cast<uint32_t>(varId)].offset = static_cast<uint32_t>(uniformOffset);
				reflection.materialVariables[static_cast<uint32_t>(varId)].enabled = 1;
			}
			else
			{
				log::error("Unknown uniform block: %s for uniform %s", blockName.c_str(), uniformName.c_str());
			}
		}
	}
}

void dumpSource(const std::string& s)
{
	StringList lines = split(s, "\n");

	uint32_t lineIndex = 1;
	for (const std::string& line : lines)
	{
		log::info("%04u: %s", lineIndex, line.c_str());
		++lineIndex;
	}
}

void crossCompile(const std::vector<uint32_t>& spirv)
{
	spirv_cross::CompilerGLSL compiler(spirv);
	compiler.build_combined_image_samplers();
	dumpSource(compiler.compile());
}

}

namespace glslang
{
const TBuiltInResource DefaultTBuiltInResource = {
	/* .MaxLights = */ 32,
	/* .MaxClipPlanes = */ 6,
	/* .MaxTextureUnits = */ 32,
	/* .MaxTextureCoords = */ 32,
	/* .MaxVertexAttribs = */ 64,
	/* .MaxVertexUniformComponents = */ 4096,
	/* .MaxVaryingFloats = */ 64,
	/* .MaxVertexTextureImageUnits = */ 32,
	/* .MaxCombinedTextureImageUnits = */ 80,
	/* .MaxTextureImageUnits = */ 32,
	/* .MaxFragmentUniformComponents = */ 4096,
	/* .MaxDrawBuffers = */ 32,
	/* .MaxVertexUniformVectors = */ 128,
	/* .MaxVaryingVectors = */ 8,
	/* .MaxFragmentUniformVectors = */ 16,
	/* .MaxVertexOutputVectors = */ 16,
	/* .MaxFragmentInputVectors = */ 15,
	/* .MinProgramTexelOffset = */ -8,
	/* .MaxProgramTexelOffset = */ 7,
	/* .MaxClipDistances = */ 8,
	/* .MaxComputeWorkGroupCountX = */ 65535,
	/* .MaxComputeWorkGroupCountY = */ 65535,
	/* .MaxComputeWorkGroupCountZ = */ 65535,
	/* .MaxComputeWorkGroupSizeX = */ 1024,
	/* .MaxComputeWorkGroupSizeY = */ 1024,
	/* .MaxComputeWorkGroupSizeZ = */ 64,
	/* .MaxComputeUniformComponents = */ 1024,
	/* .MaxComputeTextureImageUnits = */ 16,
	/* .MaxComputeImageUniforms = */ 8,
	/* .MaxComputeAtomicCounters = */ 8,
	/* .MaxComputeAtomicCounterBuffers = */ 1,
	/* .MaxVaryingComponents = */ 60,
	/* .MaxVertexOutputComponents = */ 64,
	/* .MaxGeometryInputComponents = */ 64,
	/* .MaxGeometryOutputComponents = */ 128,
	/* .MaxFragmentInputComponents = */ 128,
	/* .MaxImageUnits = */ 8,
	/* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
	/* .MaxCombinedShaderOutputResources = */ 8,
	/* .MaxImageSamples = */ 0,
	/* .MaxVertexImageUniforms = */ 0,
	/* .MaxTessControlImageUniforms = */ 0,
	/* .MaxTessEvaluationImageUniforms = */ 0,
	/* .MaxGeometryImageUniforms = */ 0,
	/* .MaxFragmentImageUniforms = */ 8,
	/* .MaxCombinedImageUniforms = */ 8,
	/* .MaxGeometryTextureImageUnits = */ 16,
	/* .MaxGeometryOutputVertices = */ 256,
	/* .MaxGeometryTotalOutputComponents = */ 1024,
	/* .MaxGeometryUniformComponents = */ 1024,
	/* .MaxGeometryVaryingComponents = */ 64,
	/* .MaxTessControlInputComponents = */ 128,
	/* .MaxTessControlOutputComponents = */ 128,
	/* .MaxTessControlTextureImageUnits = */ 16,
	/* .MaxTessControlUniformComponents = */ 1024,
	/* .MaxTessControlTotalOutputComponents = */ 4096,
	/* .MaxTessEvaluationInputComponents = */ 128,
	/* .MaxTessEvaluationOutputComponents = */ 128,
	/* .MaxTessEvaluationTextureImageUnits = */ 16,
	/* .MaxTessEvaluationUniformComponents = */ 1024,
	/* .MaxTessPatchComponents = */ 120,
	/* .MaxPatchVertices = */ 32,
	/* .MaxTessGenLevel = */ 64,
	/* .MaxViewports = */ 16,
	/* .MaxVertexAtomicCounters = */ 0,
	/* .MaxTessControlAtomicCounters = */ 0,
	/* .MaxTessEvaluationAtomicCounters = */ 0,
	/* .MaxGeometryAtomicCounters = */ 0,
	/* .MaxFragmentAtomicCounters = */ 8,
	/* .MaxCombinedAtomicCounters = */ 8,
	/* .MaxAtomicCounterBindings = */ 1,
	/* .MaxVertexAtomicCounterBuffers = */ 0,
	/* .MaxTessControlAtomicCounterBuffers = */ 0,
	/* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
	/* .MaxGeometryAtomicCounterBuffers = */ 0,
	/* .MaxFragmentAtomicCounterBuffers = */ 1,
	/* .MaxCombinedAtomicCounterBuffers = */ 1,
	/* .MaxAtomicCounterBufferSize = */ 16384,
	/* .MaxTransformFeedbackBuffers = */ 4,
	/* .MaxTransformFeedbackInterleavedComponents = */ 64,
	/* .MaxCullDistances = */ 8,
	/* .MaxCombinedClipAndCullDistances = */ 8,
	/* .MaxSamples = */ 4,
	/* .limits = */ {
		/* .nonInductiveForLoops = */ 1,
		/* .whileLoops = */ 1,
		/* .doWhileLoops = */ 1,
		/* .generalUniformIndexing = */ 1,
		/* .generalAttributeMatrixVectorIndexing = */ 1,
		/* .generalVaryingIndexing = */ 1,
		/* .generalSamplerIndexing = */ 1,
		/* .generalVariableIndexing = */ 1,
		/* .generalConstantMatrixVectorIndexing = */ 1,
	} };
}
