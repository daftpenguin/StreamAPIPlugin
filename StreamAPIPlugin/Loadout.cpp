#include "Loadout.h"
#include "StreamAPIPlugin.h"
#include "BMLoadoutLib/helper_classes.h"

#include <bakkesmod/wrappers/PluginManagerWrapper.h>
#include <iomanip>


#define APPEND_STRING_WITH_ITEM(oss, separator, showSlotName, slotName, itemString) { \
	oss << separator; \
	if (showSlotName) oss << slotName << ": "; \
	oss << itemString; \
}

#define ASSIGN_ITEM_TO_SLOT(id, loadoutSlot, isOnline, gameWrapper) \
	if (id != 0 && (isOnline || loadoutSlot.productId == 0)) \
		loadoutSlot.fromItem(id, isOnline, gameWrapper);

using namespace std;

const vector<RGBColor> team0Colors = {
	RGBColor(80, 127, 57),
	RGBColor(57, 127, 63),
	RGBColor(57, 127, 100),
	RGBColor(57, 125, 127),
	RGBColor(57, 107, 127),
	RGBColor(57, 93, 127),
	RGBColor(57, 79, 127),
	RGBColor(57, 66, 127),
	RGBColor(76, 57, 127),
	RGBColor(81, 57, 127),
	RGBColor(101, 178, 62),
	RGBColor(62, 178, 72),
	RGBColor(62, 178, 134),
	RGBColor(62, 174, 178),
	RGBColor(62, 145, 178),
	RGBColor(62, 122, 178),
	RGBColor(62, 99, 178),
	RGBColor(62, 77, 178),
	RGBColor(93, 62, 178),
	RGBColor(103, 62, 178),
	RGBColor(114, 229, 57),
	RGBColor(57, 229, 71),
	RGBColor(57, 229, 163),
	RGBColor(57, 223, 229),
	RGBColor(57, 180, 229),
	RGBColor(57, 146, 229),
	RGBColor(57, 111, 229),
	RGBColor(57, 80, 229),
	RGBColor(103, 57, 229),
	RGBColor(117, 57, 229),
	RGBColor(92, 252, 12),
	RGBColor(12, 252, 32),
	RGBColor(12, 252, 160),
	RGBColor(12, 244, 252),
	RGBColor(12, 184, 252),
	RGBColor(12, 136, 252),
	RGBColor(12, 88, 252),
	RGBColor(12, 44, 252),
	RGBColor(76, 12, 252),
	RGBColor(96, 12, 252),
	RGBColor(74, 204, 10),
	RGBColor(10, 204, 26),
	RGBColor(10, 204, 129),
	RGBColor(10, 197, 204),
	RGBColor(10, 149, 204),
	RGBColor(10, 110, 204),
	RGBColor(10, 71, 204),
	RGBColor(10, 36, 204),
	RGBColor(61, 10, 204),
	RGBColor(78, 10, 204),
	RGBColor(60, 165, 8),
	RGBColor(8, 165, 21),
	RGBColor(8, 165, 105),
	RGBColor(8, 160, 165),
	RGBColor(8, 121, 165),
	RGBColor(8, 89, 165),
	RGBColor(8, 58, 165),
	RGBColor(8, 29, 165),
	RGBColor(50, 8, 165),
	RGBColor(63, 8, 165),
	RGBColor(46, 127, 6),
	RGBColor(6, 127, 16),
	RGBColor(6, 127, 81),
	RGBColor(6, 123, 127),
	RGBColor(6, 93, 127),
	RGBColor(6, 68, 127),
	RGBColor(6, 44, 127),
	RGBColor(6, 22, 127),
	RGBColor(38, 6, 127),
	RGBColor(48, 6, 127),
};

const vector<RGBColor> team1Colors = {
	RGBColor(127, 127, 57),
	RGBColor(127, 112, 57),
	RGBColor(127, 99, 57),
	RGBColor(127, 90, 57),
	RGBColor(127, 84, 57),
	RGBColor(127, 78, 57),
	RGBColor(127, 71, 57),
	RGBColor(127, 57, 57),
	RGBColor(127, 57, 81),
	RGBColor(127, 57, 92),
	RGBColor(178, 178, 62),
	RGBColor(178, 153, 62),
	RGBColor(178, 132, 62),
	RGBColor(178, 116, 62),
	RGBColor(178, 106, 62),
	RGBColor(178, 97, 62),
	RGBColor(178, 85, 62),
	RGBColor(178, 62, 62),
	RGBColor(178, 62, 103),
	RGBColor(178, 62, 120),
	RGBColor(229, 229, 57),
	RGBColor(229, 192, 57),
	RGBColor(229, 160, 57),
	RGBColor(229, 137, 57),
	RGBColor(229, 123, 57),
	RGBColor(229, 109, 57),
	RGBColor(229, 91, 57),
	RGBColor(229, 57, 57),
	RGBColor(229, 57, 117),
	RGBColor(229, 57, 143),
	RGBColor(252, 252, 12),
	RGBColor(252, 200, 12),
	RGBColor(252, 156, 12),
	RGBColor(252, 124, 12),
	RGBColor(252, 104, 12),
	RGBColor(252, 84, 12),
	RGBColor(252, 60, 12),
	RGBColor(252, 12, 12),
	RGBColor(252, 12, 96),
	RGBColor(252, 12, 132),
	RGBColor(204, 204, 10),
	RGBColor(204, 162, 10),
	RGBColor(204, 126, 10),
	RGBColor(204, 100, 10),
	RGBColor(204, 84, 10),
	RGBColor(204, 68, 10),
	RGBColor(204, 48, 10),
	RGBColor(204, 10, 10),
	RGBColor(204, 10, 78),
	RGBColor(204, 10, 107),
	RGBColor(165, 165, 8),
	RGBColor(165, 131, 8),
	RGBColor(165, 102, 8),
	RGBColor(165, 81, 8),
	RGBColor(165, 68, 8),
	RGBColor(165, 55, 8),
	RGBColor(165, 39, 8),
	RGBColor(165, 8, 8),
	RGBColor(165, 8, 63),
	RGBColor(165, 8, 87),
	RGBColor(127, 127, 6),
	RGBColor(127, 101, 6),
	RGBColor(127, 79, 6),
	RGBColor(127, 62, 6),
	RGBColor(127, 52, 6),
	RGBColor(127, 42, 6),
	RGBColor(127, 30, 6),
	RGBColor(127, 6, 6),
	RGBColor(127, 6, 48),
	RGBColor(127, 6, 66),
};

