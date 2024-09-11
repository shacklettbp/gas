#include "mem.hpp"
#include <cstring>
#include <bit>

#if 0
OffsetAlloc class modified from https://github.com/sebbbi/OffsetOffsetAlloc
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

namespace gas {

namespace {

inline u32 lzcntNonZero(u32 v)
{
#ifdef _MSC_VER
  unsigned long ret_val;
  _BitScanReverse(&ret_val, v);
  return 31 - ret_val;
#else
  return __builtin_clz(v);
#endif
}

inline u32 tzcntNonZero(u32 v)
{
#ifdef _MSC_VER
  unsigned long ret_val;
  _BitScanForward(&ret_val, v);
  return ret_val;
#else
  return __builtin_ctz(v);
#endif
}

template <u32 MANTISSA_BITS>
struct Binner {
  static constexpr inline u32 MANTISSA_VALUE = 1 << MANTISSA_BITS;
  static constexpr inline u32 MANTISSA_MASK = MANTISSA_VALUE - 1;

  // Bin sizes follow floating point (exponent + mantissa) distribution
  // (piecewise linear log approx)
  // This ensures that for each size class, the average overhead percentage
  // stays the same
  static inline u32 sizeToBinRoundUp(u32 size)
  {
    // Modeled after asfloat with a small float with
    // MANTISSA_BITS mantissa size

    u32 exp = 0;
    u32 mantissa = 0;

    if (size < MANTISSA_VALUE) {
      // Denorm: 0..(MANTISSA_VALUE-1)
      mantissa = size;
    } else {
      // Normalized: Hidden high bit always 1. Not stored. Just like float.
      u32 leadingZeros = lzcntNonZero(size);
      u32 highestSetBit = 31 - leadingZeros;

      u32 mantissaStartBit = highestSetBit - MANTISSA_BITS;
      exp = mantissaStartBit + 1;
      mantissa = (size >> mantissaStartBit) & MANTISSA_MASK;

      u32 lowBitsMask = (1 << mantissaStartBit) - 1;

      // Round up!
      if ((size & lowBitsMask) != 0) {
        mantissa++;
      }
    }

    // + allows mantissa->exp overflow for round up
    return (exp << MANTISSA_BITS) + mantissa;
  }

  static inline u32 sizeToBinRoundDown(u32 size)
  {
    // Modeled after asfloat with a small float with mantissa bits = 3
    //
    u32 exp = 0;
    u32 mantissa = 0;

    if (size < MANTISSA_VALUE) {
      // Denorm: 0..(MANTISSA_VALUE-1)
      mantissa = size;
    } else {
      // Normalized: Hidden high bit always 1. Not stored. Just like float.
      u32 leadingZeros = lzcntNonZero(size);
      u32 highestSetBit = 31 - leadingZeros;

      u32 mantissaStartBit = highestSetBit - MANTISSA_BITS;
      exp = mantissaStartBit + 1;
      mantissa = (size >> mantissaStartBit) & MANTISSA_MASK;
    }

    return (exp << MANTISSA_BITS) | mantissa;
  }

