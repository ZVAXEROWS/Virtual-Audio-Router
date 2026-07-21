// =============================================================================
// var_bindings.cpp — Virtual Audio Router
// =============================================================================
// Exposes the C++ AudioEngine to Python as `import var_engine`.
//
// DESIGN PRINCIPLES:
//   1. Only AudioEngine is exposed — Python never holds raw pointers into
//      any other engine subsystem.
//   2. C++ enums → Python IntEnum (via pybind11 enum_)
//   3. C++ structs → Python dicts via automatic conversion (py::dict)
//      OR pybind11 class_ bindings for strongly-typed access.
//   4. Result<T, E> is unwrapped here: success returns value, error raises
//      a Python exception (RuntimeError) with the VarError message.
//   5. All calls from Python arrive on the Python thread. AudioEngine methods
//      that touch audio threads are designed to be non-blocking.
//
// PYTHON USAGE EXAMPLE:
//   import var_engine
//   engine = var_engine.AudioEngine()
//   engine.initialize("C:/AppData/VAR")
//   for dev in engine.get_devices():
//       print(dev['name'], dev['sample_rate'])
//   engine.shutdown()
// =============================================================================

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include "var/AudioEngine.h"
#include "var/Types.h"
#include "var/Result.h"

namespace py = pybind11;
using namespace var;

// ---------------------------------------------------------------------------
// Helper: unwrap Result<T> — raise Python exception on error
// ---------------------------------------------------------------------------

template<typename T>
T UnwrapResult(Result<T, VarError>&& r, const char* context) {
    if (!r) {
        throw py::value_error(
            std::string(context) + ": " + r.error().message
        );
    }
    return r.value();
}

void UnwrapVoidResult(VoidResult&& r, const char* context) {
    if (!r) {
        throw py::value_error(
            std::string(context) + ": " + r.error().message
        );
    }
}

// ---------------------------------------------------------------------------
// Conversion helpers: C++ structs → Python dicts
// ---------------------------------------------------------------------------

py::dict DeviceInfoToDict(const DeviceInfo& d) {
    py::dict out;
    out["id"]          = d.id;
    out["name"]        = d.name;
    out["description"] = d.description;
    out["state"]       = static_cast<uint32_t>(d.state);
    out["is_default"]  = d.isDefault;
    out["sample_rate"] = d.nativeFormat.sampleRate;
    out["channels"]    = d.nativeFormat.channels;
    out["bits_per_sample"] = d.nativeFormat.bitsPerSample;
    out["latency_ms"]  = d.latencyMs;
    return out;
}

py::dict LogEntryToDict(const LogEntry& e) {
    py::dict out;
    out["level"]        = static_cast<uint32_t>(e.level);
    out["message"]      = e.message;
    out["source"]       = e.source;
    out["timestamp_ms"] = e.timestampMs;
    return out;
}

RouterConfig DictToRouterConfig(const py::dict& d) {
    RouterConfig cfg;
    if (d.contains("input_device_id"))
        cfg.inputDeviceId = d["input_device_id"].cast<std::string>();
    if (d.contains("output_device_ids"))
        cfg.outputDeviceIds = d["output_device_ids"].cast<std::vector<std::string>>();
    if (d.contains("buffer_size_ms"))
        cfg.bufferSizeMs = d["buffer_size_ms"].cast<uint32_t>();
    if (d.contains("enable_resampling"))
        cfg.enableResampling = d["enable_resampling"].cast<bool>();
    if (d.contains("enable_drift_correction"))
        cfg.enableDriftCorrection = d["enable_drift_correction"].cast<bool>();
    return cfg;
}

// ---------------------------------------------------------------------------
// Module definition
// ---------------------------------------------------------------------------

PYBIND11_MODULE(var_engine, m) {
    m.doc() = "Virtual Audio Router C++ engine bindings";

    // ---- Enums -------------------------------------------------------------

    py::enum_<EngineStatus>(m, "EngineStatus")
        .value("Uninitialized", EngineStatus::Uninitialized)
        .value("Initializing",  EngineStatus::Initializing)
        .value("Ready",         EngineStatus::Ready)
        .value("Routing",       EngineStatus::Routing)
        .value("Stopping",      EngineStatus::Stopping)
        .value("Error",         EngineStatus::Error)
        .value("ShuttingDown",  EngineStatus::ShuttingDown)
        .export_values();

    py::enum_<LogLevel>(m, "LogLevel")
        .value("Debug",   LogLevel::Debug)
        .value("Info",    LogLevel::Info)
        .value("Warning", LogLevel::Warning)
        .value("Error",   LogLevel::Error)
        .value("Fatal",   LogLevel::Fatal)
        .export_values();

    // ---- AudioEngine class -------------------------------------------------

    py::class_<AudioEngine>(m, "AudioEngine")
        .def(py::init<>())

        // Lifecycle
        .def("initialize",
            [](AudioEngine& self, const std::string& logDir) {
                UnwrapVoidResult(self.Initialize(logDir), "initialize");
            },
            py::arg("log_directory") = ".",
            "Initialize all engine subsystems. Must be called first.")

        .def("shutdown",
            [](AudioEngine& self) { self.Shutdown(); },
            "Cleanly shut down all subsystems.")

        // Device management
        .def("get_devices",
            [](AudioEngine& self) -> py::list {
                auto result = self.GetDevices();
                if (!result) {
                    throw py::value_error("get_devices: " + result.error().message);
                }
                py::list out;
                for (const auto& dev : result.value()) {
                    out.append(DeviceInfoToDict(dev));
                }
                return out;
            },
            "Return a list of dicts describing all available render devices.")

        .def("get_default_device",
            [](AudioEngine& self) -> py::dict {
                auto result = self.GetDefaultDevice();
                if (!result) {
                    throw py::value_error("get_default_device: " + result.error().message);
                }
                return DeviceInfoToDict(result.value());
            },
            "Return the system default render device as a dict.")

        // Routing
        .def("start_routing",
            [](AudioEngine& self, const py::dict& configDict) {
                RouterConfig cfg = DictToRouterConfig(configDict);
                UnwrapVoidResult(self.StartRouting(cfg), "start_routing");
            },
            py::arg("config"),
            "Start routing audio. config is a dict with keys: "
            "input_device_id, output_device_ids, buffer_size_ms, "
            "enable_resampling, enable_drift_correction.")

        .def("stop_routing",
            [](AudioEngine& self) {
                UnwrapVoidResult(self.StopRouting(), "stop_routing");
            },
            "Stop active audio routing.")

        // Status & diagnostics
        .def("get_status",
            [](const AudioEngine& self) {
                return self.GetStatus();
            },
            "Return current EngineStatus enum value.")

        .def("get_recent_logs",
            [](const AudioEngine& self, uint32_t maxEntries) -> py::list {
                py::list out;
                for (const auto& entry : self.GetRecentLogs(maxEntries)) {
                    out.append(LogEntryToDict(entry));
                }
                return out;
            },
            py::arg("max_entries") = 100,
            "Return up to max_entries recent log entries as a list of dicts.")

        .def("get_current_config",
            [](const AudioEngine& self) -> py::dict {
                RouterConfig cfg = self.GetCurrentConfig();
                py::dict d;
                d["input_device_id"]         = cfg.inputDeviceId;
                d["output_device_ids"]        = cfg.outputDeviceIds;
                d["buffer_size_ms"]           = cfg.bufferSizeMs;
                d["enable_resampling"]        = cfg.enableResampling;
                d["enable_drift_correction"]  = cfg.enableDriftCorrection;
                return d;
            },
            "Return the currently active RouterConfig as a dict.");
}
