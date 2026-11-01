#include <new>
#include <unordered_map>
#include <vector>

#include "lib/LightEngine.h"

#include "PhpLightArray.h"
#include "PhpLightEngine.h"
#include "PhpPalettedBlockArray.h"
#include "PhpPalettedBlockArrayObj.h"
#include "ZendUtil.h"
#include "stubs/pocketmine/world/format/LightEngine_arginfo.h"

extern "C" {
#include "ext/spl/spl_exceptions.h"
#include "Zend/zend_exceptions.h"
}

typedef LightEngine<Block> NormalLightEngine;

//keeps neighbour arithmetic on block coordinates far away from int32 overflow
static const zend_long MAX_CHUNK_COORD = 1 << 26;
static const zend_long MAX_BLOCK_COORD = MAX_CHUNK_COORD << 4;

struct LightEngineState {
	NormalLightEngine engine;
	//the PHP objects each registered column borrows arrays from
	std::unordered_map<int64_t, std::vector<zend_object *>> owned;
	bool constructed = false;
};

typedef struct {
	LightEngineState state;
	zend_object std;
} light_engine_obj;

zend_class_entry *light_engine_entry;
static zend_object_handlers light_engine_handlers;

static int64_t column_key(zend_long chunkX, zend_long chunkZ) {
	return static_cast<int64_t>((static_cast<uint64_t>(static_cast<uint32_t>(chunkX)) << 32) | static_cast<uint32_t>(chunkZ));
}

static void release_all(std::vector<zend_object *> &objects) {
	for (zend_object *object : objects) {
		OBJ_RELEASE(object);
	}
	objects.clear();
}

static void release_owned(LightEngineState &state) {
	for (auto &entry : state.owned) {
		release_all(entry.second);
	}
	state.owned.clear();
}

static zend_object *light_engine_new(zend_class_entry *class_type) {
	light_engine_obj *object = alloc_custom_zend_object<light_engine_obj>(class_type, &light_engine_handlers);
	new (&object->state) LightEngineState();
	return &object->std;
}

static void light_engine_free(zend_object *obj) {
	light_engine_obj *object = fetch_from_zend_object<light_engine_obj>(obj);
	object->state.engine.clear();
	release_owned(object->state);
	object->state.~LightEngineState();
	zend_object_std_dtor(obj);
}

static LightEngineState &state_of(zval *self) {
	return fetch_from_zend_object<light_engine_obj>(Z_OBJ_P(self))->state;
}

static bool valid_chunk_coords(zend_long chunkX, zend_long chunkZ) {
	return chunkX > -MAX_CHUNK_COORD && chunkX < MAX_CHUNK_COORD && chunkZ > -MAX_CHUNK_COORD && chunkZ < MAX_CHUNK_COORD;
}

static bool valid_block_coords(zend_long x, zend_long y, zend_long z) {
	return
		x > -MAX_BLOCK_COORD && x < MAX_BLOCK_COORD &&
		y > -MAX_BLOCK_COORD && y < MAX_BLOCK_COORD &&
		z > -MAX_BLOCK_COORD && z < MAX_BLOCK_COORD;
}

static bool read_block_id(zend_ulong key, zend_string *stringKey, Block &block) {
	if (stringKey != nullptr || key > UINT32_MAX) {
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0, "Block state IDs must be integers between 0 and %u", UINT32_MAX);
		return false;
	}
	block = static_cast<Block>(key);
	return true;
}

static bool read_light_level(zval *value, const char *name, uint8_t &level) {
	ZVAL_DEREF(value);
	if (Z_TYPE_P(value) != IS_LONG || Z_LVAL_P(value) < 0 || Z_LVAL_P(value) > LightLevel::MAX) {
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0, "%s must be integers between 0 and %u", name, LightLevel::MAX);
		return false;
	}
	level = static_cast<uint8_t>(Z_LVAL_P(value));
	return true;
}

static zval *subchunk_entry(HashTable *array, int index, const char *name) {
	zval *value = zend_hash_index_find(array, static_cast<zend_ulong>(static_cast<zend_long>(index)));
	if (value == nullptr) {
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0, "%s is missing subchunk %d", name, index);
		return nullptr;
	}
	ZVAL_DEREF(value);
	return value;
}

