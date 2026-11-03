// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#pragma once

#include "nau/animation/components/skeleton_component.h"
#include "nau/scene/components/billboard_component.h"
#include "nau/scene/components/skinned_mesh_component.h"
#include "nau/scene/components/static_mesh_component.h"
#include "nau/scene/scene_factory.h"

namespace nau::test
{
    struct CameraPreviewMarkers
    {
        Uid staticUid;
        Uid skinnedUid;
        Uid billboardUid;
        scene::ObjectWeakRef<scene::BillboardComponent> billboard;
    };

    inline CameraPreviewMarkers addCameraPreviewMarkers(scene::ISceneFactory& factory, scene::SceneObject& root)
    {
        auto& cube = root.attachChild(factory.createSceneObject<scene::StaticMeshComponent>())
                         .getRootComponent<scene::StaticMeshComponent>();
        cube.setMeshGeometry(StaticMeshAssetRef{"file:/content/scenes/scene_demo.gltf+[mesh/0]"});
        cube.setScale({0.48f, 0.48f, 0.48f});

        auto& robot = root.attachChild(factory.createSceneObject<SkinnedMeshComponent>());
        auto& mesh = robot.getRootComponent<SkinnedMeshComponent>();
        mesh.setMeshGeometry(SkinnedMeshAssetRef{"file:/content/scenes/robot/robot_skeletal_pbr.gltf+[mesh/0]"});
        mesh.setMaterial(MaterialAssetRef{"file:/res/materials/skinned_robot.nmat_inst_json"});
        // The robot rig uses centimetres. Face it towards the camera and keep its bind pose
        // for deterministic comparisons between captured frames.
        mesh.setScale({0.01f, 0.01f, 0.01f});
        mesh.setRotation({0.f, 0.70710678f, 0.f, 0.70710678f});
        mesh.setTranslation({1.15f, 0.f, 0.f});
        robot.addComponent<SkeletonComponent>().setSkeletonAsset(SkeletonAssetRef{
            "file:/content/scenes/robot/robot_skeletal_pbr.gltf+[skin/0//content/scenes/robot/robot_pbr_skeleton.ozz]"});

        auto& billboard = root.attachChild(factory.createSceneObject<scene::BillboardComponent>())
                             .getRootComponent<scene::BillboardComponent>();
        billboard.setTranslation({-1.15f, 0.4f, 0.f});
        // Pink and lilac quadrants expose an accidental second sRGB conversion.
        billboard.setTextureRef(TextureAssetRef{"file:/content/textures/default_albedo_8x8.png"});
        billboard.setScreenPercentageSize(0.07f);

        return {cube.getUid(), mesh.getUid(), billboard.getUid(), billboard};
    }
}  // namespace nau::test
