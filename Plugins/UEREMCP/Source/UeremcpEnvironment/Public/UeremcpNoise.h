// UEREMCP — deterministic seeded noise (BACKLOG 5.4 / 5.6).
#pragma once

#include "CoreMinimal.h"

namespace UeremcpNoise
{
	/** SplitMix64-style mixer for reproducible seeds. */
	inline uint64 MixSeed(uint64 Seed)
	{
		Seed += 0x9E3779B97F4A7C15ull;
		Seed = (Seed ^ (Seed >> 30)) * 0xBF58476D1CE4E5B9ull;
		Seed = (Seed ^ (Seed >> 27)) * 0x94D049BB133111EBull;
		return Seed ^ (Seed >> 31);
	}

	/** Value noise in [0,1] at integer lattice; pure function of (seed,x,y). */
	inline float ValueNoise2D(uint64 Seed, int32 X, int32 Y)
	{
		const uint64 H = MixSeed(Seed ^ (uint64(uint32(X)) * 0xD1B54A32Dull) ^ (uint64(uint32(Y)) * 0xABC98388ull));
		return float(H & 0xFFFFFFull) / float(0xFFFFFFull);
	}

	inline float Fade(float T)
	{
		return T * T * (3.f - 2.f * T);
	}

	/** Bilinear value noise in [0,1]. */
	inline float SmoothNoise2D(uint64 Seed, float X, float Y)
	{
		const int32 X0 = FMath::FloorToInt(X);
		const int32 Y0 = FMath::FloorToInt(Y);
		const float Fx = Fade(X - float(X0));
		const float Fy = Fade(Y - float(Y0));
		const float V00 = ValueNoise2D(Seed, X0, Y0);
		const float V10 = ValueNoise2D(Seed, X0 + 1, Y0);
		const float V01 = ValueNoise2D(Seed, X0, Y0 + 1);
		const float V11 = ValueNoise2D(Seed, X0 + 1, Y0 + 1);
		const float A = FMath::Lerp(V00, V10, Fx);
		const float B = FMath::Lerp(V01, V11, Fx);
		return FMath::Lerp(A, B, Fy);
	}

	/** Multi-octave fBm in approx [0,1]. */
	inline float FBm2D(uint64 Seed, float X, float Y, int32 Octaves, float Lacunarity, float Gain)
	{
		float Sum = 0.f;
		float Amp = 1.f;
		float Norm = 0.f;
		float Fx = X;
		float Fy = Y;
		for (int32 O = 0; O < Octaves; ++O)
		{
			Sum += SmoothNoise2D(Seed + uint64(O) * 1013ull, Fx, Fy) * Amp;
			Norm += Amp;
			Amp *= Gain;
			Fx *= Lacunarity;
			Fy *= Lacunarity;
		}
		return Norm > 0.f ? Sum / Norm : 0.f;
	}
}
