#include "Ranks.h"

#include <unordered_set>
#include <sstream>
#include <string>
#include <boost/algorithm/string.hpp>

using namespace std;

enum StreamAPIPlaylist {
	// Copied from NLHero's Session MMR and streak tracker plugin
	PLAYLIST_UNKNOWN = -1,
	PLAYLIST_DUEL = 1,
	PLAYLIST_DOUBLES = 2,
	PLAYLIST_STANDARD = 3,
	PLAYLIST_CHAOS = 4,
	PLAYLIST_RANKEDDUEL = 10,
	PLAYLIST_RANKEDDOUBLES = 11,
	PLAYLIST_RANKEDSOLOSTANDARD = 12,
	PLAYLIST_RANKEDSTANDARD = 13,
	PLAYLIST_RANKEDHOOPS = 27,
	PLAYLIST_RANKEDRUMBLE = 28,
	PLAYLIST_RANKEDDROPSHOT = 29,
	PLAYLIST_RANKEDSNOWDAY = 30,
	PLAYLIST_CASUAL = 999,
};

unordered_map<StreamAPIPlaylist, string> PlaylistIdToName({
	{ PLAYLIST_UNKNOWN, "Unknown" },
	{ PLAYLIST_DUEL, "Casual 1v1" },
	{ PLAYLIST_DOUBLES, "Casual 2v2" },
	{ PLAYLIST_STANDARD, "Casual 3v3" },
	{ PLAYLIST_CHAOS, "Casual 4v4" },
	{ PLAYLIST_RANKEDDUEL, "1v1" },
	{ PLAYLIST_RANKEDDOUBLES, "2v2" },
	{ PLAYLIST_RANKEDSTANDARD, "3v3" },
	{ PLAYLIST_RANKEDHOOPS, "Hoops" },
	{ PLAYLIST_RANKEDRUMBLE, "Rumble" },
	{ PLAYLIST_RANKEDDROPSHOT, "Dropshot" },
	{ PLAYLIST_RANKEDSNOWDAY, "Snowday" },
	{ PLAYLIST_CASUAL, "Casual" }
	});

StreamAPIPlaylist casualRetrievalPlaylist = PLAYLIST_DUEL;

unordered_set<StreamAPIPlaylist> casualPlaylists({
	PLAYLIST_DUEL,
	PLAYLIST_DOUBLES,
	PLAYLIST_STANDARD,
	PLAYLIST_CHAOS });

vector<StreamAPIPlaylist> playlistsToRetrieve({
	PLAYLIST_RANKEDDUEL,
	PLAYLIST_RANKEDDOUBLES,
	PLAYLIST_RANKEDSOLOSTANDARD,
	PLAYLIST_RANKEDSTANDARD,
	PLAYLIST_RANKEDHOOPS,
	PLAYLIST_RANKEDRUMBLE,
	PLAYLIST_RANKEDDROPSHOT,
	PLAYLIST_RANKEDSNOWDAY,
	casualRetrievalPlaylist,
	});

std::unordered_map<std::string, StreamAPIPlaylist> playlistArgToId({
	{ "casuals", PLAYLIST_CASUAL },
	{ "casual", PLAYLIST_CASUAL },
	{ "unrank", PLAYLIST_CASUAL },
	{ "unranked", PLAYLIST_CASUAL },

	{ "ones", PLAYLIST_RANKEDDUEL },
	{ "1s", PLAYLIST_RANKEDDUEL },
	{ "1v1", PLAYLIST_RANKEDDUEL },
	{ "1v1s", PLAYLIST_RANKEDDUEL },
	{ "duel", PLAYLIST_RANKEDDUEL },

	{ "twos", PLAYLIST_RANKEDDOUBLES },
	{ "2s", PLAYLIST_RANKEDDOUBLES },
	{ "2v2", PLAYLIST_RANKEDDOUBLES },
	{ "2v2s", PLAYLIST_RANKEDDOUBLES },
	{ "doubles", PLAYLIST_RANKEDDOUBLES },

	{ "threes", PLAYLIST_RANKEDSTANDARD },
	{ "3s", PLAYLIST_RANKEDSTANDARD },
	{ "3v3", PLAYLIST_RANKEDSTANDARD },
	{ "3v3s", PLAYLIST_RANKEDSTANDARD },
	{ "standard", PLAYLIST_RANKEDSTANDARD },

	{ "hoops", PLAYLIST_RANKEDHOOPS },
	{ "rumble", PLAYLIST_RANKEDRUMBLE },
	{ "dropshot", PLAYLIST_RANKEDDROPSHOT },
	{ "snowday", PLAYLIST_RANKEDSNOWDAY },
	{ "hockey", PLAYLIST_RANKEDSNOWDAY },
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
		auto mmr = round(mmrWrapper.GetPlayerMMR(UID, p));
		auto rank = mmrWrapper.GetPlayerRank(UID, p);

		auto playlistName = PlaylistIdToName[p == casualRetrievalPlaylist ? PLAYLIST_CASUAL : p];
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
		for (auto playlistId : playlistsToRetrieve) {
			auto it = ranks.find(playlistId);
			if (it != ranks.end()) {
				oss << it->second << ", ";
			}
		}
	}
	else {
		vector<StreamAPIPlaylist> playlists;
		stringstream iss(args);

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
			if (playlistId == PLAYLIST_CASUAL) {
				playlistId = casualRetrievalPlaylist;
			}

			auto rankIt = ranks.find(playlistId);
			if (rankIt == ranks.end()) {
				return "Failed to get rank for playlist: " + PlaylistIdToName[playlistId] + ". If this keeps happening, someone please whisper DaftPenguin on Twitch. Thanks :)";
			}

			oss << ranks[playlistId] << ", ";
		}
	}

	auto output = oss.str();
	return output.substr(0, output.size() - 2);
}