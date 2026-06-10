#pragma once

// ================ 设置 ================ //

#define SHORT_MESSAGE_MAX_LENGTH 8192  // 短文本最大长度，单位为字节

#define ENABLE_SQLCIPHER
#define ENABLE_NLOHMANN_JSON
#define ENABLE_M_INI
#define ENABLE_BOOST_DLL
#define ENABLE_BOOST_UUID
#define ENABLE_BOOST_PROGRAM_OPTIONS
#define ENABLE_FMT
#define ENABLE_STDUUID
#define ENABLE_HASH_LIBRARY
// #define ENABLE_CLIENT

#ifdef ENABLE_CLIENT
#define ENABLE_HEADERS_QT
#define ENABLE_QT_MACRO
// #define ENABLE_QDEBUG_LOGGER
// #define ENABLE_SIMPLE_QT_LOGGER
// #define ENABLE_SPDLOG
#elif defined(ENABLE_SERVER)  // ENABLE_SERVER
#define ENABLE_HEADERS_STD
#define ENABLE_GOOGLE_GRPC
#define ENABLE_SPDLOG
#define ENABLE_ABSL_LIB
#endif // ENABLE_CLIENT

#define ENABLE_PROTO_PROJECT_NAMESPACE

// ================ 内容 ================ //

// std
// #define ENABLE_HEADERS_STD
#ifdef ENABLE_HEADERS_STD
// #include <ccomplex> // 已废弃，使用 <complex> 替代
// #include <ciso646> // 已废弃，使用 <version> 来检测实现特定的宏
// #include <cstdbool> // 已废弃，使用 <stdbool.h> 替代
// #include <ctgmath> // 已废弃，使用 <complex> 或 <cmath> 替代
#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cfenv>
#include <cfloat>
#include <chrono>
#include <cinttypes>
#include <climits>
#include <clocale>
#include <cmath>
#include <codecvt>
#include <complex>
#include <concepts>  // C++20，模板约束条件
#include <condition_variable>
#include <csetjmp>
#include <csignal>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <cwctype>
#include <deque>
#include <exception>
#include <filesystem>
#include <format>
#include <forward_list>
#include <fstream>
#include <functional>
#include <future>
#include <initializer_list>
#include <iomanip>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <istream>
#include <iterator>
#include <limits>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <numeric>
#include <ostream>
#include <queue>
#include <random>
#include <ratio>
#include <regex>
#include <scoped_allocator>
#include <set>
#include <shared_mutex>
#include <source_location>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <streambuf>
#include <string_view>
#include <string>
#include <system_error>
#include <thread>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <valarray>
#include <variant>
#include <vector>

// auto now = std::chrono::system_clock::now;
// using string_ref = ::grpc::string_ref;
using time_point = std::chrono::system_clock::time_point;
//
using std::chrono::current_zone;
using std::chrono::days;
using std::chrono::duration_cast;
using std::chrono::duration;
using std::chrono::high_resolution_clock;
using std::chrono::hours;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::minutes;
using std::chrono::months;
using std::chrono::nanoseconds;
using std::chrono::parse;
using std::chrono::seconds;
using std::chrono::steady_clock;
using std::chrono::system_clock;
using std::chrono::weekday;
using std::chrono::weeks;
using std::chrono::years;
using std::chrono::zoned_time;
using std::condition_variable;
using std::decay_t;
using std::filesystem::absolute;
using std::filesystem::create_directories;
using std::filesystem::create_directory;
using std::filesystem::current_path;
using std::filesystem::exists;
using std::filesystem::file_size;
using std::filesystem::filesystem_error;
using std::filesystem::path;
using std::format;
using std::function;
using std::get_if;
using std::is_same_v;
using std::list;
using std::lock_guard;
using std::make_pair;
using std::make_shared;
using std::make_tuple;
using std::make_unique;
using std::map;
using std::move;
using std::multimap;
using std::mutex;
using std::nullptr_t;
using std::ofstream;
using std::pair;
using std::queue;
using std::ref;
using std::scoped_lock;
using std::set;
using std::shared_ptr;
using std::string_view;
using std::string;
using std::stringstream;
using std::this_thread::sleep_for;
using std::thread;
using std::tuple;
using std::unique_lock;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::variant;
using std::vector;
using std::visit;
using stm = std::tm;
#endif // ENABLE_HEADERS_STD

