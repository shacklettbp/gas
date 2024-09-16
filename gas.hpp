#pragma once

#include "namespace.hpp"

#include <madrona/stack_alloc.hpp>

#include "uuid.hpp"

namespace gas {

class RasterPassEncoder;
class ComputePassEncoder;
class CopyPassEncoder;
class CommandEncoder;
class GPURuntime;
class ShaderCompiler;

struct APIConfig {
  bool enableValidation = false;
  bool runtimeErrorsAreFatal = false;
  bool enablePresent = false;
  Span<const char *const> apiExtensions = {};
};

// Constants
constexpr inline i32 MAX_COLOR_ATTACHMENTS = 8;
constexpr inline i32 MAX_BIND_GROUPS_PER_SHADER = 4;
constexpr inline i32 MAX_BINDINGS_PER_GROUP = 128;

// Resource Handles
template <typename T>
struct GenHandle {
  constexpr inline bool null() const;
  constexpr inline u32 uint() const;
  static constexpr inline T fromUInt(u32 v);

  constexpr inline friend bool operator==(T a, T b)
  {
    return a.id == b.id && a.gen == b.gen;
  }
};

struct Texture : GenHandle<Texture> {
  u16 gen = 0;
  u16 id = 0;
};

struct Sampler : GenHandle<Sampler> {
  u16 gen = 0;
  u16 id = 0;
};

struct Buffer : GenHandle<Buffer> {
  u16 gen = 0;
  u16 id = 0;
};

struct ParamBlockType : GenHandle<ParamBlockType> {
  u16 gen = 0;
  u16 id = 0;
};

struct ParamBlock : GenHandle<ParamBlock> {
  u16 gen = 0;
  u16 id = 0;
};

struct RasterPassInterface : GenHandle<RasterPassInterface> {
  u16 gen = 0;
  u16 id = 0;
};

struct RasterPass : GenHandle<RasterPass> {
  u16 gen = 0;
  u16 id = 0;
};

struct RasterShader : GenHandle<RasterShader> {
  u16 gen = 0;
  u16 id = 0;
};

struct ComputeShader : GenHandle<ComputeShader> {
  u16 gen = 0;
  u16 id = 0;
};

struct ResourceAlloc : GenHandle<ResourceAlloc> {
  u16 gen = 0;
  u16 id = 0;
};

struct BackendHandle {
  union {
    void *ptr;
    uint64_t id;
  };
};

struct StagingHandle {
  Buffer buffer = {};
  u32 offset = 0;
  void *ptr = nullptr;
};

struct MappedTmpBuffer {
  Buffer buffer;
  u32 offset;
  u8 *ptr;
};

struct GPUQueue {
  i32 id = -1;
};

enum class ErrorStatus : u32 {
  None        = 0,
  TableFull   = 1 << 0,
  OutOfMemory = 1 << 1,
  NullBuffer  = 1 << 2,
};

// Buffer setup
enum class BufferUsage : u16 {
  CopySrc       = 1 << 0,
  CopyDst       = 1 << 1,
  DrawIndex     = 1 << 2,
  DrawVertex    = 1 << 3,
  ShaderUniform = 1 << 4,
  ShaderStorage = 1 << 5,
  CPUAccessible = 1 << 6,
};
inline BufferUsage & operator|=(BufferUsage &a, BufferUsage b);
inline BufferUsage operator|(BufferUsage a, BufferUsage b);
inline BufferUsage & operator&=(BufferUsage &a, BufferUsage b);
inline BufferUsage operator&(BufferUsage a, BufferUsage b);

struct BufferInit {
  u32 numBytes;
  BufferUsage usage = BufferUsage::ShaderUniform;
  StagingHandle initData = {};
};

// Texture Setup
enum class TextureFormat : u16 {
  None,
  RGBA8_UNorm,
  RGBA8_SRGB,
  BGRA8_UNorm,
  BGRA8_SRGB,
  Depth32_Float,
};

enum class TextureUsage : u16 {
  None             = 0,
  CopySrc          = 1 << 0,
  CopyDst          = 1 << 1,
  ShaderSampled    = 1 << 2,
  ShaderStorage    = 1 << 3,
  ColorAttachment  = 1 << 4,
  DepthAttachment  = 1 << 5,
};
inline TextureUsage & operator|=(TextureUsage &a, TextureUsage b);
inline TextureUsage operator|(TextureUsage a, TextureUsage b);
inline TextureUsage & operator&=(TextureUsage &a, TextureUsage b);
inline TextureUsage operator&(TextureUsage a, TextureUsage b);

struct TextureInit {
  TextureFormat format;
  u16 width;
  u16 height;
  u16 depth = 0;
  u16 numMipLevels = 1;
  TextureUsage usage = TextureUsage::ShaderSampled;
  StagingHandle initData = {};
};

struct GPUResourcesCreate {
  Span<const BufferInit> buffers = {};
  Span<Buffer> buffersOut = {};
  Span<const TextureInit> textures = {};
  Span<Texture> texturesOut = {};
};

struct GPUResourcesDestroy {
  Span<const Buffer> buffers = {};
  Span<const Texture> textures = {};
};

// Sampler setup
enum class SamplerAddressMode : u16 {
  Clamp,
  Repeat,
  MirrorRepeat,
  InheritUMode,
};

enum class SamplerFilterMode : u16 {
  Nearest,
  Linear,
};

struct SamplerInit {
  union {
    SamplerAddressMode addressMode;
    SamplerAddressMode addressModeU;
  };
  SamplerAddressMode addressModeV = SamplerAddressMode::InheritUMode;
  SamplerAddressMode addressModeW = SamplerAddressMode::InheritUMode;

