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
 * @brief PeerPicker class for managing peers in local node.
 * 
 * It uses etcd for service discovery and consistent hashing for peer selection.
 * This class handles peer management, service discovery, and load balancing 
 * across multiple cache nodes in a distributed system.
 */
class PeerPicker {
public:
    /**
     * @brief Construct a PeerPicker with etcd configuration.
     * 
     * @param etcd_prefix The prefix for service keys in etcd.
     * @param etcd_key The specific key for this service instance.
     * @param etcd_endpoints Comma-separated list of etcd endpoints.
     */
    PeerPicker(const std::string& etcd_prefix, const std::string& etcd_key, const std::string& etcd_endpoints);
    
    /**
     * @brief Destructor that cleans up etcd watchers and connections.
     */
    ~PeerPicker();

    /**
     * @brief Select a peer to handle a given key using consistent hashing.
     * 
     * @param key The key for which to select a peer.
     * @return Pointer to the selected peer, or nullptr if no peers are available.
     */
    Peer* PickPeer(const std::string& key);

private:
    /**
     * @brief Initialize service discovery and start watching for changes.
     * 
     * @return True if discovery was successfully started, false otherwise.
     */
    bool StartDiscovery();
    
    /**
     * @brief Watch for changes in the etcd service registry.
     * 
     * This method runs in a separate thread and monitors etcd for
     * peer additions, removals, and modifications.
     */
    void WatchChanges();
    
    /**
     * @brief Handle etcd events (peer additions, removals, updates).
     * 
     * @param resp The etcd response containing the events.
     */
    void HandleEvents(const etcd::Response& resp);
    
    /**
     * @brief Fetch all currently registered services from etcd.
     * 
     * @return True if services were successfully fetched, false otherwise.
     */
    bool FetchAllServices();
    
    /**
     * @brief Add a peer with the given address to the peer pool.
     * 
     * @param addr The network address of the peer to add.
     */
    void Set(const std::string& addr);
    
    /**
     * @brief Remove a peer with the given address from the peer pool.
     * 
     * @param addr The network address of the peer to remove.
     */
    void Remove(const std::string& addr);
    
    /**
     * @brief Extract the network address from an etcd key.
     * 
     * @param key The etcd key containing the address information.
     * @return The extracted network address.
     */
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