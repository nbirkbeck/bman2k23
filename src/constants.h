#ifndef _BMAN_CONSTANTS_H
#define _BMAN_CONSTANTS_H

const int kDefaultWidth = 17;
const int kDefaultHeight = 13;

const int kSubpixelSize = 64;
const int kSubPixelSize = 64;
const int kDefaultBombTimer = 5 * 60;
const int kExplosionTimer = 30;
const int kMovePadding = kSubpixelSize / 16;
const int kDyingTimer = 4 * 3 * 3;

const int kPointsKill = 50;
const int kPointsPowerUp = 10;
const int kPointsBrick = 1;

const int kAgentSpeed = 4;
const int kBombSpeed = 3;

static constexpr int kScreenWidth = 640;
static constexpr int kScreenHeight = 480;
static constexpr int kGridSize = 32;
static constexpr int kOffsetX = 48;
static constexpr int kOffsetY = 32;

static constexpr int kDirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
#endif
