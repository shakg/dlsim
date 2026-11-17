// Turns raw failure text (tc/ip stderr, helper status, validation) into a
// cause the user can read, what to do about it, and an optional action the
// GUI can offer as a button.
#pragma once

#include <QString>

namespace dlsim {

struct ExplainedError {
    QString title;       // what went wrong, in plain words
    QString fix;         // what to do; may be empty
    QString action;      // GUI action id: "retry-helper", "refresh-interfaces", "copy-command"; may be empty
    QString actionLabel; // button text for action
    QString actionArg;   // e.g. the command to copy
};

// context: "helper" (start/exit), "bridge" (setup_bridge), "apply" (apply_profile,
// cut, restore, stop, reset), "socket" (socket monitor), "start" (validation before start).
ExplainedError explainError(const QString &context, const QString &raw);

} // namespace dlsim
