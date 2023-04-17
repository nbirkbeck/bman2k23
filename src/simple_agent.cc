#include "simple_agent.h"
#include "glog/logging.h"

struct Option {
  Point2i pos;
  double score = 0;
  int num_bricks = 0;
  bool has_powerup = 0;

  bool operator<(const Option& other) const { return score < other.score; }
};

bman::MovePlayerRequest
SimpleAgent::GetPlayerAction(const bman::GameState& game_state_const) {
  bman::GameState game_state(game_state_const);
  // If no plan, or reached new milestone (reconsider)
  LOG(INFO) << game_state_const.players_size() << " "
            << game_state.players_size() << " " << player_index_;

  bman::MovePlayerRequest move;
  if (!game_state.players_size() ||
      player_index_ >= game_state.players_size()) {
    move.add_actions();
    return move;
  }
  const auto& player = game_state.players(player_index_);

  if (player.state() == bman::PlayerState::STATE_DYING ||
      player.state() == bman::PlayerState::STATE_SPAWNING) {
    plan_.clear();
    move.add_actions();
    return move;
  }

  Point2i pos(GridRound(player.x()), GridRound(player.y()));
  bool reached_waypoint = false;
  bool place_bomb = false;

  int cx = player.x() - pos.x * kSubpixelSize;
  int cy = player.y() - pos.y * kSubpixelSize;
  if (!plan_.empty() && plan_.front() == pos &&
      abs(cx - kSubPixelSize / 2) <= 2 && abs(cy - kSubPixelSize / 2) <= 2) {
    reached_waypoint = true;
    place_bomb = plan_.front().place_bomb;
    plan_.pop_front();
    if (place_bomb) {
      // Place a bomb (in our local copy) to avoid killing ourself on upcoming
      // plan
      auto* bomb = game_state.mutable_level()->add_bombs();
      bomb->set_x(pos.x);
      bomb->set_y(pos.y);
    }
  }

  // If next point is obstructed, replan
  MaybeCreateNewPlan(reached_waypoint, game_state, pos);
  if (!plan_.size()) {
    LOG(WARNING) << "No valid plan!";
    move.add_actions();
    return move;
  }
  Point2i desired_pos(plan_.front().pos.x * kSubpixelSize + kSubpixelSize / 2,
                      plan_.front().pos.y * kSubpixelSize + kSubpixelSize / 2);
  Point2i delta = desired_pos - Point2i(player.x(), player.y());
  const int dir = Agent::GetDirFromDelta(delta.x, delta.y);
  GetDeltaFromDir(dir, &delta.x, &delta.y);

  if (dir >= 0) {
    auto* action = move.add_actions();
    action->set_dir(static_cast<bman::Direction>(dir));
    action->set_dx(delta.x);
    action->set_dy(delta.y);
    if (place_bomb) {
      action->set_place_bomb(true);
    }
  } else {
    move.add_actions();
  }
  return move;
}

