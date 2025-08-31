#include "include/peerpicker.h"
#include "include/ConsistentHash.h"

#include <cassert>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <fmt/core.h>
#include <spdlog/spdlog.h>
#include <etcd/Client.hpp>
#include <etcd/Response.hpp>
#include <etcd/Value.hpp>
#include <etcd/Watcher.hpp>

PeerPicker::PeerPicker(const std::string& service_name, const std::string& etcd_key, const std::string& etcd_endpoints)
    : service_name_(service_name), etcd_key(etcd_key), hash_ring(50,10,200,0.25) {
    etcd_client = std::make_shared<etcd::Client>(etcd_endpoints);
    if(!StartDiscovery()) {
        spdlog::error("Failed to start discovery for PeerPicker with etcd endpoints: {}", etcd_endpoints);
        throw std::runtime_error("Failed to start discovery for PeerPicker");
    }
}

PeerPicker::~PeerPicker() {
    watcher->Stop();
    if (watcher_thread.joinable()) {
        watcher_thread.join();
    }
}

Peer* PeerPicker::PickPeer(const std::string& key) {
    std::shared_lock lock(mtx);
    auto peer_name = hash_ring.Get(key);
    if(!peer_name.empty() && peer_name != etcd_key) {
        spdlog::debug("{} picked peer: {}", etcd_key, peer_name);
        return peers[peer_name].get();
    }
    return nullptr;
}

bool PeerPicker::StartDiscovery() {
    if(!FetchAllServices()) {
        spdlog::error("Failed to fetch all services for PeerPicker");
        return false;
    }
    watcher_thread = std::thread([this]() {
        WatchChanges();
    });
    return true;
}

void PeerPicker::WatchChanges() {
    std::string key = service_name_ + "/" + etcd_key;
    watcher = std::make_unique<etcd::Watcher>(*etcd_client, key, [this](const etcd::Response& resp) {
        HandleEvents(resp);
    });
    watcher->Watch(key);
}

void PeerPicker::HandleEvents(const etcd::Response& resp) {
    std::lock_guard lock(mtx);
    if(!resp.is_ok()) {
        spdlog::error("Failed to handle etcd response: {}", resp.error_message());
        return;
    }
    for (const auto& event : resp.events()) {
        std::string key = event.kv().key();
        std::string addr = ParseAddrFromKey(key);
        spdlog::debug("Handling etcd event: {} - {}", event.type(), addr);
        switch (event.type()) {
            case etcd::Event::EventType::PUT: {
                Set(addr);
                break;
            }
            case etcd::Event::EventType::DELETE: {
                Remove(addr);
                break;
            }
        }
    }
}

bool PeerPicker::FetchAllServices() {
    std::lock_guard lock(mtx);
    std::string key = service_name_ + "/" + etcd_key;
    etcd::Response resp = etcd_client->ls(key).get();

    if (!resp.is_ok()) {
        spdlog::error("Failed to fetch all services: {}", resp.error_message());
        return false;
    }

    for (const auto& kv : resp.kvs()) {
        std::string addr = ParseAddrFromKey(kv.key());
        spdlog::debug("Found service: {}", addr);
        Set(addr);
    }
    return true;
}

void PeerPicker::Set(const std::string& addr) {
    std::unique_lock lock(mtx);
    peers[addr] = std::make_shared<peer>(addr);
    hash_ring.AddNode(addr);
}

void PeerPicker::Remove(const std::string& addr) {
    std::unique_lock lock(mtx);
    peers.erase(addr);
    hash_ring.RemoveNode(addr);
}

std::string PeerPicker::ParseAddrFromKey(const std::string& key) {
    if (key.starts_with(service_name_)) {
        return key.substr(service_name_.length()+1);
    }
    return "";
}