  SamplerFilterMode mipmapFilterMode = SamplerFilterMode::Linear;
  SamplerFilterMode magnificationFilterMode = SamplerFilterMode::Linear;
  SamplerFilterMode minificationFilterMode = SamplerFilterMode::Linear;

  u16 anisotropy = 16;
  float mipClamp[2] = { 0.f, 32.f };
};

// Creating bind groups
struct ParamBlockTypeID {
  inline ParamBlockTypeID(UUID uuid);
  inline ParamBlockTypeID(ParamBlockType blk_type);

  // For backend use
  inline bool isUUID() const;
  inline UUID asUUID() const;
  inline ParamBlockType asHandle() const ;
  u64 id[2];
};

enum class ShaderStage : u16 {
  Vertex     = 1 << 0,
  Fragment   = 1 << 1,
  Compute    = 1 << 2,
};
inline ShaderStage & operator|=(ShaderStage &a, ShaderStage b);
inline ShaderStage operator|(ShaderStage a, ShaderStage b);
inline ShaderStage & operator&=(ShaderStage &a, ShaderStage b);
inline ShaderStage operator&(ShaderStage a, ShaderStage b);

enum class BufferBindingType : u16 {
  Uniform,
  DynamicUniform,
  Storage,
  StorageRW,
};

enum class TextureBindingType : u16 {
  Texture1D,
  Texture2D,
  Texture3D,
  DepthTexture2D,
};

struct BufferBindingConfig {
  BufferBindingType type;
  i32 bindLocation = -1;
  ShaderStage shaderUsage =
    ShaderStage::Vertex | ShaderStage::Fragment | ShaderStage::Compute;
  u16 numBuffers = 1;
};

struct TextureBindingConfig {
  TextureBindingType type;
  i32 bindLocation = -1;
  ShaderStage shaderUsage =
    ShaderStage::Vertex | ShaderStage::Fragment | ShaderStage::Compute;
  u16 numTextures = 1;
};

struct SamplerBindingConfig {
  i32 bindLocation = -1;
  ShaderStage shaderUsage =
    ShaderStage::Vertex | ShaderStage::Fragment | ShaderStage::Compute;
  u16 numSamplers = 1;
};

struct ParamBlockTypeInit {
  UUID uuid;
  Span<const BufferBindingConfig> buffers = {};
  Span<const TextureBindingConfig> textures = {};
  Span<const SamplerBindingConfig> samplers = {};
};

struct BufferBinding {
  Buffer buffer;
  u32 offset = 0;
  u32 numBytes = 0xFFFF'FFFF;
};

struct ParamBlockInit {
  ParamBlockTypeID typeID;
  Span<const BufferBinding> buffers = {};
  Span<const Texture> textures = {};
  Span<const Sampler> samplers = {};
};

// Creating Raster Pass
struct RasterPassInterfaceID {
  inline RasterPassInterfaceID(UUID uuid);
  inline RasterPassInterfaceID(RasterPassInterface interface);

