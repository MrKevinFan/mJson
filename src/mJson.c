//
// Created by fanmin on 2025-03-25.
//
#include "mJson.h"

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
                                           (arr.value.array_value.ele_count + 1) * sizeof(JsonValue));
        arr.value.array_value.elements[arr.value.array_value.ele_count++] = element;

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
                                         (obj.value.object_value.pair_count + 1) * sizeof(JsonPair));
        obj.value.object_value.pairs[obj.value.object_value.pair_count] = (JsonPair){
                .key = key,
                .value = value
        };
        obj.value.object_value.pair_count++;

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
            for (size_t i = 0; i < value->value.array_value.ele_count; i++)
                json_free(&value->value.array_value.elements[i]);
            free(value->value.array_value.elements);
            break;
        case JSON_OBJECT:
            for (size_t i = 0; i < value->value.object_value.pair_count; i++) {
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
            if (current->type != JSON_ARRAY || index < 0 || index >= (long int)current->value.array_value.ele_count) {
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
            for (i = 0; i < current->value.object_value.pair_count; i++) {
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


// 解析路径字符串，拆分为层级结构
char** parse_path(const char* path, int* depth) {
    char* token;
    char** parts = NULL;
    char* path_copy = strdup(path);
    *depth = 0;

    token = strtok(path_copy, ".[]");
    while (token != NULL) {
        parts = realloc(parts, (*depth + 1) * sizeof(char*));
        parts[*depth] = strdup(token);
        (*depth)++;
        token = strtok(NULL, ".[]");
    }

    free(path_copy);
    return parts;
}

// =============================  2025年3月26日 新增功能 start ================================

/**
 * 创建基础类型实例
 * @param val
 * @return
 */
JsonValue* create_bool(bool val) {
    JsonValue *jv = malloc(sizeof(JsonValue));
    jv->type = JSON_BOOL;
    jv->value.bool_value = val;
    return jv;
}

JsonValue* create_int(int val) {
    JsonValue *jv = malloc(sizeof(JsonValue));
    jv->type = JSON_INT;
    jv->value.int_value = val;
    return jv;
}

JsonValue* create_float(float val) {
    JsonValue *jv = malloc(sizeof(JsonValue));
    jv->type = JSON_FLOAT;
    jv->value.float_value = val;
    return jv;
}

JsonValue* create_double(double val) {
    JsonValue *jv = malloc(sizeof(JsonValue));
    jv->type = JSON_DOUBLE;
    jv->value.double_value = val;
    return jv;
}

JsonValue* create_string(const char* val) {
    JsonValue *jv = malloc(sizeof(JsonValue));
    jv->type = JSON_STRING;
    jv->value.string_value = strdup(val); // 存储原始字符串;
    return jv;
}

/**
 * 创建数组容器
 * @return
 */
JsonValue* create_array() {
    JsonValue* jv = malloc(sizeof(JsonValue));
    if (!jv) return NULL;
    jv->type = JSON_ARRAY;
    jv->value.array_value.elements = NULL;
    jv->value.array_value.ele_count = 0;
    return jv;
}

/**
 * 创建对象容器
 * @return
 */
JsonValue* create_object() {
    JsonValue *jv = malloc(sizeof(JsonValue));
    if (!jv) return NULL;
    jv->type = JSON_OBJECT;
    jv->value.object_value.pairs = NULL;
    jv->value.object_value.pair_count = 0;
    return jv;
}

/**
 * 数组追加元素
 * @param array
 * @param element
 * @return
 */
int array_append(JsonValue* array, JsonValue* element) {
    if (!array || array->type != JSON_ARRAY) return 0;
    size_t new_count = array->value.array_value.ele_count + 1;
    JsonValue* new_elements = realloc(
            array->value.array_value.elements,
            sizeof(JsonValue*) * new_count
    );

    if (!new_elements) return 0;
    new_elements[new_count-1] = *element;
    array->value.array_value.elements = new_elements;
    array->value.array_value.ele_count = new_count;
    return 1;
}

/**
 * 对象添加键值对
 * @param obj
 * @param key
 * @param value
 * @return
 */
int object_add_pair(JsonValue* obj, const char* key, JsonValue* value) {
    if (!obj || obj->type != JSON_OBJECT || !key) return 0;
    size_t new_count = obj->value.object_value.pair_count + 1;
    JsonPair* new_pairs = realloc(
            obj->value.object_value.pairs,
            sizeof(JsonPair) * new_count
    );
    if (!new_pairs) return 0;
    char* key_copy = strdup(key);
    if (!key_copy) return 0;
    new_pairs[new_count-1] = (JsonPair){ key_copy, *value};
    obj->value.object_value.pairs = new_pairs;
    obj->value.object_value.pair_count = new_count;
    return 1;
}


/**
 * 批量添加
 * @param arr
 * @param elements
 * @param num
 */
void batch_append(JsonValue* arr, JsonValue** elements, size_t num) {
    size_t new_count = arr->value.array_value.ele_count + num;
    JsonValue** new_elements = realloc(
            arr->value.array_value.elements,
            sizeof(JsonValue*) * new_count
    );

    memcpy(&new_elements[arr->value.array_value.ele_count],
           elements,
           sizeof(JsonValue*) * num);
    arr->value.array_value.ele_count = new_count;
}

int array_insert_at(JsonValue* array, size_t index, JsonValue* element) {
    if (!array || array->type != JSON_ARRAY) return 0;
    if (index > array->value.array_value.ele_count) return 0;

    const size_t new_count = array->value.array_value.ele_count + 1;
    JsonValue** new_elements = realloc(
            array->value.array_value.elements,
            sizeof(JsonValue*) * new_count
    );
    if (!new_elements) return 0;

    // 移动插入点之后的元素
    memmove(&new_elements[index+1],
            &new_elements[index],
            sizeof(JsonValue*) * (array->value.array_value.ele_count - index));

    new_elements[index] = element;
    array->value.array_value.elements = *new_elements;
    array->value.array_value.ele_count = new_count;
    return 1;
}

int object_insert_at(JsonValue* obj, size_t index, const char* key, JsonValue* value) {
    if (!obj || obj->type != JSON_OBJECT || !key) return 0;
    if (index > obj->value.object_value.pair_count) return 0;

    const size_t new_count = obj->value.object_value.pair_count + 1;
    JsonPair* new_pairs = realloc(
            obj->value.object_value.pairs,
            sizeof(JsonPair) * new_count
    );
    if (!new_pairs) return 0;

    // 移动插入点之后的键值对
    memmove(&new_pairs[index+1],
            &new_pairs[index],
            sizeof(JsonPair) * (obj->value.object_value.pair_count - index));

    char* key_copy = strdup(key);
    if (!key_copy) {
        free(new_pairs); // 回滚内存分配
        return 0;
    }
    new_pairs[index] = (JsonPair){ key_copy, *value };
    obj->value.object_value.pairs = new_pairs;
    obj->value.object_value.pair_count = new_count;
    return 1;
}
/**
 * 获取数组中指定下标的值
 * @param arr
 * @param index
 * @return
 */
JsonValue* array_get(JsonValue* arr, size_t index) {
    CHECK_TYPE(arr, JSON_ARRAY);
    return (index < arr->value.array_value.ele_count) ?
           &arr->value.array_value.elements[index] :
           NULL;
}
/**
 * ‌批量插入
 * @param array
 * @param index
 * @param elements
 * @param num
 * @return
 */
int array_insert_batch(JsonValue* array, size_t index, JsonValue** elements, size_t num) {
    if (!array || array->type != JSON_ARRAY) return 0;
    if (index > array->value.array_value.ele_count) return 0;

    const size_t new_count = array->value.array_value.ele_count + num;
    JsonValue** new_elements = realloc(
            array->value.array_value.elements,
            sizeof(JsonValue*) * new_count
    );
    if (!new_elements) return 0;

    // 移动现有元素
    memmove(&new_elements[index+num],
            &new_elements[index],
            sizeof(JsonValue*) * (array->value.array_value.ele_count - index));

    // 拷贝新元素
    memcpy(&new_elements[index],
           elements,
           sizeof(JsonValue*) * num);

    array->value.array_value.elements = *new_elements;
    array->value.array_value.ele_count = new_count;
    return 1;
}
/**
 * 对象排序插入
 * @param obj
 * @param key
 * @param value
 * @return
 */
int object_insert_sorted(JsonValue* obj, const char* key, JsonValue* value) {
    // 二分查找插入位置
    size_t low = 0, high = obj->value.object_value.pair_count;
    while (low < high) {
        size_t mid = low + (high - low)/2;
        int cmp = strcmp(key, obj->value.object_value.pairs[mid].key);
        if (cmp < 0) high = mid;
        else low = mid + 1;
    }
    return object_insert_at(obj, low, key, value);
}

/**
 * 修改数组指定位置的元素
 * @param array
 * @param index
 * @param new_element
 * @return  返回旧元素
 */
JsonValue* array_replace_at(JsonValue* array, size_t index, JsonValue* new_element) {
    if (!array || array->type != JSON_ARRAY) return NULL;
    if (index >= array->value.array_value.ele_count) return NULL;

    JsonValue* elements = array->value.array_value.elements;
    JsonValue* old = &elements[index];
    elements[index] = *new_element;
    return old;
}

/**
 * 删除数组指定位置元素
 * @param array
 * @param index
 * @return 返回被删除元素
 */
JsonValue* array_remove_at(JsonValue* array, size_t index) {
    if (!array || array->type != JSON_ARRAY) return NULL;
    if (index >= array->value.array_value.ele_count) return NULL;

    JsonValue* elements = array->value.array_value.elements;
    JsonValue* removed = &elements[index];
    const size_t new_count = array->value.array_value.ele_count - 1;

    // 移动后续元素
    memmove(&elements[index],
            &elements[index+1],
            sizeof(JsonValue*) * (new_count - index));

    // 缩小内存分配
    JsonValue** new_elements = realloc(elements, sizeof(JsonValue*) * new_count);
    if (new_elements || new_count == 0) { // 允许realloc(0)返回NULL
        array->value.array_value.elements = *new_elements;
        array->value.array_value.ele_count = new_count;
    }
    return removed;
}

/**
 * 查找键对应的索引
 * @param obj
 * @param key
 * @return
 */
static int find_key_index(const JsonValue* obj, const char* key) {
    if (!obj || obj->type != JSON_OBJECT) return -1;

    for (size_t i = 0; i < obj->value.object_value.pair_count; ++i) {
        if (strcmp(obj->value.object_value.pairs[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * 修改对象键对应的值
 * @param obj
 * @param key
 * @param new_value
 * @return  返回旧值
 */
JsonValue* object_update(JsonValue* obj, const char* key, JsonValue* new_value) {
    int index = find_key_index(obj, key);
    if (index == -1) return NULL;

    JsonPair* pairs = obj->value.object_value.pairs;
    JsonValue* old = &pairs[index].value;
    pairs[index].value = *new_value;
    return old;
}

/**
 * 删除对象指定键的元素
 * @param obj
 * @param key
 * @return  返回被删除的值
 */
JsonValue* object_remove(JsonValue* obj, const char* key) {
    int index = find_key_index(obj, key);
    if (index == -1) return NULL;

    JsonPair* pairs = obj->value.object_value.pairs;
    JsonValue* removed = &pairs[index].value;
    const size_t new_count = obj->value.object_value.pair_count - 1;

    // 释放键内存
    free(pairs[index].key);

    // 移动后续键值对
    memmove(&pairs[index],
            &pairs[index+1],
            sizeof(JsonPair) * (new_count - index));

    // 调整内存分配
    JsonPair* new_pairs = realloc(pairs, sizeof(JsonPair) * new_count);
    if (new_pairs || new_count == 0) {
        obj->value.object_value.pairs = new_pairs;
        obj->value.object_value.pair_count = new_count;
    }
    return removed;
}
/**
 * 计算所需字符串长度
 * @param jv
 * @return
 */
static int json_value_length(const JsonValue* jv) {
    if (!jv) return -1;

    switch (jv->type) {
        case JSON_NULL:   return 4; // "null"
        case JSON_BOOL:   return jv->value.bool_value ? 4 : 5; // "true"/"false"
        case JSON_INT:    return snprintf(NULL, 0, "%d", jv->value.int_value);
        case JSON_FLOAT:  return snprintf(NULL, 0, "%.6g", jv->value.float_value);
        case JSON_DOUBLE: return snprintf(NULL, 0, "%.14g", jv->value.double_value);
        case JSON_STRING: return strlen(jv->value.string_value) + 2; // 引号

        case JSON_ARRAY: {
            int length = 2; // []
            for (size_t i = 0; i < jv->value.array_value.ele_count; ++i) {
                //JsonValue* arr = create_array();
                //array_append(arr, create_int(42));
                printf("jv ->: %d\n", jv->value.array_value.elements[i].value.int_value);

                JsonValue arr = jv->value.array_value.elements[i];
                printf("arr ->: %d\n", arr.value.int_value);

                int elem_len = json_value_length(&jv->value.array_value.elements[i]);
                if (elem_len < 0) return -1;
                length += elem_len + 1; // 元素+逗号
            }
            if (jv->value.array_value.ele_count > 0) length--; // 去掉最后一个逗号
            return length;
        }

        case JSON_OBJECT: {
            int length = 2; // {}
            for (size_t i = 0; i < jv->value.object_value.pair_count; ++i) {
                const JsonPair* pair = &jv->value.object_value.pairs[i];
                // Key长度: 字符串长度 + 2引号 + 1冒号
                int key_len = strlen(pair->key) + 3;
                int val_len = json_value_length(&pair->value);
                if (val_len < 0) return -1;
                length += key_len + val_len + 1; // key:value,
            }
            if (jv->value.object_value.pair_count > 0) length--; // 去掉最后一个逗号
            return length;
        }
    }
    return -1;
}

/**
 * 序列化实现
 * @param buf
 * @param jv
 * @return
 */
static char* json_value_serialize(char* buf, const JsonValue* jv) {
    switch (jv->type) {
        case JSON_NULL:  return strcpy(buf, "null");
        case JSON_BOOL:  return strcpy(buf, jv->value.bool_value ? "true" : "false");
        case JSON_INT:   return buf + sprintf(buf, "%d", jv->value.int_value);
        case JSON_FLOAT: return buf + sprintf(buf, "%.6g", jv->value.float_value);
        case JSON_DOUBLE:return buf + sprintf(buf, "%.14g", jv->value.double_value);
        case JSON_STRING: {
            *buf++ = '"';
            buf = strcpy(buf, jv->value.string_value);
            *buf++ = '"';
            return buf;
        }

        case JSON_ARRAY: {
            *buf++ = '[';
            for (size_t i = 0; i < jv->value.array_value.ele_count; ++i) {
                buf = json_value_serialize(buf, &jv->value.array_value.elements[i]);
                if (i != jv->value.array_value.ele_count - 1) {
                    *buf++ = ',';
                }
            }
            *buf++ = ']';
            return buf;
        }

        case JSON_OBJECT: {
            *buf++ = '{';
            for (size_t i = 0; i < jv->value.object_value.pair_count; ++i) {
                const JsonPair* pair = &jv->value.object_value.pairs[i];
                // 序列化key
                *buf++ = '"';
                buf = strcpy(buf, pair->key);
                *buf++ = '"';
                *buf++ = ':';
                // 序列化value
                buf = json_value_serialize(buf, &pair->value);
                if (i != jv->value.object_value.pair_count - 1) {
                    *buf++ = ',';
                }
            }
            *buf++ = '}';
            return buf;
        }
    }
    return buf;
}

char* json_to_string(const JsonValue* jv) {
    if (!jv) return NULL;

    int length = json_value_length(jv);
    if (length < 0) return NULL;

    char* result = malloc(length + 1);
    if (!result) return NULL;

    json_value_serialize(result, jv);
    return result;
}

// =============================  2025年3月26日 新增功能 end ================================