const vector<RGBColor> customColors = {
	RGBColor(229, 229, 229),
	RGBColor(255, 127, 127),
	RGBColor(255, 159, 127),
	RGBColor(255, 207, 127),
	RGBColor(239, 255, 127),
	RGBColor(175, 255, 127),
	RGBColor(127, 255, 127),
	RGBColor(127, 255, 178),
	RGBColor(127, 233, 255),
	RGBColor(127, 176, 255),
	RGBColor(127, 136, 255),
	RGBColor(174, 127, 255),
	RGBColor(229, 127, 255),
	RGBColor(255, 127, 208),
	RGBColor(255, 127, 148),
	RGBColor(191, 191, 191),
	RGBColor(255, 89, 89),
	RGBColor(255, 130, 89),
	RGBColor(255, 192, 89),
	RGBColor(234, 255, 89),
	RGBColor(151, 255, 89),
	RGBColor(89, 255, 89),
	RGBColor(89, 255, 155),
	RGBColor(89, 227, 255),
	RGBColor(89, 152, 255),
	RGBColor(89, 100, 255),
	RGBColor(150, 89, 255),
	RGBColor(221, 89, 255),
	RGBColor(255, 89, 194),
	RGBColor(255, 89, 116),
	RGBColor(153, 153, 153),
	RGBColor(255, 50, 50),
	RGBColor(255, 101, 50),
	RGBColor(255, 178, 50),
	RGBColor(229, 255, 50),
	RGBColor(127, 255, 50),
	RGBColor(50, 255, 50),
	RGBColor(50, 255, 132),
	RGBColor(50, 220, 255),
	RGBColor(50, 129, 255),
	RGBColor(50, 64, 255),
	RGBColor(125, 50, 255),
	RGBColor(214, 50, 255),
	RGBColor(255, 50, 180),
	RGBColor(255, 50, 85),
	RGBColor(102, 102, 102),
	RGBColor(255, 0, 0),
	RGBColor(255, 63, 0),
	RGBColor(255, 159, 0),
	RGBColor(223, 255, 0),
	RGBColor(95, 255, 0),
	RGBColor(0, 255, 0),
	RGBColor(0, 255, 102),
	RGBColor(0, 212, 255),
	RGBColor(0, 97, 255),
	RGBColor(0, 17, 255),
	RGBColor(93, 0, 255),
	RGBColor(204, 0, 255),
	RGBColor(255, 0, 161),
	RGBColor(255, 0, 42),
	RGBColor(63, 63, 63),
	RGBColor(178, 0, 0),
	RGBColor(178, 44, 0),
	RGBColor(178, 111, 0),
	RGBColor(156, 178, 0),
	RGBColor(66, 178, 0),
	RGBColor(0, 178, 0),
	RGBColor(0, 178, 71),
	RGBColor(0, 148, 178),
	RGBColor(0, 68, 178),
	RGBColor(0, 11, 178),
	RGBColor(65, 0, 178),
	RGBColor(142, 0, 178),
	RGBColor(178, 0, 113),
	RGBColor(178, 0, 29),
	RGBColor(38, 38, 38),
	RGBColor(102, 0, 0),
	RGBColor(102, 25, 0),
	RGBColor(102, 63, 0),
	RGBColor(89, 102, 0),
	RGBColor(38, 102, 0),
	RGBColor(0, 102, 0),
	RGBColor(0, 102, 40),
	RGBColor(0, 84, 102),
	RGBColor(0, 39, 102),
	RGBColor(0, 6, 102),
	RGBColor(37, 0, 102),
	RGBColor(81, 0, 102),
	RGBColor(102, 0, 64),
	RGBColor(102, 0, 17),
	RGBColor(5, 5, 5),
	RGBColor(51, 0, 0),
	RGBColor(51, 12, 0),
	RGBColor(51, 31, 0),
	RGBColor(44, 51, 0),
	RGBColor(19, 51, 0),
	RGBColor(0, 51, 0),
	RGBColor(0, 51, 20),
	RGBColor(0, 42, 51),
	RGBColor(0, 19, 51),
	RGBColor(0, 3, 51),
	RGBColor(18, 0, 51),
	RGBColor(40, 0, 51),
	RGBColor(51, 0, 32),
	RGBColor(51, 0, 8),
};

