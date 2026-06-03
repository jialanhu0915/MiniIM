/**
 * @file   JsonUtils.h
 * @brief  轻量级手写 JSON 工具（无外部依赖，UTF-8 字符串处理）
 * @author Yan Runxin
 * @date   2026-05-25
 */
#pragma once
#include <string>

/**
 * @brief 从 JSON 字符串中提取字符串值
 * @param json UTF-8 JSON 字符串
 * @param key 键名
 * @return 未找到返回空字符串
 * @note 仅支持 "key":"value"，不处理转义
 */
// 从 JSON 中提取字符串值：{"key":"value"} → "value"
inline std::string JsonGetString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.length();
    size_t end = json.find("\"", pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

/**
 * @brief 从 JSON 字符串中提取整数值
 * @param json UTF-8 JSON 字符串
 * @param key 键名
 * @return 未找到或错误返回 0
 */
// 从 JSON 中提取整数值：{"key":123} 123
inline int JsonGetInt(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return 0;
    pos += search.length();
    std::string num;
    while (pos < json.size() && (isdigit(json[pos]) || json[pos] == '-'))
        num += json[pos++];
    return num.empty() ? 0 : std::stoi(num);
}

/**
 * @brief 构造 "key":"value" JSON 片段
 * @param key 键名
 * @param value 字符串值
 * @return "key":"value" 片段（不含外层 {}）
 */
// 构造 "key":"value" 片段
inline std::string JsonSetString(const std::string& key, const std::string& value) {
    return "\"" + key + "\":\"" + value + "\"";
}

/**
 * @brief 构造 "key":123 JSON 片段
 * @param key 键名
 * @param value 整数值
 * @return "key":123 片段（不含外层 {}）
 */
// 构造 "key":123 片段
inline std::string JsonSetInt(const std::string& key, int value) {
    return "\"" + key + "\":" + std::to_string(value);
}

/**
 * @brief 构造 64 位整数字段（用于大文件 filesize 等）
 * @param key 字段名
 * @param value 64 位整数值
 * @return "key":123 片段（不含外层 {}）
 */
inline std::string JsonSetLongLong(const std::string& key, long long value) {
    return "\"" + key + "\":" + std::to_string(value);
}

/**
 * @brief 将多个 JSON 片段拼装成完整 JSON 对象
 * @param pairs initializer_list of JSON 片段
 * @return {"k1":v1,...}
 */
// 构造简单 JSON 对象 {"k1":v1,"k2":v2,...}
// 用法: JsonMakeObject({JsonSetInt("id",1), JsonSetString("name","A")})
inline std::string JsonMakeObject( std::initializer_list<std::string> pairs) {
    std::string result = "{";
    bool first = true;
    for (auto& p : pairs) {
        if (!first) result += ",";
        result += p;
        first = false;
    }
    result += "}";
    return result;
}

/**
 * @brief 从 JSON 字符串中提取数组元素
 * @param json UTF-8 JSON 字符串
 * @param key 数组所在字段名
 * @return 元素 JSON 字符串数组（未找到返回空 vector）
 */
// 从 JSON 中提取数组元素：{"key":[{...},{...}]} → vector<每个{...}的字符串>
// 算法：找到 "key":[  然后逐个匹配 { } 对，提取每个完整的对象字符串
inline std::vector<std::string> JsonGetArray(const std::string& json,
                                              const std::string& key) {
    std::vector<std::string> result;
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return result;
    pos += search.length();

    // 跳过空白，找 '['
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
        ++pos;
    if (pos >= json.size() || json[pos] != '[') return result;
    ++pos;  // 跳过 '['

    // 逐个提取 { ... } 对象
    while (pos < json.size()) {
        // 跳过空白和逗号
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'
               || json[pos] == ',' || json[pos] == '\r' || json[pos] == '\n'))
            ++pos;
        if (pos >= json.size() || json[pos] == ']') break;

        // 找到 '{' 开始匹配括号深度
        if (json[pos] != '{') { ++pos; continue; }
        size_t start = pos;
        int depth = 0;
        while (pos < json.size()) {
            if (json[pos] == '{') ++depth;
            else if (json[pos] == '}') { --depth; if (depth == 0) { ++pos; break; } }
            ++pos;
        }
        result.push_back(json.substr(start, pos - start));
    }
    return result;
}

