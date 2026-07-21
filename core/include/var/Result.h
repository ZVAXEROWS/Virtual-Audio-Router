#pragma once

// =============================================================================
// Result.h — Virtual Audio Router
// =============================================================================
// A lightweight Result<T, E> type modelled after Rust's Result enum.
//
// WHY THIS EXISTS:
//   WASAPI audio threads run at real-time priority. C++ exceptions thrown on a
//   real-time thread that propagate across thread boundaries produce undefined
//   behaviour on Windows (the runtime may call std::terminate). This forces us
//   to handle every error explicitly at the call site — the compiler rejects
//   any code that silently ignores a failure.
//
// USAGE:
//   Result<DeviceInfo, VarError> r = deviceManager.GetDefaultDevice();
//   if (!r) {
//       logger.Error("GetDefaultDevice: {}", r.error().message);
//       return r.propagate<void>();   // forward error up the call stack
//   }
//   DeviceInfo info = r.value();
// =============================================================================

#include <string>
#include <variant>
#include <stdexcept>
#include <functional>
#include <optional>

namespace var {

// ---------------------------------------------------------------------------
// VarError — structured error type
// ---------------------------------------------------------------------------

enum class ErrorCode : uint32_t {
    None             = 0,
    Unknown          = 1,

    // COM / Windows
    ComInitFailed    = 100,
    DeviceNotFound   = 101,
    DeviceAccessDenied = 102,
    WasapiError      = 103,
    InvalidDeviceState = 104,

    // Engine
    EngineNotInitialized = 200,
    AlreadyInitialized   = 201,
    InvalidConfig        = 202,
    StartFailed          = 203,
    StopFailed           = 204,

    // Threading
    ThreadStartFailed = 300,
    QueueFull         = 301,

    // I/O
    ConfigReadFailed  = 400,
    ConfigWriteFailed = 401,
    LogOpenFailed     = 402,
};

struct VarError {
    ErrorCode   code    { ErrorCode::Unknown };
    std::string message {};
    uint32_t    hresult { 0 };  // populated for WASAPI/COM errors

    VarError() = default;

    explicit VarError(ErrorCode c, std::string msg = {}, uint32_t hr = 0)
        : code(c), message(std::move(msg)), hresult(hr) {}

    /// Convenience factory for HRESULT-bearing errors
    static VarError fromHresult(ErrorCode c, uint32_t hr, const std::string& context = {}) {
        return VarError{ c, context + " (HRESULT=0x" + toHex(hr) + ")", hr };
    }

    bool isOk() const { return code == ErrorCode::None; }

private:
    static std::string toHex(uint32_t v) {
        char buf[9];
        snprintf(buf, sizeof(buf), "%08X", v);
        return buf;
    }
};

// ---------------------------------------------------------------------------
// Result<T, E>
// ---------------------------------------------------------------------------

template<typename T, typename E = VarError>
class Result {
public:
    using value_type = T;
    using error_type = E;

    // ---- constructors -------------------------------------------------------

    /// Construct a successful result
    static Result ok(T value) {
        Result r;
        r.m_data = std::move(value);
        return r;
    }

    /// Construct a failed result
    static Result err(E error) {
        Result r;
        r.m_data = std::move(error);
        return r;
    }

    // ---- observers ----------------------------------------------------------

    bool isOk()  const { return std::holds_alternative<T>(m_data); }
    bool isErr() const { return std::holds_alternative<E>(m_data); }

    explicit operator bool() const { return isOk(); }

    /// Access value — throws if in error state (only call after checking isOk())
    T& value() {
        if (!isOk()) throw std::runtime_error("Result::value() called on error state");
        return std::get<T>(m_data);
    }
    const T& value() const {
        if (!isOk()) throw std::runtime_error("Result::value() called on error state");
        return std::get<T>(m_data);
    }

    /// Access error — throws if in ok state
    E& error() {
        if (!isErr()) throw std::runtime_error("Result::error() called on ok state");
        return std::get<E>(m_data);
    }
    const E& error() const {
        if (!isErr()) throw std::runtime_error("Result::error() called on ok state");
        return std::get<E>(m_data);
    }

    /// Unwrap with a default value if error
    T valueOr(T defaultVal) const {
        return isOk() ? std::get<T>(m_data) : std::move(defaultVal);
    }

    // ---- transformations ----------------------------------------------------

    /// Map success value; propagate error unchanged
    template<typename U>
    Result<U, E> map(std::function<U(T)> fn) const {
        if (isOk()) return Result<U, E>::ok(fn(std::get<T>(m_data)));
        return Result<U, E>::err(std::get<E>(m_data));
    }

    /// Propagate error into a different result type (loses value)
    template<typename U>
    Result<U, E> propagate() const {
        return Result<U, E>::err(std::get<E>(m_data));
    }

private:
    std::variant<T, E> m_data;
    Result() = default;
};

// ---------------------------------------------------------------------------
// Specialisation: Result<void, E>
// ---------------------------------------------------------------------------

template<typename E>
class Result<void, E> {
public:
    using value_type = void;
    using error_type = E;

    static Result ok() {
        Result r;
        r.m_ok = true;
        return r;
    }
    static Result err(E error) {
        Result r;
        r.m_ok    = false;
        r.m_error = std::move(error);
        return r;
    }

    bool isOk()  const { return m_ok; }
    bool isErr() const { return !m_ok; }
    explicit operator bool() const { return m_ok; }

    E& error() {
        if (m_ok) throw std::runtime_error("Result::error() called on ok state");
        return *m_error;
    }
    const E& error() const {
        if (m_ok) throw std::runtime_error("Result::error() called on ok state");
        return *m_error;
    }

    template<typename U>
    Result<U, E> propagate() const {
        return Result<U, E>::err(*m_error);
    }

private:
    bool                 m_ok    { false };
    std::optional<E>     m_error {};
    Result() = default;
};

// ---------------------------------------------------------------------------
// Convenience aliases
// ---------------------------------------------------------------------------

using VoidResult = Result<void, VarError>;

/// Helper macro: return early if result is an error
#define VAR_TRY(expr)                         \
    do {                                       \
        auto _r = (expr);                      \
        if (!_r) return _r.propagate<void>();  \
    } while (0)

} // namespace var
