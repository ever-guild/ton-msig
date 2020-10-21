#pragma once

#include <tonlib/Stuff.h>

#include <ftabi/Abi.hpp>
#include <nlohmann/json.hpp>

namespace app
{
using EncodedMessage = std::tuple<ftabi::FunctionRef, td::Ref<vm::Cell>, td::Ref<vm::Cell>>;

struct ActionBase {
    virtual auto create_message() -> td::Result<EncodedMessage> = 0;
    virtual auto handle_prepared(const td::Ref<vm::Cell>& message) -> td::Status { return td::Status::OK(); };
    virtual auto handle_result(std::vector<ftabi::ValueRef>&& result) -> td::Status = 0;
    virtual void handle_error(td::Status error) = 0;

    [[nodiscard]] virtual auto created_at() const -> td::uint64 { return 0u; }
    [[nodiscard]] virtual auto expires_at() const -> td::uint32 { return std::numeric_limits<td::uint32>::max(); }
    [[nodiscard]] virtual auto is_get_method() const -> bool = 0;

    template <typename T>
    auto as() -> T&
    {
        static_assert(std::is_base_of_v<ActionBase, T>);
        return *dynamic_cast<T*>(this);
    }

    template <typename T>
    auto as() const -> const T&
    {
        static_assert(std::is_base_of_v<ActionBase, T>);
        return *dynamic_cast<const T*>(this);
    }
};

template <typename R>
struct Action : ActionBase {
    using Result = R;
    using Handler = td::Promise<R>;

    explicit Action(td::Promise<R>&& promise)
        : promise(std::move(promise)){};

    void handle_error(td::Status error) final { promise.set_error(error.move_as_error()); }

    td::Promise<R> promise;
};

namespace msig
{
struct Parameters {
    td::uint8 max_queued_transactions{};
    td::uint8 max_custodian_count{};
    td::uint64 expiration_time{};
    td::BigInt256 min_value{};
    td::uint8 required_txn_confirms{};
};

void to_json(nlohmann::json& j, const Parameters& v);

struct Transaction {
    td::uint64 id{};
    td::uint32 confirmationMask{};
    td::uint8 signsRequired{};
    td::uint8 signsReceived{};
    td::BigInt256 creator{};
    td::uint8 index{};
    block::StdAddress dest{};
    td::BigInt256 value{};
    td::uint16 sendFlags{};
    bool bounce{};
};

void to_json(nlohmann::json& j, const Transaction& v);

struct Custodian {
    td::uint8 index{};
    td::BigInt256 pubkey{};
};

void to_json(nlohmann::json& j, const Custodian& v);

struct TransactionSent {
    td::uint64 transactionId{};
};

void to_json(nlohmann::json& j, const TransactionSent& v);

struct Confirmation {
    bool confirmed{};
};

void to_json(nlohmann::json& j, const Confirmation& v);

struct Constructor final : Action<std::nullopt_t> {
    explicit Constructor(
        Handler&& promise,
        bool force_local,
        td::uint64 time,
        td::uint32 expire,
        std::vector<td::BigInt256>&& owners,
        td::uint8 req_confirms,
        const td::Ed25519::PrivateKey& private_key,
        std::optional<std::string> msg_info_path);

    static auto output_type() -> std::vector<ftabi::ParamRef> { return {}; }
    auto create_message() -> td::Result<EncodedMessage> final;
    auto handle_prepared(const td::Ref<vm::Cell>& message) -> td::Status final;
    auto handle_result(std::vector<ftabi::ValueRef>&& result) -> td::Status final;

    [[nodiscard]] auto created_at() const -> td::uint64 final { return time_; }
    [[nodiscard]] auto expires_at() const -> td::uint32 final { return expire_; }
    [[nodiscard]] auto is_get_method() const -> bool final { return force_local_; }

    bool force_local_;
    td::uint64 time_;
    td::uint32 expire_;
    std::vector<td::BigInt256> owners_;
    td::uint8 req_confirms_;
    td::Ed25519::PrivateKey private_key_;
    std::optional<std::string> msg_info_path_;
};

struct SubmitTransaction final : Action<TransactionSent> {
    explicit SubmitTransaction(
        Handler&& promise,
        bool force_local,
        td::uint64 time,
        td::uint32 expire,
        const block::StdAddress& dest,
        const td::BigInt256& value,
        bool bounce,
        bool all_balance,
        td::Ref<vm::Cell> payload,
        const td::Ed25519::PrivateKey& private_key,
        std::optional<std::string> msg_info_path);

