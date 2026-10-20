#pragma once

#include <string>
#include <variant>
#include <utility>
#include <type_traits>
#include <concepts>
#include <optional>
#include <system_error>
#include <stdexcept>
#include <cstdint>

#if __has_include(<source_location>)
#include <source_location>
#define ZENITH_HAS_SOURCE_LOCATION 1
#else
#define ZENITH_HAS_SOURCE_LOCATION 0
#endif

namespace zenith {

enum class ErrorCode : uint16_t {
    Success = 0,
    InvalidArgument,
    NotFound,
    PermissionDenied,
    IoError,
    SystemError,
    Timeout,
    AlreadyExists,
    Conflict,
    RollbackFailed,
    SubsystemFailed,
    NotSupported,
    CorruptedState,
    InternalError
};

[[nodiscard]] constexpr const char* error_code_to_string(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Success: return "Success";
        case ErrorCode::InvalidArgument: return "InvalidArgument";
        case ErrorCode::NotFound: return "NotFound";
        case ErrorCode::PermissionDenied: return "PermissionDenied";
        case ErrorCode::IoError: return "IoError";
        case ErrorCode::SystemError: return "SystemError";
        case ErrorCode::Timeout: return "Timeout";
        case ErrorCode::AlreadyExists: return "AlreadyExists";
        case ErrorCode::Conflict: return "Conflict";
        case ErrorCode::RollbackFailed: return "RollbackFailed";
        case ErrorCode::SubsystemFailed: return "SubsystemFailed";
        case ErrorCode::NotSupported: return "NotSupported";
        case ErrorCode::CorruptedState: return "CorruptedState";
        case ErrorCode::InternalError: return "InternalError";
        default: return "UnknownError";
    }
}

struct Error {
    ErrorCode code{ErrorCode::InternalError};
    std::string message;
    int system_errno{0};
    const char* file{""};
    int line{0};
    const char* function{""};

    Error() = default;

#if ZENITH_HAS_SOURCE_LOCATION
    Error(ErrorCode c, std::string msg, int sys_errno = 0,
          std::source_location loc = std::source_location::current())
        : code(c),
          message(std::move(msg)),
          system_errno(sys_errno),
          file(loc.file_name()),
          line(static_cast<int>(loc.line())),
          function(loc.function_name()) {}
#else
    Error(ErrorCode c, std::string msg, int sys_errno = 0,
          const char* f = "", int l = 0, const char* fn = "")
        : code(c),
          message(std::move(msg)),
          system_errno(sys_errno),
          file(f),
          line(l),
          function(fn) {}
#endif

    [[nodiscard]] std::string to_string() const {
        std::string s = "[";
        s += error_code_to_string(code);
        s += "] ";
        s += message;
        if (system_errno != 0) {
            s += " (errno: " + std::to_string(system_errno) + " - " +
                 std::generic_category().message(system_errno) + ")";
        }
        if (file && file[0] != '\0') {
            s += " at " + std::string(file) + ":" + std::to_string(line);
        }
        return s;
    }
};

// Unit type representing void in Result<void, E>
struct Unit {
    constexpr bool operator==(const Unit&) const noexcept = default;
};

template <typename T, typename E = Error>
class Result {
    static_assert(!std::is_same_v<T, E>, "T and E cannot be identical types in Result");

public:
    using value_type = T;
    using error_type = E;

    // Constructors
    constexpr Result(const T& val) : storage_(val) {}
    constexpr Result(T&& val) noexcept(std::is_nothrow_move_constructible_v<T>)
        : storage_(std::move(val)) {}

    constexpr Result(const E& err) : storage_(err) {}
    constexpr Result(E&& err) noexcept(std::is_nothrow_move_constructible_v<E>)
        : storage_(std::move(err)) {}

    static constexpr Result ok(T val) noexcept(std::is_nothrow_move_constructible_v<T>) {
        return Result(std::move(val));
    }
    static constexpr Result err(E error_val) noexcept(std::is_nothrow_move_constructible_v<E>) {
        return Result(std::move(error_val));
    }

