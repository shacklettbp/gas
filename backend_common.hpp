#pragma once

#include "namespace.hpp"
#include "gas.hpp"
#include "mem.hpp"

#include <madrona/utils.hpp>
#include <madrona/macros.hpp>

namespace gas {

inline u32 bytesPerTexelForFormat(TextureFormat fmt)
{
  using enum TextureFormat;

  switch (fmt) {
    case None: return 0;
    case RGBA8_UNorm:
    case RGBA8_SRGB:
    case BGRA8_UNorm:
    case BGRA8_SRGB:
        return 4;
    case Depth32_Float: return 4;
    default: MADRONA_UNREACHABLE();
  }
}

template <typename ID, typename Hot, typename Cold>
struct ResourceTable {
  static constexpr inline i32 MAX_NUM_ELEMS =
    (i32)TableAllocator::MAX_NUM_ELEMS;

  static constexpr inline i32 computeChunkSize()
  {
    constexpr i32 num_hot_bytes = (i32)sizeof(Hot) + (i32)sizeof(u16);

    constexpr i32 desired_chunk_size = utils::roundUp(
      std::max(MADRONA_CACHE_LINE, num_hot_bytes), MADRONA_CACHE_LINE);

    constexpr i32 elems_per_chunk = desired_chunk_size / num_hot_bytes;

    static_assert(elems_per_chunk > 0);
    return elems_per_chunk;
  }

  static constexpr inline i32 CHUNK_SIZE = computeChunkSize();
  static constexpr inline i32 NUM_HOT_CHUNKS = 
    utils::divideRoundUp(MAX_NUM_ELEMS, CHUNK_SIZE);

  struct alignas(MADRONA_CACHE_LINE) HotChunk {
    Hot hot[CHUNK_SIZE];
    u16 gen[CHUNK_SIZE];
  };

  static_assert(sizeof(HotChunk) % MADRONA_CACHE_LINE == 0);

  union ColdDataAndFreeList {
    Cold data;
    TableAllocator::Node freeNode;

    ~ColdDataAndFreeList() {};
  };

  struct Uninit {};

  struct Store {
    HotChunk hotChunks[NUM_HOT_CHUNKS];
    ColdDataAndFreeList cold[MAX_NUM_ELEMS];
  };

  union {
    Uninit uninit;
    Store store;
  };

  struct Ref {
    Hot *hot;
    Cold *cold;
    ID id;
  };

  TableAllocator alloc;

  inline ResourceTable()
    : uninit(),
      alloc()
  {
    for (i32 chunk_idx = 0; chunk_idx < NUM_HOT_CHUNKS; chunk_idx++) {
      HotChunk &chunk = store.hotChunks[chunk_idx];
      for (i32 i = 0; i < CHUNK_SIZE; i++) {
        chunk.gen[i] = 1;
      }
    }
  }

  inline ~ResourceTable() {}

  inline Hot * hot(ID id)
  {
    i32 idx = (i32)id.id;

    i32 chunk_idx = idx / CHUNK_SIZE;
    i32 chunk_offset = idx % CHUNK_SIZE;

    HotChunk &hot_chunk = store.hotChunks[chunk_idx];

    if (hot_chunk.gen[chunk_offset] != id.gen) {
      return nullptr;
    }

    return &hot_chunk.hot[chunk_offset];
  }

  // Note: this does not verify gen!
  inline Cold * cold(ID id)
  {
    return &store.cold[id.id].data;
  }

  inline Ref get(u32 range_start, i32 offset)
  {
    i32 idx = (i32)range_start + offset;

    i32 chunk_idx = idx / CHUNK_SIZE;
    i32 chunk_offset = idx % CHUNK_SIZE;

    HotChunk &hot_chunk = store.hotChunks[chunk_idx];

    return {
      .hot = &hot_chunk.hot[chunk_offset],
      .cold = &store.cold[idx].data,
      .id = {
        .gen = hot_chunk.gen[chunk_offset],
        .id = (u16)idx,
      },
    };
  }

