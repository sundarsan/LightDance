#include <sys/time.h>

double get_time_in_seconds() {
  struct timeval t;
  gettimeofday(&t, NULL);
  return (double) t.tv_sec + t.tv_usec * 1.e-6;
}

double get_elapsed_time_in_seconds() {
  static double start_time = get_time_in_seconds();

  return get_time_in_seconds() - start_time;
}
