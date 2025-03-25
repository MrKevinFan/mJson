//
// Created by fanmin on 2025-03-25.
//
#include "mJson.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

typedef struct {
    const char *start;
    const char *pos;
    char *error;
} ParserContext;

/**
 * 词法分析
 * @param ctx
 */
static void skip_whitespace(ParserContext *ctx) {
    while (isspace(*ctx->pos)) ctx->pos++;
}

// 递归解析
static JsonValue parse_value(ParserContext *ctx, int *error);

static int parse_hex(ParserContext *ctx) {
    int hex = 0;
    for (int i = 0; i < 4; i++) {
        char c = *ctx->pos++;
        hex *= 16;
        if (c >= '0' && c <= '9') hex += c - '0';
        else if (c >= 'a' && c <= 'f') hex += 10 + c - 'a';
        else if (c >= 'A' && c <= 'F') hex += 10 + c - 'A';
        else return -1;
    }
    return hex;
}

/**
 * 精确统计有效位数（处理前导零、末尾零、科学计数法）
 * @param start
 * @param end
 * @return
 */
static int count_significant_digits(const char *start, const char *end) {
    const char *p = start;
    int significant = 0;
    bool has_decimal = false;

    // 跳过符号
    if (*p == '+' || *p == '-') {
        p++;
    }

    // 分割整数和小数部分
    const char *int_start = p;
    const char *int_end = p;
    const char *frac_start = NULL;
    const char *frac_end = NULL;

    // 定位整数部分结束位置
    while (p < end && *p != '.' && *p != 'e' && *p != 'E') {
        p++;
    }
    int_end = p;

    // 处理小数部分
    if (p < end && *p == '.') {
        has_decimal = true;
        p++;
        frac_start = p;
        while (p < end && *p != 'e' && *p != 'E') {
            p++;
        }
        frac_end = p;
    }

    // 统计整数部分有效位数
    const char *int_p = int_start;
    bool int_non_zero_found = false;
    const char *int_first_non_zero = NULL;
    const char *int_last_non_zero = NULL;

    while (int_p < int_end) {
        if (isdigit(*int_p)) {
            if (*int_p != '0') {
                if (!int_non_zero_found) {
                    int_first_non_zero = int_p;
                }
                int_last_non_zero = int_p;
                int_non_zero_found = true;
            } else if (int_non_zero_found) {
                int_last_non_zero = int_p; // 中间的零计入有效位
            }
        }
        int_p++;
    }

    if (int_non_zero_found) {
        significant += (int_last_non_zero - int_first_non_zero + 1);
    }

    // 统计小数部分有效位数
    if (has_decimal) {
        const char *frac_p = frac_start;
        bool frac_non_zero_found = false;
        const char *frac_first_non_zero = NULL;
        const char *frac_last_non_zero = NULL;

        while (frac_p < frac_end) {
            if (isdigit(*frac_p)) {
                if (*frac_p != '0') {
                    if (!frac_non_zero_found) {
                        frac_first_non_zero = frac_p;
                    }
                    frac_last_non_zero = frac_p;
                    frac_non_zero_found = true;
                } else if (frac_non_zero_found) {
                    frac_last_non_zero = frac_p; // 中间的零计入有效位
                }
            }
            frac_p++;
        }

        if (frac_non_zero_found) {
            significant += (frac_last_non_zero - frac_first_non_zero + 1);
        }
    }

    return significant;
}

/**
 * 解析数字
 * @param ctx
 * @return
 */
JsonValue parse_number(ParserContext *ctx) {
    const char *start = ctx->pos;
    char *end_ptr;
    double dbl_val = strtod(start, &end_ptr);
    if (end_ptr == start) {
        return (JsonValue){JSON_NULL,{0}};
    }
    ctx->pos = end_ptr;
    // --- 判断整数 ---
    bool is_integer = (fmod(dbl_val, 1.0) == 0.0);
    if (is_integer && dbl_val >= INT_MIN && dbl_val <= INT_MAX) {
        return (JsonValue){JSON_INT, .value.int_value = (int)dbl_val};
    }
    // --- 计算有效位数 ---
    int significant_digits = count_significant_digits(start, end_ptr);
    // --- 判断是否适合 float ---
    if (significant_digits <= FLT_DIG && dbl_val >= -FLT_MAX && dbl_val <= FLT_MAX) {
        return (JsonValue){JSON_FLOAT, .value.float_value = (float)dbl_val};
    }
    // --- 默认返回 double ---
    return (JsonValue){JSON_DOUBLE, .value.double_value = dbl_val};
}

