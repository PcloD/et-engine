/*
 * This file is part of `et engine`
 * Copyright 2009-2015 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#include <et/primitives/primitives.h>
#include <et/vertexbuffer/vertexstorage.h>
#include <et/scene3d/supportmesh.h>

using namespace et;
using namespace et::s3d;

SupportMesh::SupportMesh(const std::string& name, Element* parent) :
	Mesh(name, parent)  {}

SupportMesh::SupportMesh(const std::string& name, const VertexArrayObject& ib, const Material::Pointer& material,
	uint32_t start, uint32_t num, const VertexStorage::Pointer& storage, const IndexArray::Pointer& ia, Element* parent) :
	Mesh(name, ib, material, start, num, storage, ia, parent), _data(num / 3, 0)
{
}

void SupportMesh::setNumIndexes(uint32_t num)
{
	Mesh::setNumIndexes(num);
	_data.fitToSize(num / 3);
}

void SupportMesh::fillCollisionData(const VertexStorage::Pointer& v, const IndexArray::Pointer& indexArray)
{
}

SupportMesh* SupportMesh::duplicate()
{
	SupportMesh* result = sharedObjectFactory().createObject<SupportMesh>(name(), vertexArrayObject(),
		material(), startIndex(), numIndexes(), vertexStorage(), indexArray(), parent());

	duplicateBasePropertiesToObject(result);
	duplicateChildrenToObject(result);

	result->_data = _data;
	
	return result;
}

OBB SupportMesh::obb()
{
	mat4 ft = finalTransform();
	mat3 r = ft.mat3();
	vec3 s = removeMatrixScale(r);
	return OBB(ft * supportData().averageCenter, 0.5f * s * supportData().dimensions, r);
}

void SupportMesh::serialize(std::ostream& stream, SceneVersion version)
{
	serializeFloat(stream, 0.0f);
	serializeVector(stream, vec3(0.0f));
	serializeVector(stream, vec3(0.0f));
	serializeUInt64(stream, _data.size());
	stream.write(_data.binary(), _data.dataSize());
	Mesh::serialize(stream, version);
}

void SupportMesh::deserialize(std::istream& stream, ElementFactory* factory, SceneVersion version)
{
	deserializeFloat(stream);
	deserializeVector<vec3>(stream);
	deserializeVector<vec3>(stream);
	
	uint64_t dataSize = (version < SceneVersion_1_1_0) ? deserializeUInt32(stream) : deserializeUInt64(stream);
	_data.resize(dataSize);

	if (version <= SceneVersion_1_0_1)
	{
		for (size_t i = 0, e = _data.size(); i != e; ++i)
		{
			vec3 vertices[3];
			stream.read(reinterpret_cast<char*>(vertices), sizeof(vertices));
			_data[i] = triangle(vertices[0], vertices[1], vertices[2]);
		}
	}
	else 
	{
		stream.read(_data.binary(), _data.dataSize());
	}

	Mesh::deserialize(stream, factory, version);
}

void SupportMesh::transformInvalidated()
{
}