  inline u32 reserveRows(i32 num_elems)
  {
    return alloc.alloc((u32)num_elems);
  }

  inline void releaseRows(u32 range_start, i32 num_elems)
  {
    alloc.dealloc(range_start, (u32)num_elems);
  }

  template <typename Fn>
  void releaseResources(i32 num_resources,
                        const ID *resources,
                        Fn &&destructor)
  {
    u32 range_start;
    i32 prev_row;
    i32 range_size = 0;

    for (i32 resource_idx = 0; resource_idx < num_resources; resource_idx++) {
      ID id = resources[resource_idx];

      i32 tbl_row = (i32)id.id;

      i32 chunk_idx = tbl_row / CHUNK_SIZE;
      i32 chunk_offset = tbl_row % CHUNK_SIZE;

      HotChunk &hot_chunk = store.hotChunks[chunk_idx];

      u16 gen = hot_chunk.gen[chunk_offset];
      if (gen != id.gen) {
        continue;
      }

      auto *to_hot = &hot_chunk.hot[chunk_offset];
      auto *to_cold = cold(id);
  
      destructor(to_hot, to_cold);

      // gen == 0 represents an invalid ID, so we increment directly to 1 
      // in the wraparound case
      hot_chunk.gen[chunk_offset] = gen == 65535 ? 1 : (gen + 1);

      if (range_size == 0) {
        range_start = (u32)tbl_row;
      } else if (tbl_row != prev_row + 1) {
        releaseRows(range_start, range_size);
        range_size = 0;
      }

      prev_row = tbl_row;
      range_size += 1;
    }

    if (range_size > 0) {
      releaseRows(range_start, range_size);
    }
  }
};


class ResourceUUIDMap {
public:
  static constexpr inline i32 NOT_FOUND = 0xFFFF'FFFF;

  ResourceUUIDMap();
  i32 lookup(UUID uuid);
  void insert(UUID uuid, u16 row);
  void remove(UUID uuid);

private:
  static constexpr inline i32 BUCKET_SIZE = 4;
  static constexpr inline i32 NUM_BUCKETS =
    TableAllocator::MAX_NUM_ELEMS;

  struct Bucket {
    u64 hashes[BUCKET_SIZE];
    u16 rows[BUCKET_SIZE]; 
  };
  
  struct Hash {
    u64 key1;
    u64 key2;
    Bucket *bucket1;
    Bucket *bucket2;
  };

  inline Hash hash(UUID uuid);

  Bucket buckets_[NUM_BUCKETS];
};

#if 0
template <typename ID, typename T, u16 N>
class ResourceArrayStore {
public:
  ID reserve()
  {

  }

  void release(ID id)
  {
    AtomicU32Ref atomic_tail(free_tail_);

    u32 freelist_slot = atomic_tail.fetch_add_relaxed();
  }

  T * operator[](ID id)
  {
    return store_.elems[idx].data;
  }

private:
  struct Elem {
    T data;
    u16 gen;
    u16 free_idx_;
  };

  struct Empty {};

  union Store {
    Elem elems[N];
    Empty empty;

    inline Store() : empty() {};
  }

  Store store_ {};
  u32 free_head_ { 0 };
  u32 free_tail_ { N - 1 };
};
#endif

template <typename T, i32 NUM_INLINE = 1>
class InlineArrayFreeList {
public:
  InlineArrayFreeList()
    : inline_data_(),
      free_head_(0),
      capacity_(NUM_INLINE)
  {
    static_assert(NUM_INLINE > 0);

    for (i32 i = 0; i < NUM_INLINE - 1; i++) {
      inline_data_[i].nextFree = i + 1;
    }
    inline_data_[NUM_INLINE - 1].nextFree = -1;
  }

  InlineArrayFreeList(const InlineArrayFreeList &) = delete;
  ~InlineArrayFreeList()
  {
    if (capacity_ > NUM_INLINE) {
      free(data_);
    }
  }

