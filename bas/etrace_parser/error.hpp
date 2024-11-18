/*
 * Monadic Errorable type with type erasure and static type information for classes derived from
 * AbstractError.
 *
 * (Thanks LLVM and Rust)
 */

#pragma once

#include <cstring>
#include <memory>
#include <exception>
#include <string>
#include <string_view>
#include <optional>

#include "tags.hpp"

class AbstractError {
private:
    static char m_identifier;
protected:
    std::string m_error_message;
public:
    virtual ~AbstractError() = default;

    std::string_view explain(void) {
        return m_error_message;
    }

    static const void *identifier() {
        return &m_identifier;
    }

    template <typename T>
    bool compare_to() {
        return identifier() == T::identifier();
    }
};

template <typename T>
class BasicError : public AbstractError {
public:
    using AbstractError::AbstractError;

    static const void *identifier() {
        return &T::m_identifier;
    }
};

class BadSeparatorError : public BasicError<BadSeparatorError> {
public:
    BadSeparatorError(size_t line_number, const char *line) {
        m_error_message = "err: No separator on line " + std::to_string(line_number) + ": " + line + '\n';
    }

    static char m_identifier;
};

class IntegerParseError : public BasicError<IntegerParseError> {
private:
    static char m_identifier;
public:
    IntegerParseError(size_t line_number, const char *line, int err_num) {
        m_error_message = "err: Error while parsing an integer on line " + std::to_string(line_number)
            + " " + std::strerror(err_num) + ": " + line + "\n";
    }
};

class BadFormatError : public BasicError<BadFormatError> {
public:
    BadFormatError(size_t line_number)
    {
        m_error_message = "err: Invalid format on line: " + std::to_string(line_number) + "\n";
    }

    static char m_identifier;
};

class UnexpectedArgumentEndError : public BasicError<UnexpectedArgumentEndError> {
public:
    UnexpectedArgumentEndError(size_t line_number)
    {
        m_error_message = "err: Unexpected end of arguments on line: " + std::to_string(line_number) + "\n";
    }

    static char m_identifier;
};

class SizeMismatchError : public BasicError<SizeMismatchError> {
public:
    SizeMismatchError(size_t line_number, size_t got, size_t expected)
    {
        m_error_message = "err: Size mismatch on line " +
            std::to_string(line_number) + ": expected " + std::to_string(expected) + " got " +
            std::to_string(got) + '\n';
    }
};

class UnexpectedTagError : public BasicError<UnexpectedTagError> {
public:
    UnexpectedTagError(size_t line_number, Tag tag)
    {
        (void) tag;
        m_error_message = "err: Expected different tag on line " + std::to_string(line_number) + '\n';
    }

    UnexpectedTagError(size_t line_number, ShortArguments tag)
    {
        (void) tag;
        m_error_message = "err: Expected different tag on line " + std::to_string(line_number) + '\n';
    }

    static char m_identifier;
};

template <typename T>
class Errorable {
    friend Errorable<void>;
private:
    std::unique_ptr<T> m_value_ptr;
    std::unique_ptr<AbstractError> m_error_ptr;
    bool m_error;
public:
    Errorable() = default;
    Errorable(T t)
        : m_value_ptr (std::make_unique<T>(std::move(t)))
        , m_error_ptr ()
        , m_error (false)
    {};

    template <typename U>
    Errorable(U u)
        : m_value_ptr ()
        , m_error_ptr (std::make_unique<U>(std::move(u)))
        , m_error (true)
    {};

    Errorable(Errorable const& other) = delete;

    template <typename U>
    Errorable(Errorable<U>&& other)
        : m_value_ptr ()
        , m_error_ptr (std::move(other.m_error_ptr))
        , m_error (true)
    {};

    Errorable(Errorable<T>&& other) = default;

    ~Errorable() = default;

    std::string_view explain(void) const {
        if (!m_error)
            throw std::bad_optional_access();

        return m_error_ptr->explain();
    };

    bool is_error(void) {
        return m_error;
    };

    T& value(void) {
        if (m_error || !m_value_ptr.get())
            throw std::bad_optional_access();

        return *m_value_ptr;
    };
};

template <>
class Errorable<void> {
private:
    std::unique_ptr<AbstractError> m_error_ptr;
    bool m_error;
public:
    Errorable() = default;

    template <typename T>
    Errorable(T t)
        : m_error_ptr (std::make_unique<T>(std::move(t)))
        , m_error (true)
    {};

    Errorable(Errorable const& other) = delete;

    template <typename U>
    Errorable(Errorable<U>&& other)
        : m_error_ptr (std::move(other.m_error_ptr))
        , m_error (true)
    {};

    ~Errorable() = default;

    std::string_view explain(void) const {
        if (!m_error || !m_error_ptr.get())
            throw std::bad_optional_access();

        return m_error_ptr->explain();
    };

    bool is_error(void) {
        return m_error;
    };

    void value(void) {
        throw std::bad_optional_access();
    };
};
