// UEREMCP — shared spline corridor abstraction (BACKLOG 5.3).
#pragma once

#include "CoreMinimal.h"
#include "UeremcpNoise.h"

enum class EUeremcpSplineKind : uint8
{
	River,
	Road,
	Wall,
	Fence,
	Cliff,
	ExclusionCorridor
};

struct FUeremcpSplinePoint
{
	FVector Location = FVector::ZeroVector;
	float Width = 100.f;
};

struct FUeremcpSplinePath
{
	EUeremcpSplineKind Kind = EUeremcpSplineKind::River;
	TArray<FUeremcpSplinePoint> Points;

	bool IsValid() const { return Points.Num() >= 2; }

	float ApproximateLength() const
	{
		float Len = 0.f;
		for (int32 I = 1; I < Points.Num(); ++I)
		{
			Len += FVector::Distance(Points[I - 1].Location, Points[I].Location);
		}
		return Len;
	}

	/** Closest distance in XY to the polyline. */
	float DistanceToXY(const FVector& World) const
	{
		if (Points.Num() < 2)
		{
			return TNumericLimits<float>::Max();
		}
		float Best = TNumericLimits<float>::Max();
		const FVector2D P(World.X, World.Y);
		for (int32 I = 1; I < Points.Num(); ++I)
		{
			const FVector2D A(Points[I - 1].Location.X, Points[I - 1].Location.Y);
			const FVector2D B(Points[I].Location.X, Points[I].Location.Y);
			const FVector2D AB = B - A;
			const float AbLenSq = AB.SizeSquared();
			float T = 0.f;
			if (AbLenSq > KINDA_SMALL_NUMBER)
			{
				T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / AbLenSq, 0.f, 1.f);
			}
			const FVector2D Closest = A + AB * T;
			Best = FMath::Min(Best, FVector2D::Distance(P, Closest));
		}
		return Best;
	}

	float WidthAtClosest(const FVector& World) const
	{
		if (Points.Num() == 0)
		{
			return 0.f;
		}
		float Best = TNumericLimits<float>::Max();
		float Width = Points[0].Width;
		for (const FUeremcpSplinePoint& Pt : Points)
		{
			const float D = FVector2D::Distance(
				FVector2D(World.X, World.Y),
				FVector2D(Pt.Location.X, Pt.Location.Y));
			if (D < Best)
			{
				Best = D;
				Width = Pt.Width;
			}
		}
		return Width;
	}
};

namespace UeremcpSpline
{
	/**
	 * Build a meandering river path across a landscape extents box using seeded noise.
	 * Deterministic for (Seed, Extents).
	 */
	inline FUeremcpSplinePath MakeRiverAcross(
		uint64 Seed,
		const FBox& Extents,
		int32 NumPoints,
		float BaseWidth)
	{
		FUeremcpSplinePath Path;
		Path.Kind = EUeremcpSplineKind::River;
		NumPoints = FMath::Max(4, NumPoints);
		const float Y0 = Extents.Min.Y;
		const float Y1 = Extents.Max.Y;
		const float MidX = (Extents.Min.X + Extents.Max.X) * 0.5f;
		const float XAmp = (Extents.Max.X - Extents.Min.X) * 0.28f;
		for (int32 I = 0; I < NumPoints; ++I)
		{
			const float T = float(I) / float(NumPoints - 1);
			const float Y = FMath::Lerp(Y0, Y1, T);
			const float Wobble = (UeremcpNoise::FBm2D(Seed, T * 4.f, 0.5f, 4, 2.f, 0.5f) - 0.5f) * 2.f;
			FUeremcpSplinePoint Pt;
			Pt.Location = FVector(MidX + Wobble * XAmp, Y, Extents.Min.Z);
			Pt.Width = BaseWidth * (0.85f + 0.3f * UeremcpNoise::ValueNoise2D(Seed ^ 0xC0FFull, I, 7));
			Path.Points.Add(Pt);
		}
		return Path;
	}

	inline FUeremcpSplinePath MakeExclusionFrom(const FUeremcpSplinePath& Source, float ExtraWidth)
	{
		FUeremcpSplinePath Out = Source;
		Out.Kind = EUeremcpSplineKind::ExclusionCorridor;
		for (FUeremcpSplinePoint& Pt : Out.Points)
		{
			Pt.Width += ExtraWidth;
		}
		return Out;
	}
}
