
(本人菜鸡一枚，欢迎大佬么指正！)

### mJson 说明

mJson说明 是一个轻量级的JSON解析库，将JSON格式的字符串解析为C语言中的数据结构。类似于键值对的结构，如对象和数组。
个人趋向于简单化所以只有一个 .c 和 .h 文件，目前主要功能是解析json字符串。

### mJson 使用

只需将 mJson.h 和 mJson.c 引入到项目中即可直接使用，支持 bool、int、float、double、string、arrary、object等多种数据类型的解析。
```c
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
```


### mLog示例

下面是一个简单的使用示例，通过logger_create来初始化一个日志实例。logger_add_console用来添加控制台的输出，logger_add_file用来添加文件的输出。

```
#include "mJson.h"
#include <stdio.h>

int main() {
    const char *json = "{"
                       "\"name\": \"kevinfan\","
                       "\"age\": 30,"
                       "\"student\": false,"
                       "\"height\": 175.23,"
                       "\"weight\": 120.2645735,"
                       "\"skill\": [\"c\", \"c#\"],"
                       "\"address\": {"
                       "\"street\": \"baoan\","
                       "\"number\": 60,"
                       "}"
                       "}";

    int error;
    JsonValue root = json_parse(json, &error);

    if (error != JSON_SUCCESS) {
        printf("Parse error: %d\n", error);
        return 1;
    }

    // 查询示例
    JsonValue* name = json_get(&root, "name");
    JsonValue* age = json_get(&root, "age");
    JsonValue* student = json_get(&root, "student");
    JsonValue* height = json_get(&root, "height");
    JsonValue* weight = json_get(&root, "weight");
    JsonValue* skill = json_get(&root, "skill");
    JsonValue* address = json_get(&root, "address");
    JsonValue* street = json_get(&root, "address.street");

    printf("Name ->: %s\n", name->value.string_value);
    printf("age ->: %d\n", age->value.int_value);
    printf("student ->: %d\n", student->value.bool_value);
    printf("height ->: %.4f\n", height->value.float_value);
    printf("weight ->: %lf\n", weight->value.double_value);

    size_t i;
    for (i = 0; i < skill->value.array_value.count; i++) {
        printf("skill ->: %s\n", skill->value.array_value.elements[i].value.string_value);
    }

    printf("address.street ->: %s\n",address->value.object_value.pairs[0].value.value.string_value);
    printf("address.number ->: %d\n",address->value.object_value.pairs[1].value.value.int_value);

    printf("street ->: %s\n", street->value.string_value);

    json_free(&root);
    return 0;
}

```



### mLog版本说明


V1.0.0-Alpha

2025年3月25日更新，初版发布 。

