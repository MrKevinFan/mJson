//
// Created by fanmin on 2025-03-25.
//
#ifndef MJSON_H
#define MJSON_H
#include <stddef.h>
#include <float.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

// 类型校验示例
#define CHECK_TYPE(jv, expected) \
    do { if (!jv || jv->type != (expected)) return NULL; } while(0)

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
            size_t  ele_count;
        } array_value;
        struct {
            JsonPair *pairs;
            size_t pair_count;
        } object_value;
    } value;
};

struct JsonPair {
    char *key;
    JsonValue value;
};

// 2025年3月26日 新增  创建基础类型实例
JsonValue* create_bool(bool val);
JsonValue* create_int(int val);
JsonValue* create_float(float val);
JsonValue* create_double(double val);
JsonValue* create_string(const char* val);
JsonValue* create_array();
int array_append(JsonValue* array, JsonValue* element);
JsonValue* create_object();
int object_add_pair(JsonValue* obj, const char* key, JsonValue* value);
char* json_to_string(const JsonValue* jv);

// 解析接口
JsonValue json_parse(const char *json, int *error);
void json_free(JsonValue *value);

// 查询接口
JsonValue *json_get(const JsonValue *obj, const char *path);
int json_insert(JsonValue** root, const char* path, JsonValue* new_item);
// 错误码
#define JSON_SUCCESS 0
#define JSON_INVALID 1
#define JSON_MEM_ERROR 2

#endif