  // For backend use
  inline bool isUUID() const;
  inline UUID asUUID() const;
  inline RasterPassInterface asHandle() const ;
  u64 id[2];
};

enum class AttachmentLoadMode : u16 {
  Load,
  Clear,
  Undefined,
};

enum class AttachmentStoreMode : u16 {
  Store,
  Undefined,
};

struct DepthAttachmentConfig {
  TextureFormat format = TextureFormat::None;
  AttachmentLoadMode loadMode = AttachmentLoadMode::Clear;
  AttachmentStoreMode storeMode = AttachmentStoreMode::Store;
  float clearValue = 0.f;
};

struct ColorAttachmentConfig {
  TextureFormat format;
  AttachmentLoadMode loadMode = AttachmentLoadMode::Clear;
  AttachmentStoreMode storeMode = AttachmentStoreMode::Store;
  Vector4 clearValue = Vector4::zero();
};

struct RasterPassInterfaceInit {
  UUID uuid;
  DepthAttachmentConfig depthAttachment = {};
  Span<const ColorAttachmentConfig> colorAttachments = {};
};

struct RasterPassInit {
  RasterPassInterface interface;
  Texture depthAttachment = {};
  Span<const Texture> colorAttachments = {};
};

// Creating shaders
// Loading / compiling shader source code
enum class ShaderByteCodeType : u32 {
  SPIRV,
  MTLLib,
  DXIL,
  WGSL,
};

struct ShaderByteCode {
  void *data;
  int64_t numBytes;
};

enum class DepthCompare : u16 {
  GreaterOrEqual,
  Disabled,
};

enum class CullMode : u16 {
  None,
  FrontFace,
  BackFace,
};

struct RasterHWConfig {
  DepthCompare depthCompare = DepthCompare::GreaterOrEqual;
  bool writeDepth = true;
  CullMode cullMode = CullMode::BackFace;
};

struct RasterShaderInit {
  ShaderByteCode byteCode;
  const char *vertexEntry;
  const char *fragmentEntry;
  RasterPassInterfaceID rasterPass;
  Span<const ParamBlockTypeID> paramBlockTypes = {};
  uint32_t numPerDrawBytes = 0;
  RasterHWConfig rasterConfig = {};
};

struct ComputeShaderInit {
  Span<const uint8_t> byteCode;
  const char *entry;
  Span<const ParamBlockTypeID> paramBlockTypes = {};
};

// Presenting Handles

struct Surface {
  BackendHandle hdl;
  i32 width;
  i32 height;
};

struct Swapchain {
  i32 id = 0;
  inline Texture proxyAttachment() const;
};

enum class SwapchainStatus : u32 {
  Valid,
  Suboptimal,
  Invalid,
};

struct AcquireSwapchainResult {
  Texture texture;
  SwapchainStatus status;
};

struct SwapchainProperties {
  TextureFormat format;
  bool supportsCopyDst;
};

constexpr inline u32 TABLE_FULL = 0xFFFF'FFFF;

enum class CommandCtrl : u32 {
  None                   = 0,
  RasterPass             = 1 << 0,
  ComputePass            = 1 << 1,
  CopyPass               = 1 << 2,

