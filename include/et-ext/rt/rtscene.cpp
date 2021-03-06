/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#include <et-ext/rt/rtscene.h>

namespace et
{
namespace rt
{

inline float4 gammaCorrectedInput(const vec4& c)
{
	return float4(std::pow(c.x, 2.2f), std::pow(c.y, 2.2f), std::pow(c.z, 2.2f), 1.0f);
}

void Scene::build(const Vector<SceneEntry>& geometry, const Camera::Pointer& camera)
{
	materials.clear();
	emitters.clear();

	TriangleList triangles;
	triangles.reserve(0xffff);

	auto materialIndexWithName = [this](const std::string& name) -> uint32_t
	{
		for (size_t i = 0, e = materials.size(); i < e; ++i)
		{
			if (materials[i].name == name)
				return static_cast<uint32_t>(i);
		}
		return InvalidIndex;
	};

	for (const SceneEntry& scn : geometry)
	{
		if (scn.light.valid())
		{
			switch (scn.light->type())
			{
				case Light::Type::UniformColorEnvironment:
					addEmitter(UniformEmitter::Pointer::create(float4(scn.light->color(), 1.0f)));
					break;
				default:
					ET_FAIL_FMT("Unsupported light type %u", scn.light->type());
			}
			continue;
		}

		et::Material::Pointer batchMaterial = scn.batch->material();

		uint32_t materialIndex = materialIndexWithName(batchMaterial->name());
		if (materialIndex == InvalidIndex)
		{
			float alpha = clamp(batchMaterial->getFloat(MaterialVariable::RoughnessScale), 0.0f, 1.0f);
			float metallness = clamp(batchMaterial->getFloat(MaterialVariable::MetallnessScale), 0.0f, 1.0f);
			float eta = batchMaterial->getFloat(MaterialVariable::IndexOfRefraction);

			Material::Class cls = Material::Class::Diffuse;
			if (metallness == 1.0f)
			{
				log::info("Adding new conductor material: %s", batchMaterial->name().c_str());
				cls = Material::Class::Conductor;
			}
			else if (metallness > 0.0f)
			{
				log::info("Adding new dielectric material: %s", batchMaterial->name().c_str());
				cls = Material::Class::Dielectric;
			}
			else
			{
				log::info("Adding new diffuse material: %s", batchMaterial->name().c_str());
			}

			materialIndex = static_cast<uint32_t>(materials.size());
			materials.emplace_back(cls);
			auto& mat = materials.back();

			mat.name = batchMaterial->name();
			mat.diffuse = gammaCorrectedInput(batchMaterial->getVector(MaterialVariable::DiffuseReflectance));
			mat.specular = gammaCorrectedInput(batchMaterial->getVector(MaterialVariable::SpecularReflectance));
			mat.emissive = float4(batchMaterial->getVector(MaterialVariable::EmissiveColor));
			mat.roughness = clamp(std::pow(alpha, 4.0f), 0.001f, 1.0f);
			mat.metallness = metallness;
			mat.ior = eta;
		}

		VertexStorage::Pointer vs = scn.batch->vertexStorage();
		ET_ASSERT(vs.valid());

		IndexArray::Pointer ia = scn.batch->indexArray();
		ET_ASSERT(ia.valid());

		triangles.reserve(triangles.size() + scn.batch->numIndexes());

		const auto pos = vs->accessData<DataType::Vec3>(VertexAttributeUsage::Position, 0);
		const auto nrm = vs->accessData<DataType::Vec3>(VertexAttributeUsage::Normal, 0);

		bool hasUV = vs->hasAttribute(VertexAttributeUsage::TexCoord0);
		VertexDataAccessor<DataType::Vec2> uv0;
		if (hasUV)
		{
			uv0 = vs->accessData<DataType::Vec2>(VertexAttributeUsage::TexCoord0, 0);
		}

		uint32_t firstTriange = static_cast<int>(triangles.size());

		const mat4& t = scn.transformation;
		for (uint32_t i = 0; i < scn.batch->numIndexes(); i += 3)
		{
			uint32_t i0 = ia->getIndex(scn.batch->firstIndex() + i + 0);
			uint32_t i1 = ia->getIndex(scn.batch->firstIndex() + i + 1);
			uint32_t i2 = ia->getIndex(scn.batch->firstIndex() + i + 2);

			triangles.emplace_back();
			auto& tri = triangles.back();
			tri.v[0] = float4(t * pos[i0], 1.0f);
			tri.v[1] = float4(t * pos[i1], 1.0f);
			tri.v[2] = float4(t * pos[i2], 1.0f);
			tri.n[0] = float4(t.rotationMultiply(nrm[i0]).normalized(), 0.0f);
			tri.n[1] = float4(t.rotationMultiply(nrm[i1]).normalized(), 0.0f);
			tri.n[2] = float4(t.rotationMultiply(nrm[i2]).normalized(), 0.0f);
			if (hasUV)
			{
				tri.t[0] = float4(uv0[i0].x, uv0[i0].y, 0.0f, 0.0f);
				tri.t[1] = float4(uv0[i1].x, uv0[i1].y, 0.0f, 0.0f);
				tri.t[2] = float4(uv0[i2].x, uv0[i2].y, 0.0f, 0.0f);
			}
			else
			{
				tri.t[0] = tri.t[1] = tri.t[2] = float4(0.0f);
			}
			tri.materialIndex = static_cast<uint32_t>(materialIndex);
			tri.computeSupportData();
		}

		uint32_t numTriangles = static_cast<uint32_t>(triangles.size()) - firstTriange;
		bool isEmitter = materials[materialIndex].emissive.length() > 0.0f;
		if (isEmitter && (numTriangles > 0))
		{
			log::info("Adding area emitter");
			addEmitter(MeshEmitter::Pointer::create(firstTriange, numTriangles, materialIndex));
		}
	}

	kdTree.build(triangles, options.maxKDTreeDepth);

	for (Emitter::Pointer& em : emitters)
		em->prepare(*this);

	centerRay = camera->castRay(vec2(0.0f));
	KDTree::TraverseResult centerHit = kdTree.traverse(centerRay);
	if (centerHit.triangleIndex != InvalidIndex)
		focalDistance = (centerHit.intersectionPoint - float4(centerRay.origin, 0.0f)).length();
	focalDistance += options.focalDistanceCorrection;

	auto stats = kdTree.nodesStatistics();
	log::info("KD-Tree statistics:\n\t%llu nodes\n\t%llu leaf nodes\n\t%llu empty leaf nodes"
		"\n\t%llu max depth\n\t%llu min triangles per node\n\t%llu max triangles per node"
		"\n\t%llu total triangles\n\t%llu distributed triangles"
		"\n\t%.2f focal distance"
		"\n\t%.2f aperture size",
		uint64_t(stats.totalNodes), uint64_t(stats.leafNodes), uint64_t(stats.emptyLeafNodes),
		uint64_t(stats.maxDepth), uint64_t(stats.minTrianglesPerNode), uint64_t(stats.maxTrianglesPerNode),
		uint64_t(stats.totalTriangles), uint64_t(stats.distributedTriangles),
		focalDistance, options.apertureSize);

	if (options.renderKDTree)
	{
		kdTree.printStructure();
	}
}

void Scene::addEmitter(const Emitter::Pointer& em)
{
	emitters.emplace_back(em);
}

}
}
