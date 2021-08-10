// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPMesh.h"
#include "SPPSerialization.h"
#include "SPPAssetCache.h"
#include "SPPMeshlets.h"
#include "SPPMeshSimplifying.h"
#include "SPPLogging.h"

#include "SPPAssetImporterFile.h"
#include "SPPBlenderFile.h"

namespace SPP
{
	LogEntry LOG_MESH("MESH");

	template<>
	inline BinarySerializer& operator<< <Meshlet> (BinarySerializer& Storage, const Meshlet& Value)
	{
		Storage << Value.VertCount;
		Storage << Value.VertOffset;
		Storage << Value.PrimCount;
		Storage << Value.PrimOffset;
		return Storage;
	}

	template<>
	inline BinarySerializer& operator<< <PackedTriangle> (BinarySerializer& Storage, const PackedTriangle& Value)
	{
		Storage << Value.packed;
		return Storage;
	}
	
	bool Mesh::LoadMesh(const AssetPath& FileName)
	{
		SPP_LOG(LOG_MESH, LOG_INFO, "Loading Mesh: %s", *FileName);

		auto FoundCachedBlob = GetCachedAsset(FileName);

		if (false)//FoundCachedBlob)
		{
			BinaryBlobSerializer& blobAsset = *FoundCachedBlob;

			uint32_t MeshCount = 0;
			blobAsset >> MeshCount;

			for (uint32_t Iter = 0; Iter < MeshCount; Iter++)
			{
				AABB meshBounds;
				blobAsset >> meshBounds;

				std::vector<Subset>  meshletSubsets;
				blobAsset >> meshletSubsets;

				auto newMeshElement = std::make_shared<MeshElement>();
				std::swap(newMeshElement->MeshletSubsets, meshletSubsets);
				newMeshElement->Bounds = meshBounds;

				{
					auto meshShaderResource = std::make_shared< ArrayResource >();
					blobAsset >> *meshShaderResource;
					newMeshElement->MeshletResource = GGI()->CreateStaticBuffer(GPUBufferType::Generic, meshShaderResource);
				}
				{
					auto meshShaderResource = std::make_shared< ArrayResource >();
					blobAsset >> *meshShaderResource;
					newMeshElement->UniqueVertexIndexResource = GGI()->CreateStaticBuffer(GPUBufferType::Generic, meshShaderResource);
				}
				{
					auto meshShaderResource = std::make_shared< ArrayResource >();
					blobAsset >> *meshShaderResource;
					newMeshElement->PrimitiveIndexResource = GGI()->CreateStaticBuffer(GPUBufferType::Generic, meshShaderResource);
				}


				auto verticesResource = std::make_shared< ArrayResource >();
				auto indicesResource = std::make_shared< ArrayResource >();

				blobAsset >> *verticesResource;
				blobAsset >> *indicesResource;

				newMeshElement->VertexResource = GGI()->CreateStaticBuffer(GPUBufferType::Vertex, verticesResource);
				newMeshElement->IndexResource = GGI()->CreateStaticBuffer(GPUBufferType::Index, indicesResource);

				_elements.push_back(newMeshElement);

				//RegisterMeshElement(newMeshElement);
			}
		}
		else
		{
			// create cache
			BinaryBlobSerializer outCachedAsset;

			LoadedMeshes loadedMeshes;
#if 1
			LoadBlenderFile(FileName, loadedMeshes);			
#else
			LoadUsingAssImp(FileName, loadedMeshes);
#endif

			for(auto &curLayer : loadedMeshes.Layers)
			{
				uint32_t MeshletMaxVerts = 64;
				uint32_t MeshletMaxPrims = 126;
				
				std::vector<Subset>                     meshletSubsets;
				std::vector<Meshlet>                    meshlets;
				std::vector<MeshNode>                   meshletNodes;
				std::vector<uint8_t>                    uniqueVertexIndices;
				std::vector<PackedTriangle>             primitiveIndices;

				//std::vector<CullData>                   m_cullData;
				Meshletizer  tizing(FileName, std::make_shared< MeshVertexTranslator >(curLayer.VertexResource, curLayer.IndexResource), MeshletMaxVerts, MeshletMaxPrims);
				tizing.ComputeMeshlets(
					meshletSubsets,
					meshlets,
					meshletNodes,
					uniqueVertexIndices,
					primitiveIndices);

				auto newMeshElement = std::make_shared<MeshElement>();
				std::swap(newMeshElement->MeshletSubsets, meshletSubsets);
				std::swap(newMeshElement->MeshletNodes, meshletNodes);
				//newMeshElement->Bounds = meshBounds;

				{
					auto meshShaderResource = std::make_shared< ArrayResource >();
					auto pMeshlets = meshShaderResource->InitializeFromType<Meshlet>(meshlets.size());
					memcpy(pMeshlets, meshlets.data(), sizeof(Meshlet) * meshlets.size());
					newMeshElement->MeshletResource = GGI()->CreateStaticBuffer(GPUBufferType::Generic, meshShaderResource);
				}
				{
					auto meshShaderResource = std::make_shared< ArrayResource >();
					auto puniqueVertexIndices = meshShaderResource->InitializeFromType<Meshlet>(DivRoundUp(uniqueVertexIndices.size(), 4));
					memcpy(puniqueVertexIndices, uniqueVertexIndices.data(), sizeof(uint8_t) * uniqueVertexIndices.size());
					newMeshElement->UniqueVertexIndexResource = GGI()->CreateStaticBuffer(GPUBufferType::Generic, meshShaderResource);
				}
				{
					auto meshShaderResource = std::make_shared< ArrayResource >();
					auto pprimitiveIndices = meshShaderResource->InitializeFromType<PackedTriangle>(primitiveIndices.size());
					memcpy(pprimitiveIndices, primitiveIndices.data(), sizeof(PackedTriangle) * primitiveIndices.size());
					newMeshElement->PrimitiveIndexResource = GGI()->CreateStaticBuffer(GPUBufferType::Generic, meshShaderResource);
				}

				newMeshElement->VertexResource = GGI()->CreateStaticBuffer(GPUBufferType::Vertex, curLayer.VertexResource);
				newMeshElement->IndexResource = GGI()->CreateStaticBuffer(GPUBufferType::Index, curLayer.IndexResource);

				_elements.push_back(newMeshElement);
				//RegisterMeshElement(newMeshElement);
			}

//			Assimp::Importer importer;
//			const aiScene* scene = importer.ReadFile(*FileName, GeneicStaticImportFlags);
//
//			uint32_t MeshCount = scene ? scene->mNumMeshes : 0;
//			outCachedAsset << MeshCount;
//
//			if (scene && scene->HasMeshes())
//			{
//				for (uint32_t Iter = 0; Iter < scene->mNumMeshes; Iter++)
//				{
//					AABB meshBounds;
//
//					//just grab the first
//					auto mesh = scene->mMeshes[Iter];
//
//					auto verticesResource = std::make_shared< ArrayResource >();
//					{
//						auto pvertices = verticesResource->InitializeFromType< MeshVertex>(mesh->mNumVertices);
//						for (size_t i = 0; i < mesh->mNumVertices; ++i)
//						{
//							MeshVertex& vertex = pvertices[i];
//							vertex.position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
//
//							meshBounds += vertex.position;
//
//							if (mesh->HasNormals())
//							{
//								vertex.normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
//							}
//							if (mesh->HasTangentsAndBitangents()) {
//								vertex.tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
//								vertex.bitangent = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
//							}
//							if (mesh->HasTextureCoords(0)) {
//								vertex.texcoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
//							}
//						}
//					}
//
//					auto indicesResource = std::make_shared< ArrayResource >();
//					{
//						auto pindices = indicesResource->InitializeFromType<uint32_t>(mesh->mNumFaces * 3);
//						for (size_t i = 0; i < mesh->mNumFaces; ++i)
//						{
//							SE_ASSERT(mesh->mFaces[i].mNumIndices == 3);
//
//							pindices[i * 3 + 0] = mesh->mFaces[i].mIndices[0];
//							pindices[i * 3 + 1] = mesh->mFaces[i].mIndices[1];
//							pindices[i * 3 + 2] = mesh->mFaces[i].mIndices[2];
//						}
//					}
//
//					SPP_LOG(LOG_MESH, LOG_INFO, " - verts %d", verticesResource->GetElementCount());
//					SPP_LOG(LOG_MESH, LOG_INFO, " - indices %d tris %d", indicesResource->GetElementCount(), indicesResource->GetElementCount() / 3);
//
//#if 0
//					{
//						Simplify::FastQuadricMeshSimplification meshSimplification(std::make_shared< MeshVertexTranslator >(*verticesResource, *indicesResource));
//						meshSimplification.simplify_mesh(mesh->mNumFaces * 0.5, 7.0, true);
//					}
//
//					SPP_LOG(LOG_MESH, LOG_INFO, " - REDUCED verts %d", verticesResource->GetElementCount());
//					SPP_LOG(LOG_MESH, LOG_INFO, " - REDUCED indices %d tris %d", indicesResource->GetElementCount(), indicesResource->GetElementCount() / 3);
//#endif
//
//					TSpan<MeshVertex> pvertices = verticesResource->GetSpan<MeshVertex>();
//					TSpan<uint32_t> pindices = indicesResource->GetSpan<uint32_t>();
//					auto TotalIndexCount = indicesResource->GetElementCount();
//
//					uint32_t MeshletMaxVerts = 64;
//					uint32_t MeshletMaxPrims = 126;
//
//					std::vector<Subset>                     m_indexSubsets;
//					m_indexSubsets.push_back(Subset{ 0, (uint32_t)TotalIndexCount });
//
//					std::vector<Subset>                     meshletSubsets;
//					std::vector<Meshlet>                    meshlets;
//					std::vector<uint8_t>                    uniqueVertexIndices;
//					std::vector<PackedTriangle>             primitiveIndices;
//
//					//std::vector<CullData>                   m_cullData;
//					Meshletizer  tizing(std::make_shared< MeshVertexTranslator >(verticesResource, indicesResource), MeshletMaxVerts, MeshletMaxPrims);
//					tizing.ComputeMeshlets(
//						meshletSubsets,
//						meshlets,
//						uniqueVertexIndices,
//						primitiveIndices);
//
//					// Meshletize our mesh and generate per-meshlet culling data
//					//ComputeMeshlets<MeshVertex, uint32_t>(
//					//	MeshletMaxVerts, MeshletMaxPrims,
//					//	pindices, TotalIndexCount,
//					//	m_indexSubsets.data(), static_cast<uint32_t>(m_indexSubsets.size()),
//					//	pvertices, pvertices.GetCount(),
//					//	meshletSubsets,
//					//	meshlets,
//					//	uniqueVertexIndices,
//					//	primitiveIndices
//					//	);
//
//					SPP_LOG(LOG_MESH, LOG_INFO, " - meshlets created %d", meshlets.size());
//
//					auto newMeshElement = std::make_shared<MeshElement>();
//					std::swap(newMeshElement->MeshletSubsets, meshletSubsets);
//					newMeshElement->Bounds = meshBounds;
//
//					outCachedAsset << newMeshElement->Bounds;
//					outCachedAsset << newMeshElement->MeshletSubsets;
//
//					{
//						auto meshShaderResource = std::make_shared< ArrayResource >();
//						auto pMeshlets = meshShaderResource->InitializeFromType<Meshlet>(meshlets.size());
//						memcpy(pMeshlets, meshlets.data(), sizeof(Meshlet) * meshlets.size());
//						newMeshElement->MeshletResource = CreateStaticBuffer(GPUBufferType::Generic, meshShaderResource);
//						outCachedAsset << *meshShaderResource;
//					}
//					{
//						auto meshShaderResource = std::make_shared< ArrayResource >();
//						auto puniqueVertexIndices = meshShaderResource->InitializeFromType<Meshlet>(DivRoundUp(uniqueVertexIndices.size(), 4));
//						memcpy(puniqueVertexIndices, uniqueVertexIndices.data(), sizeof(uint8_t) * uniqueVertexIndices.size());
//						newMeshElement->UniqueVertexIndexResource = CreateStaticBuffer(GPUBufferType::Generic, meshShaderResource);
//						outCachedAsset << *meshShaderResource;
//					}
//					{
//						auto meshShaderResource = std::make_shared< ArrayResource >();
//						auto pprimitiveIndices = meshShaderResource->InitializeFromType<PackedTriangle>(primitiveIndices.size());
//						memcpy(pprimitiveIndices, primitiveIndices.data(), sizeof(PackedTriangle) * primitiveIndices.size());
//						newMeshElement->PrimitiveIndexResource = CreateStaticBuffer(GPUBufferType::Generic, meshShaderResource);
//						outCachedAsset << *meshShaderResource;
//					}
//
//					newMeshElement->VertexResource = CreateStaticBuffer(GPUBufferType::Vertex, verticesResource);
//					newMeshElement->IndexResource = CreateStaticBuffer(GPUBufferType::Index, indicesResource);
//
//					outCachedAsset << *verticesResource;
//					outCachedAsset << *indicesResource;

				//	_elements.push_back(newMeshElement);

				//	RegisterMeshElement(newMeshElement);
				//}
			//}
			//else
			//{
			//	return false;
			//}

			PutCachedAsset(FileName, outCachedAsset);
		}

		return true;
	}

}