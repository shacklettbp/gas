#include "test_gpu.hpp"

namespace gas::test {
namespace {

class GPUTmpInput : public GPUTest {
protected:
  RasterShader setupFullScreenTestShader()
  {
    auto backend_bytecode_type = gpuAPI->backendShaderByteCodeType();
  
    StackAlloc shaderc_alloc;
    ShaderByteCode shader_bytecode;
    {
      ShaderCompileResult compile_result =
        shaderc->compileShader(shaderc_alloc, {
          .path = GAS_TEST_DIR "tmp_input.slang",
        });
  
      if (compile_result.diagnostics.size() != 0) {
        fprintf(stderr, "%s", compile_result.diagnostics.data());
      }
  
      if (!compile_result.success) {
        FATAL("Shader compilation failed!");
      }
  
      shader_bytecode =
          compile_result.getByteCodeForBackend(backend_bytecode_type);
    }
  
    RasterShader shader = gpu->createRasterShader({
      .byteCode = shader_bytecode,
      .vertexEntry = "vertMain",
      .fragmentEntry = "fragMain",
      .rasterPass = { rp_iface_ },
      .numPerDrawBytes = sizeof(Vector3),
    });
    shaderc_alloc.release();
  
    return shader;
  }

  void SetUp() override
  {
    main_queue_ = gpu->getMainQueue();

    rp_iface_ = gpu->createRasterPassInterface({
      .uuid = "rp"_to_uuid,
      .colorAttachments = {
        { .format = TextureFormat::RGBA8_UNorm },
      },
    });

    shader_ = setupFullScreenTestShader();
  }

  void TearDown() override
  {
    gpu->waitUntilIdle();

    gpu->destroyRasterShader(shader_);
    gpu->destroyRasterPassInterface(rp_iface_);
  }

