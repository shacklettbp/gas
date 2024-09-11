#include "uuid.hpp"
#include "backend_common.hpp"

#include <madrona/rand.hpp>
#include <madrona/dyn_array.hpp>

#include <gtest/gtest.h>

using namespace gas;

TEST(UUID, Parse)
{
  constexpr UUID test_uuid = "6359f494-ec9d-495a-82e3-6d1a8623af79"_uuid;
  EXPECT_EQ(test_uuid.id[0], 0x82e36d1a8623af79);
  EXPECT_EQ(test_uuid.id[1], 0x6359f494ec9d495a);

  constexpr UUID bad_uuid = "-6359f494-ec9d-495a-82e3-6d1a8623af79"_uuid;
  EXPECT_EQ(bad_uuid.id[0], 0);
  EXPECT_EQ(bad_uuid.id[1], 0);
}

TEST(UUID, StrGen)
{
  constexpr UUID gen1 = "a"_to_uuid;
  constexpr UUID gen2 = "b"_to_uuid;
  constexpr UUID gen3 = "abcdefgh"_to_uuid;

  EXPECT_NE(gen1.id[0], gen2.id[0]);
  EXPECT_NE(gen1.id[1], gen2.id[1]);

  EXPECT_NE(gen1.id[0], gen3.id[0]);
  EXPECT_NE(gen1.id[1], gen3.id[1]);

  EXPECT_NE(gen2.id[0], gen3.id[0]);
  EXPECT_NE(gen2.id[1], gen3.id[1]);
}

TEST(ResourceUUIDMap, Collisions)
{
  ResourceUUIDMap map;

  constexpr u16 num_trials = 16;
  constexpr u16 num_uuids = 65535;

  UUID uuids[num_uuids];

  for (i32 trial = 0; trial < num_trials; trial++) {
    RNG rng(trial);

    for (u16 i = 0; i < num_uuids; i++) {
      UUID uuid;
      {
        RandKey k1 = rng.randKey();
        RandKey k2 = rng.randKey();
        uuid[0] = rand::bits64(k1);
        uuid[1] = rand::bits64(k2);
      }

      uuids[i] = uuid;
      map.insert(uuid, i);
    }

    for (u16 i = 0; i < num_uuids; i++) {
      i32 lookup = map.lookup(uuids[i]);
      EXPECT_NE(lookup, ResourceUUIDMap::NOT_FOUND);
      EXPECT_EQ((u16)lookup, i);
    }

    for (u16 i = 0; i < num_uuids; i++) {
      map.remove(uuids[i]);
    }
  }
}
