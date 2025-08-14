// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "vfx_mod_fx_instance.h"

#include "nau/shaders/shader_globals.h"

#include "math/vfx_random.h"

#include "modfx/emitter/emitter_utils.h"

#include "modfx/modfx_sim.h"

#include "modfx/modfx_life.h"
#include "modfx/modfx_radius.h"
#include "modfx/modfx_velocity.h"
#include "modfx/modfx_color.h"
#include "modfx/modfx_texture.h"
#include "modfx/modfx_position.h"

namespace nau::vfx::modfx
{
    VFXModFXInstance::VFXModFXInstance(const nau::MaterialAssetView::Ptr material)
        : m_positionBuffer(nullptr)
        , m_normalBuffer(nullptr)
        , m_texCoordBuffer(nullptr)
        , m_quadIndexBuffer(nullptr)
        , m_instanceData(nullptr)
        , m_material(material)
        , m_actualParticlePoolSize(0)
        , m_transform(nau::math::Matrix4::identity())
        , m_offset(nau::math::Vector3::zero())
        , m_isPause(false)
    {
        prepareQuadBuffer();
        prepareInstanceBuffer();

        m_particlePool.reserve(PoolSizeMultiplier * MaxParticleCount);
        m_instanceData.reserve(PoolSizeMultiplier * MaxParticleCount);
    }

    void VFXModFXInstance::serialize(nau::DataBlock* blk) const
    {
        if (!blk)
        {
            return;
        }

        blk->addPoint3("translation", m_transform.getTranslation());

        auto* spawnBlock = blk->addNewBlock("spawn");
        m_spawn.save(spawnBlock);

        auto* lifeBlock = blk->addNewBlock("life");
        m_life.save(lifeBlock);

        auto* positionBlock = blk->addNewBlock("position");
        m_position.save(positionBlock);

        auto* radiusBlock = blk->addNewBlock("radius");
        m_radius.save(radiusBlock);

        auto* rotationBlock = blk->addNewBlock("rotation");
        m_rotation.save(rotationBlock);

        auto* velocityBlock = blk->addNewBlock("velocity");
        m_velocity.save(velocityBlock);

        auto* textureBlock = blk->addNewBlock("texture");
        m_texture.save(textureBlock);
    }