static zend_object *object_of(zval *value, zend_class_entry *ce, const char *name, int index) {
	if (Z_TYPE_P(value) != IS_OBJECT || Z_OBJCE_P(value) != ce) {
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0, "%s subchunk %d must be a %s", name, index, ZSTR_VAL(ce->name));
		return nullptr;
	}
	return Z_OBJ_P(value);
}

#define ENGINE_CALL(statement) \
	try { \
		statement; \
	} catch (std::exception &e) { \
		zend_throw_exception(zend_ce_exception, e.what(), 0); \
		RETURN_THROWS(); \
	}

#define LIGHT_ENGINE_METHOD(name) PHP_METHOD(pocketmine_world_format_LightEngine, name)

LIGHT_ENGINE_METHOD(__construct) {
	HashTable *lightFilters, *lightEmitters, *skyLightBlockers;

	ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_THROW, 3, 3)
		Z_PARAM_ARRAY_HT(lightFilters)
		Z_PARAM_ARRAY_HT(lightEmitters)
		Z_PARAM_ARRAY_HT(skyLightBlockers)
	ZEND_PARSE_PARAMETERS_END();

	LightEngineState &state = state_of(getThis());
	if (state.constructed) {
		zend_throw_error(nullptr, "LightEngine is already constructed");
		RETURN_THROWS();
	}

	std::unordered_map<Block, NormalLightEngine::Properties> properties;
	zend_ulong key;
	zend_string *stringKey;
	zval *value;
	Block block;
	uint8_t level;

	try {
		ZEND_HASH_FOREACH_KEY_VAL(lightFilters, key, stringKey, value) {
			if (!read_block_id(key, stringKey, block) || !read_light_level(value, "Light filters", level)) {
				RETURN_THROWS();
			}
			if (level != NormalLightEngine::BASE_LIGHT_FILTER) {
				properties[block].filter = level;
			}
		} ZEND_HASH_FOREACH_END();

		ZEND_HASH_FOREACH_KEY_VAL(lightEmitters, key, stringKey, value) {
			if (!read_block_id(key, stringKey, block) || !read_light_level(value, "Light emitters", level)) {
				RETURN_THROWS();
			}
			if (level != 0) {
				properties[block].emitter = level;
			}
		} ZEND_HASH_FOREACH_END();

		ZEND_HASH_FOREACH_KEY(skyLightBlockers, key, stringKey) {
			if (!read_block_id(key, stringKey, block)) {
				RETURN_THROWS();
			}
			properties[block].blocksDirectSkyLight = true;
		} ZEND_HASH_FOREACH_END();

		for (const auto &entry : properties) {
			state.engine.setProperties(entry.first, entry.second);
		}
	} catch (std::exception &e) {
		zend_throw_exception(zend_ce_exception, e.what(), 0);
		RETURN_THROWS();
	}
	state.constructed = true;
}

