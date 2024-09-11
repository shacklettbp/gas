#pragma once

namespace gas {

template <typename T>
constexpr bool GenHandle<T>::null() const
{
    return static_cast<const T *>(this)->gen == 0;
}

template <typename T>
constexpr u32 GenHandle<T>::uint() const
{
    const T &t = *static_cast<const T *>(this);
    return ((u32)t.id << 16) | (u32)t.gen;
}

template <typename T>
constexpr T GenHandle<T>::fromUInt(u32 v)
{
  return T {
    .gen = u16(v),
    .id = u16(v >> 16),
  };
}

RasterPassInterfaceID::RasterPassInterfaceID(UUID uuid)
  : id { uuid.id[0], uuid.id[1] }
{}

RasterPassInterfaceID::RasterPassInterfaceID(RasterPassInterface interface)
  : id { 0xFFFF'FFFF'FFFF'FFFF, (u64)interface.uint() }
{}

bool RasterPassInterfaceID::isUUID() const
{
  return id[0] != 0xFFFF'FFFF'FFFF'FFFF;
}

UUID RasterPassInterfaceID::asUUID() const
{
  return UUID { id[0], id[1] };
}

RasterPassInterface RasterPassInterfaceID::asHandle() const
{
  return RasterPassInterface::fromUInt((u32)id[1]);
}

ParamBlockTypeID::ParamBlockTypeID(UUID uuid)
  : id { uuid.id[0], uuid.id[1] }
{}

ParamBlockTypeID::ParamBlockTypeID(ParamBlockType blk_type)
  : id { 0xFFFF'FFFF'FFFF'FFFF, (u64)blk_type.uint() }
{}

bool ParamBlockTypeID::isUUID() const
{
  return id[0] != 0xFFFF'FFFF'FFFF'FFFF;
}

UUID ParamBlockTypeID::asUUID() const
{
  return UUID { id[0], id[1] };
}

ParamBlockType ParamBlockTypeID::asHandle() const
{
  return ParamBlockType::fromUInt((u32)id[1]);
}

Texture Swapchain::proxyAttachment() const
{
  return Texture { .gen = 0, .id = (u16)id };
}

void CommandWriter::val(u32 v)
{
  if ((size_t)cmds->offset == cmds->data.size()) [[unlikely]] {
    FrontendCommands *next = alloc->alloc<FrontendCommands>();
    cmds->next = next;

    next->offset = 0;
    next->next = nullptr;

    cmds = next;
  }

  cmds->data[cmds->offset++] = v;
}

template <typename T>
void CommandWriter::id(T t)
{
  val(t.uint());
}

void CommandWriter::ctrl(CommandCtrl ctrl)
{
  val((u32)ctrl);
}

void RasterPassEncoder::setShader(RasterShader shader)
{
  if (state_.shader == shader) {
    return;
  }

  ctrl_ |= CommandCtrl::DrawShader;
  state_.shader = shader;
}

void RasterPassEncoder::setParamBlock(i32 idx, ParamBlock param_block)
{
  assert(idx >= 0 && idx <= 2);
  if (state_.paramBlocks[idx] == param_block) {
    return;
  }

  ctrl_ |= CommandCtrl((u32)CommandCtrl::DrawParamBlock0 << idx);
  state_.paramBlocks[idx] = param_block;
}

void RasterPassEncoder::setIndexBuffer(Buffer buffer)
{
  if (state_.indexBuffer == buffer) {
    return;
  }

  ctrl_ |= CommandCtrl::DrawIndexBuffer;
  state_.indexBuffer = buffer;
}


void RasterPassEncoder::draw(u32 vertex_offset, u32 num_triangles)
{
  drawInstanced(vertex_offset, num_triangles, 0, 1);
}

void RasterPassEncoder::drawIndexed(
  u32 vertex_offset,
  u32 index_offset, u32 num_triangles)
{
  drawIndexedInstanced(vertex_offset, index_offset, num_triangles, 0, 1);
}

void RasterPassEncoder::drawInstanced(
  u32 vertex_offset, u32 num_triangles,
  u32 instance_offset, u32 num_instances)
{
  encodeDraw(CommandCtrl::Draw, vertex_offset,
             0, num_triangles,
             instance_offset, num_instances);
}

void RasterPassEncoder::drawIndexedInstanced(
  u32 vertex_offset,
  u32 index_offset, u32 num_triangles,
  u32 instance_offset, u32 num_instances)
{
  encodeDraw(CommandCtrl::DrawIndexed, vertex_offset,
             index_offset, num_triangles,
             instance_offset, num_instances);
}

void RasterPassEncoder::encodeDraw(
  CommandCtrl draw_type, u32 vertex_offset,
  u32 index_offset, u32 num_triangles,
  u32 instance_offset, u32 num_instances)
{
  using enum CommandCtrl;

  if (state_.vertexOffset != vertex_offset) {
    ctrl_ |= DrawVertexOffset;
    state_.vertexOffset = vertex_offset;
  }

  if (state_.indexOffset != index_offset) {
    ctrl_ |= DrawIndexOffset;
    state_.indexOffset = index_offset;
  }

  if (state_.numTriangles != num_triangles) {
    ctrl_ |= DrawNumTriangles;
    state_.numTriangles = num_triangles;
  }

  if (state_.instanceOffset != instance_offset) {
    ctrl_ |= DrawInstanceOffset;
    state_.instanceOffset = instance_offset;
  }

  if (state_.numInstances != num_instances) {
    ctrl_ |= DrawNumInstances;
    state_.numInstances = num_instances;
  }

  ctrl_ |= draw_type;

  writer_.ctrl(ctrl_);

  if ((ctrl_ & DrawShader) != None) {
    writer_.id(state_.shader);
  }

  if ((ctrl_ & DrawParamBlock0) != None) {
    writer_.id(state_.paramBlocks[0]);
  }

  if ((ctrl_ & DrawParamBlock1) != None) {
    writer_.id(state_.paramBlocks[1]);
  }

  if ((ctrl_ & DrawParamBlock2) != None) {
    writer_.id(state_.paramBlocks[2]);
  }

  if ((ctrl_ & DrawDynamicOffsets0) != None) {
    writer_.val(state_.dynamicBufferOffsets[0]);
  }

  if ((ctrl_ & DrawDynamicOffsets1) != None) {
    writer_.val(state_.dynamicBufferOffsets[1]);
  }

  if ((ctrl_ & DrawIndexOffset) != None) {
    writer_.val(state_.indexOffset);
  }

  if ((ctrl_ & DrawNumTriangles) != None) {
    writer_.val(state_.numTriangles);
  }

  if ((ctrl_ & DrawVertexOffset) != None) {
    writer_.val(state_.vertexOffset);
  }

  if ((ctrl_ & DrawInstanceOffset) != None) {
    writer_.val(state_.instanceOffset);
  }

  if ((ctrl_ & DrawNumInstances) != None) {
    writer_.val(state_.numInstances);
  }

  ctrl_ = None;
}

RasterPassEncoder::RasterPassEncoder(CommandWriter writer)
  : writer_(writer),
    ctrl_(CommandCtrl::None),
    state_()
{}

CopyCommand::CopyCommand()
  : b2b()
{}

void CopyPassEncoder::copyBufferToBuffer(Buffer src, Buffer dst,
                                            u32 src_offset, u32 dst_offset,
                                            u32 num_bytes)
{
  using enum CommandCtrl;

  if (state_.b2b.src != src) {
    ctrl_ |= CopyB2BSrcBuffer;
    state_.b2b.src = src;
  }

  if (state_.b2b.dst != dst) {
    ctrl_ |= CopyB2BDstBuffer;
    state_.b2b.dst = dst;
  }

  if (state_.b2b.srcOffset != src_offset) {
    ctrl_ |= CopyB2BSrcOffset;
    state_.b2b.srcOffset = src_offset;
  }

  if (state_.b2b.dstOffset != dst_offset) {
    ctrl_ |= CopyB2BDstOffset;
    state_.b2b.dstOffset = dst_offset;
  }

  if (state_.b2b.numBytes != num_bytes) {
    ctrl_ |= CopyB2BNumBytes;
    state_.b2b.numBytes = num_bytes;
  }

  ctrl_ |= CopyBufferToBuffer;

  writer_.ctrl(ctrl_);

  if ((ctrl_ & CopyB2BSrcBuffer) != None) {
    writer_.id(state_.b2b.src);
  }

  if ((ctrl_ & CopyB2BDstBuffer) != None) {
    writer_.id(state_.b2b.dst);
  }

  if ((ctrl_ & CopyB2BSrcOffset) != None) {
    writer_.val(state_.b2b.srcOffset);
  }

  if ((ctrl_ & CopyB2BDstOffset) != None) {
    writer_.val(state_.b2b.dstOffset);
  }

  if ((ctrl_ & CopyB2BNumBytes) != None) {
    writer_.val(state_.b2b.numBytes);
  }

  ctrl_ = None;
}

void CopyPassEncoder::copyBufferToTexture(Buffer src,
                                             Texture dst,
                                             u32 src_offset,
                                             u32 dst_mip_offset,
                                             u32 dst_num_mips)
{
  using enum CommandCtrl;

  if (state_.b2t.src != src) {
    ctrl_ |= CopyB2TSrcBuffer;
    state_.b2t.src = src;
  }

  if (state_.b2t.dst != dst) {
    ctrl_ |= CopyB2TDstTexture;
    state_.b2t.dst = dst;
  }

  if (state_.b2t.srcOffset != src_offset) {
    ctrl_ |= CopyB2TSrcOffset;
    state_.b2t.srcOffset = src_offset;
  }

  u32 mip_slice = ((dst_mip_offset & 0xFFFF) << 16) | (dst_num_mips & 0xFFFF);

  if (state_.b2t.dstMipSlice != mip_slice) {
    ctrl_ |= CopyB2TDstMipSlice;
    state_.b2t.dstMipSlice = mip_slice;
  }

  ctrl_ |= CopyBufferToTexture;

  writer_.ctrl(ctrl_);

  if ((ctrl_ & CopyB2TSrcBuffer) != None) {
    writer_.id(state_.b2t.src);
  }

  if ((ctrl_ & CopyB2TDstTexture) != None) {
    writer_.id(state_.b2t.dst);
  }

  if ((ctrl_ & CopyB2TSrcOffset) != None) {
    writer_.val(state_.b2t.srcOffset);
  }

  if ((ctrl_ & CopyB2TDstMipSlice) != None) {
    writer_.val(state_.b2t.dstMipSlice);
  }

  ctrl_ = None;
}

void CopyPassEncoder::copyTextureToBuffer(Texture src, Buffer dst,
                                             u32 src_mip_offset,
                                             u32 src_num_mips,
                                             u32 dst_offset)
{
  using enum CommandCtrl;

  if (state_.t2b.src != src) {
    ctrl_ |= CopyT2BSrcTexture;
    state_.t2b.src = src;
  }

  if (state_.t2b.dst != dst) {
    ctrl_ |= CopyT2BDstBuffer;
    state_.t2b.dst = dst;
  }

  u32 mip_slice = ((src_mip_offset & 0xFFFF) << 16) | (src_num_mips & 0xFFFF);

  if (state_.t2b.srcMipSlice != mip_slice) {
    ctrl_ |= CopyT2BSrcMipSlice;
    state_.t2b.srcMipSlice = mip_slice;
  }

  if (state_.t2b.dstOffset != dst_offset) {
    ctrl_ |= CopyT2BDstOffset;
    state_.t2b.dstOffset = dst_offset;
  }

  ctrl_ |= CopyBufferToTexture;

  writer_.ctrl(ctrl_);

  if ((ctrl_ & CopyT2BSrcTexture) != None) {
    writer_.id(state_.t2b.src);
  }

  if ((ctrl_ & CopyT2BDstBuffer) != None) {
    writer_.id(state_.t2b.dst);
  }

  if ((ctrl_ & CopyT2BSrcMipSlice) != None) {
    writer_.val(state_.t2b.srcMipSlice);
  }

  if ((ctrl_ & CopyT2BDstOffset) != None) {
    writer_.val(state_.t2b.dstOffset);
  }

  ctrl_ = None;
}

void CopyPassEncoder::fillBuffer(Buffer buffer, u32 offset,
                                    u32 num_bytes, u32 v)
{
  using enum CommandCtrl;

  if (state_.fill.buffer != buffer) {
    ctrl_ |= CopyFillBuffer;
    state_.fill.buffer = buffer;
  }

  if (state_.fill.offset != offset) {
    ctrl_ |= CopyFillOffset;
    state_.fill.offset = offset;
  }

  if (state_.fill.numBytes != num_bytes) {
    ctrl_ |= CopyFillNumBytes;
    state_.fill.numBytes = num_bytes;
  }

  if (state_.fill.value != v) {
    ctrl_ |= CopyFillValue;
    state_.fill.value = v;
  }

  ctrl_ |= CopyBufferFill;

  writer_.ctrl(ctrl_);

  if ((ctrl_ & CopyFillBuffer) != None) {
    writer_.id(state_.fill.buffer);
  }

  if ((ctrl_ & CopyFillOffset) != None) {
    writer_.val(state_.fill.offset);
  }

  if ((ctrl_ & CopyFillNumBytes) != None) {
    writer_.val(state_.fill.numBytes);
  }

  if ((ctrl_ & CopyFillValue) != None) {
    writer_.val(state_.fill.value);
  }

  ctrl_ = None;
}

CopyPassEncoder::CopyPassEncoder(CommandWriter writer)
  : writer_(writer),
    ctrl_(CommandCtrl::None),
    state_()
{}

RasterPassEncoder CommandEncoder::beginRasterPass(
  RasterPass render_pass)
{
  cmd_writer_.ctrl(CommandCtrl::RasterPass);
  cmd_writer_.id(render_pass);

  return RasterPassEncoder(cmd_writer_);
}

void CommandEncoder::endRasterPass(RasterPassEncoder &render_enc)
{
  cmd_writer_.cmds = render_enc.writer_.cmds;
  cmd_writer_.ctrl(CommandCtrl::None);
}

ComputePassEncoder CommandEncoder::beginComputePass()
{
  return {};
}

void CommandEncoder::endComputePass(ComputePassEncoder &compute_enc)
{
  (void)compute_enc;
}

CopyPassEncoder CommandEncoder::beginCopyPass()
{
  cmd_writer_.ctrl(CommandCtrl::CopyPass);
  return CopyPassEncoder(cmd_writer_);
}

void CommandEncoder::endCopyPass(CopyPassEncoder &copy_enc)
{
  cmd_writer_.cmds = copy_enc.writer_.cmds;
  cmd_writer_.ctrl(CommandCtrl::CopyPass);
}

CommandEncoder::CommandEncoder(StackAlloc &alloc)
  : cmd_writer_(&alloc, alloc.alloc<FrontendCommands>()),
    cmds_head_(cmd_writer_.cmds)
{
  cmds_head_->offset = 0;
  cmds_head_->next = nullptr;
}

Buffer GPURuntime::createBuffer(BufferInit init,
                                TransferQueue tx_queue)
{
  Buffer out {};
  createBuffers(1, &init, &out, tx_queue);
  return out;
}

void GPURuntime::destroyBuffer(Buffer buffer)
{
  destroyBuffers(1, &buffer);
}

Texture GPURuntime::createTexture(TextureInit init,
                                  TransferQueue tx_queue)
{
  Texture out {};
  createTextures(1, &init, &out, tx_queue);
  return out;
}

void GPURuntime::destroyTexture(Texture texture)
{
  destroyTextures(1, &texture);
}

void GPURuntime::createBuffers(i32 num_buffers,
                               const BufferInit *buffer_inits,
                               Buffer *handles_out,
                               TransferQueue tx_queue)
{
  createGPUResources(num_buffers, buffer_inits, handles_out,
                     0, nullptr, nullptr, tx_queue);
}

void GPURuntime::destroyBuffers(i32 num_buffers, Buffer *buffers)
{
  destroyGPUResources(num_buffers, buffers, 0, nullptr);
}

void GPURuntime::createTextures(i32 num_textures,
                                const TextureInit *texture_inits,
                                Texture *handles_out,
                                TransferQueue tx_queue)
{
  createGPUResources(0, nullptr, nullptr,
                     num_textures, texture_inits, handles_out, tx_queue);
}

void GPURuntime::destroyTextures(i32 num_textures, Texture *textures)
{
  destroyGPUResources(0, nullptr, num_textures, textures);
}

void GPURuntime::createGPUResources(GPUResourcesCreate create,
                                    TransferQueue tx_queue)
{
  assert(create.buffers.size() == create.buffersOut.size());
  assert(create.textures.size() == create.texturesOut.size());

  createGPUResources((i32)create.buffers.size(),
                     create.buffers.data(),
                     create.buffersOut.data(),
                     (i32)create.textures.size(),
                     create.textures.data(),
                     create.texturesOut.data(),
                     tx_queue);
}

void GPURuntime::destroyGPUResources(GPUResourcesDestroy destroy)
{
  destroyGPUResources((i32)destroy.buffers.size(),
                      destroy.buffers.data(),
                      (i32)destroy.textures.size(),
                      destroy.textures.data());
}

Sampler GPURuntime::createSampler(SamplerInit init)
{
  Sampler out {};
  createSamplers(1, &init, &out);
  return out;
}

void GPURuntime::destroySampler(Sampler sampler)
{
  destroySamplers(1, &sampler);
}

ParamBlockType GPURuntime::createParamBlockType(
    ParamBlockTypeInit init)
{
  ParamBlockType blk_type {};
  createParamBlockTypes(1, &init, &blk_type);
  return blk_type;
}

void GPURuntime::destroyParamBlockType(ParamBlockType blk_type)
{
  destroyParamBlockTypes(1, &blk_type);
}

ParamBlock GPURuntime::createParamBlock(ParamBlockInit init)
{
  ParamBlock group {};
  createParamBlocks(1, &init, &group);
  return group;
}

void GPURuntime::destroyParamBlock(ParamBlock group)
{
  destroyParamBlocks(1, &group);
}

RasterPassInterface GPURuntime::createRasterPassInterface(
    RasterPassInterfaceInit init)
{
  RasterPassInterface interface {};
  createRasterPassInterfaces(1, &init, &interface);
  return interface;
}

void GPURuntime::destroyRasterPassInterface(RasterPassInterface interface)
{
  destroyRasterPassInterfaces(1, &interface);
}

RasterPass GPURuntime::createRasterPass(RasterPassInit init)
{
  RasterPass pass {};
  createRasterPasses(1, &init, &pass);
  return pass;
}

void GPURuntime::destroyRasterPass(RasterPass pass)
{
  destroyRasterPasses(1, &pass);
}

RasterShader GPURuntime::createRasterShader(RasterShaderInit init)
{
  RasterShader out {};
  createRasterShaders(1, &init, &out);
  return out;
}

void GPURuntime::destroyRasterShader(RasterShader shader)
{
  destroyRasterShaders(1, &shader);
}

CommandEncoder GPURuntime::createCommandEncoder(StackAlloc &alloc)
{
  return CommandEncoder(alloc);
}

void GPURuntime::submit(CommandEncoder &enc)
{
  submit(enc.cmds_head_);
}

inline BufferUsage & operator|=(BufferUsage &a, BufferUsage b)
{
    a = BufferUsage(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    return a;
}

inline BufferUsage operator|(BufferUsage a, BufferUsage b)
{
    a |= b;

    return a;
}

inline BufferUsage & operator&=(BufferUsage &a, BufferUsage b)
{
    a = BufferUsage(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    return a;
}

inline BufferUsage operator&(BufferUsage a, BufferUsage b)
{
    a &= b;

    return a;
}

inline TextureUsage & operator|=(TextureUsage &a, TextureUsage b)
{
    a = TextureUsage(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    return a;
}

inline TextureUsage operator|(TextureUsage a, TextureUsage b)
{
    a |= b;

    return a;
}

inline TextureUsage & operator&=(TextureUsage &a, TextureUsage b)
{
    a = TextureUsage(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    return a;
}

inline TextureUsage operator&(TextureUsage a, TextureUsage b)
{
    a &= b;

    return a;
}

inline ShaderStage & operator|=(ShaderStage &a, ShaderStage b)
{
    a = ShaderStage(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    return a;
}

inline ShaderStage operator|(ShaderStage a, ShaderStage b)
{
    a |= b;

    return a;
}

inline ShaderStage & operator&=(ShaderStage &a, ShaderStage b)
{
    a = ShaderStage(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    return a;
}

inline ShaderStage operator&(ShaderStage a, ShaderStage b)
{
    a &= b;

    return a;
}

inline CommandCtrl & operator|=(CommandCtrl &a, CommandCtrl b)
{
    a = CommandCtrl(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    return a;
}

inline CommandCtrl operator|(CommandCtrl a, CommandCtrl b)
{
    a |= b;

    return a;
}

inline CommandCtrl & operator&=(CommandCtrl &a, CommandCtrl b)
{
    a = CommandCtrl(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    return a;
}

inline CommandCtrl operator&(CommandCtrl a, CommandCtrl b)
{
    a &= b;

    return a;
}

constexpr bool operator==(Swapchain a, Swapchain b)
{
  return a.id == b.id;
}

}
