// Tests for Logger — uses mock MATLAB engine
#include "../include/mexforge/core/logger.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <filesystem>

void test_level_filtering() {
    auto engine = std::make_shared<matlab::engine::MATLABEngine>();
    mexforge::Logger logger(engine);

    logger.setLevel(mexforge::LogLevel::Warn);

    logger.debug("should not appear");
    logger.info("should not appear");
    assert(engine->calls.empty());

    logger.warn("should appear");
    assert(engine->calls.size() == 1);

    logger.error("should also appear");
    assert(engine->calls.size() == 2);

    std::cout << "  [PASS] level_filtering\n";
}

void test_level_from_string() {
    auto engine = std::make_shared<matlab::engine::MATLABEngine>();
    mexforge::Logger logger(engine);

    logger.setLevel("error");
    assert(logger.getLevel() == mexforge::LogLevel::Error);

    logger.setLevel("trace");
    assert(logger.getLevel() == mexforge::LogLevel::Trace);

    logger.setLevel("off");
    assert(logger.getLevel() == mexforge::LogLevel::Off);

    std::cout << "  [PASS] level_from_string\n";
}

void test_matlab_disable() {
    auto engine = std::make_shared<matlab::engine::MATLABEngine>();
    mexforge::Logger logger(engine);

    logger.setLevel(mexforge::LogLevel::Trace);
    logger.enableMatlab(false);

    logger.info("invisible");
    assert(engine->calls.empty());

    logger.enableMatlab(true);
    logger.info("visible");
    assert(engine->calls.size() == 1);

    std::cout << "  [PASS] matlab_disable\n";
}

void test_buffered_mode() {
    auto engine = std::make_shared<matlab::engine::MATLABEngine>();
    mexforge::Logger logger(engine);

    logger.setLevel(mexforge::LogLevel::Trace);
    logger.enableBuffered(true);

    logger.info("msg1");
    logger.info("msg2");
    logger.info("msg3");

    // Nothing sent to MATLAB yet
    assert(engine->calls.empty());
    assert(logger.bufferSize() == 3);

    // Flush sends one combined call
    logger.flush();
    assert(engine->calls.size() == 1);
    assert(logger.bufferSize() == 0);

    std::cout << "  [PASS] buffered_mode\n";
}

void test_file_logging() {
    const std::string testFile = "/tmp/mexforge_test_log.txt";

    // Clean up from previous runs
    std::filesystem::remove(testFile);

    auto engine = std::make_shared<matlab::engine::MATLABEngine>();
    mexforge::Logger logger(engine);

    logger.setLevel(mexforge::LogLevel::Trace);
    logger.enableMatlab(false);
    logger.setLogFile(testFile);

    logger.info("file test message");
    logger.closeLogFile();

    // Verify file was written
    std::ifstream in(testFile);
    assert(in.is_open());

    std::string line;
    std::getline(in, line);
    assert(line.find("[INFO]") != std::string::npos);
    assert(line.find("file test message") != std::string::npos);

    std::filesystem::remove(testFile);
    std::cout << "  [PASS] file_logging\n";
}

void test_off_level() {
    auto engine = std::make_shared<matlab::engine::MATLABEngine>();
    mexforge::Logger logger(engine);

    logger.setLevel(mexforge::LogLevel::Off);

    logger.trace("no");
    logger.debug("no");
    logger.info("no");
    logger.warn("no");
    logger.error("no");
    logger.fatal("no");

    assert(engine->calls.empty());

    std::cout << "  [PASS] off_level\n";
}

int main() {
    std::cout << "Logger tests:\n";
    test_level_filtering();
    test_level_from_string();
    test_matlab_disable();
    test_buffered_mode();
    test_file_logging();
    test_off_level();
    std::cout << "All Logger tests passed.\n\n";
    return 0;
}
