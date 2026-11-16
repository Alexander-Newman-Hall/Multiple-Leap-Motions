#include "engine/SnapshotMailbox.hpp"
#include <gtest/gtest.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <thread>
#include <vector>

namespace {
struct Record {
    explicit Record(std::uint64_t value) : generation(value), complement(~value) {
        payload.fill(value);
    }
    std::uint64_t generation, complement;
    std::array<std::uint64_t, 8> payload;

    bool coherent() const {
        if (complement != ~generation) return false;
        for (const auto value : payload) if (value != generation) return false;
        return true;
    }
};

// Keep cleanup valid even if starting a later thread or publishing throws.
struct Readers {
    std::atomic<bool>& done;
    std::vector<std::thread> threads;
    ~Readers() {
        done.store(true);
        join();
    }
    void join() {
        for (auto& thread : threads) if (thread.joinable()) thread.join();
    }
};
}

TEST(WorkerConcurrency, RetainedSnapshotOutlivesReplacementAndMailbox) {
    std::shared_ptr<const Record> retained;
    std::weak_ptr<const Record> original;
    {
        handbridge::SnapshotMailbox<Record> mailbox;
        EXPECT_FALSE(mailbox.load());
        auto first = std::make_shared<const Record>(17);
        original = first;
        mailbox.store(std::move(first));
        retained = mailbox.load();
        mailbox.store(std::make_shared<const Record>(42));
        ASSERT_TRUE(retained);
        EXPECT_EQ(retained->generation, 17u);
        EXPECT_TRUE(retained->coherent());
        EXPECT_EQ(mailbox.load()->generation, 42u);
        EXPECT_FALSE(original.expired());
    }
    ASSERT_TRUE(retained);
    EXPECT_EQ(retained->generation, 17u);
    EXPECT_TRUE(retained->coherent());
    EXPECT_FALSE(original.expired());
    retained.reset();
    EXPECT_TRUE(original.expired());
}

TEST(WorkerConcurrency, ConcurrentReadersSeeCoherentImmutableSnapshots) {
    handbridge::SnapshotMailbox<Record> mailbox;
    mailbox.store(std::make_shared<const Record>(0));
    const auto retained = mailbox.load();
    std::atomic<bool> done{false}, coherent{true};
    std::atomic<std::uint64_t> observations{0};
    std::array<std::promise<void>, 2> ready;
    std::array<std::future<void>, 2> started{ready[0].get_future(), ready[1].get_future()};
    Readers readers{done, {}};
    readers.threads.reserve(2);
    for (std::size_t index = 0; index < ready.size(); ++index) {
        readers.threads.emplace_back([&, index] {
            const auto observe = [&] {
                const auto current = mailbox.load();
                if (!current || !current->coherent()) coherent.store(false);
                observations.fetch_add(1);
            };
            observe();
            ready[index].set_value();
            while (!done.load()) {
                observe();
                std::this_thread::yield();
            }
            observe();
        });
    }
    // Each reader owns at least one observation before publication starts.
    // No assertion requires the scheduler to expose every intermediate value.
    for (auto& future : started) future.get();
    constexpr std::uint64_t publications = 5000;
    for (std::uint64_t generation = 1; generation <= publications; ++generation)
        mailbox.store(std::make_shared<const Record>(generation));
    done.store(true);
    readers.join();
    EXPECT_TRUE(coherent.load());
    EXPECT_GE(observations.load(), 4u);
    ASSERT_TRUE(mailbox.load());
    EXPECT_EQ(mailbox.load()->generation, publications);
    EXPECT_EQ(retained->generation, 0u);
    EXPECT_TRUE(retained->coherent());
}
