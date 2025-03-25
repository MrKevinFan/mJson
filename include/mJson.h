//
// Created by fanmin on 2025-03-25.
//
#ifndef MJSON_H
#define MJSON_H
#include <stddef.h>
#include <float.h>
#include <stdbool.h>

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_INT,
    JSON_FLOAT,
    JSON_DOUBLE,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

typedef struct JsonValue JsonValue;
typedef struct JsonPair JsonPair;

struct JsonValue {
    JsonType type;
    union {
        bool bool_value;
        int int_value;
        float float_value; // 32位单精度
        double double_value;  // 64位双精度
        char *string_value;
        struct {
            JsonValue *elements;
            size_t count;
        } array_value;
        struct {
            JsonPair *pairs;
            size_t count;
        } object_value;
    } value;
};

struct JsonPair {
    char *key;
    JsonValue value;
};

// 解析接口
JsonValue json_parse(const char *json, int *error);
void json_free(JsonValue *value);

// 查询接口
JsonValue *json_get(const JsonValue *obj, const char *path);

// 错误码
#define JSON_SUCCESS 0
#define JSON_INVALID 1
#define JSON_MEM_ERROR 2

#endif
