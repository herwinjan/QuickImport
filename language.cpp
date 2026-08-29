#include "language.h"

#include <QCoreApplication>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

namespace {

const char *const kSettingsKey = "language";

// Function-local statics: constructed on first use and destroyed at exit,
// which is after QCoreApplication has already released them.
QTranslator &appTranslator()
{
    static QTranslator translator;
    return translator;
}

QTranslator &qtTranslator()
{
    static QTranslator translator;
    return translator;
}

// Loads ":/translation/<prefix>_<code>.qm", or lets QTranslator resolve the
// system locale (including its fallbacks) when code is the system sentinel.
bool loadInto(QTranslator &translator, const QString &prefix, const QString &code)
{
    QCoreApplication::removeTranslator(&translator);

    const bool loaded =
        (code == AppLanguage::systemCode())
            ? translator.load(QLocale(), prefix, QStringLiteral("_"),
                              QStringLiteral(":/translation"))
            : translator.load(QStringLiteral(":/translation/%1_%2.qm").arg(prefix, code));

    if (loaded)
        QCoreApplication::installTranslator(&translator);
    return loaded;
}

} // namespace

QString AppLanguage::systemCode()
{
    return QStringLiteral("system");
}

QStringList AppLanguage::availableCodes()
{
    return {QStringLiteral("en"), QStringLiteral("nl"), QStringLiteral("de"),
            QStringLiteral("es")};
}

QString AppLanguage::nativeName(const QString &code)
{
    if (code == QLatin1String("en"))
        return QStringLiteral("English");
    if (code == QLatin1String("nl"))
        return QStringLiteral("Nederlands");
    if (code == QLatin1String("de"))
        return QStringLiteral("Deutsch");
    if (code == QLatin1String("es"))
        return QStringLiteral("Español");
    return code;
}

QString AppLanguage::currentCode()
{
    QSettings settings;
    const QString code = settings.value(QLatin1String(kSettingsKey), systemCode()).toString();
    if (code != systemCode() && !availableCodes().contains(code))
        return systemCode();
    return code;
}

void AppLanguage::setCurrentCode(const QString &code)
{
    QSettings settings;
    settings.setValue(QLatin1String(kSettingsKey), code);
}

QString AppLanguage::install()
{
    const QString code = currentCode();

    if (!loadInto(appTranslator(), QStringLiteral("quickimport"), code))
        return QString();

    // Qt's own strings (standard buttons, file dialog). Best effort: English
    // needs no catalogue, and a missing one only leaves those strings English.
    const QString resolved = QLocale(appTranslator().language()).name().section('_', 0, 0);
    loadInto(qtTranslator(), QStringLiteral("qtbase"),
             code == systemCode() ? resolved : code);

    return code == systemCode() ? resolved : code;
}