// qt
// #define ENABLE_HEADERS_QT
#ifdef ENABLE_HEADERS_QT
#include <QAbstractListModel>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBitmap>
#include <QBuffer>
#include <QButtonGroup>
#include <QByteArray>
#include <QByteArrayList>
#include <QByteArrayMatcher>
#include <QByteArrayView>
#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDate>
#include <QDateEdit>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>          // 遍历子目录，较老版本 // QDir::entryList()
#include <QDirIterator>  // 遍历子目录，较新版本，单个遍历
#include <QDirListing>   // 遍历子目录，最新版本，c++20 方式
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGrpcChannelOptions>
#include <QGrpcHttp2Channel>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QList>
#include <QListWidgetItem>
#include <QLocale>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMetaEnum>
#include <QMimeDatabase>
#include <QMimeType>
#include <QMovie>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPicture>
#include <QPixmap>
#include <QProcess>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QProtobufJsonSerializer>
#include <QProtobufMessage>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>
#include <QRegularExpressionValidator>
#include <QSaveFile>
#include <QScopedArrayPointer>
#include <QScopedPointer>
#include <QScopeGuard>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
#include <QSemaphore>
#include <QSet>
#include <QSettings>
#include <QSharedData>
#include <QSharedDataPointer>
#include <QSharedMemory>
#include <QSharedPointer>
#include <QShortcut>
#include <QSize>
#include <QSpinBox>
#include <QSplitter>
#include <QSslCipher>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QSvgGenerator>
#include <QSvgRenderer>
#include <QSvgWidget>
#include <QSyntaxHighlighter>
#include <QSysInfo>
#include <QSystemTrayIcon>
#include <QtConcurrent/QtConcurrent>
#include <QtDebug>
#include <QtGrpc/QGrpcStatus>
#include <QThread>
#include <QThreadPool>
#include <QTimeEdit>
#include <QTimeLine>
#include <QTimer>
#include <QTimeZone>
#include <QToolBar>
#include <QToolBox>
#include <QToolButton>
#include <QToolTip>
#include <QTranslator>
#include <QtSvg/QSvgGenerator>
#include <QUrl>
#include <QUuid>
#include <QVariant>
#include <QVector>
#include <QWidget>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#endif // ENABLE_HEADERS_QT

// 宏工具：将宏参数转换为字符串
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define ENABLE_QT_MACRO
#ifdef ENABLE_QT_MACRO
// 宏展开：强制 Q_INIT_RESOURCE 的参数先进行宏展开
#define Q_INIT_RESOURCE_EXPAND(name) Q_INIT_RESOURCE(name)
// 仅支持 std 输入，不支持 QT 输入
#define STD_VARGS_TO_QSTRING(...) QString::fromStdString(fmt::format(__VA_ARGS__))
#endif // ENABLE_QT_MACRO

// #define ENABLE_SQLCIPHER
#ifdef ENABLE_SQLCIPHER
// sqlcipher
// #define SQLITE_ENABLE_FTS5
// #define SQLITE_ENABLE_JSON1
// #define SQLITE_ENABLE_MEMORY_MANAGEMENT
// #define SQLITE_ENABLE_STAT4
// #define SQLITE_ENABLE_UNLOCK_NOTIFY
#ifndef SQLITE_HAS_CODEC
#define SQLITE_HAS_CODEC  // 允许加密
#endif // SQLITE_HAS_CODEC
#if __has_include(<sqlcipher/sqlite3.h>)
#include <sqlcipher/sqlite3.h>
#elif __has_include(<sqlite3.h>)
#include <sqlite3.h> // ubuntu homebrew `pkg-config --cflags sqlcipher` 输出的目录要去掉一层
#else
#error "SQLCipher header not found: expected <sqlcipher/sqlite3.h> or <sqlite3.h>"
#endif
#endif // ENABLE_SQLCIPHER

// #define ENABLE_NLOHMANN_JSON
#ifdef ENABLE_NLOHMANN_JSON
// nlohmann json
#include <nlohmann/json.hpp>
#endif // ENABLE_NLOHMANN_JSON

