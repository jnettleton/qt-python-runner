#pragma once

#include "models/OutputChannel.h"
#include "models/StatusSeverity.h"

#include <QColor>
#include <QPalette>
#include <QTextCharFormat>

namespace pyrun {

// THE knob for how script stderr looks in the GUI. One hue; change this one
// line to switch (yellow: 0xFFC800). Lightness is derived per palette below, so
// any hue stays readable on both light and dark backgrounds.
inline constexpr QRgb kStderrColor = 0xD32F2F;  // red

// kStderrColor with its lightness pinned for the palette it will be drawn on:
// pale on a dark base, deep on a light base. Hue and saturation are kept.
[[nodiscard]] inline QColor stderrTextColor(const QPalette& palette) {
    const bool darkBase = palette.color(QPalette::Base).lightnessF() < 0.5;
    const QColor hue(kStderrColor);
    return QColor::fromHslF(hue.hslHueF(), hue.hslSaturationF(), darkBase ? 0.68 : 0.36);
}

// stdout uses the default format on purpose: the text colour then follows the
// palette (light or dark) with nothing hard-coded.
[[nodiscard]] inline QTextCharFormat formatFor(OutputChannel channel, const QPalette& palette) {
    QTextCharFormat format;
    if (channel == OutputChannel::StdErr) {
        format.setForeground(stderrTextColor(palette));
    }
    return format;
}

// Failures reuse the stderr colour (kStderrColor): one constant, one look for
// "something went wrong". Info and Success are plain.
[[nodiscard]] inline QTextCharFormat statusFormat(StatusSeverity severity,
                                                  const QPalette& palette) {
    QTextCharFormat format;
    if (severity == StatusSeverity::Failure) {
        format.setForeground(stderrTextColor(palette));
    }
    return format;
}

}  // namespace pyrun
