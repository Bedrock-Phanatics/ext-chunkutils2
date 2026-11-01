--TEST--
LightEngine keeps registered arrays alive and lets go of them when chunks are replaced or removed
--SKIPIF--
<?php if(!extension_loaded('chunkutils2')) die('skip: extension not loaded'); ?>
--FILE--
<?php declare(strict_types=1);

namespace pocketmine\world\format;

$engine = new LightEngine([], [1 => 15], []);

function register(LightEngine $engine, int $x, int $z, bool $withLight) : \WeakReference{
	$empty = $blocks = $light = $sky = [];
	for($y = -4; $y <= 19; ++$y){
		$empty[$y] = 0;
		$blocks[$y] = null;
		$light[$y] = LightArray::fill(0);
		$sky[$y] = LightArray::fill(0);
	}
	if($withLight){
		$blocks[0] = new PalettedBlockArray(0);
		$blocks[0]->set(1, 1, 1, 1);
	}
	$engine->setChunk($x, $z, $empty, $blocks, $light, $sky, array_fill(0, 256, -64));
	return \WeakReference::create($light[0]);
}

$first = register($engine, 0, 0, true);
var_dump($first->get() !== null);
$engine->recalculateBlockChunk(0, 0);
$engine->executeBlock();
var_dump($first->get()->get(1, 1, 1), $first->get()->get(2, 1, 1));

//queued nodes survive the chunk being pointed at new arrays
$engine->recalculateBlockNode(1, 1, 1);
$replacement = register($engine, 0, 0, false);
var_dump($first->get() === null);
$engine->executeBlock();
var_dump($replacement->get()->get(1, 1, 1));

$other = register($engine, 4, 4, false);
$engine->removeChunk(0, 0);
var_dump($replacement->get() === null, $other->get() !== null);
$engine->clear();
var_dump($other->get() === null);

for($i = 0; $i < 50; ++$i){
	register($engine, $i % 3, 0, $i % 2 === 0);
	$engine->recalculateBlockChunk($i % 3, 0);
	$engine->executeBlock();
}
$last = register($engine, 9, 9, false);
unset($engine);
var_dump($last->get() === null);
?>
--EXPECT--
bool(true)
int(15)
int(14)
bool(true)
int(15)
bool(true)
bool(true)
bool(true)
bool(true)
