#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"

#include <unordered_map>

extern enum StreamAPIPlaylist;

class Ranks
{
public:
	void getRanks(std::shared_ptr<GameWrapper> gw);
	void updateRank(std::shared_ptr<GameWrapper> gw);
	std::string toString(std::string args, std::shared_ptr<CVarManagerWrapper> cv);

private:
	std::unordered_map<StreamAPIPlaylist, std::string> ranks;
};