  Draw                   = 1 << 0,
  DrawIndexed            = 1 << 1,
  DrawShader             = 1 << 2,
  DrawParamBlock0        = 1 << 3,
  DrawParamBlock1        = 1 << 4,
  DrawParamBlock2        = 1 << 5,
  DrawDataBuffer         = 1 << 6,
  DrawDataOffset         = 1 << 7,
  DrawVertexBuffer0      = 1 << 8,
  DrawVertexBuffer1      = 1 << 9,
  DrawIndexBuffer32      = 1 << 10,
  DrawIndexBuffer16      = 1 << 11,
  DrawIndexOffset        = 1 << 12,
  DrawNumTriangles       = 1 << 13,
  DrawVertexOffset       = 1 << 14,
  DrawInstanceOffset     = 1 << 15,
  DrawNumInstances       = 1 << 16,

  Dispatch               = 1 << 0,
  ComputeShader          = 1 << 1,
  ComputeParamBlock0     = 1 << 2,
  ComputeParamBlock1     = 1 << 3,
  ComputeParamBlock2     = 1 << 4,
  ComputeNumBlocksX      = 1 << 5,
  ComputeNumBlocksY      = 1 << 6,
  ComputeNumBlocksZ      = 1 << 7,

  CopyCmdBufferToBuffer  = 1 << 0,
  CopyCmdBufferToTexture = 1 << 1,
  CopyCmdTextureToBuffer = 1 << 2,
  CopyCmdBufferClear     = 1 << 3,

  CopyB2BSrcBuffer       = 1 << 4,
  CopyB2BDstBuffer       = 1 << 5,
  CopyB2BSrcOffset       = 1 << 6,
  CopyB2BDstOffset       = 1 << 7,
  CopyB2BNumBytes        = 1 << 8,

  CopyB2TSrcBuffer       = 1 << 4,
  CopyB2TDstTexture      = 1 << 5,
  CopyB2TSrcOffset       = 1 << 6,
  CopyB2TDstMipLevel     = 1 << 7,

  CopyT2BSrcTexture      = 1 << 4,
  CopyT2BDstBuffer       = 1 << 5,
  CopyT2BSrcMipLevel     = 1 << 6,
  CopyT2BDstOffset       = 1 << 7,

  CopyClearBuffer        = 1 << 4,
  CopyClearOffset        = 1 << 5,
  CopyClearNumBytes      = 1 << 6,
};
inline CommandCtrl & operator|=(CommandCtrl &a, CommandCtrl b);
inline CommandCtrl operator|(CommandCtrl a, CommandCtrl b);
inline CommandCtrl & operator&=(CommandCtrl &a, CommandCtrl b);
inline CommandCtrl operator&(CommandCtrl a, CommandCtrl b);

struct DrawCommand {
  RasterShader shader = {};
  ParamBlock paramBlocks[3] = {};
  Buffer vertexBuffer[2] = {};
  Buffer indexBuffer = {};
  Buffer dataBuffer = {};
  u32 dataOffset = 0;
  u32 indexOffset = 0;
  u32 numTriangles = 0;
  u32 vertexOffset = 0;
  u32 instanceOffset = 0;
  u32 numInstances = 1;
};

struct ComputeCommand {
  ComputeShader shader = {};
  ParamBlock paramBlocks[3] = {};
  u32 numBlocksX = 0;
  u32 numBlocksY = 1;
  u32 numBlocksZ = 1;
};

struct CopyCommand {
  inline CopyCommand();
  // Due to strict aliasing rules we can't use a union over the different
  // sub commands to track data changes
  std::array<u32, 5> data;
};

// Used by backends
struct FrontendCommands {
  std::array<u32, 1024 - 2> data;
  FrontendCommands *next;
};

static_assert(sizeof(FrontendCommands) == 4096);

class CommandWriter {
public:
  inline u32 * reserve(GPURuntime *gpu);
  inline void writeU32(GPURuntime *gpu, u32 v);

