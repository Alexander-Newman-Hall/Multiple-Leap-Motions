#include <atomic>
#include <memory>
#include <thread>

int main() {
    std::atomic<std::shared_ptr<const int>> snapshot{std::make_shared<const int>(0)};
    std::thread writer([&] {
        for (int value = 1; value <= 1000; ++value)
            snapshot.store(std::make_shared<const int>(value));
    });
    for (int i = 0; i < 1000; ++i) {
        const auto owned = snapshot.load();
        if (!owned || *owned < 0 || *owned > 1000) {
            writer.join();
            return 1;
        }
    }
    writer.join();
    return *snapshot.load() == 1000 ? 0 : 2;
}
