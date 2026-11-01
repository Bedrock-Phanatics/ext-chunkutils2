#ifndef HAVE_LIGHT_ENGINE_H
#define HAVE_LIGHT_ENGINE_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

#include "BlockArrayContainer.h"
#include "LightArray.h"

/*
 * Block and sky light propagation over registered chunk columns, matching PocketMine's LightUpdate, BlockLightUpdate
 * and SkyLightUpdate node for node: same seeds, same queue order, same results. Positions outside the registered
 * columns or the world's height are skipped, like SubChunkExplorer does for unloaded chunks.
 */
template<typename Block>
class LightEngine {
public:
	static const int MIN_SUBCHUNK = -4;
	static const int MAX_SUBCHUNK = 19;
	static const int SUBCHUNK_COUNT = MAX_SUBCHUNK - MIN_SUBCHUNK + 1;
	static const int MIN_Y = MIN_SUBCHUNK * 16;
	static const int MAX_Y = (MAX_SUBCHUNK + 1) * 16;
	static const uint8_t BASE_LIGHT_FILTER = 1;

	enum Type { BLOCK = 0, SKY = 1 };

	struct Properties {
		uint8_t filter = BASE_LIGHT_FILTER;
		uint8_t emitter = 0;
		bool blocksDirectSkyLight = false;
	};

private:
	static const int8_t NOT_VISITED = -2;
	static const int8_t SEED = -1;

	struct Scratch {
		std::array<int32_t, 4096> nodes[2];
		std::array<int8_t, 4096> spread;
		std::array<uint8_t, 512> removal;
	};

	struct Slot {
		Block emptyBlockId = 0;
		const BlockArrayContainer<Block> *blocks = nullptr;
		LightArray *light[2] = {nullptr, nullptr};

		//palette entries mapped to their light properties, checked against the live palette once per call
		uint64_t checkedCall = 0;
		uint8_t bitsPerBlock = 0;
		uint8_t blocksPerWord = 0;
		uint32_t mask = 0;
		const uint32_t *words = nullptr;
		std::vector<Block> palette;
		std::vector<Properties> paletteProperties;
		Properties emptyProperties;

		std::unique_ptr<Scratch> scratch;
		bool nodesUsed[2] = {false, false};
		bool visitsUsed = false;
	};

	struct Column {
		int32_t x;
		int32_t z;
		std::array<Slot, SUBCHUNK_COUNT> slots;
		std::array<int32_t, 256> heightMap;
	};

	struct Node {
		int32_t x;
		int32_t y;
		int32_t z;
		int32_t level;
	};

	struct Position {
		int32_t x;
		int32_t y;
		int32_t z;
		Slot *slot;
		unsigned index;
	};

	struct RemovalNode {
		int32_t x;
		int32_t y;
		int32_t z;
		int32_t level;
	};

	static const int OFFSETS[6][3];

	std::unordered_map<Block, Properties> properties;
	std::unordered_map<int64_t, std::unique_ptr<Column>> columns;
	Column *lastColumn = nullptr;
	uint64_t call = 0;

	std::vector<Node> nodes[2];
	std::vector<Slot *> nodeSlots[2];
	std::vector<Slot *> visitedSlots;

