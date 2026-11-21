#pragma once

#include <QByteArrayView>
#include <QString>
#include <QStringDecoder>

namespace pyrun {

// Turns arbitrary byte chunks of child output into text.
//  - Stateful UTF-8: a multi-byte sequence split across two chunks decodes
//    correctly (one decoder per run, never per chunk).
//  - Line endings normalised to LF. A CR at the end of a chunk is held back
//    until the next chunk (or flush()) so a CRLF split across reads is not
//    seen as two line breaks. A lone CR (progress bars) is preserved.
// Never use fromLocal8Bit here: the child is forced into UTF-8 mode.
class OutputDecoder {
public:
    OutputDecoder() = default;

    // Start of a new stream: drop any partial sequence / held CR.
    void reset();

    [[nodiscard]] QString decode(QByteArrayView chunk);
    // Call once at end of stream; releases a held CR.
    [[nodiscard]] QString flush();

private:
    QStringDecoder m_decoder{QStringConverter::Utf8};
    bool m_heldCr = false;
};

}  // namespace pyrun
