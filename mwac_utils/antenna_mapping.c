
int done_mapping = 0;
int rec_index_to_actual[4] = {
		2,
		1,
		4,
		3
};

int input_to_slot[16] = {
		4,
		4,
		3,
		3,
		2,
		2,
		1,
		1,
		8,
		8,
		7,
		7,
		6,
		6,
		5,
		5
};
int single_pfb_mapping[64] = {
		0,
		16,
		32,
		48,
		1,
		17,
		33,
		49,
		2,
		18,
		34,
		50,
		3,
		19,
		35,
		51,
		4,
		20,
		36,
		52,
		5,
		21,
		37,
		53,
		6,
		22,
		38,
		54,
		7,
		23,
		39,
		55,
		8,
		24,
		40,
		56,
		9,
		25,
		41,
		57,
		10,
		26,
		42,
		58,
		11,
		27,
		43,
		59,
		12,
		28,
		44,
		60,
		13,
		29,
		45,
		61,
		14,
		30,
		46,
		62,
		15,
		31,
		47,
		63
};
int natural_to_mwac[256] = {
        150,
        150,
        148,
        148,
        146,
        146,
        144,
        144,
        158,
        158,
        156,
        156,
        154,
        154,
        152,
        152,
        230,
        230,
        228,
        228,
        226,
        226,
        224,
        224,
        238,
        238,
        236,
        236,
        234,
        234,
        232,
        232,
        246,
        246,
        244,
        244,
        242,
        242,
        240,
        240,
        254,
        254,
        252,
        252,
        250,
        250,
        248,
        248,
        102,
        102,
        100,
        100,
        98,
        98,
        96,
        96,
        110,
        110,
        108,
        108,
        106,
        106,
        104,
        104,
        22,
        22,
        20,
        20,
        18,
        18,
        16,
        16,
        30,
        30,
        28,
        28,
        26,
        26,
        24,
        24,
        6,
        6,
        4,
        4,
        2,
        2,
        0,
        0,
        14,
        14,
        12,
        12,
        10,
        10,
        8,
        8,
        134,
        134,
        132,
        132,
        130,
        130,
        128,
        128,
        142,
        142,
        140,
        140,
        138,
        138,
        136,
        136,
        118,
        118,
        116,
        116,
        114,
        114,
        112,
        112,
        126,
        126,
        124,
        124,
        122,
        122,
        120,
        120,
        182,
        182,
        180,
        180,
        178,
        178,
        176,
        176,
        190,
        190,
        188,
        188,
        186,
        186,
        184,
        184,
        166,
        166,
        164,
        164,
        162,
        162,
        160,
        160,
        174,
        174,
        172,
        172,
        170,
        170,
        168,
        168,
        214,
        214,
        212,
        212,
        210,
        210,
        208,
        208,
        222,
        222,
        220,
        220,
        218,
        218,
        216,
        216,
        198,
        198,
        196,
        196,
        194,
        194,
        192,
        192,
        206,
        206,
        204,
        204,
        202,
        202,
        200,
        200,
        38,
        38,
        36,
        36,
        34,
        34,
        32,
        32,
        46,
        46,
        44,
        44,
        42,
        42,
        40,
        40,
        54,
        54,
        52,
        52,
        50,
        50,
        48,
        48,
        62,
        62,
        60,
        60,
        58,
        58,
        56,
        56,
        86,
        86,
        84,
        84,
        82,
        82,
        80,
        80,
        94,
        94,
        92,
        92,
        90,
        90,
        88,
        88,
        70,
        70,
        68,
        68,
        66,
        66,
        64,
        64,
        78,
        78,
        76,
        76,
        74,
        74,
        72,
        72
};
int miriad_to_mwac[256] = {

		 150,
		 150,
		 148,
		 148,
		 146,
		 146,
		 144,
		 144,
		 158,
		 158,
		 156,
		 156,
		 154,
		 154,
		 152,
		 152,
		 102,
		 102,
		 100,
		 100,
		 98,
		 98,
		 96,
		 96,
		 110,
		 110,
		 108,
		 108,
		 106,
		 106,
		 104,
		 104,
		 246,
		 246,
		 244,
		 244,
		 242,
		 242,
		 240,
		 240,
		 254,
		 254,
		 252,
		 252,
		 250,
		 250,
		 248,
		 248,
		 230,
		 230,
		 228,
		 228,
		 226,
		 226,
		 224,
		 224,
		 238,
		 238,
		 236,
		 236,
		 234,
		 234,
		 232,
		 232,
		 22,
		 22,
		 20,
		 20,
		 18,
		 18,
		 16,
		 16,
		 30,
		 30,
		 28,
		 28,
		 26,
		 26,
		 24,
		 24,
		 6,
		 6,
		 4,
		 4,
		 2,
		 2,
		 0,
		 0,
		 14,
		 14,
		 12,
		 12,
		 10,
		 10,
		 8,
		 8,
		 134,
		 134,
		 132,
		 132,
		 130,
		 130,
		 128,
		 128,
		 142,
		 142,
		 140,
		 140,
		 138,
		 138,
		 136,
		 136,
		 118,
		 118,
		 116,
		 116,
		 114,
		 114,
		 112,
		 112,
		 126,
		 126,
		 124,
		 124,
		 122,
		 122,
		 120,
		 120,
		 182,
		 182,
		 180,
		 180,
		 178,
		 178,
		 176,
		 176,
		 190,
		 190,
		 188,
		 188,
		 186,
		 186,
		 184,
		 184,
		 166,
		 166,
		 164,
		 164,
		 162,
		 162,
		 160,
		 160,
		 174,
		 174,
		 172,
		 172,
		 170,
		 170,
		 168,
		 168,
		 214,
		 214,
		 212,
		 212,
		 210,
		 210,
		 208,
		 208,
		 222,
		 222,
		 220,
		 220,
		 218,
		 218,
		 216,
		 216,
		 198,
		 198,
		 196,
		 196,
		 194,
		 194,
		 192,
		 192,
		 206,
		 206,
		 204,
		 204,
		 202,
		 202,
		 200,
		 200,
		 38,
		 38,
		 36,
		 36,
		 34,
		 34,
		 32,
		 32,
		 46,
		 46,
		 44,
		 44,
		 42,
		 42,
		 40,
		 40,
		 54,
		 54,
		 52,
		 52,
		 50,
		 50,
		 48,
		 48,
		 62,
		 62,
		 60,
		 60,
		 58,
		 58,
		 56,
		 56,
		 86,
		 86,
		 84,
		 84,
		 82,
		 82,
		 80,
		 80,
		 94,
		 94,
		 92,
		 92,
		 90,
		 90,
		 88,
		 88,
		 70,
		 70,
		 68,
		 68,
		 66,
		 66,
		 64,
		 64,
		 78,
		 78,
		 76,
		 76,
		 74,
		 74,
		 72,
		 72
};