// #define ENABLE_M_INI
#ifdef ENABLE_M_INI
// m-ini
#include <mini/ini.h>   // cpp
#endif // ENABLE_M_INI

// #define ENABLE_BOOST_DLL
#ifdef ENABLE_BOOST_DLL
// boost dll  // 用于获取当前可执行文件的路径
#include <boost/dll.hpp>
#endif // ENABLE_BOOST_DLL

// #define ENABLE_BOOST_UUID
#ifdef ENABLE_BOOST_UUID
// boost uuid
#include <boost/uuid.hpp>
using boost::uuids::random_generator;
using boost::uuids::string_generator;
using boost::uuids::uuid;
#endif // ENABLE_BOOST_UUID

// #define ENABLE_BOOST_PROGRAM_OPTIONS
#ifdef ENABLE_BOOST_PROGRAM_OPTIONS
// boost program_options
#include <boost/program_options.hpp>
#endif // ENABLE_BOOST_PROGRAM_OPTIONS

#ifdef ENABLE_STDUUID
#include <stduuid/uuid.h>
#endif // ENABLE_STDUUID

// hash-library
#ifdef ENABLE_HASH_LIBRARY
#include <crc32.h>
#include <hash.h>
#include <hmac.h>
#include <keccak.h>
#include <md5.h>
#include <sha1.h>
#include <sha3.h>
#include <sha256.h>
#endif // ENABLE_HASH_LIBRARY

// #define ENABLE_FMT
#ifdef ENABLE_FMT
#include <fmt/format.h>
#endif // ENABLE_FMT

#ifdef ENABLE_ABSL_LIB
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/absl_log.h"
#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/log/globals.h"
#include "absl/strings/str_format.h"
#endif // ENABLE_ABSL_LIB

// #define ENABLE_GOOGLE_GRPC
#ifdef ENABLE_GOOGLE_GRPC
// grpc
#include <google/protobuf/util/time_util.h>  // google::protobuf::util::TimeUtil::GetCurrentTime();
#include <grpcpp/alarm.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
using ::google::protobuf::Timestamp;
using ::grpc::CallbackServerContext;
using ::grpc::Channel;
using ::grpc::ClientContext;
using ::grpc::Server;
using ::grpc::ServerBuilder;
using ::grpc::ServerContext;
using ::grpc::ServerUnaryReactor;
using ::grpc::Status;
using ::grpc::StatusCode;
#endif // ENABLE_GOOGLE_GRPC

// grpc
#ifdef ENABLE_CLIENT
// client
#include "taotie_chat.qpb.h"
#include "taotie_chat_client.grpc.qpb.h"
using AccountClient = ::TaoTieChat::AccountService::Client;
using FileClient    = ::TaoTieChat::FileService::Client;
using MessageClient = ::TaoTieChat::MessageService::Client;
using SearchClient  = ::TaoTieChat::SearchService::Client;
using StatusClient  = ::TaoTieChat::StatusService::Client;
using SyncClient    = ::TaoTieChat::SyncService::Client;
using ::TaoTieChat::ContactReplyCodeGadget::ContactReplyCode;
using ::TaoTieChat::ErrorCodeGadget::ErrorCode;
using ::TaoTieChat::FileStatusCodeGadget::FileStatusCode;
using ::TaoTieChat::PermissionCodeGadget::PermissionCode;
using ::TaoTieChat::SuccessCodeGadget::SuccessCode;
using ::TaoTieChat::SyncSubTypeContactGadget::SyncSubTypeContact;
using ::TaoTieChat::SyncSubTypeGroupGadget::SyncSubTypeGroup;
using ::TaoTieChat::SyncSubTypeMessageGadget::SyncSubTypeMessage;
using ::TaoTieChat::SyncTypeGadget::SyncType;
#elif defined(ENABLE_SERVER)  // ENABLE_SERVER
// server
#include "taotie_chat.grpc.pb.h"
using ::grpc::Alarm;
using ::grpc::AuthContext;
using ::grpc::ByteBuffer;
using ::grpc::CallbackServerContext;
using ::grpc::ChannelInterface;
using ::grpc::ClientAsyncReader;
using ::grpc::ClientAsyncReaderInterface;
using ::grpc::ClientAsyncResponseReader;
using ::grpc::ClientAsyncResponseReaderInterface;
using ::grpc::ClientAsyncWriter;
using ::grpc::ClientAsyncWriterInterface;
using ::grpc::ClientContext;
using ::grpc::ClientReader;
using ::grpc::ClientReaderInterface;
using ::grpc::ClientReadReactor;
using ::grpc::ClientUnaryReactor;
using ::grpc::ClientWriter;
using ::grpc::ClientWriteReactor;
using ::grpc::ClientWriterInterface;
using ::grpc::CompletionQueue;
using ::grpc::internal::CallbackClientStreamingHandler;
using ::grpc::internal::CallbackServerStreamingHandler;
using ::grpc::internal::CallbackUnaryHandler;
using ::grpc::internal::RpcMethod;
using ::grpc::internal::SplitServerStreamingHandler;
using ::grpc::internal::StreamedUnaryHandler;
using ::grpc::MessageAllocator;
using ::grpc::Server;
using ::grpc::ServerAsyncResponseWriter;
using ::grpc::ServerBuilder;
using ::grpc::ServerCompletionQueue;
using ::grpc::ServerContext;
using ::grpc::ServerReader;
using ::grpc::ServerReaderWriter;
using ::grpc::ServerReadReactor;
using ::grpc::ServerSplitStreamer;
using ::grpc::ServerUnaryReactor;
using ::grpc::ServerUnaryStreamer;
using ::grpc::ServerWriter;
using ::grpc::ServerWriteReactor;
using ::grpc::Service;
using ::grpc::Status;
using ::grpc::StatusCode;
using ::grpc::StubOptions;

