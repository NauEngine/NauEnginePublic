#if 0
;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Position              0   xyzw        0      POS   float
; TEXCOORD                 0   xy          1     NONE   float
;
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; SV_Target                0   xyzw        0   TARGET   float   xyzw
;
;
; Pipeline Runtime Information:
;
; Pixel Shader
; DepthOutput=0
; SampleFrequency=0
;
;
; Input signature:
;
; Name                 Index             InterpMode DynIdx
; -------------------- ----- ---------------------- ------
; SV_Position              0          noperspective
; TEXCOORD                 0                 linear
;
; Output signature:
;
; Name                 Index             InterpMode DynIdx
; -------------------- ----- ---------------------- ------
; SV_Target                0
;
; Buffer Definitions:
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; smpl                              sampler      NA          NA      S0             s0     1
; tex                               texture     f32          2d      T0             t0     1
;
;
; ViewId state:
;
; Number of inputs: 6, outputs: 4
; Outputs dependent on ViewId: {  }
; Inputs contributing to computation of Outputs:
;   output 0 depends on inputs: { 4, 5 }
;   output 1 depends on inputs: { 4, 5 }
;   output 2 depends on inputs: { 4, 5 }
;   output 3 depends on inputs: { 4, 5 }
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%struct.SamplerState = type { i32 }
%"class.Texture2D<vector<float, 4> >" = type { <4 x float>, %"class.Texture2D<vector<float, 4> >::mips_type" }
%"class.Texture2D<vector<float, 4> >::mips_type" = type { i32 }
%dx.types.Handle = type { i8* }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }
%struct.VsOut = type { <4 x float>, <2 x float> }

@"\01?smpl@@3USamplerState@@A" = external constant %struct.SamplerState, align 4
@"\01?tex@@3V?$Texture2D@V?$vector@M$03@@@@A" = external constant %"class.Texture2D<vector<float, 4> >", align 4

define void @main() {
  %tex_texture_2d = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %smpl_sampler = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 3, i32 0, i32 0, i1 false)  ; CreateHandle(resourceClass,rangeId,index,nonUniformIndex)
  %1 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %2 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 undef)  ; LoadInput(inputSigId,rowIndex,colIndex,gsVertexAxis)
  %3 = call %dx.types.ResRet.f32 @dx.op.sample.f32(i32 60, %dx.types.Handle %tex_texture_2d, %dx.types.Handle %smpl_sampler, float %1, float %2, float undef, float undef, i32 undef, i32 undef, i32 undef, float undef)  ; Sample(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,clamp)
  %4 = extractvalue %dx.types.ResRet.f32 %3, 0
  %5 = extractvalue %dx.types.ResRet.f32 %3, 1
  %6 = extractvalue %dx.types.ResRet.f32 %3, 2
  %7 = extractvalue %dx.types.ResRet.f32 %3, 3
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %4)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %5)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %6)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %7)  ; StoreOutput(outputSigId,rowIndex,colIndex,value)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #0

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.sample.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32, float) #2

; Function Attrs: nounwind readonly
declare %dx.types.Handle @dx.op.createHandle(i32, i8, i32, i32, i1) #2

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind }
attributes #2 = { nounwind readonly }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.resources = !{!4}
!dx.typeAnnotations = !{!10, !19}
!dx.viewIdState = !{!23}
!dx.entryPoints = !{!24}

