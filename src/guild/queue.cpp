#include "guild/queue.hpp"

namespace tts_bot {

void GuildQueue::push(TTSRequest req) {
    std::lock_guard lock(mutex_);
    queue_.push_back(std::move(req));
}

bool GuildQueue::try_pop(TTSRequest& out) {
    std::lock_guard lock(mutex_);
    if (queue_.empty()) return false;
    out = std::move(queue_.front());
    queue_.pop_front();
    return true;
}

size_t GuildQueue::size() const {
    std::lock_guard lock(mutex_);
    return queue_.size();
}

bool GuildQueue::empty() const {
    std::lock_guard lock(mutex_);
    return queue_.empty();
}

} // namespace tts_bot
