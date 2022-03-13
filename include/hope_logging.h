#ifndef HOPE_LOGGING_H
#define HOPE_LOGGING_H

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
namespace Hope{
inline std::shared_ptr<spdlog::logger> logger;
}

#ifdef QT_CORE_LIB
#include <QDesktopServices>
#include <QStandardPaths>
#include <QUrl>

namespace Hope{
static const std::string log_dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation).toStdString();

inline void openLogFile(){
    logger->flush();
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(log_dir + "/log.txt")));
}
}
#else
namespace Hope{
static const std::string log_dir = ".";
}
#endif

namespace Hope {

inline void initLogging(){
    logger = spdlog::basic_logger_mt("basic_logger", log_dir + "/log.txt");
}

inline std::string cStr(const std::string& str){
    std::string out;
    out += '"';
    for(char ch : str){
        if(ch == '\n' || ch == '\r'){
            out += "\\n";
        }else{
            if(ch == '"' || ch == '\\') out += '\\';
            out += ch;
        }
    }
    out += '"';

    return out;
}

}

#endif // HOPE_LOGGING_H
