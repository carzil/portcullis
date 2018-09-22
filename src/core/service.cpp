#include "service.h"

#include <chrono>
#include <csignal>
#include <deque>
#include <list>
#include <exception>
#include <functional>

#include <systemd/sd-daemon.h>

#include <coro/reactor.h>
#include <python/wrappers.h>
#include <util/python.h>
#include <handles/http.h>

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
        context->Config.BackendIp,
        context->Config.BackendPort,
        false,
        context->Config.Protocol
    )[0];

    Reactor()->SetCoroutineStackSize(config.CoroutineStackSize);

    return context;
}

void TService::Start() {
    /* import portcullis module to initialize binding of C++ wrappers */
    py::module::import("portcullis");

    std::shared_ptr<TContext> context = ReloadContext();
    atomic_store(&Context_, context);

    if (!Context_) {
        return;
    }

    MainState_ = PyThreadState_Swap(nullptr);

    TSignalSet shutdownSignals;
    shutdownSignals.Add(SIGINT);
    shutdownSignals.Add(SIGTERM);

    EIpVersionMode ipVersion = V4_ONLY;

    if (context->Config.BackendIpv6.length() > 0) {
        if (context->Config.BackendIp.length() > 0) {
            ipVersion = V4_AND_V6;
        } else {
            ipVersion = V6_ONLY;
        }
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
            PyThreadState_Swap(MainState_);
            std::shared_ptr<TContext> newContext = ReloadContext();
            std::atomic_store(&Context_, newContext);
            /* delete python objects that hold old context */
            py::module::import("gc").attr("collect")();
            newContext->Logger->info("context reloaded");
            ::sd_notify(0, "STATUS=Reload succesful");
            MainState_ = PyThreadState_Swap(nullptr);
        } catch (const std::exception& e) {
            ::sd_notify(0, "STATUS=Reload failed");
            oldContext->Logger->error("context reload failed: {}", e.what());
        }
        ::sd_notify(0, "READY=1");
    });

    PyInterpreterState* interpreter = MainState_->interp;

    Listener_.OnAccepted([this, interpreter](TTcpHandlePtr accepted) {
        Reactor()->StartCoroutine([this, interpreter, accepted]() {
            TContextPtr context = std::atomic_load(&Context_);
            TContextWrapper wrapper(context);

            PyThreadState* newState = nullptr;
            if (PyStates_.empty()) {
                newState = PyThreadState_New(interpreter);
            } else {
                newState = *PyStates_.begin();
                PyStates_.pop_front();
            }
            if (!newState) {
                context->Logger->critical("cannot create python thread state");
                return;
            }

            PyThreadState_Swap(newState);
            const auto& internals = py::detail::get_internals();
            PyThread_set_key_value(internals.tstate, newState);

            try {
                context->HandlerObject(wrapper, TTcpHandleWrapper(context, accepted));

                /* if handler still holds pointer to accepted */
                accepted->Close();
            } catch (const std::exception& e) {
                context->Logger->error("exception in handler: {}", e.what());
                accepted->Close();
            }

            PyThreadState* oldState = PyThreadState_Swap(nullptr);
            ASSERT(oldState == newState);

            PyStates_.push_back(newState);
        });
        Reactor()->Yield();
    });

    ::sd_notify(0, "READY=1");
    ::sd_notify(0, "STATUS=Started");

    Listener_.Run(context->Config.Backlog);
}

TService::~TService() {
    PyThreadState_Swap(MainState_);
    const auto& internals = py::detail::get_internals();
    PyThread_set_key_value(internals.tstate, MainState_);

    for (PyThreadState* state : PyStates_) {
        PyThreadState_Clear(state);
        PyThreadState_Delete(state);
    }
}
