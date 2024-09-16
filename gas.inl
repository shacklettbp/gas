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

u32 * CommandWriter::reserve(GPURuntime *gpu)
{
  if ((size_t)offset_ == cmds_->data.size()) [[unlikely]] {
    if (cmds_->next == nullptr) [[unlikely]] {
      auto next = gpu->allocCommandBlock();
      cmds_->next = next;
    }

    cmds_ = cmds_->next;
    offset_ = 0;
  }

  return &cmds_->data[offset_++];
}

void CommandWriter::writeU32(GPURuntime *gpu, uint32_t v)
{
  *reserve(gpu) = v;
}

template <typename T>
void CommandWriter::id(GPURuntime *gpu, T t)
{
  writeU32(gpu, t.uint());
}

void CommandWriter::ctrl(GPURuntime *gpu, CommandCtrl ctrl)
{
  writeU32(gpu, (u32)ctrl);
}

u32 GPUTmpInputBlock::alloc(u32 num_bytes)
{
  u32 start = utils::roundUpPow2(offset, 256);
  offset = start + num_bytes;
  return start;
}

bool GPUTmpInputBlock::blockFull() const
{ 
  return offset > BLOCK_SIZE; 
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

void RasterPassEncoder::setVertexBuffer(i32 idx, Buffer buffer)
{
  assert(idx >= 0 && idx < 2);

  if (state_.vertexBuffer[idx] == buffer) {
    return;
  }

  ctrl_ |= CommandCtrl((u32)CommandCtrl::DrawVertexBuffer0 << idx);
  state_.vertexBuffer[idx] = buffer;
}

void RasterPassEncoder::setIndexBufferU32(Buffer buffer)
{
  if (state_.indexBuffer == buffer) {
    return;
  }

  ctrl_ |= CommandCtrl::DrawIndexBuffer32;
  state_.indexBuffer = buffer;
}

void RasterPassEncoder::setIndexBufferU16(Buffer buffer)
{
  if (state_.indexBuffer == buffer) {
    return;
  }

  ctrl_ |= CommandCtrl::DrawIndexBuffer16;
  state_.indexBuffer = buffer;
}

MappedTmpBuffer RasterPassEncoder::tmpBuffer(u32 num_bytes)
{
  if (num_bytes > GPUTmpInputBlock::BLOCK_SIZE) [[unlikely]] {
    return MappedTmpBuffer {
      .buffer = {},
      .offset = 0,
      .ptr = nullptr,
    };
  }

  u32 offset = allocGPUTmpInput(num_bytes);

  return MappedTmpBuffer {
    .buffer = gpu_input_.buffer,
    .offset = offset,
    .ptr = gpu_input_.ptr,
  };
}

void * RasterPassEncoder::drawData(u32 num_bytes)
{
  u32 offset = allocGPUTmpInput(num_bytes);

  ctrl_ |= CommandCtrl::DrawDataOffset;
  state_.dataOffset = offset;

  return gpu_input_.ptr + offset;
}

u32 RasterPassEncoder::allocGPUTmpInput(u32 num_bytes)
{
  u32 offset = gpu_input_.alloc(num_bytes);
  if (gpu_input_.blockFull()) [[unlikely]] {
    GPUTmpInputBlock new_block = gpu_->allocGPUTmpInputBlock(queue_);

    if (gpu_input_.buffer != new_block.buffer) [[unlikely]] {
      // Note that tmpBuffer also calls this function, and does not
      // normally need to set CommandCtrl dirty bits. However in this
      // situation, tmpBuffer can trigger switching to a new buffer
      // and later drawData calls in the same buffer need to set this
      // bit so the backend loop updates the current buffer
      ctrl_ |= CommandCtrl::DrawDataBuffer;
      state_.dataBuffer = new_block.buffer;

      gpu_input_.ptr = new_block.ptr;
      gpu_input_.buffer = new_block.buffer;
    }

    gpu_input_.offset = new_block.offset + num_bytes;
    offset = new_block.offset;
  }

  return offset;
}

template <typename T>
T * RasterPassEncoder::drawData()
{
  return (T *)drawData((u32)sizeof(T));
}

template <typename T>
void RasterPassEncoder::drawData(T v)
{
  *drawData<T>() = v;
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

  ctrl_ |= draw_type;

  u32 *ctrl_out = writer_.reserve(gpu_);

  if ((ctrl_ & DrawShader) != None) {
    writer_.id(gpu_, state_.shader);
  }

  if ((ctrl_ & DrawParamBlock0) != None) {
    writer_.id(gpu_, state_.paramBlocks[0]);
  }

  if ((ctrl_ & DrawParamBlock1) != None) {
    writer_.id(gpu_, state_.paramBlocks[1]);
  }

  if ((ctrl_ & DrawParamBlock2) != None) {
    writer_.id(gpu_, state_.paramBlocks[2]);
  }

  if ((ctrl_ & DrawDataBuffer) != None) {
    writer_.id(gpu_, state_.dataBuffer);
  }

  if ((ctrl_ & DrawDataOffset) != None) {
    writer_.writeU32(gpu_, state_.dataOffset);
  }

  if ((ctrl_ & DrawVertexBuffer0) != None) {
    writer_.id(gpu_, state_.vertexBuffer[0]);
  }

  if ((ctrl_ & DrawVertexBuffer1) != None) {
    writer_.id(gpu_, state_.vertexBuffer[1]);
  }

  if ((ctrl_ & (DrawIndexBuffer32 | DrawIndexBuffer16)) != None) {
    writer_.id(gpu_, state_.indexBuffer);
  }

  if (state_.indexOffset != index_offset) {
    ctrl_ |= DrawIndexOffset;
    state_.indexOffset = index_offset;
    writer_.writeU32(gpu_, index_offset);
  }

  if (state_.numTriangles != num_triangles) {
    ctrl_ |= DrawNumTriangles;
    state_.numTriangles = num_triangles;
    writer_.writeU32(gpu_, num_triangles);
  }

  if (state_.vertexOffset != vertex_offset) {
    ctrl_ |= DrawVertexOffset;
    state_.vertexOffset = vertex_offset;
    writer_.writeU32(gpu_, vertex_offset);
  }

  if (state_.instanceOffset != instance_offset) {
    ctrl_ |= DrawInstanceOffset;
    state_.instanceOffset = instance_offset;
    writer_.writeU32(gpu_, instance_offset);
  }

  if (state_.numInstances != num_instances) {
    ctrl_ |= DrawNumInstances;
    state_.numInstances = num_instances;
    writer_.writeU32(gpu_, num_instances);
  }

  *ctrl_out = (u32)ctrl_;

  ctrl_ = None;
}

RasterPassEncoder::RasterPassEncoder(GPURuntime *gpu,
                                     CommandWriter writer,
                                     GPUQueue queue,
                                     GPUTmpInputBlock gpu_input)
  : gpu_(gpu),
    writer_(writer),
    queue_(queue),
    gpu_input_(gpu_input),
    ctrl_(CommandCtrl::None),
    state_()
{}

CopyCommand::CopyCommand()
  : data { 0, 0, 0, 0, 0 }
{}

void CopyPassEncoder::copyBufferToBuffer(Buffer src, Buffer dst,
                                         u32 src_offset, u32 dst_offset,
                                         u32 num_bytes)
{
  using enum CommandCtrl;

  ctrl_ |= CopyCmdBufferToBuffer;

  u32 *ctrl_out = writer_.reserve(gpu_);

  if (u32 hdl = src.uint(); hdl != state_.data[0]) {
    ctrl_ |= CopyB2BSrcBuffer;
    state_.data[0] = hdl;
    writer_.writeU32(gpu_, hdl);
  }

  if (u32 hdl = dst.uint(); hdl != state_.data[1]) {
    ctrl_ |= CopyB2BDstBuffer;
    state_.data[1] = hdl;
    writer_.writeU32(gpu_, hdl);
  }

  if (src_offset != state_.data[2]) {
    ctrl_ |= CopyB2BSrcOffset;
    state_.data[2] = src_offset;
    writer_.writeU32(gpu_, src_offset);
  }

  if (dst_offset != state_.data[3]) {
    ctrl_ |= CopyB2BDstOffset;
    state_.data[3] = dst_offset;
    writer_.writeU32(gpu_, dst_offset);
  }

  if (num_bytes != state_.data[4]) {
    ctrl_ |= CopyB2BNumBytes;
    state_.data[4] = num_bytes;
    writer_.writeU32(gpu_, num_bytes);
  }

  *ctrl_out = (u32)ctrl_;
  ctrl_ = None;
}

void CopyPassEncoder::copyBufferToTexture(Buffer src,
                                          Texture dst,
                                          u32 src_offset,
                                          u32 dst_mip_level)
{
  using enum CommandCtrl;

  ctrl_ |= CopyCmdBufferToTexture;

  u32 *ctrl_out = writer_.reserve(gpu_);

  if (u32 hdl = src.uint(); hdl != state_.data[0]) {
    ctrl_ |= CopyB2TSrcBuffer;
    state_.data[0] = hdl;
    writer_.writeU32(gpu_, hdl);
  }

  if (u32 hdl = dst.uint(); hdl != state_.data[1]) {
    ctrl_ |= CopyB2TDstTexture;
    state_.data[1] = hdl;
    writer_.writeU32(gpu_, hdl);
  }

  if (src_offset != state_.data[2]) {
    ctrl_ |= CopyB2TSrcOffset;
    state_.data[2] = src_offset;
    writer_.writeU32(gpu_, src_offset);
  }

  if (dst_mip_level != state_.data[3]) {
    ctrl_ |= CopyB2TDstMipLevel;
    state_.data[3] = dst_mip_level;
    writer_.writeU32(gpu_, dst_mip_level);
  }

  *ctrl_out = (u32)ctrl_;
  ctrl_ = None;
}

void CopyPassEncoder::copyTextureToBuffer(Texture src,
                                          Buffer dst,
                                          u32 src_mip_level,
                                          u32 dst_offset)
{
  using enum CommandCtrl;

  ctrl_ |= CopyCmdTextureToBuffer;

  u32 *ctrl_out = writer_.reserve(gpu_);

  if (u32 hdl = src.uint(); hdl != state_.data[0]) {
    ctrl_ |= CopyT2BSrcTexture;
    state_.data[0] = hdl;
    writer_.writeU32(gpu_, hdl);
  }

  if (u32 hdl = dst.uint(); hdl != state_.data[1]) {
    ctrl_ |= CopyT2BDstBuffer;
    state_.data[1] = hdl;
    writer_.writeU32(gpu_, hdl);
  }

  if (src_mip_level != state_.data[2]) {
    ctrl_ |= CopyT2BSrcMipLevel;
    state_.data[2] = src_mip_level;
    writer_.writeU32(gpu_, src_mip_level);
  }

  if (dst_offset != state_.data[3]) {
    ctrl_ |= CopyT2BDstOffset;
    state_.data[3] = dst_offset;
    writer_.writeU32(gpu_, dst_offset);
  }

  *ctrl_out = (u32)ctrl_;
  ctrl_ = None;
}

void CopyPassEncoder::clearBuffer(Buffer buffer, u32 offset, u32 num_bytes)
{
  using enum CommandCtrl;

  ctrl_ |= CopyCmdBufferClear;

  u32 *ctrl_out = writer_.reserve(gpu_);

  if (u32 hdl = buffer.uint(); hdl != state_.data[0]) {
    ctrl_ |= CopyClearBuffer;
    state_.data[0] = hdl;
    writer_.writeU32(gpu_, hdl);
  }

  if (offset != state_.data[1]) {
    ctrl_ |= CopyClearOffset;
    state_.data[1] = offset;
    writer_.writeU32(gpu_, offset);
  }

  if (num_bytes != state_.data[2]) {
    ctrl_ |= CopyClearNumBytes;
    state_.data[2] = num_bytes;
    writer_.writeU32(gpu_, num_bytes);
  }

  *ctrl_out = (u32)ctrl_;
  ctrl_ = None;
}

CopyPassEncoder::CopyPassEncoder(GPURuntime *gpu, CommandWriter writer)
  : gpu_(gpu),
    writer_(writer),
    ctrl_(CommandCtrl::None),
    state_()
{}

void CommandEncoder::beginEncoding()
{
  cmd_writer_.cmds_ = cmds_head_;
  cmd_writer_.offset_ = 0;

  gpu_input_ = GPUTmpInputBlock {};
}

void CommandEncoder::endEncoding()
{
  cmd_writer_.ctrl(gpu_, CommandCtrl::None);
}

RasterPassEncoder CommandEncoder::beginRasterPass(
  RasterPass render_pass)
{
  cmd_writer_.ctrl(gpu_, CommandCtrl::RasterPass);
  cmd_writer_.id(gpu_, render_pass);

  return RasterPassEncoder(gpu_, cmd_writer_, queue_, gpu_input_);
}

void CommandEncoder::endRasterPass(RasterPassEncoder &render_enc)
{
  cmd_writer_ = render_enc.writer_;
  cmd_writer_.ctrl(gpu_, CommandCtrl::None);
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
  cmd_writer_.ctrl(gpu_, CommandCtrl::CopyPass);
  return CopyPassEncoder(gpu_, cmd_writer_);
}

void CommandEncoder::endCopyPass(CopyPassEncoder &copy_enc)
{
  cmd_writer_ = copy_enc.writer_;
  cmd_writer_.ctrl(gpu_, CommandCtrl::None);
}

Buffer GPURuntime::createBuffer(BufferInit init,
                                GPUQueue tx_queue)
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
                                  GPUQueue tx_queue)
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
                               GPUQueue tx_queue)
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
                                GPUQueue tx_queue)
{
  createGPUResources(0, nullptr, nullptr,
                     num_textures, texture_inits, handles_out, tx_queue);
}

void GPURuntime::destroyTextures(i32 num_textures, Texture *textures)
{
  destroyGPUResources(0, nullptr, num_textures, textures);
}

void GPURuntime::createGPUResources(GPUResourcesCreate create,
                                    GPUQueue tx_queue)
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

CommandEncoder::CommandEncoder(GPURuntime *gpu,
                               GPUQueue queue)
  : gpu_(gpu),
    cmds_head_(gpu->allocCommandBlock()),
    cmd_writer_(),
    queue_(queue),
    gpu_input_()
{}

GPUQueue GPURuntime::getMainQueue()
{
  return GPUQueue { 0 };
}

CommandEncoder GPURuntime::createCommandEncoder(GPUQueue queue)
{
  return CommandEncoder(this, queue);
}

void GPURuntime::destroyCommandEncoder(CommandEncoder &encoder)
{
  deallocCommandBlocks(encoder.cmds_head_);
}

void GPURuntime::submit(GPUQueue queue, CommandEncoder &enc)
{
  submit(queue, enc.cmds_head_);
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
