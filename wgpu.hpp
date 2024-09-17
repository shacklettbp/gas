#pragma once

#include "gas.hpp"
#include "backend_common.hpp"

#include <webgpu/webgpu_cpp.h>

namespace gas::webgpu {

class Backend;

class BackendSwapchain {
public:
  wgpu::Surface surface;
  wgpu::TextureView view;
  Texture reservedHandle;
};

struct BackendTexture {
  wgpu::TextureView view;
};

struct BackendTextureCold {
  wgpu::Texture texture;
  u32 baseWidth;
  u32 baseHeight;
  u32 baseDepth;
  u32 numBytesPerTexel;
};

struct BackendParamBlockType {
  wgpu::BindGroupLayout layout;
};

struct BackendDepthAttachmentConfig {
  wgpu::TextureFormat format;
  wgpu::LoadOp loadOp;
  wgpu::StoreOp storeOp;
  float clearValue;
};

struct BackendColorAttachmentConfig {
  wgpu::TextureFormat format;
  wgpu::LoadOp loadOp;
  wgpu::StoreOp storeOp;
  math::Vector4 clearValue;
};

struct BackendRasterPassConfig {
  BackendDepthAttachmentConfig depthAttachment;
  BackendColorAttachmentConfig colorAttachments[MAX_COLOR_ATTACHMENTS];
  i32 numColorAttachments;
};

struct BackendDepthAttachment {
  wgpu::TextureView view;
  wgpu::LoadOp loadOp;
  wgpu::StoreOp storeOp;
  float clearValue;
};

struct BackendColorAttachment {
  wgpu::TextureView view;
  wgpu::LoadOp loadOp;
  wgpu::StoreOp storeOp;
  math::Vector4 clearValue;
};

// FIXME: this will be gigantic
struct BackendRasterPass {
  BackendDepthAttachment depthAttachment;
  BackendColorAttachment colorAttachments[MAX_COLOR_ATTACHMENTS];
  i32 numColorAttachments;
  i32 swapchainAttachmentIndex;
  Swapchain swapchain;
};

struct BackendRasterShader {
  wgpu::RenderPipeline pipeline;
  i32 perDrawBindGroupSlot;
};

struct NoMetadata {};

constexpr inline u32 TMP_BUFFER_SIZE = 64 * 1024 * 1024;
constexpr inline i32 MAX_TMP_BUFFERS_PER_QUEUE = 16;
constexpr inline i32 MAX_TMP_STAGING_BUFFERS = 64;
constexpr inline u32 NUM_BLOCKS_PER_TMP_BUFFER =
  TMP_BUFFER_SIZE / GPUTmpInputBlock::BLOCK_SIZE;

struct StagingBelt {
  struct CallbackState {
    Backend *backend;
    i32 idx;
  };

  std::array<wgpu::Buffer, MAX_TMP_STAGING_BUFFERS> buffers;
  std::array<u8 *, MAX_TMP_STAGING_BUFFERS> ptrs;

  std::array<CallbackState, MAX_TMP_STAGING_BUFFERS> cbStates;

  std::array<i32, MAX_TMP_STAGING_BUFFERS> freeList;
  i32 numFree;
  i32 numAllocated;

  u32 stagingBufferHandlesBase;

  SpinLock lock;
};

struct TmpDynUniformBuffer {
  wgpu::Buffer buffer;
  wgpu::BindGroup bindGroup;
};

struct GPUTmpInputState {
  std::array<TmpDynUniformBuffer, MAX_TMP_BUFFERS_PER_QUEUE>
      dynUniformBuffers;

  std::array<i32, MAX_TMP_BUFFERS_PER_QUEUE> activeStagingBuffers;

  std::array<i32, MAX_TMP_STAGING_BUFFERS> stagingBeltIdxToDynUniform;

  alignas(MADRONA_CACHE_LINE) u64 curStagingRange;
  alignas(MADRONA_CACHE_LINE) u32 numDynUniforms;
  u32 maxNumUsedDynUniforms;
  SpinLock lock {};
};

struct BackendQueueData {
  GPUTmpInputState gpuTmpInput;
};

class WebGPUAPI final : public GPUAPI {
public:
  wgpu::Instance inst;
  WGPUDevice destroyingDevice;
  bool errorsAreFatal;

  static GPUAPI * init(const APIConfig &cfg);
  void shutdown() final;

  Surface createSurface(void *os_data, i32 width, i32 height) final;
  void destroySurface(Surface surface) final;

  GPURuntime * createRuntime(
      i32 gpu_idx, Span<const Surface> surfaces) final;
  void destroyRuntime(GPURuntime *runtime) final;

  ShaderByteCodeType backendShaderByteCodeType() final;
};

using TextureTable = ResourceTable<
    Texture,
    BackendTexture, 
    BackendTextureCold
  >;
using SamplerTable = ResourceTable<
    Sampler,
    wgpu::Sampler, 
    NoMetadata 
  >;

using BufferTable = ResourceTable<
    Buffer,
    wgpu::Buffer,
    BufferInit
  >;

using ParamBlockTypeTable = ResourceTable<
    ParamBlockType,
    BackendParamBlockType,
    ParamBlockTypeID
  >;

using ParamBlockTable = ResourceTable<
    ParamBlock,
    wgpu::BindGroup,
    NoMetadata
  >;

using RasterPassInterfaceTable = ResourceTable<
    RasterPassInterface,
    BackendRasterPassConfig,
    RasterPassInterfaceID
  >;
using RasterPassTable = ResourceTable<
    RasterPass,
    BackendRasterPass,
    NoMetadata
  >;

using RasterShaderTable = ResourceTable<
    RasterShader,
    gas::webgpu::BackendRasterShader,
    NoMetadata
  >;

using SwapchainStorage = InlineArrayFreeList<BackendSwapchain, 1>;

struct BackendLimits {
  u32 maxNumUniformBytes;
};

class Backend final : public BackendCommon {
public:
  wgpu::Adapter adapter;
  wgpu::Device dev;
  wgpu::Queue queue;
  wgpu::Instance inst;
  BackendLimits limits;

