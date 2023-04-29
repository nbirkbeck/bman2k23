#ifndef TYPES_H
#define TYPES_H

#include "level.grpc.pb.h"
#include <unordered_map>

typedef std::unordered_map<Point2i, bman::LevelState::Bomb*, PointHash> BombMap;
typedef std::unordered_map<Point2i, const bman::LevelState::Bomb*, PointHash>
    BombMapConst;
typedef std::unordered_map<Point2i, bman::LevelState::Brick*, PointHash>
    BrickMap;
typedef std::unordered_map<Point2i, const bman::LevelState::Brick*, PointHash>
    BrickMapConst;

#endif
