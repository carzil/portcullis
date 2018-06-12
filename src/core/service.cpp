#include <csignal>
#include <deque>
#include <exception>
#include <functional>
#include <systemd/sd-daemon.h>

#include "service.h"
#include "version.h"
#include "python/module.h"

using namespace std::placeholders;

static py::object PyEvalFile(const std::string& path) {
    py::object module = py::dict();
    module["__builtins__"] = PyEval_GetBuiltins();
    py::eval_file(path, module);
    return module;
}

TService::TService(TEventLoop* loop, const std::string& configPath)
    : Loop_(loop)
    , ConfigPath_(configPath)
{
    std::shared_ptr<TContext> context = ReloadContext();
    atomic_store(&Context_, context);

    /* import portcullis module to initialize binding of C++ wrappers */
    py::module::import("portcullis");
}

std::shared_ptr<TContext> TService::ReloadContext() {
    std::shared_ptr<TContext> oldContext = std::atomic_load(&Context_);

    auto logger = spdlog::get("main");

    logger->info("loading config from file {}", AbsPath(ConfigPath_));

    py::object pyConfig = PyEvalFile(ConfigPath_);

    TConfig config;
    config.Name = pyConfig["name"].cast<std::string>();
    config.Host = pyConfig["host"].cast<std::string>();
    config.Port = pyConfig["port"].cast<std::string>();
    config.Backlog = pyConfig["backlog"].cast<size_t>();
    config.HandlerFile = pyConfig["handler_file"].cast<std::string>();

    py::object handlerModule = PyEvalFile(config.HandlerFile);

    TContextPtr context(new TContext());
    context->Config = config;
    context->HandlerClass = handlerModule["Handler"];
    context->HandlerModule = std::move(handlerModule);
    context->Logger = logger;
    context->Loop = Loop_;

    std::vector<TSocketAddress> addrs = GetAddrInfo(context->Config.Host, context->Config.Port, true, "tcp");

    if (!oldContext || (oldContext->Listener->Address() != addrs[0])) {
        context->Listener = Loop_->MakeTcp();
        context->Listener->ReuseAddr();
        context->Listener->Bind(addrs[0]);
        context->Listener->Listen([this](TSocketHandlePtr, TSocketHandlePtr accepted) {
            TContextPtr context = std::atomic_load(&Context_);
            context->HandlerClass(
                TContextWrapper(std::move(context)),
                TSocketHandleWrapper(std::move(accepted))
            );
        }, context->Config.Backlog);

        context->Logger->info("listening on {}:{}", addrs[0].Host(), addrs[0].Port());
    } else {
        context->Listener = oldContext->Listener;
    }

    context->Loop->Cleanup([context]() {
        context->Cleanup();
    });

    return context;
}

void TService::Shutdown() {
    ::sd_notify(0, "STOPPING=1");
    Context_->Logger->info("shutdown request");
    Loop_->Shutdown();
}

void TService::Start() {
    if (!Context_) {
        return;
    }

    Loop_->Signal(SIGINT, [this](TSignalInfo info) {
        std::shared_ptr<TContext> context = std::atomic_load(&Context_);
        context->Logger->info("caught SIGINT from pid {}", info.Sender());
        Shutdown();
    });

    Loop_->Signal(SIGTERM, [this](TSignalInfo info) {
        std::shared_ptr<TContext> context = std::atomic_load(&Context_);
        context->Logger->info("caught SIGTERM from pid {}", info.Sender());
        Shutdown();
    });

    Loop_->Signal(SIGUSR1, [this](TSignalInfo info) {
        std::shared_ptr<TContext> context = std::atomic_load(&Context_);
        context->Logger->info("caught SIGUSR1 from {}", info.Sender());
        try {
            ::sd_notify(0, "RELOADING=1");
            std::shared_ptr<TContext> newContext = ReloadContext();
            std::atomic_store(&Context_, newContext);
            ::sd_notify(0, "READY=1");
            newContext->Logger->info("context reloaded");
        } catch (const std::exception& e) {
            context->Logger->error("context reload failed: {}", e.what());
        }
    });

    /* well, loop not started yet, but we're almost ready */
    ::sd_notify(0, "READY=1");
}