  static inline u32 sizeToBin(u32 size)
  {
    u32 exponent = size >> MANTISSA_BITS;
    u32 mantissa = size & MANTISSA_MASK;
    if (exponent == 0)
    {
      // Denorms
      return mantissa;
    }
    else
    {
      return (mantissa | MANTISSA_VALUE) << (exponent - 1);
    }
  }
};

// Utility functions
u32 findLowestSetBitAfter(u32 bit_mask, u32 startBitIndex)
{
  u32 maskBeforeStartIndex = (1 << startBitIndex) - 1;
  u32 maskAfterStartIndex = ~maskBeforeStartIndex;

  u32 bitsAfter = bit_mask & maskAfterStartIndex;
  if (bitsAfter == 0) {
    return AllocOOM;
  }

  return tzcntNonZero(bitsAfter);
}

}

OffsetAllocator::OffsetAllocator(u32 size)
  : size_(size),
    nodes_((Node *)malloc(sizeof(Node) * MAX_ALLOCS)),
    free_nodes_((u32 *)malloc(sizeof(u32) * MAX_ALLOCS))
{
  reset();
}

OffsetAllocator::OffsetAllocator(OffsetAllocator &&o)
  : size_(o.size_),
    free_storage_(o.free_storage_),
    used_bins_top_(o.used_bins_top_),
    nodes_(o.nodes_),
    free_nodes_(o.free_nodes_),
    free_offset_(o.free_offset_)
{
  memcpy(used_bins_, o.used_bins_, sizeof(u8) * NUM_TOP_BINS);
  memcpy(bin_indices_, o.bin_indices_, sizeof(u32) * NUM_LEAF_BINS);

  o.nodes_ = nullptr;
  o.free_nodes_ = nullptr;
  o.free_offset_ = 0;
  o.used_bins_top_ = 0;
}

void OffsetAllocator::reset()
{
  free_storage_ = 0;
  used_bins_top_ = 0;
  free_offset_ = MAX_ALLOCS - 1;

  for (u32 i = 0 ; i < NUM_TOP_BINS; i++) {
    used_bins_[i] = 0;
  }

  for (u32 i = 0 ; i < NUM_LEAF_BINS; i++) {
    bin_indices_[i] = Node::UNUSED;
  }

  // Freelist is a stack. Nodes in inverse order so that [0] pops first.
  for (u32 i = 0; i < MAX_ALLOCS; i++) {
    free_nodes_[i] = MAX_ALLOCS - i - 1;
  }

  // Start state: Whole storage as one big node
  // Algorithm will split remainders and push them back as smaller nodes
  insertNodeIntoBin(size_, 0);
}

OffsetAllocator::~OffsetAllocator()
{        
  free(nodes_);
  free(free_nodes_);
}

OffsetAllocation OffsetAllocator::alloc(u32 size)
{
  // Out of allocations?
  if (free_offset_ == 0) {
    return {
      .offset = AllocOOM,
      .metadata = AllocOOM,
    };
  }

  // Round up to bin index to ensure that alloc >= bin
  // Gives us min bin index that fits the size
  u32 minBinIndex = Binner<BIN_FLT_MANTISSA_BITS>::sizeToBinRoundUp(size);

  u32 minTopBinIndex = minBinIndex >> BIN_FLT_MANTISSA_BITS;
  u32 minLeafBinIndex = minBinIndex & LEAF_BINS_INDEX_MASK;

  u32 top_bin_idx = minTopBinIndex;
  u32 leaf_bin_idx = AllocOOM;

  // If top bin exists, scan its leaf bin. This can fail (NO_SPACE).
  if (used_bins_top_ & (1 << top_bin_idx))
  {
    leaf_bin_idx = findLowestSetBitAfter(
      used_bins_[top_bin_idx], minLeafBinIndex);
  }

  // If we didn't find space in top bin, we search top bin from +1
  if (leaf_bin_idx == AllocOOM)
  {
    top_bin_idx = findLowestSetBitAfter(
      used_bins_top_, minTopBinIndex + 1);

    // Out of space?
    if (top_bin_idx == AllocOOM) {
      return {
        .offset = AllocOOM,
        .metadata = AllocOOM,
      };
    }

    // All leaf bins here fit the alloc, since the top bin was rounded up. Start leaf search from bit 0.
    // NOTE: This search can't fail since at least one leaf bit was set because the top bit was set.
    leaf_bin_idx = tzcntNonZero(used_bins_[top_bin_idx]);
  }

  u32 bin_idx = (top_bin_idx << BIN_FLT_MANTISSA_BITS) | leaf_bin_idx;

  // Pop the top node of the bin. Bin top = node.next.
  u32 node_idx = bin_indices_[bin_idx];
  Node &node = nodes_[node_idx];
  u32 nodeTotalSize = node.dataSize;
  node.dataSize = size;
  node.used = true;
  bin_indices_[bin_idx] = node.binListNext;
  if (node.binListNext != Node::UNUSED) nodes_[node.binListNext].binListPrev = Node::UNUSED;
  free_storage_ -= nodeTotalSize;
#ifdef DEBUG_VERBOSE
  printf("Free storage: %u (-%u) (allocate)\n", free_storage_, nodeTotalSize);
#endif

  // Bin empty?
  if (bin_indices_[bin_idx] == Node::UNUSED)
  {
    // Remove a leaf bin mask bit
    used_bins_[top_bin_idx] &= ~(1 << leaf_bin_idx);

    // All leaf bins empty?
    if (used_bins_[top_bin_idx] == 0)
    {
      // Remove a top bin mask bit
      used_bins_top_ &= ~(1 << top_bin_idx);
    }
  }

  // Push back reminder N elements to a lower bin
  u32 reminderSize = nodeTotalSize - size;
  if (reminderSize > 0) {
    u32 newu32 = insertNodeIntoBin(reminderSize, node.dataOffset + size);

    // Link nodes next to each other so that we can merge them later if both
    // are free.
    // And update the old next neighbor to point to the new node
    // (in middle)
    if (node.neighborNext != Node::UNUSED) {
      nodes_[node.neighborNext].neighborPrev = newu32;
    }
    nodes_[newu32].neighborPrev = node_idx;
    nodes_[newu32].neighborNext = node.neighborNext;
    node.neighborNext = newu32;
  }

  return {
    .offset = node.dataOffset,
    .metadata = node_idx,
  };
}

void OffsetAllocator::dealloc(OffsetAllocation allocation)
{
  assert(allocation.metadata != AllocOOM);

  u32 node_idx = allocation.metadata;
  Node &node = nodes_[node_idx];

  // Double delete check
  assert(node.used == true);

  // Merge with neighbors...
  u32 offset = node.dataOffset;
  u32 size = node.dataSize;

  if ((node.neighborPrev != Node::UNUSED) &&
      (nodes_[node.neighborPrev].used == false)) {
    // Previous (contiguous) free node: Change offset to previous node offset.
    //  Sum sizes
    Node &prev_node = nodes_[node.neighborPrev];
    offset = prev_node.dataOffset;
    size += prev_node.dataSize;

    // Remove node from the bin linked list and put it in the freelist
    removeNodeFromBin(node.neighborPrev);

    assert(prev_node.neighborNext == node_idx);
    node.neighborPrev = prev_node.neighborPrev;
  }

  if ((node.neighborNext != Node::UNUSED) &&
      (nodes_[node.neighborNext].used == false)) {
    // Next (contiguous) free node: Offset remains the same. Sum sizes.
    Node &nextNode = nodes_[node.neighborNext];
    size += nextNode.dataSize;

    // Remove node from the bin linked list and put it in the freelist
    removeNodeFromBin(node.neighborNext);

    assert(nextNode.neighborPrev == node_idx);
    node.neighborNext = nextNode.neighborNext;
  }

  u32 neighborNext = node.neighborNext;
  u32 neighborPrev = node.neighborPrev;

  // Insert the removed node to freelist
#ifdef DEBUG_VERBOSE
  printf("Putting node %u into freelist[%u] (free)\n",
         node_idx, free_offset_ + 1);
#endif
  free_nodes_[++free_offset_] = node_idx;

  // Insert the (combined) free node to bin
  u32 combinedu32 = insertNodeIntoBin(size, offset);

  // Connect neighbors with the new combined node
  if (neighborNext != Node::UNUSED)
  {
    nodes_[combinedu32].neighborNext = neighborNext;
    nodes_[neighborNext].neighborPrev = combinedu32;
  }
  if (neighborPrev != Node::UNUSED)
  {
    nodes_[combinedu32].neighborPrev = neighborPrev;
    nodes_[neighborPrev].neighborNext = combinedu32;
  }
}

u32 OffsetAllocator::insertNodeIntoBin(u32 size, u32 dataOffset)
{
  // Round down to bin index to ensure that bin >= alloc
  u32 bin_idx = Binner<BIN_FLT_MANTISSA_BITS>::sizeToBinRoundDown(size);

  u32 top_bin_idx = bin_idx >> BIN_FLT_MANTISSA_BITS;
  u32 leaf_bin_idx = bin_idx & LEAF_BINS_INDEX_MASK;

  // Bin was empty before?
  if (bin_indices_[bin_idx] == Node::UNUSED) {
    // Set bin mask bits
    used_bins_[top_bin_idx] |= 1 << leaf_bin_idx;
    used_bins_top_ |= 1 << top_bin_idx;
  }

  // Take a freelist node and insert on top of the bin linked list
  // (next = old top)
  u32 top_node_idx = bin_indices_[bin_idx];
  u32 node_idx = free_nodes_[free_offset_--];
#ifdef DEBUG_VERBOSE
  printf("Getting node %u from freelist[%u]\n", node_idx, free_offset_ + 1);
#endif
  nodes_[node_idx] = {
    .dataOffset = dataOffset,
    .dataSize = size,
    .binListNext = (u16)top_node_idx,
  };

  if (top_node_idx != Node::UNUSED) {
    nodes_[top_node_idx].binListPrev = node_idx;
  }
  bin_indices_[bin_idx] = node_idx;

  free_storage_ += size;
#ifdef DEBUG_VERBOSE
  printf("Free storage: %u (+%u) (insertNodeIntoBin)\n", free_storage_, size);
#endif

  return node_idx;
}

void OffsetAllocator::removeNodeFromBin(u32 node_idx)
{
  Node &node = nodes_[node_idx];

  if (node.binListPrev != Node::UNUSED) {
    // Easy case: We have previous node. Just remove this node
    // from the middle of the list.
    nodes_[node.binListPrev].binListNext = node.binListNext;
    if (node.binListNext != Node::UNUSED) {
      nodes_[node.binListNext].binListPrev = node.binListPrev;
    }
  } else {
    // Hard case: We are the first node in a bin. Find the bin.

    // Round down to bin index to ensure that bin >= alloc
    u32 bin_idx = Binner<BIN_FLT_MANTISSA_BITS>::sizeToBinRoundDown(
      node.dataSize);

    u32 top_bin_idx = bin_idx >> BIN_FLT_MANTISSA_BITS;
    u32 leaf_bin_idx = bin_idx & LEAF_BINS_INDEX_MASK;

    bin_indices_[bin_idx] = node.binListNext;
    if (node.binListNext != Node::UNUSED) {
      nodes_[node.binListNext].binListPrev = Node::UNUSED;
    }

    // Bin empty?
    if (bin_indices_[bin_idx] == Node::UNUSED) {
      // Remove a leaf bin mask bit
      used_bins_[top_bin_idx] &= ~(1 << leaf_bin_idx);

      // All leaf bins empty?
      if (used_bins_[top_bin_idx] == 0) {
        // Remove a top bin mask bit
        used_bins_top_ &= ~(1 << top_bin_idx);
      }
    }
  }

  // Insert the node to freelist
#ifdef DEBUG_VERBOSE
  printf("Putting node %u into freelist[%u] (removeNodeFromBin)\n",
         node_idx, free_offset_ + 1);
#endif
  free_nodes_[++free_offset_] = node_idx;

  free_storage_ -= node.dataSize;
#ifdef DEBUG_VERBOSE
  printf("Free storage: %u (-%u) (removeNodeFromBin)\n",
         free_storage_, node.dataSize);
#endif
}

u32 OffsetAllocator::allocationSize(OffsetAllocation allocation)
{
  if (allocation.metadata == AllocOOM) {
    return 0;
  }

  return nodes_[allocation.metadata].dataSize;
}

OffsetAllocator::StorageReport OffsetAllocator::storageReport()
{
  u32 largest_free_region = 0;
  u32 free_storage = 0;

  // Out of allocations? -> Zero free space
  if (free_offset_ > 0) {
    free_storage = free_storage_;
    if (used_bins_top_) {
      u32 top_bin_idx = 31 - lzcntNonZero(used_bins_top_);
      u32 leaf_bin_idx = 31 - lzcntNonZero(used_bins_[top_bin_idx]);
      largest_free_region = (
        (top_bin_idx << BIN_FLT_MANTISSA_BITS) | leaf_bin_idx);
      assert(free_storage >= largest_free_region);
    }
  }

  return {
    .totalFreeSpace = free_storage,
    .largestFreeRegion = largest_free_region,
  };
}

OffsetAllocator::StorageReportFull OffsetAllocator::storageReportFull()
{
  StorageReportFull report;
  for (u32 i = 0; i < NUM_LEAF_BINS; i++) {
    u32 count = 0;
    u32 node_idx = bin_indices_[i];

    while (node_idx != Node::UNUSED) {
      node_idx = nodes_[node_idx].binListNext;
      count++;
    }

    report.freeRegions[i] = {
      .size = Binner<BIN_FLT_MANTISSA_BITS>::sizeToBin(i),
      .count = count,
    };
  }
  return report;
}

TableAllocator::TableAllocator()
{
  free_top_bins_ = 0;
  for (u32 i = 0; i < NUM_TOP_BINS; i++) {
    free_leaf_bins_[i] = 0;
  }

  for (u32 i = 0; i < NUM_LEAF_BINS; i++) {
    bin_free_heads_[i] = SENTINEL;
  }

  addFreeNode(0, MAX_NUM_ELEMS);

  assert(bin_free_heads_[NUM_LEAF_BINS - 1] == 0);
  assert(nodes_[0].freeListNext == SENTINEL);
}

u32 TableAllocator::alloc(u32 size)
{
  assert(size > 0 && size <= 1024);

  // Out of allocations?
  if (free_top_bins_ == 0) {
    return AllocOOM;
  }

  u32 free_top_bitmask = (u32)free_top_bins_;

  u32 top_bin_idx;
  u32 leaf_bin_idx;
  {
    // Round up to bin index to ensure that alloc >= bin
    // Gives us min bin index that fits the size
    u32 min_bin_idx = Binner<BIN_FLT_MANTISSA_BITS>::sizeToBinRoundUp(size);
    min_bin_idx = std::min(min_bin_idx, NUM_LEAF_BINS - 1);

    u32 min_top_bin_idx = min_bin_idx >> BIN_FLT_MANTISSA_BITS;
    u32 min_leaf_bin_idx = min_bin_idx & LEAF_BINS_IDX_MASK;

    top_bin_idx = min_top_bin_idx;
    leaf_bin_idx = AllocOOM;

    // If top bin exists, scan its leaf bin. This can fail, because the
    // properly sized leaves may actually be empty (AllocOOM).
    if (free_top_bitmask & (1 << top_bin_idx)) {
      leaf_bin_idx = findLowestSetBitAfter(
        free_leaf_bins_[top_bin_idx], min_leaf_bin_idx);
    }

    // If we didn't find space in top bin, we search top bin from +1
    if (leaf_bin_idx == AllocOOM) {
      top_bin_idx = findLowestSetBitAfter(
        free_top_bitmask, min_top_bin_idx + 1);

      // Out of space?
      if (top_bin_idx == AllocOOM) {
        return AllocOOM;
      }

      // All leaf bins here fit the alloc, since the top bin was rounded up.
      // Start leaf search from bit 0.
      // NOTE: This search can't fail since at least one leaf bit was set
      // because the top bit was set.
      leaf_bin_idx = tzcntNonZero(free_leaf_bins_[top_bin_idx]);
    }
  }

  u32 bin_idx = (top_bin_idx << BIN_FLT_MANTISSA_BITS) | leaf_bin_idx;
  assert(bin_idx < NUM_LEAF_BINS);

  // Pop the top node of the bin.
  u32 node_idx = (u32)bin_free_heads_[bin_idx];

  Node &node = nodes_[node_idx];
  u32 node_orig_size = (u32)node.size;

  u16 new_bin_head = node.freeListNext;
  bin_free_heads_[bin_idx] = new_bin_head;

  if (new_bin_head != SENTINEL) { // Bin still has free nodes
    nodes_[new_bin_head].freeListPrev = SENTINEL;
  } else { // Bin empty?
    // Remove a leaf bin mask bit
    free_leaf_bins_[top_bin_idx] &= ~(1 << leaf_bin_idx);

    // All leaf bins empty?
    if (free_leaf_bins_[top_bin_idx] == 0) {
      // Remove a top bin mask bit
      free_top_bitmask &= ~(1 << top_bin_idx);
      free_top_bins_ = (u8)free_top_bitmask;
    }
  }

  // Mark node as allocated
  {
    u32 free_bit_idx = node_idx / 32;
    u32 free_bit_offset = node_idx % 32;
    node_free_states_[free_bit_idx] |= (1 << free_bit_offset);
  }

  // Mark this new region tail as allocated
  if (size > 1) {
    u32 tail_idx = node_idx + size - 1;
    u32 free_bit_idx = tail_idx / 32;
    u32 free_bit_offset = tail_idx % 32;
    node_free_states_[free_bit_idx] |= (1 << free_bit_offset);
  }

  // Push back remainder N elements to a lower bin
  u32 node_remainder = node_orig_size - size;
  if (node_remainder > 0) {
    u32 new_node_idx = node_idx + size;
    addFreeNode(new_node_idx, node_remainder);
  }

  return node_idx;
}

void TableAllocator::dealloc(u32 offset, u32 size)
{
  auto isNodeFree = [this](u32 node_idx) {
    // Note that this check catches accesses off both ends of the array
    // due to the usage of u32.
    if (node_idx >= MAX_NUM_ELEMS) {
      return false;
    }

    u32 free_bit_idx = node_idx / 32;
    u32 free_bit_offset = node_idx % 32;

    return (node_free_states_[free_bit_idx] & (1 << free_bit_offset)) == 0;
  };

  u32 node_idx = offset;
  // Double delete check
  assert(!isNodeFree(node_idx));

  // Merge with neighbors...
  if (isNodeFree(node_idx - 1)) {
    Node &prev_neighbor_tail = nodes_[node_idx - 1];
    u32 prev_neighbor = node_idx - prev_neighbor_tail.size;

    removeFreeNodeForMerge(prev_neighbor);

    size += prev_neighbor_tail.size;
    node_idx = prev_neighbor;
  }

  if (isNodeFree(node_idx + size)) {
    u32 next_neighbor = node_idx + size;

    removeFreeNodeForMerge(next_neighbor);

    size += nodes_[next_neighbor].size;
  }

  addFreeNode(node_idx, size);
}

void TableAllocator::addFreeNode(u32 node_idx, u32 size)
{
  Node &node = nodes_[node_idx];

  // Mark the head of this node's region as free
  {
    u32 free_bit_idx = node_idx / 32;
    u32 free_bit_offset = node_idx % 32;
    node_free_states_[free_bit_idx] &= ~(1 << free_bit_offset);

    node.size = size;
  }

  // Mark the tail of this node's region as free
  if (size > 1) {
    u32 tail_idx = node_idx + size - 1;
    u32 free_bit_idx = tail_idx / 32;
    u32 free_bit_offset = tail_idx % 32;
    node_free_states_[free_bit_idx] &= ~(1 << free_bit_offset);

    nodes_[tail_idx].size = size;
  }

  // Round down to bin index to ensure that bin >= alloc
  u32 bin_idx = Binner<BIN_FLT_MANTISSA_BITS>::sizeToBinRoundDown(size);
  bin_idx = std::min(bin_idx, NUM_LEAF_BINS - 1);

  u32 top_bin_idx = bin_idx >> BIN_FLT_MANTISSA_BITS;
  u32 leaf_bin_idx = bin_idx & LEAF_BINS_IDX_MASK;

  // Take a freelist node and insert on top of the bin linked list
  u16 old_bin_head = bin_free_heads_[bin_idx];

  // Bin was empty before?
  if (old_bin_head == SENTINEL) {
    // Set bin mask bits
    free_leaf_bins_[top_bin_idx] |= 1 << leaf_bin_idx;
    free_top_bins_ |= 1 << top_bin_idx;
  }

#ifdef DEBUG_VERBOSE
  printf("Getting node %u from freelist[%u]\n",
         node_idx, free_offset_ + 1);
#endif

  // (next = old free head)
  node.freeListPrev = SENTINEL;
  node.freeListNext = old_bin_head;

  if (old_bin_head != SENTINEL) {
    nodes_[old_bin_head].freeListPrev = node_idx;
  }

  bin_free_heads_[bin_idx] = node_idx;
}

void TableAllocator::removeFreeNodeForMerge(u32 node_idx)
{
  Node &node = nodes_[node_idx];

  if (node.freeListPrev != SENTINEL) {
    // Easy case: We have previous node. Just remove this node
    // from the middle of the list.
    nodes_[node.freeListPrev].freeListNext = node.freeListNext;
    if (node.freeListNext != SENTINEL) {
      nodes_[node.freeListNext].freeListPrev = node.freeListPrev;
    }
  } else {
    // Hard case: We are the first node in a bin. Find the bin.

    // Round down to bin index to ensure that bin >= alloc
    u32 bin_idx = Binner<BIN_FLT_MANTISSA_BITS>::sizeToBinRoundDown(node.size);
    bin_idx = std::min(bin_idx, NUM_LEAF_BINS - 1);

    u32 top_bin_idx = bin_idx >> BIN_FLT_MANTISSA_BITS;
    u32 leaf_bin_idx = bin_idx & LEAF_BINS_IDX_MASK;

    u16 new_bin_head = node.freeListNext;
    bin_free_heads_[bin_idx] = new_bin_head;

    if (new_bin_head != SENTINEL) { // Bin still has free nodes
      nodes_[new_bin_head].freeListPrev = SENTINEL;
    } else { // Bin empty?
      // Remove a leaf bin mask bit
      free_leaf_bins_[top_bin_idx] &= ~(1 << leaf_bin_idx);

      // All leaf bins empty?
      if (free_leaf_bins_[top_bin_idx] == 0) {
        // Remove a top bin mask bit
        u32 free_top_bitmask = (u32)free_top_bins_;
        free_top_bitmask &= ~(1 << top_bin_idx);
        free_top_bins_ = (u8)free_top_bitmask;
      }
    }
  }
}

}