	static int64_t columnKey(int32_t x, int32_t z) {
		return static_cast<int64_t>((static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint32_t>(z));
	}

	static unsigned localIndex(int32_t x, int32_t y, int32_t z) {
		return ((x & 0xf) << 8) | ((z & 0xf) << 4) | (y & 0xf);
	}

	Properties lookup(Block block) const {
		auto found = properties.find(block);
		return found != properties.end() ? found->second : Properties();
	}

	Column *columnAt(int32_t chunkX, int32_t chunkZ) {
		if (lastColumn != nullptr && lastColumn->x == chunkX && lastColumn->z == chunkZ) {
			return lastColumn;
		}
		auto found = columns.find(columnKey(chunkX, chunkZ));
		if (found == columns.end()) {
			return nullptr;
		}
		lastColumn = found->second.get();
		return lastColumn;
	}

	Slot *slotAt(int32_t x, int32_t y, int32_t z) {
		int32_t subChunkY = y >> 4;
		if (subChunkY < MIN_SUBCHUNK || subChunkY > MAX_SUBCHUNK) {
			return nullptr;
		}
		Column *column = columnAt(x >> 4, z >> 4);
		if (column == nullptr) {
			return nullptr;
		}
		return &column->slots[subChunkY - MIN_SUBCHUNK];
	}

	void check(Slot &slot) {
		if (slot.checkedCall == call) {
			return;
		}
		slot.checkedCall = call;
		if (slot.blocks == nullptr) {
			return;
		}
		auto palette = slot.blocks->getPalette();
		auto words = slot.blocks->getWordArray();
		auto bitsPerBlock = static_cast<uint8_t>(slot.blocks->getBitsPerBlock());
		slot.words = reinterpret_cast<const uint32_t *>(words.data());
		if (
			bitsPerBlock == slot.bitsPerBlock &&
			static_cast<size_t>(palette.size()) == slot.palette.size() &&
			(palette.size() == 0 || std::memcmp(palette.data(), slot.palette.data(), palette.size() * sizeof(Block)) == 0)
		) {
			return;
		}
		slot.bitsPerBlock = bitsPerBlock;
		slot.blocksPerWord = bitsPerBlock == 0 ? 0 : static_cast<uint8_t>(32 / bitsPerBlock);
		slot.mask = bitsPerBlock == 0 ? 0 : (bitsPerBlock == 32 ? 0xffffffffu : ((1u << bitsPerBlock) - 1));
		slot.palette.assign(palette.begin(), palette.end());
		slot.paletteProperties.resize(slot.palette.size());
		for (size_t i = 0; i < slot.palette.size(); ++i) {
			slot.paletteProperties[i] = lookup(slot.palette[i]);
		}
	}

	unsigned paletteOffset(const Slot &slot, unsigned index) const {
		if (slot.bitsPerBlock == 0) {
			return 0;
		}
		return (slot.words[index / slot.blocksPerWord] >> ((index % slot.blocksPerWord) * slot.bitsPerBlock)) & slot.mask;
	}

	//SubChunk::getBlockStateId(), which reads the first layer or falls back to the empty block
	Block blockAt(Slot &slot, unsigned index) {
		check(slot);
		if (slot.blocks == nullptr) {
			return slot.emptyBlockId;
		}
		unsigned offset = paletteOffset(slot, index);
		return offset < slot.palette.size() ? slot.palette[offset] : slot.emptyBlockId;
	}

	const Properties &propertiesAt(Slot &slot, unsigned index) {
		check(slot);
		if (slot.blocks == nullptr) {
			return slot.emptyProperties;
		}
		unsigned offset = paletteOffset(slot, index);
		return offset < slot.paletteProperties.size() ? slot.paletteProperties[offset] : slot.emptyProperties;
	}

	static int lightAt(const Slot &slot, Type type, unsigned index) {
		return slot.light[type]->get(index >> 8, index & 0xf, (index >> 4) & 0xf);
	}

	static void setLightAt(Slot &slot, Type type, unsigned index, int level) {
		slot.light[type]->set(index >> 8, index & 0xf, (index >> 4) & 0xf, static_cast<uint8_t>(level));
	}

	Scratch &scratchOf(Slot &slot) {
		if (!slot.scratch) {
			slot.scratch.reset(new Scratch);
		}
		return *slot.scratch;
	}

	//LightUpdate::setAndUpdateLight(); nodes in unloaded chunks or outside the world are never applied, so they're dropped
	void setNode(Type type, int32_t x, int32_t y, int32_t z, int level) {
		Slot *slot = slotAt(x, y, z);
		if (slot == nullptr) {
			return;
		}
		Scratch &scratch = scratchOf(*slot);
		if (!slot->nodesUsed[type]) {
			slot->nodesUsed[type] = true;
			scratch.nodes[type].fill(-1);
			nodeSlots[type].push_back(slot);
		}
		int32_t &existing = scratch.nodes[type][localIndex(x, y, z)];
		if (existing >= 0) {
			nodes[type][existing].level = level;
		} else {
			existing = static_cast<int32_t>(nodes[type].size());
			nodes[type].push_back({x, y, z, level});
		}
	}

	void clearNodes(Type type) {
		for (Slot *slot : nodeSlots[type]) {
			slot->nodesUsed[type] = false;
		}
		nodeSlots[type].clear();
		nodes[type].clear();
	}

	Scratch &visitsOf(Slot &slot) {
		Scratch &scratch = scratchOf(slot);
		if (!slot.visitsUsed) {
			slot.visitsUsed = true;
			scratch.spread.fill(NOT_VISITED);
			scratch.removal.fill(0);
			visitedSlots.push_back(&slot);
		}
		return scratch;
	}

	int effectiveLight(Type type, int32_t x, int32_t y, int32_t z) {
		if (type == SKY && y >= MAX_Y) {
			return 15;
		}
		Slot *slot = slotAt(x, y, z);
		return slot != nullptr ? lightAt(*slot, type, localIndex(x, y, z)) : 0;
	}

	int highestAdjacentLight(Type type, int32_t x, int32_t y, int32_t z) {
		int adjacent = 0;
		for (const auto &offset : OFFSETS) {
			adjacent = std::max(adjacent, effectiveLight(type, x + offset[0], y + offset[1], z + offset[2]));
			if (adjacent == 15) {
				break;
			}
		}
		return adjacent;
	}

	//Chunk::getHighestBlockAt(), or MIN_Y - 1 for an empty column
	int highestBlock(Column &column, int x, int z, int fromSubChunk = MAX_SUBCHUNK) {
		for (int subChunkY = fromSubChunk; subChunkY >= MIN_SUBCHUNK; --subChunkY) {
			Slot &slot = column.slots[subChunkY - MIN_SUBCHUNK];
			if (slot.blocks == nullptr) {
				continue;
			}
			for (int y = 15; y >= 0; --y) {
				if (blockAt(slot, localIndex(x, y, z)) != slot.emptyBlockId) {
					return y | (subChunkY * 16);
				}
			}
		}
		return MIN_Y - 1;
	}

	bool blocksSkyAt(Column &column, int x, int y, int z) {
		Slot &slot = column.slots[(y >> 4) - MIN_SUBCHUNK];
		return propertiesAt(slot, localIndex(x, y, z)).blocksDirectSkyLight;
	}

	//SkyLightUpdate::recalculateHeightMapColumn()
	int heightMapColumn(Column &column, int x, int z) {
		int y = highestBlock(column, x, z);
		if (y < MIN_Y) {
			return MIN_Y;
		}
		for (; y >= MIN_Y; --y) {
			if (blocksSkyAt(column, x, y, z)) {
				break;
			}
		}
		return y + 1;
	}

	void computeRemoveLight(Type type, const Position &at, int oldAdjacentLevel, std::deque<RemovalNode> &removalQueue, std::deque<Position> &spreadQueue) {
		int current = lightAt(*at.slot, type, at.index);
		Scratch &visits = visitsOf(*at.slot);
		if (current != 0 && current < oldAdjacentLevel) {
			setLightAt(*at.slot, type, at.index, 0);
			uint8_t bit = static_cast<uint8_t>(1u << (at.index & 7));
			if ((visits.removal[at.index >> 3] & bit) == 0) {
				visits.removal[at.index >> 3] |= bit;
				if (current > 1) {
					removalQueue.push_back({at.x, at.y, at.z, current});
				}
			}
		} else if (current >= oldAdjacentLevel) {
			if (visits.spread[at.index] == NOT_VISITED) {
				visits.spread[at.index] = SEED;
				spreadQueue.push_back(at);
			}
		}
	}

	void computeSpreadLight(Type type, const Position &at, int newAdjacentLevel, int side, std::deque<Position> &spreadQueue) {
		int current = lightAt(*at.slot, type, at.index);
		int potentialLight = newAdjacentLevel - propertiesAt(*at.slot, at.index).filter;
		if (current < potentialLight) {
			setLightAt(*at.slot, type, at.index, potentialLight);
			Scratch &visits = visitsOf(*at.slot);
			if (visits.spread[at.index] == NOT_VISITED && potentialLight > 1) {
				//the opposite face is the one this node was lit from
				visits.spread[at.index] = static_cast<int8_t>(side ^ 1);
				spreadQueue.push_back(at);
			}
		}
	}

public:
	LightEngine() = default;
	LightEngine(const LightEngine &) = delete;
	LightEngine &operator=(const LightEngine &) = delete;

	void setProperties(Block block, const Properties &value) {
		properties[block] = value;
	}

	/**
	 * Registers a chunk column, or points an already registered one at new arrays while keeping its queued nodes.
	 * Arrays are indexed from the lowest subchunk; blocks may be null for subchunks without layers. The engine borrows
	 * the arrays, so the caller keeps them alive until the column is replaced or removed.
	 */
	void setColumn(int32_t chunkX, int32_t chunkZ, const Block *emptyBlockIds, const BlockArrayContainer<Block> *const *blocks, LightArray *const *blockLight, LightArray *const *skyLight, const int32_t *heightMap) {
		Column *column = columnAt(chunkX, chunkZ);
		if (column == nullptr) {
			std::unique_ptr<Column> created(new Column());
			created->x = chunkX;
			created->z = chunkZ;
			column = created.get();
			columns.emplace(columnKey(chunkX, chunkZ), std::move(created));
		}
		for (int i = 0; i < SUBCHUNK_COUNT; ++i) {
			Slot &slot = column->slots[i];
			slot.emptyBlockId = emptyBlockIds[i];
			slot.blocks = blocks[i];
			slot.light[BLOCK] = blockLight[i];
			slot.light[SKY] = skyLight[i];
			slot.emptyProperties = lookup(slot.emptyBlockId);
			slot.checkedCall = 0;
			slot.bitsPerBlock = 0xff;
		}
		std::copy(heightMap, heightMap + 256, column->heightMap.begin());
	}

	bool hasColumn(int32_t chunkX, int32_t chunkZ) {
		return columnAt(chunkX, chunkZ) != nullptr;
	}

	void removeColumn(int32_t chunkX, int32_t chunkZ) {
		auto found = columns.find(columnKey(chunkX, chunkZ));
		if (found == columns.end()) {
			return;
		}
		for (Type type : {BLOCK, SKY}) {
			if (std::none_of(found->second->slots.begin(), found->second->slots.end(), [type](const Slot &slot) { return slot.nodesUsed[type]; })) {
				continue;
			}
			//PocketMine skips nodes in unloaded chunks, so keep the others in their original order
			std::vector<Node> queued;
			queued.swap(nodes[type]);
			clearNodes(type);
			for (const Node &node : queued) {
				if ((node.x >> 4) != chunkX || (node.z >> 4) != chunkZ) {
					setNode(type, node.x, node.y, node.z, node.level);
				}
			}
		}
		if (lastColumn == found->second.get()) {
			lastColumn = nullptr;
		}
		columns.erase(found);
	}

	void clear() {
		clearNodes(BLOCK);
		clearNodes(SKY);
		lastColumn = nullptr;
		columns.clear();
	}

	const std::array<int32_t, 256> *heightMap(int32_t chunkX, int32_t chunkZ) {
		Column *column = columnAt(chunkX, chunkZ);
		return column != nullptr ? &column->heightMap : nullptr;
	}

	//BlockLightUpdate::recalculateNode()
	void recalculateBlockNode(int32_t x, int32_t y, int32_t z) {
		++call;
		Slot *slot = slotAt(x, y, z);
		if (slot == nullptr) {
			return;
		}
		const Properties block = propertiesAt(*slot, localIndex(x, y, z));
		setNode(BLOCK, x, y, z, std::max<int>(block.emitter, highestAdjacentLight(BLOCK, x, y, z) - block.filter));
	}

	//SkyLightUpdate::recalculateNode(), returning the column's height map value afterwards, or INT32_MIN if unloaded
	int32_t recalculateSkyNode(int32_t x, int32_t y, int32_t z) {
		++call;
		Slot *slot = slotAt(x, y, z);
		if (slot == nullptr) {
			return INT32_MIN;
		}
		Column &column = *columnAt(x >> 4, z >> 4);
		int lx = x & 0xf;
		int lz = z & 0xf;
		int32_t &height = column.heightMap[(lz << 4) | lx];
		int oldHeightMap = height;
		const Properties source = propertiesAt(*slot, localIndex(x, y, z));

		int newHeightMap;
		int yPlusOne = y + 1;
		if (yPlusOne == oldHeightMap) {
			newHeightMap = heightMapColumn(column, lx, lz);
			height = newHeightMap;
		} else if (yPlusOne > oldHeightMap) {
			if (source.blocksDirectSkyLight) {
				height = yPlusOne;
				newHeightMap = yPlusOne;
			} else {
				return height;
			}
		} else {
			newHeightMap = oldHeightMap;
		}

		if (newHeightMap >= oldHeightMap) {
			for (int i = y - 1; i >= oldHeightMap; --i) {
				setNode(SKY, x, i, z, 0);
			}
			setNode(SKY, x, y, z, std::max(0, highestAdjacentLight(SKY, x, y, z) - source.filter));
		} else {
			for (int i = y; i >= newHeightMap; --i) {
				setNode(SKY, x, i, z, 15);
			}
		}
		return height;
	}

	//BlockLightUpdate::recalculateChunk()
	int recalculateBlockChunk(int32_t chunkX, int32_t chunkZ) {
		++call;
		Column *column = columnAt(chunkX, chunkZ);
		if (column == nullptr) {
			return -1;
		}
		int lightSources = 0;
		for (int i = 0; i < SUBCHUNK_COUNT; ++i) {
			Slot &slot = column->slots[i];
			*slot.light[BLOCK] = LightArray(LightLevel(0));
			if (slot.blocks == nullptr) {
				continue;
			}
			check(slot);
			bool emits = false;
			for (const auto &entry : slot.paletteProperties) {
				if (entry.emitter > 0) {
					emits = true;
					break;
				}
			}
			if (!emits) {
				continue;
			}
			int baseX = chunkX * 16;
			int baseY = (i + MIN_SUBCHUNK) * 16;
			int baseZ = chunkZ * 16;
			for (int x = 0; x < 16; ++x) {
				for (int z = 0; z < 16; ++z) {
					for (int y = 0; y < 16; ++y) {
						int light = propertiesAt(slot, localIndex(x, y, z)).emitter;
						if (light > 0) {
							setNode(BLOCK, baseX + x, baseY + y, baseZ + z, light);
							lightSources++;
						}
					}
				}
			}
		}
		return lightSources;
	}

	//SkyLightUpdate::recalculateChunk(), which also rebuilds the column's height map
	int recalculateSkyChunk(int32_t chunkX, int32_t chunkZ) {
		++call;
		Column *columnPtr = columnAt(chunkX, chunkZ);
		if (columnPtr == nullptr) {
			return -1;
		}
		Column &column = *columnPtr;

		int maxSubChunkY = MAX_SUBCHUNK;
		for (; maxSubChunkY >= MIN_SUBCHUNK; maxSubChunkY--) {
			if (column.slots[maxSubChunkY - MIN_SUBCHUNK].blocks != nullptr) {
				break;
			}
		}
		column.heightMap.fill(MIN_Y);
		if (maxSubChunkY >= MIN_SUBCHUNK) {
			for (int z = 0; z < 16; ++z) {
				for (int x = 0; x < 16; ++x) {
					int y = highestBlock(column, x, z, maxSubChunkY);
					if (y < MIN_Y) {
						continue;
					}
					for (; y >= MIN_Y; --y) {
						if (blocksSkyAt(column, x, y, z)) {
							column.heightMap[(z << 4) | x] = y + 1;
							break;
						}
					}
				}
			}
		}

		int highestHeightMapPlusOne = *std::max_element(column.heightMap.begin(), column.heightMap.end()) + 1;
		int lowestClearSubChunk = (highestHeightMapPlusOne >> 4) + ((highestHeightMapPlusOne & 0xf) != 0 ? 1 : 0);
		for (int y = MIN_SUBCHUNK; y <= MAX_SUBCHUNK; y++) {
			*column.slots[y - MIN_SUBCHUNK].light[SKY] = LightArray(LightLevel(y < lowestClearSubChunk ? 0 : 15));
		}

		int lightSources = 0;
		int baseX = chunkX * 16;
		int baseZ = chunkZ * 16;
		for (int x = 0; x < 16; ++x) {
			for (int z = 0; z < 16; ++z) {
				int currentHeight = column.heightMap[(z << 4) | x];
				if (currentHeight == MAX_Y) {
					//lit from above the world, which propagation never checks
					int y = currentHeight - 1;
					Slot *slot = slotAt(x + baseX, y, z + baseZ);
					if (slot != nullptr) {
						int filter = propertiesAt(*slot, localIndex(x, y, z)).filter;
						setNode(SKY, x + baseX, y, z + baseZ, std::max(0, 15 - filter));
					}
				} else {
					int maxAdjacentHeight = MIN_Y;
					if (x != 0) {
						maxAdjacentHeight = std::max(maxAdjacentHeight, column.heightMap[(z << 4) | (x - 1)]);
					}
					if (x != 15) {
						maxAdjacentHeight = std::max(maxAdjacentHeight, column.heightMap[(z << 4) | (x + 1)]);
					}
					if (z != 0) {
						maxAdjacentHeight = std::max(maxAdjacentHeight, column.heightMap[((z - 1) << 4) | x]);
					}
					if (z != 15) {
						maxAdjacentHeight = std::max(maxAdjacentHeight, column.heightMap[((z + 1) << 4) | x]);
					}
					int nodeColumnEnd = std::max(currentHeight, maxAdjacentHeight - 2);
					for (int y = currentHeight; y <= nodeColumnEnd; y++) {
						setNode(SKY, x + baseX, y, z + baseZ, 15);
						lightSources++;
					}
					for (int y = nodeColumnEnd + 1, yMax = lowestClearSubChunk * 16; y < yMax; y++) {
						Slot *slot = slotAt(x + baseX, y, z + baseZ);
						if (slot != nullptr) {
							setLightAt(*slot, SKY, localIndex(x, y, z), 15);
						}
					}
				}
			}
		}
		return lightSources;
	}

	//LightUpdate::execute(); queued nodes are consumed
	int execute(Type type) {
		++call;
		std::deque<RemovalNode> removalQueue;
		std::deque<Position> spreadQueue;

		//prepareNodes()
		for (const Node &node : nodes[type]) {
			Slot *slot = slotAt(node.x, node.y, node.z);
			if (slot == nullptr) {
				continue;
			}
			unsigned index = localIndex(node.x, node.y, node.z);
			int oldLevel = lightAt(*slot, type, index);
			if (oldLevel != node.level) {
				setLightAt(*slot, type, index, node.level);
				Scratch &visits = visitsOf(*slot);
				if (oldLevel < node.level) {
					visits.spread[index] = SEED;
					spreadQueue.push_back({node.x, node.y, node.z, slot, index});
				} else {
					visits.removal[index >> 3] |= static_cast<uint8_t>(1u << (index & 7));
					removalQueue.push_back({node.x, node.y, node.z, oldLevel});
				}
			}
		}
		clearNodes(type);

		int touched = 0;
		while (!removalQueue.empty()) {
			touched++;
			RemovalNode node = removalQueue.front();
			removalQueue.pop_front();
			for (const auto &offset : OFFSETS) {
				int32_t cx = node.x + offset[0];
				int32_t cy = node.y + offset[1];
				int32_t cz = node.z + offset[2];
				Slot *slot = slotAt(cx, cy, cz);
				if (slot == nullptr) {
					continue;
				}
				computeRemoveLight(type, {cx, cy, cz, slot, localIndex(cx, cy, cz)}, node.level, removalQueue, spreadQueue);
			}
		}

		while (!spreadQueue.empty()) {
			touched++;
			Position node = spreadQueue.front();
			spreadQueue.pop_front();
			Scratch &visits = visitsOf(*node.slot);
			int from = visits.spread[node.index];
			visits.spread[node.index] = NOT_VISITED;

			int newAdjacentLight = lightAt(*node.slot, type, node.index);
			if (newAdjacentLight <= 0) {
				continue;
			}
			for (int side = 0; side < 6; ++side) {
				if (from == side) {
					continue;
				}
				int32_t cx = node.x + OFFSETS[side][0];
				int32_t cy = node.y + OFFSETS[side][1];
				int32_t cz = node.z + OFFSETS[side][2];
				Slot *slot = slotAt(cx, cy, cz);
				if (slot == nullptr) {
					continue;
				}
				computeSpreadLight(type, {cx, cy, cz, slot, localIndex(cx, cy, cz)}, newAdjacentLight, side, spreadQueue);
			}
		}

		for (Slot *slot : visitedSlots) {
			slot->visitsUsed = false;
		}
		visitedSlots.clear();
		return touched;
	}
};

template<typename Block> const int LightEngine<Block>::MIN_SUBCHUNK;
template<typename Block> const int LightEngine<Block>::MAX_SUBCHUNK;
template<typename Block> const int LightEngine<Block>::SUBCHUNK_COUNT;
template<typename Block> const int LightEngine<Block>::MIN_Y;
template<typename Block> const int LightEngine<Block>::MAX_Y;
template<typename Block> const uint8_t LightEngine<Block>::BASE_LIGHT_FILTER;
template<typename Block> const int8_t LightEngine<Block>::NOT_VISITED;
template<typename Block> const int8_t LightEngine<Block>::SEED;

//Facing::OFFSET: down, up, north, south, west, east
template<typename Block>
const int LightEngine<Block>::OFFSETS[6][3] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};

#endif
