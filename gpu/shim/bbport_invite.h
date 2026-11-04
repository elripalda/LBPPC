// lbppc: the PS4 system's "X invited you to play" notification, which LBP3 relies on: the player answers here
// (bbport_overlay.cpp draws it, window.cpp takes Cross/Enter and Circle/Esc) and Join raises the game's
// SESSION_INVITATION event (bbnet_glue.cpp, NpHandler::AcceptSessionInvitation).
#pragma once

#include <functional>
#include <string>

namespace BbInvite {

/// Any thread: queues an invitation; a newer one with the same id replaces it. `answer(true)` joins.
void Show(const std::string& invitation_id, const std::string& from, std::function<void(bool join)> answer);
/// Any thread: drops it (answered another way, or withdrawn).
void Dismiss(const std::string& invitation_id);
/// Whether one is on screen (the game's input is held neutral meanwhile).
bool Shown();
/// Window thread: the player's answer to the one on screen.
void Answer(bool join);
/// Present thread: who invited, for drawing ("" when none).
std::string ShownFrom();

} // namespace BbInvite
