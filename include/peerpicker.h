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
 * @brief PeerPicker class for managing peers in the local node.
 * 
 * PeerPicker is responsible for peer management in a distributed cache system.
 * It uses etcd for service discovery and consistent hashing for peer selection.
 * This class handles peer management, service discovery, and load balancing 
 * across multiple cache nodes in a distributed system. It automatically detects
 * when peers join or leave the cluster and updates the hash ring accordingly.
 */
class PeerPicker {
public:
    /**
     * @brief Construct a PeerPicker with etcd configuration.
     * 
     * @param service_name_ The prefix for service keys in etcd.
     * @param etcd_key The specific key for this service instance.
     * @param etcd_endpoints Comma-separated list of etcd endpoints.
     */
    PeerPicker(const std::string& service_name_, const std::string& etcd_key, const std::string& etcd_endpoints);
    
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

    std::shared_mutex mtx; ///< Reader-writer mutex for thread-safe peer management.
    std::unordered_map<std::string, std::shared_ptr<peer>> peers; ///< Map of peer addresses to peer instances.
    ConsistentHash hash_ring; ///< Consistent hash ring for peer selection.
    std::shared_ptr<etcd::Client> etcd_client; ///< etcd client for service discovery.
    std::unique_ptr<etcd::Watcher> watcher; ///< etcd watcher for monitoring service changes.
    std::thread watcher_thread; ///< Thread for running the etcd watcher.
    std::string service_name_; ///< The service name prefix for etcd keys.
    std::string etcd_key; ///< The specific etcd key for this service instance.
    
};
#endif // PEER_PICKER_H