"""Source/contract checks only. These do not replace Windows shader/GPU tests."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
BRIDGE = ROOT / 'engine/core/modules/mgpu/src/Bridge/dof_bridge.cpp'
GRAPH = ROOT / 'engine/core/modules/graphics/src/render_window/render_graph.cpp'
CONTENT = ROOT / 'samples/sceneBase/content'


class DofIntegrationTests(unittest.TestCase):
    def test_secondary_pass_bindings_match_hlsl(self):
        shader = (CONTENT / 'shaders/src/dof_pipeline.hlsl').read_text()
        bridge = BRIDGE.read_text()
        plan = re.findall(r'\{"DOF_PASS_(\w+)", \{([^}]+)\}, (\w+)\}', bridge)
        self.assertEqual([p[0] for p in plan], [
            'COC', 'HOLES', 'PYRAMID1', 'PYRAMID2', 'PATCH2', 'PATCH1', 'PATCH0', 'WMAX', 'WFILTER'])
        expected_inputs = {
            'COC': ['Depth'], 'HOLES': ['Depth'], 'PYRAMID1': ['Color', 'Mask'],
            'PYRAMID2': ['Pyramid1'], 'PATCH2': ['Color', 'Mask'],
            'PATCH1': ['Color', 'Pyramid2', 'Mask'], 'PATCH0': ['Color', 'Pyramid1', 'Mask'],
            'WMAX': ['Mask', 'Coc'], 'WFILTER': ['Wmax']}
        for name, inputs, output in plan:
            with self.subTest(pass_name=name):
                block = re.search(r'#(?:if|elif) DOF_PASS_' + name + r'\b(.*?)(?=#elif|#endif)', shader, re.S)[1]
                actual = [s.strip() for s in inputs.split(',') if s.strip() != '-1']
                self.assertEqual(actual, expected_inputs[name])
                slots = [int(x) for x in re.findall(r'register\(t(\d+)\)', block)]
                self.assertEqual(slots, list(range(len(actual))))
                self.assertEqual(re.findall(r'register\(u(\d+)\)', block), ['0'])
                constants = re.search(r'cbuffer\s+\w+.*?\{(.*?)\};', block, re.S)
                if constants:
                    self.assertLessEqual(len(re.findall(r'\bfloat\s+\w+;', constants[1])), 8)

    def test_graph_routes_and_fallback(self):
        graph = GRAPH.read_text()
        fallback = graph.split('// Local GPU1-equivalent block', 1)[1].split('// GPU 0 gather', 1)[0]
        self.assertEqual(fallback.count('if (!m_depthOfFieldEnabled || m_dofMultiGpuUsed) return;'), 9)
        self.assertEqual(graph.count('.renameTexture('), 2)
        self.assertIn('m_dofMultiGpuUsed ? multiFilled.get() : colorFilled.get()', graph)
        self.assertEqual(graph.count('m_dofMultiGpuUsed ? multiFiltered.get() : filtered.get()'), 2)
        self.assertIn('createMgpuNodes();', graph)
        self.assertIn('DRV3D_COMMAND_PREPARE_TEXTURE_FOR_EXTERNAL_COPY', graph)
        self.assertLess(graph.index('d3d::finish_render_commands();'), graph.index('m_dofBridge.execute('))

    def test_no_cpu_pixel_transport(self):
        bridge = BRIDGE.read_text()
        self.assertNotIn('D3D12_HEAP_TYPE_READBACK', bridge)
        self.assertNotIn('D3D12_HEAP_TYPE_UPLOAD', bridge)
        self.assertNotIn('->Map(', bridge)
        self.assertIn('D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER', bridge)
        self.assertIn('queue->GetD3D12CommandQueue()->Wait(secondaryFence.Get(), serial)', bridge)
        self.assertIn('primaryQueue->Wait(primaryFence.Get(), serial)', bridge)

    def test_asset_copies_and_runtime_shader(self):
        for rel in ['shaders/src/dof_pipeline.hlsl', 'shaders/meta/dof_pipeline.blk',
                    'materials/pp_depth_of_field_compute.nmat_json',
                    'shaders/src/cs_panorama_to_cubemap.hlsl', 'shaders/src/cs_gen_irradiance_map.hlsl',
                    'shaders/src/cs_gen_reflection_map.hlsl']:
            self.assertEqual((CONTENT / rel).read_bytes(),
                             (ROOT / 'samples/sceneBase/resources' / rel).read_bytes())
        cmake = (ROOT / 'samples/sceneBase/CMakeLists.txt').read_text()
        self.assertIn('content/shaders/src/dof_pipeline.hlsl', cmake)
        self.assertIn('/Shaders/DOF/dof_pipeline.hlsl', cmake)

    def test_native_transition_is_forced(self):
        driver = (ROOT / 'engine/core/modules/render/src/drv3d_DX12/dx12.cpp').read_text()
        block = driver.split('case DRV3D_COMMAND_PREPARE_TEXTURE_FOR_EXTERNAL_COPY:', 1)[1].split('case ', 1)[0]
        self.assertIn('GpuPipeline::GRAPHICS, true', block)
        self.assertIn('RB_RW_COPY_DEST : RB_RO_COPY_SOURCE', block)

    def test_single_adapter_preflight_precedes_module_factory(self):
        bridge = BRIDGE.read_text()
        start = bridge.index('void initialize(')
        block = bridge[start:bridge.index('void resize(', start)]
        self.assertLess(block.index('if (!hasSecondary)'), block.index('GDeviceFactory::GetAllDevices'))
        self.assertLess(block.index('if (!hasSecondary)'), block.index('GDeviceFactory::SetDebugLayerEnabled'))
        self.assertIn('DXGI_ADAPTER_FLAG_SOFTWARE', block)
        self.assertIn('desc.AdapterLuid.HighPart == luid.HighPart', block)


if __name__ == '__main__':
    unittest.main()
