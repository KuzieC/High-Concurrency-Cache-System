#include <chrono>
#include <csignal>
#include <exception>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <fmt/base.h>
#include <fmt/core.h>
#include <gflags/gflags.h>
#include <spdlog/spdlog.h>

#include "include/cachegroup.h"
#include "include/cacheserver.h"
#include "include/peer.h"

DEFINE_int32(port, 8001, "port");
DEFINE_string(node, "A", "node");
DEFINE_string(etcd_endpoints, "http://127.0.0.1:2379", "etcd endpoints");

std::unordered_map<std::string, std::string> db = {
    {"Tom", "Tom"},  {"Jack", "Jack"},  {"Alice", "Alice"},
    {"Bob", "Bob"}, {"Charlie", "Charlie"}, {"Diana", "Diana"}
};

std::function<void(int)> handler;
void HandleCtrlC(int signum) { handler(signum); }

int main(int argc, char** argv){
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[node][%^%l%$] %v")

    std::string addr = std::to_string(FLAGS_port);
    std::string service_name = "cache" + FLAGS_node;
    spdlog::info("Starting {} on {}", service_name, addr);

    try {
        ServerOptions opts;
        opts.etcd_endpoints = {FLAGS_etcd_endpoints};
        auto node = make_unique<CacheServer>(addr, service_name, opts);

        std::thread server_thread{[&] {
            try{
                node->Start();
            } catch (const std::exception& e) {
                spdlog::error("Exception in server thread: {}", e.what());
                std::exit(1);
            }
        }};

        handler = [&] (int signum) {
            if(signum == SIGINT) {
                spdlog::info("Received SIGINT, shutting down...");
                node->Stop();
            }
        };
        std::signal(SIGINT, HandleCtrlC);

        auto& CacheGroup = CacheGroup<std::string>::CreateCacheGroup(
            "test",
            [&](const std::string& key) -> std::string {
                spdlog::info("Cache miss for key: {}", key);
                if (db.find(key) != db.end()) {
                    return db[key];
                }
                spdlog::warn("Key {} not found in database", key);
                return "";
            },
            service_name,
            addr,
            FLAGS_etcd_endpoints
        );

        spdlog::info("[node{}] service running, press Ctrl+C to exit...", FLAGS_node);

        if(server_thread.joinable()){
            server_thread.join();
        }
    } catch (const std::exception& e) {
        spdlog::error("Exception in main: {}", e.what());
        std::exit(1);
    }
    return 0;
}