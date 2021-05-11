#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"

#include <unordered_map>

enum class StreamAPIPlaylist {
	UNKNOWN = -1,
	DUEL = 1,
	DOUBLES = 2,
	STANDARD = 3,
	CHAOS = 4,
	RANKEDDUEL = 10,
	RANKEDDOUBLES = 11,
	RANKEDSOLOSTANDARD = 12,
	RANKEDSTANDARD = 13,
	RANKEDHOOPS = 27,
	RANKEDRUMBLE = 28,
	RANKEDDROPSHOT = 29,
	RANKEDSNOWDAY = 30,
	CASUAL = 999,
};

class Ranks
{
public:
	void getRanks(std::shared_ptr<GameWrapper> gw);
	void updateRank(std::shared_ptr<GameWrapper> gw);
	std::string toString(std::string args, std::shared_ptr<CVarManagerWrapper> cv);

private:
	std::unordered_map<StreamAPIPlaylist, std::string> ranks;
};