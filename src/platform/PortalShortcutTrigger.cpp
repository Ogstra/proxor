#include "PortalShortcutTrigger.hpp"

#include <QHash>
#include <QRegularExpression>

namespace ProxorPlatform {

namespace {

const QHash<QString, QString> &NamedKeys() {
    static const QHash<QString, QString> keys = {
        {QStringLiteral("space"), QStringLiteral("space")},
        {QStringLiteral("return"), QStringLiteral("Return")},
        {QStringLiteral("enter"), QStringLiteral("Return")},
        {QStringLiteral("esc"), QStringLiteral("Escape")},
        {QStringLiteral("escape"), QStringLiteral("Escape")},
        {QStringLiteral("backspace"), QStringLiteral("BackSpace")},
        {QStringLiteral("del"), QStringLiteral("Delete")},
        {QStringLiteral("delete"), QStringLiteral("Delete")},
        {QStringLiteral("ins"), QStringLiteral("Insert")},
        {QStringLiteral("insert"), QStringLiteral("Insert")},
        {QStringLiteral("pgup"), QStringLiteral("Prior")},
        {QStringLiteral("pgdown"), QStringLiteral("Next")},
        {QStringLiteral("left"), QStringLiteral("Left")},
        {QStringLiteral("right"), QStringLiteral("Right")},
        {QStringLiteral("up"), QStringLiteral("Up")},
        {QStringLiteral("down"), QStringLiteral("Down")},
        {QStringLiteral("home"), QStringLiteral("Home")},
        {QStringLiteral("end"), QStringLiteral("End")},
        {QStringLiteral("tab"), QStringLiteral("Tab")},
        {QStringLiteral("print"), QStringLiteral("Print")},
        {QStringLiteral("pause"), QStringLiteral("Pause")},
        {QStringLiteral("menu"), QStringLiteral("Menu")},
    };
    return keys;
}

const QHash<QChar, QString> &PunctuationKeys() {
    static const QHash<QChar, QString> keys = {
        {QLatin1Char('+'), QStringLiteral("plus")},         {QLatin1Char('-'), QStringLiteral("minus")},
        {QLatin1Char(','), QStringLiteral("comma")},        {QLatin1Char('.'), QStringLiteral("period")},
        {QLatin1Char('/'), QStringLiteral("slash")},        {QLatin1Char(';'), QStringLiteral("semicolon")},
        {QLatin1Char('\''), QStringLiteral("apostrophe")},  {QLatin1Char('['), QStringLiteral("bracketleft")},
        {QLatin1Char(']'), QStringLiteral("bracketright")}, {QLatin1Char('\\'), QStringLiteral("backslash")},
        {QLatin1Char('`'), QStringLiteral("grave")},        {QLatin1Char('='), QStringLiteral("equal")},
    };
    return keys;
}

QString KeysymFromKeyName(const QString &key) {
    if (key.isEmpty()) return QString();
    if (key.size() == 1) {
        const QChar c = key.at(0);
        if (c.unicode() < 128 && c.isLetter()) return QString(c.toLower());
        if (c.unicode() < 128 && c.isDigit()) return key;
        return PunctuationKeys().value(c);
    }
    static const QRegularExpression fkey(QStringLiteral("^[Ff]([1-9][0-9]?)$"));
    const auto m = fkey.match(key);
    if (m.hasMatch()) return QStringLiteral("F") + m.captured(1);
    return NamedKeys().value(key.toLower());
}

} // namespace

QString PortalTriggerFromKeySequence(const QString &portableText) {
    QString rest = portableText.trimmed();
    if (rest.isEmpty()) return QString();
    if (rest.contains(QStringLiteral(", "))) return QString(); // several chords

    bool ctrl = false, alt = false, shift = false, logo = false;
    for (;;) {
        const int plus = rest.indexOf(QLatin1Char('+'));
        if (plus <= 0) break; // no more "Mod+" prefix ("+" alone is the plus key)
        const QString mod = rest.left(plus).toLower();
        if (mod == QLatin1String("ctrl")) ctrl = true;
        else if (mod == QLatin1String("alt")) alt = true;
        else if (mod == QLatin1String("shift")) shift = true;
        else if (mod == QLatin1String("meta")) logo = true;
        else return QString();
        rest = rest.mid(plus + 1);
    }
    const QString keysym = KeysymFromKeyName(rest);
    if (keysym.isEmpty()) return QString();

    QStringList parts;
    if (ctrl) parts << QStringLiteral("CTRL");
    if (alt) parts << QStringLiteral("ALT");
    if (shift) parts << QStringLiteral("SHIFT");
    if (logo) parts << QStringLiteral("LOGO");
    parts << keysym;
    return parts.join(QLatin1Char('+'));
}

} // namespace ProxorPlatform
