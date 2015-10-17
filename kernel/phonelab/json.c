#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/printk.h>

#include <linux/json.h>
#else
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include "json.h"
#endif

#include "mem.h"

#ifdef __KERNEL__
#define printf(...) printk(KERN_INFO __VA_ARGS__);
#endif

struct json_object *json_create_object(void)
{
	struct json_object *obj = alloc(sizeof(struct json_object));
	if (obj)
		memset(obj, 0, sizeof(struct json_object));
	return obj;
}
struct json_array *json_create_array(void)
{
	struct json_array *array = alloc(sizeof(struct json_array));
	if (array)
		memset(array, 0, sizeof(struct json_array));
	return array;
}
static struct json_pair *json_create_pair(const char *name, struct json_value *value)
{
	struct json_pair *pair = alloc(sizeof(struct json_pair));
	if (pair) {
		pair->name = stringdup(name);
		pair->value = value;
		value->parent_type = JSON_PARENT_TYPE_PAIR;
		value->parent_pair = pair;
	}
	return pair;
}
static struct json_value *json_create_value_int(long number)
{
	struct json_value *value = alloc(sizeof(struct json_value));
	if (value) {
		value->type = JSON_TYPE_INTEGER;
		value->integer_number = number;
	}
	return value;
}
#ifndef __KERNEL__
static struct json_value *json_create_value_float(float number)
{
	struct json_value *value = alloc(sizeof(struct json_value));
	if (value) {
		value->type = JSON_TYPE_FLOAT;
		value->float_number = number;
	}
	return value;
}
#endif
static char *strdup_escape(const char *str)
{
	const char *input = str;
	char *p, *ret;
	int escapes;
	if (!strlen(str))
		return NULL;
	escapes = 0;
	while ((input = strpbrk(input, "\\\"")) != NULL) {
		escapes++;
		input++;
	}
	p = ret = alloc(strlen(str) + escapes + 1);
	while (*str) {
		if (*str == '\\' || *str == '\"')
			*p++ = '\\';
		*p++ = *str++;
	}
	*p = '\0';
	return ret;
}
/*
 * Valid JSON strings must escape '"' and '/' with a preceeding '/'
 */
