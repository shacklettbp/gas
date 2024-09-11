#pragma once

#include "namespace.hpp"

namespace gas {

#if 0
OffsetAlloc class modified from https://github.com/sebbbi/OffsetAllocator
Copyright (c) 2023 Sebastian Aaltonen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
#endif

constexpr inline u32 AllocOOM = 0xFFFF'FFFF;

struct OffsetAllocation {
  u32 offset;
  u32 metadata;
};

class OffsetAllocator { 
public:
  static constexpr inline u32 NUM_TOP_BINS = 32;
  static constexpr inline u32 BINS_PER_LEAF = 8;
  static constexpr inline u32 BIN_FLT_MANTISSA_BITS = 3;
  static constexpr inline u32 LEAF_BINS_INDEX_MASK = 0x7;
  static constexpr inline u32 NUM_LEAF_BINS = NUM_TOP_BINS * BINS_PER_LEAF;
  static constexpr inline u32 MAX_ALLOCS = 128 * 1024;

  OffsetAllocator(u32 init_size);
  OffsetAllocator(OffsetAllocator &&other);
  ~OffsetAllocator();

  void reset();

  OffsetAllocation alloc(u32 size);
  void dealloc(OffsetAllocation allocation);

  u32 allocationSize(OffsetAllocation allocation);

  struct StorageReport {
    u32 totalFreeSpace;
    u32 largestFreeRegion;
  };

  struct StorageReportFull {
    struct Region {
      u32 size;
      u32 count;
    };

    Region freeRegions[NUM_LEAF_BINS];
  };

  StorageReport storageReport();
  StorageReportFull storageReportFull();
    
private:
  u32 insertNodeIntoBin(u32 size, u32 dataOffset);
  void removeNodeFromBin(u32 nodeIndex);

  struct Node {
    static constexpr inline u32 UNUSED = 0xFFFF'FFFF;
    
    u32 dataOffset = 0;
    u32 dataSize = 0;
    u32 binListPrev = UNUSED;
    u32 binListNext = UNUSED;
    u32 neighborPrev = UNUSED;
    u32 neighborNext = UNUSED;
    bool used = false; // TODO: Merge as bit flag
  };

  u32 size_;
  u32 free_storage_;

  u32 used_bins_top_;
  u8 used_bins_[NUM_TOP_BINS];
  u32 bin_indices_[NUM_LEAF_BINS];
          
  Node *nodes_;
  u32 *free_nodes_;
  u32 free_offset_;
};

// Modified version of OffsetAllocator that can only assign up to
// 65536 elements
class TableAllocator {
public:
  static constexpr inline u32 MAX_NUM_ELEMS = 1 << 16;
  static constexpr inline u32 MAX_CONTIGUOUS = 1024;

  struct Node {
    u32 size;
    u16 freeListPrev;
    u16 freeListNext;
  };

  TableAllocator();
  TableAllocator(TableAllocator &) = delete;

  u32 alloc(u32 size);
  void dealloc(u32 offset, u32 size);

private:
  static constexpr inline u32 NUM_TOP_BINS = 7;
  static constexpr inline u32 BIN_FLT_MANTISSA_BITS = 3;
  static constexpr inline u32 BINS_PER_LEAF = 1 << BIN_FLT_MANTISSA_BITS;
  static constexpr inline u32 LEAF_BINS_IDX_MASK = BINS_PER_LEAF - 1;
  static constexpr inline u32 NUM_LEAF_BINS = NUM_TOP_BINS * BINS_PER_LEAF;
  static constexpr inline u16 SENTINEL = 0xFFFF;
  static constexpr inline u32 NUM_FREE_BITFIELDS = MAX_NUM_ELEMS / 32;

  void addFreeNode(u32 node_idx, u32 size);
  void removeFreeNodeForMerge(u32 node_idx);

  u8 free_top_bins_;
  u8 free_leaf_bins_[NUM_TOP_BINS];
  u16 bin_free_heads_[NUM_LEAF_BINS];
  u32 node_free_states_[NUM_FREE_BITFIELDS];
  Node nodes_[MAX_NUM_ELEMS];
};

}
