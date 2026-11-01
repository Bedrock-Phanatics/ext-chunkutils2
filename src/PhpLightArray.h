#ifndef HAVE_PHP_LIGHT_ARRAY_H
#define HAVE_PHP_LIGHT_ARRAY_H

#include "lib/LightArray.h"

extern "C" {
#include "php.h"
}

typedef struct {
	LightArray lightArray;
	zend_object std;
} light_array_obj;

extern zend_class_entry* light_array_entry;

PHP_METHOD(PhpLightArray, __construct);
PHP_METHOD(PhpLightArray, fill);
PHP_METHOD(PhpLightArray, get);
PHP_METHOD(PhpLightArray, set);
PHP_METHOD(PhpLightArray, getData);
PHP_METHOD(PhpLightArray, collectGarbage);
PHP_METHOD(PhpLightArray, isUniform);

void register_light_array_class();

#endif
