<?php

/**
 * @generate-function-entries
 */

namespace pocketmine\world\format;

final class LightEngine{

	/**
	 * @param int[]  $lightFilters
	 * @param int[]  $lightEmitters
	 * @param true[] $directSkyLightBlockers
	 */
	public function __construct(array $lightFilters, array $lightEmitters, array $directSkyLightBlockers){}

	public function setChunk(int $chunkX, int $chunkZ, array $emptyBlockIds, array $blocks, array $blockLight, array $skyLight, array $heightMap) : void{}

	public function hasChunk(int $chunkX, int $chunkZ) : bool{}

	public function removeChunk(int $chunkX, int $chunkZ) : void{}

	public function clear() : void{}

	/** @return int[] */
	public function getHeightMap(int $chunkX, int $chunkZ) : array{}

	public function recalculateBlockNode(int $x, int $y, int $z) : void{}

	public function recalculateSkyNode(int $x, int $y, int $z) : ?int{}

	/** @param int[][] $changes */
	public function applyChanges(array $changes) : void{}

	public function recalculateBlockChunk(int $chunkX, int $chunkZ) : int{}

	public function recalculateSkyChunk(int $chunkX, int $chunkZ) : int{}

	public function executeBlock() : int{}

	public function executeSky() : int{}
}
