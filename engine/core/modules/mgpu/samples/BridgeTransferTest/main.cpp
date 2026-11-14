// Console check of the Mgpu <-> Nau bridges (no Nau involved).
// 1. Transfer: GPU2 draws a sine gradient (compute shader) -> shared heap -> GPU1 -> CPU; every frame the received
//    pixels are compared bit-exactly with what GPU2 produced.
// 2. Shadow cascades: GPU2 draws a small two-cascade depth atlas with known depths (static, instanced and skinned
//    casters) -> shared heap -> GPU1 -> CPU; the depths are checked at known points.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "nau/mgpu/bridge/mgpu_bridge.h"
#include "nau/mgpu/bridge/mgpu_shadows.h"

namespace
{
    // Column-major 4x4 (Nau memory layout) that moves along z.
    std::vector<float> translationZ(const float z)
    {
        std::vector<float> m(16, 0.0f);
        m[0] = m[5] = m[10] = m[15] = 1.0f;
        m[14] = z;
        return m;
    }

    // Quad [-size, size]^2 at z = 0, both windings, so the test does not depend on the cull mode.
    void makeQuad(const float size, std::vector<float>& positions, std::vector<uint32_t>& indices)
    {
        positions = {-size, -size, 0.0f, size, -size, 0.0f, size, size, 0.0f, -size, size, 0.0f};
        indices = {0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2};
    }

    bool runShadowTest()
    {
        using namespace nau::mgpu::shadows;

        CascadeRenderer renderer;
        if (!renderer.initialize(nullptr))
        {
            std::printf("Shadow renderer initialization failed: %s\n", renderer.lastError());
            return false;
        }

        std::vector<float> positions;
        std::vector<uint32_t> indices32;

        // Mesh 1: static, 16-bit indices, half of the cascade viewport.
        makeQuad(0.5f, positions, indices32);
        const std::vector<uint16_t> indices16(indices32.begin(), indices32.end());
        MeshDesc small;
        small.positions = positions.data();
        small.vertexCount = 4;
        small.indices = indices16.data();
        small.indexCount = static_cast<uint32_t>(indices16.size());
        bool registered = renderer.registerMesh(1, small);

        // Mesh 2: static, 32-bit indices, the whole cascade viewport.
        std::vector<float> fullPositions;
        std::vector<uint32_t> fullIndices;
        makeQuad(1.0f, fullPositions, fullIndices);
        MeshDesc full;
        full.positions = fullPositions.data();
        full.vertexCount = 4;
        full.indices = fullIndices.data();
        full.indexCount = static_cast<uint32_t>(fullIndices.size());
        full.indices32 = true;
        registered = renderer.registerMesh(2, full) && registered;

        // Mesh 3: skinned, every vertex bound to bone 0.
        std::vector<float> tinyPositions;
        std::vector<uint32_t> tinyIndices;
        makeQuad(0.25f, tinyPositions, tinyIndices);
        const std::vector<float> weights = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
        const std::vector<uint32_t> boneIndices(16, 0);
        MeshDesc tiny;
        tiny.positions = tinyPositions.data();
        tiny.vertexCount = 4;
        tiny.indices = tinyIndices.data();
        tiny.indexCount = static_cast<uint32_t>(tinyIndices.size());
        tiny.indices32 = true;
        tiny.boneWeights = weights.data();
        tiny.boneIndices = boneIndices.data();
        registered = renderer.registerMesh(3, tiny) && registered;

        if (!registered)
        {
            std::printf("Shadow mesh registration failed: %s\n", renderer.lastError());
            return false;
        }

        // Two 128x128 cascades side by side; identity view-projection, so depth = z of the caster.
        CascadeDesc cascades[2];
        for (int c = 0; c < 2; ++c)
        {
            const auto identity = translationZ(0.0f);
            std::copy(identity.begin(), identity.end(), cascades[c].viewProj);
            cascades[c].viewport[0] = c * 128;
            cascades[c].viewport[1] = 0;
            cascades[c].viewport[2] = 128;
            cascades[c].viewport[3] = 128;
        }

        // matrices: [0] small quad at z 0.25, [1], [2] two instances of the full quad at z 0.5 and 0.75, [3] bone 0 at z 0.4.
        std::vector<float> matrices;
        for (const float z : {0.25f, 0.5f, 0.75f, 0.4f})
        {
            const auto m = translationZ(z);
            matrices.insert(matrices.end(), m.begin(), m.end());
        }

        DrawDesc draws[3];
        draws[0] = {1, 0, 0, 12, 0, 1, 0};
        draws[1] = {2, 1, 0, 12, 1, 2, 0};
        draws[2] = {3, 1, 0, 12, 3, 1, 1};

        FrameDesc frame;
        frame.atlasWidth = 256;
        frame.atlasHeight = 128;
        frame.format = AtlasFormat::Depth16;
        frame.cascades = cascades;
        frame.cascadeCount = 2;
        frame.draws = draws;
        frame.drawCount = 3;
        frame.matrices = matrices.data();
        frame.matrixCount = static_cast<uint32_t>(matrices.size() / 16);
        frame.userToken = 42;

        if (!renderer.submit(frame))
        {
            std::printf("Shadow submit failed: %s\n", renderer.lastError());
            return false;
        }

        std::vector<float> depths;
        uint32_t width = 0;
        uint32_t height = 0;
        if (!renderer.waitAndReadBack(depths, width, height))
        {
            std::printf("Shadow read-back failed: %s\n", renderer.lastError());
            return false;
        }

        struct Probe
        {
            uint32_t x, y;
            float expected;
            const char* what;
        };
        const Probe probes[] = {
            {64, 64, 0.25f, "cascade 0: static caster"},
            {5, 5, 1.0f, "cascade 0: cleared"},
            {128 + 64, 64, 0.4f, "cascade 1: skinned caster"},
            {128 + 10, 10, 0.5f, "cascade 1: nearest of two instances"},
        };

        bool ok = true;
        for (const Probe& probe : probes)
        {
            const float value = depths[static_cast<size_t>(probe.y) * width + probe.x];
            const bool match = std::fabs(value - probe.expected) < 1e-3f;
            ok = ok && match;
            std::printf("shadow atlas (%3u,%3u) %-36s depth %.4f, expected %.4f: %s\n", probe.x, probe.y, probe.what,
                        value, probe.expected, match ? "OK" : "FAILED");
        }

        const RendererStats& stats = renderer.stats();
        std::printf("shadow atlas %ux%u, %u draws, %.2f ms on the second GPU\n", width, height, stats.lastDrawCount,
                    stats.lastLatencyMs);
        renderer.shutdown();
        return ok;
    }
}

