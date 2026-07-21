#pragma once

// =============================================================================
// EventDispatcher.h — Virtual Audio Router
// =============================================================================
// Type-safe publish/subscribe event bus.
//
// RESPONSIBILITY:
//   Allow decoupled modules to communicate without knowing about each other.
//   For example, DeviceManager publishes a DeviceConnected event; AudioRouter
//   subscribes and reconfigures itself automatically.
//
// INPUTS:
//   - Subscribers register a callable with Subscribe<EventType>(handler).
//   - Publishers call Publish(event) from any thread.
//
// OUTPUTS:
//   - All registered handlers for that event type are invoked synchronously
//     on the publishing thread (or via the Python polling queue for UI events).
//
// DESIGN:
//   Uses std::type_index as the map key so event types are identified at
//   compile time with no string overhead. The map value is a vector of
//   type-erased std::function<void(const void*)> wrappers.
//
// THREADING:
//   - Subscribe/Unsubscribe are NOT safe to call concurrently with Publish.
//     Registration should happen during initialization before any threads start.
//   - Publish IS safe to call from multiple threads simultaneously (shared_mutex).
//
// FUTURE:
//   - Add async dispatch (post to a queue, process on a specific thread).
//   - Add event priority levels.
// =============================================================================

#include <functional>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <any>
#include <cstdint>

namespace var {

// ---------------------------------------------------------------------------
// Standard events (published by engine modules)
// ---------------------------------------------------------------------------

struct EvEngineInitialized   {};
struct EvEngineShutdown      {};

struct EvDeviceConnected     { std::string deviceId; };
struct EvDeviceDisconnected  { std::string deviceId; };
struct EvDeviceStateChanged  { std::string deviceId; uint32_t newState; };

struct EvRoutingStarted      { std::string inputId; std::vector<std::string> outputIds; };
struct EvRoutingStopped      {};
struct EvRoutingError        { std::string message; };

struct EvLogMessage          { /* LogEntry forwarded to Python */ };

// ---------------------------------------------------------------------------
// EventDispatcher
// ---------------------------------------------------------------------------

class EventDispatcher {
public:
    using SubscriberId = uint64_t;

    EventDispatcher()  = default;
    ~EventDispatcher() = default;

    // Non-copyable (owns handler maps)
    EventDispatcher(const EventDispatcher&)            = delete;
    EventDispatcher& operator=(const EventDispatcher&) = delete;

    // -------------------------------------------------------------------------
    // Registration
    // -------------------------------------------------------------------------

    /// Register a handler for EventType. Returns a SubscriberId for later removal.
    template<typename EventType>
    SubscriberId Subscribe(std::function<void(const EventType&)> handler) {
        std::unique_lock lock(m_mutex);
        auto key = std::type_index(typeid(EventType));
        SubscriberId id = m_nextId++;

        m_handlers[key].push_back({
            id,
            [h = std::move(handler)](const std::any& ev) {
                h(std::any_cast<const EventType&>(ev));
            }
        });

        return id;
    }

    /// Remove a previously registered handler.
    void Unsubscribe(SubscriberId id) {
        std::unique_lock lock(m_mutex);
        for (auto& [key, list] : m_handlers) {
            auto it = std::find_if(list.begin(), list.end(),
                [id](const Handler& h) { return h.id == id; });
            if (it != list.end()) {
                list.erase(it);
                return;
            }
        }
    }

    // -------------------------------------------------------------------------
    // Publishing
    // -------------------------------------------------------------------------

    /// Publish an event to all subscribers of EventType.
    /// Handlers are called synchronously on the calling thread.
    template<typename EventType>
    void Publish(const EventType& event) {
        std::shared_lock lock(m_mutex);
        auto key = std::type_index(typeid(EventType));
        auto it = m_handlers.find(key);
        if (it == m_handlers.end()) return;

        std::any wrapped = event;
        for (const auto& handler : it->second) {
            handler.fn(wrapped);
        }
    }

    // -------------------------------------------------------------------------
    // Utility
    // -------------------------------------------------------------------------

    /// Remove all handlers (called during Shutdown)
    void Clear() {
        std::unique_lock lock(m_mutex);
        m_handlers.clear();
    }

private:
    struct Handler {
        SubscriberId                       id;
        std::function<void(const std::any&)> fn;
    };

    mutable std::shared_mutex                                        m_mutex;
    std::unordered_map<std::type_index, std::vector<Handler>>        m_handlers;
    std::atomic<SubscriberId>                                        m_nextId { 1 };
};

} // namespace var