/**
 * 解析字符串
 * @param ctx
 * @param error
 * @return
 */
static char *parse_string(ParserContext *ctx, int *error) {
    if (*ctx->pos != '"') {
        *error = JSON_INVALID;
        return NULL;
    }
    ctx->pos++;

    char *buffer = malloc(256);
    size_t capacity = 256, length = 0;

    while (*ctx->pos != '"') {
        if (*ctx->pos == '\\') {
            ctx->pos++;
            switch (*ctx->pos++) {
                case '"':  buffer[length++] = '"';  break;
                case '\\': buffer[length++] = '\\'; break;
                case '/':  buffer[length++] = '/';  break;
                case 'b':  buffer[length++] = '\b'; break;
                case 'f':  buffer[length++] = '\f'; break;
                case 'n':  buffer[length++] = '\n'; break;
                case 'r':  buffer[length++] = '\r'; break;
                case 't':  buffer[length++] = '\t'; break;
                case 'u': {
                    int codepoint = parse_hex(ctx);
                    // 简化处理：只支持基本多语言平面
                    if (codepoint <= 0x7F) {
                        buffer[length++] = codepoint;
                    } else if (codepoint <= 0x7FF) {
                        buffer[length++] = 0xC0 | (codepoint >> 6);
                        buffer[length++] = 0x80 | (codepoint & 0x3F);
                    }
                    break;
                }
                default: *error = JSON_INVALID; break;
            }
        } else {
            buffer[length++] = *ctx->pos++;
        }

        // 动态扩容
        if (length >= capacity - 4) {
            capacity *= 2;
            char *new_buf = realloc(buffer, capacity);
            if (!new_buf) {
                *error = JSON_MEM_ERROR;
                free(buffer);
                return NULL;
            }
            buffer = new_buf;
        }
    }
    ctx->pos++;
    buffer[length] = '\0';
    return buffer;
}

/**
 * 解析数组
 * @param ctx
 * @param error
 * @return
 */
static JsonValue parse_array(ParserContext *ctx, int *error) {
    JsonValue arr = {JSON_ARRAY, {.array_value = {NULL, 0}}};
    ctx->pos++;  // 跳过'['

    while (1) {
        skip_whitespace(ctx);
        if (*ctx->pos == ']') {
            ctx->pos++;
            return arr;
        }

        JsonValue element = parse_value(ctx, error);
        if (*error) {
            json_free(&arr);
            return (JsonValue){0};
        }

        // 扩容数组
        arr.value.array_value.elements = realloc(arr.value.array_value.elements,
                                           (arr.value.array_value.count + 1) * sizeof(JsonValue));
        arr.value.array_value.elements[arr.value.array_value.count++] = element;

        skip_whitespace(ctx);
        if (*ctx->pos == ',') ctx->pos++;
        else if (*ctx->pos != ']') *error = JSON_INVALID;
    }
}
/**
 * 解析对象
 * @param ctx
 * @param error
 * @return
 */
static JsonValue parse_object(ParserContext *ctx, int *error) {
    JsonValue obj = {JSON_OBJECT, {.object_value = {NULL, 0}}};
    ctx->pos++;  // 跳过'{'

    while (1) {
        skip_whitespace(ctx);
        if (*ctx->pos == '}') {
            ctx->pos++;
            return obj;
        }

        // 解析key
        int key_error = JSON_SUCCESS;
        char *key = parse_string(ctx, &key_error);
        if (key_error) {
            *error = key_error;
            json_free(&obj);
            return (JsonValue){0};
        }

        skip_whitespace(ctx);
        if (*ctx->pos != ':') {
            *error = JSON_INVALID;
            free(key);
            json_free(&obj);
            return (JsonValue){0};
        }
        ctx->pos++;

        // 解析value
        JsonValue value = parse_value(ctx, error);
        if (*error) {
            free(key);
            json_free(&obj);
            return (JsonValue){0};
        }

        // 添加键值对
        obj.value.object_value.pairs = realloc(obj.value.object_value.pairs,
                                         (obj.value.object_value.count + 1) * sizeof(JsonPair));
        obj.value.object_value.pairs[obj.value.object_value.count] = (JsonPair){
                .key = key,
                .value = value
        };
        obj.value.object_value.count++;

        skip_whitespace(ctx);
        if (*ctx->pos == ',') ctx->pos++;
        else if (*ctx->pos != '}') *error = JSON_INVALID;
    }
}
/**
 * 解析json对象中的值
 * @param ctx
 * @param error
 * @return
 */