int main(int argc, char** argv)
{
    const uint32_t iterations = argc > 1 ? static_cast<uint32_t>(std::atoi(argv[1])) : 120;

    nau::mgpu::bridge::TransferBridge bridge;

    // nullptr: standalone process, the first hardware adapter becomes the primary one.
    if (!bridge.initialize(nullptr))
    {
        std::printf("Bridge initialization failed: %s\n", bridge.lastError());
        return 2;
    }

    std::printf("Primary adapter  : %s\n", bridge.primaryAdapterName());
    std::printf("Secondary adapter: %s\n", bridge.secondaryAdapterName());

    uint32_t failed = 0;
    for (uint32_t frame = 0; frame < iterations; ++frame)
    {
        if (!bridge.update(frame))
        {
            ++failed;
            std::printf("frame %u: FAILED (%s)\n", frame, bridge.lastError());
        }
        else if (frame % 30 == 0)
        {
            const auto& info = bridge.info();
            std::printf("frame %u: OK, %ux%u, checksum %016llx\n", frame, info.width, info.height,
                        static_cast<unsigned long long>(info.checksum));
        }
    }

    std::printf("%u of %u transfers failed\n", failed, iterations);
    bridge.shutdown();

    const bool shadowsOk = runShadowTest();
    std::printf("shadow cascades: %s\n", shadowsOk ? "OK" : "FAILED");

    return failed == 0 && shadowsOk ? 0 : 1;
}
