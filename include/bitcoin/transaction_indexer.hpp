#ifndef LIBBITCOIN_TRANSACTION_INDEXER_HPP
#define LIBBITCOIN_TRANSACTION_INDEXER_HPP

#include <unordered_map>
#include <queue>
#include <system_error>

#include <bitcoin/address.hpp>
#include <bitcoin/primitives.hpp>
#include <bitcoin/threadpool.hpp>
#include <bitcoin/types.hpp>

namespace libbitcoin {

struct spend_info_type
{
    input_point point;
    output_point previous_output;
};

struct output_info_type
{
    output_point point;
    uint64_t value;
};

typedef std::vector<spend_info_type> spend_info_list;
typedef std::vector<output_info_type> output_info_list;

class transaction_indexer
  : public async_strand
{
public:
    typedef std::function<void (const std::error_code&)> completion_handler;

    typedef std::function<void (const std::error_code& ec,
        const spend_info_list& spends, const output_info_list& outputs)>
            query_handler;

    transaction_indexer(threadpool& pool);

    /// Non-copyable class
    transaction_indexer(const transaction_indexer&) = delete;
    /// Non-copyable class
    void operator=(const transaction_indexer&) = delete;

    /**
     * Query all transactions indexed that are related to a Bitcoin address.
     *
     * @param[in]   payaddr         Bitcoin address to lookup.
     * @param[in]   handle_query    Completion handler for fetch operation.
     * @code
     *  void handle_query(
     *      const std::error_code& ec,       // Status of operation
     *      const spend_info_list& spends,   // Inputs
     *      const output_info_list& outputs  // Outputs
     *  );
     * @endcode
     * @code
     *  struct spend_info_type
     *  {
     *      input_point point;
     *      output_point previous_output;
     *  };
     *
     *  struct output_info_type
     *  {
     *      output_point point;
     *      uint64_t value;
     *  };
     * @endcode
     */
    void query(const payment_address& payaddr,
        query_handler handle_query);

    /**
     * Index a transaction.
     *
     * @param[in]   tx              Transaction to index.
     * @param[in]   handle_index    Completion handler for index operation.
     * @code
     *  void handle_index(
     *      const std::error_code& ec        // Status of operation
     *  );
     * @endcode
     */
    void index(const transaction_type& tx,
        completion_handler handle_index);

    /**
     * Deindex (remove from index) a transaction.
     *
     * @param[in]   tx              Transaction to deindex.
     * @param[in]   handle_index    Completion handler for deindex operation.
     * @code
     *  void handle_deindex(
     *      const std::error_code& ec        // Status of operation
     *  );
     * @endcode
     */
    void deindex(const transaction_type& tx,
        completion_handler handle_deindex);

private:
    // addr -> spend
    typedef std::unordered_multimap<payment_address, spend_info_type>
        spends_multimap;
    // addr -> output
    typedef std::unordered_multimap<payment_address, output_info_type>
        outputs_multimap;

    void do_query(const payment_address& payaddr,
        query_handler handle_query);

    void do_index(const transaction_type& tx,
        completion_handler handle_index);

    void do_deindex(const transaction_type& tx,
        completion_handler handle_deindex);

    // De-index expired transactions.
    void periodic_update();

    spends_multimap spends_map_;
    outputs_multimap outputs_map_;
};

} // namespace libbitcoin

#endif