  template <typename T>
  inline void id(GPURuntime *gpu, T id);
  inline void ctrl(GPURuntime *gpu, CommandCtrl ctrl);

private:
  FrontendCommands *cmds_;
  u32 offset_;

friend class CommandEncoder;
friend class GPURuntime;
};

struct GPUTmpInputBlock
{
  inline u32 alloc(u32 num_bytes);
  inline bool blockFull() const;

  static constexpr inline u32 BLOCK_SIZE = 4 * 1024 * 1024;

  uint8_t *ptr = nullptr;
  Buffer buffer {};
  u32 offset = BLOCK_SIZE + 1; // blockFull() is true when offset > BLOCK_SIZE
};

class RasterPassEncoder {
public:
  inline void setShader(RasterShader shader);
  inline void setParamBlock(i32 idx, ParamBlock param_block);
  inline void setVertexBuffer(i32 idx, Buffer buffer);
  inline void setIndexBufferU32(Buffer buffer);
  inline void setIndexBufferU16(Buffer buffer);

  inline MappedTmpBuffer tmpBuffer(u32 num_bytes);

  inline void * drawData(u32 num_bytes);
  template <typename T> T * drawData();
  template <typename T> void drawData(T v);

  inline void draw(u32 vertex_offset, u32 num_triangles);
  inline void drawIndexed(u32 vertex_offset,
                          u32 index_offset, u32 num_triangles);

  inline void drawInstanced(u32 vertex_offset, u32 num_triangles,
                            u32 instance_offset, u32 num_instances);
  inline void drawIndexedInstanced(u32 vertex_offset,
                                   u32 index_offset, u32 num_triangles,
                                   u32 instance_offset, u32 num_instances);

private:
  inline void encodeDraw(CommandCtrl draw_type, u32 vertex_offset,
                         u32 index_offset, u32 num_triangles,
                         u32 instance_offset, u32 num_instances);

  inline u32 allocGPUTmpInput(u32 num_bytes);

  inline RasterPassEncoder(GPURuntime *gpu,
                           CommandWriter writer,
                           GPUQueue queue,
                           GPUTmpInputBlock gpu_input);

  GPURuntime *gpu_;
  CommandWriter writer_;
  GPUQueue queue_;
  GPUTmpInputBlock gpu_input_;
  CommandCtrl ctrl_;
  DrawCommand state_;

friend class CommandEncoder;
};

class ComputePassEncoder {
friend class CommandEncoder;
};

class CopyPassEncoder {
public:
  inline void copyBufferToBuffer(Buffer src, Buffer dst,
                                 u32 src_offset, u32 dst_offset,
                                 u32 num_bytes);

  inline void copyBufferToTexture(Buffer src, Texture dst,
                                  u32 src_offset = 0,
                                  u32 dst_mip_level = 0);

  inline void copyTextureToBuffer(Texture src, Buffer dst,
                                  u32 src_mip_level = 0,
                                  u32 dst_offset = 0);

  inline void clearBuffer(Buffer buffer, u32 offset, u32 num_bytes);

  inline MappedTmpBuffer tmpBuffer(u32 num_bytes);

private:
  inline CopyPassEncoder(GPURuntime *gpu, CommandWriter writer,
                         GPUQueue queue, GPUTmpInputBlock gpu_input);

  GPURuntime *gpu_;
  CommandWriter writer_;
  GPUQueue queue_;
  GPUTmpInputBlock gpu_input_;
  CommandCtrl ctrl_;
  CopyCommand state_;

friend class CommandEncoder;
};

class CommandEncoder {
public:
  inline void beginEncoding();
  inline void endEncoding();

  inline RasterPassEncoder beginRasterPass(RasterPass render_pass);
  inline void endRasterPass(RasterPassEncoder &render_enc);

  inline ComputePassEncoder beginComputePass();
  inline void endComputePass(ComputePassEncoder &compute_enc);

