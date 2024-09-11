#include "wgpu.hpp"
#include "wgpu_init.hpp"

#include <dawn/native/DawnNative.h>

#include <madrona/macros.hpp>

#ifdef MADRONA_LINUX
#include "linux.hpp"
#endif

//#define GAS_WGPU_DEBUG_PRINT (1)

namespace gas::webgpu {

namespace {

inline wgpu::TextureFormat convertTextureFormat(TextureFormat in)
{
  using Out = wgpu::TextureFormat;
  using enum TextureFormat;

  switch (in) {
    case None: return Out::Undefined;
    case RGBA8_UNorm: return Out::RGBA8Unorm;
    case RGBA8_SRGB: return Out::RGBA8UnormSrgb;
    case BGRA8_UNorm: return Out::BGRA8Unorm;
    case BGRA8_SRGB: return Out::BGRA8UnormSrgb;
    case Depth32_Float: return Out::Depth32Float;
    default: MADRONA_UNREACHABLE();
  }
}

inline TextureFormat convertWebGPUTextureFormat(wgpu::TextureFormat in)
{
  using In = wgpu::TextureFormat;
  using enum TextureFormat;

  switch (in) {
    case In::RGBA8Unorm: return RGBA8_UNorm;
    case In::RGBA8UnormSrgb: return RGBA8_SRGB;
    case In::BGRA8Unorm: return BGRA8_UNorm;
    case In::BGRA8UnormSrgb: return BGRA8_SRGB;
    default: return None;
  }
};

inline wgpu::TextureUsage convertTextureUsage(TextureUsage in)
{
  using Out = wgpu::TextureUsage;
  using enum TextureUsage;

  u64 out = (u64)Out::None;
  static_assert(sizeof(u64) == sizeof(Out));

  if ((in & CopySrc) == CopySrc) {
    out |= (u64)Out::CopySrc;
  }

  if ((in & CopyDst) == CopyDst) {
    out |= (u64)Out::CopyDst;
  }

  if ((in & ShaderSampled) == ShaderSampled) {
    out |= (u64)Out::TextureBinding;
  }

  if ((in & ShaderStorage) == ShaderSampled) {
    out |= (u64)Out::StorageBinding;
  }

  if ((in & ColorAttachment) == ColorAttachment) {
    out |= (u64)Out::RenderAttachment;
  }

  if ((in & DepthAttachment) == DepthAttachment) {
    out |= (u64)Out::RenderAttachment;
  }

  return (Out)out;
}

inline wgpu::AddressMode convertSamplerAddressMode(SamplerAddressMode in)
{
  using Out = wgpu::AddressMode;
  using enum SamplerAddressMode;

  switch (in) {
    case Clamp: return Out::ClampToEdge;
    case Repeat: return Out::Repeat;
    case MirrorRepeat: return Out::MirrorRepeat;
    case InheritUMode: return Out::Undefined;
    default: MADRONA_UNREACHABLE();
  }
}

inline wgpu::FilterMode convertSamplerFilterMode(SamplerFilterMode in)
{
  using Out = wgpu::FilterMode;
  using enum SamplerFilterMode;

  switch (in) {
    case Nearest: return Out::Nearest;
    case Linear: return Out::Linear;
    default: MADRONA_UNREACHABLE();
  }
}

inline wgpu::MipmapFilterMode
    convertSamplerFilterModeToMipFilterMode(SamplerFilterMode in)
{
  using Out = wgpu::MipmapFilterMode;
  using enum SamplerFilterMode;

  switch (in) {
    case Nearest: return Out::Nearest;
    case Linear: return Out::Linear;
    default: MADRONA_UNREACHABLE();
  }
}

inline wgpu::BufferUsage convertBufferUsage(BufferUsage in)
{
  using Out = wgpu::BufferUsage;
  using enum BufferUsage;

  u64 out = (u64)Out::None;
  static_assert(sizeof(u64) == sizeof(Out));

  if ((in & CopySrc) == CopySrc) {
    out |= (u64)Out::CopySrc;
  }

  if ((in & CopyDst) == CopyDst) {
    out |= (u64)Out::CopyDst;
  }

  if ((in & DrawIndex) == DrawIndex) {
    out |= (u64)Out::Index;
  }

  if ((in & DrawVertex) == DrawVertex) {
    out |= (u64)Out::Vertex;
  }

  if ((in & ShaderUniform) == ShaderUniform) {
    out |= (u64)Out::Uniform;
  }

  if ((in & ShaderStorage) == ShaderStorage) {
    out |= (u64)Out::Storage;
  }

  return (Out)out;
}

inline wgpu::CompareFunction convertDepthCompare(DepthCompare in)
{
  using Out = wgpu::CompareFunction;
  using enum DepthCompare;

  switch (in) {
    case GreaterOrEqual: return Out::GreaterEqual;
    case Disabled: return Out::Never;
    default: MADRONA_UNREACHABLE();
  }
};

inline wgpu::CullMode convertCullMode(CullMode in)
{
  using Out = wgpu::CullMode;
  using enum CullMode;

  switch (in) {
    case None: return Out::None;
    case FrontFace: return Out::Front;
    case BackFace: return Out::Back;
    default: MADRONA_UNREACHABLE();
  }
}

inline wgpu::LoadOp convertAttachmentLoadMode(AttachmentLoadMode in)
{
  using Out = wgpu::LoadOp;
  using enum AttachmentLoadMode;

  switch (in) {
    case Load: return Out::Load;
    case Clear: return Out::Clear;
    case Undefined: return Out::Undefined;
    default: MADRONA_UNREACHABLE();
  }
}

inline wgpu::StoreOp convertAttachmentStoreMode(
    AttachmentStoreMode in)
{
  using Out = wgpu::StoreOp;
  using enum AttachmentStoreMode;

  switch (in) {
    case Store: return Out::Store;
    case Undefined: return Out::Undefined;
    default: MADRONA_UNREACHABLE();
  }
}

inline wgpu::ShaderStage convertShaderStage(ShaderStage in)
{
  using Out = wgpu::ShaderStage;
  using enum ShaderStage;

  u64 out = (u64)Out::None;
  static_assert(sizeof(u64) == sizeof(Out));

  if ((in & Vertex) == Vertex) {
    out |= (u64)Out::Vertex;
  }

  if ((in & Fragment) == Fragment) {
    out |= (u64)Out::Fragment;
  }

  if ((in & Compute) == Compute) {
    out |= (u64)Out::Compute;
  }

  return (Out)out;
}

inline wgpu::BufferBindingType convertBufferBindingType(
  BufferBindingType in)
{
  using Out = wgpu::BufferBindingType;
  using enum BufferBindingType;

  switch (in) {
    case Uniform: case DynamicUniform: return Out::Uniform;
    case Storage: return Out::ReadOnlyStorage;
    case StorageRW: return Out::Storage;
    default: MADRONA_UNREACHABLE();
  }
}

void instanceLoggingCB(WGPULoggingType type,
                       const char *message,
                       void *user_data)
{
  (void)type;
  (void)user_data;
  fprintf(stderr, " Instance Logging: %s\n", message);
}

void deviceLoggingCB(WGPULoggingType type,
                     const char *message,
                     void *user_data)
{
  (void)type;
  (void)user_data;
  fprintf(stderr, " Device Logging: %s\n", message);
}

void deviceLostCB(const wgpu::Device &wgpu_dev,
                  wgpu::DeviceLostReason lost_reason,
                  const char *message,
                  WGPUDevice *destroying_device)
{
  (void)lost_reason;

  if (wgpu_dev.Get() == *destroying_device) {
    return;
  }

  FATAL(" device lost: %s", message);
}

void uncapturedErrorCB(const wgpu::Device &wgpu_dev,
                       wgpu::ErrorType error_type,
                       const char *message,
                       void *user_data)
{
  (void)wgpu_dev;
  (void)error_type;
  (void)user_data;

  fprintf(stderr, " uncaptured error: %s\n", message);
  debuggerBreakPoint();
}

}

GPUAPI * WebGPUAPI::init(const APIConfig &cfg)
{
  wgpu::InstanceDescriptor inst_desc {
    .features = {
      .timedWaitAnyEnable = false,
    },
  };

  dawn::native::DawnInstanceDescriptor dawn_inst_desc;
  if (cfg.enableValidation) {
    dawn_inst_desc.nextInChain = inst_desc.nextInChain;
    inst_desc.nextInChain = &dawn_inst_desc;

    dawn_inst_desc.backendValidationLevel =
      dawn::native::BackendValidationLevel::Full;
    dawn_inst_desc.loggingCallback = instanceLoggingCB;
  }

  wgpu::Instance instance = wgpu::CreateInstance(&inst_desc);
  auto *api = new WebGPUAPI();
  api->inst = std::move(instance);
  api->destroyingDevice = nullptr;
  api->errorsAreFatal = cfg.runtimeErrorsAreFatal;
  return api;
}

void WebGPUAPI::shutdown()
{
  delete this;
}

Surface WebGPUAPI::createSurface(void *os_data, i32 width, i32 height)
{
#if defined(MADRONA_LINUX)

  LinuxWindowHandle &linux_hdl = *(LinuxWindowHandle *)os_data;

  wgpu::SurfaceDescriptorFromXlibWindow from_xlib;
  wgpu::SurfaceDescriptorFromWaylandSurface from_wayland;
  wgpu::SurfaceDescriptor surface_desc;

  switch (linux_hdl.backend) {
    case LinuxWindowHandle::Backend::X11: {
      from_xlib.display = linux_hdl.x11.display;
      from_xlib.window = linux_hdl.x11.window;

      surface_desc = wgpu::SurfaceDescriptor {
        .nextInChain = &from_xlib,
        .label = nullptr,
      };
    } break;
    case LinuxWindowHandle::Backend::Wayland: {
      from_wayland.display = linux_hdl.wayland.display;
      from_wayland.surface = linux_hdl.wayland.surface;

      surface_desc = wgpu::SurfaceDescriptor {
        .nextInChain = &from_wayland,
        .label = nullptr,
      };
    } break;
    default: MADRONA_UNREACHABLE();
  }
#elif defined(MADRONA_MACOS)
  wgpu::SurfaceDescriptorFromMetalLayer from_metal({
    .nextInChain = nullptr,
    .layer = os_data,
  });
  
  wgpu::SurfaceDescriptor surface_desc {
    .nextInChain = &from_metal,
    .label = nullptr,
  };
#endif

  wgpu::Surface surface = inst.CreateSurface(&surface_desc);
  return Surface {
    .hdl = {
      .ptr = surface.MoveToCHandle(),
    },
    .width = width,
    .height = height,
  };
}

void WebGPUAPI::destroySurface(Surface surface)
{
  wgpuSurfaceRelease((WGPUSurface)surface.hdl.ptr);
} 

namespace {
struct InitDeviceResult {
  wgpu::Adapter adapter;
  wgpu::Device device;
};
}

static InitDeviceResult initDevice(
  WebGPUAPI *api, i32 idx, Span<const Surface> surfaces)
{
  // Cannot select specific GPU in webgpu
  assert(idx == 0);
  assert(surfaces.size() <= 1); // Can only have one compatible surface

  wgpu::Adapter adapter;
  {
    wgpu::RequestAdapterOptions request_options {
      .powerPreference = wgpu::PowerPreference::HighPerformance,
    };

    if (surfaces.size() == 1) {
      request_options.compatibleSurface =
        wgpu::Surface((WGPUSurface)surfaces[0].hdl.ptr);
    }

    wgpu::Future future = api->inst.RequestAdapter(
      &request_options, wgpu::CallbackMode::WaitAnyOnly,
      [](wgpu::RequestAdapterStatus status, wgpu::Adapter returned_adapter,
         const char *message, wgpu::Adapter *out_adapter)
      {
        if (status != wgpu::RequestAdapterStatus::Success) {
          FATAL("Requesting adapter failed: %s", message);
        }

       *out_adapter = returned_adapter;
      }, &adapter);

    wgpu::WaitStatus wait_status = api->inst.WaitAny(future, 0);

    if (wait_status != wgpu::WaitStatus::Success) {
      FATAL("Requesting adapter failed during wait: %d", (int)wait_status);
    }
  }

  wgpu::Device device;
  {
    wgpu::DeviceDescriptor dev_desc;
    dev_desc.SetDeviceLostCallback(wgpu::CallbackMode::AllowSpontaneous,
                                   deviceLostCB, &api->destroyingDevice);
    dev_desc.SetUncapturedErrorCallback(uncapturedErrorCB, (void *)nullptr);

    wgpu::Future future = adapter.RequestDevice(
      &dev_desc, wgpu::CallbackMode::WaitAnyOnly,
      [](wgpu::RequestDeviceStatus status, wgpu::Device returned_device,
         const char *message, wgpu::Device *out_device)
      {
        if (status != wgpu::RequestDeviceStatus::Success) {
          FATAL("Requesting device failed: %s", message);
        }

        *out_device = returned_device;
      }, &device);

    wgpu::WaitStatus wait_status = api->inst.WaitAny(future, 0);

    if (wait_status != wgpu::WaitStatus::Success) {
      FATAL("Requesting device failed during wait");
    }
  }

  device.SetLoggingCallback(deviceLoggingCB, nullptr);

  return { std::move(adapter), std::move(device) };
}

GPURuntime * WebGPUAPI::createRuntime(
    i32 gpu_idx, Span<const Surface> surfaces)
{
  auto [adapter, device] = initDevice(this, gpu_idx, surfaces);

  wgpu::Queue queue = device.GetQueue();

  return new Backend(std::move(adapter), std::move(device), std::move(queue),
                     inst, errorsAreFatal);
}

void WebGPUAPI::destroyRuntime(GPURuntime *runtime)
{
  auto wgpu_backend = static_cast<Backend *>(runtime);

  // Hacky, the device lost callback has a pointer
  // to the destroyingDevice member in the WebGPUAPI class,
  // which it checks to see if we're manually destroying this device.
  destroyingDevice = wgpu_backend->dev.Get();

  delete wgpu_backend;

  destroyingDevice = nullptr;
}

void WebGPUAPI::processGraphicsEvents()
{
  inst.ProcessEvents();
}

ShaderByteCodeType WebGPUAPI::backendShaderByteCodeType()
{
  return ShaderByteCodeType::WGSL;
}

Backend::Backend(wgpu::Adapter &&adapter_in,
                 wgpu::Device &&dev_in,
                 wgpu::Queue &&queue_in,
                 wgpu::Instance &inst_in,
                 bool errors_are_fatal)
  : BackendCommon(errors_are_fatal),
    adapter(std::move(adapter_in)),
    dev(std::move(dev_in)),
    queue(std::move(queue_in)),
    inst(inst_in)
{}

void Backend::createGPUResources(i32 num_buffers,
                                 const BufferInit *buffer_inits,
                                 Buffer *buffer_handles_out,
                                 i32 num_textures,
                                 const TextureInit *texture_inits,
                                 Texture *texture_handles_out,
                                 TransferQueue tx_queue)
{
  (void)tx_queue;

  u32 buffer_tbl_offset;
  if (num_buffers > 0) {
    buffer_tbl_offset = buffers.reserveRows(num_buffers);
    if (buffer_tbl_offset == AllocOOM) [[unlikely]] {
      reportError(ErrorStatus::TableFull);
      return;
    }
  }

  u32 texture_tbl_offset;
  if (num_textures > 0) {
    texture_tbl_offset = textures.reserveRows(num_textures);
    if (texture_tbl_offset == AllocOOM) [[unlikely]] {
      if (num_buffers > 0) {
        buffers.releaseRows(buffer_tbl_offset, num_buffers);
      }
      reportError(ErrorStatus::TableFull);
      return;
    }
  }

  for (i32 buf_idx = 0; buf_idx < num_buffers; buf_idx++) {
    const BufferInit &buf_init = buffer_inits[buf_idx];

    auto [to_hot, to_cold, id] = buffers.get(buffer_tbl_offset, buf_idx);

    wgpu::BufferUsage wgpu_usage = convertBufferUsage(buf_init.usage);

    void *init_ptr = buf_init.initData.ptr;

    wgpu::BufferDescriptor buf_desc {
      .usage = wgpu_usage,
      .size = buf_init.numBytes,
      .mappedAtCreation = init_ptr != nullptr,
    };

    new (to_hot) wgpu::Buffer(dev.CreateBuffer(&buf_desc));
    *to_cold = buf_init;
    buffer_handles_out[buf_idx] = id;

    if (init_ptr) {
      void *dst = to_hot->GetMappedRange();
      memcpy(dst, init_ptr, buf_init.numBytes);
      to_hot->Unmap();
    }
  }

  for (i32 tex_idx = 0; tex_idx < num_textures; tex_idx++) {
    const TextureInit &tex_init = texture_inits[tex_idx];

    wgpu::TextureUsage wgpu_usage = convertTextureUsage(tex_init.usage);

    wgpu::TextureDimension dim;
    if (tex_init.depth == 0) {
      if (tex_init.height == 0) {
        dim = wgpu::TextureDimension::e1D;
      } else {
        dim = wgpu::TextureDimension::e2D;
      }
    } else {
      dim = wgpu::TextureDimension::e3D;
    }

    wgpu::TextureDescriptor tex_desc {
      .usage = wgpu_usage,
      .dimension = dim,
      .size = {
        .width = (u32)tex_init.width,
        .height = tex_init.height != 0 ? (u32)tex_init.height : 1,
        .depthOrArrayLayers = tex_init.depth != 0 ? (u32)tex_init.depth : 1,
      },
      .format = convertTextureFormat(tex_init.format),
      .mipLevelCount = (u32)tex_init.numMipLevels,
      .sampleCount = 1,
      .viewFormatCount = 0,
      .viewFormats = nullptr,
    };

    auto [to_hot, to_cold, id] = textures.get(texture_tbl_offset, tex_idx);

    wgpu::Texture wgpu_tex = dev.CreateTexture(&tex_desc);

    wgpu::TextureViewDescriptor view_desc {
      .format = tex_desc.format,
    };

    wgpu::TextureView wgpu_tex_view = wgpu_tex.CreateView(&view_desc);

    new (to_cold) BackendTextureCold {
      .texture = std::move(wgpu_tex),
    };

    new (to_hot) BackendTexture {
      .view = std::move(wgpu_tex_view),
    };

    texture_handles_out[tex_idx] = id;
  }
}

void Backend::destroyGPUResources(i32 num_buffers,
                                  const Buffer *buffer_hdls,
                                  i32 num_textures,
                                  const Texture *texture_hdls)
{
  buffers.releaseResources(num_buffers, buffer_hdls,
    [](wgpu::Buffer *to_buf, BufferInit *)
  {
    to_buf->Destroy();
    to_buf->~Buffer();
  });

  textures.releaseResources(num_textures, texture_hdls,
    [](BackendTexture *to_hot, BackendTextureCold *to_cold)
  {
    to_hot->~BackendTexture();

    to_cold->texture.Destroy();
    to_cold->~BackendTextureCold();
  });
}

Buffer Backend::createStagingBuffer(u32 num_bytes)
{
  u32 tbl_offset = buffers.reserveRows(1);
  if (tbl_offset == AllocOOM) [[unlikely]] {
    reportError(ErrorStatus::TableFull);
    return {};
  }

  wgpu::BufferDescriptor buf_desc {
    .usage = (wgpu::BufferUsage)(
        (u64)wgpu::BufferUsage::MapWrite |
        (u64)wgpu::BufferUsage::CopySrc),
    .size = num_bytes,
  };

  auto [to_hot, to_cold, id] = buffers.get(tbl_offset, 0);

  new (to_hot) wgpu::Buffer(dev.CreateBuffer(&buf_desc));
  *to_cold = {};

  return id;
}

void Backend::destroyStagingBuffer(Buffer staging)
{
  buffers.releaseResources(1, &staging,
    [](wgpu::Buffer *to_buf, auto)
  {
    to_buf->Destroy();
    to_buf->~Buffer();
  });
}

void Backend::prepareStagingBuffers(i32 num_buffers,
                                    Buffer *buffer_hdls,
                                    void **mapped_out)
{
  for (i32 buf_idx = 0; buf_idx < num_buffers; buf_idx++) {
    Buffer hdl = buffer_hdls[buf_idx];
    wgpu::Buffer *to_buffer = buffers.hot(hdl);
    if (!to_buffer) [[unlikely]] {
      reportError(ErrorStatus::NullBuffer);
      continue;
    }

    // FIXME
    auto map_cb = [](wgpu::MapAsyncStatus, char const *, void *) {};

    wgpu::Future map_future = to_buffer->MapAsync(
        wgpu::MapMode::Write, 0, WGPU_WHOLE_SIZE,
        wgpu::CallbackMode::WaitAnyOnly, map_cb, (void *)nullptr);

    wgpu::WaitStatus map_wait_status = inst.WaitAny(map_future, 0);
    assert(map_wait_status == wgpu::WaitStatus::Success);

    mapped_out[buf_idx] = to_buffer->GetMappedRange();
  }
}

void Backend::flushStagingBuffers(i32 num_buffers, Buffer *buffer_hdls)
{
  for (i32 buf_idx = 0; buf_idx < num_buffers; buf_idx++) {
    Buffer hdl = buffer_hdls[buf_idx];
    wgpu::Buffer *to_buffer = buffers.hot(hdl);
    if (!to_buffer) [[unlikely]] {
      reportError(ErrorStatus::NullBuffer);
      continue;
    }

    to_buffer->Unmap();
  }
}

Buffer Backend::createReadbackBuffer(u32 num_bytes, void **mapped_out)
{
  (void)num_bytes;
  (void)mapped_out;
  FATAL("Unimplemented");
}

void Backend::destroyReadbackBuffer(Buffer readback, void *mapped)
{
  (void)readback;
  (void)mapped;
  FATAL("Unimplemented");
}
 
Buffer Backend::createStandaloneBuffer(BufferInit init, bool external_export)
{
  (void)init;
  (void)external_export;
  FATAL("Unimplemented");
}

void Backend::destroyStandaloneBuffer(Buffer buffer)
{
  (void)buffer;
  FATAL("Unimplemented");
}

Texture Backend::createStandaloneTexture(TextureInit init)
{
  (void)init;
  FATAL("Unimplemented");
}

void Backend::destroyStandaloneTexture(Texture texture)
{
  (void)texture;
  FATAL("Unimplemented");
}


void Backend::createSamplers(i32 num_samplers,
                             const SamplerInit *sampler_inits,
                             Sampler *handles_out)
{
  u32 tbl_offset = samplers.reserveRows(num_samplers);
  if (tbl_offset == AllocOOM) [[unlikely]] {
    reportError(ErrorStatus::TableFull);
    return;
  }

  for (i32 sampler_idx = 0; sampler_idx < num_samplers; sampler_idx++) {
    const SamplerInit &sampler_init = sampler_inits[sampler_idx];

    auto [to_out, _, id] = samplers.get(tbl_offset, sampler_idx);

    wgpu::SamplerDescriptor sampler_descriptor;

    sampler_descriptor.addressModeU =
      convertSamplerAddressMode(sampler_init.addressModeU);

    if (sampler_init.addressModeV == SamplerAddressMode::InheritUMode) {
      sampler_descriptor.addressModeV = sampler_descriptor.addressModeU;
    } else {
      sampler_descriptor.addressModeV =
          convertSamplerAddressMode(sampler_init.addressModeV);
    }

    if (sampler_init.addressModeW == SamplerAddressMode::InheritUMode) {
      sampler_descriptor.addressModeW = sampler_descriptor.addressModeU;
    } else {
      sampler_descriptor.addressModeW =
          convertSamplerAddressMode(sampler_init.addressModeW);
    }

    sampler_descriptor.magFilter =
        convertSamplerFilterMode(sampler_init.magnificationFilterMode);
    sampler_descriptor.minFilter =
        convertSamplerFilterMode(sampler_init.minificationFilterMode);
    sampler_descriptor.mipmapFilter =
        convertSamplerFilterModeToMipFilterMode(sampler_init.mipmapFilterMode);
    sampler_descriptor.lodMinClamp = sampler_init.mipClamp[0];
    sampler_descriptor.lodMaxClamp = sampler_init.mipClamp[1];
    sampler_descriptor.maxAnisotropy = sampler_init.anisotropy;

    new (to_out) wgpu::Sampler(dev.CreateSampler(&sampler_descriptor));

    handles_out[sampler_idx] = id;
  }
}

void Backend::destroySamplers(i32 num_samplers, Sampler *handles)
{
  samplers.releaseResources(num_samplers, handles,
    [](wgpu::Sampler *to_sampler, auto)
  {
    to_sampler->~Sampler();
  });
}

void Backend::createParamBlockTypes(
    i32 num_types,
    const ParamBlockTypeInit *blk_types,
    ParamBlockType *handles_out)
{
  u32 tbl_offset = paramBlockTypes.reserveRows(num_types);
  if (tbl_offset == AllocOOM) [[unlikely]] {
    reportError(ErrorStatus::TableFull);
    return;
  }

  for (i32 type_idx = 0; type_idx < num_types; type_idx++) {
    const ParamBlockTypeInit &type_init = blk_types[type_idx];

    auto [to_block_type, to_uuid, id] =
      paramBlockTypes.get(tbl_offset, type_idx);

    i32 auto_binding_idx = 0;
    wgpu::BindGroupLayoutEntry layout_entries[MAX_BINDINGS_PER_GROUP];

    const i32 num_buffer_bindings = (i32)type_init.buffers.size();
    const i32 num_texture_bindings = (i32)type_init.textures.size();
    const i32 num_sampler_bindings = (i32)type_init.textures.size();

    i32 out_binding_idx = 0;
    for (i32 buffer_binding_idx = 0;
         buffer_binding_idx < num_buffer_bindings;
         buffer_binding_idx++) {
      const BufferBindingConfig buffer_cfg =
        type_init.buffers[buffer_binding_idx];
      assert(buffer_cfg.numBuffers == 1);

      i32 binding = buffer_cfg.bindLocation;
      if (binding == -1) {
        binding = auto_binding_idx;
      }
      auto_binding_idx = binding + 1;

      layout_entries[out_binding_idx++] = wgpu::BindGroupLayoutEntry {
        .binding = (u32)binding,
        .visibility = convertShaderStage(buffer_cfg.shaderUsage),
        .buffer = wgpu::BufferBindingLayout {
          .type = convertBufferBindingType(buffer_cfg.type),
          .hasDynamicOffset =
              buffer_cfg.type == BufferBindingType::DynamicUniform,
          .minBindingSize = 0,
        },
      };
    }

    for (i32 texture_binding_idx = 0;
         texture_binding_idx < num_texture_bindings;
         texture_binding_idx++) {
      const TextureBindingConfig texture_cfg =
        type_init.textures[texture_binding_idx];
      assert(texture_cfg.numTextures == 1);

      i32 binding = texture_cfg.bindLocation;
      if (binding == -1) {
        binding = auto_binding_idx;
      }
      auto_binding_idx = binding + 1;

      using enum TextureBindingType;

      wgpu::TextureSampleType sample_type;
      wgpu::TextureViewDimension tex_dim;
      switch (texture_cfg.type) {
        case Texture1D: {
          sample_type = wgpu::TextureSampleType::Float;
          tex_dim = wgpu::TextureViewDimension::e1D;
        } break;
        case Texture2D: {
          sample_type = wgpu::TextureSampleType::Float;
          tex_dim = wgpu::TextureViewDimension::e2D;
        } break;
        case Texture3D: {
          sample_type = wgpu::TextureSampleType::Float;
          tex_dim = wgpu::TextureViewDimension::e3D;
        } break;
        case DepthTexture2D: {
          sample_type = wgpu::TextureSampleType::Depth;
          tex_dim = wgpu::TextureViewDimension::e2D;
        } break;
      }

      layout_entries[out_binding_idx++] = wgpu::BindGroupLayoutEntry {
        .binding = (u32)binding,
        .visibility = convertShaderStage(texture_cfg.shaderUsage),
        .texture = wgpu::TextureBindingLayout {
          .sampleType = sample_type,
          .viewDimension = tex_dim,
        },
      };
    }

    for (i32 sampler_binding_idx = 0;
         sampler_binding_idx < num_sampler_bindings;
         sampler_binding_idx++) {
      const SamplerBindingConfig sampler_cfg =
        type_init.samplers[sampler_binding_idx];
      assert(sampler_cfg.numSamplers == 1);

      i32 binding = sampler_cfg.bindLocation;
      if (binding == -1) {
        binding = auto_binding_idx;
      }
      auto_binding_idx = binding + 1;

      layout_entries[out_binding_idx++] = wgpu::BindGroupLayoutEntry {
        .binding = (u32)binding,
        .visibility = convertShaderStage(sampler_cfg.shaderUsage),
        .sampler = wgpu::SamplerBindingLayout {
          .type = wgpu::SamplerBindingType::Filtering,
        },
      };
    }

    wgpu::BindGroupLayoutDescriptor layout_descriptor;
    layout_descriptor.entryCount = size_t(out_binding_idx);
    layout_descriptor.entries = layout_entries;

    new (to_block_type) BackendParamBlockType {
      .layout = dev.CreateBindGroupLayout(&layout_descriptor),
    };

    *to_uuid = ParamBlockTypeID(type_init.uuid);

    paramBlockTypeIDs.insert(type_init.uuid, id.id);

    handles_out[type_idx] = id;
  }
}

void Backend::destroyParamBlockTypes(i32 num_types,
                                     ParamBlockType *handles)
{
  paramBlockTypes.releaseResources(num_types, handles,
    [this](BackendParamBlockType *to_param_block_type,
           ParamBlockTypeID *to_uuid)
  {
    to_param_block_type->~BackendParamBlockType();
    paramBlockTypeIDs.remove(to_uuid->asUUID());
  });
}

void Backend::createParamBlocks(i32 num_blks,
                               const ParamBlockInit *blk_inits,
                               ParamBlock *handles_out)
{
  u32 tbl_offset = paramBlocks.reserveRows(num_blks);
  if (tbl_offset == AllocOOM) [[unlikely]] {
    reportError(ErrorStatus::TableFull);
    return;
  }

  for (i32 blk_idx = 0; blk_idx < num_blks; blk_idx++) {
    const ParamBlockInit &blk_init = blk_inits[blk_idx];

    auto [to_group, _, id] = paramBlocks.get(tbl_offset, blk_idx);

    wgpu::BindGroupLayout layout =
      getBindGroupLayoutByParamBlockTypeID(blk_init.typeID);

    wgpu::BindGroupEntry entries[MAX_BINDINGS_PER_GROUP];

    i32 entry_idx = 0;
    for (BufferBinding binding : blk_init.buffers) {
      wgpu::Buffer *to_buf = buffers.hot(binding.buffer);
      entries[entry_idx] = wgpu::BindGroupEntry {
        .binding = (u32)entry_idx,
        .buffer = *to_buf,
        .offset = (u64)binding.offset,
        .size = binding.numBytes == 0xFFFF'FFFF ?
            WGPU_WHOLE_SIZE : (u64)binding.numBytes,
      };

      entry_idx += 1;
    }

    for (Texture texture : blk_init.textures) {
      BackendTexture *to_tex = textures.hot(texture);
      entries[entry_idx] = wgpu::BindGroupEntry {
        .binding = (u32)entry_idx,
        .textureView = to_tex->view,
      };
      entry_idx += 1;
    }

    for (Sampler sampler : blk_init.samplers) {
      wgpu::Sampler *to_sampler = samplers.hot(sampler);
      entries[entry_idx] = wgpu::BindGroupEntry {
        .binding = (u32)entry_idx,
        .sampler = *to_sampler,
      };
      entry_idx += 1;
    }
         
    wgpu::BindGroupDescriptor descriptor {
      .layout = layout,
      .entryCount = size_t(
          blk_init.buffers.size() +
          blk_init.textures.size() +
          blk_init.samplers.size()),
      .entries = entries,
    }; 

    new (to_group) wgpu::BindGroup(dev.CreateBindGroup(&descriptor));

    handles_out[blk_idx] = id;
  }
}

void Backend::destroyParamBlocks(i32 num_blks, ParamBlock *blks)
{
  paramBlocks.releaseResources(num_blks, blks,
    [](wgpu::BindGroup *to_group, auto)
  {
    to_group->~BindGroup();
  });
}

void Backend::createRasterPassInterfaces(
    i32 num_interfaces,
    const RasterPassInterfaceInit *interface_inits,
    RasterPassInterface *handles_out)
{
  u32 tbl_offset = rasterPassInterfaces.reserveRows(num_interfaces);
  if (tbl_offset == AllocOOM) [[unlikely]] {
    reportError(ErrorStatus::TableFull);
    return;
  }

  for (i32 interface_idx = 0; interface_idx < num_interfaces;
       interface_idx++) {
    const RasterPassInterfaceInit &interface_init =
      interface_inits[interface_idx];

    auto [to_cfg, to_uuid, id] =
      rasterPassInterfaces.get(tbl_offset, interface_idx);

    const DepthAttachmentConfig &depth_cfg = interface_init.depthAttachment;
    to_cfg->depthAttachment = {
      .format = convertTextureFormat(depth_cfg.format),
      .loadOp = convertAttachmentLoadMode(depth_cfg.loadMode),
      .storeOp = convertAttachmentStoreMode(depth_cfg.storeMode),
      .clearValue = depth_cfg.clearValue,
    };

    i32 num_color_attachments = (i32)interface_init.colorAttachments.size();
    for (i32 i = 0; i < num_color_attachments; i++) {
      const ColorAttachmentConfig &color_cfg =
          interface_init.colorAttachments[i];
      to_cfg->colorAttachments[i] = {
        .format = convertTextureFormat(color_cfg.format),
        .loadOp = convertAttachmentLoadMode(color_cfg.loadMode),
        .storeOp = convertAttachmentStoreMode(color_cfg.storeMode),
        .clearValue = color_cfg.clearValue,
      };
    }

    to_cfg->numColorAttachments = num_color_attachments;

    *to_uuid = RasterPassInterfaceID(interface_init.uuid);

    rasterPassInterfaceIDs.insert(interface_init.uuid, id.id);

    handles_out[interface_idx] = id;
  }
}

void Backend::destroyRasterPassInterfaces(
    i32 num_interfaces, RasterPassInterface *handles)
{
  rasterPassInterfaces.releaseResources(num_interfaces, handles,
    [this](auto, RasterPassInterfaceID *to_uuid)
  {
    rasterPassInterfaceIDs.remove(to_uuid->asUUID());
  });
}

void Backend::createRasterPasses(
    i32 num_passes,
    const RasterPassInit *pass_inits,
    RasterPass *handles_out)
{
  u32 tbl_offset = rasterPasses.reserveRows(num_passes);
  if (tbl_offset == AllocOOM) [[unlikely]] {
    reportError(ErrorStatus::TableFull);
    return;
  }

  for (i32 pass_idx = 0; pass_idx < num_passes; pass_idx++) {
    const RasterPassInit &pass_init = pass_inits[pass_idx];

    auto [out, _, id] = rasterPasses.get(tbl_offset, pass_idx);

    new (out) BackendRasterPass {};

    const BackendRasterPassConfig *cfg =
      rasterPassInterfaces.hot(pass_init.interface);

    if (!pass_init.depthAttachment.null()) {
      out->depthAttachment = {
        .view = textures.hot(pass_init.depthAttachment)->view,
        .loadOp = cfg->depthAttachment.loadOp,
        .storeOp = cfg->depthAttachment.storeOp,
        .clearValue = cfg->depthAttachment.clearValue,
      };
    } else {
      assert(cfg->depthAttachment.format == wgpu::TextureFormat::Undefined);
    }

    out->numColorAttachments = (i32)pass_init.colorAttachments.size();
    out->swapchainAttachmentIndex = -1;
    for (i32 i = 0; i < (i32)pass_init.colorAttachments.size(); i++) {
      const BackendColorAttachmentConfig &attach_cfg =
          cfg->colorAttachments[i];
      BackendColorAttachment &out_attach = out->colorAttachments[i];

      Texture tex_hdl = pass_init.colorAttachments[i];
      if (tex_hdl.gen == 0) {
        assert(out->swapchainAttachmentIndex == -1);
        out->swapchainAttachmentIndex = i;
        out->swapchain = { tex_hdl.id };
      } else {
        out_attach.view = textures.hot(tex_hdl)->view;
      }
      out_attach.loadOp = attach_cfg.loadOp;
      out_attach.storeOp = attach_cfg.storeOp;
      out_attach.clearValue = attach_cfg.clearValue;
    }

    handles_out[pass_idx] = id;
  }
}

void Backend::destroyRasterPasses(
    i32 num_passes, RasterPass *handles)
{
  rasterPasses.releaseResources(num_passes, handles,
    [](BackendRasterPass *to_pass, auto)
  {
    to_pass->~BackendRasterPass();
  });
}

void Backend::createRasterShaders(i32 num_shaders,
                                  const RasterShaderInit *shader_inits,
                                  RasterShader *handles_out)
{
  u32 tbl_offset = rasterShaders.reserveRows(num_shaders);
  if (tbl_offset == AllocOOM) [[unlikely]] {
    reportError(ErrorStatus::TableFull);
    return;
  }

  for (i32 shader_idx = 0; shader_idx < num_shaders; shader_idx++) {
    const RasterShaderInit &shader_init = shader_inits[shader_idx];
    const RasterHWConfig &raster_cfg = shader_init.rasterConfig;

    wgpu::ShaderModuleWGSLDescriptor wgsl_desc {{
      .nextInChain = nullptr,
      .code = (const char *)shader_init.byteCode.data,
    }};
    wgpu::ShaderModuleDescriptor shader_mod_desc {
      .nextInChain = &wgsl_desc,
    };

    wgpu::ShaderModule shader_mod = dev.CreateShaderModule(&shader_mod_desc);

    wgpu::PrimitiveState primitive_state {
      .cullMode = convertCullMode(raster_cfg.cullMode),
    };

    const BackendRasterPassConfig *pass_cfg = 
        getRasterPassConfigByID(shader_init.rasterPass);

    wgpu::DepthStencilState depth_state {
      .format = pass_cfg->depthAttachment.format,
      .depthWriteEnabled = raster_cfg.writeDepth,
      .depthCompare = convertDepthCompare(raster_cfg.depthCompare),
    };

    wgpu::ColorTargetState color_tgt_states[MAX_COLOR_ATTACHMENTS];
    for (i32 i = 0; i < pass_cfg->numColorAttachments; i++) {
      color_tgt_states[i] = {
        .format = pass_cfg->colorAttachments[i].format,
      };
    }

    wgpu::VertexState vert_state {
      .module = shader_mod,
      .entryPoint = shader_init.vertexEntry,
      .constantCount = 0,
      .constants = nullptr,
      .bufferCount = 0,
      .buffers = nullptr,
    };

    wgpu::FragmentState frag_state {
      .module = shader_mod,
      .entryPoint = shader_init.fragmentEntry,
      .constantCount = 0,
      .constants = nullptr,
      .targetCount = (size_t)pass_cfg->numColorAttachments,
      .targets = color_tgt_states,
    };

    wgpu::BindGroupLayout bind_group_layouts[MAX_BIND_GROUPS_PER_SHADER];

    i32 num_bind_groups = shader_init.paramBlockTypes.size();
    for (i32 i = 0; i < num_bind_groups; i++) {
      bind_group_layouts[i] =
        getBindGroupLayoutByParamBlockTypeID(shader_init.paramBlockTypes[i]);
    }

    wgpu::PipelineLayoutDescriptor layout_descriptor {
      .bindGroupLayoutCount = (size_t)num_bind_groups,
      .bindGroupLayouts = bind_group_layouts,
    };

    wgpu::PipelineLayout pipeline_layout =
      dev.CreatePipelineLayout(&layout_descriptor);

    wgpu::RenderPipelineDescriptor pipeline_desc {
      .layout = std::move(pipeline_layout),
      .vertex = vert_state,
      .primitive = primitive_state,
      .depthStencil =
          pass_cfg->depthAttachment.format == wgpu::TextureFormat::Undefined ?
              nullptr : &depth_state,
      .fragment = &frag_state,
    };

    wgpu::RenderPipeline pipeline = dev.CreateRenderPipeline(&pipeline_desc);

    auto [to_pipeline, _, id] = rasterShaders.get(tbl_offset, shader_idx);
    new (to_pipeline) wgpu::RenderPipeline(std::move(pipeline));
    handles_out[shader_idx] = id;
  }
}

void Backend::destroyRasterShaders(i32 num_shaders, RasterShader *handles)
{
  rasterShaders.releaseResources(num_shaders, handles,
    [](wgpu::RenderPipeline *to_pipeline, auto)
  {
    to_pipeline->~RenderPipeline();
  });
}

Swapchain Backend::createSwapchain(Surface surface,
                                   SwapchainProperties *properties)
{
  wgpu::Surface wgpu_surface((WGPUSurface)surface.hdl.ptr);

  wgpu::SurfaceCapabilities capabilities;
  wgpu_surface.GetCapabilities(adapter, &capabilities);
  bool supports_copy_dst =
    ((u64)capabilities.usages & (u64)wgpu::TextureUsage::CopyDst) != 0;
  wgpu::TextureFormat format = capabilities.formats[0];

  wgpu::SurfaceConfiguration surface_cfg {
      .device = dev,
      .format = format,
      .viewFormatCount = 0,
      .viewFormats = nullptr,
      .alphaMode = wgpu::CompositeAlphaMode::Opaque,
      .width = (u32)surface.width,
      .height = (u32)surface.height,
  };
  wgpu_surface.Configure(&surface_cfg);

  i32 swapchain_idx = swapchains.popFromFreeList();

  Texture reserved_hdl;
  {
    u32 tbl_row = textures.reserveRows(1);
    if (tbl_row == AllocOOM) {
      FATAL("Out of texture handles while creating swapchain");
    }

    auto [to_hot, to_cold, id] = textures.get(tbl_row, 0);

    new (to_cold) BackendTextureCold {};
    new (to_hot) BackendTexture {};
    reserved_hdl = id;
  }

  TextureFormat mapped_fmt = convertWebGPUTextureFormat(format);

  if (mapped_fmt == TextureFormat::None) {
    FATAL("Unknown / unsupported swapchain format: 0x%X", (u32)format);
  }

  *properties = {
    .format = mapped_fmt,
    .supportsCopyDst = supports_copy_dst,
  };

  new (&swapchains[swapchain_idx]) BackendSwapchain {
    .surface = std::move(wgpu_surface),
    .view = {},
    .reservedHandle = reserved_hdl,
  };

  return Swapchain { swapchain_idx };
}

void Backend::destroySwapchain(Swapchain swapchain)
{
  i32 swapchain_idx = swapchain.id;

  BackendSwapchain &wgpu_swapchain = swapchains[swapchain.id];
  assert(wgpu_swapchain.view == nullptr);

  {
    textures.releaseResources(1, &wgpu_swapchain.reservedHandle,
      [](BackendTexture *to_hot, BackendTextureCold *to_cold)
    {
      assert(to_hot->view == nullptr &&
             to_cold->texture == nullptr);

      to_hot->~BackendTexture();
      to_cold->~BackendTextureCold();
    });
  }

  wgpu_swapchain.surface.Unconfigure();
  wgpu_swapchain.~BackendSwapchain();

  swapchains.pushToFreeList(swapchain_idx);
}

AcquireSwapchainResult Backend::acquireSwapchainImage(Swapchain swapchain)
{
  BackendSwapchain &wgpu_swapchain = swapchains[swapchain.id];

  wgpu::SurfaceTexture surface_tex;
  wgpu_swapchain.surface.GetCurrentTexture(&surface_tex);
  if (surface_tex.status !=
      wgpu::SurfaceGetCurrentTextureStatus::Success) [[unlikely]] {
    return {
      .texture = {},
      .status = SwapchainStatus::Invalid,
    };
  }

  wgpu::Texture tex = surface_tex.texture;
  wgpu::TextureView view = tex.CreateView();

  wgpu_swapchain.view = view;

  auto [to_hot, to_cold, id] =
    textures.get(wgpu_swapchain.reservedHandle.id, 0);
  to_cold->texture = std::move(tex);
  to_hot->view = std::move(view);

  return {
    .texture = wgpu_swapchain.reservedHandle,
    .status = surface_tex.suboptimal ?
      SwapchainStatus::Suboptimal : SwapchainStatus::Valid,
  };
}

void Backend::presentSwapchainImage(Swapchain swapchain)
{
  BackendSwapchain &wgpu_swapchain = swapchains[swapchain.id];

  wgpu_swapchain.surface.Present();

  wgpu_swapchain.view = nullptr;

  auto [to_hot, to_cold, id] =
    textures.get(wgpu_swapchain.reservedHandle.id, 0);
  to_hot->view = nullptr;
  to_cold->texture = nullptr;
}

void Backend::waitForIdle()
{
}


void Backend::submit(FrontendCommands *frontend_cmds)
{
#ifdef GAS_WGPU_DEBUG_PRINT
  printf("WGPU: begin submit\n");
#endif

  wgpu::CommandEncoder wgpu_enc = dev.CreateCommandEncoder();

  CommandDecoder decoder(frontend_cmds);

  auto encodeRasterPass = [&]()
  {
    decoder.resetDrawParams();

    auto raster_pass = decoder.id<RasterPass>();
    const BackendRasterPass &backend_pass = *rasterPasses.hot(raster_pass);

    wgpu::RenderPassColorAttachment color_attachments[MAX_COLOR_ATTACHMENTS];
    wgpu::RenderPassDepthStencilAttachment depth_attachment;

    for (i32 i = 0; i < backend_pass.numColorAttachments; i++) {
      const BackendColorAttachment &in =
        backend_pass.colorAttachments[i];
      wgpu::RenderPassColorAttachment &out =
        color_attachments[i];

      if (i == backend_pass.swapchainAttachmentIndex) {
        BackendSwapchain &backend_swapchain =
          swapchains[backend_pass.swapchain.id];
        out.view = backend_swapchain.view;
      } else {
        out.view = in.view;
      }
      out.loadOp = in.loadOp;
      out.storeOp = in.storeOp;
      out.clearValue = {
        in.clearValue.x, 
        in.clearValue.y, 
        in.clearValue.z, 
        in.clearValue.w, 
      };
    }

    wgpu::RenderPassDescriptor pass_descriptor;
    pass_descriptor.colorAttachmentCount =
      (size_t)backend_pass.numColorAttachments;
    pass_descriptor.colorAttachments = color_attachments;

    if (backend_pass.depthAttachment.view == nullptr) {
      pass_descriptor.depthStencilAttachment = nullptr;
    } else {
      depth_attachment.view = backend_pass.depthAttachment.view;
      depth_attachment.depthLoadOp =
        backend_pass.depthAttachment.loadOp;
      depth_attachment.depthStoreOp =
        backend_pass.depthAttachment.storeOp;
      depth_attachment.depthClearValue =
        backend_pass.depthAttachment.clearValue;

      pass_descriptor.depthStencilAttachment = &depth_attachment;
    }

    wgpu::RenderPassEncoder pass_enc =
        wgpu_enc.BeginRenderPass(&pass_descriptor);

    for (CommandCtrl ctrl; (ctrl = decoder.ctrl()) != CommandCtrl::None;) {
#ifdef GAS_WGPU_DEBUG_PRINT
      debugPrintDrawCommandCtrl(ctrl);
#endif

      bool draw = (ctrl & CommandCtrl::Draw) != CommandCtrl::None;
      bool indexed_draw =
        (ctrl & CommandCtrl::DrawIndexed) != CommandCtrl::None;
      if (draw || indexed_draw) {
        if (RasterShader shader = decoder.drawShader(ctrl); !shader.null()) {
          pass_enc.SetPipeline(*rasterShaders.hot(shader));
        }

        if (ParamBlock pb0 = decoder.drawParamBlock0(ctrl); !pb0.null()) {
          pass_enc.SetBindGroup(0, *paramBlocks.hot(pb0));
        }

        if (ParamBlock pb1 = decoder.drawParamBlock1(ctrl); !pb1.null()) {
          pass_enc.SetBindGroup(1, *paramBlocks.hot(pb1));
        }

        if (ParamBlock pb2 = decoder.drawParamBlock2(ctrl); !pb2.null()) {
          pass_enc.SetBindGroup(2, *paramBlocks.hot(pb2));
        }

        if (u32 dyn_offset_0 = decoder.dynamicBufferOffset0(ctrl);
            dyn_offset_0 != 0xFFFF'FFFF) {
          assert(false);
        }

        if (u32 dyn_offset_1 = decoder.dynamicBufferOffset1(ctrl);
            dyn_offset_1 != 0xFFFF'FFFF) {
          assert(false);
        }

        DrawParams draw_params = decoder.drawParams(ctrl);

#ifdef GAS_WGPU_DEBUG_PRINT
        printf(R"(DrawParams:
  NumTriangles: %u
  NumInstances: %u
  VertexOffset: %u
  InstanceOffset: %u
)", draw_params.numTriangles, draw_params.numInstances,
    draw_params.vertexOffset, draw_params.instanceOffset);
#endif

        if (draw) {
          pass_enc.Draw(draw_params.numTriangles * 3,
                        draw_params.numInstances,
                        draw_params.vertexOffset,
                        draw_params.instanceOffset);
        } else {
          pass_enc.DrawIndexed(draw_params.numTriangles * 3,
                               draw_params.numInstances,
                               draw_params.indexOffset,
                               draw_params.vertexOffset,
                               draw_params.instanceOffset);
        }
      } else {
        assert(false);
      }
    }

    pass_enc.End();
  };

