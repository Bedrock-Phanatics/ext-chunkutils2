--TEST--
LightEngine::applyChanges() queues the same updates as recalculating after every change
--SKIPIF--
<?php if(!extension_loaded('chunkutils2')) die('skip: extension not loaded'); ?>
--FILE--
<?php declare(strict_types=1);

namespace pocketmine\world\format;

const AIR = 0;
const STONE = 1;
const GLOWSTONE = 2;
const GLASS = 3;

function area() : array{
	$engine = new LightEngine([STONE => 15, GLASS => 1], [GLOWSTONE => 15], [STONE => true]);
	$columns = [];
	for($chunkX = 0; $chunkX <= 1; ++$chunkX){
		$empty = $blocks = $light = $sky = [];
		for($y = -4; $y <= 19; ++$y){
			$empty[$y] = AIR;
			$blocks[$y] = null;
			$light[$y] = LightArray::fill(0);
			$sky[$y] = LightArray::fill(0);
		}
		foreach([3, 4, 5, 6, 7] as $y) $blocks[$y] = new PalettedBlockArray(AIR);
		for($x = 0; $x < 16; ++$x) for($z = 0; $z < 16; ++$z) $blocks[4]->set($x, 0, $z, STONE);
		$columns[$chunkX] = [$empty, $blocks, $light, $sky];
		$engine->setChunk($chunkX, 0, $empty, $blocks, $light, $sky, array_fill(0, 256, -64));
	}
	foreach([0, 1] as $chunkX){
		$engine->recalculateBlockChunk($chunkX, 0);
		$engine->executeBlock();
		$engine->recalculateSkyChunk($chunkX, 0);
		$engine->executeSky();
	}
	return [$engine, $columns];
}

function snapshot(LightEngine $engine, array $columns) : string{
	$data = "";
	foreach($columns as $chunkX => [, , $light, $sky]){
		foreach($light as $y => $array) $data .= $array->getData() . $sky[$y]->getData();
		$data .= implode(",", $engine->getHeightMap($chunkX, 0));
	}
	return md5($data);
}

$changes = [];
mt_srand(7);
for($i = 0; $i < 300; ++$i){
	$changes[] = [mt_rand(0, 31), mt_rand(60, 120), mt_rand(0, 15), [AIR, STONE, GLOWSTONE, GLASS][mt_rand(0, 3)]];
}
$changes[] = [15, 64, 4, GLOWSTONE];
$changes[] = [15, 64, 4, AIR];
$changes[] = [16, 100, 4, STONE];
$changes[] = [16, 100, 4, GLASS];

[$sequential, $columns] = area();
foreach($changes as [$x, $y, $z, $block]){
	$columns[$x >> 4][1][$y >> 4]->set($x & 15, $y & 15, $z & 15, $block);
	$sequential->recalculateSkyNode($x, $y, $z);
	$sequential->recalculateBlockNode($x, $y, $z);
}
$sequential->executeBlock();
$sequential->executeSky();
$expected = snapshot($sequential, $columns);

[$batched, $columns] = area();
$applied = [];
foreach($changes as [$x, $y, $z, $block]){
	$layer = $columns[$x >> 4][1][$y >> 4];
	$applied[] = [$x, $y, $z, $layer->get($x & 15, $y & 15, $z & 15), $block];
	$layer->set($x & 15, $y & 15, $z & 15, $block);
}
$batched->applyChanges($applied);
$batched->executeBlock();
$batched->executeSky();
var_dump(snapshot($batched, $columns) === $expected);
var_dump($expected !== snapshot(...area()));

$batched->applyChanges([]);
foreach([[[1, 2, 3, 4]], [[1, 2, 3, 4, -1]], [[1, 2, 3, "4", 5]], [[PHP_INT_MAX, 2, 3, 4, 5]], ["x"]] as $bad){
	try{
		$batched->applyChanges($bad);
		echo "no exception\n";
	}catch(\InvalidArgumentException $e){
		echo $e->getMessage(), "\n";
	}
}
$batched->applyChanges([[1000, 64, 1000, STONE, AIR]]);
var_dump($batched->executeBlock(), $batched->executeSky());
?>
--EXPECT--
bool(true)
bool(true)
Changes must be lists of x, y, z, the previous block state ID and the new one
Changes must be lists of x, y, z, the previous block state ID and the new one
Changes must be lists of x, y, z, the previous block state ID and the new one
Changes must be lists of x, y, z, the previous block state ID and the new one
Changes must be lists of x, y, z, the previous block state ID and the new one
int(0)
int(0)