// enums
using ::TaoTieChat::ContactReplyCode;
using ::TaoTieChat::ErrorCode;
using ::TaoTieChat::FileStatusCode;
using ::TaoTieChat::PermissionCode;
using ::TaoTieChat::SuccessCode;
using ::TaoTieChat::SyncType;
// service
using ::TaoTieChat::AccountService;
using ::TaoTieChat::FileService;
using ::TaoTieChat::SearchService;
using ::TaoTieChat::StatusService;
using ::TaoTieChat::SyncService;
using ::TaoTieChat::MessageService;
// stubs
using AccountStub = ::TaoTieChat::AccountService::Stub;
using FileStub    = ::TaoTieChat::FileService::Stub;
using MessageStub = ::TaoTieChat::MessageService::Stub;
using SearchStub  = ::TaoTieChat::SearchService::Stub;
using StatusStub  = ::TaoTieChat::StatusService::Stub;
using SyncStub    = ::TaoTieChat::SyncService::Stub;
#endif // ENABLE_CLIENT

// #define ENABLE_PROTO_PROJECT_NAMESPACE
#ifdef ENABLE_PROTO_PROJECT_NAMESPACE
using ::TaoTieChat::AuthReply;
using ::TaoTieChat::AuthRequest;
using ::TaoTieChat::ContactInfo;
using ::TaoTieChat::ContactInfoBase;
using ::TaoTieChat::ContactInfoFriend;
using ::TaoTieChat::ContactInfos;
using ::TaoTieChat::ContactInfoSelf;  // 实际未使用
using ::TaoTieChat::ContactReplyReply;
using ::TaoTieChat::ContactReplyRequest;
using ::TaoTieChat::ContactRequestReply;
using ::TaoTieChat::ContactRequestRequest;
using ::TaoTieChat::FileChunkData;
using ::TaoTieChat::FileDownloadHeader;
using ::TaoTieChat::FileDownloadReply;
using ::TaoTieChat::FileDownloadRequest;
using ::TaoTieChat::FileMetadata;
using ::TaoTieChat::FileUploadHeader;
using ::TaoTieChat::FileUploadReply;
using ::TaoTieChat::FileUploadRequest;
using ::TaoTieChat::GetContactRequestsReply;
using ::TaoTieChat::GetFileIDReply;
using ::TaoTieChat::GetFileIDRequest;
using ::TaoTieChat::GetLastIDReply;
using ::TaoTieChat::GetSecurityQuestionReply;
using ::TaoTieChat::MessageContext;
using ::TaoTieChat::MessageReply;
using ::TaoTieChat::MessageRequest;
using ::TaoTieChat::Result;
using ::TaoTieChat::SearchGroupReply;
using ::TaoTieChat::SearchGroupRequest;
using ::TaoTieChat::SearchUserReply;
using ::TaoTieChat::SearchUserRequest;
using ::TaoTieChat::SignInReply;
using ::TaoTieChat::SignInRequest;
using ::TaoTieChat::SignUpReply;
using ::TaoTieChat::SignUpRequest;
using ::TaoTieChat::StatusReply;
using ::TaoTieChat::StatusRequest;
using ::TaoTieChat::SyncContact;
using ::TaoTieChat::SyncContactReply;
using ::TaoTieChat::SyncContactRequest;
using ::TaoTieChat::SyncContacts;
using ::TaoTieChat::SyncContactShip;
using ::TaoTieChat::SyncContactUpdate;
using ::TaoTieChat::SyncMessage;
using ::TaoTieChat::SyncMessages;
using ::TaoTieChat::SyncReply;
using ::TaoTieChat::SyncReplyBody;
using ::TaoTieChat::SyncReplyHeader;
using ::TaoTieChat::SyncRequest;
using ::TaoTieChat::SyncSubType;
using ::TaoTieChat::SyncTag;
using ::TaoTieChat::UpdateProfileReply;
using ::TaoTieChat::UpdateProfileRequest;
using ::TaoTieChat::UpdateSecurityInfoNewPasswordInfo;
using ::TaoTieChat::UpdateSecurityInfoNewSecurityInfo;
using ::TaoTieChat::UpdateSecurityInfoOldPasswordInfo;
using ::TaoTieChat::UpdateSecurityInfoOldSecurityInfo;
using ::TaoTieChat::UpdateSecurityInfoReply;
using ::TaoTieChat::UpdateSecurityInfoRequest;
#endif  // ENABLE_PROTO_PROJECT_NAMESPACE