static struct json_value *json_create_value_string(const char *str)
{
	struct json_value *value = alloc(sizeof(struct json_value));
	if (value) {
		value->type = JSON_TYPE_STRING;
		value->string = strdup_escape(str);
		if (!value->string) {
			release(value);
			value = NULL;
		}
	}
	return value;
}
static struct json_value *json_create_value_object(struct json_object *obj)
{
	struct json_value *value = alloc(sizeof(struct json_value));
	if (value) {
		value->type = JSON_TYPE_OBJECT;
		value->object = obj;
		obj->parent = value;
	}
	return value;
}
static struct json_value *json_create_value_array(struct json_array *array)
{
	struct json_value *value = alloc(sizeof(struct json_value));
	if (value) {
		value->type = JSON_TYPE_ARRAY;
		value->array = array;
		array->parent = value;
	}
	return value;
}
static void json_free_pair(struct json_pair *pair);
static void json_free_value(struct json_value *value);
void json_free_object(struct json_object *obj)
{
	int i;
	for (i = 0; i < obj->pair_cnt; i++)
		json_free_pair(obj->pairs[i]);
	release(obj->pairs);
	release(obj);
}
static void json_free_array(struct json_array *array)
{
	int i;
	for (i = 0; i < array->value_cnt; i++)
		json_free_value(array->values[i]);
	release(array->values);
	release(array);
}
static void json_free_pair(struct json_pair *pair)
{
	json_free_value(pair->value);
	release(pair->name);
	release(pair);
}
static void json_free_value(struct json_value *value)
{
	switch (value->type) {
	case JSON_TYPE_STRING:
		release(value->string);
		break;
	case JSON_TYPE_OBJECT:
		json_free_object(value->object);
		break;
	case JSON_TYPE_ARRAY:
		json_free_array(value->array);
		break;
	}
	release(value);
}
static int json_array_add_value(struct json_array *array, struct json_value *value)
{
	struct json_value **values = grealloc(array->values,
		sizeof(struct json_value *) * (array->value_cnt + 1));
	if (!values)
		return ENOMEM;
	values[array->value_cnt] = value;
	array->value_cnt++;
	array->values = values;
	value->parent_type = JSON_PARENT_TYPE_ARRAY;
	value->parent_array = array;
	return 0;
}
static int json_object_add_pair(struct json_object *obj, struct json_pair *pair)
{
	struct json_pair **pairs = grealloc(obj->pairs,
		sizeof(struct json_pair *) * (obj->pair_cnt + 1));
	if (!pairs)
		return ENOMEM;
	pairs[obj->pair_cnt] = pair;
	obj->pair_cnt++;
	obj->pairs = pairs;
	pair->parent = obj;
	return 0;
}
int json_object_add_value_type(struct json_object *obj, const char *name, int type, ...)
{
	struct json_value *value;
	struct json_pair *pair;
	va_list args;
	int ret;
	va_start(args, type);
	if (type == JSON_TYPE_STRING)
		value = json_create_value_string(va_arg(args, char *));
	else if (type == JSON_TYPE_INTEGER)
		value = json_create_value_int(va_arg(args, long));
#ifndef __KERNEL__
	else if (type == JSON_TYPE_FLOAT)
		value = json_create_value_float(va_arg(args, double));
#endif
	else if (type == JSON_TYPE_OBJECT)
		value = json_create_value_object(va_arg(args, struct json_object *));
	else
		value = json_create_value_array(va_arg(args, struct json_array *));
	va_end(args);
	if (!value)
		return ENOMEM;
	pair = json_create_pair(name, value);
	if (!pair) {
		json_free_value(value);
		return ENOMEM;
	}
	ret = json_object_add_pair(obj, pair);
	if (ret) {
		json_free_pair(pair);
		return ENOMEM;
	}
	return 0;
}
static void json_print_array(struct json_array *array);
int json_array_add_value_type(struct json_array *array, int type, ...)
{
	struct json_value *value;
	va_list args;
	int ret;
	va_start(args, type);
	if (type == JSON_TYPE_STRING)
		value = json_create_value_string(va_arg(args, char *));
	else if (type == JSON_TYPE_INTEGER)
		value = json_create_value_int(va_arg(args, long));
#ifndef __KERNEL__
	else if (type == JSON_TYPE_FLOAT)
		value = json_create_value_float(va_arg(args, double));
#endif
	else if (type == JSON_TYPE_OBJECT)
		value = json_create_value_object(va_arg(args, struct json_object *));
	else
		value = json_create_value_array(va_arg(args, struct json_array *));
	va_end(args);
	if (!value)
		return ENOMEM;
	ret = json_array_add_value(array, value);
	if (ret) {
		json_free_value(value);
		return ENOMEM;
	}
	return 0;
}
static int json_value_level(struct json_value *value);
static int json_pair_level(struct json_pair *pair);
static int json_array_level(struct json_array *array);
static int json_object_level(struct json_object *object)
{
	if (object->parent == NULL)
		return 0;
	return json_value_level(object->parent);
}
static int json_pair_level(struct json_pair *pair)
{
	return json_object_level(pair->parent) + 1;
}
static int json_array_level(struct json_array *array)
{
	return json_value_level(array->parent);
}
static int json_value_level(struct json_value *value)
{
	if (value->parent_type == JSON_PARENT_TYPE_PAIR)
		return json_pair_level(value->parent_pair);
	else
		return json_array_level(value->parent_array) + 1;
}
static void json_print_level(int level)
{
	while (level-- > 0)
		printf("  ");
}
static void json_sprint_level(int level, char *buffer, int *offset)
{
	while (level-- > 0)
		*offset += sprintf(buffer + (*offset), "  ");
}

