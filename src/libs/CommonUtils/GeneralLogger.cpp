#include "GeneralLogger.h"

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "spdlog/async.h"
#include "spdlog/async_logger.h"
#include "spdlog/common.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

namespace CommonUtils
{

std::shared_ptr<spdlog::async_logger> GeneralLogger::s_generalLogger;
std::shared_ptr<spdlog::async_logger> GeneralLogger::s_traceLogger;

GeneralLogger::~GeneralLogger()
{
    s_generalLogger->info("General Logger Destructor");
    s_traceLogger->dump_backtrace();
    s_traceLogger->flush();
}

void GeneralLogger::init(const std::string &logNameBase)
{
    if (_isInited)
    {
        return;
    }
    _isInited = true;

    spdlog::init_thread_pool(8192, 1);

    constexpr auto MAX_FILE_SIZE = static_cast<size_t>(1024 * 1024 * 5);
    constexpr size_t MAX_NUM_FILES = 3;

    const std::string logfileName = logNameBase + ".log";
    const std::string tracefileName = logNameBase + "_trace.log";

    /**
     * General Logging: Debugs or higher go to the Console.  Infos
     * or higher go the log file.  Trace logs do not show up in
     * the general logger, see trace logger for that!
     */
    auto generalStdOutSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    generalStdOutSink->set_level(spdlog::level::debug);
    auto generalRotatingSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logfileName, MAX_FILE_SIZE, MAX_NUM_FILES);
    generalRotatingSink->set_level(spdlog::level::info);

    std::vector<spdlog::sink_ptr> generalSinks{generalStdOutSink, generalRotatingSink};
    s_generalLogger = std::make_shared<spdlog::async_logger>(
        std::string(GENERALLOGGER_NAME), generalSinks.begin(), generalSinks.end(),
        spdlog::thread_pool(), spdlog::async_overflow_policy::overrun_oldest);
    s_generalLogger->set_pattern(std::string(LOG_PATTERN));
    s_generalLogger->set_level(spdlog::level::debug);
    s_generalLogger->info("General Purpose Logger is Created! " + logfileName);

    /**
     * Trace Logging: Trace Statements should go to the trace logger.
     * they do not show up in the std out or in a log file, but go to
     * a circular buffer in the log object and dumped when  a segfault
     * is caught. 
     */
    auto traceStdOutSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto traceRotatingSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(tracefileName, MAX_FILE_SIZE, MAX_NUM_FILES);
    std::vector<spdlog::sink_ptr> traceSinks{traceStdOutSink, traceRotatingSink};
    s_traceLogger = std::make_shared<spdlog::async_logger>(
        std::string(TRACELOGGER_NAME), traceSinks.begin(), traceSinks.end(),
        spdlog::thread_pool(), spdlog::async_overflow_policy::overrun_oldest);
    s_traceLogger->set_level(spdlog::level::err);
    s_traceLogger->enable_backtrace(128);
    s_traceLogger->set_pattern(std::string(LOG_PATTERN));
    s_generalLogger->info("Trace Logger is Created! " + tracefileName);
    s_traceLogger->trace("Trace Logger is Created!");

    spdlog::flush_every(std::chrono::seconds(1));
    spdlog::register_logger(s_generalLogger);
    spdlog::register_logger(s_traceLogger);

}

}
