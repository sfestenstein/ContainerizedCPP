#ifndef GENERALLOGGER_H_
#define GENERALLOGGER_H_

/**
 * @file GeneralLogger.h
 * @brief Provides a general-purpose async logging facility using spdlog.
 */

#include <memory>

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include "spdlog/spdlog.h"
#include "spdlog/async.h"

/**
 * @defgroup LoggingMacros Logging Macros
 * @brief Convenience macros for logging at various levels.
 * @{
 */

/** @brief Log a critical message. */
#define GPCRIT(...) SPDLOG_LOGGER_CRITICAL(CommonUtils::GeneralLogger::s_generalLogger, __VA_ARGS__);
/** @brief Log an error message. */
#define GPERROR(...) SPDLOG_LOGGER_ERROR(CommonUtils::GeneralLogger::s_generalLogger, __VA_ARGS__);
/** @brief Log a warning message. */
#define GPWARN(...) SPDLOG_LOGGER_WARN(CommonUtils::GeneralLogger::s_generalLogger, __VA_ARGS__);
/** @brief Log an informational message. */
#define GPINFO(...) SPDLOG_LOGGER_INFO(CommonUtils::GeneralLogger::s_generalLogger, __VA_ARGS__);
/** @brief Log a debug message. */
#define GPDEBUG(...) SPDLOG_LOGGER_DEBUG(CommonUtils::GeneralLogger::s_generalLogger, __VA_ARGS__);
/** @brief Log a trace message to the backtrace ring-buffer (dumped on segfault). */
#define GPTRACE(...) SPDLOG_LOGGER_TRACE(CommonUtils::GeneralLogger::s_traceLogger, __VA_ARGS__);

/** @} */ // end of LoggingMacros

namespace CommonUtils
{

/** @brief Name identifier for the general logger. */
static constexpr std::string_view GENERALLOGGER_NAME = "generalLogger";
/** @brief Name identifier for the trace logger. */
static constexpr std::string_view TRACELOGGER_NAME = "traceLogger";

/**
 * @class GeneralLogger
 * @brief Async logging wrapper around spdlog with dual-logger support.
 *
 * Provides a general-purpose logger for standard log levels (critical, error,
 * warning, info, debug) and a separate trace logger that stores messages in a
 * backtrace ring-buffer for post-mortem debugging.
 *
 * @note This class is non-copyable. Use the provided macros (GPCRIT, GPERROR,
 *       GPWARN, GPINFO, GPDEBUG, GPTRACE) for convenient logging.
 *
 * @par Example Usage:
 * @code
 * CommonUtils::GeneralLogger logger;
 * logger.init("myapp");
 * GPINFO("Application started");
 * GPDEBUG("Debug value: {}", someValue);
 * @endcode
 */
class GeneralLogger
{
public:
    /**
     * @brief Default constructor.
     */
    GeneralLogger() = default;

    /**
     * @brief Virtual destructor.
     *
     * Ensures proper cleanup of logging resources.
     */
    virtual ~GeneralLogger();

    /** @brief Deleted copy constructor. */
    GeneralLogger(const GeneralLogger&) = delete;
    /** @brief Deleted copy assignment operator. */
    GeneralLogger& operator=(const GeneralLogger&) = delete;

    /**
     * @brief Initialize the logging system.
     *
     * Sets up both the general logger and trace logger with file and console sinks.
     * Log files are created with the provided base name.
     *
     * @param logNameBase Base name for log files (e.g., "myapp" creates "myapp.log").
     */
    void init(const std::string &logNameBase);

    /** @brief Shared pointer to the general async logger instance. */
    // NOLINTNEXTLINE(readability-identifier-naming)
    static std::shared_ptr<spdlog::async_logger> s_generalLogger;

    /** @brief Shared pointer to the trace async logger instance. */
    // NOLINTNEXTLINE(readability-identifier-naming)
    static std::shared_ptr<spdlog::async_logger> s_traceLogger;

private:
    /** @brief Log message format pattern. */
    static constexpr std::string_view LOG_PATTERN = "%Y%m%d_%H%M%S.%e [%t][%s::%! %# %l] %v";
    /** @brief Flag indicating whether the logger has been initialized. */
    std::atomic_bool _isInited = false;
}; 

}
#endif