  inline CopyPassEncoder beginCopyPass();
  inline void endCopyPass(CopyPassEncoder &copy_enc);

private:
  inline CommandEncoder(GPURuntime *gpu, GPUQueue queue);

  GPURuntime *gpu_;
  FrontendCommands *cmds_head_;
  CommandWriter cmd_writer_;
  GPUQueue queue_;
  GPUTmpInputBlock gpu_input_;

friend class GPURuntime;
};

class GPURuntime {
public:
  // ==== Create & destroy buffers and textures  ==============================
  inline Buffer createBuffer(BufferInit init,
                             GPUQueue tx_queue = {});

  inline void destroyBuffer(Buffer buffer);

  inline Texture createTexture(TextureInit init,
                               GPUQueue tx_queue = {});

  inline void destroyTexture(Texture texture);

  inline void createBuffers(i32 num_buffers,
                            const BufferInit *buffer_inits,
                            Buffer *handles_out,
                            GPUQueue tx_queue = {});

  inline void destroyBuffers(i32 num_buffers, Buffer *buffers);

  inline void createTextures(i32 num_textures,
                             const TextureInit *texture_inits,
                             Texture *handles_out,
                             GPUQueue tx_queue = {});

  inline void destroyTextures(i32 num_textures, Texture *textures);

  // Bulk create / destroy, prefer these for big groups of assets!
  inline void createGPUResources(GPUResourcesCreate create,
                                 GPUQueue tx_queue = {});

  inline void destroyGPUResources(GPUResourcesDestroy destroy);

  virtual void createGPUResources(i32 num_buffers,
                                  const BufferInit *buffer_inits,
                                  Buffer *buffer_handles_out,
                                  i32 num_textures,
                                  const TextureInit *texture_inits,
                                  Texture *texture_handles_out,
                                  GPUQueue tx_queue = {}) = 0;

  virtual void destroyGPUResources(i32 num_buffers,
                                   const Buffer *buffers,
                                   i32 num_textures,
                                   const Texture *textures) = 0;

  // Special-purpose (non-grouped) buffer and texture creation
  virtual Buffer createStagingBuffer(u32 num_bytes) = 0;
  virtual void destroyStagingBuffer(Buffer staging) = 0;

  virtual void prepareStagingBuffers(
      i32 num_buffers, Buffer *buffers, void **mapped_out) = 0;
  virtual void flushStagingBuffers(i32 num_buffers, Buffer *buffers) = 0;

  virtual Buffer createReadbackBuffer(u32 num_bytes) = 0;
  virtual void destroyReadbackBuffer(Buffer buffer) = 0;

  virtual void * beginReadback(Buffer buffer) = 0;
  virtual void endReadback(Buffer buffer) = 0;
   
  virtual Buffer createStandaloneBuffer(
      BufferInit init, bool external_export = false) = 0;
  virtual void destroyStandaloneBuffer(Buffer buffer) = 0;

  virtual Texture createStandaloneTexture(TextureInit init) = 0;
  virtual void destroyStandaloneTexture(Texture texture) = 0;

  // ==== Create & destroy samplers ===========================================
  inline Sampler createSampler(SamplerInit init);
  inline void destroySampler(Sampler sampler);

  virtual void createSamplers(i32 num_samplers,
                              const SamplerInit *sampler_inits,
                              Sampler *handles_out) = 0;
  virtual void destroySamplers(i32 num_samplers, Sampler *samplers) = 0;

  // ==== Create & destroy parameter blocks ===================================
  inline ParamBlockType createParamBlockType(
      ParamBlockTypeInit init);
  inline void destroyParamBlockType(ParamBlockType blk_type);

  virtual void createParamBlockTypes(
      i32 num_types,
      const ParamBlockTypeInit *blk_types,
      ParamBlockType *handles_out) = 0;

  virtual void destroyParamBlockTypes(
      i32 num_types, ParamBlockType *blk_types) = 0;

