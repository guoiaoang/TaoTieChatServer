#include "headers.h"
#include "utils.h"

#include "DBSQLCipherCmds.h"
#include "DBSQLCipher.h"
#include "AccountService.h"
#include "FileService.h"
#include "MessageService.h"
#include "SearchService.h"
#include "StatusService.h"
#include "SyncService.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using std::shared_ptr;
using std::make_shared;
using std::unique_ptr;
using std::lock_guard;
using std::unique_lock;
using std::condition_variable;
using std::mutex;
using std::string;
using std::function;
using std::filesystem::path;
using std::filesystem::current_path;
using std::filesystem::exists;
using std::filesystem::filesystem_error;
using std::format;

static string get_current_file_name(const string& full_path) {
    // __FILE__ 是一个预定义的宏，包含当前源文件的路径
    // string full_path = __FILE__;
    // 使用 std::filesystem::path 来处理路径
    path p(full_path);
    // 返回文件名部分
    return p.stem    ().string();  // stem() 返回不带扩展名的文件名
    return p.filename().string();  // filename() 返回带扩展名的文件名
}