    bool VFXModFXInstance::deserialize(const nau::DataBlock* blk)
    {
        if (!blk)
        {
            return false;
        }

        // Spawn
        m_spawn.type = static_cast<EmitterType>(blk->getInt("spawnType", 2));
        m_spawn.linear.count_min = blk->getInt("linearParticleCountMin", 1);
        m_spawn.linear.count_max = blk->getInt("linearParticleCountMax", 1);

        m_spawn.burst.count_min = blk->getInt("burstParticleCountMin", 0);
        m_spawn.burst.count_max = blk->getInt("burstParticleCountMax", 0);
        m_spawn.burst.cycles = blk->getInt("cycles", 0);
        m_spawn.burst.period = blk->getReal("period", 0.0f);

        m_spawn.fixed.count = blk->getInt("fixedParticleCount", 0);
        //

        // Life
        m_life.part_life_min = blk->getReal("lifeMin", 5.0f);
        m_life.part_life_max = blk->getReal("lifeMax", 5.0f);
        m_life.part_life_rnd_offset = blk->getReal("rndOffset", 0.0f);
        m_life.inst_life_delay = blk->getReal("delay", 0.0f);
        //

        // Position
        m_position.type = static_cast<settings::PositionType>(blk->getInt("positionType", 1));

        m_position.enabled = blk->getBool("positionEnabled", false);
        m_position.volume = blk->getReal("positionVolume", 0.0f);
        m_position.offset = blk->getPoint3("positionVolume", nau::math::Vector3::zero());
        
        m_position.sphere.volume = blk->getReal("sphereVolume", 0.0f);
        m_position.sphere.radius = blk->getReal("sphereRadius", 0.0f);

        m_position.cylinder.vec = blk->getPoint3("cylinderVec", nau::math::Vector3::zero());
        m_position.cylinder.volume = blk->getReal("cylinderVolume", 0.0f);
        m_position.cylinder.radius = blk->getReal("cylinderRadius", 0.0f);
        m_position.cylinder.height = blk->getReal("cylinderHeight", 0.0f);
        m_position.cylinder.random_burst = blk->getReal("cylinderRandomBurst", 0.0f);
        
        m_position.cone.vec = blk->getPoint3("coneVec", nau::math::Vector3::zero());
        m_position.cone.volume = blk->getReal("coneVolume", 0.0f);
        m_position.cone.width_bottom = blk->getReal("coneWidthBottom", 0.0f);
        m_position.cone.height = blk->getReal("coneHeight", 0.0f);
        m_position.cone.random_burst = blk->getReal("coneRandomBurst", 0.0f);
        
        m_position.box.volume = blk->getReal("boxVolume", 0.0f);
        m_position.box.width = blk->getReal("boxWidth", 0.0f);
        m_position.box.height = blk->getReal("boxHeight", 0.0f);
        m_position.box.depth = blk->getReal("boxDepth", 0.0f);
        //

        // Rotation
        m_rotation.enabled = blk->getBool("rotationEnabled", false);
        m_rotation.start_min = blk->getReal("startAngleMin", 0.0f);
        m_rotation.start_max = blk->getReal("startAngleMax", 0.0f);
        //

        // Radius
        m_radius.enabled = blk->getBool("radiusEnabled", true);
        m_radius.rad_min = blk->getReal("radiusMin", 1.0f);
        m_radius.rad_max = blk->getReal("radiusmax", 1.0f);
        //

        // Velocity
        m_velocity.enabled = blk->getBool("velocityEnabled", false);

        m_velocity.mass = blk->getReal("mass", 0.0f);
        m_velocity.drag_coeff = blk->getReal("dragCoeff", 0.0f);
        m_velocity.drag_to_rad_k = blk->getReal("dragToRadK", 0.0f);

        m_velocity.apply_gravity = blk->getBool("applyGravity", false);
        m_velocity.gravity_transform = blk->getBool("gravityTransform", false);
        m_velocity.apply_parent_velocity = blk->getBool("applyParentVelocity", false);

        m_velocity.start.enabled = blk->getBool("velocityStartEnabled", false);
        m_velocity.start.vel_min = blk->getReal("velocityStartMin", 0.0f);
        m_velocity.start.vel_max = blk->getReal("velocityStartMax", 0.0f);
        m_velocity.start.vec_rnd = blk->getReal("velocityStartVecRnd", 0.0f);

        m_velocity.start.type = static_cast<settings::StartType>(blk->getInt("velocityStartType", 0));
        m_velocity.start.point.offset = blk->getPoint3("velocityStartFxInitVelocityPoint", nau::math::Vector3::zero());
        m_velocity.start.vec.vec = blk->getPoint3("velocityStartFxInitVelocityVec", nau::math::Vector3(0.0f, 1.0f, 0.0f));
        
        m_velocity.add.enabled = blk->getBool("velocityAddEnabled", false);
        m_velocity.add.vel_min = blk->getReal("velocityAddMin", 0.0f);
        m_velocity.add.vel_min = blk->getReal("velocityAddMax", 0.0f);
        m_velocity.add.vec_rnd = blk->getReal("velocityAddVecRnd", 0.0f);
        m_velocity.add.type = static_cast<settings::AddType>(blk->getInt("velocityAddType", 0));
        
        m_velocity.add.point.offset = blk->getPoint3("velocityAddFxInitVelocityPoint", nau::math::Vector3::zero());
        m_velocity.add.vec.vec = blk->getPoint3("velocityAddFxInitVelocityVec", nau::math::Vector3::zero());
        
        m_velocity.add.cone.vec = blk->getPoint3("fxInitVelocityConeVec", nau::math::Vector3::zero());
        m_velocity.add.cone.offset = blk->getPoint3("fxInitVelocityConeOffset", nau::math::Vector3::zero());
        m_velocity.add.cone.width_top = blk->getReal("fxInitVelocityConeWidthTop", 0.0f);
        m_velocity.add.cone.width_bottom = blk->getReal("fxInitVelocityConeWidthBottom", 0.0f);
        m_velocity.add.cone.center_power = blk->getReal("fxInitVelocityConeCenterPower", 0.0f);
        m_velocity.add.cone.border_power = blk->getReal("fxInitVelocityConeBorderPower", 0.0f);
        
        m_velocity.force_field.vortex.enabled = blk->getBool("fxForceFieldVortexEnabled", false);
        m_velocity.force_field.vortex.axis_direction = blk->getPoint3("fxForceFieldVortexAxisDirection", nau::math::Vector3::zero());
        m_velocity.force_field.vortex.direction_rnd = blk->getReal("fxForceFieldVortexDirectionRnd", 0.0f);
        m_velocity.force_field.vortex.axis_position = blk->getPoint3("fxForceFieldVortexAxisPosition", nau::math::Vector3::zero());
        m_velocity.force_field.vortex.position_rnd = blk->getPoint3("fxForceFieldVortexPositionRnd", nau::math::Vector3::zero());
        m_velocity.force_field.vortex.rotation_speed_min = blk->getReal("fxForceFieldVortexRotationSpeedMin", 0.2f);
        m_velocity.force_field.vortex.rotation_speed_max = blk->getReal("fxForceFieldVortexRotationSpeedMax", 0.2f);
        m_velocity.force_field.vortex.pull_speed_max = blk->getReal("fxForceFieldVortexPullSpeedMin", 0.2f);
        m_velocity.force_field.vortex.pull_speed_max = blk->getReal("fxForceFieldVortexPullSpeedMax", 0.2f);

        m_velocity.force_field.noise.enabled = blk->getBool("fxForceFieldNoiseEnabled", false);
        m_velocity.force_field.noise.type = static_cast<settings::ForceFieldNoiseType>(blk->getInt("forceFieldNoiseType", 0));
        m_velocity.force_field.noise.pos_scale = blk->getReal("FxForceFieldNoisePosScale", 0.0f);
        m_velocity.force_field.noise.power_scale = blk->getReal("FxForceFieldNoisePowerScale", 0.0f);
        m_velocity.force_field.noise.power_rnd = blk->getReal("FxForceFieldNoisePowerRnd", 0.0f);
        m_velocity.force_field.noise.power_per_part_rnd = blk->getReal("FxForceFieldNoisePowerPerPartRnd", 0.0f);

        m_velocity.wind.enabled = blk->getBool("FxWindWindEnabled", false);
        m_velocity.wind.directional_force = blk->getReal("FxWindWindDirectionalForce", 0.0f);
        m_velocity.wind.directional_freq = blk->getReal("FxWindWindDirectionalFreq", 0.0f);
        m_velocity.wind.turbulence_force = blk->getReal("FxWindWindTurbulenceForce", 0.0f);
        m_velocity.wind.turbulence_freq = blk->getReal("FxWindWindTurbulenceFreq", 0.0f);
        m_velocity.wind.impulse_wind = blk->getBool("FxWindWindTurbulenceFreq", false);
        m_velocity.wind.impulse_wind_force = blk->getReal("FxWindWindImpulseWindForce", 0.0f);
        //

        // Texture
        m_texture.enabled = blk->getBool("textureEnabled", false);
        m_texture.frames_y = eastl::max(blk->getInt("columns", 1), 1);
        m_texture.frames_x = eastl::max(blk->getInt("rows", 1), 1);
        //

        // Color
        auto start_color = blk->getE3dcolor("startColor", 0.0f);
        auto end_color = blk->getE3dcolor("startColor", 0.0f);

        m_color.enabled = blk->getBool("colorEnabled", false);
        m_color.start_color = nau::math::Color4(start_color.r, start_color.g, start_color.b, start_color.a);
        m_color.end_color = nau::math::Color4(end_color.r, end_color.g, end_color.b, end_color.a);
        //

        updateSpawnSettings();

        return true;
    }

