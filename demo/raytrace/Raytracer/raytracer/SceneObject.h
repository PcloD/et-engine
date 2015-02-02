//
//  SceneObject.h
//  Raytracer
//
//  Created by Sergey Reznik on 16/11/2014.
//  Copyright (c) 2014 Cheetek. All rights reserved.
//

#pragma once

#include "SceneIntersection.h"

namespace rt
{
	struct SceneTriangle
	{
		et::triangleEx tri;
		size_t materialIndex = 0;
		
		SceneTriangle()
			{ }

		SceneTriangle(const et::vec3& v1, const et::vec3& v2, const et::vec3& v3, const et::vec3& n1,
			const et::vec3& n2, const et::vec3& n3, size_t m) : tri(v1, v2, v3, n1, n2, n3), materialIndex(m) { }
		
		SceneTriangle(const et::triangleEx& t, size_t m) :
			tri(t), materialIndex(m) { }
	};
	
	typedef et::DataStorage<SceneTriangle> SceneTriangleList;
	
	class SceneObject
	{
	public:
		virtual ~SceneObject() { }
		virtual bool intersects(const et::ray3d&, et::vec3&, et::vec3&, size_t&) = 0;
	
	public:
		SceneObject(size_t m) :
			_materialId(m) { }
		
		size_t materialId() const
			{ return _materialId; }

	private:
		size_t _materialId = 0;
	};
	
	class MeshObject : public SceneObject
	{
	public:
		MeshObject(size_t, size_t, const SceneTriangleList&);
		
		bool intersects(const et::ray3d&, et::vec3&, et::vec3&, size_t&);
		
	private:
		void buildBoundingBox();
		
	private:
		const SceneTriangleList& _triangles;
		size_t _firstTriangle = 0;
		size_t _numTriangles = 0;
		et::AABB _boundingBox;
	};
}
