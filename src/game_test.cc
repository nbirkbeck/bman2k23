
#include <gtest/gtest.h>
#include <vector>

#include "game.h"
#include "level.grpc.pb.h"

class GameTest : public testing::Test {
public:
  void SetUp() {
    game_.BuildSimpleLevel(3);
    game_.AddPlayer();
  }
  Game game_;
};

TEST_F(GameTest, TestMoveRight) {
  std::vector<bman::MovePlayerRequest> move(1);
  auto* action = move[0].add_actions();
  action->set_dx(1);
  action->set_dy(0);

  for (int i = 0; i < 32; ++i) {
    EXPECT_TRUE(game_.Step(move));

    auto& state = game_.game_state();
    EXPECT_EQ(32 + (i + 1), state.players(0).x());
    EXPECT_EQ(32, state.players(0).y());
  }
}

TEST_F(GameTest, TestMoveDown) {
  std::vector<bman::MovePlayerRequest> move(1);
  auto* action = move[0].add_actions();
  action->set_dx(0);
  action->set_dy(1);

  for (int i = 0; i < 32; ++i) {
    EXPECT_TRUE(game_.Step(move));

    auto& state = game_.game_state();
    EXPECT_EQ(32, state.players(0).x());
    EXPECT_EQ(32 + (i + 1), state.players(0).y());
  }
}

TEST_F(GameTest, TestMoveLeft) {
  std::vector<bman::MovePlayerRequest> move(1);
  auto* action = move[0].add_actions();
  action->set_dx(-kMovePadding / 2);
  action->set_dy(0);

  EXPECT_TRUE(game_.Step(move));

  auto& state = game_.game_state();
  EXPECT_EQ(32 - kMovePadding / 2, state.players(0).x());
  EXPECT_EQ(32, state.players(0).y());

  EXPECT_TRUE(game_.Step(move));
  state = game_.game_state();
  EXPECT_EQ(32 - kMovePadding, state.players(0).x());
  EXPECT_EQ(32, state.players(0).y());

  EXPECT_TRUE(game_.Step(move));
  state = game_.game_state();
  EXPECT_EQ(32 - kMovePadding, state.players(0).x());
  EXPECT_EQ(32, state.players(0).y());
}

TEST_F(GameTest, TestMoveUp) {
  std::vector<bman::MovePlayerRequest> move(1);
  auto* action = move[0].add_actions();
  action->set_dx(0);
  action->set_dy(-kMovePadding / 2);

  EXPECT_TRUE(game_.Step(move));

  auto& state = game_.game_state();
  EXPECT_EQ(32, state.players(0).x());
  EXPECT_EQ(32 - kMovePadding / 2, state.players(0).y());

  EXPECT_TRUE(game_.Step(move));
  state = game_.game_state();
  EXPECT_EQ(32, state.players(0).x());
  EXPECT_EQ(32 - kMovePadding, state.players(0).y());

  EXPECT_TRUE(game_.Step(move));
  state = game_.game_state();
  EXPECT_EQ(32, state.players(0).x());
  EXPECT_EQ(32 - kMovePadding, state.players(0).y());
}

TEST_F(GameTest, TestMoveUpLarge) {
  std::vector<bman::MovePlayerRequest> move(1);
  auto* action = move[0].add_actions();
  action->set_dx(0);
  action->set_dy(-2 * kMovePadding);

  EXPECT_TRUE(game_.Step(move));

  auto& state = game_.game_state();
  EXPECT_EQ(32, state.players(0).x());
  EXPECT_EQ(32 - kMovePadding, state.players(0).y());
}

TEST_F(GameTest, TestMoveComplex) {
  std::vector<bman::MovePlayerRequest> move(1);
  auto* action = move[0].add_actions();
  action->set_dx(64);
  action->set_dy(0);
  EXPECT_TRUE(game_.Step(move));
  EXPECT_TRUE(game_.Step(move));

  action->set_dx(0);
  action->set_dy(2);
  EXPECT_TRUE(game_.Step(move));

  auto& state = game_.game_state();
  EXPECT_EQ(32 + 128, state.players(0).x());
  EXPECT_EQ(32 + 2, state.players(0).y());
}

// Test moving right, left, up, down into a corridor
TEST_F(GameTest, TestApplyCons) {
  std::vector<bman::MovePlayerRequest> move(1);
  auto* action = move[0].add_actions();
  action->set_dx(kSubpixelSize * 2 - kMovePadding);
  action->set_dy(0);

  auto& state = game_.game_state();
  EXPECT_TRUE(game_.Step(move));
  EXPECT_EQ(32 + kSubpixelSize * 2 - kMovePadding, state.players(0).x());
  EXPECT_EQ(32, state.players(0).y());

  action->set_dx(0);
  action->set_dy(kSubPixelSize / 2);
  EXPECT_TRUE(game_.Step(move));
  EXPECT_EQ(32 + kSubpixelSize * 2, state.players(0).x());
  EXPECT_EQ(32 + kSubPixelSize / 2, state.players(0).y());
}

class PowerupTest : public GameTest {

public:
  void SetUp() {
    GameTest::SetUp();
    bman::GameState& state = game_.game_state();
    brick_ = state.mutable_level()->add_bricks();
    brick_->set_x(1);
    brick_->set_y(0);
    brick_->set_solid(false);
  }
  bool MovePlayer(int dx = 32) {
    std::vector<bman::MovePlayerRequest> move(1);
    auto* action = move[0].add_actions();
    action->set_dx(dx);
    action->set_dy(0);
    return game_.Step(move) && (game_.game_state().players(0).x() == 32 + dx);
  }