    void VFXModFXInstance::setSpawnSettings(const settings::FxSpawn& spawn)
    {
        m_spawn = spawn;
        updateSpawnSettings();
    }

    settings::FxSpawn VFXModFXInstance::spawnSettings() const
    {
        return m_spawn;
    }

    void VFXModFXInstance::setLifeSettings(const settings::FxLife& life)
    {
        m_life = life;
        updateSpawnSettings();
    }

    settings::FxLife VFXModFXInstance::lifeSettings() const
    {
        return m_life;
    }

    void VFXModFXInstance::setPositionSettings(const settings::FxPosition& position)
    {
        m_position = position;
    }

    settings::FxPosition VFXModFXInstance::positionSettings() const
    {
        return m_position;
    }

    void VFXModFXInstance::setRadiusSettings(const settings::FxRadius& radius)
    {
        m_radius = radius;
    }

    settings::FxRadius VFXModFXInstance::radiusSettings() const
    {
        return m_radius;
    }

    void VFXModFXInstance::setColorSettings(const settings::FxColor& color)
    {
        m_color = color;
    }

    settings::FxColor VFXModFXInstance::colorSettings() const
    {
        return m_color;
    }

    void VFXModFXInstance::setRotationSettings(const settings::FxRotation& rotation)
    {
        m_rotation = rotation;
    }

