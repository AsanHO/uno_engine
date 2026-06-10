#pragma once
#pragma once

#include "ModelLoader.h"

// vcpkg install assimp:x64-windows
// Preprocessor definitions¿¡ NOMINMAX Ãß°¡
#include <assimp\Importer.hpp>
#include <assimp\postprocess.h>
#include <assimp\scene.h>
#include <iostream>
#include <string>
#include <vector>

#include "MeshData.h"
#include "Vertex.h"

namespace hlab {
class ModelLoader {
  public:
    void Load(std::string basePath, std::string filename);

    void ProcessNode(aiNode *node, const aiScene *scene, DirectX::SimpleMath::Matrix tr);

    MeshData ProcessMesh(aiMesh *mesh, const aiScene *scene);

    std::string ReadFilename(aiMaterial *material, aiTextureType type);

    void UpdateTangents();

  public:
    std::string basePath;
    std::vector<MeshData> meshes;
    bool m_isGLTF = false; // gltf or fbx
};
} // namespace hlab