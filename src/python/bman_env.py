import game_wrapper as bman
import numpy as np
import gym
import math
from gym import spaces

class BManGridEnv(gym.Env):
  metadata = {'render.modes': ['human']}

  def __init__(self):
    super(BManGridEnv, self).__init__()

    self.game = bman.GameWrapper()
    self.game.build_simple_level(2)
    access_map = self.game.get_map()

    # Can move in one of 4 directions, set bombs or use power-up 
    n_actions = 5
    self.action_space = spaces.Discrete(n_actions)
    self.observation_space = spaces.Box(low=-1.,
                                        high=1.,
                                        shape=(access_map.h(),access_map.w()), dtype=np.float)
    self.window = None
    self.current_step = 0
    
  def reset(self):
    self.game.reset()
    self.current_step = 0
    access_map = self.game.get_map()
    return np.array(access_map.data()).reshape(access_map.h(), access_map.w())

  def step(self, action):
    score_before = self.game.get_score(0)
    pos_before = self.game.pos()
    self.game.move_agent(action, action == 4, action == 5)
    score_after = self.game.get_score(0)
    pos_after = self.game.pos()
    self.current_step += 1

    # Compute reward as delta of score
    reward = score_after - score_before
    reward += ((pos_before[0] - pos_after[0]) ** 2 + (pos_before[1] - pos_after[1]) ** 2) / 1000.0

    # Optionally we can pass additional info, we are not using that for now
    info = {'moved': action, 'score': score_after}
    access_map = self.game.get_map()
    done = self.game.player_is_dead()
    if done: reward -= 5
    return np.array(access_map.data()).reshape(access_map.h(), access_map.w()), reward , done, info

  def render(self, mode='console'):
    if mode != 'human':
      raise NotImplementedError()
    if not self.window:
      self.window = bman.GameWindow()
    self.window.draw_game(self.game)
  
  def close(self):
    pass
    
