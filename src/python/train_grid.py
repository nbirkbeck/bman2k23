import game_wrapper
import bman_env
import os
import sys
import time
import argparse
import numpy as np
from stable_baselines3.common.vec_env import VecFrameStack
from stable_baselines3 import PPO, A2C
from sb3_contrib import QRDQN

from stable_baselines3.common.cmd_util import make_vec_env

parser = argparse.ArgumentParser(description='Train single agent.')
parser.add_argument('--num_train_its', type=int,
                    help='Number of training iterations', default=10000)
parser.add_argument('--num_steps', type=int,
                    help='Number of training iterations', default=1000)
parser.add_argument('--model', type=str, help='DQN or PPO', default='DQN')
parser.add_argument('--save_dir', type=str, help='Director to save state',
                    default='/tmp/gym')
parser.add_argument('--train', type=int, help='Whether to train or not',
                    default=0)
args = parser.parse_args(sys.argv[1:])

def callback(a1, a2):
    if a1["infos"][0]["score"] > 100:
        print(a1["infos"])
        return False
    #if a1["n_updates"] > 2140:
    #    return False
    return True

if args.model == 'DQN':
    env_raw = bman_env.BManGridEnv()
    env = make_vec_env(lambda: env_raw, n_envs=1)
    env = VecFrameStack(env, n_stack=4)
    policy_kwargs = dict(n_quantiles=50)
    model = QRDQN('MlpPolicy', env, verbose=2,
                  policy_kwargs=policy_kwargs,
                  learning_rate=0.00035,
                  buffer_size=100000,
                  exploration_initial_eps=0.2,
                  exploration_fraction=0.95,
                  exploration_final_eps=0.01)
else:
    env_raw = bman_env.BManGridEnv()
    env = make_vec_env(lambda: env_raw, n_envs=4 if args.train else 1)

    if args.train:
        env = VecFrameStack(env, n_stack=10)
    model = PPO('MlpPolicy', env, verbose=1)
    
if args.train:
    model.learn(args.num_train_its, callback=callback)
    os.makedirs(args.save_dir, exist_ok=True)
    model.save(args.save_dir + "/10k")
    if args.model == 'DQN':
        model.save_replay_buffer(args.save_dir + "/replay_buffer")
else:
    if args.model == 'DQN':
        model = QRDQN.load(args.save_dir + "/10k")
        model.load_replay_buffer(args.save_dir + "/replay_buffer")
    else:
        model.load(args.save_dir + "/10k")


obs = env.reset()
env_raw.render_all = True
n_steps = args.num_steps
for step in range(n_steps):
  action, _ = model.predict(obs, deterministic=False)
  obs, reward, done, info = env.step(action)
  if done:
      obs = env.reset()
  #print('obs=', obs, 'reward=', reward, 'done=', done)
  env.render(mode='human')
  #if env_raw.window.is_closed(): break