std::string productNameFromID(int id, std::shared_ptr<GameWrapper> gw)
{
	if (id == 0) return "None";
	return gw->GetItemsWrapper().GetProduct(id).GetAsciiLabel().ToString();
}

std::string paintNameFromPaintID(int paintId, bool isPrimary)
{
	int rowLength = isPrimary ? 10 : 15;
	return "Row: " + to_string((paintId / rowLength) + 1) + ", Col: " + to_string((paintId % rowLength) + 1);
}

std::map<uint8_t, BM::Item> read_items_from_buffer(BitBinaryReader<unsigned char>& reader)
{
	std::map<uint8_t, BM::Item> items;
	int itemsSize = reader.ReadBits<int>(4); //Read the length of the item array
	for (int i = 0; i < itemsSize; i++)
	{
		BM::Item option;
		int slotIndex = reader.ReadBits<int>(5); //Read slot of item
		int productId = reader.ReadBits<int>(13); //Read product ID
		bool isPaintable = reader.ReadBool(); //Read whether item is paintable or not

		if (isPaintable)
		{
			int paintID = reader.ReadBits<int>(6); //Read paint index
			option.paint_index = paintID;
		}
		option.product_id = productId;
		option.slot_index = slotIndex;
		items.insert_or_assign(slotIndex, option); //Add item to loadout at its selected slot
	}
	return items;
}

BM::RGB read_colors_from_buffer(BitBinaryReader<unsigned char>& reader)
{
	BM::RGB col;
	col.r = reader.ReadBits<uint8_t>(8);
	col.g = reader.ReadBits<uint8_t>(8);
	col.b = reader.ReadBits<uint8_t>(8);
	return col;
}

void Loadout::fromBakkesMod(int teamNum, std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw)
{
	bool boostSet = false;

	auto itemModEnabled = cv->getCvar("cl_itemmod_enabled");
	auto itemModCode = cv->getCvar("cl_itemmod_code");

	if (!itemModEnabled.IsNull() && !itemModCode.IsNull() && itemModEnabled.getBoolValue()) {
		
		string code = itemModCode.getStringValue();

		BitBinaryReader<unsigned char> reader(code);
		BM::BMLoadout bmloadout;

		bmloadout.header.version = reader.ReadBits<uint8_t>(6);
		bmloadout.header.code_size = reader.ReadBits<uint16_t>(10);
		bmloadout.header.crc = reader.ReadBits<uint8_t>(8);

		/*
		Calculate whether code_size converted to base64 is actually equal to the given input string
		Mostly done so we don't end up with invalid buffers, but this step is not required.
		*/
		int stringSizeCalc = ((int)ceil((4 * (float)bmloadout.header.code_size / 3)) + 3) & ~3;
		int stringSize = code.size();

		if (abs(stringSizeCalc - stringSize) > 6) //Diff may be at most 4 (?) because of base64 padding, but we check > 6 because IDK
		{
			//Given input string is probably invalid, handle
			_globalCvarManager->log("Invalid cl_itemmod_code detected");
			return;
		}

		/*
		Verify CRC, aka check if user didn't mess with the input string to create invalid loadouts
		*/
		if (!reader.VerifyCRC(bmloadout.header.crc, 3, bmloadout.header.code_size))
		{
			//User changed characters in input string, items isn't valid! handle here
			_globalCvarManager->log("Invalid input string! CRC check failed");
			return;
		}

		bmloadout.body.blue_is_orange = reader.ReadBool(); //Read single bit indicating whether blue = orange
		bmloadout.body.blue_loadout = read_items_from_buffer(reader); //Read loadout

		bmloadout.body.blueColor.should_override = reader.ReadBool(); //Read whether custom colors is on
		if (bmloadout.body.blueColor.should_override) {
			bmloadout.body.blueColor.primary_colors = read_colors_from_buffer(reader);
			bmloadout.body.blueColor.secondary_colors = read_colors_from_buffer(reader);

			if (bmloadout.header.version > 2) {
				bmloadout.body.blue_wheel_team_id = reader.ReadBits<int>(6);
			}
		}

		if (bmloadout.body.blue_is_orange) //User has same loadout for both teams
		{
			bmloadout.body.orange_loadout = bmloadout.body.blue_loadout;
			bmloadout.body.orange_wheel_team_id = bmloadout.body.blue_wheel_team_id;
		}
		else {
			bmloadout.body.orange_loadout = read_items_from_buffer(reader);
			bmloadout.body.orangeColor.should_override = reader.ReadBool(); //Read whether custom colors is on
			if (bmloadout.body.blueColor.should_override) {
				bmloadout.body.orangeColor.primary_colors = read_colors_from_buffer(reader);
				bmloadout.body.orangeColor.secondary_colors = read_colors_from_buffer(reader);
			}

			if (bmloadout.header.version > 2) {
				bmloadout.body.orange_wheel_team_id = reader.ReadBits<int>(6);
			}
		}

		// Loadout parsed from code, now apply to this

		auto items = (teamNum == 0 ? bmloadout.body.blue_loadout : bmloadout.body.orange_loadout);

		this->body.fromBMItem(items[BM::SLOT_BODY], gw);
		this->decal.fromBMItem(items[BM::SLOT_SKIN], gw);
		this->wheels.fromBMItem(items[BM::SLOT_WHEELS], gw);
		this->boost.fromBMItem(items[BM::SLOT_BOOST], gw);
		this->antenna.fromBMItem(items[BM::SLOT_ANTENNA], gw);
		this->topper.fromBMItem(items[BM::SLOT_HAT], gw);
		this->primaryFinish.fromBMItem(items[BM::SLOT_PAINTFINISH], gw);
		this->accentFinish.fromBMItem(items[BM::SLOT_PAINTFINISH_SECONDARY], gw);
		this->engineAudio.fromBMItem(items[BM::SLOT_ENGINE_AUDIO], gw);
		this->trail.fromBMItem(items[BM::SLOT_SUPERSONIC_TRAIL], gw);
		this->goalExplosion.fromBMItem(items[BM::SLOT_GOALEXPLOSION], gw);

		auto teamWheelId = (teamNum == 0 ? bmloadout.body.blue_wheel_team_id : bmloadout.body.orange_wheel_team_id);
		if (teamWheelId > 0) this->wheels.addTeamId(teamWheelId, gw);

		this->primaryPaint.fromBMPaint((teamNum == 0 ? bmloadout.body.blueColor.primary_colors : bmloadout.body.orangeColor.primary_colors));
		this->accentPaint.fromBMPaint((teamNum == 0 ? bmloadout.body.blueColor.secondary_colors : bmloadout.body.orangeColor.secondary_colors));

		boostSet = items[BM::SLOT_BOOST].product_id != 0;
	}

	// TODO: Doesn't alpha console also have painted alpha boost?
	if (!boostSet) {
		auto alphaBoostCvar = cv->getCvar("cl_alphaboost");
		if (!alphaBoostCvar.IsNull() && alphaBoostCvar.getBoolValue()) {
			this->boost.fromBMItem(BM::Item{ 3, 32, 0 }, gw);
		}
	}
}