  StagingBelt stagingBelt {};
  wgpu::BindGroupLayout tmpDynamicUniformLayout;
  std::array<BackendQueueData, 2> queueDatas;

  BufferTable buffers {};
  TextureTable textures {};

  SamplerTable samplers {};

  ParamBlockTypeTable paramBlockTypes {};
  ParamBlockTable paramBlocks {};

  RasterPassInterfaceTable rasterPassInterfaces {};
  RasterPassTable rasterPasses {};

  RasterShaderTable rasterShaders {};

  SwapchainStorage swapchains {};

  inline Backend(wgpu::Adapter &&adapter,
                 wgpu::Device &&dev,
                 wgpu::Queue &&queue,
                 wgpu::Instance &inst,
                 BackendLimits &limits,
                 bool errors_are_fatal);

  inline void destroy();

  void createGPUResources(i32 num_buffers,
                          const BufferInit *buffer_inits,
                          Buffer *buffer_handles_out,
                          i32 num_textures,
                          const TextureInit *texture_inits,
                          Texture *texture_handles_out,
                          GPUQueue tx_queue = {}) final;

  void destroyGPUResources(i32 num_buffers,
                           const Buffer *buffer_hdls,
                           i32 num_textures,
                           const Texture *texture_hdls) final;

  Buffer createStagingBuffer(u32 num_bytes) final;
  void destroyStagingBuffer(Buffer staging) final;

  void prepareStagingBuffers(
      i32 num_buffers, Buffer *buffer_hdls, void **mapped_out) final;
  void flushStagingBuffers(i32 num_buffers, Buffer *buffers) final;

  Buffer createReadbackBuffer(u32 num_bytes) final;
  void destroyReadbackBuffer(Buffer readback) final;

  void * beginReadback(Buffer buffer) final;
  void endReadback(Buffer buffer) final;
   
  Buffer createStandaloneBuffer(
      BufferInit init, bool external_export = false) final;
  void destroyStandaloneBuffer(Buffer buffer) final;

  Texture createStandaloneTexture(TextureInit init) final;
  void destroyStandaloneTexture(Texture texture) final;

  void createSamplers(i32 num_samplers,
                      const SamplerInit *sampler_inits,
                      Sampler *handles_out) final;
  void destroySamplers(i32 num_samplers, Sampler *handles) final;

  void createParamBlockTypes(
    i32 num_types,
    const ParamBlockTypeInit *blk_types,
    ParamBlockType *handles_out) final;

  void destroyParamBlockTypes(
      i32 num_types, ParamBlockType *handles) final;

  void createParamBlocks(
      i32 num_blks,
      const ParamBlockInit *blk_inits,
      ParamBlock *handles_out) final;

  void destroyParamBlocks(
      i32 num_blks, ParamBlock *blks) final;

  void createRasterPassInterfaces(
      i32 num_interfaces,
      const RasterPassInterfaceInit *interface_inits,
      RasterPassInterface *handles_out) final;

  void destroyRasterPassInterfaces(
      i32 num_interfaces, RasterPassInterface *handles) final;

  void createRasterPasses(
      i32 num_interfaces,
      const RasterPassInit *pass_inits,
      RasterPass *handles_out) final;
  void destroyRasterPasses(
      i32 num_passes, RasterPass *handles) final;

  void createRasterShaders(i32 num_shaders,
                          const RasterShaderInit *shader_inits,
                          RasterShader *handles_out) final;
  void destroyRasterShaders(i32 num_shaders, RasterShader *handles) final;

  Swapchain createSwapchain(Surface surface,
                            SwapchainProperties *properties) final;
  void destroySwapchain(Swapchain swapchain) final;
  AcquireSwapchainResult acquireSwapchainImage(Swapchain swapchain) final;
  void presentSwapchainImage(Swapchain swapchain) final;

  void waitUntilReady(GPUQueue queue_hdl) final;
  void waitUntilWorkFinished(GPUQueue queue_hdl) final;

  void waitUntilIdle() final;

  ShaderByteCodeType backendShaderByteCodeType() final;

  GPUTmpInputBlock allocGPUTmpInputBlock(GPUQueue queue_hdl) final;

  inline BackendRasterPassConfig * getRasterPassConfigByID(
      RasterPassInterfaceID id);

  inline wgpu::BindGroupLayout getBindGroupLayoutByParamBlockTypeID(
      ParamBlockTypeID id);

  i32 allocStagingBufferFromBelt();
  static void returnBufferToStagingBeltCallback(
    wgpu::MapAsyncStatus async_status, const char *msg, void *user_data);

  //void allocTmpDynUniformBlock(BackendQueueData &queue_data);

  inline i32 allocTmpDynUniformBuffer(
      GPUTmpInputState &state);

  i32 unmapActiveStagingBuffers(GPUTmpInputState &gpu_tmp_input);
  void mapActiveStagingBuffers(GPUTmpInputState &gpu_tmp_input,
                               i32 num_active_staging_buffers);

  void submit(GPUQueue queue_hdl, FrontendCommands *cmds) final;
};

}
