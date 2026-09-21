#ifndef SETTINGS_H
#define SETTINGS_H

#include <QString>
#include <QColor>

#define APPLICATION_BACKGROUND_COLOR QColor(62, 62, 62)

#define DEFAULT_ICON_SIZE 512

namespace Settings {
inline QString GENERAL_STYLESHEET =
    "QWidget {"
    "    background-color: " + APPLICATION_BACKGROUND_COLOR.name() + ";"
    "    color: rgb(255, 255, 255);"
    "}"
    "QCheckBox {"
    "    color: #ffffff;"
    "}"
    "QLineEdit {"
    "    background-color: #777777;"
    "    color: #ffffff;"
    "}"
    "QPlainTextEdit {"
    "    background-color: #777777;"
    "    color: #ffffff;"
    "}"
    "QSplitter::handle {"
    "   background-color: #bbb;"
    "}"                                                                     
    "QPushButton {"
    "    background-color: #555555;"
    "    color: #ffffff;"
    "    border: 1px solid #777777;"
    "    padding: 5px;"
    "}"
    "QComboBox {"
    "    background-color: #555555;"
    "    color: #ffffff;"
    "    padding: 5px;"
    "}"
    "QRadioButton {"
    "    color: #ffffff;"
    "}"
    "QRadioButton::indicator {"
    "    width: 14px;"
    "    height: 14px;"
    "    border-radius: 7px;"
    "    border: 2px solid #ffffff;"
    "    background-color: #ffffff;"
    "}"
    "QRadioButton::indicator:checked {"
    "    background-color: #555555;"
    "}"
    "QListWidget {"
    "    background-color: #555555"
    "}"
    ;
}

#endif // SETTINGS_H