    settings::FxRotation VFXModFXInstance::rotationSettings() const
    {
        return m_rotation;
    }

    void VFXModFXInstance::setVelocitySettings(const settings::FxVelocity& velocity)
    {
        m_velocity = velocity;
    }

    settings::FxVelocity VFXModFXInstance::velocitySettings() const
    {
        return m_velocity;
    }

    void VFXModFXInstance::setTextureSettings(const settings::FxTexture& texture)
    {
        m_texture = texture;
    }

    settings::FxTexture VFXModFXInstance::textureSettings() const
    {
        return m_texture;
    }

    void VFXModFXInstance::setTexture(ReloadableAssetView::Ptr assetTexture)
    {
        m_assetTexture = assetTexture;
    }

    void VFXModFXInstance::play()
    {
        m_isPause = false;
    }

    void VFXModFXInstance::stop()
    {
        m_isPause = true;
    }

    void VFXModFXInstance::setTransform(const nau::math::Matrix4& transform)
    {
        m_transform = transform;
        m_offset = m_transform.getTranslation();
    }

    nau::math::Matrix4 VFXModFXInstance::transform() const
    {
        return m_transform;
    }

    void VFXModFXInstance::update(float dt)
    {
        if (m_isPause)
        {
            return;
        }

        int particleToSpawn = emitter_utils::update_emitter(m_emitterState, dt);
        if (particleToSpawn > 0)
        {
            addParticles(particleToSpawn);
        }

        if (m_actualParticlePoolSize != 0)
        {
            simulateParticles(dt);
        }
    }

    void VFXModFXInstance::render(const nau::math::Matrix4& view, const nau::math::Matrix4& projection)
    {
        if (m_actualParticlePoolSize == 0 || !m_assetTexture || !m_material)
        {
            return;
        }

        d3d::set_buffer(STAGE_VS, 1, m_instanceBuffer);

        shader_globals::setVariable("view", &view);
        shader_globals::setVariable("projection", &projection);

        //shader_globals::setVariable("frames_y", &m_texture.frames_y);
        //shader_globals::setVariable("frames_x", &m_texture.frames_x);

        nau::Ptr<TextureAssetView> textureView;
        m_assetTexture->getTyped<TextureAssetView>(textureView);

        m_material->setTexture("default", "tex", textureView->getTexture());

        m_material->bindPipeline("default");
        
        d3d::setvsrc(0, m_positionBuffer, sizeof(nau::math::float3));
        d3d::setvsrc(1, m_normalBuffer, sizeof(nau::math::float3));
        d3d::setvsrc(2, m_texCoordBuffer, sizeof(nau::math::float2));

        d3d::setind(m_quadIndexBuffer);

        d3d::drawind_instanced(PRIM_TRILIST, 0, 6, 0, m_actualParticlePoolSize);
    }