void Loadout::fromPlugins(int teamNum, std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw)
{
	bool rainbowPluginLoaded = false;
	bool alphaConsoleLoaded = false;

	auto loadedPlugins = gw->GetPluginManager().GetLoadedPlugins();
	for (auto it = loadedPlugins->begin(); it != loadedPlugins->end(); ++it) {
		auto pName = string((*it)->_details->pluginName);
		if (pName.compare("AlphaConsole Plugin") == 0) {
			alphaConsoleLoaded = true;
		}
		else if (pName.compare("Rainbow car") == 0) {
			rainbowPluginLoaded = true;
		}
	}

	if (rainbowPluginLoaded) {
		this->primaryPaint.fromRainbowPlugin(cv, true);
		this->accentPaint.fromRainbowPlugin(cv, false);
	}

	if (alphaConsoleLoaded) {
		this->antenna.fromAlphaConsolePlugin(cv, teamNum, "antenna");
		this->boost.fromAlphaConsolePlugin(cv, teamNum, "boost");
		this->wheels.fromAlphaConsolePlugin(cv, teamNum, "wheel");
		this->decal.fromAlphaConsolePlugin(cv, teamNum, "decal");
		this->topper.fromAlphaConsolePlugin(cv, teamNum, "topper");
	}
}

std::string Loadout::toBMCode()
{
	BM::BMLoadout bmLoadout;

	// TODO: Store orange and blue sides and update this to handle both, or do a loadout for each, but combine them both here.

	bmLoadout.body.blue_is_orange = true; // We only have one side of the preset for now, so just apply it to both sides.
	bmLoadout.body.blueColor.should_override = true; // We can't set the in game paint color, so we'll use the RGB for all cases.

	// Do paints
	if (primaryPaint.type == ItemType::BAKKESMOD || primaryPaint.type == ItemType::IN_GAME) {
		bmLoadout.body.blueColor.primary_colors = { primaryPaint.r, primaryPaint.g, primaryPaint.b };
		bmLoadout.body.blueColor.secondary_colors = { accentPaint.r, accentPaint.g, accentPaint.b };
	}

	// Now add the items (slot_index, product_id, paint_index)
	bmLoadout.body.blue_loadout.insert_or_assign(
		BM::SLOT_BODY,
		BM::Item{ BM::SLOT_BODY, (uint16_t)this->body.productId, this->body.paintId });
	bmLoadout.body.blue_loadout.insert_or_assign(
		BM::SLOT_SKIN,
		BM::Item{ BM::SLOT_SKIN, (uint16_t)this->decal.productId, this->decal.paintId });
	bmLoadout.body.blue_loadout.insert_or_assign(
		BM::SLOT_WHEELS,
		BM::Item{ BM::SLOT_WHEELS, (uint16_t)this->wheels.productId, this->wheels.paintId });
	bmLoadout.body.blue_loadout.insert_or_assign(
		BM::SLOT_BOOST,
		BM::Item{ BM::SLOT_BOOST, (uint16_t)this->boost.productId, this->boost.paintId });
	bmLoadout.body.blue_loadout.insert_or_assign(
		BM::SLOT_ANTENNA,
		BM::Item{ BM::SLOT_ANTENNA, (uint16_t)this->antenna.productId, this->antenna.paintId });
	bmLoadout.body.blue_loadout.insert_or_assign(
		BM::SLOT_HAT,
		BM::Item{ BM::SLOT_HAT, (uint16_t)this->topper.productId, this->topper.paintId });
	bmLoadout.body.blue_loadout.insert_or_assign(
		BM::SLOT_SUPERSONIC_TRAIL,
		BM::Item{ BM::SLOT_SUPERSONIC_TRAIL, (uint16_t)this->trail.productId, this->trail.paintId });
	bmLoadout.body.blue_loadout.insert_or_assign(
		BM::SLOT_GOALEXPLOSION,
		BM::Item{ BM::SLOT_GOALEXPLOSION, (uint16_t)this->goalExplosion.productId, this->goalExplosion.paintId });
	bmLoadout.body.blue_loadout.insert_or_assign(
		BM::SLOT_PAINTFINISH,
		BM::Item{ BM::SLOT_PAINTFINISH, (uint16_t)this->primaryFinish.productId, this->primaryFinish.paintId });
	bmLoadout.body.blue_loadout.insert_or_assign(
		BM::SLOT_PAINTFINISH_SECONDARY,
		BM::Item{ BM::SLOT_PAINTFINISH_SECONDARY, (uint16_t)this->accentFinish.productId, this->accentFinish.paintId });
	bmLoadout.body.blue_wheel_team_id = this->wheels.teamId;

	return save(bmLoadout);
}