LIGHT_ENGINE_METHOD(setChunk) {
	zend_long chunkX, chunkZ;
	HashTable *emptyBlockIds, *blocks, *blockLight, *skyLight, *heightMap;

	ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_THROW, 7, 7)
		Z_PARAM_LONG(chunkX)
		Z_PARAM_LONG(chunkZ)
		Z_PARAM_ARRAY_HT(emptyBlockIds)
		Z_PARAM_ARRAY_HT(blocks)
		Z_PARAM_ARRAY_HT(blockLight)
		Z_PARAM_ARRAY_HT(skyLight)
		Z_PARAM_ARRAY_HT(heightMap)
	ZEND_PARSE_PARAMETERS_END();

	if (!valid_chunk_coords(chunkX, chunkZ)) {
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0, "Chunk coordinates must be within +-%lld", static_cast<long long>(MAX_CHUNK_COORD));
		RETURN_THROWS();
	}

	const int count = NormalLightEngine::SUBCHUNK_COUNT;
	Block emptyIds[count];
	const NormalBlockArrayContainer *containers[count];
	LightArray *blockArrays[count];
	LightArray *skyArrays[count];
	int32_t heights[256];
	std::vector<zend_object *> objects;

	try {
		objects.reserve(count * 3);
		for (int i = 0; i < count; ++i) {
			int y = i + NormalLightEngine::MIN_SUBCHUNK;
			zval *value;

			if ((value = subchunk_entry(emptyBlockIds, y, "Empty block IDs")) == nullptr) {
				RETURN_THROWS();
			}
			if (Z_TYPE_P(value) != IS_LONG || Z_LVAL_P(value) < 0 || static_cast<zend_ulong>(Z_LVAL_P(value)) > UINT32_MAX) {
				zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0, "Empty block ID for subchunk %d must be an integer between 0 and %u", y, UINT32_MAX);
				RETURN_THROWS();
			}
			emptyIds[i] = static_cast<Block>(Z_LVAL_P(value));

			if ((value = subchunk_entry(blocks, y, "Blocks")) == nullptr) {
				RETURN_THROWS();
			}
			containers[i] = nullptr;
			if (Z_TYPE_P(value) != IS_NULL) {
				zend_object *object = object_of(value, paletted_block_array_entry, "Blocks", y);
				if (object == nullptr) {
					RETURN_THROWS();
				}
				containers[i] = &fetch_from_zend_object<paletted_block_array_obj>(object)->container;
				objects.push_back(object);
			}

			if ((value = subchunk_entry(blockLight, y, "Block light")) == nullptr) {
				RETURN_THROWS();
			}
			zend_object *object = object_of(value, light_array_entry, "Block light", y);
			if (object == nullptr) {
				RETURN_THROWS();
			}
			blockArrays[i] = &fetch_from_zend_object<light_array_obj>(object)->lightArray;
			objects.push_back(object);

			if ((value = subchunk_entry(skyLight, y, "Sky light")) == nullptr) {
				RETURN_THROWS();
			}
			object = object_of(value, light_array_entry, "Sky light", y);
			if (object == nullptr) {
				RETURN_THROWS();
			}
			skyArrays[i] = &fetch_from_zend_object<light_array_obj>(object)->lightArray;
			objects.push_back(object);
		}

		for (int i = 0; i < 256; ++i) {
			zval *value = zend_hash_index_find(heightMap, i);
			if (value != nullptr) {
				ZVAL_DEREF(value);
			}
			if (value == nullptr || Z_TYPE_P(value) != IS_LONG) {
				zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0, "Height map must hold 256 integers");
				RETURN_THROWS();
			}
			//heights outside the world mean nothing to light, so they're treated as its edges
			heights[i] = static_cast<int32_t>(std::max<zend_long>(NormalLightEngine::MIN_Y, std::min<zend_long>(NormalLightEngine::MAX_Y, Z_LVAL_P(value))));
		}

		LightEngineState &state = state_of(getThis());
		std::vector<zend_object *> &owned = state.owned[column_key(chunkX, chunkZ)];
		state.engine.setColumn(static_cast<int32_t>(chunkX), static_cast<int32_t>(chunkZ), emptyIds, containers, blockArrays, skyArrays, heights);
		for (zend_object *object : objects) {
			GC_ADDREF(object);
		}
		owned.swap(objects);
		release_all(objects);
	} catch (std::exception &e) {
		zend_throw_exception(zend_ce_exception, e.what(), 0);
		RETURN_THROWS();
	}
}

LIGHT_ENGINE_METHOD(hasChunk) {
	zend_long chunkX, chunkZ;

	ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_THROW, 2, 2)
		Z_PARAM_LONG(chunkX)
		Z_PARAM_LONG(chunkZ)
	ZEND_PARSE_PARAMETERS_END();

	RETURN_BOOL(valid_chunk_coords(chunkX, chunkZ) && state_of(getThis()).engine.hasColumn(static_cast<int32_t>(chunkX), static_cast<int32_t>(chunkZ)));
}

