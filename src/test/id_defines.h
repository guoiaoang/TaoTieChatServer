// 定义
#pragma once
#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <map>
#include <set>
#include <vector>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <source_location>
#include <string_view>

consteval int ID_UNUSED_STRING(std::string_view x, 
    const std::source_location& loc = std::source_location::current()) { 
    return loc.line(); // 返回调用处的行号
}

// 保存 ID 对应的字符串，这里只是为了方便查看，实际编译链接不在这里
enum class ID_STRINGS : int {
    SERVER_LISTENING = ID_UNUSED_STRING(
        R"(Server is listening :\n\t{})"
        "hello world"),
    INVALID_ARGUMENT = ID_UNUSED_STRING("Invalid Argument"),
    NOT_FOUND = ID_UNUSED_STRING("Not Found"),
    INTERNAL_ERROR = ID_UNUSED_STRING("Internal Error"),
    PERMISSION_DENIED = ID_UNUSED_STRING("Permission Denied"),
    UNAUTHENTICATED = ID_UNUSED_STRING("Unauthenticated"),
    UNKNOWN_ERROR = ID_UNUSED_STRING("Unknown Error")
};

ID_STRINGS ext = ID_STRINGS::SERVER_LISTENING;

// char* sz = SERVER_LISTENING::str();