    static auto output_type() -> ftabi::ParamRef;
    auto create_message() -> td::Result<EncodedMessage> final;
    auto handle_prepared(const td::Ref<vm::Cell>& message) -> td::Status final;
    auto handle_result(std::vector<ftabi::ValueRef>&& result) -> td::Status final;

    [[nodiscard]] auto created_at() const -> td::uint64 final { return time_; }
    [[nodiscard]] auto expires_at() const -> td::uint32 final { return expire_; }
    [[nodiscard]] auto is_get_method() const -> bool final { return force_local_; }

    bool force_local_;
    td::uint64 time_;
    td::uint32 expire_;
    block::StdAddress dest_;
    td::BigInt256 value_;
    bool bounce_;
    bool all_balance_;
    td::Ref<vm::Cell> payload_;
    td::Ed25519::PrivateKey private_key_;
    std::optional<std::string> msg_info_path_;
};

struct ConfirmTransaction final : Action<std::nullopt_t> {
    explicit ConfirmTransaction(
        Handler&& promise,
        bool force_local,
        td::uint64 time,
        td::uint32 expire,
        td::uint64 transaction_id,
        const td::Ed25519::PrivateKey& private_key,
        std::optional<std::string> msg_info_path);

    static auto output_type() -> std::vector<ftabi::ParamRef> { return {}; }
    auto create_message() -> td::Result<EncodedMessage> final;
    auto handle_prepared(const td::Ref<vm::Cell>& message) -> td::Status final;
    auto handle_result(std::vector<ftabi::ValueRef>&& result) -> td::Status final;

    [[nodiscard]] auto created_at() const -> td::uint64 final { return time_; }
    [[nodiscard]] auto expires_at() const -> td::uint32 final { return expire_; }
    [[nodiscard]] auto is_get_method() const -> bool final { return force_local_; }

    bool force_local_;
    td::uint64 time_;
    td::uint32 expire_;
    td::uint64 transaction_id_;
    td::Ed25519::PrivateKey private_key_;
    std::optional<std::string> msg_info_path_;
};

struct IsConfirmed final : Action<Confirmation> {
    explicit IsConfirmed(Handler&& promise, td::uint32 mask, td::uint8 index);

    static auto output_type() -> ftabi::ParamRef;
    auto create_message() -> td::Result<EncodedMessage> final;
    auto handle_result(std::vector<ftabi::ValueRef>&& result) -> td::Status final;

    [[nodiscard]] auto is_get_method() const -> bool final { return true; };

    td::uint32 mask_;
    td::uint8 index_;
};

struct GetParameters final : Action<Parameters> {
    explicit GetParameters(Handler&& promise);

    static auto output_type() -> std::vector<ftabi::ParamRef>;
    auto create_message() -> td::Result<EncodedMessage> final;
    auto handle_result(std::vector<ftabi::ValueRef>&& result) -> td::Status final;

    [[nodiscard]] auto is_get_method() const -> bool final { return true; };
};

struct GetTransaction final : Action<Transaction> {
    explicit GetTransaction(Handler&& promise, td::uint64 transaction_id);

    static auto output_type() -> ftabi::ParamRef;
    auto create_message() -> td::Result<EncodedMessage> final;
    auto handle_result(std::vector<ftabi::ValueRef>&& result) -> td::Status final;

    [[nodiscard]] auto is_get_method() const -> bool final { return true; };

    td::uint64 transaction_id_;
};

struct GetTransactions final : Action<std::vector<Transaction>> {
    explicit GetTransactions(Handler&& promise);

    static auto output_type() -> ftabi::ParamRef;
    auto create_message() -> td::Result<EncodedMessage> final;
    auto handle_result(std::vector<ftabi::ValueRef>&& result) -> td::Status final;

    [[nodiscard]] auto is_get_method() const -> bool final { return true; };
};

struct GetTransactionIds final : Action<std::vector<td::uint64>> {
    explicit GetTransactionIds(Handler&& promise);

    static auto output_type() -> ftabi::ParamRef;
    auto create_message() -> td::Result<EncodedMessage> final;
    auto handle_result(std::vector<ftabi::ValueRef>&& result) -> td::Status final;

    [[nodiscard]] auto is_get_method() const -> bool final { return true; };
};

struct GetCustodians final : Action<std::vector<Custodian>> {
    explicit GetCustodians(Handler&& promise);

    static auto output_type() -> ftabi::ParamRef;
    auto create_message() -> td::Result<EncodedMessage> final;
    auto handle_result(std::vector<ftabi::ValueRef>&& result) -> td::Status final;

    [[nodiscard]] auto is_get_method() const -> bool final { return true; };
};

}  // namespace msig

}  // namespace app
