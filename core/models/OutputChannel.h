#pragma once

namespace pyrun {

// Which of the script's two output streams a piece of text came from.
// Plain enum: no Qt types, so models, views and presenters can all use it.
enum class OutputChannel {
    StdOut,
    StdErr,
};

}  // namespace pyrun