!0 = !{!"dxc 1.2"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 4}
!3 = !{!"ps", i32 6, i32 0}
!4 = !{!5, null, null, !8}
!5 = !{!6}
!6 = !{i32 0, %"class.Texture2D<vector<float, 4> >"* undef, !"tex", i32 0, i32 0, i32 1, i32 2, i32 0, !7}
!7 = !{i32 0, i32 9}
!8 = !{!9}
!9 = !{i32 0, %struct.SamplerState* undef, !"smpl", i32 0, i32 0, i32 1, i32 0, null}
!10 = !{i32 0, %"class.Texture2D<vector<float, 4> >" undef, !11, %"class.Texture2D<vector<float, 4> >::mips_type" undef, !14, %struct.VsOut undef, !16}
!11 = !{i32 20, !12, !13}
!12 = !{i32 6, !"h", i32 3, i32 0, i32 7, i32 9}
!13 = !{i32 6, !"mips", i32 3, i32 16}
!14 = !{i32 4, !15}
!15 = !{i32 6, !"handle", i32 3, i32 0, i32 7, i32 5}
!16 = !{i32 24, !17, !18}
!17 = !{i32 6, !"pos", i32 3, i32 0, i32 4, !"SV_POSITION", i32 7, i32 9}
!18 = !{i32 6, !"coord", i32 3, i32 16, i32 4, !"TEXCOORD0", i32 7, i32 9}
!19 = !{i32 1, void ()* @main, !20}
!20 = !{!21}
!21 = !{i32 0, !22, !22}
!22 = !{}
!23 = !{[8 x i32] [i32 6, i32 4, i32 0, i32 0, i32 0, i32 0, i32 15, i32 15]}
!24 = !{void ()* @main, !"main", !25, !4, null}
!25 = !{!26, !30, null}
!26 = !{!27, !29}
!27 = !{i32 0, !"SV_Position", i8 9, i8 3, !28, i8 4, i32 1, i8 4, i32 0, i8 0, null}
!28 = !{i32 0}
!29 = !{i32 1, !"TEXCOORD", i8 9, i8 0, !28, i8 2, i32 1, i8 2, i32 1, i8 0, null}
!30 = !{!31}
!31 = !{i32 0, !"SV_Target", i8 9, i8 16, !28, i8 0, i32 1, i8 4, i32 0, i8 0, null}

#endif

