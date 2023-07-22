#ifndef QT_COMPATABILITY_H
#define QT_COMPATABILITY_H

#include <filesystem>
#include <QString>

inline std::string toCppString(const QString& q_str) {
    auto utf8_str= q_str.toUtf8();
    return std::string(utf8_str.data(), utf8_str.size());
}

inline std::filesystem::path toCppPath(const QString& q_str) {
    return std::filesystem::u8path(toCppString(q_str));
}

inline QString toQString(const std::string& str) {
    return QString::fromUtf8(str.data(), static_cast<int>(str.size()));
}

inline QString toQString(const std::filesystem::path& path){
    return toQString(path.u8string());
}

#endif // QT_COMPATABILITY_H
