// UEREMCP — pixel generators for procedural textures (WS-08).

#include "UeremcpProceduralTextureGenerator.h"

#include "Math/UnrealMathUtility.h"

namespace
{
	static float Hash01(int32 X, int32 Y, int32 Seed)
	{
		const uint32 Hash = HashCombine(GetTypeHash(Seed), HashCombine(GetTypeHash(X), GetTypeHash(Y)));
		return static_cast<float>(Hash & 0xFFFF) / 65535.0f;
	}

	static FColor ToColor(float R, float G, float B, float A = 1.0f)
	{
		return FColor(
			static_cast<uint8>(FMath::Clamp(R, 0.0f, 1.0f) * 255.0f),
			static_cast<uint8>(FMath::Clamp(G, 0.0f, 1.0f) * 255.0f),
			static_cast<uint8>(FMath::Clamp(B, 0.0f, 1.0f) * 255.0f),
			static_cast<uint8>(FMath::Clamp(A, 0.0f, 1.0f) * 255.0f));
	}
}

bool UeremcpProceduralTextureGenerator::GeneratePixels(
	const FString& Kind,
	int32 Width,
	int32 Height,
	int32 Seed,
	TArray<FColor>& OutPixels,
	FString& OutError)
{
	if (Width < 1 || Height < 1 || Width > 4096 || Height > 4096)
	{
		OutError = TEXT("dimensions must be between 1 and 4096.");
		return false;
	}

	OutPixels.SetNum(Width * Height);

	if (Kind == TEXT("noise"))
	{
		const float Frequency = 4.0f + static_cast<float>(Seed % 8);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const float U = static_cast<float>(X) / static_cast<float>(Width);
				const float V = static_cast<float>(Y) / static_cast<float>(Height);
				const float N = FMath::PerlinNoise2D(FVector2D(U * Frequency, V * Frequency + Seed * 0.01f));
				const float Gray = (N + 1.0f) * 0.5f;
				OutPixels[Y * Width + X] = ToColor(Gray, Gray, Gray);
			}
		}
		return true;
	}

	if (Kind == TEXT("gradient"))
	{
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const float T = static_cast<float>(Y) / static_cast<float>(FMath::Max(Height - 1, 1));
				OutPixels[Y * Width + X] = ToColor(T, T * 0.5f, 1.0f - T);
			}
		}
		return true;
	}

	if (Kind == TEXT("voronoi"))
	{
		const int32 CellSize = FMath::Max(Width / 8, 8);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				float MinDist = MAX_FLT;
				for (int32 Cy = -1; Cy <= 1; ++Cy)
				{
					for (int32 Cx = -1; Cx <= 1; ++Cx)
					{
						const int32 CellX = (X / CellSize + Cx);
						const int32 CellY = (Y / CellSize + Cy);
						const float Px = (CellX + Hash01(CellX, CellY, Seed)) * CellSize;
						const float Py = (CellY + Hash01(CellY, CellX, Seed + 17)) * CellSize;
						const float Dx = static_cast<float>(X) - Px;
						const float Dy = static_cast<float>(Y) - Py;
						MinDist = FMath::Min(MinDist, FMath::Sqrt(Dx * Dx + Dy * Dy));
					}
				}
				const float V = FMath::Clamp(MinDist / static_cast<float>(CellSize), 0.0f, 1.0f);
				OutPixels[Y * Width + X] = ToColor(V, V, V);
			}
		}
		return true;
	}

	if (Kind == TEXT("ring_mask"))
	{
		const float CenterX = Width * 0.5f;
		const float CenterY = Height * 0.5f;
		const float Outer = FMath::Min(CenterX, CenterY) * 0.9f;
		const float Inner = Outer * 0.65f;
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const float Dx = static_cast<float>(X) - CenterX;
				const float Dy = static_cast<float>(Y) - CenterY;
				const float Dist = FMath::Sqrt(Dx * Dx + Dy * Dy);
				const float Alpha = (Dist >= Inner && Dist <= Outer) ? 1.0f : 0.0f;
				OutPixels[Y * Width + X] = ToColor(1.0f, 1.0f, 1.0f, Alpha);
			}
		}
		return true;
	}

	if (Kind == TEXT("flow_map"))
	{
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const float U = static_cast<float>(X) / static_cast<float>(Width);
				const float V = static_cast<float>(Y) / static_cast<float>(Height);
				const float Angle = (U + V) * PI * 2.0f + Seed * 0.01f;
				const float R = FMath::Clamp(0.5f + 0.5f * FMath::Cos(Angle), 0.0f, 1.0f);
				const float G = FMath::Clamp(0.5f + 0.5f * FMath::Sin(Angle), 0.0f, 1.0f);
				OutPixels[Y * Width + X] = ToColor(R, G, 0.5f);
			}
		}
		return true;
	}

	OutError = FString::Printf(TEXT("Unsupported generate kind '%s'."), *Kind);
	return false;
}

bool UeremcpProceduralTextureGenerator::GenerateFlipbookAtlasPixels(
	int32 AtlasWidth,
	int32 AtlasHeight,
	int32 Columns,
	int32 Rows,
	int32 FrameCount,
	int32 Seed,
	TArray<FColor>& OutPixels,
	FString& OutError)
{
	if (AtlasWidth < 1 || AtlasHeight < 1 || AtlasWidth > 4096 || AtlasHeight > 4096)
	{
		OutError = TEXT("flipbook_atlas dimensions must be between 1 and 4096.");
		return false;
	}
	if (Columns < 1 || Rows < 1 || Columns > 64 || Rows > 64)
	{
		OutError = TEXT("flipbook columns and rows must be between 1 and 64.");
		return false;
	}
	if (FrameCount < 1 || FrameCount > Columns * Rows)
	{
		OutError = TEXT("flipbook.frame_count must be between 1 and columns * rows.");
		return false;
	}
	if (AtlasWidth % Columns != 0 || AtlasHeight % Rows != 0)
	{
		OutError = TEXT("flipbook_atlas atlas dimensions must divide evenly by columns and rows.");
		return false;
	}

	const int32 CellWidth = AtlasWidth / Columns;
	const int32 CellHeight = AtlasHeight / Rows;
	OutPixels.SetNum(AtlasWidth * AtlasHeight);

	for (int32 FrameIndex = 0; FrameIndex < FrameCount; ++FrameIndex)
	{
		TArray<FColor> FramePixels;
		const int32 FrameSeed = Seed + FrameIndex * 7919;
		if (!GeneratePixels(TEXT("noise"), CellWidth, CellHeight, FrameSeed, FramePixels, OutError))
		{
			return false;
		}

		const int32 Col = FrameIndex % Columns;
		const int32 Row = FrameIndex / Columns;
		const int32 OffsetX = Col * CellWidth;
		const int32 OffsetY = Row * CellHeight;

		for (int32 Y = 0; Y < CellHeight; ++Y)
		{
			for (int32 X = 0; X < CellWidth; ++X)
			{
				OutPixels[(OffsetY + Y) * AtlasWidth + (OffsetX + X)] = FramePixels[Y * CellWidth + X];
			}
		}
	}

	return true;
}