  bman::LevelState::Brick* brick_;
};

// Test get a powerup.
TEST_F(PowerupTest, TestGetBomb) {
  bman::GameState& state = game_.game_state();
  int num_bombs = state.players(0).num_bombs();
  brick_->set_powerup(bman::PUP_EXTRA_BOMB);

  EXPECT_TRUE(MovePlayer(32));

  EXPECT_EQ(num_bombs + 1, state.players(0).num_bombs());
  EXPECT_FALSE(brick_->has_powerup());
}

TEST_F(PowerupTest, TestDoesntGetBomb) {
  bman::GameState& state = game_.game_state();
  int num_bombs = state.players(0).num_bombs();
  brick_->set_powerup(bman::PUP_EXTRA_BOMB);

  EXPECT_TRUE(MovePlayer(31));

  EXPECT_EQ(num_bombs, state.players(0).num_bombs());
  EXPECT_TRUE(brick_->has_powerup());
}

TEST_F(PowerupTest, TestGetFlame) {
  bman::GameState& state = game_.game_state();
  brick_->set_powerup(bman::PUP_FLAME);

  EXPECT_TRUE(MovePlayer());

  EXPECT_EQ(2, state.players(0).strength());
  EXPECT_FALSE(brick_->has_powerup());
}

TEST_F(PowerupTest, TestGetDetonator) {
  bman::GameState& state = game_.game_state();
  brick_->set_powerup(bman::PUP_DETONATOR);

  EXPECT_TRUE(MovePlayer());

  EXPECT_TRUE(state.players(0).has_detonator());
  EXPECT_FALSE(brick_->has_powerup());
}

class BombTest : public GameTest {
public:
  void SetUp() { GameTest::SetUp(); }
};

TEST_F(BombTest, TestPlaceBombZeroZero) {
  std::vector<bman::MovePlayerRequest> move(1);
  auto* action = move[0].add_actions();
  action->set_place_bomb(true);
  action->set_dx(1);
  game_.Step(move);

  auto& state = game_.game_state();
  EXPECT_EQ(1, state.players(0).num_used_bombs());
  EXPECT_EQ(1, state.level().bombs_size());

  for (int t = 0; t < kDefaultBombTimer; ++t) {
    action->set_place_bomb(false);
    game_.Step(move);
  }
  EXPECT_EQ(0, state.level().bombs_size());
}

TEST_F(BombTest, TestPlaceBombZeroZeroCantMoveBack) {
  std::vector<bman::MovePlayerRequest> move(1);
  auto* action = move[0].add_actions();
  action->set_place_bomb(true);
  action->set_dx(0);
  game_.Step(move);

  for (int i = 0; i < kSubpixelSize; ++i) {
    action->set_dx(1);
    action->set_place_bomb(0);
    game_.Step(move);
  }

  for (int i = 0; i < kSubpixelSize; ++i) {
    action->set_dx(-1);
    game_.Step(move);
  }
  EXPECT_EQ(kSubpixelSize * 3 / 2 - kMovePadding,
            game_.game_state().players(0).x());
}

TEST_F(BombTest, TestPlaceTwoBombsHorizontal) {
  std::vector<bman::MovePlayerRequest> move(1);
  auto& state = game_.game_state();
  state.mutable_players(0)->set_num_bombs(2);

  auto* action = move[0].add_actions();
  action->set_place_bomb(true);
  action->set_dx(0);
  game_.Step(move);

  for (int i = 0; i < 64 - 1; ++i) {
    action->set_place_bomb(false);
    action->set_dx(1);
    game_.Step(move);
  }

  action->set_place_bomb(true);
  action->set_dx(0);
  game_.Step(move);
  EXPECT_EQ(2, state.players(0).num_used_bombs());
  EXPECT_EQ(2, state.level().bombs_size());

  EXPECT_EQ(0, state.level().bombs(0).x());
  EXPECT_EQ(1, state.level().bombs(1).x());

  for (int t = 0; t < kDefaultBombTimer - 64; ++t) {
    action->set_place_bomb(false);
    action->set_dx(1);
    game_.Step(move);
  }

  EXPECT_EQ(0, state.level().bombs_size());
}

TEST_F(BombTest, TestPlaceTwoBombsVertical) {
  std::vector<bman::MovePlayerRequest> move(1);
  auto& state = game_.game_state();
  state.mutable_players(0)->set_num_bombs(2);

  auto* action = move[0].add_actions();
  action->set_place_bomb(true);
  game_.Step(move);

  for (int i = 0; i < 64 - 1; ++i) {
    action->set_place_bomb(false);
    action->set_dy(1);
    game_.Step(move);
  }

  action->set_place_bomb(true);
  action->set_dx(0);
  game_.Step(move);
  EXPECT_EQ(2, state.players(0).num_used_bombs());
  EXPECT_EQ(2, state.level().bombs_size());

  EXPECT_EQ(0, state.level().bombs(0).y());
  EXPECT_EQ(1, state.level().bombs(1).y());

  for (int t = 0; t < kDefaultBombTimer - 64; ++t) {
    action->set_place_bomb(false);
    action->set_dy(1);
    game_.Step(move);
  }

  EXPECT_EQ(0, state.level().bombs_size());
}

int main() { return RUN_ALL_TESTS(); }
