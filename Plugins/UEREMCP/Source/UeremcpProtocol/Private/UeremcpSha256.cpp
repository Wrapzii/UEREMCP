#include "UeremcpSha256.h"

namespace UeremcpSha256
{
	static uint32 Rotr(uint32 X, uint32 N)
	{
		return (X >> N) | (X << (32 - N));
	}

	static void ProcessBlock(uint32 State[8], const uint8 Block[64])
	{
		static const uint32 K[64] = {
			0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
			0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
			0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
			0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
			0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
			0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
			0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
			0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
		};

		uint32 W[64];
		for (int32 I = 0; I < 16; ++I)
		{
			W[I] = (uint32(Block[I * 4]) << 24) | (uint32(Block[I * 4 + 1]) << 16)
				| (uint32(Block[I * 4 + 2]) << 8) | uint32(Block[I * 4 + 3]);
		}
		for (int32 I = 16; I < 64; ++I)
		{
			const uint32 S0 = Rotr(W[I - 15], 7) ^ Rotr(W[I - 15], 18) ^ (W[I - 15] >> 3);
			const uint32 S1 = Rotr(W[I - 2], 17) ^ Rotr(W[I - 2], 19) ^ (W[I - 2] >> 10);
			W[I] = W[I - 16] + S0 + W[I - 7] + S1;
		}

		uint32 A = State[0], B = State[1], C = State[2], D = State[3];
		uint32 E = State[4], F = State[5], G = State[6], H = State[7];

		for (int32 I = 0; I < 64; ++I)
		{
			const uint32 S1 = Rotr(E, 6) ^ Rotr(E, 11) ^ Rotr(E, 25);
			const uint32 Ch = (E & F) ^ ((~E) & G);
			const uint32 Temp1 = H + S1 + Ch + K[I] + W[I];
			const uint32 S0 = Rotr(A, 2) ^ Rotr(A, 13) ^ Rotr(A, 22);
			const uint32 Maj = (A & B) ^ (A & C) ^ (B & C);
			const uint32 Temp2 = S0 + Maj;

			H = G;
			G = F;
			F = E;
			E = D + Temp1;
			D = C;
			C = B;
			B = A;
			A = Temp1 + Temp2;
		}

		State[0] += A; State[1] += B; State[2] += C; State[3] += D;
		State[4] += E; State[5] += F; State[6] += G; State[7] += H;
	}

	void Hash(const uint8* Data, int64 Length, uint8 OutDigest[DigestBytes])
	{
		uint32 State[8] = {
			0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
			0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
		};

		uint8 Block[64];
		int64 Offset = 0;
		while (Length - Offset >= 64)
		{
			FMemory::Memcpy(Block, Data + Offset, 64);
			ProcessBlock(State, Block);
			Offset += 64;
		}

		const int64 Remaining = Length - Offset;
		FMemory::Memzero(Block, 64);
		if (Remaining > 0)
		{
			FMemory::Memcpy(Block, Data + Offset, Remaining);
		}
		Block[Remaining] = 0x80;

		if (Remaining >= 56)
		{
			ProcessBlock(State, Block);
			FMemory::Memzero(Block, 64);
		}

		const uint64 BitLen = uint64(Length) * 8ull;
		for (int32 I = 0; I < 8; ++I)
		{
			Block[63 - I] = uint8((BitLen >> (8 * I)) & 0xff);
		}
		ProcessBlock(State, Block);

		for (int32 I = 0; I < 8; ++I)
		{
			OutDigest[I * 4] = uint8((State[I] >> 24) & 0xff);
			OutDigest[I * 4 + 1] = uint8((State[I] >> 16) & 0xff);
			OutDigest[I * 4 + 2] = uint8((State[I] >> 8) & 0xff);
			OutDigest[I * 4 + 3] = uint8(State[I] & 0xff);
		}
	}

	FString ToHex(const uint8 Digest[DigestBytes])
	{
		static const TCHAR* Hex = TEXT("0123456789abcdef");
		FString Out;
		Out.Reserve(DigestBytes * 2);
		for (int32 I = 0; I < DigestBytes; ++I)
		{
			Out.AppendChar(Hex[(Digest[I] >> 4) & 0xf]);
			Out.AppendChar(Hex[Digest[I] & 0xf]);
		}
		return Out;
	}
}
