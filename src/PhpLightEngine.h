#ifndef HAVE_PHP_LIGHT_ENGINE_H
#define HAVE_PHP_LIGHT_ENGINE_H

extern "C" {
#include "php.h"
}

extern zend_class_entry *light_engine_entry;

void register_light_engine_class();

#endif