    [[nodiscard]] constexpr bool has_value() const noexcept {
        return std::holds_alternative<T>(storage_);
    }
    [[nodiscard]] constexpr bool is_ok() const noexcept { return has_value(); }
    [[nodiscard]] constexpr bool is_err() const noexcept { return !has_value(); }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] constexpr const T& value() const& {
        return std::get<T>(storage_);
    }
    [[nodiscard]] constexpr T& value() & {
        return std::get<T>(storage_);
    }
    [[nodiscard]] constexpr T&& value() && {
        return std::get<T>(std::move(storage_));
    }

    [[nodiscard]] constexpr const T* operator->() const noexcept {
        return &std::get<T>(storage_);
    }
    [[nodiscard]] constexpr T* operator->() noexcept {
        return &std::get<T>(storage_);
    }
    [[nodiscard]] constexpr const T& operator*() const& noexcept {
        return std::get<T>(storage_);
    }
    [[nodiscard]] constexpr T& operator*() & noexcept {
        return std::get<T>(storage_);
    }
    [[nodiscard]] constexpr T&& operator*() && noexcept {
        return std::get<T>(std::move(storage_));
    }

    template <typename U>
    [[nodiscard]] constexpr T value_or(U&& default_val) const& {
        return has_value() ? std::get<T>(storage_) : static_cast<T>(std::forward<U>(default_val));
    }

    [[nodiscard]] constexpr const E& error() const& {
        return std::get<E>(storage_);
    }
    [[nodiscard]] constexpr E& error() & {
        return std::get<E>(storage_);
    }
    [[nodiscard]] constexpr E&& error() && {
        return std::get<E>(std::move(storage_));
    }

    // Monadic .map(Fn)
    template <typename Fn>
    auto map(Fn&& fn) const& {
        using ReturnType = std::invoke_result_t<Fn, const T&>;
        if (has_value()) {
            return Result<ReturnType, E>(std::forward<Fn>(fn)(std::get<T>(storage_)));
        }
        return Result<ReturnType, E>(std::get<E>(storage_));
    }

    template <typename Fn>
    auto map(Fn&& fn) && {
        using ReturnType = std::invoke_result_t<Fn, T&&>;
        if (has_value()) {
            return Result<ReturnType, E>(std::forward<Fn>(fn)(std::get<T>(std::move(storage_))));
        }
        return Result<ReturnType, E>(std::get<E>(std::move(storage_)));
    }

    // Monadic .map_error(Fn)
    template <typename Fn>
    auto map_error(Fn&& fn) const& {
        using NewErrorType = std::invoke_result_t<Fn, const E&>;
        if (has_value()) {
            return Result<T, NewErrorType>(std::get<T>(storage_));
        }
        return Result<T, NewErrorType>(std::forward<Fn>(fn)(std::get<E>(storage_)));
    }

    // Monadic .and_then(Fn)
    template <typename Fn>
    auto and_then(Fn&& fn) const& {
        using SubResult = std::invoke_result_t<Fn, const T&>;
        if (has_value()) {
            return std::forward<Fn>(fn)(std::get<T>(storage_));
        }
        return SubResult(std::get<E>(storage_));
    }

    template <typename Fn>
    auto and_then(Fn&& fn) && {
        using SubResult = std::invoke_result_t<Fn, T&&>;
        if (has_value()) {
            return std::forward<Fn>(fn)(std::get<T>(std::move(storage_)));
        }
        return SubResult(std::get<E>(std::move(storage_)));
    }

    // Monadic .or_else(Fn)
    template <typename Fn>
    Result or_else(Fn&& fn) const& {
        if (!has_value()) {
            return std::forward<Fn>(fn)(std::get<E>(storage_));
        }
        return *this;
    }

private:
    std::variant<T, E> storage_;
};

// Convenient Result<void> specialization using Unit
template <typename E>
class Result<void, E> {
public:
    using value_type = void;
    using error_type = E;

    constexpr Result() : inner_(Unit{}) {}
    constexpr Result(const E& err) : inner_(err) {}
    constexpr Result(E&& err) noexcept(std::is_nothrow_move_constructible_v<E>)
        : inner_(std::move(err)) {}

    [[nodiscard]] constexpr bool has_value() const noexcept { return inner_.has_value(); }
    [[nodiscard]] constexpr bool is_ok() const noexcept { return inner_.is_ok(); }
    [[nodiscard]] constexpr bool is_err() const noexcept { return inner_.is_err(); }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return has_value(); }

    constexpr void value() const {
        if (is_err()) {
            // Hot path returns without throwing if checked; if unchecked, throws logic_error
            throw std::logic_error("Attempted to access value() on error Result<void>: " +
                                  inner_.error().to_string());
        }
    }

    [[nodiscard]] constexpr const E& error() const& { return inner_.error(); }
    [[nodiscard]] constexpr E& error() & { return inner_.error(); }
    [[nodiscard]] constexpr E&& error() && { return std::move(inner_).error(); }

    template <typename Fn>
    auto and_then(Fn&& fn) const& {
        using SubResult = std::invoke_result_t<Fn>;
        if (has_value()) {
            return std::forward<Fn>(fn)();
        }
        return SubResult(inner_.error());
    }

    template <typename Fn>
    Result or_else(Fn&& fn) const& {
        if (!has_value()) {
            return std::forward<Fn>(fn)(inner_.error());
        }
        return *this;
    }

    // Static helper to create success
    static constexpr Result ok() noexcept { return Result(); }
    static constexpr Result err(E error_val) noexcept { return Result(std::move(error_val)); }

private:
    Result<Unit, E> inner_;
};

// Helper factory functions
template <typename T>
[[nodiscard]] constexpr Result<std::decay_t<T>, Error> Ok(T&& val) {
    return Result<std::decay_t<T>, Error>(std::forward<T>(val));
}

[[nodiscard]] inline Result<void, Error> Ok() {
    return Result<void, Error>::ok();
}

template <typename E = Error>
[[nodiscard]] inline auto Err(E&& err) {
    return std::forward<E>(err);
}

} // namespace zenith