LIGHT_ENGINE_METHOD(removeChunk) {
	zend_long chunkX, chunkZ;

	ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_THROW, 2, 2)
		Z_PARAM_LONG(chunkX)
		Z_PARAM_LONG(chunkZ)
	ZEND_PARSE_PARAMETERS_END();

	if (!valid_chunk_coords(chunkX, chunkZ)) {
		return;
	}
	LightEngineState &state = state_of(getThis());
	ENGINE_CALL(state.engine.removeColumn(static_cast<int32_t>(chunkX), static_cast<int32_t>(chunkZ)));
	auto found = state.owned.find(column_key(chunkX, chunkZ));
	if (found != state.owned.end()) {
		release_all(found->second);
		state.owned.erase(found);
	}
}

LIGHT_ENGINE_METHOD(clear) {
	zend_parse_parameters_none_throw();

	LightEngineState &state = state_of(getThis());
	state.engine.clear();
	release_owned(state);
}

LIGHT_ENGINE_METHOD(getHeightMap) {
	zend_long chunkX, chunkZ;

	ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_THROW, 2, 2)
		Z_PARAM_LONG(chunkX)
		Z_PARAM_LONG(chunkZ)
	ZEND_PARSE_PARAMETERS_END();

	auto heightMap = valid_chunk_coords(chunkX, chunkZ) ? state_of(getThis()).engine.heightMap(static_cast<int32_t>(chunkX), static_cast<int32_t>(chunkZ)) : nullptr;
	if (heightMap == nullptr) {
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0, "Chunk " ZEND_LONG_FMT " " ZEND_LONG_FMT " is not registered", chunkX, chunkZ);
		RETURN_THROWS();
	}
	array_init_size(return_value, 256);
	for (int32_t height : *heightMap) {
		add_next_index_long(return_value, height);
	}
}

LIGHT_ENGINE_METHOD(recalculateBlockNode) {
	zend_long x, y, z;

	ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_THROW, 3, 3)
		Z_PARAM_LONG(x)
		Z_PARAM_LONG(y)
		Z_PARAM_LONG(z)
	ZEND_PARSE_PARAMETERS_END();

	if (valid_block_coords(x, y, z)) {
		ENGINE_CALL(state_of(getThis()).engine.recalculateBlockNode(static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z)));
	}
}

LIGHT_ENGINE_METHOD(recalculateSkyNode) {
	zend_long x, y, z;

	ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_THROW, 3, 3)
		Z_PARAM_LONG(x)
		Z_PARAM_LONG(y)
		Z_PARAM_LONG(z)
	ZEND_PARSE_PARAMETERS_END();

	if (!valid_block_coords(x, y, z)) {
		RETURN_NULL();
	}
	int32_t height;
	ENGINE_CALL(height = state_of(getThis()).engine.recalculateSkyNode(static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z)));
	if (height == INT32_MIN) {
		RETURN_NULL();
	}
	RETURN_LONG(height);
}

static bool read_block_state(zval *value, Block &block) {
	ZVAL_DEREF(value);
	if (Z_TYPE_P(value) != IS_LONG || Z_LVAL_P(value) < 0 || static_cast<zend_ulong>(Z_LVAL_P(value)) > UINT32_MAX) {
		return false;
	}
	block = static_cast<Block>(Z_LVAL_P(value));
	return true;
}

static bool read_change(zval *entry, NormalLightEngine::Change &change) {
	ZVAL_DEREF(entry);
	if (Z_TYPE_P(entry) != IS_ARRAY || zend_hash_num_elements(Z_ARRVAL_P(entry)) != 5) {
		return false;
	}
	zend_long coords[3];
	for (zend_ulong i = 0; i < 3; ++i) {
		zval *coord = zend_hash_index_find(Z_ARRVAL_P(entry), i);
		if (coord == nullptr) {
			return false;
		}
		ZVAL_DEREF(coord);
		if (Z_TYPE_P(coord) != IS_LONG) {
			return false;
		}
		coords[i] = Z_LVAL_P(coord);
	}
	zval *before = zend_hash_index_find(Z_ARRVAL_P(entry), 3);
	zval *after = zend_hash_index_find(Z_ARRVAL_P(entry), 4);
	if (
		!valid_block_coords(coords[0], coords[1], coords[2]) ||
		before == nullptr || !read_block_state(before, change.before) ||
		after == nullptr || !read_block_state(after, change.after)
	) {
		return false;
	}
	change.x = static_cast<int32_t>(coords[0]);
	change.y = static_cast<int32_t>(coords[1]);
	change.z = static_cast<int32_t>(coords[2]);
	return true;
}

