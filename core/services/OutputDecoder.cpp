#include "services/OutputDecoder.h"

namespace pyrun {

void OutputDecoder::reset() {
    m_decoder.resetState();
    m_heldCr = false;
}

QString OutputDecoder::decode(QByteArrayView chunk) {
    QString text = m_decoder(chunk);

    if (m_heldCr) {
        text.prepend(QLatin1Char('\r'));
        m_heldCr = false;
    }
    if (text.endsWith(QLatin1Char('\r'))) {
        text.chop(1);
        m_heldCr = true;
    }
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    return text;
}

QString OutputDecoder::flush() {
    if (!m_heldCr) {
        return {};
    }
    m_heldCr = false;
    return QStringLiteral("\r");
}

}  // namespace pyrun
