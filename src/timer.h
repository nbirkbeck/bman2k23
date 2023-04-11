#ifndef BMAN_TIMER_H
#define BMAN_TIMER_H

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

extern FILE* debug;

namespace bman {
class Timer {
public:
  Timer(void) { Start(); }

  void Start() { gettimeofday(&start_time_, nullptr); }
  void Wait(int64_t ms_to_wait) {
    struct timeval tv, elapsed_time;
    gettimeofday(&tv, nullptr);
    timersub(&tv, &start_time_, &elapsed_time);
    int64_t millis =
        ms_to_wait - (elapsed_time.tv_sec * 1000 + elapsed_time.tv_usec / 1000);
    if (millis >= 2)
      Timer::SleepMillis(millis);
  }

  static void SleepMillis(int32_t ms) {
    useconds_t usec = ((useconds_t)ms) * 1000;
#ifdef __EMSCRIPTEN__
    emscripten_sleep(usec / 1000);
#else
    usleep(usec);
#endif
  }

private:
  struct timeval start_time_;
};
} // namespace bman

#endif
