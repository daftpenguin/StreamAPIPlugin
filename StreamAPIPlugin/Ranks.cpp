#include "Ranks.h"

#include <unordered_set>
#include <sstream>
#include <string>
#include <boost/algorithm/string.hpp>

using namespace std;

unordered_map<StreamAPIPlaylist, string> PlaylistIdToName({
	{ StreamAPIPlaylist::UNKNOWN, "Unknown" },
	{ StreamAPIPlaylist::DUEL, "Casual 1v1" },
	{ StreamAPIPlaylist::DOUBLES, "Casual 2v2" },
	{ StreamAPIPlaylist::STANDARD, "Casual 3v3" },
	{ StreamAPIPlaylist::CHAOS, "Casual 4v4" },
	{ StreamAPIPlaylist::RANKEDDUEL, "1v1" },
	{ StreamAPIPlaylist::RANKEDDOUBLES, "2v2" },
	{ StreamAPIPlaylist::RANKEDSTANDARD, "3v3" },
	{ StreamAPIPlaylist::RANKEDHOOPS, "Hoops" },
	{ StreamAPIPlaylist::RANKEDRUMBLE, "Rumble" },
	{ StreamAPIPlaylist::RANKEDDROPSHOT, "Dropshot" },
	{ StreamAPIPlaylist::RANKEDSNOWDAY, "Snowday" },
	{ StreamAPIPlaylist::CASUAL, "Casual" }
	});

StreamAPIPlaylist casualRetrievalPlaylist = StreamAPIPlaylist::DUEL;

unordered_set<StreamAPIPlaylist> casualPlaylists({
	StreamAPIPlaylist::DUEL,
	StreamAPIPlaylist::DOUBLES,
	StreamAPIPlaylist::STANDARD,
	StreamAPIPlaylist::CHAOS
	});

vector<StreamAPIPlaylist> playlistsToRetrieve({
	StreamAPIPlaylist::RANKEDDUEL,
	StreamAPIPlaylist::RANKEDDOUBLES,
	StreamAPIPlaylist::RANKEDSTANDARD,
	StreamAPIPlaylist::RANKEDHOOPS,
	StreamAPIPlaylist::RANKEDRUMBLE,
	StreamAPIPlaylist::RANKEDDROPSHOT,
	StreamAPIPlaylist::RANKEDSNOWDAY,
	casualRetrievalPlaylist,
	});

std::unordered_map<std::string, StreamAPIPlaylist> playlistArgToId({
	{ "casuals", StreamAPIPlaylist::CASUAL },
	{ "casual", StreamAPIPlaylist::CASUAL },
	{ "unrank", StreamAPIPlaylist::CASUAL },
	{ "unranked", StreamAPIPlaylist::CASUAL },

	{ "ones", StreamAPIPlaylist::RANKEDDUEL },
	{ "1s", StreamAPIPlaylist::RANKEDDUEL },
	{ "1v1", StreamAPIPlaylist::RANKEDDUEL },
	{ "1v1s", StreamAPIPlaylist::RANKEDDUEL },
	{ "duel", StreamAPIPlaylist::RANKEDDUEL },

	{ "twos", StreamAPIPlaylist::RANKEDDOUBLES },
	{ "2s", StreamAPIPlaylist::RANKEDDOUBLES },
	{ "2v2", StreamAPIPlaylist::RANKEDDOUBLES },
	{ "2v2s", StreamAPIPlaylist::RANKEDDOUBLES },
	{ "doubles", StreamAPIPlaylist::RANKEDDOUBLES },

	{ "threes", StreamAPIPlaylist::RANKEDSTANDARD },
	{ "3s", StreamAPIPlaylist::RANKEDSTANDARD },
	{ "3v3", StreamAPIPlaylist::RANKEDSTANDARD },
	{ "3v3s", StreamAPIPlaylist::RANKEDSTANDARD },
	{ "standard", StreamAPIPlaylist::RANKEDSTANDARD },

	{ "hoops", StreamAPIPlaylist::RANKEDHOOPS },
	{ "rumble", StreamAPIPlaylist::RANKEDRUMBLE },
	{ "dropshot", StreamAPIPlaylist::RANKEDDROPSHOT },
	{ "snowday", StreamAPIPlaylist::RANKEDSNOWDAY },
	{ "hockey", StreamAPIPlaylist::RANKEDSNOWDAY },
	});

