/*
 * This file is part of `et engine`
 * Copyright 2009-2015 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#pragma once

#include <ostream>
#include <istream>
#include <et/core/et.h>
#include <et/scene3d/base.h>
#include <et/vertexbuffer/vertexstorage.h>
#include <et/vertexbuffer/indexarray.h>
#include <et/rendering/vertexarrayobject.h>

namespace et
{
	namespace s3d
	{
		class SceneMaterial;
		class BaseElement;
		typedef IntrusivePtr<BaseElement> BaseElementPointer;

		class SerializationHelper
		{
		public:
			virtual const std::string& serializationBasePath() const = 0;
			virtual BaseElementPointer createElementOfType(ElementType, BaseElement*) = 0;
			virtual SceneMaterial* materialWithName(const std::string&) = 0;
			
			virtual IndexArray::Pointer indexArrayWithName(const std::string&) = 0;
			virtual VertexStorage::Pointer vertexStorageWithName(const std::string&) = 0;
			
			virtual VertexArrayObject vertexArrayWithStorageName(const std::string&) = 0;

		public:
			virtual ~SerializationHelper() { }
		};

		enum : uint32_t
		{
			DeserializeOption_LoadHierarchy = 0x0000,
			
			DeserializeOption_KeepGeometry = 0x0001,
			DeserializeOption_KeepSupportMeshes = 0x0002,
			
			DeserializeOption_CreateTextures = 0x0010,
			DeserializeOption_CreateVertexBuffers = 0x0020,
			
			DeserializeOption_KeepAndCreateEverything = DeserializeOption_LoadHierarchy |
				DeserializeOption_KeepGeometry | DeserializeOption_KeepSupportMeshes | 
				DeserializeOption_CreateTextures | DeserializeOption_CreateVertexBuffers
		};
		
		extern const std::string kStorage;
		extern const std::string kName;
		extern const std::string kChildren;
		extern const std::string kElementTypeCode;
		extern const std::string kMaterialName;
		extern const std::string kFlagsValue;
		extern const std::string kTranslation;
		extern const std::string kScale;
		extern const std::string kOrientation;
		extern const std::string kProperties;
		extern const std::string kAnimations;
		extern const std::string kMaterials;
		extern const std::string kBlendState;
		extern const std::string kDepthState;
		extern const std::string kIntegerValues;
		extern const std::string kFloatValues;
		extern const std::string kVectorValues;
		extern const std::string kTextures;
		extern const std::string kLods;
		extern const std::string kStartIndex;
		extern const std::string kIndexesCount;
		extern const std::string kVertexArrayName;
		extern const std::string kVertexStorageName;
		extern const std::string kIndexArrayName;
		extern const std::string kMinMaxCenter;
		extern const std::string kAverageCenter;
		extern const std::string kDimensions;
		extern const std::string kBoundingSphereRadius;
		extern const std::string kSupportData;
		extern const std::string kVertexStorages;
		extern const std::string kIndexArray;
		extern const std::string kBinary;
		extern const std::string kVertexDeclaration;
		extern const std::string kUsage;
		extern const std::string kType;
		extern const std::string kDataType;
		extern const std::string kStride;
		extern const std::string kOffset;
		extern const std::string kComponents;
		extern const std::string kDataSize;
		extern const std::string kFormat;
		extern const std::string kPrimitiveType;
		extern const std::string kModelView;
		extern const std::string kProjection;
		extern const std::string kUpVector;
		extern const std::string kUpVectorLocked;
		extern const std::string kVertexBufferName;
		extern const std::string kIndexBufferName;
		extern const std::string kVertexBufferSourceName;
		extern const std::string kIndexBufferSourceName;
		extern const std::string kVertexArrayObjects;
		extern const std::string kBlendEnabled;
		extern const std::string kSourceColor;
		extern const std::string kDestColor;
		extern const std::string kSourceAlpha;
		extern const std::string kDestAlpha;
		extern const std::string kColorOperation;
		extern const std::string kAlphaOperation;
		extern const std::string kDepthFunction;
		extern const std::string kDepthWriteEnabled;
		extern const std::string kDepthTestEnabled;
	}
}