  inline ParamBlock createParamBlock(ParamBlockInit init);
  inline void destroyParamBlock(ParamBlock group);

  virtual void createParamBlocks(
      i32 num_blks,
      const ParamBlockInit *blk_inits,
      ParamBlock *handles_out) = 0;

  virtual void destroyParamBlocks(
      i32 num_blks, ParamBlock *blks) = 0;

  // ==== Create & destroy render passes ======================================
  inline RasterPassInterface createRasterPassInterface(
      RasterPassInterfaceInit init);
  inline void destroyRasterPassInterface(RasterPassInterface interface);

  virtual void createRasterPassInterfaces(
      i32 num_interfaces,
      const RasterPassInterfaceInit *interface_inits,
      RasterPassInterface *handles_out) = 0;
  virtual void destroyRasterPassInterfaces(
      i32 num_interfaces, RasterPassInterface *interfaces) = 0;

  inline RasterPass createRasterPass(RasterPassInit init);
  inline void destroyRasterPass(RasterPass interface);

  virtual void createRasterPasses(
      i32 num_interfaces,
      const RasterPassInit *pass_inits,
      RasterPass *handles_out) = 0;
  virtual void destroyRasterPasses(
      i32 num_passes, RasterPass *passes) = 0;

  // ==== Create & destroy shaders ============================================
  inline RasterShader createRasterShader(RasterShaderInit init);
  inline void destroyRasterShader(RasterShader shader);

  virtual void createRasterShaders(i32 num_shaders,
                                   const RasterShaderInit *shader_inits,
                                   RasterShader *handles_out) = 0;
  virtual void destroyRasterShaders(i32 num_shaders, RasterShader *shaders)
      = 0;

  // ==== Command recording & submission ======================================
  inline GPUQueue getMainQueue();
  
  inline CommandEncoder createCommandEncoder(GPUQueue queue);
  inline void destroyCommandEncoder(CommandEncoder &encoder);

  inline void submit(GPUQueue queue, CommandEncoder &enc);

  virtual void waitUntilReady(GPUQueue queue) = 0;
  virtual void waitUntilIdle() = 0;

  // ==== Swapchain & presentation ============================================
  virtual Swapchain createSwapchain(Surface surface,
                                    SwapchainProperties *properties) = 0;
  virtual void destroySwapchain(Swapchain swapchain) = 0;
  virtual AcquireSwapchainResult acquireSwapchainImage(Swapchain swapchain)
      = 0;
  virtual void presentSwapchainImage(Swapchain swapchain) = 0;

  ErrorStatus currentErrorStatus();

protected:
  virtual void submit(GPUQueue queue, FrontendCommands *cmds) = 0;

  FrontendCommands * allocCommandBlock();
  void deallocCommandBlocks(FrontendCommands *cmds);

  virtual GPUTmpInputBlock allocGPUTmpInputBlock(GPUQueue queue) = 0;

friend class CommandEncoder;
friend class RasterPassEncoder;
friend class ComputePassEncoder;
friend class CopyPassEncoder;
friend class CommandWriter;
};

class GPULib {
public:
  virtual inline ~GPULib() {};
};

struct ShaderCompilerLib {
  void *hdl;
  ShaderCompiler * (*createCompiler)();
  void (*destroyCompiler)(ShaderCompiler *shaderc);
};

class GPUAPI {
public:
  virtual void shutdown() = 0;

  virtual Surface createSurface(void *os_data, i32 width, i32 height) = 0;
  virtual void destroySurface(Surface surface) = 0;

  virtual GPURuntime * createRuntime(
    i32 gpu_idx, Span<const Surface> surfaces = {}) = 0;
  virtual void destroyRuntime(GPURuntime *runtime) = 0;

  virtual void processGraphicsEvents() = 0;

  virtual ShaderByteCodeType backendShaderByteCodeType() = 0;
};


inline constexpr bool operator==(Swapchain a, Swapchain b);

}

#include "gas.inl"
