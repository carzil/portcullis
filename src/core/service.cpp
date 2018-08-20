#include "service.h"
#include <csignal>
#include <deque>
#include <exception>
#include <functional>

#include <systemd/sd-daemon.h>

#include <coro/reactor.h>
#include <python/wrappers.h>
#include <util/python.h>

using namespace std::placeholders;

TService::TService(const std::string& configPath)
    : ConfigPath_(configPath)
{
}

std::shared_ptr<TContext> TService::ReloadContext() {
    std::shared_ptr<TContext> oldContext = std::atomic_load(&Context_);

    auto logger = spdlog::get("main");

    logger->info("loading config from file {}", AbsPath(ConfigPath_));

    TConfig config = ReadConfigFromFile(ConfigPath_);

    py::object handlerModule = PyEvalFile(config.HandlerFile);

    TContextPtr context = std::make_shared<TContext>();
    context->Config = config;
    context->HandlerObject = handlerModule["handler"];
    context->HandlerModule = std::move(handlerModule);
    context->Logger = logger;

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

    // TODO: multiple bind address
    TSocketAddress listeningAddress = GetAddrInfo(context->Config.Host, context->Config.Port, true, "tcp")[0];
    Listener_ = TTcpHandle::Create();
    Listener_->ReuseAddr();
    Listener_->Bind(listeningAddress);
    Listener_->Listen(context->Config.Backlog);
    context->Logger->info("listening on {}:{}", listeningAddress.Host(), listeningAddress.Port());

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

    Reactor()->StartCoroutine([this]() {
        while (true) {
            TResult<TTcpHandlePtr> result = Listener_->Accept();

            if (!result) {
                if (result.Error() == ECANCELED) {
                    break;
                } else {
                    ThrowErr(result.Error(), "accept failed");
                }
            }

            TTcpHandlePtr accepted = std::move(result.Result());

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
        }
    });

    ::sd_notify(0, "READY=1");
    ::sd_notify(0, "STATUS=Started");
}
