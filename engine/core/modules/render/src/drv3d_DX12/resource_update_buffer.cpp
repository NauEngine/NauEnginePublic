// Copyright 2024 N-GINN LLC. All rights reserved.

// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved

#include "nau/3d/dag_drv3d.h"
#include "nau/3d/dag_tex3d.h"

#include "device.h"
#include "driver.h"
#include "texture.h"

using namespace drv3d_dx12;

struct ResUpdateBufferImp
{
  BaseTex *destTex;
  HostDeviceSharedMemoryRegion stagingBuffer;
  BufferImageCopy uploadInfo;
  uint32_t slicePitch;
};

namespace
{
d3d::ResUpdateBuffer *allocate_update_buffer(BaseTex *texture, Image *image, MipMapIndex mip, ArrayLayerIndex array, Offset3D offset,
  Extent3D extent)
{
  NAU_ASSERT_RETURN(mip < image->getMipLevelRange(), nullptr, "DX12: mip: {} < image->getMipLevelRange(): {}", mip.index(),
    image->getMipLevelRange().count());
  NAU_ASSERT_RETURN(array < image->getArrayLayerRange(), nullptr, "DX12: array: {} < image->getArrayLayerRange(): {}", array.index(),
    image->getArrayLayerRange().count());

  Extent3D blockExtent{1, 1, 1};
  image->getFormat().getBytesPerPixelBlock(&blockExtent.width, &blockExtent.height);

  auto mipExtents = align_value(image->getMipExtents(mip), blockExtent);
  auto outline = extent + offset;
  NAU_ASSERT_RETURN(0 == offset.x % blockExtent.width, nullptr, "DX12: 0 == offset.x: {} % blockExtent.width: {}", offset.x,
    blockExtent.width);
  NAU_ASSERT_RETURN(0 == offset.y % blockExtent.height, nullptr, "DX12: 0 == offset.y: {} % blockExtent.height: {}", offset.y,
    blockExtent.height);
  NAU_ASSERT_RETURN(0 == extent.width % blockExtent.width, nullptr, "DX12: 0 == extent.width: {} % blockExtent.width: {}", extent.width,
    blockExtent.width);
  NAU_ASSERT_RETURN(0 == extent.height % blockExtent.height, nullptr, "DX12: 0 == extent.height: {} % blockExtent.height: {}",
    extent.height, blockExtent.height);
  NAU_ASSERT_RETURN(offset.x < mipExtents.width, nullptr, "DX12: offset.x: {} < mipExtents.width: {}", offset.x, mipExtents.width);
  NAU_ASSERT_RETURN(offset.y < mipExtents.height, nullptr, "DX12: offset.y: {} < mipExtents.height: {}", offset.y, mipExtents.height);
  NAU_ASSERT_RETURN(offset.z < mipExtents.depth, nullptr, "DX12: offset.z: {} < mipExtents.width: {}", offset.z, mipExtents.depth);
  NAU_ASSERT_RETURN(outline.width <= mipExtents.width, nullptr, "DX12: outline.depth: {} <= mipExtents.depth: {}", outline.width,
    mipExtents.width);
  NAU_ASSERT_RETURN(outline.height <= mipExtents.height, nullptr, "DX12: outline.depth: {} <= mipExtents.depth: {}", outline.height,
    mipExtents.height);
  NAU_ASSERT_RETURN(outline.depth <= mipExtents.depth, nullptr, "DX12: outline.depth: {} <= mipExtents.depth: {}", outline.depth,
    mipExtents.depth);

  auto subResInfo = calculate_texture_region_info(extent, ArrayLayerCount::make(1), image->getFormat());
  auto memory = drv3d_dx12::get_device().allocateTemporaryUploadMemoryForUploadBuffer(subResInfo.totalByteSize,
    D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
  if (!memory)
  {
    NAU_LOG_DEBUG("DX12: Out of memory for allocate_update_buffer_for_tex(_region)");
    return nullptr;
  }

  auto rubImp = new ResUpdateBufferImp();//new (memalloc(sizeof(ResUpdateBufferImp), tmpmem), _NEW_INPLACE) ResUpdateBufferImp{};
  rubImp->destTex = texture;
  rubImp->stagingBuffer = memory;
  rubImp->uploadInfo.layout.Footprint = subResInfo.footprint;
  rubImp->uploadInfo.subresourceIndex = image->mipAndLayerIndexToStateIndex(mip, array).index();
  rubImp->uploadInfo.imageOffset = offset;
  rubImp->slicePitch = subResInfo.rowByteSize * subResInfo.rowCount;

  return reinterpret_cast<d3d::ResUpdateBuffer *>(rubImp);
}
} // namespace

d3d::ResUpdateBuffer *d3d::allocate_update_buffer_for_tex_region(BaseTexture *dest_base_texture, unsigned dest_mip,
  unsigned dest_slice, unsigned offset_x, unsigned offset_y, unsigned offset_z, unsigned width, unsigned height, unsigned depth)
{
  NAU_ASSERT_RETURN(dest_base_texture, nullptr);

  BaseTex *texture = cast_to_texture_base(dest_base_texture);
  Image *image = texture->getDeviceImage();
  if (!image)
  {
    NAU_LOG_ERROR("DX12: Texture {:p} <{}> doesn't have device image", (void*)texture, texture->getResName());
    return nullptr;
  }

  return allocate_update_buffer(texture, image, MipMapIndex::make(dest_mip), ArrayLayerIndex::make(dest_slice),
    Offset3D{int32_t(offset_x), int32_t(offset_y), int32_t(offset_z)}, Extent3D{uint32_t(width), uint32_t(height), uint32_t(depth)});
}

d3d::ResUpdateBuffer *d3d::allocate_update_buffer_for_tex(BaseTexture *dest_base_texture, int dest_mip, int dest_slice)
{
  NAU_ASSERT_RETURN(dest_base_texture, nullptr);

  BaseTex *texture = cast_to_texture_base(dest_base_texture);
  Image *image = texture->getDeviceImage();
  if (!image)
  {
    NAU_LOG_ERROR("DX12: Texture {:p} <{}> doesn't have device image", (void*)texture, texture->getResName());
    return nullptr;
  }
  auto mipIndex = MipMapIndex::make(dest_mip);
  auto ext = image->getMipExtents2D(mipIndex);

  Extent3D blockExtent{1, 1, 1};
  image->getFormat().getBytesPerPixelBlock(&blockExtent.width, &blockExtent.height);

  return allocate_update_buffer(texture, image, mipIndex,
    ArrayLayerIndex::make((image->getType() == D3D12_RESOURCE_DIMENSION_TEXTURE3D) ? 0 : dest_slice),
    Offset3D{0, 0, (image->getType() == D3D12_RESOURCE_DIMENSION_TEXTURE3D) ? dest_slice : 0},
    align_value(Extent3D{ext.width, ext.height, 1}, blockExtent));
}

void d3d::release_update_buffer(d3d::ResUpdateBuffer *&rub)
{
  if (ResUpdateBufferImp *&rubImp = reinterpret_cast<ResUpdateBufferImp *&>(rub))
  {
    drv3d_dx12::get_device().getContext().freeMemoryOfUploadBuffer(rubImp->stagingBuffer);
    //memfree(rubImp, tmpmem); // release update buffer
    delete rubImp;
    rub = nullptr;
  }
}

char *d3d::get_update_buffer_addr_for_write(d3d::ResUpdateBuffer *rub)
{
  return rub ? ((ResUpdateBufferImp *)rub)->stagingBuffer.as<char>() : nullptr;
}

size_t d3d::get_update_buffer_size(d3d::ResUpdateBuffer *rub)
{
  return rub ? reinterpret_cast<ResUpdateBufferImp *>(rub)->stagingBuffer.range.size() : 0;
}

size_t d3d::get_update_buffer_pitch(d3d::ResUpdateBuffer *rub)
{
  return rub ? ((ResUpdateBufferImp *)rub)->uploadInfo.layout.Footprint.RowPitch : 0;
}

size_t d3d::get_update_buffer_slice_pitch(d3d::ResUpdateBuffer *rub)
{
  return rub ? reinterpret_cast<ResUpdateBufferImp *>(rub)->slicePitch : 0;
}

bool d3d::update_texture_and_release_update_buffer(d3d::ResUpdateBuffer *&rub)
{
  if (!rub)
    return false;

  STORE_RETURN_ADDRESS();
  ResUpdateBufferImp *&rubImp = reinterpret_cast<ResUpdateBufferImp *&>(rub);
  rubImp->stagingBuffer.flush();
  drv3d_dx12::get_device().getContext().uploadToImage(rubImp->destTex->getDeviceImage(), &rubImp->uploadInfo, 1, rubImp->stagingBuffer,
    DeviceQueueType::UPLOAD, false);
  d3d::release_update_buffer(rub);
  return true;
}