static void json_print_pair(struct json_pair *pair);
static void json_print_array(struct json_array *array);
static void json_print_value(struct json_value *value);
static void json_sprint_pair(struct json_pair *pair, char *buffer, int *offset);
static void json_sprint_array(struct json_array *array, char *buffer, int *offset);
static void json_sprint_value(struct json_value *value, char *buffer, int *offset);
void json_print_object(struct json_object *obj)
{
	int i;
	printf("{\n");
	for (i = 0; i < obj->pair_cnt; i++) {
		if (i > 0)
			printf(",\n");
		json_print_pair(obj->pairs[i]);
	}
	printf("\n");
	json_print_level(json_object_level(obj));
	printf("}");
}
void json_sprint_object(struct json_object *obj, char *buffer, int *offset)
{
	int i;
	*offset += sprintf(buffer + (*offset), "{");
	for (i = 0; i < obj->pair_cnt; i++) {
		if (i > 0)
			*offset += sprintf(buffer + (*offset), ",");
		json_sprint_pair(obj->pairs[i], buffer, offset);
	}
#if 0
	*offset += sprintf(buffer + (*offset), "");
#endif
	json_sprint_level(json_object_level(obj), buffer, offset);
	*offset += sprintf(buffer + (*offset), "}");
}
static void json_print_pair(struct json_pair *pair)
{
	json_print_level(json_pair_level(pair));
	printf("\"%s\" : ", pair->name);
	json_print_value(pair->value);
}
static void json_sprint_pair(struct json_pair *pair, char *buffer, int *offset)
{
	json_sprint_level(json_pair_level(pair), buffer, offset);
	*offset += sprintf(buffer + (*offset), "\"%s\" : ", pair->name);
	json_sprint_value(pair->value, buffer, offset);
}
static void json_print_array(struct json_array *array)
{
	int i;
	printf("[\n");
	for (i = 0; i < array->value_cnt; i++) {
		if (i > 0)
			printf(",\n");
		json_print_level(json_value_level(array->values[i]));
		json_print_value(array->values[i]);
	}
	printf("\n");
	json_print_level(json_array_level(array));
	printf("]");
}
static void json_sprint_array(struct json_array *array, char *buffer, int *offset)
{
	int i;
	*offset += sprintf(buffer + (*offset), "[");
	for (i = 0; i < array->value_cnt; i++) {
		if (i > 0)
			*offset += sprintf(buffer + (*offset), ",");
		json_sprint_level(json_value_level(array->values[i]), buffer, offset);
		json_sprint_value(array->values[i], buffer, offset);
	}
#if 0
	*offset += sprintf(buffer + (*offset), "");
#endif
	json_sprint_level(json_array_level(array), buffer, offset);
	*offset += sprintf(buffer + (*offset), "]");
}
static void json_print_value(struct json_value *value)
{
	switch (value->type) {
	case JSON_TYPE_STRING:
		printf("\"%s\"", value->string);
		break;
	case JSON_TYPE_INTEGER:
		printf("%ld", value->integer_number);
		break;
#ifndef __KERNEL__
	case JSON_TYPE_FLOAT:
		printf("%.2f", value->float_number);
		break;
#endif
	case JSON_TYPE_OBJECT:
		json_print_object(value->object);
		break;
	case JSON_TYPE_ARRAY:
		json_print_array(value->array);
		break;
	}
}
static void json_sprint_value(struct json_value *value, char *buffer, int *offset)
{
	switch (value->type) {
	case JSON_TYPE_STRING:
		*offset += sprintf(buffer + (*offset), "\"%s\"", value->string);
		break;
	case JSON_TYPE_INTEGER:
		*offset += sprintf(buffer + (*offset), "%ld", value->integer_number);
		break;
#ifndef __KERNEL__
	case JSON_TYPE_FLOAT:
		*offset += sprintf(buffer + (*offset), "%.2f", value->float_number);
		break;
#endif
	case JSON_TYPE_OBJECT:
		json_sprint_object(value->object, buffer, offset);
		break;
	case JSON_TYPE_ARRAY:
		json_sprint_array(value->array, buffer, offset);
		break;
	}
}