  i32 popFromFreeList()
  {
    if (free_head_ == -1) [[unlikely]] {
      i32 new_capacity = capacity_ * 2;
      auto new_data = (Elem *)malloc(sizeof(Elem) * new_capacity);
      if (capacity_ > NUM_INLINE) {
        memcpy(new_data, data_, sizeof(Elem) * capacity_);
        free(data_);
      } else {
        memcpy(new_data, inline_data_, sizeof(Elem) * capacity_);
      }

      data_ = new_data;

      for (i32 i = capacity_; i < new_capacity - 1; i++) {
        data_[i].nextFree = i + 1;
      }
      data_[new_capacity - 1].nextFree = -1;
      free_head_ = capacity_;
      capacity_ = new_capacity;
    }

    i32 idx = free_head_;
    Elem &e = get(idx);
    free_head_ = e.nextFree;

    return idx;
  }

  void pushToFreeList(i32 idx)
  {
    Elem &e = get(idx);
    e.nextFree = free_head_;
    free_head_ = idx;
  }

  inline T & operator[](i32 idx)
  { 
    return get(idx).data;
  }

private:
  union Elem {
    T data;
    i32 nextFree;

    inline Elem() : nextFree() {}
    inline ~Elem() {}
  };

  inline Elem & get(i32 idx)
  {
    if (capacity_ <= NUM_INLINE) [[likely]] {
      return inline_data_[idx];
    } else {
      return data_[idx];
    }
  }

  union {
    Elem inline_data_[NUM_INLINE];
    Elem *data_;
  };
  i32 free_head_;
  i32 capacity_;
};

struct DrawParams {
  u32 indexOffset = 0;
  u32 numTriangles = 0;
  u32 vertexOffset = 0;
  u32 instanceOffset = 0;
  u32 numInstances = 1;
};

inline void debugPrintDrawCommandCtrl(CommandCtrl ctrl)
{
  using enum CommandCtrl;

  assert((ctrl & (RasterDraw | RasterDrawIndexed)) != None);

  printf("Draw Control: ");
  if ((ctrl & RasterDraw) != None) {
    printf("RasterDraw");
  } else if ((ctrl & RasterDrawIndexed) != None) {
    printf("RasterDrawIndexed");
  }

  if ((ctrl & DrawShader) != None) {
    printf(" | Shader");
  }
  if ((ctrl & DrawParamBlock0) != None) {
    printf(" | ParamBlock[0]");
  }
  if ((ctrl & DrawParamBlock1) != None) {
    printf(" | ParamBlock[1]");
  }
  if ((ctrl & DrawParamBlock2) != None) {
    printf(" | ParamBlock[2]");
  }

  if ((ctrl & DrawDataBuffer) != None) {
    printf(" | DrawDataBuffer");
  }
  if ((ctrl & DrawDataOffset) != None) {
    printf(" | DrawDataOffset");
  }

  if ((ctrl & DrawIndexBuffer32) != None) {
    printf(" | IndexBuffer32");
  }
  if ((ctrl & DrawIndexBuffer16) != None) {
    printf(" | IndexBuffer32");
  }
  if ((ctrl & DrawIndexOffset) != None) {
    printf(" | IndexOffset");
  }
  if ((ctrl & DrawNumTriangles) != None) {
    printf(" | NumTriangles");
  }
  if ((ctrl & DrawVertexOffset) != None) {
    printf(" | VertexOffset");
  }
  if ((ctrl & DrawInstanceOffset) != None) {
    printf(" | InstanceOffset");
  }
  if ((ctrl & DrawNumInstances) != None) {
    printf(" | NumInstances");
  }
  printf("\n");
}

struct CopyBufferToBufferCmd {
  Buffer src;
  Buffer dst;
  u32 srcOffset;
  u32 dstOffset;
  u32 numBytes;
};

struct CopyBufferToTextureCmd {
  Buffer src;
  Texture dst;
  u32 srcOffset;
  u32 dstMipLevel;
};

struct CopyTextureToBufferCmd {
  Texture src;
  Buffer dst;
  u32 srcMipLevel;
  u32 dstOffset;
};

struct CopyClearBufferCmd {
  Buffer buffer;
  u32 offset;
  u32 numBytes;
};

class CommandDecoder {
public:
  inline CommandDecoder(FrontendCommands *cmds)
    : cmds_(cmds),
      offset_(0),
      draw_params_(),
      copy_cmd_()
  {}

