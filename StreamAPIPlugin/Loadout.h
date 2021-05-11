#pragma once

#define WIN32_LEAN_AND_MEAN

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "BMLoadoutLib/bmloadout.h"

#include <string>
#include <sstream>
#include <unordered_set>
#include <vector>

enum class ItemType {
	NONE,
	IN_GAME,
	BAKKESMOD,
	ALPHA_CONSOLE,
	RAINBOW_PLUGIN,
};

// TODO: Replace these by using PaintDatabase and CertifiedStatDatabase
const std::vector<std::string> CertToString = {
	"None", "Aviator", "Playmaker", "Show-Off", "Acrobat", "Tactician", "Sweeper", "Guardian", "Scorer",
	"Juggler", "Sniper", "Paragon", "Goalkeeper", "Striker", "Turtle", "Victor"
};

const std::vector<std::string> PaintToString = {
	"None", "Crimson", "Lime", "Black", "Sky Blue", "Cobalt", "Burnt Sienna", "Forest Green",
	"Purple", "Pink", "Orange", "Grey", "Titanium White", "Saffron", "Gold", "Rose Gold",
	"White Gold", "Onyx", "Platinum"
};

// Esports DB team name for G2 seems to return just "G2" and not "G2 Esports"
const std::unordered_set<std::string> TeamAppendEsports = { "G2" };

struct RGBColor {
	uint8_t r;
	uint8_t g;
	uint8_t b;

	RGBColor(uint8_t r, uint8_t g, uint8_t b) {
		this->r = r;
		this->g = g;
		this->b = b;
	}
};

struct LoadoutItem {
	ItemType type = ItemType::NONE;
	int productId;
	unsigned long long instanceId;
	uint8_t paintId;
	uint8_t teamId;
	std::string itemString;

	void clear();
	void fromItem(unsigned long long id, bool isOnline, std::shared_ptr<GameWrapper> gw);
	void fromBMItem(BM::Item item, std::shared_ptr<GameWrapper> gw);
	void fromAlphaConsolePlugin(std::shared_ptr<CVarManagerWrapper> cvarManager, int teamNum, std::string itemType);
	void addTeamId(uint8_t teamId, std::shared_ptr<GameWrapper> gw);
	std::string toString();
	void handleAttributes(ArrayWrapper<ProductAttributeWrapper> attrs, std::shared_ptr<GameWrapper> gw);
};

struct PaintItem {
	ItemType type = ItemType::NONE;
	int paintId;
	uint8_t r;
	uint8_t g;
	uint8_t b;

	bool reverse;
	float saturation;
	float speed;
	float value;

	std::string itemString;

	void clear();
	void fromPaintId(int paintId, bool isPrimary, const std::vector<RGBColor>& colorSet);
	void fromBMPaint(BM::RGB paint);
	void fromRainbowPlugin(std::shared_ptr<CVarManagerWrapper> cvarManager, bool isPrimary);
	std::string toString(bool isPrimary);
};

class Loadout {
public:
	void clear();
	void load(int teamNum, std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw);
	void assignItemToSlot(unsigned long long id, bool isOnline, std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw);

	void fromLoadoutWrapper(int teamNum, std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw);
	void fromBakkesMod(int teamNum, std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw);
	void fromPlugins(int teamNum, std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw);
	void cleanUpStandardItems();

	std::string toBMCode();
	std::string toString();
	std::string getItemString(std::string itemType, std::string outputSeparator, bool showSlotName, bool showBMCode);

private:
	PaintItem primaryPaint;
	PaintItem accentPaint;
	LoadoutItem body;
	LoadoutItem decal;
	LoadoutItem wheels;
	LoadoutItem boost;
	LoadoutItem antenna;
	LoadoutItem topper;
	LoadoutItem primaryFinish;
	LoadoutItem accentFinish;
	LoadoutItem engineAudio;
	LoadoutItem trail;
	LoadoutItem goalExplosion;
};