  GPUQueue main_queue_;
  RasterPassInterface rp_iface_;
  RasterShader shader_;
};

TEST_F(GPUTmpInput, MultiRasterPass)
{
  u16 res = 64;

  Texture attachment0 = gpu->createTexture({
    .format = TextureFormat::RGBA8_UNorm,
    .width = res,
    .height = res,
    .usage = TextureUsage::ColorAttachment | TextureUsage::CopySrc,
  });

  Texture attachment1 = gpu->createTexture({
    .format = TextureFormat::RGBA8_UNorm,
    .width = res,
    .height = res,
    .usage = TextureUsage::ColorAttachment | TextureUsage::CopySrc,
  });

  RasterPass rp0 = gpu->createRasterPass({
    .interface = rp_iface_,
    .colorAttachments = { attachment0 },
  });

  RasterPass rp1 = gpu->createRasterPass({
    .interface = rp_iface_,
    .colorAttachments = { attachment1 },
  });

  Buffer readback = gpu->createReadbackBuffer(2 * (u32)res * (u32)res * 4);

  CommandEncoder enc = gpu->createCommandEncoder(main_queue_);
  const i32 num_iters = 8;
  for (i32 i = 0; i < num_iters; i++) {
    gpuAPI->processGraphicsEvents();

    enc.beginEncoding();

    u8 *base;
    {
      RasterPassEncoder raster_enc = enc.beginRasterPass(rp0);
      base = raster_enc.tmpBuffer(1).ptr;

      raster_enc.setShader(shader_);
      raster_enc.drawData(Vector3 { 1, 1, 0 });
      u8 *post_draw_data = raster_enc.tmpBuffer(1).ptr;
      EXPECT_EQ(post_draw_data - base, 512);
      raster_enc.draw(0, 1);
      enc.endRasterPass(raster_enc);
    }

    {
      RasterPassEncoder raster_enc = enc.beginRasterPass(rp1);
      raster_enc.setShader(shader_);
      u8 *new_pass_data = raster_enc.tmpBuffer(1).ptr;
      EXPECT_EQ(new_pass_data - base, 768);
      raster_enc.drawData(Vector3 { 1, 0, 1 });
      u8 *post_draw_data = raster_enc.tmpBuffer(1).ptr;
      EXPECT_EQ(post_draw_data - base, 1280);
      raster_enc.draw(0, 1);
      enc.endRasterPass(raster_enc);
    }

    {
      CopyPassEncoder copy_enc = enc.beginCopyPass();

      copy_enc.copyTextureToBuffer(
          attachment0, readback, 0, 0);

      copy_enc.copyTextureToBuffer(
          attachment1, readback, 0, (u32)res * (u32)res * 4);

      enc.endCopyPass(copy_enc);
    }

    enc.endEncoding();

    gpu->submit(main_queue_, enc);
    gpu->waitUntilReady(main_queue_);

    {
      uint8_t *readback_ptr1 = (uint8_t *)gpu->beginReadback(readback);
      uint8_t *readback_ptr2 = readback_ptr1 + (u32)res * (u32)res * 4;

      for (i32 y = 0; y < res; y++) {
        for (i32 x = 0; x < res; x++) {
          EXPECT_EQ(readback_ptr1[0], 255);
          EXPECT_EQ(readback_ptr1[1], 255);
          EXPECT_EQ(readback_ptr1[2], 0);
          EXPECT_EQ(readback_ptr1[3], 255);

          EXPECT_EQ(readback_ptr2[0], 255);
          EXPECT_EQ(readback_ptr2[1], 0);
          EXPECT_EQ(readback_ptr2[2], 255);
          EXPECT_EQ(readback_ptr2[3], 255);

          readback_ptr1 += 4;
          readback_ptr2 += 4;
        }
      }

      gpu->endReadback(readback);
    }
  }

  gpuAPI->processGraphicsEvents();
  gpu->waitUntilReady(main_queue_);

  gpu->destroyCommandEncoder(enc);
  gpu->destroyReadbackBuffer(readback);

  gpu->destroyTexture(attachment1);
  gpu->destroyTexture(attachment0);
  gpu->destroyRasterPass(rp1);
  gpu->destroyRasterPass(rp0);
}

TEST_F(GPUTmpInput, MultiBlock)
{
  u16 res = 64;

  Texture attachment0 = gpu->createTexture({
    .format = TextureFormat::RGBA8_UNorm,
    .width = res,
    .height = res,
    .usage = TextureUsage::ColorAttachment | TextureUsage::CopySrc,
  });

  Texture attachment1 = gpu->createTexture({
    .format = TextureFormat::RGBA8_UNorm,
    .width = res,
    .height = res,
    .usage = TextureUsage::ColorAttachment | TextureUsage::CopySrc,
  });

  RasterPass rp0 = gpu->createRasterPass({
    .interface = rp_iface_,
    .colorAttachments = { attachment0 },
  });

  RasterPass rp1 = gpu->createRasterPass({
    .interface = rp_iface_,
    .colorAttachments = { attachment1 },
  });

  Buffer readback = gpu->createReadbackBuffer(2 * (u32)res * (u32)res * 4);

  CommandEncoder enc = gpu->createCommandEncoder(main_queue_);
  const i32 num_iters = 16; 
  for (i32 i = 0; i < num_iters; i++) {
    gpuAPI->processGraphicsEvents();

    enc.beginEncoding();

    f32 iter_v = f32(i) / (num_iters - 1);
    {
      RasterPassEncoder raster_enc = enc.beginRasterPass(rp0);
      raster_enc.tmpBuffer(GPUTmpInputBlock::BLOCK_SIZE);

      raster_enc.setShader(shader_);
      raster_enc.drawData(Vector3 { 1, 1, iter_v });
      raster_enc.draw(0, 1);
      enc.endRasterPass(raster_enc);
    }

    {
      CopyPassEncoder copy_enc = enc.beginCopyPass();
      for (i32 j = 0; j < 65; j++) {
        copy_enc.tmpBuffer(GPUTmpInputBlock::BLOCK_SIZE);
      }
      enc.endCopyPass(copy_enc);
    }

    {
      RasterPassEncoder raster_enc = enc.beginRasterPass(rp1);
      raster_enc.tmpBuffer(GPUTmpInputBlock::BLOCK_SIZE);

      raster_enc.setShader(shader_);
      raster_enc.drawData(Vector3 { 1, iter_v, 1 });
      raster_enc.draw(0, 1);
      enc.endRasterPass(raster_enc);
    }

    {
      CopyPassEncoder copy_enc = enc.beginCopyPass();

      copy_enc.copyTextureToBuffer(
          attachment0, readback, 0, 0);

      copy_enc.copyTextureToBuffer(
          attachment1, readback, 0, (u32)res * (u32)res * 4);

      enc.endCopyPass(copy_enc);
    }

    enc.endEncoding();

    gpu->submit(main_queue_, enc);
    gpu->waitUntilReady(main_queue_);

    {
      uint8_t *readback_ptr1 = (uint8_t *)gpu->beginReadback(readback);
      uint8_t *readback_ptr2 = readback_ptr1 + (u32)res * (u32)res * 4;

      for (i32 y = 0; y < res; y++) {
        for (i32 x = 0; x < res; x++) {
          EXPECT_EQ(readback_ptr1[0], 255);
          EXPECT_EQ(readback_ptr1[1], 255);
          EXPECT_EQ(readback_ptr1[2], u8(255 * iter_v));
          EXPECT_EQ(readback_ptr1[3], 255);

          EXPECT_EQ(readback_ptr2[0], 255);
          EXPECT_EQ(readback_ptr2[1], u8(255 * iter_v));
          EXPECT_EQ(readback_ptr2[2], 255);
          EXPECT_EQ(readback_ptr2[3], 255);

          readback_ptr1 += 4;
          readback_ptr2 += 4;
        }
      }

      gpu->endReadback(readback);
    }
  }

  gpuAPI->processGraphicsEvents();
  gpu->waitUntilReady(main_queue_);

  gpu->destroyCommandEncoder(enc);
  gpu->destroyReadbackBuffer(readback);

  gpu->destroyTexture(attachment1);
  gpu->destroyTexture(attachment0);
  gpu->destroyRasterPass(rp1);
  gpu->destroyRasterPass(rp0);
}

}
}
