#pragma once

namespace pyrun {

// How a status line should be presented. Decided by the presenter; views map it
// to colour or nothing and never inspect the text.
enum class StatusSeverity {
    Info,     // run headers, "Summary:"
    Success,  // [ok], a clean summary (views render it plain)
    Failure,  // [failed], [timeout], [stopped], FAILED, a summary with failures
};

}  // namespace pyrun