    void VFXModFXInstance::prepareQuadBuffer()
    {
        nau::math::float3 quadPositions[] = {
            {-0.5f, -0.5f, 0.0f},
            { 0.5f, -0.5f, 0.0f},
            { 0.5f,  0.5f, 0.0f},
            {-0.5f,  0.5f, 0.0f}
        };

        nau::math::float3 quadNormals[] = {
            {0, 0,  1},
            {0, 0,  1},
            {0, 0, -1},
            {0, 0, -1}
        };

        nau::math::float2 quadTexCoords[] = {
            {0, 1},
            {1, 1},
            {1, 0},
            {0, 0}
        };

        quadIndices[0] = 0;
        quadIndices[1] = 1;
        quadIndices[2] = 2;
        quadIndices[3] = 2;
        quadIndices[4] = 3;
        quadIndices[5] = 0;

        size_t posBufferSize = sizeof(quadPositions);
        m_positionBuffer = d3d::create_vb(posBufferSize, SBCF_DYNAMIC, u8"PositionBuffer");
        std::byte* posMemory = nullptr;
        m_positionBuffer->lock(0, posBufferSize, reinterpret_cast<void**>(&posMemory), VBLOCK_WRITEONLY);
        std::memcpy(posMemory, quadPositions, posBufferSize);
        m_positionBuffer->unlock();

        size_t nrmBufferSize = sizeof(quadNormals);
        m_normalBuffer = d3d::create_vb(nrmBufferSize, SBCF_DYNAMIC, u8"NormalBuffer");
        std::byte* nrmMemory = nullptr;
        m_normalBuffer->lock(0, nrmBufferSize, reinterpret_cast<void**>(&nrmMemory), VBLOCK_WRITEONLY);
        std::memcpy(nrmMemory, quadNormals, nrmBufferSize);
        m_normalBuffer->unlock();

        size_t texBufferSize = sizeof(quadTexCoords);
        m_texCoordBuffer = d3d::create_vb(texBufferSize, SBCF_DYNAMIC, u8"TexCoordBuffer");
        std::byte* texMemory = nullptr;
        m_texCoordBuffer->lock(0, texBufferSize, reinterpret_cast<void**>(&texMemory), VBLOCK_WRITEONLY);
        std::memcpy(texMemory, quadTexCoords, texBufferSize);
        m_texCoordBuffer->unlock();

        size_t indexBufferSize = sizeof(quadIndices);
        m_quadIndexBuffer = d3d::create_ib(indexBufferSize, SBCF_DYNAMIC, u8"CubeIndexBuffer");
        std::byte* indexMemory = nullptr;
        m_quadIndexBuffer->lock(0, indexBufferSize, reinterpret_cast<void**>(&indexMemory), VBLOCK_WRITEONLY);
        std::memcpy(indexMemory, quadIndices, indexBufferSize);
        m_quadIndexBuffer->unlock();
    }

    void VFXModFXInstance::prepareInstanceBuffer()
    {
        m_instanceBuffer = d3d::create_sbuffer(
            sizeof(InstanceData),
            PoolSizeMultiplier * MaxParticleCount,
            SBCF_MISC_STRUCTURED | SBCF_BIND_SHADER_RES | SBCF_DYNAMIC,
            0,
            u8"VFXInstanceBuffer");

        for (int i = 0; i < PoolSizeMultiplier * MaxParticleCount; ++i)
        {
            m_instanceData.emplace_back(InstanceData{nau::math::Matrix4::identity(), 0, nau::math::Color4(1.0f)});
        }
    }

    void VFXModFXInstance::addParticles(int particleToSpawn)
    {
        for (int i = 0; i < particleToSpawn; ++i)
        {
            if (m_actualParticlePoolSize < PoolSizeMultiplier * MaxParticleCount)
            {
                ModFxData data;
                initializeParticleData(data);
                m_particlePool.push_back(data);

                ++m_actualParticlePoolSize;
            }
            else if (!m_freeIndexPool.empty())
            {
                int freeIndex = *m_freeIndexPool.begin();
                initializeParticleData(m_particlePool[freeIndex]);
                m_freeIndexPool.erase(freeIndex);
            }
            else
            {
                break;
            }
        }
    }

