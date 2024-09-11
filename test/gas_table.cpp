#include "mem.hpp"

#include <madrona/utils.hpp>

#include <gtest/gtest.h>

using namespace gas;

TEST(gas, TableAllocatorBasic)
{
  TableAllocator alloc;

  u32 alc1 = alloc.alloc(2);
  EXPECT_EQ(alc1, 0);
  u32 alc2 = alloc.alloc(8);
  EXPECT_EQ(alc2, 2);
  u32 alc3 = alloc.alloc(15);
  EXPECT_EQ(alc3, 10);
  u32 alc4 = alloc.alloc(1020);
  EXPECT_EQ(alc4, 25);
  alloc.dealloc(alc3, 15);

  u32 alc5 = alloc.alloc(200);
  EXPECT_EQ(alc5, 1045);
  u32 alc6 = alloc.alloc(15);
  EXPECT_EQ(alc6, 10);

  alloc.dealloc(alc1, 2);
  alloc.dealloc(alc6, 15);
  alloc.dealloc(alc4, 1020);
  alloc.dealloc(alc5, 200);
  alloc.dealloc(alc2, 8);

  const u32 num_chunks = 65535 / 1024;
  u32 allocs[num_chunks + 1];

  for (u32 i = 0; i < num_chunks; i++) {
    allocs[i] = alloc.alloc(1024);
    EXPECT_EQ(allocs[i], i * 1024);
  }

  allocs[num_chunks] = alloc.alloc(65535 - 1024 * num_chunks);
    EXPECT_EQ(allocs[num_chunks], num_chunks * 1024);

  u32 alc_final = alloc.alloc(1);
  EXPECT_EQ(alc_final, 65535);

  u32 alc_fail = alloc.alloc(1024);
  EXPECT_EQ(alc_fail, AllocOOM);

  for (u32 i = 0; i < num_chunks; i++) {
    alloc.dealloc(allocs[i], 1024);
  }
  alloc.dealloc(allocs[num_chunks], 65535 - 1024 * num_chunks);
  alloc.dealloc(alc_final, 1);

  for (u32 i = 0; i < 65536 / 1024; i++) {
    u32 alc = alloc.alloc(1024);
    EXPECT_EQ(alc, i * 1024);
  }

  alc_fail = alloc.alloc(1);
  EXPECT_EQ(alc_fail, AllocOOM);
  
}