std::string Loadout::toString()
{
	stringstream oss;
	oss << "Loadout:\n"
		<< "\tBody: " << body.toString() << endl
		<< "\tPrimary: Paint: " << primaryPaint.toString(true) << ", Finish: " << primaryFinish.toString() << endl;
	if (accentFinish.toString().compare("None") != 0 || decal.type == ItemType::ALPHA_CONSOLE) {
		oss << "\tAccent: Paint: " << accentPaint.toString(false) << ", Finish: " << accentFinish.toString() << endl;
	}
	oss << "\tDecal: " << decal.toString() << endl
		<< "\tWheels: " << wheels.toString() << endl
		<< "\tBoost: " << boost.toString() << endl
		<< "\tAntenna: " << antenna.toString() << endl
		<< "\tTopper: " << topper.toString() << endl
		<< "\tTrail: " << trail.toString() << endl
		<< "\tGoal Explosion: " << goalExplosion.toString() << endl
		<< "BakkesMod code: " << toBMCode() << endl;
	return oss.str();
}

std::string Loadout::getItemString(std::string itemType, std::string outputSeparator, bool showSlotName, bool showBMCode)
{
	if (itemType.compare("json") == 0) {
		stringstream oss;
		oss << "{\"body\":" << quoted(body.toString()) << ","
			<< "\"primary\":{\"paint\":" << quoted(primaryPaint.toString(true)) << ",\"finish\":" << quoted(primaryFinish.toString()) << "},";
		if (accentFinish.toString().compare("None") != 0 || decal.type == ItemType::ALPHA_CONSOLE) {
			oss << "\"accent\":{\"paint\":" << quoted(accentPaint.toString(false)) << ",\"finish\":" << quoted(accentFinish.toString()) << "},";
		}
		oss << "\"decal\":" << quoted(decal.toString()) << ","
			<< "\"wheels\":" << quoted(wheels.toString()) << ","
			<< "\"boost\":" << quoted(boost.toString()) << ","
			<< "\"trail\":" << quoted(trail.toString()) << ","
			<< "\"goal explosion\":" << quoted(goalExplosion.toString()) << ","
			<< "\"engine audio\":" << quoted(engineAudio.toString()) << ","
			<< "\"antenna\":" << quoted(antenna.toString()) << ","
			<< "\"topper\":" << quoted(topper.toString()) << ","
			<< "\"bm code\":" << quoted(toBMCode()) << "}";
		return oss.str();
	}

	if (itemType.compare("body") == 0 || itemType.compare("car") == 0) {
		return body.toString();
	}
	if (itemType.compare("decal") == 0 || itemType.compare("skin") == 0) {
		return decal.toString();
	}
	if (itemType.compare("wheels") == 0) {
		return wheels.toString();
	}
	if (itemType.compare("boost") == 0) {
		return boost.toString();
	}
	if (itemType.compare("antenna") == 0) {
		return antenna.toString();
	}
	if (itemType.compare("topper") == 0) {
		return topper.toString();
	}
	if (itemType.compare("engine") == 0) {
		return engineAudio.toString();
	}
	if (itemType.compare("trail") == 0) {
		return trail.toString();
	}
	if (itemType.compare("goal explosion") == 0 || itemType.compare("ge") == 0) {
		return goalExplosion.toString();
	}
	if (itemType.empty()) {
		stringstream oss;
		APPEND_STRING_WITH_ITEM(oss, "", showSlotName, "body", body.toString());
		APPEND_STRING_WITH_ITEM(oss, outputSeparator, showSlotName, "primary", primaryPaint.toString(true));
		APPEND_STRING_WITH_ITEM(oss, " ", false, "", primaryFinish.toString());
		APPEND_STRING_WITH_ITEM(oss, outputSeparator, showSlotName, "accent", accentPaint.toString(false));
		APPEND_STRING_WITH_ITEM(oss, " ", false, "", accentFinish.toString());
		APPEND_STRING_WITH_ITEM(oss, outputSeparator, showSlotName, "decal", decal.toString());
		APPEND_STRING_WITH_ITEM(oss, outputSeparator, showSlotName, "wheels", wheels.toString());
		APPEND_STRING_WITH_ITEM(oss, outputSeparator, showSlotName, "boost", boost.toString());
		APPEND_STRING_WITH_ITEM(oss, outputSeparator, showSlotName, "trail", trail.toString());
		APPEND_STRING_WITH_ITEM(oss, outputSeparator, showSlotName, "ge", goalExplosion.toString());
		if (engineAudio.productId != 0)
			APPEND_STRING_WITH_ITEM(oss, outputSeparator, showSlotName, "engine", engineAudio.toString());
		if (antenna.productId != 0)
			APPEND_STRING_WITH_ITEM(oss, outputSeparator, showSlotName, "antenna", antenna.toString());
		if (topper.productId != 0)
			APPEND_STRING_WITH_ITEM(oss, outputSeparator, showSlotName, "topper", topper.toString());
		if (showBMCode)
			APPEND_STRING_WITH_ITEM(oss, outputSeparator, showSlotName, "bm code", toBMCode());
		return oss.str();
	}

	return "unknown item type requested. options: body, decal, wheels, boost, antenna, topper, engine, trail, goal explosion, or ge";
}

