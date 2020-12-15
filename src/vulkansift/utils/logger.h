#ifndef LOGGER_H
#define LOGGER_H

#ifdef VK_USE_PLATFORM_ANDROID_KHR
#include <android/log.h>
#define logInfo(TAG, ...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

#define logError(TAG, ...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#else
#include <cstdio>
#define logInfo(TAG, ...)                                                                                                                                 \
  {                                                                                                                                                       \
    fprintf(stdout, "[INFO][%s] ", TAG);                                                                                                                  \
    fprintf(stdout, __VA_ARGS__);                                                                                                                         \
    fprintf(stdout, "\n");                                                                                                                                \
    fflush(stdout);                                                                                                                                       \
  }

#define logError(TAG, ...)                                                                                                                                \
  {                                                                                                                                                       \
    fprintf(stdout, "\033[0;31m");                                                                                                                        \
    fprintf(stdout, "[ERROR][%s] ", TAG);                                                                                                                 \
    fprintf(stdout, __VA_ARGS__);                                                                                                                         \
    fprintf(stdout, "\n");                                                                                                                                \
    fprintf(stdout, "\033[0m");                                                                                                                           \
    fflush(stdout);                                                                                                                                       \
  }

#endif

#endif // LOGGER_H