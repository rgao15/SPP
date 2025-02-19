// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#pragma once

#include "SPPSceneO.h"
#include "SPPMesh.h"
#include "SPPSceneRendering.h"

#if _WIN32 && !defined(SPP_SDF_STATIC)

	#ifdef SPP_SDFO_EXPORT
		#define SPP_SDF_API __declspec(dllexport)
	#else
		#define SPP_SDF_API __declspec(dllimport)
	#endif

#else

	#define SPP_SDF_API 

#endif


namespace SPP
{		
	enum class EShapeOp : uint32_t
	{
		Add = 0,
		Subtract,
		Intersect
	};

	enum class EShapeType : uint32_t
	{
		Unknown = 0,
		Sphere,
		Box
	};

	struct SPP_SDF_API SDFShape
	{
		EShapeType shapeType = EShapeType::Unknown;
		Vector3 translation = { 0,0,0 };

		EShapeOp shapeOp = EShapeOp::Add;
		Vector3 eulerRotation = { 0,0,0 };

		Vector4 shapeBlendAndScale = { 0,0,0,0 };
		Vector4 params = { 0,0,0,0 };
	};

	class SPP_SDF_API RenderableSignedDistanceField : public Renderable
	{
	protected:
		std::vector< SDFShape > _shapes;
		Vector3 _color = { 0,0,0 };
		GPUReferencer< GPUShader > _customShader;

	public:
		std::vector< SDFShape >& GetShapes()
		{
			return _shapes;
		}
		Vector3& GetColor()
		{
			return _color;
		}
		void SetShader(GPUReferencer< GPUShader > InShader)
		{
			_customShader = InShader;
		}
	};

	class SPP_SDF_API OShape : public OElement
	{
		RTTR_ENABLE(OElement);
		RTTR_REGISTRATION_FRIEND

	protected:
		EShapeType _shapeType = EShapeType::Unknown;
		EShapeOp _shapeOp = EShapeOp::Add;
		float _shapeBlendFactor = 0.0f;

		OShape(const MetaPath& InPath) : OElement(InPath) { }

	public:
		virtual SDFShape GenerateShape() const
		{
			SDFShape oShape;
			oShape.shapeType = _shapeType;
			oShape.shapeOp = _shapeOp;
			oShape.shapeBlendAndScale[0] = _shapeBlendFactor;
			oShape.translation = _translation.cast<float>();
			return oShape;
		}

		virtual ~OShape() { }
	};

	class SPP_SDF_API OShapeGroup : public OElement
	{
		RTTR_ENABLE(OElement);
		RTTR_REGISTRATION_FRIEND

	protected:
		OShapeGroup(const MetaPath& InPath) : OElement(InPath) { }
		std::shared_ptr<RenderableSignedDistanceField> _renderableSDF;
		std::vector<SDFShape> _shapeCache;
		Vector3 _color = { 0.5f,0.5f,0.5f };
		GPUReferencer< GPUShader > _shaderOverride;

		void _GenerateShapes();

	public:

		void SetCustomShader(GPUReferencer< GPUShader > &InShader)
		{
			_shaderOverride = InShader;
		}
		
		virtual void AddedToScene(class OScene* InScene) override;
		virtual void RemovedFromScene(class OScene* InScene) override;

		virtual ~OShapeGroup() { }
	};

	class SPP_SDF_API OSDFSphere : public OShape
	{
		RTTR_ENABLE(OShape);
		RTTR_REGISTRATION_FRIEND

	protected:
		OSDFSphere(const MetaPath& InPath) : OShape(InPath) 
		{
			_shapeType = EShapeType::Sphere;
		}
		float _radius = 1.0f;

	public:		
		virtual SDFShape GenerateShape() const
		{
			SDFShape oShape;
			oShape.shapeType = _shapeType;
			oShape.shapeOp = _shapeOp;
			oShape.translation = _translation.cast<float>();
			oShape.params[0] = _radius;
			oShape.shapeBlendAndScale[0] = _shapeBlendFactor;
			return oShape;
		}
		void SetRadius(float InRadius);
		virtual ~OSDFSphere() { }
	};

	class SPP_SDF_API OSDFBox : public OShape
	{
		RTTR_ENABLE(OShape);
		RTTR_REGISTRATION_FRIEND

	protected:
		OSDFBox(const MetaPath& InPath) : OShape(InPath) 
		{
			_shapeType = EShapeType::Box;
		}
		Vector3 _extents = { 1, 1, 1 };
	public:
		virtual SDFShape GenerateShape() const
		{
			SDFShape oShape;
			oShape.shapeType = _shapeType;
			oShape.shapeOp = _shapeOp;
			oShape.translation = _translation.cast<float>();
			oShape.params[0] = _extents[0];
			oShape.params[1] = _extents[1];
			oShape.params[2] = _extents[2];
			oShape.shapeBlendAndScale[0] = _shapeBlendFactor;
			return oShape;
		}
		void SetExtents(Vector3 InExtents)
		{
			_extents = InExtents;
		}
		virtual ~OSDFBox() { }
	};

	SPP_SDF_API uint32_t GetSDFVersion();
}