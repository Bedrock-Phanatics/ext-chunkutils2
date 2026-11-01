/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: 012d26bc0408697b769b924ff1203508a8b9d2d6 */

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_pocketmine_world_format_LightEngine___construct, 0, 0, 3)
	ZEND_ARG_TYPE_INFO(0, lightFilters, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, lightEmitters, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, directSkyLightBlockers, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_pocketmine_world_format_LightEngine_setChunk, 0, 7, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, chunkX, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, chunkZ, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, emptyBlockIds, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, blocks, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, blockLight, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, skyLight, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, heightMap, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_pocketmine_world_format_LightEngine_hasChunk, 0, 2, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, chunkX, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, chunkZ, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_pocketmine_world_format_LightEngine_removeChunk, 0, 2, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, chunkX, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, chunkZ, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_pocketmine_world_format_LightEngine_clear, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_pocketmine_world_format_LightEngine_getHeightMap, 0, 2, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, chunkX, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, chunkZ, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_pocketmine_world_format_LightEngine_recalculateBlockNode, 0, 3, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, x, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, y, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, z, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_pocketmine_world_format_LightEngine_recalculateSkyNode, 0, 3, IS_LONG, 1)
	ZEND_ARG_TYPE_INFO(0, x, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, y, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, z, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_pocketmine_world_format_LightEngine_recalculateBlockChunk, 0, 2, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, chunkX, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, chunkZ, IS_LONG, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_pocketmine_world_format_LightEngine_recalculateSkyChunk arginfo_class_pocketmine_world_format_LightEngine_recalculateBlockChunk

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_pocketmine_world_format_LightEngine_executeBlock, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_pocketmine_world_format_LightEngine_executeSky arginfo_class_pocketmine_world_format_LightEngine_executeBlock

ZEND_METHOD(pocketmine_world_format_LightEngine, __construct);
ZEND_METHOD(pocketmine_world_format_LightEngine, setChunk);
ZEND_METHOD(pocketmine_world_format_LightEngine, hasChunk);
ZEND_METHOD(pocketmine_world_format_LightEngine, removeChunk);
ZEND_METHOD(pocketmine_world_format_LightEngine, clear);
ZEND_METHOD(pocketmine_world_format_LightEngine, getHeightMap);
ZEND_METHOD(pocketmine_world_format_LightEngine, recalculateBlockNode);
ZEND_METHOD(pocketmine_world_format_LightEngine, recalculateSkyNode);
ZEND_METHOD(pocketmine_world_format_LightEngine, recalculateBlockChunk);
ZEND_METHOD(pocketmine_world_format_LightEngine, recalculateSkyChunk);
ZEND_METHOD(pocketmine_world_format_LightEngine, executeBlock);
ZEND_METHOD(pocketmine_world_format_LightEngine, executeSky);

static const zend_function_entry class_pocketmine_world_format_LightEngine_methods[] = {
	ZEND_ME(pocketmine_world_format_LightEngine, __construct, arginfo_class_pocketmine_world_format_LightEngine___construct, ZEND_ACC_PUBLIC)
	ZEND_ME(pocketmine_world_format_LightEngine, setChunk, arginfo_class_pocketmine_world_format_LightEngine_setChunk, ZEND_ACC_PUBLIC)
	ZEND_ME(pocketmine_world_format_LightEngine, hasChunk, arginfo_class_pocketmine_world_format_LightEngine_hasChunk, ZEND_ACC_PUBLIC)
	ZEND_ME(pocketmine_world_format_LightEngine, removeChunk, arginfo_class_pocketmine_world_format_LightEngine_removeChunk, ZEND_ACC_PUBLIC)
	ZEND_ME(pocketmine_world_format_LightEngine, clear, arginfo_class_pocketmine_world_format_LightEngine_clear, ZEND_ACC_PUBLIC)
	ZEND_ME(pocketmine_world_format_LightEngine, getHeightMap, arginfo_class_pocketmine_world_format_LightEngine_getHeightMap, ZEND_ACC_PUBLIC)
	ZEND_ME(pocketmine_world_format_LightEngine, recalculateBlockNode, arginfo_class_pocketmine_world_format_LightEngine_recalculateBlockNode, ZEND_ACC_PUBLIC)
	ZEND_ME(pocketmine_world_format_LightEngine, recalculateSkyNode, arginfo_class_pocketmine_world_format_LightEngine_recalculateSkyNode, ZEND_ACC_PUBLIC)
	ZEND_ME(pocketmine_world_format_LightEngine, recalculateBlockChunk, arginfo_class_pocketmine_world_format_LightEngine_recalculateBlockChunk, ZEND_ACC_PUBLIC)
	ZEND_ME(pocketmine_world_format_LightEngine, recalculateSkyChunk, arginfo_class_pocketmine_world_format_LightEngine_recalculateSkyChunk, ZEND_ACC_PUBLIC)
	ZEND_ME(pocketmine_world_format_LightEngine, executeBlock, arginfo_class_pocketmine_world_format_LightEngine_executeBlock, ZEND_ACC_PUBLIC)
	ZEND_ME(pocketmine_world_format_LightEngine, executeSky, arginfo_class_pocketmine_world_format_LightEngine_executeSky, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};
