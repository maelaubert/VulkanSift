#ifndef VKENV_LOGGER_H
#define VKENV_LOGGER_H

typedef enum
{
  VKENV_LOG_NONE,
  VKENV_LOG_ERROR,
  VKENV_LOG_WARNING,
  VKENV_LOG_INFO,
  VKENV_LOG_DEBUG
} vkenv_LogLevel;

#define logError(TAG, ...) vkenv_log(VKENV_LOG_ERROR, TAG, __VA_ARGS__)
#define logWarning(TAG, ...) vkenv_log(VKENV_LOG_WARNING, TAG, __VA_ARGS__)
#define logInfo(TAG, ...) vkenv_log(VKENV_LOG_INFO, TAG, __VA_ARGS__)
#define logDebug(TAG, ...) vkenv_log(VKENV_LOG_DEBUG, TAG, __VA_ARGS__)

void vkenv_setLogLevel(vkenv_LogLevel log_level);
void vkenv_log(vkenv_LogLevel log_level, const char *tag, const char *format, ...);

#endif // VKENV_LOGGER_H