#ifndef peer_h
#define peer_h

#include <fmt/core.h>
#include <google/protobuf/any.pb.h> 
#include <grpcpp/channel.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/security/credentials.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <etcd/Client.hpp>
#include <etcd/Response.hpp>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>

#include "cache.grpc.pb.h"

/**
 * @brief Represents a peer cache node in the distributed cache system.
 * 
 * The peer class provides a gRPC client interface to communicate with remote
 * cache nodes. It supports templated get/set operations for different data types
 * and handles automatic connection management, timeouts, and error handling.
 * This class is used by the PeerPicker to route cache operations to the
 * appropriate nodes in the distributed system.
 */
class peer {
public:
    /**
     * @brief Constructs a Peer and establishes a gRPC connection.
     * @param name The address (ip:port) of the peer.
     */
    peer(const std::string& name): name_(name) {
        grpc::ChannelArguments args;
        args.SetInt(GRPC_ARG_INITIAL_RECONNECT_BACKOFF_MS, 100);
        args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 1000);
        channel_ = grpc::CreateCustomChannel(name_, grpc::InsecureChannelCredentials(), args);
        stub_ = cache::Cache::NewStub(channel_);
    }

    /**
     * @brief Gets the value associated with a key in a specific group.
     * 
     * This method sends a gRPC Get request to the peer and deserializes the response
     * based on the template type. Currently supports std::string and int types.
     * 
     * @tparam T The type of the value to retrieve (std::string or int).
     * @param group_name The name of the group.
     * @param key The key to look up.
     * @return An optional containing the value if found, or std::nullopt if not found.
     */
    template<typename T>
    std::optional<T> get(const std::string& group_name, const std::string& key) {
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(3));
        cache::Request request;
        request.set_group(group_name);
        request.set_key(key);
        cache::GetResponse response;
        grpc::Status status = stub_->Get(&context, request, &response);
        if (!status.ok() || !response.has_value()) {
            return std::nullopt;
        }
        if constexpr (std::is_same_v<T, std::string>) {
            google::protobuf::StringValue w;
            if (response.value().UnpackTo(&w)) {
                return w.value();
            }
        } else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, int32_t>) {
            google::protobuf::Int32Value w;
            if (response.value().UnpackTo(&w)) {
                return static_cast<T>(w.value());
            }
        } else {
            static_assert(std::is_same_v<T, void>, "peer::get supports only std::string and int");
        }

        spdlog::error("Failed to unpack response value for key: {} to requested type", key);
        return std::nullopt;
    }

    /**
     * @brief Sets a value for a key in a specific group.
     * 
     * This method sends a gRPC Set request to the peer with the specified value.
     * The value is automatically serialized based on its type using Protocol Buffers.
     * Currently supports std::string and int types.
     * 
     * @tparam T The type of the value to set (std::string or int).
     * @param group_name The name of the group.
     * @param key The key to set.
     * @param value The value to associate with the key.
     * @return True if the operation was successful, false otherwise.
     */
    template<typename T>
    bool set(const std::string& group_name, const std::string& key, const T& value) {
        grpc::ClientContext context;
        cache::Request request;
        request.set_group(group_name);
        request.set_key(key);

        if constexpr (std::is_same_v<T, std::string>) {
            google::protobuf::StringValue w;
            w.set_value(value);
            request.mutable_value()->PackFrom(w);
        } else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, int32_t>) {
            google::protobuf::Int32Value w;
            w.set_value(static_cast<int32_t>(value));
            request.mutable_value()->PackFrom(w);
        } else {
            static_assert(std::is_same_v<T, void>, "peer::set supports only std::string and int");
        }

        cache::SetResponse response;
        grpc::Status status = stub_->Set(&context, request, &response);
        if (!status.ok()) {
            spdlog::error("Set RPC failed for {}:{} — {} (code={})",
                        group_name, key, status.error_message(), static_cast<int>(status.error_code()));
            return false;
        }
        return true;
    }

    /**
     * @brief Deletes a key from a specific group.
     * 
     * This method sends a gRPC Delete request to the peer to remove the specified key.
     * 
     * @param group_name The name of the group.
     * @param key The key to delete.
     * @return True if the operation was successful, false otherwise.
     */
    bool delete_key(const std::string& group_name, const std::string& key) {
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(3));
        cache::Request request;
        request.set_group(group_name);
        request.set_key(key);
        cache::DeleteResponse response;
        grpc::Status status = stub_->Delete(&context, request, &response);
        if (!status.ok()) {
            spdlog::error("Failed to delete key from peer: {}", status.error_message());
            return false;
        }
        return true;
    }

private:
    std::string name_; ///< The network address (host:port) of this peer.
    std::shared_ptr<grpc::Channel> channel_; ///< gRPC channel for communication with the peer.
    std::unique_ptr<cache::Cache::Stub> stub_; ///< gRPC stub for making cache service calls.
};
#endif // peer_h