void LoadoutItem::clear()
{
	this->type = ItemType::NONE;
	this->instanceId = 0;
	this->productId = 0;
	this->paintId = 0;
	this->teamId = 0;
	this->itemString = "None";
}

void LoadoutItem::handleAttributes(ArrayWrapper<ProductAttributeWrapper> attrs, std::shared_ptr<GameWrapper> gw)
{
	stringstream ss;
	if (attrs.IsNull()) {
		return;
	}
	for (int i = 0; i < attrs.Count(); i++) {
		auto attr = attrs.Get(i);
		if (attr.GetAttributeType().compare("ProductAttribute_Certified_TA") == 0) {
			auto statName = gw->GetItemsWrapper().GetCertifiedStatDB().GetStatName(ProductAttribute_CertifiedWrapper(attr.memory_address).GetStatId());
			ss << " " << statName << " Certified";
		}
		else if (attr.GetAttributeType().compare("ProductAttribute_Painted_TA") == 0) {
			this->paintId = ProductAttribute_PaintedWrapper(attr.memory_address).GetPaintID();
			this->itemString = PaintToString[this->paintId] + " " + this->itemString;
		}
		else if (attr.GetAttributeType().compare("ProductAttribute_SpecialEdition_TA") == 0) {
			auto specEdName = gw->GetItemsWrapper().GetSpecialEditionDB().GetSpecialEditionName(ProductAttribute_SpecialEditionWrapper(attr.memory_address).GetEditionID());
			if (specEdName.find("Edition_") == 0) {
				specEdName = specEdName.substr(8);
			}
			ss << " " << specEdName;
		}
		else if (attr.GetAttributeType().compare("ProductAttribute_TeamEdition_TA") == 0) {
			this->teamId = ProductAttribute_TeamEditionWrapper(attr.memory_address).GetId();
			auto teamEdName = gw->GetItemsWrapper().GetEsportTeamDB().GetName(this->teamId);
			if (TeamAppendEsports.find(teamEdName) != TeamAppendEsports.end()) {
				teamEdName += " Esports";
			}
			ss << " " << teamEdName;
		}
	}
	this->itemString += ss.str();
}

void LoadoutItem::fromItem(unsigned long long id, bool isOnline, std::shared_ptr<GameWrapper> gw)
{
	if (id == 0) return;
	auto iw = gw->GetItemsWrapper();
	if (iw.IsNull()) return;

	// I really don't know diff between products and online products, so this might be a dumb way of doing things...
	if (isOnline) {
		auto onlineProduct = iw.GetOnlineProduct(id);
		if (onlineProduct.IsNull()) {
			isOnline = false;
		} else {
			this->productId = onlineProduct.GetProductID();
			this->itemString = onlineProduct.GetLongLabel().ToString();
			handleAttributes(onlineProduct.GetAttributes(), gw);
		}
	}

	if (!isOnline) {
		auto product = iw.GetProduct(id);
		if (product.IsNull()) return;

		this->productId = id;
		this->itemString = product.GetLongLabel().ToString();
		handleAttributes(product.GetAttributes(), gw);
	}

	this->instanceId = id;
	this->type = ItemType::BAKKESMOD;
}

