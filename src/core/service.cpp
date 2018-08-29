#include "service.h"

#include <chrono>
#include <csignal>
#include <deque>
#include <exception>
#include <functional>

#include <systemd/sd-daemon.h>

#include <coro/reactor.h>
#include <python/wrappers.h>
#include <util/python.h>

using namespace std::placeholders;

TService::TService(std::shared_ptr<spdlog::logger> logger, const std::string& configPath)
    : ConfigPath_(configPath)
    , Logger_(std::move(logger))
    , Listener_(Logger_)
{
}

std::shared_ptr<TContext> TService::ReloadContext() {
    std::shared_ptr<TContext> oldContext = std::atomic_load(&Context_);

    Logger_->info("loading config from file {}", AbsPath(ConfigPath_));

    TConfig config = ReadConfigFromFile(ConfigPath_);

    py::object handlerModule = PyEvalFile(config.HandlerFile);

    TContextPtr context = std::make_shared<TContext>();
    context->Config = config;
    context->HandlerObject = handlerModule["handler"];
    context->HandlerModule = std::move(handlerModule);
    context->Logger = Logger_;

    context->BackendAddr = GetAddrInfo(
        context->Config.BackendHost,
        context->Config.BackendPort,
        false,
        context->Config.Protocol
    )[0];

    return context;
}

void TService::Start() {
    std::shared_ptr<TContext> context = ReloadContext();
    atomic_store(&Context_, context);

    /* import portcullis module to initialize binding of C++ wrappers */
    py::module::import("portcullis");

    if (!Context_) {
        return;
    }

    TSignalSet shutdownSignals;
    shutdownSignals.Add(SIGINT);
    shutdownSignals.Add(SIGTERM);

    EIpVersionMode ipVersion = V4_ONLY;

    if (context->Config.AllowIpv6) {
        if (context->Config.Ipv6Only) {
            ipVersion = V6_ONLY;
        } else {
            ipVersion = V4_AND_V6;
        }
    } else if (context->Config.Ipv6Only) {
        throw TException() << "ipv6_only set but IPv6 is not allowed";
    }

    std::vector<TSocketAddress> listeningAddresses = GetAddrInfo(context->Config.Host, context->Config.Port, true, "tcp", ipVersion);
    Listener_.ReuseAddr();
    Listener_.Bind(listeningAddresses);

    Reactor()->OnSignals(shutdownSignals, [this](TSignalInfo info) {
        Context_->Logger->info("caught shutdown signal from pid {}", info.Sender());
        Reactor()->CancelAll();
    });

    Reactor()->OnSignal(SIGUSR1, [this](TSignalInfo info) {
        ::sd_notify(0, "RELOADING=1");
        std::shared_ptr<TContext> oldContext = std::atomic_load(&Context_);
        oldContext->Logger->info("caught SIGUSR1 from {}", info.Sender());
        try {
            std::shared_ptr<TContext> newContext = ReloadContext();
            std::atomic_store(&Context_, newContext);
            /* delete python objects that hold old context */
            py::module::import("gc").attr("collect")();
            newContext->Logger->info("context reloaded");
            ::sd_notify(0, "STATUS=Reload succesful");
        } catch (const std::exception& e) {
            ::sd_notify(0, "STATUS=Reload failed");
            oldContext->Logger->error("context reload failed: {}", e.what());
        }
        ::sd_notify(0, "READY=1");
    });

    Listener_.OnAccepted([this](TTcpHandlePtr accepted) {
        TContextPtr context = std::atomic_load(&Context_);
        Reactor()->StartCoroutine([context, accepted]() {
            try {
                context->HandlerObject(context, TTcpHandleWrapper(context, accepted));

                /* if handler still holds pointer to accepted */
                accepted->Close();
            } catch (const std::exception& e) {
                context->Logger->error("exception in handler: {}", e.what());
                accepted->Close();
            }
        });
    });

    Listener_.Run(context->Config.Backlog);

    ::sd_notify(0, "READY=1");
    ::sd_notify(0, "STATUS=Started");
}