LIGHT_ENGINE_METHOD(applyChanges) {
	HashTable *changes;

	ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_THROW, 1, 1)
		Z_PARAM_ARRAY_HT(changes)
	ZEND_PARSE_PARAMETERS_END();

	try {
		std::vector<NormalLightEngine::Change> parsed(zend_hash_num_elements(changes));
		size_t i = 0;
		zval *entry;
		ZEND_HASH_FOREACH_VAL(changes, entry) {
			if (!read_change(entry, parsed[i++])) {
				zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0, "Changes must be lists of x, y, z, the previous block state ID and the new one");
				RETURN_THROWS();
			}
		} ZEND_HASH_FOREACH_END();

		state_of(getThis()).engine.applyChanges(parsed);
	} catch (std::exception &e) {
		zend_throw_exception(zend_ce_exception, e.what(), 0);
		RETURN_THROWS();
	}
}

static void recalculate_chunk(INTERNAL_FUNCTION_PARAMETERS, NormalLightEngine::Type type) {
	zend_long chunkX, chunkZ;

	ZEND_PARSE_PARAMETERS_START_EX(ZEND_PARSE_PARAMS_THROW, 2, 2)
		Z_PARAM_LONG(chunkX)
		Z_PARAM_LONG(chunkZ)
	ZEND_PARSE_PARAMETERS_END();

	int lightSources = -1;
	if (valid_chunk_coords(chunkX, chunkZ)) {
		NormalLightEngine &engine = state_of(getThis()).engine;
		int32_t x = static_cast<int32_t>(chunkX);
		int32_t z = static_cast<int32_t>(chunkZ);
		ENGINE_CALL(lightSources = type == NormalLightEngine::BLOCK ? engine.recalculateBlockChunk(x, z) : engine.recalculateSkyChunk(x, z));
	}
	if (lightSources < 0) {
		zend_throw_exception_ex(spl_ce_InvalidArgumentException, 0, "Chunk " ZEND_LONG_FMT " " ZEND_LONG_FMT " is not registered", chunkX, chunkZ);
		RETURN_THROWS();
	}
	RETURN_LONG(lightSources);
}

LIGHT_ENGINE_METHOD(recalculateBlockChunk) {
	recalculate_chunk(INTERNAL_FUNCTION_PARAM_PASSTHRU, NormalLightEngine::BLOCK);
}

LIGHT_ENGINE_METHOD(recalculateSkyChunk) {
	recalculate_chunk(INTERNAL_FUNCTION_PARAM_PASSTHRU, NormalLightEngine::SKY);
}

LIGHT_ENGINE_METHOD(executeBlock) {
	zend_parse_parameters_none_throw();

	int touched;
	ENGINE_CALL(touched = state_of(getThis()).engine.execute(NormalLightEngine::BLOCK));
	RETURN_LONG(touched);
}

LIGHT_ENGINE_METHOD(executeSky) {
	zend_parse_parameters_none_throw();

	int touched;
	ENGINE_CALL(touched = state_of(getThis()).engine.execute(NormalLightEngine::SKY));
	RETURN_LONG(touched);
}

void register_light_engine_class() {
	memcpy(&light_engine_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	light_engine_handlers.offset = XtOffsetOf(light_engine_obj, std);
	light_engine_handlers.free_obj = light_engine_free;
	light_engine_handlers.clone_obj = nullptr;

	zend_class_entry ce;
	INIT_CLASS_ENTRY(ce, "pocketmine\\world\\format\\LightEngine", class_pocketmine_world_format_LightEngine_methods);
	ce.create_object = light_engine_new;
	light_engine_entry = zend_register_internal_class(&ce);
	light_engine_entry->ce_flags |= ZEND_ACC_FINAL;
#ifdef ZEND_ACC_NOT_SERIALIZABLE
	light_engine_entry->ce_flags |= ZEND_ACC_NOT_SERIALIZABLE;
#endif
}