void LoadoutItem::fromBMItem(BM::Item item, std::shared_ptr<GameWrapper> gw)
{
	if (item.product_id == 0) return;

	auto product = gw->GetItemsWrapper().GetProduct(item.product_id);
	if (product.IsNull()) {
		return;
	}

	this->instanceId = item.product_id;
	this->paintId = item.paint_index;
	this->type = ItemType::BAKKESMOD;

	stringstream ss;
	
	bool labelSet = false;
	auto label = product.GetLongLabel();
	if (label.IsNull()) {
		auto label = product.GetAsciiLabel();
		if (!label.IsNull()) {
			ss << label.ToString();
			labelSet = true;
		}
		else {
			ss << "None";
		}
	}
	else {
		ss << label.ToString();
		labelSet = true;
	}

	if (labelSet) {
		if (paintId != 0) {
			ss << " " << PaintToString[paintId];
		}
	}

	this->itemString = ss.str();

	if (labelSet) {
		handleAttributes(OnlineProductWrapper(product.memory_address).GetAttributes(), gw);
	}
}

void LoadoutItem::fromAlphaConsolePlugin(std::shared_ptr<CVarManagerWrapper> cvarManager, int teamNum, std::string itemType)
{
	string teamStr = teamNum == 0 ? "blue" : "orange";

	if (itemType.compare("wheel") == 0 || itemType.compare("antenna") == 0 || itemType.compare("decal") || itemType.compare("topper")) {
		// Get the cvar values
		CVarWrapper cvarTexture = cvarManager->getCvar("acplugin_" + itemType + "texture_selectedtexture_" + teamStr);
		if (cvarTexture.IsNull()) return;
		auto texture = cvarTexture.getStringValue();

		// Get reactive cvars if wheel
		bool reactive = false;
		string reactiveMultiplier;
		if (itemType.compare("wheel") == 0) {
			auto cvarReactive = cvarManager->getCvar("acplugin_reactivewheels_enabled");
			if (!cvarReactive.IsNull() && cvarReactive.getBoolValue()) {
				auto cvarMulti = cvarManager->getCvar("acplugin_reactivewheels_brightnessmultiplier");
				if (!cvarMulti.IsNull()) {
					reactive = true;
					reactiveMultiplier = cvarMulti.getStringValue();
				}
			}
		}

		if (texture.compare("Default") == 0) {
			if (reactive) {
				this->itemString += " (AlphaConsole reactive: " + reactiveMultiplier + "x)";
			}
			return;
		}

		this->type = ItemType::ALPHA_CONSOLE;
		this->itemString = "AlphaConsole: " + texture;
		if (reactive) {
			this->itemString += " (Reactive: " + reactiveMultiplier + "x)";
		}
	}
}

void LoadoutItem::addTeamId(uint8_t teamId, std::shared_ptr<GameWrapper> gw)
{
	this->teamId = teamId;
	auto teamEdName = gw->GetItemsWrapper().GetEsportTeamDB().GetName(this->teamId);
	if (TeamAppendEsports.find(teamEdName) != TeamAppendEsports.end()) {
		teamEdName += " Esports";
	}
	this->itemString += " " + teamEdName;
}

std::string LoadoutItem::toString()
{
	return this->itemString;
}

void PaintItem::clear()
{
	this->type = ItemType::NONE;
	this->paintId = 0;
	this->itemString = "";
}

void PaintItem::fromPaintId(int paintId, bool isPrimary, const vector<RGBColor>& colorSet)
{
	int rowLength = isPrimary ? 10 : 15;
	if (paintId < colorSet.size()) {
		auto color = colorSet.at(paintId);

		this->type = ItemType::IN_GAME;
		this->paintId = paintId;
		this->r = color.r;
		this->g = color.g;
		this->b = color.b;
		this->itemString = "Row: " + to_string((paintId / rowLength) + 1) + ", Col: " + to_string((paintId % rowLength) + 1);
	}
}

void PaintItem::fromBMPaint(BM::RGB paint)
{
	this->r = paint.r;
	this->g = paint.g;
	this->b = paint.b;
	this->type = ItemType::BAKKESMOD;
	this->itemString = "BM: (R: " + to_string(this->r) + ", G: " + to_string(this->g) + ", B: " + to_string(this->b) + ")";
}

void PaintItem::fromRainbowPlugin(std::shared_ptr<CVarManagerWrapper> cvarManager, bool isPrimary)
{
	string pType = isPrimary ? "Primary" : "Secondary";

	auto cvarEnabled = cvarManager->getCvar("RainbowCar_" + pType + "_Enable");
	if (cvarEnabled.IsNull() || cvarEnabled.getBoolValue() == false) {
		return;
	}

	auto cvarReverse = cvarManager->getCvar("RainbowCar_" + pType + "_Reverse");
	auto cvarSaturation = cvarManager->getCvar("RainbowCar_" + pType + "_Saturation");
	auto cvarSpeed = cvarManager->getCvar("RainbowCar_" + pType + "_Speed");
	auto cvarValue = cvarManager->getCvar("RainbowCar_" + pType + "_Value");

	if (cvarReverse.IsNull() || cvarSaturation.IsNull() || cvarSpeed.IsNull() || cvarValue.IsNull()) {
		return;
	}

	this->type = ItemType::RAINBOW_PLUGIN;
	this->reverse = cvarReverse.getBoolValue();
	this->saturation = cvarSaturation.getFloatValue();
	this->speed = cvarSpeed.getFloatValue();
	this->value = cvarValue.getFloatValue();

	stringstream out;
	out.precision(3);
	out << "Rainbow: (";
	if (this->reverse) out << "Reversed, ";
	out << "Sat: " << this->saturation
		<< ", Spd: " << this->speed
		<< ", Val: " << this->value
		<< ")";

	this->itemString = out.str();
}