// logs
// VERBOSE / NOTICE / EMERGENCY / PANIC / AUDIT / ALERT
// FATAL / CRITICAL / ERROR / WARNING / INFO / DEBUG / TRACE
// #define ENABLE_SIMPLE_QT_LOGGER
// #define ENABLE_SPDLOG
// #define ENABLE_GLOG
// #define ENABLE_ABSL_LOG
#if defined(ENABLE_SIMPLE_QT_LOGGER)
#include <simpleQtLogger.h>
#define LOG_TRACE(...)    L_DEBUG(__VA_ARGS__)
#define LOG_DEBUG(...)    L_DEBUG(__VA_ARGS__)
#define LOG_INFO(...)     L_INFO(__VA_ARGS__)
#define LOG_WARNING(...)  L_WARN(__VA_ARGS__)
#define LOG_ERROR(...)    L_ERROR(__VA_ARGS__)
#define LOG_CRITICAL(...) L_ERROR(__VA_ARGS__)
#define LOG_FATAL(...)    L_FATAL(__VA_ARGS__)
// 只支持 std 类型输入，不支持 qt 类型输入
#define LOG_TRACEV(...)    LOG_TRACE(STD_VARGS_TO_QSTRING(__VA_ARGS__))
#define LOG_DEBUGV(...)    LOG_DEBUG(STD_VARGS_TO_QSTRING(__VA_ARGS__))
#define LOG_INFOV(...)     LOG_INFO(STD_VARGS_TO_QSTRING(__VA_ARGS__))
#define LOG_WARNINGV(...)  LOG_WARNING(STD_VARGS_TO_QSTRING(__VA_ARGS__))
#define LOG_ERRORV(...)    LOG_ERROR(STD_VARGS_TO_QSTRING(__VA_ARGS__))
#define LOG_CRITICALV(...) LOG_CRITICAL(STD_VARGS_TO_QSTRING(__VA_ARGS__))
#define LOG_FATALV(...)    LOG_FATAL(STD_VARGS_TO_QSTRING(__VA_ARGS__))
#elif defined(ENABLE_SPDLOG)
// spdlog
// 1. 必须在 include 之前定义，指定允许输出的最低日志级别
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE // 允许输出 trace 级别及以上的日志
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>
#include <spdlog/async.h>
#include <spdlog/mdc.h>
#include <spdlog/cfg/env.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/callback_sink.h>
// #include "spdlog/sinks/qt_sinks.h"
// #include "spdlog/sinks/syslog_sink.h"
#define LOG_TRACE(...)    SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...)    SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...)     SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARNING(...)  SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...)    SPDLOG_ERROR(__VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)
#define LOG_FATAL(...)    SPDLOG_CRITICAL(__VA_ARGS__)
#elif defined(ENABLE_GLOG)
#include <glog/logging.h>
#define LOG_TRACE(...)    LOG(GLOG_INFO) << __VA_ARGS__
#define LOG_DEBUG(...)    LOG(GLOG_INFO) << __VA_ARGS__
#define LOG_INFO(...)     LOG(GLOG_INFO) << __VA_ARGS__
#define LOG_WARNING(...)  LOG(GLOG_WARNING) << __VA_ARGS__
#define LOG_ERROR(...)    LOG(GLOG_ERROR) << __VA_ARGS__
#define LOG_CRITICAL(...) LOG(GLOG_FATAL) << __VA_ARGS__
#define LOG_FATAL(...)    LOG(GLOG_FATAL) << __VA_ARGS__
#elif defined(ENABLE_ABSL_LOG)
// absl
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/absl_log.h"
#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/log/globals.h"
#include "absl/strings/str_format.h"
#define LOG_TRACE(...)    ABSL_LOG(GLOG_INFO) << __VA_ARGS__
#define LOG_DEBUG(...)    ABSL_LOG(GLOG_INFO) << __VA_ARGS__
#define LOG_INFO(...)     ABSL_LOG(GLOG_INFO) << __VA_ARGS__
#define LOG_WARNING(...)  ABSL_LOG(GLOG_WARNING) << __VA_ARGS__
#define LOG_ERROR(...)    ABSL_LOG(GLOG_ERROR) << __VA_ARGS__
#define LOG_CRITICAL(...) ABSL_LOG(GLOG_FATAL) << __VA_ARGS__
#define LOG_FATAL(...)    ABSL_LOG(GLOG_FATAL) << __VA_ARGS__
#elif defined(ENABLE_QDEBUG_LOGGER)
#include <QDebug>
#define LOG_TRACE(...)    qDebug() << __VA_ARGS__
#define LOG_DEBUG(...)    qDebug() << __VA_ARGS__
#define LOG_INFO(...)     qInfo() << __VA_ARGS__
#define LOG_WARNING(...)  qWarning() << __VA_ARGS__
#define LOG_ERROR(...)    qWarning() << __VA_ARGS__
#define LOG_CRITICAL(...) qCritical() << __VA_ARGS__
#define LOG_FATAL(...)    qFatal() << __VA_ARGS__
#define LOG_TRACEV(...)    LOG_TRACE(STD_VARGS_TO_QSTRING(__VA_ARGS__))
#define LOG_DEBUGV(...)    LOG_DEBUG(STD_VARGS_TO_QSTRING(__VA_ARGS__))
#define LOG_INFOV(...)     LOG_INFO(STD_VARGS_TO_QSTRING(__VA_ARGS__))
#define LOG_WARNINGV(...)  LOG_WARNING(STD_VARGS_TO_QSTRING(__VA_ARGS__))
#define LOG_ERRORV(...)    LOG_ERROR(STD_VARGS_TO_QSTRING(__VA_ARGS__))
#define LOG_CRITICALV(...) LOG_CRITICAL(STD_VARGS_TO_QSTRING(__VA_ARGS__))
#define LOG_FATALV(...)    LOG_FATAL(STD_VARGS_TO_QSTRING(__VA_ARGS__))
#else
#define LOG_TRACE(...)    std::cout << __VA_ARGS__ << std::endl
#define LOG_DEBUG(...)    std::cout << __VA_ARGS__ << std::endl
#define LOG_INFO(...)     std::cout << __VA_ARGS__ << std::endl
#define LOG_WARNING(...)  std::cout << __VA_ARGS__ << std::endl
#define LOG_ERROR(...)    std::cerr << __VA_ARGS__ << std::endl
#define LOG_CRITICAL(...) std::cerr << __VA_ARGS__ << std::endl
#define LOG_FATAL(...)    std::cerr << __VA_ARGS__ << std::endl
#endif  // LOGGER_USE_SPDLOG
