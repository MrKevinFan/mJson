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
