// SPDX-License-Identifier: GPL-2.0-or-later
// lbppc: invitations waiting for the player's answer (bbport_invite.h).
#include "bbport_invite.h"

#include <algorithm>
#include <cstdio>
#include <deque>
#include <mutex>

namespace BbInvite {
namespace {

struct Pending {
    std::string id, from;
    std::function<void(bool)> answer;
};
std::mutex mutex;
std::deque<Pending> pending; // the front one is on screen

} // namespace

void Show(const std::string& invitation_id, const std::string& from, std::function<void(bool join)> answer) {
    std::scoped_lock lock{mutex};
    std::erase_if(pending, [&](const Pending& p) { return p.id == invitation_id; });
    pending.push_back({invitation_id, from, std::move(answer)});
    std::printf("Online: %s invited you to play (Cross or Enter joins, Circle or Esc does not)\n", from.c_str());
}

void Dismiss(const std::string& invitation_id) {
    std::scoped_lock lock{mutex};
    std::erase_if(pending, [&](const Pending& p) { return p.id == invitation_id; });
}

bool Shown() {
    std::scoped_lock lock{mutex};
    return !pending.empty();
}

void Answer(bool join) {
    Pending answered;
    {
        std::scoped_lock lock{mutex};
        if (pending.empty()) {
            return;
        }
        answered = std::move(pending.front());
        pending.pop_front();
    }
    std::printf("Online: invitation from %s %s\n", answered.from.c_str(), join ? "accepted, joining" : "declined");
    // Outside the lock: accepting dismisses the invitation again (NpHandler::AcceptSessionInvitation).
    answered.answer(join);
}

std::string ShownFrom() {
    std::scoped_lock lock{mutex};
    return pending.empty() ? std::string{} : pending.front().from;
}

} // namespace BbInvite