    void VFXModFXInstance::updateSpawnSettings()
    {
        m_emitterData.type = m_spawn.type;
        m_emitterData.linearData.countMin = m_spawn.linear.count_min;
        m_emitterData.linearData.countMax = m_spawn.linear.count_max;
        m_emitterData.linearData.lifeLimit = m_life.part_life_max;

        m_emitterData.burstData.countMin = m_spawn.burst.count_min;
        m_emitterData.burstData.countMax = m_spawn.burst.count_max;
        m_emitterData.burstData.lifeLimit = m_life.part_life_max;

        m_emitterData.burstData.countMin = m_spawn.burst.count_min;
        m_emitterData.burstData.countMax = m_spawn.burst.count_max;
        m_emitterData.burstData.cycles = m_spawn.burst.cycles;
        m_emitterData.burstData.period = m_spawn.burst.period;
        m_emitterData.burstData.lifeLimit = m_life.part_life_max;
        m_emitterData.burstData.elemLimit = MaxParticleCount;

        m_emitterData.fixedData.count = eastl::min<int>(m_spawn.fixed.count, MaxParticleCount);
    
        emitter_utils::create_emitter_state(m_emitterState, m_emitterData, MaxParticleCount, 1.0f);
    }

    void VFXModFXInstance::simulateParticles(float dt)
    {
        for (int i = 0; i < m_actualParticlePoolSize; ++i)
        {
            if (m_particlePool[i].sdata.life_norm < 1.0f)
            {
                sim::modfx_apply_sim(m_particlePool[i].rdata, m_particlePool[i].sdata, dt, m_life, m_radius, m_velocity, m_color, m_texture);
                
                // TODO Multiply in the shader
                m_instanceData[i].worldMatrix = nau::math::Matrix4::translation(m_particlePool[i].rdata.pos + m_offset) * nau::math::Matrix4::scale(nau::math::Vector3(m_particlePool[i].rdata.radius));
                m_instanceData[i].frameID = m_particlePool[i].rdata.frame_idx;
                m_instanceData[i].color = m_particlePool[i].rdata.color;
            }
            else
            {
                m_particlePool[i].sdata.life_norm = 2.0f;
                m_freeIndexPool.insert(i);
            }
        }

        m_instanceBuffer->updateData(0, sizeof(InstanceData) * m_actualParticlePoolSize, m_instanceData.data(), VBLOCK_WRITEONLY | VBLOCK_DISCARD);
    }

    void VFXModFXInstance::initializeParticleData(ModFxData& data)
    {
        data.sdata.clear();
        data.rdata.clear();

        const int seed = PoolSizeMultiplier * MaxParticleCount;
        int gid = rand() % (seed + 1);
        int dispatch_seed = rand() % (seed + 1);
        data.sdata.rnd_seed = vfx::math::dafx_calc_instance_rnd_seed(gid, dispatch_seed);

        life::modfx_life_init(data.sdata.rnd_seed, data.sdata.life_norm, m_life);

        if (m_radius.enabled)
        {
            radius::modfx_radius_init(data.sdata.rnd_seed, data.rdata.radius, m_radius);
        }

        nau::math::Vector3 pos_v = nau::math::Vector3::zero();
        if (m_position.enabled)
        {
            position::modfx_position_init(data.sdata.rnd_seed, dispatch_seed, data.rdata.pos, pos_v, m_position);
        }

        if (m_velocity.enabled)
        {
            velocity::modfx_velocity_init(data.rdata.pos, pos_v, data.sdata.velocity, data.sdata.rnd_seed, m_velocity);
        }

        if (m_color.enabled)
        {
            color::modfx_color_init(data.sdata.rnd_seed, data.rdata.color, m_color);
        }
    }
}  // namespace nau::vfx::modfx
