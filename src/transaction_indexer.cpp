#include <bitcoin/transaction_indexer.hpp>

#include <bitcoin/constants.hpp>
#include <bitcoin/transaction.hpp>
#include <bitcoin/utility/assert.hpp>
#include <bitcoin/utility/logger.hpp>
#include <bitcoin/format.hpp>

namespace libbitcoin {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

transaction_indexer::transaction_indexer(threadpool& pool)
  : strand_(pool)
{
    log_debug(LOG_TXIDX) << "transaction_indexer()";
}

void transaction_indexer::query(const payment_address& payaddr,
    query_handler handle_query)
{
    log_debug(LOG_TXIDX) << "transaction_indexer::query("
        << payaddr.encoded() << ")";
    strand_.queue(
        std::bind(&transaction_indexer::do_query,
            this, payaddr, handle_query));
}

template <typename InfoList, typename EntryMultimap>
InfoList get_info_list(const payment_address& payaddr,
    EntryMultimap& map)
{
    InfoList info;
    auto iter_pair = map.equal_range(payaddr);
    for (auto it = iter_pair.first; it != iter_pair.second; ++it)
        info.push_back(it->second);
    return info;
}
void transaction_indexer::do_query(const payment_address& payaddr,
    query_handler handle_query)
{
    log_debug(LOG_TXIDX) << "transaction_indexer::do_query("
        << payaddr.encoded() << ")";
    handle_query(std::error_code(),
        get_info_list<output_info_list>(payaddr, outputs_map_),
        get_info_list<spend_info_list>(payaddr, spends_map_));
}

template <typename Point, typename EntryMultimap>
auto find_entry(const payment_address& key,
    const Point& value_point, EntryMultimap& map) -> decltype(map.begin())
{
    auto iter_pair = map.equal_range(key);
    for (auto it = iter_pair.first; it != iter_pair.second; ++it)
    {
        // This entry should only occur once in the multimap.
        if (it->second.point == value_point)
            return it;
    }
    return map.end();
}

template <typename Point, typename EntryMultimap>
bool index_does_not_exist(const payment_address& key,
    const Point& value_point, EntryMultimap& map)
{
    return find_entry(key, value_point, map) == map.end();
}

void transaction_indexer::index(const transaction_type& tx,
    completion_handler handle_index)
{
    log_debug(LOG_TXIDX) << "transaction_indexer::index("
        << hash_transaction(tx) << ")";
    strand_.queue(
        std::bind(&transaction_indexer::do_index,
            this, tx, handle_index));
}
void transaction_indexer::do_index(const transaction_type& tx,
    completion_handler handle_index)
{
    log_debug(LOG_TXIDX) << "transaction_indexer::do_index("
        << hash_transaction(tx) << ")";
    hash_digest tx_hash = hash_transaction(tx);
    for (uint32_t i = 0; i < tx.inputs.size(); ++i)
    {
        auto l = log_debug(LOG_TXIDX);
        l << "  " << i;
        const transaction_input_type& input = tx.inputs[i];
        payment_address payaddr;
        // Nothing to see here folks. Move along.
        if (!extract(payaddr, input.script))
            continue;
        l << "  " << payaddr.encoded();
        input_point point{tx_hash, i};
        BITCOIN_ASSERT_MSG(
            index_does_not_exist(payaddr, point, spends_map_),
            "Transaction is indexed duplicate times!");
        spends_map_.emplace(payaddr,
            spend_info_type{point, input.previous_output});
    }
    for (uint32_t i = 0; i < tx.outputs.size(); ++i)
    {
        auto l = log_debug(LOG_TXIDX);
        l << "  " << i;
        const transaction_output_type& output = tx.outputs[i];
        payment_address payaddr;
        // Nothing to see here folks. Move along.
        if (!extract(payaddr, output.script))
            continue;
        l << "  " << payaddr.encoded();
        output_point point{tx_hash, i};
        BITCOIN_ASSERT_MSG(
            index_does_not_exist(payaddr, point, outputs_map_),
            "Transaction is indexed duplicate times!");
        outputs_map_.emplace(payaddr,
            output_info_type{point, output.value});
    }
}

void transaction_indexer::deindex(const transaction_type& tx,
    completion_handler handle_deindex)
{
    log_debug(LOG_TXIDX) << "transaction_indexer::deindex("
        << hash_transaction(tx) << ")";
    strand_.queue(
        std::bind(&transaction_indexer::do_deindex,
            this, tx, handle_deindex));
}
void transaction_indexer::do_deindex(const transaction_type& tx,
    completion_handler handle_deindex)
{
    log_debug(LOG_TXIDX) << "transaction_indexer::do_deindex("
        << hash_transaction(tx) << ")";
    hash_digest tx_hash = hash_transaction(tx);
    for (uint32_t i = 0; i < tx.inputs.size(); ++i)
    {
        auto l = log_debug(LOG_TXIDX);
        l << "  " << i;
        const transaction_input_type& input = tx.inputs[i];
        payment_address payaddr;
        // Nothing to see here folks. Move along.
        if (!extract(payaddr, input.script))
            continue;
        l << "  " << payaddr.encoded();
        input_point point{tx_hash, i};
        auto it = find_entry(payaddr, point, spends_map_);
        BITCOIN_ASSERT_MSG(it != spends_map_.end(),
            "Can't deindex transaction twice");
        spends_map_.erase(it);
        BITCOIN_ASSERT_MSG(
            index_does_not_exist(payaddr, point, spends_map_),
            "Transaction is indexed duplicate times!");
    }
    for (uint32_t i = 0; i < tx.outputs.size(); ++i)
    {
        auto l = log_debug(LOG_TXIDX);
        l << "  " << i;
        const transaction_output_type& output = tx.outputs[i];
        payment_address payaddr;
        // Nothing to see here folks. Move along.
        if (!extract(payaddr, output.script))
            continue;
        l << "  " << payaddr.encoded();
        output_point point{tx_hash, i};
        auto it = find_entry(payaddr, point, outputs_map_);
        BITCOIN_ASSERT_MSG(it != outputs_map_.end(),
            "Can't deindex transaction twice");
        outputs_map_.erase(it);
        BITCOIN_ASSERT_MSG(
            index_does_not_exist(payaddr, point, outputs_map_),
            "Transaction is indexed duplicate times!");
    }
}

void blockchain_history_fetched(const std::error_code& ec,
    const blockchain::history_list& history,
    transaction_indexer& indexer, const payment_address& address,
    blockchain::fetch_handler_history handle_fetch);
void indexer_history_fetched(const std::error_code& ec,
    const output_info_list& outputs, const spend_info_list& spends,
    blockchain::history_list history,
    blockchain::fetch_handler_history handle_fetch);
// Fetch the history first from the blockchain and then from the indexer.
void fetch_history(blockchain& chain, transaction_indexer& indexer,
    const payment_address& address,
    blockchain::fetch_handler_history handle_fetch, size_t from_height)
{
    log_debug(LOG_TXIDX) << "fetch_history(chain, idx, " << address.encoded()
        << ", from_height=" << from_height << ")";
    chain.fetch_history(address,
        std::bind(blockchain_history_fetched, _1, _2,
            std::ref(indexer), address, handle_fetch), from_height);
}
void blockchain_history_fetched(const std::error_code& ec,
    const blockchain::history_list& history,
    transaction_indexer& indexer, const payment_address& address,
    blockchain::fetch_handler_history handle_fetch)
{
    log_debug(LOG_TXIDX) << "blockchain_history_fetched(" << ec.message()
        << ", history(" << history.size() << "), " << address.encoded() << ")";
    if (ec)
        handle_fetch(ec, blockchain::history_list());
    else
        indexer.query(address,
            std::bind(indexer_history_fetched, _1, _2, _3,
                history, handle_fetch));
}
void indexer_history_fetched(const std::error_code& ec,
    const output_info_list& outputs, const spend_info_list& spends,
    blockchain::history_list history,
    blockchain::fetch_handler_history handle_fetch)
{
    constexpr uint32_t max_height = std::numeric_limits<uint32_t>::max();
    log_debug(LOG_TXIDX) << "indexer_history_fetched(" << ec.message()
        << ", spends(" << spends.size() << "), outputs(" << outputs.size()
        << "), history(" << history.size() << "))";
    if (ec)
    {
        handle_fetch(ec, blockchain::history_list());
        return;
    }
    // Just add in outputs.
    for (const output_info_type& output_info: outputs)
    {
#ifndef BITCOIN_DISABLE_ASSERTS
        // Compiler should be smart enough to optimise this out
        // if contains an empty body.
        for (const blockchain::history_row& row: history)
        {
            // If the indexer and memory pool are working properly,
            // then there shouldn't be any transactions indexed
            // that are already confirmed and in the blockchain.
            if (row.output == output_info.point)
                log_debug(LOG_TXIDX) << "  conflict " << row.output;
            BITCOIN_ASSERT(row.output != output_info.point);
        }
#endif
        history.emplace_back(blockchain::history_row{
            output_info.point,
            0,
            output_info.value,
            {null_hash, max_index},
            max_height
        });
    }
    // Now mark spends.
    for (const spend_info_type& spend_info: spends)
    {
        // Iterate history looking for the output we need.
        bool found = false;
        for (blockchain::history_row& row: history)
        {
            if (row.output != spend_info.previous_output)
                continue;
            BITCOIN_ASSERT(row.spend_height == max_height);
            BITCOIN_ASSERT((row.spend == input_point{null_hash, max_index}));
            row.spend = spend_info.point;
            row.spend_height = 0;
            found = true;
            break;
        }
        BITCOIN_ASSERT_MSG(found, "Couldn't find output for adding spend");
    }
    handle_fetch(std::error_code(), history);
}

} // namespace libbitcoin