  while (!decoder.done()) {
    CommandCtrl ctrl = decoder.ctrl();

    switch (ctrl) {
      case CommandCtrl::RasterPass: {
        encodeRasterPass();
      } break;
      case CommandCtrl::ComputePass: {
      } break;
      case CommandCtrl::CopyPass: {
      } break;
      default: MADRONA_UNREACHABLE();
    }
  }

  wgpu::CommandBuffer cmd_buffer = wgpu_enc.Finish();
  queue.Submit(1, &cmd_buffer);
}

BackendRasterPassConfig * Backend::getRasterPassConfigByID(
    RasterPassInterfaceID id)
{
  if (!id.isUUID()) {
    return rasterPassInterfaces.hot(id.asHandle());
  } else {
    i32 tbl_row = rasterPassInterfaceIDs.lookup(id.asUUID());
    if (tbl_row == ResourceUUIDMap::NOT_FOUND) [[unlikely]] {
      return nullptr;
    } else {
      auto [to_cfg, _1, _2] = rasterPassInterfaces.get(tbl_row, 0);
      return to_cfg;
    }
  }
}

wgpu::BindGroupLayout Backend::getBindGroupLayoutByParamBlockTypeID(
    ParamBlockTypeID id)
{
  if (!id.isUUID()) {
    return paramBlockTypes.hot(id.asHandle())->layout;
  } else {
    i32 tbl_row = paramBlockTypeIDs.lookup(id.asUUID());
    if (tbl_row == ResourceUUIDMap::NOT_FOUND) [[unlikely]] {
      return nullptr;
    } else {
      auto [to_block_type, _1, _2] = paramBlockTypes.get(tbl_row, 0);
      return to_block_type->layout;
    }
  }
}

GPULib * loadWebGPULib()
{
  return nullptr;
}

GPUAPI * initWebGPU(GPULib *lib, const APIConfig &cfg)
{
  (void)lib;

  printf("%lu\n", sizeof(Backend));

  return WebGPUAPI::init(cfg);
}

}
