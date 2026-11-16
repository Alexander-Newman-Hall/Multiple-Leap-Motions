#include <atomic>
#include <chrono>
#include <thread>
#include <stop_token>

int main() {
    std::atomic<bool> entered{false};
    std::atomic<bool> stopped{false};
    {
        std::jthread worker([&](std::stop_token stop) {
            std::stop_callback callback(stop, [&] { stopped.store(true); });
            entered.store(true);
            while (!stop.stop_requested())
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!entered.load() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (!entered.load()) return 1;
        // Destructor requests stop and joins, as required by the engine host.
    }
    return stopped.load() ? 0 : 2;
}
