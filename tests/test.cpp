#define CATCH_CONFIG_MAIN

#if DMT_DEBUG
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <memory>
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/MessageOnlyFormatter.h>
#include <plog/Init.h>

using Appender = plog::ConsoleAppender<plog::MessageOnlyFormatter>;
static std::unique_ptr<Appender> appender;

class TestRunListener : public Catch::EventListenerBase {
public:
  using Catch::EventListenerBase::EventListenerBase;

  void testRunStarting(Catch::TestRunInfo const&) override {
    appender = std::make_unique<Appender>();
    plog::init(plog::verbose, appender.get());
  }
};

CATCH_REGISTER_LISTENER(TestRunListener)
#endif