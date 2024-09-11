#include "gen.hpp"
#include "utils.hpp"
#include "sim.hpp"
#include "nav_sys.hpp"

#include <algorithm>

namespace gas {

static inline MagicElement sampleMagicElement(RNG &rng)
{
    return (MagicElement)rng.sampleI32(1, (i32)MagicElement::NumElements);
}


static inline Entity makeDragon(Engine &ctx,
                                Vector3 pos,
                                Quat rot,
                                MagicElement element,
                                RandKey seed,
                                i32 room_idx)
{
    Entity dragon = ctx.makeRenderableEntity<DragonEntity>();
    ctx.get<Position>(dragon) = pos;
    ctx.get<Rotation>(dragon) = rot;
    ctx.get<Scale>(dragon) = Diag3x3::id();

    i32 obj_idx;
    switch (element) {
    case MagicElement::Fire: {
        obj_idx = (i32)SimObjectID::FireDragon;
    } break;
    case MagicElement::Ice: {
        obj_idx = (i32)SimObjectID::IceDragon;
    } break;
    case MagicElement::Bolt: {
        obj_idx = (i32)SimObjectID::BoltDragon;
    } break;
    default: MADRONA_UNREACHABLE();
    }
    ctx.get<ObjectID>(dragon).idx = obj_idx;
    ctx.get<ActorType>(dragon) = ActorType::Dragon;

    ctx.get<MagicElement>(dragon) = element;

    ctx.get<BodyID>(dragon) = PhysicsSystem::addZCapsuleBody(
        ctx, dragon, (u32)ActorBodyFlags::Enemy,
        consts::dragonRadius, consts::dragonRadius);
    ctx.get<DragonRNG>(dragon) = DragonRNG(seed);

    ctx.get<HP>(dragon).hp = consts::dragonHP;

    ctx.get<DragonAIState>(dragon) = {
        .spawnRoom = room_idx,
        .inCombatTimer = 0,
        .goalPosition = pos,
        .curTarget = Entity::none(),
    };

    ctx.get<DamageReceived>(dragon) = {
        .agentDmg = {},
        .dragonDmg = {},
    };

    ctx.get<PreDestroyHandler>(dragon) = {
        .handlerArchetypeID = (i32)TypeTracker::typeID<PreDestroyDragon>(),
    };

    return dragon;
}

static inline Entity makeWizard(Engine &ctx,
                                Vector3 pos,
                                Quat rot,
                                RandKey seed)
{
    Entity wiz = ctx.makeRenderableEntity<AIWizard>();

    ctx.get<Position>(wiz) = pos;
    ctx.get<Rotation>(wiz) = rot;
    ctx.get<Scale>(wiz) = Diag3x3::id();
    ctx.get<ObjectID>(wiz).idx = (i32)SimObjectID::Wizard;
    ctx.get<ActorType>(wiz) = ActorType::Wizard;

    ctx.get<BodyID>(wiz) = PhysicsSystem::addZCapsuleBody(
        ctx, wiz, (u32)ActorBodyFlags::Agent,
        consts::agentRadius, consts::agentHeight);
    ctx.get<MagicElement>(wiz) = MagicElement::Neutral;

    ctx.get<HP>(wiz).hp = consts::wizardHP;
    ctx.get<MP>(wiz).mp = consts::wizardMP;
    ctx.get<BattleStats>(wiz) = {
        .maxMP = consts::wizardMP,
    };
    ctx.get<WizAttackState>(wiz) = {
        .isStaffActive = false,
    };

    ctx.get<WizRNG>(wiz) = WizRNG(seed);

    ctx.get<DamageReceived>(wiz) = {
        .agentDmg = {},
        .dragonDmg = {},
    };

    ctx.get<HeldStaff>(wiz).staff = Entity::none();
    ctx.get<HeldItem>(wiz).item = Entity::none();

    ctx.get<PreDestroyHandler>(wiz) = {
        .handlerArchetypeID = (i32)TypeTracker::typeID<PreDestroyWizard>(),
    };

    return wiz;
}

static inline Entity makeStaff(Engine &ctx,
                               MagicElement element)
{
    Entity staff = ctx.makeRenderableEntity<Staff>();
    ctx.get<MagicElement>(staff) = element;
    ctx.get<Scale>(staff) = Diag3x3::id();

    i32 obj_idx;
    switch (element) {
    case MagicElement::Fire: {
        obj_idx = (i32)SimObjectID::FireStaff;
    } break;
    case MagicElement::Ice: {
        obj_idx = (i32)SimObjectID::IceStaff;
    } break;
    case MagicElement::Bolt: {
        obj_idx = (i32)SimObjectID::BoltStaff;
    } break;
    default: MADRONA_UNREACHABLE();
    }
    ctx.get<ObjectID>(staff).idx = obj_idx;

    return staff;
}

static inline Quat randomZRotation(RNG &rng)
{
    return Quat::angleAxis(
        rng.sampleUniform() * 2.f * math::pi, worldUp);
}

static inline void selectSpawnRooms(RNG &rng,
                                    AABB2D *room_bounds,
                                    i32 *dragon_rooms,
                                    i32 *wiz_rooms)
{
    constexpr f32 min_dragon_room_len = 2.5f * (
        consts::dragonRadius + consts::safeMoveBuffer + consts::halfWallWidth);
    constexpr f32 min_dragon_room_area =
        5.f * math::pi * consts::dragonRadius * consts::dragonRadius;

    i32 num_dragon_rooms = 0;
    i32 num_wiz_rooms = 0;
    for (i32 y = 0; y < consts::numRoomGridRows; y++) {
        for (i32 x = 0; x < consts::numRoomGridCols; x++) {
            i32 room_idx = y * consts::numRoomGridCols + x;

            AABB2D cur = room_bounds[room_idx];

            f32 dx = cur.pMax.x - cur.pMin.x;
            f32 dy = cur.pMax.y - cur.pMin.y;
            f32 area = dx * dy;

            if (dx >= min_dragon_room_len && dy >= min_dragon_room_len &&
                    area >= min_dragon_room_area && x > 1) {
                dragon_rooms[num_dragon_rooms++] = room_idx;
            } else {
                wiz_rooms[num_wiz_rooms++] = room_idx;
            }
        }
    }

    assert(num_dragon_rooms >= consts::numDragons);

    for (i32 i = 0; i < num_dragon_rooms - 1; i++) {
        i32 j = rng.sampleI32(i, num_dragon_rooms);
        std::swap(dragon_rooms[i], dragon_rooms[j]);
    }

    for (i32 i = 0; i < num_wiz_rooms - 1; i++) {
        i32 j = rng.sampleI32(i, num_wiz_rooms);
        std::swap(wiz_rooms[i], wiz_rooms[j]);
    }

    const i32 num_extra_dragon_rooms = num_dragon_rooms - consts::numDragons;
    for (i32 i = 0; i < num_extra_dragon_rooms; i++) {
        wiz_rooms[num_wiz_rooms + i] = dragon_rooms[consts::numDragons + i];
    }
}

static inline void spawnDragons(Engine &ctx,
                                RNG &rng,
                                AABB2D *room_bounds,
                                i32 *dragon_rooms,
                                MagicElement *dragon_elements_out)
{
    for (i32 i = 0; i < consts::numDragons; i++) {
        MagicElement element = sampleMagicElement(rng);
        dragon_elements_out[i] = element;

        i32 room_idx = dragon_rooms[i];
        AABB2D spawn_aabb = room_bounds[room_idx];

        Vector3 pos = pointInAABB2D(rng, spawn_aabb,
            consts::dragonRadius + consts::safeMoveBuffer +
            consts::halfWallWidth);
        Quat rot = randomZRotation(rng);

        Entity dragon = makeDragon(
            ctx, pos, rot, element, rng.randKey(), room_idx);

        ctx.get<DragonID>(dragon).idx = i;
    }
}


static inline void spawnWizards(Engine &ctx,
                                RNG &rng,
                                AABB2D *room_bounds,
                                i32 *wiz_rooms,
                                MagicElement *dragon_elements)
{
    i32 wiz_idx = 0;
    ctx.iterateQuery(ctx.singleton<AgentInterfaceQuery>().query, [
        &ctx, &rng, &wiz_idx, room_bounds, wiz_rooms, dragon_elements
    ](Entity iface_e, AgentEntityRef &entity_ref)
    {
        if (entity_ref.e != Entity::none()) {
            return;
        }

        MagicElement element = sampleMagicElement(rng);

        Entity wiz;
        {
            AABB2D spawn_bounds = room_bounds[wiz_rooms[wiz_idx]];

            Vector3 wiz_pos = pointInAABB2D(rng, spawn_bounds,
                consts::agentRadius + consts::safeMoveBuffer + consts::halfWallWidth);
            Quat wiz_rot = randomZRotation(rng);

            wiz = makeWizard(ctx, wiz_pos, wiz_rot, rng.randKey());
            ctx.get<WizID>(wiz).idx = wiz_idx;
            ctx.get<InterfaceRef>(wiz).loc = ctx.loc(iface_e);
        }
        entity_ref.e = wiz;

        Entity staff = makeStaff(ctx, element);

        holdStaff(ctx, wiz, staff);

        {
            MagicElement desired_treasure_element =
                dragon_elements[rng.sampleI32(0, consts::numDragons)];
            
            ctx.get<AgentGoalControl>(iface_e) = {
                .treasureElementID = (i32)desired_treasure_element,
                .acquireID = -1,
                .killID = -1,
            };

            ctx.get<AgentRewardEvents>(iface_e) = {};
            ctx.get<AgentRewardState>(iface_e) = {};
        }

        wiz_idx += 1;
        
    });
}

static inline void makeGround(Engine &ctx)
{
    Entity ground = ctx.makeRenderableEntity<StaticGeo>();
    ctx.get<Position>(ground) = Vector3 { 0, 0, -0.5f };
    ctx.get<Rotation>(ground) = Quat::id();
    ctx.get<Scale>(ground) = Diag3x3 {
        .d0 = 2.f * consts::worldSizeX,
        .d1 = 2.f * consts::worldSizeY,
        .d2 = 1.f,
    };
    ctx.get<ObjectID>(ground).idx = (i32)SimObjectID::Ground;
    ctx.get<ActorType>(ground) = ActorType::Ground;

    ctx.get<BodyID>(ground) = PhysicsSystem::addAABBBody(
        ctx, ground, (u32)ActorBodyFlags::Static,
        { consts::worldSizeX, consts::worldSizeY, 1.f});
}

static inline Entity makeWall(Engine &ctx,
                              NavmeshVoxelData &nav_voxels,
                              Vector2 bottom_left,
                              f32 x_len,
                              f32 y_len)
{
    Entity wall = ctx.makeRenderableEntity<StaticGeo>();
    Vector3 wall_size { x_len + 1.f, y_len + 1.f, consts::wallHeight };

    ctx.get<Position>(wall) = Vector3 {
        .x = bottom_left.x + 0.5f * wall_size.x - 0.5f,
        .y = bottom_left.y + 0.5f * wall_size.y - 0.5f,
        .z = 0.5f * wall_size.z,
    };
    ctx.get<Rotation>(wall) = Quat::id();

    ctx.get<Scale>(wall) = Diag3x3::fromVec(wall_size);
    ctx.get<ObjectID>(wall).idx = (i32)SimObjectID::Wall;
    ctx.get<ActorType>(wall) = ActorType::Wall;

    ctx.get<BodyID>(wall) = PhysicsSystem::addAABBBody(
        ctx, wall, (u32)ActorBodyFlags::Static, wall_size);

    {
        Vector2 wall_nav_offset = bottom_left - nav_voxels.gridOrigin.xy();

        i32 wall_start_x_cell = (i32)floorf(
            wall_nav_offset.x / nav_voxels.cellSize + 0.5f);
        i32 wall_start_y_cell = (i32)floorf(
            wall_nav_offset.y / nav_voxels.cellSize + 0.5f);

        i32 wall_end_x_cell =
            wall_start_x_cell + (i32)ceilf(wall_size.x / nav_voxels.cellSize);
        i32 wall_end_y_cell =
            wall_start_y_cell = (i32)ceilf(wall_size.y / nav_voxels.cellSize);

        wall_start_x_cell = std::max(wall_start_x_cell, 0);
        wall_start_y_cell = std::max(wall_start_y_cell, 0);

        wall_end_x_cell = std::min(wall_end_x_cell, nav_voxels.gridNumCellsX - 1);
        wall_end_y_cell = std::min(wall_end_y_cell, nav_voxels.gridNumCellsY - 1);

        for (i32 y_cell = wall_start_y_cell;
             y_cell <= wall_end_y_cell; y_cell++) {
            for (i32 x_cell = wall_start_x_cell;
                 x_cell <= wall_end_x_cell; x_cell++) {
                nav_voxels.markOccupied(x_cell, y_cell, 0);
            }
        }
    }

    return wall;
}

// offgridCellToRect is based on Chris Cox's offgrid.cpp
#if 0
MIT License
Copyright (c) 2023 Chris Cox

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

static inline f32 offgridXYToRandOffset(i32 x, i32 y, RandKey seed)
{
    u32 hash = utils::int32Hash(u32((x ^ seed.a) ^ (y ^ seed.b)));
    return rand::bitsToFloat01(hash);
}

static inline AABB2D offgridCellToRect(
    i32 x, i32 y, RandKey seed, const f32 edge)
{
    // Cell edges range from length of 2 * edge to 2.0 - 2 * edge
    // with avg size 1.0
    auto boxRandom = [seed, edge](i32 x, i32 y)
    {
        f32 range = 1.f - 2.f * edge;
        return edge + range * offgridXYToRandOffset(x, y, seed);
    };

    bool is_even = ((x ^ y) & 1) == 0;

    Vector2 pmin(x, y);
    Vector2 pmax(x + 1, y + 1);

    if (is_even) {
        pmin.x += boxRandom(x, y);
        pmin.y += boxRandom(x + 1, y);

        pmax.x += boxRandom(x + 1, y + 1);
        pmax.y += boxRandom(x, y + 1);
    } else {
        pmin.x += boxRandom(x, y + 1);
        pmin.y += boxRandom(x, y);
        
        pmax.x += boxRandom(x + 1, y);
        pmax.y += boxRandom(x + 1, y + 1);
    }

    return AABB2D {
        .pMin = pmin,
        .pMax = pmax,
    };
}

static inline void makeRooms(Engine &ctx, RandKey base_rnd_key,
                             AABB2D *room_bounds)
{
    RNG rng(base_rnd_key);

    RandKey offgrid_seed = rng.randKey();
    constexpr f32 offgrid_edge_width = 0.15f;
    constexpr f32 offgrid_to_world_scale = 0.85f *
        consts::worldSizeX / consts::numRoomGridCols;
    static_assert(
        2.f * offgrid_to_world_scale * offgrid_edge_width >= consts::doorSize);


    Vector2 offgrid_offset = consts::worldBounds.pMin.xy();

    offgrid_offset -= offgrid_to_world_scale * offgrid_edge_width;

    i32 door_states[consts::numRooms];
    bool dfs_visited[consts::numRooms];
    for (i32 row = 0; row < consts::numRoomGridRows; row++) {
        for (i32 col = 0; col < consts::numRoomGridCols; col++) {
            AABB2D offgrid_aabb = offgridCellToRect(
                col, row, offgrid_seed, offgrid_edge_width);

            AABB2D room_aabb;
            room_aabb.pMin =
                offgrid_aabb.pMin * offgrid_to_world_scale + offgrid_offset;
            room_aabb.pMax =
                offgrid_aabb.pMax * offgrid_to_world_scale + offgrid_offset;

            i32 room_idx = row * consts::numRoomGridCols + col;
            room_bounds[room_idx] = room_aabb;
            door_states[room_idx] = 0;
            dfs_visited[room_idx] = false;
        }
    }

    i32 dfs_stack[consts::numRooms];
    i32 dfs_stack_size = 0;
    dfs_stack[dfs_stack_size++] = 0;
    while (dfs_stack_size > 0) {
        i32 room_idx = dfs_stack[--dfs_stack_size];
        dfs_visited[room_idx] = true;

        i32 room_col = room_idx % consts::numRoomGridCols;
        i32 room_row = room_idx / consts::numRoomGridCols;

        i32 neighbor_candidates[4];
        i32 num_candidates = 0;
        { // Left
            i32 neighbor_col = room_col - 1;
            i32 neighbor_row = room_row;

            if (neighbor_col >= 0) {
                i32 neighbor_idx =
                    neighbor_row * consts::numRoomGridCols + neighbor_col;

                if (!dfs_visited[neighbor_idx] &&
                        (door_states[room_idx] & 1) == 0) {
                    neighbor_candidates[num_candidates++] = neighbor_idx;
                }
            }
        }

        { // Right
            i32 neighbor_col = room_col + 1;
            i32 neighbor_row = room_row;

            if (neighbor_col < consts::numRoomGridCols) {
                i32 neighbor_idx =
                    neighbor_row * consts::numRoomGridCols + neighbor_col;

                if (!dfs_visited[neighbor_idx] &&
                        (door_states[neighbor_idx] & 1) == 0) {
                    neighbor_candidates[num_candidates++] = neighbor_idx;
                }
            }
        }

        { // Up
            i32 neighbor_col = room_col;
            i32 neighbor_row = room_row + 1;

            if (neighbor_row < consts::numRoomGridRows) {
                i32 neighbor_idx =
                    neighbor_row * consts::numRoomGridCols + neighbor_col;

                if (!dfs_visited[neighbor_idx] &&
                        (door_states[room_idx] & 2) == 0) {
                    neighbor_candidates[num_candidates++] = neighbor_idx;
                }
            }
        }

        { // Down
            i32 neighbor_col = room_col;
            i32 neighbor_row = room_row - 1;

            if (neighbor_row >= 0) {
                i32 neighbor_idx =
                    neighbor_row * consts::numRoomGridCols + neighbor_col;

                if (!dfs_visited[neighbor_idx] &&
                        (door_states[neighbor_idx] & 2) == 0) {
                    neighbor_candidates[num_candidates++] = neighbor_idx;
                }
            }
        }

        if (num_candidates == 0) {
            continue;
        }

        i32 candidate_idx = rng.sampleI32(0, num_candidates);

        i32 neighbor_idx = neighbor_candidates[candidate_idx];
        i32 neighbor_col = neighbor_idx % consts::numRoomGridCols;
        i32 neighbor_row = neighbor_idx / consts::numRoomGridCols;

        if (neighbor_col < room_col) {
            door_states[room_idx] |= 1;
        } else if (neighbor_row > room_row) {
            door_states[room_idx] |= 2;
        } else if (neighbor_col > room_col) {
            door_states[neighbor_idx] |= 1;
        } else if (neighbor_row < room_row) {
            door_states[neighbor_idx] |= 2;
        }

        dfs_stack[dfs_stack_size++] = room_idx;
        dfs_stack[dfs_stack_size++] = neighbor_idx;
    }

    NavmeshVoxelData nav_voxels;
    {
        constexpr f32 navmesh_cell_size = 0.1f;

        i32 num_x_cells = ceilf((f32)consts::worldSizeX / navmesh_cell_size);
        i32 num_y_cells = ceilf((f32)consts::worldSizeY / navmesh_cell_size);

        nav_voxels.gridOrigin = consts::worldBounds.pMin;
        nav_voxels.cellSize = navmesh_cell_size;
        nav_voxels.gridNumCellsX = num_x_cells;
        nav_voxels.gridNumCellsY = num_y_cells;
        nav_voxels.gridNumCellsZ = 1;

        i32 num_occupancy_cells = utils::divideRoundUp(
            num_x_cells, NavmeshVoxelData::occupancyBitXDim) *
            num_y_cells;
        nav_voxels.voxelOccupancy =
            (u32 *)ctx.tmpAlloc(sizeof(u32) * (size_t)num_occupancy_cells);

        utils::fillN<u32>(
            nav_voxels.voxelOccupancy, 0, num_occupancy_cells);
    }

    for (i32 room_idx = 0; room_idx < consts::numRooms; room_idx++) {
        i32 room_col = room_idx % consts::numRoomGridCols;
        i32 room_row = room_idx / consts::numRoomGridCols;

        i32 neighbor_candidates[4];
        i32 num_candidates = 0;
        { // Left
            i32 neighbor_col = room_col - 1;
            i32 neighbor_row = room_row;

            if (neighbor_col >= 0) {
                i32 neighbor_idx =
                    neighbor_row * consts::numRoomGridCols + neighbor_col;

                if ((door_states[room_idx] & 1) == 0) {
                    neighbor_candidates[num_candidates++] = neighbor_idx;
                }
            }
        }

        { // Right
            i32 neighbor_col = room_col + 1;
            i32 neighbor_row = room_row;

            if (neighbor_col < consts::numRoomGridCols) {
                i32 neighbor_idx =
                    neighbor_row * consts::numRoomGridCols + neighbor_col;

                if ((door_states[neighbor_idx] & 1) == 0) {
                    neighbor_candidates[num_candidates++] = neighbor_idx;
                }
            }
        }

        { // Up
            i32 neighbor_col = room_col;
            i32 neighbor_row = room_row + 1;

            if (neighbor_row < consts::numRoomGridRows) {
                i32 neighbor_idx =
                    neighbor_row * consts::numRoomGridCols + neighbor_col;

                if ((door_states[room_idx] & 2) == 0) {
                    neighbor_candidates[num_candidates++] = neighbor_idx;
                }
            }
        }

        { // Down
            i32 neighbor_col = room_col;
            i32 neighbor_row = room_row - 1;

            if (neighbor_row >= 0) {
                i32 neighbor_idx =
                    neighbor_row * consts::numRoomGridCols + neighbor_col;

                if ((door_states[neighbor_idx] & 2) == 0) {
                    neighbor_candidates[num_candidates++] = neighbor_idx;
                }
            }
        }

        if (num_candidates < 2 && room_idx > 0) {
            continue;
        }

        i32 candidate_idx = rng.sampleI32(0, num_candidates);

        i32 neighbor_idx = neighbor_candidates[candidate_idx];
        i32 neighbor_col = neighbor_idx % consts::numRoomGridCols;
        i32 neighbor_row = neighbor_idx / consts::numRoomGridCols;

        if (neighbor_col < room_col) {
            door_states[room_idx] |= 1;
        } else if (neighbor_row > room_row) {
            door_states[room_idx] |= 2;
        } else if (neighbor_col > room_col) {
            door_states[neighbor_idx] |= 1;
        } else if (neighbor_row < room_row) {
            door_states[neighbor_idx] |= 2;
        }
    }

    for (i32 row = 0; row < consts::numRoomGridRows; row++) {
        for (i32 col = 0; col < consts::numRoomGridCols; col++) {
            i32 room_idx = row * consts::numRoomGridCols + col;
            AABB2D aabb = room_bounds[room_idx];

            Vector2 left_pos = aabb.pMin;
            f32 left_len = aabb.pMax.y - aabb.pMin.y;

            if (col == 0) {
                if (row == 0) {
                    makeWall(ctx, nav_voxels, left_pos, 0.f, 0.25f * left_len);
                    Vector2 next_left_pos = left_pos;
                    next_left_pos.y += 0.75f * left_len;
                    makeWall(ctx, nav_voxels, next_left_pos,
                             0.f, 0.25f * left_len);
                } else {
                    makeWall(ctx, nav_voxels, left_pos, 0.f, left_len);
                }
            } else {
                f32 door_segment[2] = {
                    aabb.pMin.y + consts::doorSize, 
                    aabb.pMax.y - consts::doorSize,
                };

                AABB2D neighbor_aabb =
                    room_bounds[row * consts::numRoomGridCols + col - 1];

                if (row == 0 || row == consts::numRoomGridRows - 1) {
                    f32 neighbor_len =
                        neighbor_aabb.pMax.y - neighbor_aabb.pMin.y;

                    if (neighbor_len > left_len) {
                        left_pos.y = neighbor_aabb.pMin.y;
                        left_len = neighbor_len;
                    }
                }

                if ((door_states[room_idx] & 1) == 0) {
                    makeWall(ctx, nav_voxels, left_pos, 0.f, left_len);
                } else {
                    {
                        f32 neighbor_door_segment[2] = {
                            neighbor_aabb.pMin.y + consts::doorSize,
                            neighbor_aabb.pMax.y - consts::doorSize,
                        };

                        if (neighbor_door_segment[0] > door_segment[0]) {
                            door_segment[0] = neighbor_door_segment[0];
                        }

                        if (neighbor_door_segment[1] < door_segment[1]) {
                            door_segment[1] = neighbor_door_segment[1];
                        }
                    }

                    if (door_segment[1] <
                            door_segment[0] + consts::doorSize) {
                        door_segment[0] -= consts::doorSize;
                        door_segment[1] += consts::doorSize;
                    }

                    // FP precision can cause these to slightly underflow
                    // what their bounds should be
                    door_segment[0] = std::max(
                        door_segment[0], left_pos.y);
                    door_segment[1] = std::min(
                        door_segment[1], left_pos.y + left_len);

                    assert(door_segment[1] >=
                           door_segment[0] + consts::doorSize);

                    float door_start_range =
                        door_segment[1] - door_segment[0] - consts::doorSize;

                    f32 door_start = door_segment[0] +
                        rng.sampleUniform() * door_start_range;

                    assert(door_start >= left_pos.y);

                    makeWall(ctx, nav_voxels, left_pos,
                             0.f, door_start - left_pos.y);

                    Vector2 past_door_start = {
                        .x = left_pos.x,
                        .y = door_start + consts::doorSize,
                    };

                    f32 wall_end = left_pos.y + left_len;
                    makeWall(ctx, nav_voxels, past_door_start,
                             0.f, wall_end  - past_door_start.y);
                }
            }

            Vector2 top_pos = { aabb.pMin.x, aabb.pMax.y };
            f32 top_len = aabb.pMax.x - aabb.pMin.x;

            if (row == consts::numRoomGridRows - 1) {
                makeWall(ctx, nav_voxels, top_pos, top_len, 0.f);
            } else {
                f32 door_segment[2] = {
                    aabb.pMin.x + consts::doorSize, 
                    aabb.pMax.x - consts::doorSize,
                };

                AABB2D neighbor_aabb =
                    room_bounds[(row + 1) * consts::numRoomGridCols + col];

                if ((col == 0 || col == consts::numRoomGridCols - 1)) {
                    f32 neighbor_len =
                        neighbor_aabb.pMax.x - neighbor_aabb.pMin.x;

                    if (neighbor_len > top_len) {
                        top_pos.x = neighbor_aabb.pMin.x;
                        top_len = neighbor_len;
                    }
                }

                if ((door_states[room_idx] & 2) == 0) {
                    makeWall(ctx, nav_voxels, top_pos, top_len, 0.f);
                } else {
                    {
                        f32 neighbor_door_segment[2] = {
                            neighbor_aabb.pMin.x + consts::doorSize,
                            neighbor_aabb.pMax.x - consts::doorSize,
                        };

                        if (neighbor_door_segment[0] > door_segment[0]) {
                            door_segment[0] = neighbor_door_segment[0];
                        }

                        if (neighbor_door_segment[1] < door_segment[1]) {
                            door_segment[1] = neighbor_door_segment[1];
                        }
                    }

                    if (door_segment[1] < door_segment[0] + consts::doorSize) {
                        door_segment[0] -= consts::doorSize;
                        door_segment[1] += consts::doorSize;
                    }

                    assert(door_segment[1] >=
                           door_segment[0] + consts::doorSize);

                    f32 door_start = door_segment[0] + rng.sampleUniform() * (
                        door_segment[1] - door_segment[0] - consts::doorSize);

                    assert(door_start >= top_pos.x);

                    makeWall(ctx, nav_voxels, top_pos,
                             door_start - top_pos.x, 0.f);

                    Vector2 past_door_start = {
                        .x = door_start + consts::doorSize,
                        .y = top_pos.y,
                    };

                    f32 wall_end = top_pos.x + top_len;
                    makeWall(ctx, nav_voxels, past_door_start,
                             wall_end - past_door_start.x, 0.f);
                }
            }
        }
    }

    // Fill in missing bottom walls
    for (i32 col = 0; col < consts::numRoomGridCols; col++) {
        const i32 row = 0;
        AABB2D aabb = room_bounds[row * consts::numRoomGridCols + col];

        Vector2 bottom_pos = aabb.pMin;
        makeWall(ctx, nav_voxels, bottom_pos,
                 aabb.pMax.x - aabb.pMin.x, 0.f);
    }

    // Fill in missing right walls
    for (i32 row = 0; row < consts::numRoomGridRows; row++) {
        const i32 col = consts::numRoomGridCols - 1;
        AABB2D aabb = room_bounds[row * consts::numRoomGridCols + col];

        Vector2 right_pos(aabb.pMax.x, aabb.pMin.y);
        makeWall(ctx, nav_voxels, right_pos,
                 0.f, aabb.pMax.y - aabb.pMin.y);
    }

    NavSystem::queueNavmeshBuildFromVoxels(ctx, nav_voxels);
}

void generateLevelGeometry(Engine &ctx, RandKey base_rnd)
{
    ctx.singleton<LabyrinthGenKey>().rnd = base_rnd;

    makeGround(ctx);

    std::array<AABB2D, consts::numRooms> room_bounds;
    makeRooms(ctx, base_rnd, room_bounds.data());

    ctx.singleton<LabyrinthState>() = {
        .roomBounds = room_bounds,
    };
}

void generateWorld(Engine &ctx)
{
    RNG &rng = ctx.singleton<GameRNG>();

    generateLevelGeometry(ctx, rng.randKey());

    AABB2D *room_bounds = ctx.singleton<LabyrinthState>().roomBounds.data();

    std::array<i32, consts::numRooms> dragon_rooms;
    std::array<i32, consts::numRooms> wiz_rooms;
    selectSpawnRooms(rng, room_bounds,
                     dragon_rooms.data(), wiz_rooms.data());

    std::array<MagicElement, consts::numDragons> dragon_elements;
    spawnDragons(ctx, rng, room_bounds, dragon_rooms.data(),
                 dragon_elements.data());
    spawnWizards(ctx, rng, room_bounds, wiz_rooms.data(),
                 dragon_elements.data());
}

}