const unsigned char g_main[] = {0x44, 0x58, 0x42, 0x43, 0x5f, 0x31, 0xf3, 0x11, 0xca, 0x92, 0xab, 0xf9, 0xdb, 0x8e, 0x31, 0x74, 0x45,
  0x49, 0x48, 0xc1, 0x01, 0x00, 0x00, 0x00, 0x77, 0x09, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x44, 0x00, 0x00,
  0x00, 0xa9, 0x00, 0x00, 0x00, 0xe3, 0x00, 0x00, 0x00, 0xa7, 0x01, 0x00, 0x00, 0x53, 0x46, 0x49, 0x30, 0x08, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49, 0x53, 0x47, 0x31, 0x5d, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x53,
  0x56, 0x5f, 0x50, 0x6f, 0x73, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x00, 0x54, 0x45, 0x58, 0x43, 0x4f, 0x4f, 0x52, 0x44, 0x00, 0x4f, 0x53,
  0x47, 0x31, 0x32, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x53, 0x56, 0x5f, 0x54, 0x61, 0x72, 0x67, 0x65, 0x74, 0x00, 0x50, 0x53, 0x56, 0x30, 0xbc, 0x00, 0x00, 0x00, 0x24, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x10, 0x00,
  0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x54, 0x45, 0x58, 0x43, 0x4f,
  0x4f, 0x52, 0x44, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x44, 0x03, 0x03, 0x04, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
  0x42, 0x00, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x44, 0x10, 0x03, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x0f, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x58, 0x49, 0x4c, 0xc8, 0x07, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00,
  0xf2, 0x01, 0x00, 0x00, 0x44, 0x58, 0x49, 0x4c, 0x00, 0x01, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0xb0, 0x07, 0x00, 0x00, 0x42, 0x43,
  0xc0, 0xde, 0x21, 0x0c, 0x00, 0x00, 0xe9, 0x01, 0x00, 0x00, 0x0b, 0x82, 0x20, 0x00, 0x02, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
  0x07, 0x81, 0x23, 0x91, 0x41, 0xc8, 0x04, 0x49, 0x06, 0x10, 0x32, 0x39, 0x92, 0x01, 0x84, 0x0c, 0x25, 0x05, 0x08, 0x19, 0x1e, 0x04,
  0x8b, 0x62, 0x80, 0x14, 0x45, 0x02, 0x42, 0x92, 0x0b, 0x42, 0xa4, 0x10, 0x32, 0x14, 0x38, 0x08, 0x18, 0x4b, 0x0a, 0x32, 0x52, 0x88,
  0x48, 0x90, 0x14, 0x20, 0x43, 0x46, 0x88, 0xa5, 0x00, 0x19, 0x32, 0x42, 0xe4, 0x48, 0x0e, 0x90, 0x91, 0x22, 0xc4, 0x50, 0x41, 0x51,
  0x81, 0x8c, 0xe1, 0x83, 0xe5, 0x8a, 0x04, 0x29, 0x46, 0x06, 0x51, 0x18, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x1b, 0x8c, 0xe0, 0xff,
  0xff, 0xff, 0xff, 0x07, 0x40, 0x02, 0xa8, 0x0d, 0x84, 0xf0, 0xff, 0xff, 0xff, 0xff, 0x03, 0x20, 0x6d, 0x30, 0x86, 0xff, 0xff, 0xff,
  0xff, 0x1f, 0x00, 0x09, 0xa8, 0x00, 0x49, 0x18, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x13, 0x82, 0x60, 0x42, 0x20, 0x4c, 0x08, 0x06,
  0x00, 0x00, 0x00, 0x00, 0x89, 0x20, 0x00, 0x00, 0x47, 0x00, 0x00, 0x00, 0x32, 0x22, 0x48, 0x09, 0x20, 0x64, 0x85, 0x04, 0x93, 0x22,
  0xa4, 0x84, 0x04, 0x93, 0x22, 0xe3, 0x84, 0xa1, 0x90, 0x14, 0x12, 0x4c, 0x8a, 0x8c, 0x0b, 0x84, 0xa4, 0x4c, 0x10, 0x70, 0x73, 0x04,
  0x60, 0x70, 0x93, 0x34, 0x45, 0x94, 0x30, 0xf9, 0x2c, 0xc0, 0x3c, 0x0b, 0x11, 0xb1, 0x13, 0x30, 0x11, 0x28, 0x00, 0x14, 0x66, 0x00,
  0x86, 0x11, 0x88, 0x61, 0xa6, 0x36, 0x18, 0x07, 0x76, 0x08, 0x87, 0x79, 0x98, 0x07, 0x37, 0xa0, 0x85, 0x72, 0xc0, 0x07, 0x7a, 0xa8,
  0x07, 0x79, 0x28, 0x07, 0x39, 0x20, 0x05, 0x3e, 0xb0, 0x87, 0x72, 0x18, 0x07, 0x7a, 0x78, 0x07, 0x79, 0xe0, 0x03, 0x73, 0x60, 0x87,
  0x77, 0x08, 0x07, 0x7a, 0x60, 0x03, 0x30, 0xa0, 0x03, 0x3f, 0x00, 0x03, 0x3f, 0xd0, 0x03, 0x3d, 0x68, 0x87, 0x74, 0x80, 0x87, 0x79,
  0xf8, 0x05, 0x7a, 0xc8, 0x07, 0x78, 0x28, 0x07, 0x14, 0x80, 0x99, 0xc4, 0x60, 0x1c, 0xd8, 0x21, 0x1c, 0xe6, 0x61, 0x1e, 0xdc, 0x80,
  0x16, 0xca, 0x01, 0x1f, 0xe8, 0xa1, 0x1e, 0xe4, 0xa1, 0x1c, 0xe4, 0x80, 0x14, 0xf8, 0xc0, 0x1e, 0xca, 0x61, 0x1c, 0xe8, 0xe1, 0x1d,
  0xe4, 0x81, 0x0f, 0xcc, 0x81, 0x1d, 0xde, 0x21, 0x1c, 0xe8, 0x81, 0x0d, 0xc0, 0x80, 0x0e, 0xfc, 0x00, 0x0c, 0xfc, 0x00, 0x09, 0xa4,
  0x90, 0x19, 0x01, 0x28, 0x01, 0xa2, 0x34, 0x47, 0x80, 0x14, 0x63, 0x00, 0x00, 0x16, 0x20, 0x56, 0x0c, 0x04, 0x00, 0x58, 0x83, 0xdc,
  0x4d, 0xc3, 0xe5, 0x4f, 0xd8, 0x43, 0x48, 0xfe, 0x4a, 0x48, 0x2b, 0x31, 0xf9, 0xc5, 0x6d, 0xa3, 0x62, 0x8c, 0x31, 0x00, 0xad, 0x7b,
  0x86, 0xcb, 0x9f, 0xb0, 0x87, 0x90, 0xfc, 0x10, 0x68, 0x86, 0x85, 0x40, 0x41, 0x2c, 0x0c, 0x04, 0x52, 0x8e, 0x31, 0x06, 0x00, 0x60,
  0xd0, 0x9c, 0x23, 0x08, 0x8a, 0x21, 0xc1, 0x02, 0xa0, 0x92, 0x1d, 0x08, 0x18, 0x46, 0x10, 0x86, 0x63, 0xa4, 0x29, 0xa2, 0x84, 0xc9,
  0x7f, 0x25, 0x94, 0x9a, 0x90, 0x40, 0x72, 0x22, 0x00, 0x00, 0x13, 0x14, 0x72, 0xc0, 0x87, 0x74, 0x60, 0x87, 0x36, 0x68, 0x87, 0x79,
  0x68, 0x03, 0x72, 0xc0, 0x87, 0x0d, 0xaf, 0x50, 0x0e, 0x6d, 0xd0, 0x0e, 0x7a, 0x50, 0x0e, 0x6d, 0x00, 0x0f, 0x7a, 0x30, 0x07, 0x72,
  0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e, 0x71, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d, 0x90, 0x0e, 0x78, 0xa0, 0x07, 0x73, 0x20,
  0x07, 0x6d, 0x90, 0x0e, 0x71, 0x60, 0x07, 0x7a, 0x30, 0x07, 0x72, 0xd0, 0x06, 0xe9, 0x30, 0x07, 0x72, 0xa0, 0x07, 0x73, 0x20, 0x07,
  0x6d, 0x90, 0x0e, 0x76, 0x40, 0x07, 0x7a, 0x60, 0x07, 0x74, 0xd0, 0x06, 0xe6, 0x10, 0x07, 0x76, 0xa0, 0x07, 0x73, 0x20, 0x07, 0x6d,
  0x60, 0x0e, 0x73, 0x20, 0x07, 0x7a, 0x30, 0x07, 0x72, 0xd0, 0x06, 0xe6, 0x60, 0x07, 0x74, 0xa0, 0x07, 0x76, 0x40, 0x07, 0x6d, 0xe0,
  0x0e, 0x78, 0xa0, 0x07, 0x71, 0x60, 0x07, 0x7a, 0x30, 0x07, 0x72, 0xa0, 0x07, 0x76, 0x40, 0x07, 0x3a, 0x0f, 0x64, 0x90, 0x21, 0x23,
  0x45, 0x44, 0x00, 0x66, 0x00, 0xc0, 0xf4, 0x00, 0x80, 0x87, 0x3c, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x0c, 0x79, 0x18, 0x20, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0xf2, 0x38, 0x40, 0x00, 0x08, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x30, 0xe4, 0x99, 0x80, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0xc8, 0x63, 0x01, 0x01,
  0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x16, 0x08, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x32, 0x1e, 0x98, 0x14,
  0x19, 0x11, 0x4c, 0x90, 0x8c, 0x09, 0x26, 0x47, 0xc6, 0x04, 0x43, 0x02, 0x25, 0x30, 0x02, 0x50, 0x08, 0xc5, 0x50, 0x04, 0x25, 0x51,
  0x28, 0x65, 0x50, 0x0e, 0x05, 0x52, 0x0a, 0x05, 0x43, 0xab, 0x40, 0x8a, 0x60, 0x04, 0xa0, 0x10, 0x4a, 0xa2, 0x0c, 0xe8, 0xcc, 0x00,
  0x90, 0x99, 0x01, 0xa0, 0x32, 0x03, 0x40, 0x62, 0x06, 0x80, 0xf4, 0x0c, 0x00, 0xed, 0xb1, 0x10, 0x83, 0x00, 0x00, 0x00, 0x78, 0x1e,
  0x00, 0x00, 0x79, 0x18, 0x00, 0x00, 0x85, 0x00, 0x00, 0x00, 0x1a, 0x03, 0x4c, 0x90, 0x46, 0x02, 0x13, 0xc4, 0x83, 0x0c, 0x6f, 0x0c,
  0x24, 0xc6, 0x45, 0x66, 0x43, 0x10, 0x4c, 0x10, 0x80, 0x63, 0x82, 0x00, 0x20, 0x1b, 0x84, 0x81, 0x98, 0x20, 0x00, 0xc9, 0x06, 0x61,
  0x30, 0x28, 0xc0, 0xcd, 0x4d, 0x10, 0x00, 0x65, 0xc3, 0x80, 0x24, 0xc4, 0x04, 0xe1, 0xc8, 0x38, 0xd0, 0x95, 0xe1, 0x4d, 0x10, 0x80,
  0x65, 0x82, 0x00, 0x30, 0x1b, 0x04, 0xc2, 0xd9, 0x90, 0x10, 0x0b, 0x43, 0x10, 0x43, 0x43, 0x3c, 0x1b, 0x02, 0x68, 0x82, 0x20, 0x70,
  0x24, 0xe6, 0xda, 0xe0, 0xd8, 0x36, 0x20, 0x84, 0x34, 0x11, 0xc4, 0x40, 0x00, 0x1b, 0x02, 0x6a, 0x03, 0x11, 0x01, 0x40, 0x35, 0x41,
  0x30, 0xb4, 0x09, 0x02, 0xd0, 0x30, 0x40, 0x9b, 0x20, 0x00, 0xce, 0x04, 0x01, 0x78, 0x36, 0x18, 0x49, 0xa6, 0x11, 0x9b, 0x43, 0xa2,
  0x2d, 0x0d, 0x6e, 0x6e, 0x82, 0x00, 0x40, 0x1b, 0x88, 0xa4, 0xd3, 0xbc, 0x0d, 0x03, 0xc6, 0x7d, 0x13, 0x84, 0x62, 0xa3, 0x81, 0x16,
  0xe6, 0x46, 0xc6, 0x56, 0x36, 0x41, 0x00, 0xa2, 0x0d, 0x46, 0x22, 0x06, 0x1a, 0xb1, 0x8d, 0xc1, 0x06, 0xc1, 0x20, 0x83, 0x09, 0x82,
  0xd6, 0x4d, 0x10, 0x00, 0x89, 0x03, 0xdc, 0xdb, 0x1c, 0x97, 0x29, 0xab, 0x2f, 0xa8, 0xa7, 0xa9, 0x24, 0xaa, 0xa4, 0x27, 0xa7, 0x0d,
  0x48, 0x82, 0x06, 0x1a, 0x61, 0xa4, 0xc1, 0xe6, 0xb0, 0x18, 0x7b, 0x7b, 0x93, 0x23, 0x63, 0x42, 0x55, 0x84, 0x35, 0xf4, 0xf4, 0x24,
  0x45, 0x04, 0xb3, 0x01, 0x49, 0xd6, 0x40, 0xf3, 0x0c, 0x36, 0xd8, 0x9c, 0x0d, 0xc3, 0x19, 0xa8, 0x41, 0x1b, 0x6c, 0x38, 0x88, 0x0b,
  0x0c, 0xc2, 0xa0, 0x0c, 0xcc, 0xc0, 0x0d, 0x26, 0x08, 0x8a, 0xb0, 0x01, 0xd8, 0x30, 0x10, 0x71, 0x10, 0x07, 0x1b, 0x02, 0x39, 0xd8,
  0x30, 0x0c, 0x70, 0x30, 0x07, 0x13, 0x84, 0xcd, 0xdb, 0x10, 0xd4, 0x01, 0x89, 0xb6, 0xb0, 0x34, 0x37, 0x2e, 0x53, 0x56, 0x5f, 0x50,
  0x6f, 0x73, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x13, 0x84, 0xe5, 0x9a, 0x20, 0x2c, 0xd8, 0x86, 0x80, 0x98, 0x20, 0x2c, 0xd6, 0x04, 0x61,
  0xa9, 0x36, 0x2c, 0x04, 0x1e, 0xe4, 0x81, 0x1e, 0xec, 0x01, 0x1f, 0x0c, 0x7c, 0x40, 0xf4, 0x01, 0x40, 0x84, 0xaa, 0x08, 0x6b, 0xe8,
  0xe9, 0x49, 0x8a, 0x68, 0x82, 0xb0, 0x50, 0x1b, 0x96, 0xe1, 0x0f, 0xf2, 0xa0, 0x0f, 0xf6, 0x00, 0x14, 0x06, 0x50, 0x18, 0xfa, 0x00,
  0xd8, 0x20, 0xf8, 0x41, 0x28, 0x30, 0x99, 0xb2, 0xfa, 0xa2, 0x0a, 0x93, 0x3b, 0x2b, 0xa3, 0x9b, 0x20, 0x2c, 0xd3, 0x86, 0x85, 0x18,
  0x85, 0x3c, 0x20, 0x85, 0x3d, 0xe8, 0x83, 0x81, 0x0f, 0x88, 0x3e, 0x00, 0x36, 0x04, 0xa5, 0xb0, 0x61, 0x10, 0x05, 0x53, 0x00, 0x36,
  0x14, 0x70, 0x70, 0x07, 0xa7, 0x60, 0x01, 0x55, 0xd8, 0xd8, 0xec, 0xda, 0x5c, 0xd2, 0xc8, 0xca, 0xdc, 0xe8, 0xa6, 0x04, 0x41, 0x15,
  0x32, 0x3c, 0x17, 0xbb, 0x32, 0xb9, 0xb9, 0xb4, 0x37, 0xb7, 0x29, 0x01, 0xd1, 0x84, 0x0c, 0xcf, 0xc5, 0x2e, 0x8c, 0xcd, 0xae, 0x4c,
  0x6e, 0x4a, 0x60, 0xd4, 0x21, 0xc3, 0x73, 0x99, 0x43, 0x0b, 0x23, 0x2b, 0x93, 0x6b, 0x7a, 0x23, 0x2b, 0x63, 0x9b, 0x12, 0x24, 0x65,
  0xc8, 0xf0, 0x5c, 0xe4, 0xca, 0xe6, 0xde, 0xea, 0xe4, 0xc6, 0xca, 0xe6, 0xa6, 0x04, 0x55, 0x25, 0x32, 0x3c, 0x17, 0xba, 0x3c, 0xb8,
  0xb2, 0x20, 0x37, 0xb7, 0x37, 0xba, 0x30, 0xba, 0xb4, 0x37, 0xb7, 0xb9, 0x29, 0x82, 0x1b, 0xcc, 0x41, 0x1d, 0x32, 0x3c, 0x17, 0xbb,
  0xb4, 0xb2, 0xbb, 0x24, 0xb2, 0x29, 0xba, 0x30, 0xba, 0xb2, 0x29, 0x41, 0x1d, 0xd4, 0x21, 0xc3, 0x73, 0x29, 0x73, 0xa3, 0x93, 0xcb,
  0x83, 0x7a, 0x4b, 0x73, 0xa3, 0x9b, 0x9b, 0x12, 0x9c, 0x02, 0x00, 0x00, 0x00, 0x00, 0x79, 0x18, 0x00, 0x00, 0x4c, 0x00, 0x00, 0x00,
  0x33, 0x08, 0x80, 0x1c, 0xc4, 0xe1, 0x1c, 0x66, 0x14, 0x01, 0x3d, 0x88, 0x43, 0x38, 0x84, 0xc3, 0x8c, 0x42, 0x80, 0x07, 0x79, 0x78,
  0x07, 0x73, 0x98, 0x71, 0x0c, 0xe6, 0x00, 0x0f, 0xed, 0x10, 0x0e, 0xf4, 0x80, 0x0e, 0x33, 0x0c, 0x42, 0x1e, 0xc2, 0xc1, 0x1d, 0xce,
  0xa1, 0x1c, 0x66, 0x30, 0x05, 0x3d, 0x88, 0x43, 0x38, 0x84, 0x83, 0x1b, 0xcc, 0x03, 0x3d, 0xc8, 0x43, 0x3d, 0x8c, 0x03, 0x3d, 0xcc,
  0x78, 0x8c, 0x74, 0x70, 0x07, 0x7b, 0x08, 0x07, 0x79, 0x48, 0x87, 0x70, 0x70, 0x07, 0x7a, 0x70, 0x03, 0x76, 0x78, 0x87, 0x70, 0x20,
  0x87, 0x19, 0xcc, 0x11, 0x0e, 0xec, 0x90, 0x0e, 0xe1, 0x30, 0x0f, 0x6e, 0x30, 0x0f, 0xe3, 0xf0, 0x0e, 0xf0, 0x50, 0x0e, 0x33, 0x10,
  0xc4, 0x1d, 0xde, 0x21, 0x1c, 0xd8, 0x21, 0x1d, 0xc2, 0x61, 0x1e, 0x66, 0x30, 0x89, 0x3b, 0xbc, 0x83, 0x3b, 0xd0, 0x43, 0x39, 0xb4,
  0x03, 0x3c, 0xbc, 0x83, 0x3c, 0x84, 0x03, 0x3b, 0xcc, 0xf0, 0x14, 0x76, 0x60, 0x07, 0x7b, 0x68, 0x07, 0x37, 0x68, 0x87, 0x72, 0x68,
  0x07, 0x37, 0x80, 0x87, 0x70, 0x90, 0x87, 0x70, 0x60, 0x07, 0x76, 0x28, 0x07, 0x76, 0xf8, 0x05, 0x76, 0x78, 0x87, 0x77, 0x80, 0x87,
  0x5f, 0x08, 0x87, 0x71, 0x18, 0x87, 0x72, 0x98, 0x87, 0x79, 0x98, 0x81, 0x2c, 0xee, 0xf0, 0x0e, 0xee, 0xe0, 0x0e, 0xf5, 0xc0, 0x0e,
  0xec, 0x30, 0x03, 0x62, 0xc8, 0xa1, 0x1c, 0xe4, 0xa1, 0x1c, 0xcc, 0xa1, 0x1c, 0xe4, 0xa1, 0x1c, 0xdc, 0x61, 0x1c, 0xca, 0x21, 0x1c,
  0xc4, 0x81, 0x1d, 0xca, 0x61, 0x06, 0xd6, 0x90, 0x43, 0x39, 0xc8, 0x43, 0x39, 0x98, 0x43, 0x39, 0xc8, 0x43, 0x39, 0xb8, 0xc3, 0x38,
  0x94, 0x43, 0x38, 0x88, 0x03, 0x3b, 0x94, 0xc3, 0x2f, 0xbc, 0x83, 0x3c, 0xfc, 0x82, 0x3b, 0xd4, 0x03, 0x3b, 0xb0, 0xc3, 0x8c, 0xcc,
  0x21, 0x07, 0x7c, 0x70, 0x03, 0x74, 0x60, 0x07, 0x37, 0x90, 0x87, 0x72, 0x98, 0x87, 0x77, 0xa8, 0x07, 0x79, 0x18, 0x87, 0x72, 0x70,
  0x83, 0x70, 0xa0, 0x07, 0x7a, 0x90, 0x87, 0x74, 0x10, 0x87, 0x7a, 0xa0, 0x87, 0x72, 0x00, 0x00, 0x00, 0x00, 0x71, 0x20, 0x00, 0x00,
  0x22, 0x00, 0x00, 0x00, 0x36, 0x30, 0x0d, 0x97, 0xef, 0x3c, 0xfe, 0xe2, 0x00, 0x83, 0xd8, 0x3c, 0xd4, 0xe4, 0x17, 0xb7, 0x6d, 0x05,
  0xd0, 0x70, 0xf9, 0xce, 0xe3, 0x4b, 0x00, 0xf3, 0x2c, 0x84, 0x5f, 0xdc, 0x76, 0x05, 0xa0, 0x41, 0xe0, 0x47, 0x2f, 0xe3, 0x81, 0xc0,
  0x99, 0xf5, 0x47, 0xa2, 0x96, 0xf1, 0xf4, 0xba, 0xbc, 0x2c, 0x23, 0x02, 0xad, 0x3f, 0x92, 0xbd, 0x3c, 0xa6, 0xbf, 0xe5, 0xc0, 0x26,
  0x09, 0x36, 0x03, 0x02, 0x81, 0xc0, 0xa0, 0x09, 0x10, 0x0c, 0x80, 0x34, 0x66, 0x20, 0x0d, 0x97, 0xef, 0x3c, 0xbe, 0x10, 0x11, 0xc0,
  0x44, 0x84, 0x40, 0x33, 0x2c, 0x44, 0x01, 0x64, 0x81, 0xdf, 0xbc, 0x0d, 0x67, 0x03, 0x81, 0xb3, 0xea, 0x34, 0xdc, 0x86, 0xb3, 0xcb,
  0xf2, 0x29, 0x3d, 0x4c, 0x2f, 0x03, 0x81, 0x41, 0x23, 0xa8, 0x86, 0xcb, 0x77, 0x1e, 0x5f, 0x9a, 0x9c, 0x88, 0x40, 0xa9, 0xe9, 0xa1,
  0x26, 0xbf, 0xb8, 0x6d, 0x00, 0x00, 0x00, 0x00, 0x61, 0x20, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x13, 0x04, 0x41, 0x2c, 0x10, 0x00,
  0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x66, 0x00, 0x4a, 0xae, 0xf0, 0xa8, 0x8e, 0x00, 0xd0, 0x2a, 0x01, 0x1a, 0x33, 0x00, 0x00,
  0x23, 0x06, 0x09, 0x00, 0x82, 0x60, 0x60, 0x7d, 0x05, 0xd4, 0x75, 0xc3, 0x88, 0x41, 0x02, 0x80, 0x20, 0x18, 0x58, 0x60, 0x60, 0x38,
  0x9e, 0x47, 0x8c, 0x18, 0x24, 0x00, 0x08, 0x82, 0x01, 0x43, 0x06, 0x1e, 0x18, 0x7c, 0x12, 0x32, 0x62, 0x90, 0x00, 0x20, 0x08, 0x06,
  0x4c, 0x19, 0x7c, 0x61, 0x00, 0x06, 0x45, 0x32, 0x62, 0xf0, 0x00, 0x20, 0x08, 0x06, 0x13, 0x19, 0x20, 0xc4, 0x20, 0x04, 0x45, 0xa1,
  0x28, 0x4a, 0x31, 0x9a, 0x10, 0x00, 0xa3, 0x09, 0x42, 0x30, 0x9a, 0x30, 0x08, 0xa3, 0x09, 0xc4, 0x30, 0x62, 0x90, 0x00, 0x20, 0x08,
  0x06, 0x8e, 0x1a, 0x74, 0x66, 0x60, 0x06, 0x19, 0x31, 0x62, 0x90, 0x00, 0x20, 0x08, 0x06, 0x8e, 0x1a, 0x74, 0x66, 0x60, 0x06, 0xcb,
  0x30, 0x62, 0x90, 0x00, 0x20, 0x08, 0x06, 0x8e, 0x1a, 0x74, 0x66, 0x60, 0x06, 0x9a, 0x30, 0x62, 0x90, 0x00, 0x20, 0x08, 0x06, 0x8e,
  0x1a, 0x74, 0x66, 0x60, 0x06, 0x56, 0x80, 0xe1, 0x40, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x56, 0xe2, 0x4c, 0xc4, 0xf5, 0x4f,
  0xc4, 0x35, 0x51, 0x11, 0xf1, 0xdb, 0x83, 0x99, 0x30, 0x12, 0xf3, 0x2c, 0xbf, 0x04, 0x30, 0xcf, 0x42, 0x44, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