std::unordered_set<Point2i, PointHash>
SimpleAgent::FindNoGoZones(const BrickMap& brick_map, const BombMap& bomb_map) {

  std::unordered_map<Point2i, int, PointHash> component;
  std::vector<std::vector<Point2i>> component_points;
  auto& config = game_config_;
  auto is_solid = [&config, &brick_map, &bomb_map](const Point2i& pt) {
    if (Game::IsStaticBrick(config, pt.x, pt.y)) {
      return true;
    }
    if (bomb_map.count(pt)) {
      return true;
    }
    if (brick_map.count(pt) && brick_map.at(pt)->solid()) {
      return true;
    }
    return false;
  };

  for (int y = 0; y < config.level_height(); ++y) {
    for (int x = 0; x < config.level_width(); ++x) {
      Point2i pos(x, y);
      if (component.count(pos))
        continue;
      if (is_solid(pos))
        continue;

      std::deque<Point2i> queue;
      queue.push_back(pos);
      component[queue.front()] = component_points.size();

      std::vector<Point2i> points;

      while (!queue.empty()) {
        const static Point2i neigh[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        const auto cur = queue.front();
        queue.pop_front();
        points.push_back(cur);

        for (const auto& n : neigh) {
          const Point2i other = cur + n;
          if (!is_solid(other) && !component.count(other)) {
            queue.push_back(other);
            component[other] = component_points.size();
          }
        }
      }
      component_points.push_back(points);
    }
  }

  std::unordered_set<Point2i, PointHash> result;

  // TOOD: This is only very approximate. Should consider blast radius, etc.
  for (const auto& cp : component_points) {
    if (cp.size() > 4)
      continue;
    for (const auto& pos : cp) {
      result.insert(pos);
    }
  }
  return result;
}

void SimpleAgent::MaybeCreateNewPlan(bool reached_waypoint,
                                     bman::GameState& game_state,
                                     const Point2i& pos) {
  // Do a BFS
  // Score each grid point
  //   Score is number of points you would get
  //   Score goes down for far away things.
  //   If close to opponent (and have bombs), try to attack
  // Pick best point.
  // Setup the goal and the plan.

  // Neighbors.
  const Point2i neigh[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

  BombMap bomb_map = Game::MakeBombMap(game_state);
  LOG(INFO) << bomb_map.size();
  if (plan_.size() && bomb_map.count(plan_.front().pos)) {
    plan_.clear();
  }
  if (!(plan_.empty() ||
        (reached_waypoint && random_double() < replan_prob_))) {
    // TODO replan occasionally
    return;
  }
  BrickMap brick_map;
  for (auto& brick : *game_state.mutable_level()->mutable_bricks()) {
    brick_map[Point2i(brick.x(), brick.y())] = &brick;
  }

  auto no_go = FindNoGoZones(brick_map, bomb_map);

  std::unordered_map<Point2i, Point2i, PointHash> prev_map;
  std::unordered_map<Point2i, int, PointHash> depth_map;
  std::vector<Option> options;
  std::deque<Point2i> queue;

  queue.push_back(pos);
  depth_map[pos] = 0;
  prev_map[pos] = Point2i(0, 0);

  while (!queue.empty()) {
    Point2i cur = queue.front();
    queue.pop_front();

    Option option;
    option.pos = cur;
    if (brick_map.count(cur) && brick_map.at(cur)->has_powerup()) {
      option.score += kPointsPowerUp;
      option.has_powerup = true;
    }
    if (no_go.count(cur)) {
      LOG(INFO) << "no go!";
      option.score = -100;
    }

    for (int n = 0; n < 4; ++n) {
      Point2i new_point = cur + neigh[n];

      if (Game::IsStaticBrick(game_config_, new_point.x, new_point.y)) {
        continue;
      }

      if (bomb_map.count(new_point)) {
        option.score -= 2;
        continue;
      }

      if (brick_map.count(new_point) && brick_map.at(new_point)->solid()) {
        option.score += kPointsBrick;
        option.num_bricks++;
        continue;
      }

      if (!prev_map.count(new_point)) {
        prev_map[new_point] = cur;
        depth_map[new_point] = depth_map[cur] + 1;
        queue.push_back(new_point);
      }

      // Update score with any other bricks in this directiont hat would get
      // hit.
      for (int k = 2; k <= game_state.players(player_index_).strength(); ++k) {
        new_point = new_point + neigh[n];
        if (brick_map.count(new_point) && brick_map.at(new_point)->solid()) {
          option.score += kPointsBrick;
          option.num_bricks++;
          break;
        }
      }
    }

    if (cur != pos) {
      // If you are going
      int num_cycles_per_grid = kSubPixelSize / kAgentSpeed;
      double norm = double(kDefaultBombTimer) / num_cycles_per_grid;
      double dist = double(depth_map[cur]) / norm;
      option.score -= dist;
      options.push_back(option);
    }
  }
  plan_.clear();
  if (!options.size())
    return;

  std::sort(options.begin(), options.end());
  const Option& chosen = options.back();
  Point2i cur = chosen.pos;
  plan_.clear();

  while (cur != pos) {
    PlanPoint plan_point;
    plan_point.pos = cur;
    if (plan_.empty()) {
      plan_point.place_bomb = !chosen.has_powerup;
    }
    plan_.push_back(plan_point);
    if (cur == pos)
      break;
    cur = prev_map[cur];
  }
  std::reverse(plan_.begin(), plan_.end());
  LOG(INFO) << "Done with plan";
}
