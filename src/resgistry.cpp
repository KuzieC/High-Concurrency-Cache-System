#include "include/resgistry.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <unistd.h>

#include <chrono>

#include <fmt/core.h>
#include <spdlog/spdlog.h>
#include <etcd/KeepAlive.hpp>
#include <etcd/v3/Transaction.hpp>

etcdRegistry::etcdRegistry(const std::string &endpoint) {
    etcd_client_ = std::make_unique<etcd::Client>(endpoint);
}

etcdRegistry::~etcdRegistry() = default;

bool etcdRegistry::Register(const std::string &service_name, const std::string &service_addr) {

    etcd::Response lease_response = etcd_client_->leasegrant(10).get();
    if (!lease_response.is_ok()) {
        spdlog::error("Failed to create lease: {}", lease_response.error_message());
        return false;
    }
    lease_id_ = lease_response.value().as_lease();
    etcd_addr_ = service_name + "/" + service_addr;
    etcd::Response put_response = etcd_client_->put(etcd_addr_, service_addr, lease_id_).get();
    if (!put_response.is_ok()) {
        spdlog::error("Failed to register service: {}", put_response.error_message());
        return false;
    }

    keep_alive_ = std::make_unique<etcd::KeepAlive>(*etcd_client_, 10, lease_id_);
    
    stop_ = false; 
    keepalive_thread_ = std::thread{
        [this] {
            this->KeepAlive();
        }
    };
    spdlog::info("Service registered: {} -> {}", etcd_addr_, service_addr);
    return true;
}

void etcdRegistry::Unregister() {
    stop_ = true;
    if (keep_alive_) {
        keep_alive_->Cancel();  
        keep_alive_.reset();
    }
    if (keepalive_thread_.joinable()) {
        keepalive_thread_.join();
    }
    if (lease_id_ != 0) {
        etcd::Response revoke_response = etcd_client_->leaserevoke(lease_id_).get();
        if (!revoke_response.is_ok()) {
            spdlog::error("Failed to revoke lease: {}", revoke_response.error_message());
        } else {
            spdlog::info("Lease revoked successfully");
        }
        lease_id_ = 0;
    }
}

void etcdRegistry::KeepAlive() {
    while (!stop_) {
        if (keep_alive_) {
            try {
                keep_alive_->Check(); 
                spdlog::debug("Keep-alive check successful");
            } catch (const std::exception& e) {
                spdlog::error("Keep-alive failed: {}", e.what());
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}