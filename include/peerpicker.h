#ifndef PEER_PICKER_H
#define PEER_PICKER_H

#include "include/peer.h"
#include "kcache.grpc.pb.h"
#include "include/ConsistentHash.h"

#include <fmt/core.h>
#include <grpcpp/channel.h>
#include <grpcpp/grpcpp.h>

#include <etcd/Client.hpp>
#include <etcd/Response.hpp>
#include <memory>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

/**
 * @brief PeerPicker class for managing peers in local node 
 * It uses etcd for service discovery and consistent hashing for peer selection.
 */

class PeerPicker {
public:
    PeerPicker(const std::string& etcd_prefix, const std::string& etcd_key, const std::string& etcd_endpoints);
    ~PeerPicker();

    Peer* PickPeer(const std::string& key);

private:

    bool StartDiscovery();
    void WatchChanges();
    void HandleEvents(const etcd::Response& resp);
    bool FetchAllServices();
    void Set(const std::string& addr);
    void Remove(const std::string& addr);
    std::string ParseAddrFromKey(const std::string& key);

    std::shared_mutex mtx;
    std::unordered_map<std::string, std::shared_ptr<peer>> peers;
    ConsistentHash hash_ring;
    std::shared_ptr<etcd::Client> etcd_client;
    std::unique_ptr<etcd::Watcher> watcher;
    std::thread watcher_thread;
    std::string etcd_prefix;
    std::string etcd_key;
    
};
#endif // PEER_PICKER_H