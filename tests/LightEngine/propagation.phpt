--TEST--
LightEngine populates and updates block and sky light across registered chunks
--SKIPIF--
<?php if(!extension_loaded('chunkutils2')) die('skip: extension not loaded'); ?>
--FILE--
<?php declare(strict_types=1);

namespace pocketmine\world\format;

const AIR = 0;
const STONE = 1;
const GLOWSTONE = 2;

$engine = new LightEngine([STONE => 15], [GLOWSTONE => 15], [STONE => true]);

function column(LightEngine $engine, int $chunkX, int $chunkZ, array $blocks) : array{
	$empty = $light = $sky = [];
	for($y = -4; $y <= 19; ++$y){
		$empty[$y] = AIR;
		$blocks[$y] ??= null;
		$light[$y] = LightArray::fill(0);
		$sky[$y] = LightArray::fill(0);
	}
	$engine->setChunk($chunkX, $chunkZ, $empty, $blocks, $light, $sky, array_fill(0, 256, -64));
	return [$blocks, $light, $sky];
}

$floor = new PalettedBlockArray(AIR);
for($x = 0; $x < 16; ++$x) for($z = 0; $z < 16; ++$z) $floor->set($x, 0, $z, STONE);
$floor->set(8, 5, 8, GLOWSTONE);
[, $light, $sky] = column($engine, 0, 0, [0 => $floor]);
[, $northLight] = column($engine, 0, 1, []);

var_dump($engine->recalculateBlockChunk(0, 0));
var_dump($engine->executeBlock() > 0);
var_dump($engine->recalculateSkyChunk(0, 0) > 0);
$engine->executeSky();

echo "block light: ", $light[0]->get(8, 5, 8), " ", $light[0]->get(9, 5, 8), " ", $light[0]->get(8, 5, 15), " ", $northLight[0]->get(8, 5, 0), " ", $light[0]->get(8, 0, 8), "\n";
echo "sky light: ", $sky[0]->get(4, 1, 4), " ", $sky[0]->get(4, 0, 4), " ", $sky[-1]->get(4, 15, 4), " ", $sky[5]->get(4, 4, 4), "\n";
echo "height map: ", implode(",", array_unique($engine->getHeightMap(0, 0))), "\n";

$floor->set(8, 5, 8, AIR);
$engine->recalculateBlockNode(8, 5, 8);
$engine->executeBlock();
echo "after removal: ", $light[0]->get(8, 5, 8), " ", $light[0]->get(9, 5, 8), " ", $northLight[0]->get(8, 5, 0), "\n";

$floor->set(3, 10, 3, STONE);
var_dump($engine->recalculateSkyNode(3, 10, 3));
$engine->executeSky();
echo "under stone: ", $sky[0]->get(3, 10, 3), " ", $sky[0]->get(3, 9, 3), " ", $engine->getHeightMap(0, 0)[(3 << 4) | 3], "\n";

$floor->set(3, 10, 3, AIR);
var_dump($engine->recalculateSkyNode(3, 10, 3));
$engine->executeSky();
echo "stone removed: ", $sky[0]->get(3, 9, 3), "\n";

var_dump($engine->recalculateSkyNode(100, 10, 100));
var_dump($engine->recalculateSkyNode(3, 400, 3));
var_dump($engine->hasChunk(0, 1), $engine->hasChunk(1, 1));
$engine->removeChunk(0, 1);
var_dump($engine->hasChunk(0, 1));
?>
--EXPECT--
int(1)
bool(true)
bool(true)
block light: 15 14 8 7 0
sky light: 15 0 0 15
height map: 1
after removal: 0 0 0
int(11)
under stone: 0 14 11
int(1)
stone removed: 15
NULL
NULL
bool(true)
bool(false)
bool(false)
