#pragma once

#include <vector>
#include <stdint.h>
#include "base64.h"

/*
Binary writer which optimizes for bit size. Programmer has to specify the number of LSB bits to store
Made by Bakkes
*/
template<typename A>
class BitBinaryWriter
{
public:
	const int type_size = sizeof(A) * 8;
	int current_bit = 0;
	A* buffer;
	BitBinaryWriter(int bufferSize)
	{
		buffer = (A*)calloc(bufferSize, type_size);
	}

	~BitBinaryWriter()
	{
		delete buffer;
	}

	template<typename T>
	void WriteBits(T t, int useLSBcount = sizeof(T) * 8)
	{
		T t2 = t;
		for (int i = 0; i < useLSBcount; i++)
		{
			if (t2 & 1) {
				buffer[current_bit / type_size] |= (1 << (current_bit % type_size));
			}
			else
			{
				buffer[current_bit / type_size] &= ~(1 << (current_bit % type_size));
			}
			current_bit++;
			t2 >>= 1;
		}
	}

	void WriteBool(bool b)
	{
		WriteBits(b ? 1 : 0, 1); //Use ternary to ensure last bit contains true false
	}

	uint8_t CalculateCRC(int startByte, int endByte)
	{
		uint8_t crc = 0b11111111;
		for (int i = startByte; i < endByte; i++)
		{
			crc = crc ^ buffer[i];
		}
		return crc;
	}

	std::string ToHex()
	{
		return base64_encode(buffer, current_bit / type_size + 1);
	}
};

template<typename A>
class BitBinaryReader
{
public:
	const int type_size = sizeof(A) * 8;
	int current_bit = 0;
	A* buffer;

	BitBinaryReader(std::string hexString)
	{
		std::vector<BYTE> decodedVector = base64_decode_bytearr(hexString);
		//std::string decoded = base64_decode(hexString);
		//const char* data = decoded.c_str();
		buffer = (A*)malloc(decodedVector.size());
		memcpy(buffer, &decodedVector[0], decodedVector.size());
	}

	BitBinaryReader(A* inBuf)
	{
		buffer = inBuf;
	}

	~BitBinaryReader()
	{
		delete buffer;
	}

	template<typename T>
	T ReadBits(int useLSBcount = sizeof(T) * 8)
	{
		T t = 0;
		for (int i = 0; i < useLSBcount; i++)
		{
			t |= ((buffer[current_bit / type_size] >> (current_bit % type_size)) & 1) << i;
			current_bit++;
		}
		return t;
	}
	uint8_t CalculateCRC(int startByte, int endByte)
	{
		uint8_t crc = 0b11111111;
		for (int i = startByte; i < endByte; i++)
		{
			crc = crc ^ buffer[i];
		}
		return crc;
	}
	bool VerifyCRC(uint8_t crc, int startByte, int endByte)
	{
		return (crc ^ CalculateCRC(startByte, endByte)) == 0;
	}

	bool ReadBool()
	{
		return ReadBits<bool>(1) & 1; //Use ternary to ensure last bit contains true false
	}
};

void write_loadout(BitBinaryWriter<unsigned char>& writer, std::map<uint8_t, BM::Item> loadout)
{
	//Save current position so we can write the length here later
	const int amountStorePos = writer.current_bit;
	//Reserve 4 bits to write size later
	writer.WriteBits(0, 4);

	//Counter that keeps track of size
	int loadoutSize = 0;
	for (auto opt : loadout)
	{
		//In bakkesmod, when unequipping the productID gets set to 0 but doesn't
		//get removed, so we do this check here.
		if (opt.second.product_id <= 0)
			continue;
		loadoutSize++;
		writer.WriteBits(opt.first, 5); //Slot index, 5 bits so we get slot upto 31
		writer.WriteBits(opt.second.product_id, 13); //Item id, 13 bits so upto 8191 should be enough
		writer.WriteBool(opt.second.paint_index > 0); //Bool indicating whether item is paintable or not
		if (opt.second.paint_index > 0) //If paintable
		{
			writer.WriteBits(opt.second.paint_index, 6); //6 bits, allow upto 63 paints
		}
	}

	//Save current position of writer
	const int amountStorePos2 = writer.current_bit;
	writer.current_bit = amountStorePos;
	//Write the size of the loadout to the spot we allocated earlier
	writer.WriteBits(loadoutSize, 4); //Gives us a max of 15 customizable slots per team
	writer.current_bit = amountStorePos2; //Set back reader to original position
}

void write_color(BitBinaryWriter<unsigned char>& writer, BM::RGB color)
{
	writer.WriteBits(color.r, 8);
	writer.WriteBits(color.g, 8);
	writer.WriteBits(color.b, 8);
}


std::string save(BM::BMLoadout loadout)
{
	//Allocate buffer thats big enough
	BitBinaryWriter<unsigned char> writer(10000);
	writer.WriteBits(CURRENT_LOADOUT_VERSION, 6); //Write current version

	/*
	We write 18 empty bits here, because we determine size and CRC after writing the whole loadout
	but we still need to allocate this space in advance
	*/
	writer.WriteBits(0, 18);

	writer.WriteBool(loadout.body.blue_is_orange); //Write blue == orange?
	write_loadout(writer, loadout.body.blue_loadout);

	writer.WriteBool(loadout.body.blueColor.should_override); //Write override blue car colors or not

	if (loadout.body.blueColor.should_override)
	{
		write_color(writer, loadout.body.blueColor.primary_colors); // write primary colors RGB (R = 0-255, G = 0-255, B = 0-255)
		write_color(writer, loadout.body.blueColor.secondary_colors); //write secondary
	}

	writer.WriteBits(loadout.body.blue_wheel_team_id, 6);

	if (!loadout.body.blue_is_orange)
	{
		write_loadout(writer, loadout.body.orange_loadout);
		writer.WriteBool(loadout.body.orangeColor.should_override);//Write override orange car colors or not
		if (loadout.body.orangeColor.should_override)
		{
			write_color(writer, loadout.body.orangeColor.primary_colors); //write primary
			write_color(writer, loadout.body.orangeColor.secondary_colors); //write secondary
		}
	}

	const int currentBit = writer.current_bit; //Save current location of writer

	int sizeInBytes = currentBit / 8 + (currentBit % 8 == 0 ? 0 : 1); //Calculate how many bytes are used
	writer.current_bit = 6; //Set writer to header (bit 6)
	writer.WriteBits(sizeInBytes, 10); //Write size
	writer.WriteBits(writer.CalculateCRC(3, sizeInBytes), 8); //Write calculated CRC
	writer.current_bit = currentBit; //Set writer back to original position
	return writer.ToHex();
}