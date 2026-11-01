--TEST--
LightEngine rejects malformed input
--SKIPIF--
<?php if(!extension_loaded('chunkutils2')) die('skip: extension not loaded'); ?>
--FILE--
<?php declare(strict_types=1);

namespace pocketmine\world\format;

function attempt(\Closure $closure) : void{
	try{
		$closure();
		echo "no exception\n";
	}catch(\Throwable $e){
		echo get_class($e), ": ", $e->getMessage(), "\n";
	}
}

attempt(fn() => new LightEngine([1 => 16], [], []));
attempt(fn() => new LightEngine(["stone" => 15], [], []));
attempt(fn() => new LightEngine([], [1 => -1], []));
attempt(fn() => new LightEngine([], [], [-1 => true]));

$engine = new LightEngine([], [], []);
attempt(fn() => $engine->__construct([], [], []));

$empty = $blocks = $light = $sky = [];
for($y = -4; $y <= 19; ++$y){
	$empty[$y] = 0;
	$blocks[$y] = null;
	$light[$y] = LightArray::fill(0);
	$sky[$y] = LightArray::fill(0);
}
$heightMap = array_fill(0, 256, 0);

$missing = $light;
unset($missing[7]);
attempt(fn() => $engine->setChunk(0, 0, $empty, $blocks, $missing, $sky, $heightMap));
attempt(fn() => $engine->setChunk(0, 0, $empty, array_replace($blocks, [3 => LightArray::fill(0)]), $light, $sky, $heightMap));
attempt(fn() => $engine->setChunk(0, 0, $empty, $blocks, array_replace($light, [3 => null]), $sky, $heightMap));
attempt(fn() => $engine->setChunk(0, 0, array_replace($empty, [0 => -5]), $blocks, $light, $sky, $heightMap));
attempt(fn() => $engine->setChunk(0, 0, $empty, $blocks, $light, $sky, array_fill(0, 255, 0)));
attempt(fn() => $engine->setChunk(0, 0, $empty, $blocks, $light, $sky, array_fill(0, 256, 321)));
attempt(fn() => $engine->setChunk(1 << 30, 0, $empty, $blocks, $light, $sky, $heightMap));
var_dump($engine->hasChunk(0, 0));

attempt(fn() => $engine->recalculateBlockChunk(0, 0));
attempt(fn() => $engine->recalculateSkyChunk(5, 5));
attempt(fn() => $engine->getHeightMap(0, 0));
attempt(fn() => clone $engine);
attempt(fn() => serialize($engine));

$engine->recalculateBlockNode(PHP_INT_MAX, 0, PHP_INT_MIN);
var_dump($engine->recalculateSkyNode(PHP_INT_MAX, 0, 0));
var_dump($engine->executeBlock(), $engine->executeSky());
$engine->removeChunk(PHP_INT_MAX, 0);
?>
--EXPECT--
InvalidArgumentException: Light filters must be integers between 0 and 15
InvalidArgumentException: Block state IDs must be integers between 0 and 4294967295
InvalidArgumentException: Light emitters must be integers between 0 and 15
InvalidArgumentException: Block state IDs must be integers between 0 and 4294967295
Error: LightEngine is already constructed
InvalidArgumentException: Block light is missing subchunk 7
InvalidArgumentException: Blocks subchunk 3 must be a pocketmine\world\format\PalettedBlockArray
InvalidArgumentException: Block light subchunk 3 must be a pocketmine\world\format\LightArray
InvalidArgumentException: Empty block ID for subchunk 0 must be an integer between 0 and 4294967295
InvalidArgumentException: Height map must hold 256 integers between -64 and 320
InvalidArgumentException: Height map must hold 256 integers between -64 and 320
InvalidArgumentException: Chunk coordinates must be within +-67108864
bool(false)
InvalidArgumentException: Chunk 0 0 is not registered
InvalidArgumentException: Chunk 5 5 is not registered
InvalidArgumentException: Chunk 0 0 is not registered
Error: Trying to clone an uncloneable object of class pocketmine\world\format\LightEngine
Exception: Serialization of 'pocketmine\world\format\LightEngine' is not allowed
NULL
int(0)
int(0)