std::vector<string> tierToRankName({
	"Unranked",
	"Bronze 1",
	"Bronze 2",
	"Bronze 3",
	"Silver 1",
	"Silver 2",
	"Silver 3",
	"Gold 1",
	"Gold 2",
	"Gold 3",
	"Platinum 1",
	"Platinum 2",
	"Platinum 3",
	"Diamond 1",
	"Diamond 2",
	"Diamond 3",
	"Champion 1",
	"Champion 2",
	"Champion 3",
	"Grand Champion 1",
	"Grand Champion 2",
	"Grand Champion 3",
	"Supersonic Legend"
});

void Ranks::getRanks(std::shared_ptr<GameWrapper> gw)
{
	auto UID = gw->GetUniqueID();
	auto mmrWrapper = gw->GetMMRWrapper();

	for (auto p : playlistsToRetrieve) {
		auto mmr = round(mmrWrapper.GetPlayerMMR(UID, (int) p));
		auto rank = mmrWrapper.GetPlayerRank(UID, (int) p);

		auto playlistName = PlaylistIdToName[p == casualRetrievalPlaylist ? StreamAPIPlaylist::CASUAL : p];
		auto tierName = tierToRankName[rank.Tier];

		stringstream oss;
		oss << playlistName << ": " << tierName;
		if (rank.Tier != 0)
			oss << " Div " << rank.Division + 1;
		oss << " (" << mmr << ")";
		ranks[p] = oss.str();
	}
}

void Ranks::updateRank(std::shared_ptr<GameWrapper> gw)
{
	getRanks(gw);
}

std::string Ranks::toString(std::string args, std::shared_ptr<CVarManagerWrapper> cv)
{
	stringstream oss;

	if (args.empty()) {
		bool isFirst = true;
		for (auto playlistId : playlistsToRetrieve) {
			auto it = ranks.find(playlistId);
			if (it != ranks.end()) {
				if (!isFirst) oss << ", ";
				oss << it->second;
				isFirst = false;
			}
		}
	}
	else if (args.compare("json") == 0) {
		// This is ugly but we're actually going to be pushing the json like { "1v1": "1v1: Champion 2 Div 3 (420)", ... }
		oss << "{";
		bool isFirst = true;
		for (auto playlistId : playlistsToRetrieve) {
			auto playlistName = PlaylistIdToName[playlistId == casualRetrievalPlaylist ? StreamAPIPlaylist::CASUAL : playlistId];

			auto it = ranks.find(playlistId);
			if (it != ranks.end()) {
				if (!isFirst) oss << ",";
				oss << "\"" << boost::to_lower_copy(playlistName) << "\":\"" << it->second << "\"";
				isFirst = false;
			}
		}
		oss << "}";
	}
	else {
		vector<StreamAPIPlaylist> playlists;
		stringstream iss(args);

		bool isFirst = true;
		while (iss.good()) {
			string playlist;
			getline(iss, playlist, ',');
			boost::trim(playlist);
			boost::to_lower(playlist);

			cv->log("Parsed playlist: " + playlist);

			auto it = playlistArgToId.find(playlist);
			if (it == playlistArgToId.end()) {
				return "Unknown playlist (" + playlist + "). Acceptable playlists: 1s, 2s, 3s, hoops, rumble, dropshot, and snowday.";
			}
			
			auto playlistId = it->second;
			if (playlistId == StreamAPIPlaylist::CASUAL) {
				playlistId = casualRetrievalPlaylist;
			}

			auto rankIt = ranks.find(playlistId);
			if (rankIt == ranks.end()) {
				return "Failed to get rank for playlist: " + PlaylistIdToName[playlistId] + ". If this keeps happening, someone please whisper DaftPenguin on Twitch. Thanks :)";
			}

			if (!isFirst) oss << ", ";
			oss << ranks[playlistId];
			isFirst = false;
		}
	}

	return oss.str();
}