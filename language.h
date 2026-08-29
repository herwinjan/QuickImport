#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <QString>
#include <QStringList>

// Interface language handling.
//
// The user can pick a language explicitly (stored in QSettings under
// "language") or leave it on "system", in which case the system locale
// decides. install() (re)loads both the application translations and the
// matching Qt base translations, so it can be called at start-up and again
// whenever the user changes the setting.
namespace AppLanguage {

// Sentinel stored in QSettings when the language should follow the system.
QString systemCode();

// Language codes that have a compiled .qm embedded in the binary.
QStringList availableCodes();

// Endonym for a code ("nl" -> "Nederlands"); deliberately not translated.
QString nativeName(const QString &code);

// Current setting: systemCode() or one of availableCodes().
QString currentCode();
void setCurrentCode(const QString &code);

// (Re)install the translators for the current setting. Returns the language
// code that ended up being used, which for "system" is the resolved locale
// language, or an empty string when no translation was loaded.
QString install();

} // namespace AppLanguage

#endif // LANGUAGE_H