static JsonValue parse_value(ParserContext *ctx, int *error) {
    skip_whitespace(ctx);
    switch (*ctx->pos) {
        case '{': return parse_object(ctx, error);
        case '[': return parse_array(ctx, error);
        case '"': {
            JsonValue str = {JSON_STRING,.value={0}};
            str.value.string_value = parse_string(ctx, error);
            return str;
        }
        case 't':  // true
            if (strncmp(ctx->pos, "true", 4) == 0) {
                ctx->pos += 4;
                return (JsonValue){JSON_BOOL, {.bool_value = true}};
            }
            break;
        case 'f':  // false
            if (strncmp(ctx->pos, "false", 5) == 0) {
                ctx->pos += 5;
                return (JsonValue){JSON_BOOL, {.bool_value = false}};
            }
            break;
        case 'n':  // null
            if (strncmp(ctx->pos, "null", 4) == 0) {
                ctx->pos += 4;
                return (JsonValue){JSON_NULL, {0}};
            }
            break;
        default: {
            // 解析数字
            if (isdigit(*ctx->pos) || *ctx->pos == '-') {
                return parse_number(ctx); // 数字解析函数
            }
        }
    }
    *error = JSON_INVALID;
    return (JsonValue){0};
}

/**
 * json字符串解析
 * @param json
 * @param error
 * @return
 */
JsonValue json_parse(const char *json, int *error) {
    ParserContext ctx = {json, json,NULL};
    int parse_error = JSON_SUCCESS;
    JsonValue result = parse_value(&ctx, &parse_error);

    if (parse_error) {
        json_free(&result);
        *error = parse_error;
        return (JsonValue){0};
    }

    skip_whitespace(&ctx);
    if (*ctx.pos != '\0') {
        json_free(&result);
        *error = JSON_INVALID;
    }

    *error = parse_error;
    return result;
}

void json_free(JsonValue *value) {
    switch (value->type) {
        case JSON_STRING:
            free(value->value.string_value);
            break;
        case JSON_ARRAY:
            for (size_t i = 0; i < value->value.array_value.count; i++)
                json_free(&value->value.array_value.elements[i]);
            free(value->value.array_value.elements);
            break;
        case JSON_OBJECT:
            for (size_t i = 0; i < value->value.object_value.count; i++) {
                free(value->value.object_value.pairs[i].key);
                json_free(&value->value.object_value.pairs[i].value);
            }
            free(value->value.object_value.pairs);
            break;
        default: break;
    }
}
/**
 * 获取json值
 * @param obj
 * @param path
 * @return
 */
JsonValue *json_get(const JsonValue *obj, const char *path) {
    if (!obj || !path) return NULL;
    JsonValue *current = (JsonValue *)obj;
    // strdup函数用于复制字符串的函数
    char *token, *tmp = strdup(path);
    char *saveptr = NULL;
    // 分割路径为多级查询
    for (token = strtok_r(tmp, ".", &saveptr); token; token = strtok_r(NULL, ".", &saveptr)) {
        // 检查数组索引格式 [数字]
        if (strchr(token, '[')) {
            char *index_str = strtok(token, "[]");
            if (!index_str) break;
            // strtol() 函数用来将字符串转换为长整型数(long)
            long  index = strtol(index_str, NULL, 10);
            if (current->type != JSON_ARRAY || index < 0 || index >= (long int)current->value.array_value.count) {
                free(tmp);
                return NULL;
            }
            current = &current->value.array_value.elements[index];
        }
        // 处理对象键
        else {
            if (current->type != JSON_OBJECT) {
                free(tmp);
                return NULL;
            }
            size_t i;
            for (i = 0; i < current->value.object_value.count; i++) {
                // 比较两个字符串并根据比较结果返回整数
                if (strcmp(current->value.object_value.pairs[i].key, token) == 0) {
                    current = &current->value.object_value.pairs[i].value;
                    break;
                }
            }
        }
    }
    free(tmp);
    return current;
}
