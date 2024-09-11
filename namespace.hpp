#pragma once

#include <madrona/components.hpp>
#include <madrona/math.hpp>

namespace gas {

using u64 = uint64_t;
using i64 = int64_t;
using u32 = uint32_t;
using i32 = int32_t;
using u16 = uint16_t;
using i16 = int16_t;
using u8 = uint8_t;
using i8 = int8_t;
using f32 = float;

using namespace madrona;

// Include several madrona types into the simulator namespace for convenience
using base::Position;
using base::Rotation;
using base::Scale;
using base::ObjectInstance;
using base::ObjectID;
using math::Vector2;
using math::Vector3;
using math::Vector4;
using math::Quat;
using math::Diag3x3;
using math::AABB;
using math::AABB2D;

}