std::string PaintItem::toString(bool isPrimary)
{
	return this->itemString;
}

void Loadout::assignItemToSlot(unsigned long long id, bool isOnline, std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw)
{
	// Hack for now to fit BakkesMod loadout support into existing code
	if (id == 0) return;

	auto iw = gw->GetItemsWrapper();
	if (iw.IsNull()) return;

	// I really don't know diff between products and online products, so this might be a dumb way of doing things...
	ProductWrapper* pw = nullptr;
	if (isOnline) {
		auto onlineProduct = iw.GetOnlineProduct(id);
		if (!onlineProduct.IsNull()) {
			auto product = onlineProduct.GetProduct();
			pw = &product;
		}		
	}
	
	if (pw == nullptr || pw->IsNull()) {
		auto product = iw.GetProduct(id);
		if (product.IsNull()) return;
		pw = &product;
	}

	auto slot = pw->GetSlot();
	if (slot.IsNull()) {
		cv->log("slot is null?");
		return;
	}

	int slotIdx = slot.GetSlotIndex();

	switch (slotIdx) {
	case BM::SLOT_BODY: ASSIGN_ITEM_TO_SLOT(id, this->body, isOnline, gw); break;
	case BM::SLOT_SKIN: ASSIGN_ITEM_TO_SLOT(id, this->decal, isOnline, gw); break;
	case BM::SLOT_WHEELS: ASSIGN_ITEM_TO_SLOT(id, this->wheels, isOnline, gw); break;
	case BM::SLOT_BOOST: ASSIGN_ITEM_TO_SLOT(id, this->boost, isOnline, gw); break;
	case BM::SLOT_ANTENNA: ASSIGN_ITEM_TO_SLOT(id, this->antenna, isOnline, gw); break;
	case BM::SLOT_HAT: ASSIGN_ITEM_TO_SLOT(id, this->topper, isOnline, gw); break;
	case BM::SLOT_PAINTFINISH: ASSIGN_ITEM_TO_SLOT(id, this->primaryFinish, isOnline, gw); break;
	case BM::SLOT_PAINTFINISH_SECONDARY: ASSIGN_ITEM_TO_SLOT(id, this->accentFinish, isOnline, gw); break;
	case BM::SLOT_ENGINE_AUDIO: ASSIGN_ITEM_TO_SLOT(id, this->engineAudio, isOnline, gw); break;
	case BM::SLOT_SUPERSONIC_TRAIL: ASSIGN_ITEM_TO_SLOT(id, this->trail, isOnline, gw); break;
	case BM::SLOT_GOALEXPLOSION: ASSIGN_ITEM_TO_SLOT(id, this->goalExplosion, isOnline, gw); break;
	}
}

void Loadout::fromLoadoutWrapper(int teamNum, std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw)
{
	auto iw = gw->GetItemsWrapper();
	if (iw.IsNull()) {
		cv->log("ItemsWrapper is NULL. Cannot load loadout.");
		return;
	}

	auto lw = iw.GetCurrentLoadout(teamNum);

	auto items = lw.GetLoadout();
	auto onlineItems = lw.GetOnlineLoadout();

	for (int i = 0; i < items.Count(); i++) {
		assignItemToSlot(items.Get(i), false, cv, gw);
	}

	for (int i = 0; i < onlineItems.Count(); i++) {
		assignItemToSlot(onlineItems.Get(i), true, cv, gw);
	}

	primaryPaint.fromPaintId(lw.GetPrimaryPaintColorId(), true, teamNum == 0 ? team0Colors : team1Colors);
	accentPaint.fromPaintId(lw.GetAccentPaintColorId(), false, customColors);
	primaryFinish.fromItem(lw.GetPrimaryFinishId(), false, gw);
	accentFinish.fromItem(lw.GetAccentFinishId(), false, gw);
}

void Loadout::cleanUpStandardItems()
{
	/* Some items stored as none when they're the standard items. Setting their item strings here to make it look better. */
	if (this->boost.itemString.compare("None") == 0) this->boost.itemString = "Standard";
	if (this->goalExplosion.itemString.compare("None") == 0) this->goalExplosion.itemString = "Classic";
	if (this->trail.itemString.compare("None") == 0) this->trail.itemString = "Classic";
	if (this->engineAudio.itemString.compare("None") == 0) this->engineAudio.itemString = "Default";
}

void Loadout::load(int teamNum, std::shared_ptr<CVarManagerWrapper> cv, std::shared_ptr<GameWrapper> gw)
{
	fromLoadoutWrapper(teamNum, cv, gw);
	fromBakkesMod(teamNum, cv, gw);
	cleanUpStandardItems();
	fromPlugins(teamNum, cv, gw);
}

void Loadout::clear()
{
	this->body.clear();
	this->decal.clear();
	this->wheels.clear();
	this->boost.clear();
	this->antenna.clear();
	this->topper.clear();
	this->primaryFinish.clear();
	this->accentFinish.clear();
	this->engineAudio.clear();
	this->trail.clear();
	this->goalExplosion.clear();

	this->primaryPaint.clear();
	this->accentPaint.clear();
}