  inline void resetDrawParams()
  {
    draw_params_ = {};
  }

  inline void resetCopyCommand()
  {
    copy_cmd_ = CopyCommand();
  }

  inline CommandCtrl ctrl() { return (CommandCtrl)next(); }

  template <typename T>
  inline T id()
  {
    u32 v = next();
    return T::fromUInt(v);
  }

  inline RasterShader drawShader(CommandCtrl ctrl)
  {
    if (t(ctrl, DrawShader)) {
      return id<RasterShader>();
    } else {
      return RasterShader {};
    }
  }

  inline ParamBlock drawParamBlock0(CommandCtrl ctrl)
  {
    if (t(ctrl, DrawParamBlock0)) {
      return id<ParamBlock>();
    } else {
      return ParamBlock {};
    }
  }

  inline ParamBlock drawParamBlock1(CommandCtrl ctrl)
  {
    if (t(ctrl, DrawParamBlock1)) {
      return id<ParamBlock>();
    } else {
      return ParamBlock {};
    }
  }

  inline ParamBlock drawParamBlock2(CommandCtrl ctrl)
  {
    if (t(ctrl, DrawParamBlock2)) {
      return id<ParamBlock>();
    } else {
      return ParamBlock {};
    }
  }

  inline Buffer drawDataBuffer(CommandCtrl ctrl)
  {
    if (t(ctrl, DrawDataBuffer)) {
      return id<Buffer>();
    } else {
      return {};
    }
  }

  inline u32 drawDataOffset(CommandCtrl ctrl)
  {
    if (t(ctrl, DrawDataOffset)) {
      return next();
    } else {
      return 0xFFFF'FFFF;
    }
  }

  inline Buffer drawVertexBuffer0(CommandCtrl ctrl)
  {
    if (t(ctrl, DrawVertexBuffer0)) {
      return id<Buffer>();
    } {
      return {};
    }
  }

  inline Buffer drawVertexBuffer1(CommandCtrl ctrl)
  {
    if (t(ctrl, CommandCtrl::DrawVertexBuffer1)) {
      return id<Buffer>();
    } {
      return {};
    }
  }

  inline Buffer drawIndexBuffer32(CommandCtrl ctrl)
  {
    if (t(ctrl, DrawIndexBuffer32)) {
      return id<Buffer>();
    } {
      return {};
    }
  }

  inline Buffer drawIndexBuffer16(CommandCtrl ctrl)
  {
    if (t(ctrl, DrawIndexBuffer16)) {
      return id<Buffer>();
    } {
      return {};
    }
  }

  inline DrawParams drawParams(CommandCtrl ctrl)
  {
    if (t(ctrl, DrawIndexOffset)) {
      draw_params_.indexOffset = next();
    }

    if (t(ctrl, DrawNumTriangles)) {
      draw_params_.numTriangles = next();
    }

    if (t(ctrl, DrawVertexOffset)) {
      draw_params_.vertexOffset = next();
    }

    if (t(ctrl, DrawInstanceOffset)) {
      draw_params_.instanceOffset = next();
    }

    if (t(ctrl, DrawNumInstances)) {
      draw_params_.numInstances = next();
    }

    return draw_params_;
  }

