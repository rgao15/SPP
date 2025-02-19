#include "SPPSDFO.h"
#include "SPPGraphicsO.h"

SPP_OVERLOAD_ALLOCATORS

namespace SPP
{
	uint32_t GetSDFVersion()
	{
		return 1;
	}

	void OShapeGroup::AddedToScene(class OScene* InScene)
	{
		if (!InScene) return;

		_GenerateShapes();

		auto SceneType = InScene->get_type();
		if (!_shapeCache.empty() &&
			SceneType.is_derived_from(rttr::type::get<ORenderableScene>()))
		{
			_renderableSDF = GGI()->CreateRenderableSDF();

			auto& pos = _renderableSDF->GetPosition();
			pos = _translation;

			auto& rot = _renderableSDF->GetRotation();
			rot = _rotation;

			_renderableSDF->SetShader(_shaderOverride);
			_renderableSDF->GetShapes() = _shapeCache;
			_renderableSDF->GetColor() = _color;
			_renderableSDF->AddToScene(((ORenderableScene*)InScene)->GetRenderScene());
		}
	}


	void OShapeGroup::_GenerateShapes()
	{
		_shapeCache.clear();
		for (auto& curChild : _children)
		{
			SE_ASSERT(curChild);
			auto childType = curChild->get_type();
			SE_ASSERT(childType.is_derived_from(rttr::type::get<OShape>()));

			_shapeCache.push_back(((OShape*)curChild)->GenerateShape());
		}
	}

	void OShapeGroup::RemovedFromScene(class OScene* InScene)
	{
		if (!InScene) return;

		if (_renderableSDF)
		{
			_renderableSDF->RemoveFromScene();
			_renderableSDF.reset();
		}
	}

	void OSDFSphere::SetRadius(float InRadius)
	{
		_radius = InRadius;
	}
}

using namespace SPP;

RTTR_REGISTRATION
{	
	rttr::registration::enumeration<EShapeOp>("EShapeOp")
		(
			rttr::value("Add",		EShapeOp::Add),
			rttr::value("Subtract",   EShapeOp::Subtract),
			rttr::value("Intersect",	EShapeOp::Intersect)
		);

	rttr::registration::class_<OShape>("OShape")
		.constructor<const MetaPath&>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_shapeOp", &OShape::_shapeOp)
		.property("_shapeBlendFactor", &OShape::_shapeBlendFactor)
		;

	rttr::registration::class_<OSDFSphere>("OSDFSphere")
		.constructor<const MetaPath&>()
		(
			rttr::policy::ctor::as_raw_ptr
			)
		.property("_radius", &OSDFSphere::_radius)
		;

	rttr::registration::class_<OSDFBox>("OSDFBox")
		.constructor<const MetaPath&>()
		(
			rttr::policy::ctor::as_raw_ptr
			)
		.property("_extents", &OSDFBox::_extents)(rttr::policy::prop::as_reference_wrapper)
		;

	rttr::registration::class_<OShapeGroup>("OShapeGroup")
		.constructor<const MetaPath&>()
		(
			rttr::policy::ctor::as_raw_ptr
		)
		.property("_color", &OShapeGroup::_color)(rttr::policy::prop::as_reference_wrapper)		
		;
}