  inline CopyBufferToBufferCmd copyBufferToBuffer(CommandCtrl ctrl)
  {
    if (t(ctrl, CopyB2BSrcBuffer)) {
      copy_cmd_.data[0] = next();
    }

    if (t(ctrl, CopyB2BDstBuffer)) {
      copy_cmd_.data[1] = next();
    }

    if (t(ctrl, CopyB2BSrcOffset)) {
      copy_cmd_.data[2] = next();
    }
    
    if (t(ctrl, CopyB2BDstOffset)) {
      copy_cmd_.data[3] = next();
    }

    if (t(ctrl, CopyB2BNumBytes)) {
      copy_cmd_.data[4] = next();
    }

    return {
      .src = Buffer::fromUInt(copy_cmd_.data[0]),
      .dst = Buffer::fromUInt(copy_cmd_.data[1]),
      .srcOffset = copy_cmd_.data[2],
      .dstOffset = copy_cmd_.data[3],
      .numBytes = copy_cmd_.data[4],
    };
  }

  inline CopyBufferToTextureCmd copyBufferToTexture(CommandCtrl ctrl)
  {
    if (t(ctrl, CopyB2TSrcBuffer)) {
      copy_cmd_.data[0] = next();
    }

    if (t(ctrl, CopyB2TDstTexture)) {
      copy_cmd_.data[1] = next();
    }

    if (t(ctrl, CopyB2TSrcOffset)) {
      copy_cmd_.data[2] = next();
    }
    
    if (t(ctrl, CopyB2TDstMipLevel)) {
      copy_cmd_.data[3] = next();
    }

    return {
      .src = Buffer::fromUInt(copy_cmd_.data[0]),
      .dst = Texture::fromUInt(copy_cmd_.data[1]),
      .srcOffset = copy_cmd_.data[2],
      .dstMipLevel = copy_cmd_.data[3],
    };
  }

  inline CopyTextureToBufferCmd copyTextureToBuffer(CommandCtrl ctrl)
  {
    if (t(ctrl, CopyT2BSrcTexture)) {
      copy_cmd_.data[0] = next();
    }

    if (t(ctrl, CopyT2BDstBuffer)) {
      copy_cmd_.data[1] = next();
    }

    if (t(ctrl, CopyT2BSrcMipLevel)) {
      copy_cmd_.data[2] = next();
    }
    
    if (t(ctrl, CopyT2BDstOffset)) {
      copy_cmd_.data[3] = next();
    }

    return {
      .src = Texture::fromUInt(copy_cmd_.data[0]),
      .dst = Buffer::fromUInt(copy_cmd_.data[1]),
      .srcMipLevel = copy_cmd_.data[2],
      .dstOffset = copy_cmd_.data[3],
    };
  }

  inline CopyClearBufferCmd copyClear(CommandCtrl ctrl)
  {
    if (t(ctrl, CopyClearBuffer)) {
      copy_cmd_.data[0] = next();
    }

    if (t(ctrl, CopyClearOffset)) {
      copy_cmd_.data[1] = next();
    }

    if (t(ctrl, CopyClearNumBytes)) {
      copy_cmd_.data[2] = next();
    }
    
    return {
      .buffer = Buffer::fromUInt(copy_cmd_.data[0]),
      .offset = copy_cmd_.data[1],
      .numBytes = copy_cmd_.data[2],
    };
  }

private:
  using enum CommandCtrl;

  inline bool t(CommandCtrl ctrl, CommandCtrl flag)
  {
    return (ctrl & flag) != CommandCtrl::None;
  }

  inline u32 next()
  {
    u32 v = cmds_->data[offset_++];

    if (offset_ == (i32)cmds_->data.size()) [[unlikely]] {
      cmds_ = cmds_->next;
      offset_ = 0;
    }

    return v;
  }

  FrontendCommands *cmds_;
  i32 offset_;
  DrawParams draw_params_;
  CopyCommand copy_cmd_;
};

class BackendCommon : public GPURuntime {
public:
  BackendCommon(bool errors_are_fatal);

  ResourceUUIDMap paramBlockTypeIDs;
  ResourceUUIDMap rasterPassInterfaceIDs;

  void reportError(ErrorStatus error);

  u32 errorStatus;
  bool errorsAreFatal;
};



}
