// UEREMCP — feature-driven master material graph builder (WS-08).

#include "UeremcpMaterialFeatureGraph.h"

#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionDepthFade.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSphereMask.h"
#include "Materials/MaterialExpressionBumpOffset.h"
#include "Materials/MaterialExpressionTextureSampleParameterSubUV.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "UeremcpMaterialFeatures.h"
#include "UeremcpMaterialFunctionComposer.h"

namespace
{
	class FFeatureGraphBuilder
	{
	public:
		FFeatureGraphBuilder(UMaterial* InMaterial, const TArray<FString>& InFeatures, int32& InOutOps)
			: Material(InMaterial)
			, Ops(InOutOps)
		{
			for (const FString& Feature : InFeatures)
			{
				FeatureSet.Add(Feature);
			}
		}

		bool Has(const FString& Token) const { return FeatureSet.Contains(Token); }

		template<typename T>
		T* AddExpression(int32 X, int32 Y)
		{
			T* Expr = Cast<T>(UMaterialEditingLibrary::CreateMaterialExpression(
				Material, T::StaticClass(), X, Y));
			if (Expr)
			{
				++Ops;
			}
			return Expr;
		}

		bool Connect(UMaterialExpression* From, const FString& FromOut, UMaterialExpression* To, const FString& ToIn)
		{
			if (!From || !To)
			{
				return false;
			}
			const bool bOk = UMaterialEditingLibrary::ConnectMaterialExpressions(From, FromOut, To, ToIn);
			if (bOk)
			{
				++Ops;
			}
			return bOk;
		}

		bool ConnectToInputWithFallback(
			UMaterialExpression* From,
			const FString& FromOut,
			UMaterialExpression* To,
			std::initializer_list<const TCHAR*> InputPinCandidates)
		{
			for (const TCHAR* PinName : InputPinCandidates)
			{
				if (Connect(From, FromOut, To, PinName))
				{
					return true;
				}
			}
			return false;
		}

		UMaterialExpressionMultiply* Multiply(
			UMaterialExpression* A,
			const FString& AOut,
			UMaterialExpression* B,
			const FString& BOut,
			int32 X,
			int32 Y)
		{
			UMaterialExpressionMultiply* Mul = AddExpression<UMaterialExpressionMultiply>(X, Y);
			if (!Mul)
			{
				return nullptr;
			}
			if (!Connect(A, AOut, Mul, TEXT("A")) || !Connect(B, BOut, Mul, TEXT("B")))
			{
				return nullptr;
			}
			return Mul;
		}

		bool Build(FUeremcpFeatureGraphBuildResult& Result)
		{
			Material->BlendMode = BLEND_Additive;
			Material->TwoSided = true;
			Material->SetShadingModel(MSM_Unlit);

			UMaterialExpressionVectorParameter* ParticleColor =
				AddExpression<UMaterialExpressionVectorParameter>(-800, 0);
			if (!ParticleColor)
			{
				Result.Error = TEXT("Failed to create ParticleColor parameter.");
				return false;
			}
			ParticleColor->ParameterName = FName(TEXT("ParticleColor"));

			UMaterialExpressionVectorParameter* ColorSecondary =
				AddExpression<UMaterialExpressionVectorParameter>(-800, 120);
			if (!ColorSecondary)
			{
				Result.Error = TEXT("Failed to create ColorSecondary parameter.");
				return false;
			}
			ColorSecondary->ParameterName = FName(TEXT("ColorSecondary"));

			UMaterialExpressionScalarParameter* EmissiveScale =
				AddExpression<UMaterialExpressionScalarParameter>(-800, 240);
			if (!EmissiveScale)
			{
				Result.Error = TEXT("Failed to create EmissiveScale parameter.");
				return false;
			}
			EmissiveScale->ParameterName = FName(TEXT("EmissiveScale"));
			EmissiveScale->DefaultValue = 1.0f;

			UMaterialExpressionScalarParameter* SoftEdge =
				AddExpression<UMaterialExpressionScalarParameter>(-800, 360);
			if (SoftEdge)
			{
				SoftEdge->ParameterName = FName(TEXT("SoftEdge"));
				SoftEdge->DefaultValue = 0.75f;
			}

			UMaterialExpressionScalarParameter* FlowSpeed =
				AddExpression<UMaterialExpressionScalarParameter>(-800, 480);
			if (FlowSpeed)
			{
				FlowSpeed->ParameterName = FName(TEXT("FlowSpeed"));
				FlowSpeed->DefaultValue = 0.5f;
			}

			UMaterialExpressionScalarParameter* Turbulence =
				AddExpression<UMaterialExpressionScalarParameter>(-800, 600);
			if (Turbulence)
			{
				Turbulence->ParameterName = FName(TEXT("Turbulence"));
				Turbulence->DefaultValue = 0.5f;
			}

			UMaterialExpressionScalarParameter* DepthFadeParam =
				AddExpression<UMaterialExpressionScalarParameter>(-800, 720);
			if (DepthFadeParam)
			{
				DepthFadeParam->ParameterName = FName(TEXT("DepthFade"));
				DepthFadeParam->DefaultValue = 100.0f;
			}

			UMaterialExpressionScalarParameter* DissolveAmount =
				AddExpression<UMaterialExpressionScalarParameter>(-800, 840);
			if (DissolveAmount)
			{
				DissolveAmount->ParameterName = FName(TEXT("DissolveAmount"));
				DissolveAmount->DefaultValue = 0.0f;
			}

			UMaterialExpressionScalarParameter* DistortionStrength =
				AddExpression<UMaterialExpressionScalarParameter>(-800, 960);
			if (DistortionStrength)
			{
				DistortionStrength->ParameterName = FName(TEXT("DistortionStrength"));
				DistortionStrength->DefaultValue = 0.05f;
			}

			UMaterialExpression* ColorChain = ParticleColor;
			if (Has(TEXT("dynamic_color")))
			{
				UMaterialExpressionLinearInterpolate* Lerp =
					AddExpression<UMaterialExpressionLinearInterpolate>(-500, 60);
				UMaterialExpressionConstant* LerpAlpha =
					AddExpression<UMaterialExpressionConstant>(-650, 180);
				if (!Lerp || !LerpAlpha)
				{
					Result.Error = TEXT("Failed to create dynamic_color lerp.");
					return false;
				}
				LerpAlpha->R = 0.35f;
				if (!Connect(ParticleColor, TEXT("RGB"), Lerp, TEXT("A")) ||
					!Connect(ColorSecondary, TEXT("RGB"), Lerp, TEXT("B")) ||
					!Connect(LerpAlpha, TEXT(""), Lerp, TEXT("Alpha")))
				{
					Result.Error = TEXT("Failed to wire dynamic_color lerp.");
					return false;
				}
				ColorChain = Lerp;
				Result.WiredFeatures.Add(TEXT("dynamic_color"));
			}

			UMaterialExpressionTextureCoordinate* TexCoord =
				AddExpression<UMaterialExpressionTextureCoordinate>(-500, 300);

			UMaterialExpression* MaskChain = nullptr;
			if (Has(TEXT("radial_falloff")) && TexCoord && SoftEdge)
			{
				UMaterialExpressionConstant* Center = AddExpression<UMaterialExpressionConstant>(-650, 300);
				UMaterialExpressionSphereMask* SphereMask =
					AddExpression<UMaterialExpressionSphereMask>(-350, 300);
				if (!Center || !SphereMask)
				{
					Result.Error = TEXT("Failed to create radial_falloff sphere mask.");
					return false;
				}
				Center->R = 0.5f;
				SphereMask->AttenuationRadius = 0.45f;
				SphereMask->HardnessPercent = 80.0f;
				if (!Connect(TexCoord, TEXT(""), SphereMask, TEXT("A")) ||
					!Connect(Center, TEXT(""), SphereMask, TEXT("B")) ||
					!Connect(SoftEdge, TEXT(""), SphereMask, TEXT("Hardness")))
				{
					Result.Error = TEXT("Failed to wire radial_falloff.");
					return false;
				}
				MaskChain = SphereMask;
				Result.WiredFeatures.Add(TEXT("radial_falloff"));
			}

			UMaterialExpression* EmissiveChain = ColorChain;
			if (MaskChain)
			{
				UMaterialExpressionMultiply* ColorMask = Multiply(ColorChain, TEXT(""), MaskChain, TEXT(""), -250, 60);
				if (!ColorMask)
				{
					Result.Error = TEXT("Failed to multiply color by radial mask.");
					return false;
				}
				EmissiveChain = ColorMask;
			}

			if (Has(TEXT("animated_noise")) && TexCoord && Turbulence)
			{
				UMaterialExpressionTime* TimeExpr = AddExpression<UMaterialExpressionTime>(-650, 420);
				UMaterialExpressionPanner* NoisePanner = AddExpression<UMaterialExpressionPanner>(-500, 420);
				UMaterialExpressionAppendVector* NoisePosition =
					AddExpression<UMaterialExpressionAppendVector>(-420, 420);
				UMaterialExpressionConstant* NoisePositionZ =
					AddExpression<UMaterialExpressionConstant>(-550, 480);
				UMaterialExpressionNoise* NoiseExpr = AddExpression<UMaterialExpressionNoise>(-350, 420);
				if (!TimeExpr || !NoisePanner || !NoisePosition || !NoisePositionZ || !NoiseExpr)
				{
					Result.Error = TEXT("Failed to create animated_noise nodes.");
					return false;
				}
				NoiseExpr->Scale = 4.0f;
				NoiseExpr->Quality = 2;
				NoisePanner->SpeedX = 0.25f;
				NoisePanner->SpeedY = 0.25f;
				NoisePositionZ->R = 0.0f;
				// Panner outputs float2; Noise Position expects float3 [DOCS: utility material expressions].
				if (!Connect(TexCoord, TEXT(""), NoisePanner, TEXT("Coordinate")) ||
					!Connect(TimeExpr, TEXT(""), NoisePanner, TEXT("Time")) ||
					!ConnectToInputWithFallback(NoisePanner, TEXT(""), NoisePosition, {TEXT("A"), TEXT("Input1"), TEXT("")}) ||
					!ConnectToInputWithFallback(NoisePositionZ, TEXT(""), NoisePosition, {TEXT("B"), TEXT("Input2"), TEXT("")}) ||
					!ConnectToInputWithFallback(
						NoisePosition,
						TEXT(""),
						NoiseExpr,
						{TEXT("World Position"), TEXT("Position"), TEXT("")}) ||
					!Connect(Turbulence, TEXT(""), NoiseExpr, TEXT("FilterWidth")))
				{
					Result.Error = TEXT("Failed to wire animated_noise.");
					return false;
				}
				UMaterialExpressionMultiply* NoiseMod = Multiply(EmissiveChain, TEXT(""), NoiseExpr, TEXT(""), -120, 200);
				if (!NoiseMod)
				{
					Result.Error = TEXT("Failed to multiply emissive by noise.");
					return false;
				}
				EmissiveChain = NoiseMod;
				Result.WiredFeatures.Add(TEXT("animated_noise"));
			}

			if (Has(TEXT("fresnel")))
			{
				UMaterialExpressionFresnel* FresnelExpr = AddExpression<UMaterialExpressionFresnel>(-350, 540);
				if (!FresnelExpr)
				{
					Result.Error = TEXT("Failed to create fresnel node.");
					return false;
				}
				FresnelExpr->Exponent = 3.0f;
				UMaterialExpressionMultiply* FresnelMod = Multiply(EmissiveChain, TEXT(""), FresnelExpr, TEXT(""), -120, 320);
				if (!FresnelMod)
				{
					Result.Error = TEXT("Failed to wire fresnel.");
					return false;
				}
				EmissiveChain = FresnelMod;
				Result.WiredFeatures.Add(TEXT("fresnel"));
			}

			UMaterialExpressionTextureSampleParameter2D* MainTextureSample = nullptr;
			if (Has(TEXT("flipbook_subuv")))
			{
				UMaterialExpressionTextureSampleParameterSubUV* SubUvSample =
					AddExpression<UMaterialExpressionTextureSampleParameterSubUV>(-550, 640);
				if (!SubUvSample)
				{
					Result.Error = TEXT("Failed to create MainTexture SubUV sample.");
					return false;
				}
				SubUvSample->ParameterName = FName(TEXT("MainTexture"));
				SubUvSample->bBlend = true;
				MainTextureSample = SubUvSample;
				Result.WiredFeatures.Add(TEXT("flipbook_subuv"));
			}
			else
			{
				MainTextureSample = AddExpression<UMaterialExpressionTextureSampleParameter2D>(-550, 640);
				if (!MainTextureSample)
				{
					Result.Error = TEXT("Failed to create MainTexture sample.");
					return false;
				}
				MainTextureSample->ParameterName = FName(TEXT("MainTexture"));
			}

			UMaterialExpressionTextureSampleParameter2D* NoiseTextureSample =
				AddExpression<UMaterialExpressionTextureSampleParameter2D>(-550, 760);
			UMaterialExpressionTextureSampleParameter2D* FlowMapSample =
				AddExpression<UMaterialExpressionTextureSampleParameter2D>(-550, 880);
			UMaterialExpressionTextureSampleParameter2D* MaskTextureSample =
				AddExpression<UMaterialExpressionTextureSampleParameter2D>(-550, 1000);
			if (!NoiseTextureSample || !FlowMapSample || !MaskTextureSample)
			{
				Result.Error = TEXT("Failed to create MI texture parameter samples.");
				return false;
			}
			NoiseTextureSample->ParameterName = FName(TEXT("NoiseTexture"));
			FlowMapSample->ParameterName = FName(TEXT("FlowMap"));
			MaskTextureSample->ParameterName = FName(TEXT("MaskTexture"));

			UMaterialExpression* UvChain = TexCoord;
			if (Has(TEXT("distortion")) && DistortionStrength && NoiseTextureSample && UvChain)
			{
				UMaterialExpressionBumpOffset* BumpOffsetExpr =
					AddExpression<UMaterialExpressionBumpOffset>(-350, 900);
				if (!BumpOffsetExpr)
				{
					Result.Error = TEXT("Failed to create distortion BumpOffset node.");
					return false;
				}
				BumpOffsetExpr->ReferencePlane = 0.5f;
				if (!Connect(UvChain, TEXT(""), BumpOffsetExpr, TEXT("Coordinate")) ||
					!Connect(NoiseTextureSample, TEXT("R"), BumpOffsetExpr, TEXT("Height")) ||
					!Connect(DistortionStrength, TEXT(""), BumpOffsetExpr, TEXT("HeightRatioInput")))
				{
					Result.Error = TEXT("Failed to wire distortion (BumpOffset approximation).");
					return false;
				}
				UvChain = BumpOffsetExpr;
				Result.WiredFeatures.Add(TEXT("distortion"));
			}

			const bool bUsesMainTextureChain =
				Has(TEXT("panning_textures")) || Has(TEXT("flipbook_subuv")) || Has(TEXT("distortion"));

			if (Has(TEXT("panning_textures")) && UvChain && FlowSpeed && MainTextureSample)
			{
				UMaterialExpressionPanner* PannerExpr = AddExpression<UMaterialExpressionPanner>(-350, 660);
				if (!PannerExpr)
				{
					Result.Error = TEXT("Failed to create panner node.");
					return false;
				}
				PannerExpr->SpeedX = 0.5f;
				PannerExpr->SpeedY = 0.0f;
				if (!Connect(UvChain, TEXT(""), PannerExpr, TEXT("Coordinate")) ||
					!Connect(FlowSpeed, TEXT(""), PannerExpr, TEXT("Speed")))
				{
					Result.Error = TEXT("Failed to wire panning_textures.");
					return false;
				}
				UvChain = PannerExpr;
				Result.WiredFeatures.Add(TEXT("panning_textures"));
			}

			if (bUsesMainTextureChain && MainTextureSample && UvChain)
			{
				if (!Connect(UvChain, TEXT(""), MainTextureSample, TEXT("Coordinates")))
				{
					Result.Error = TEXT("Failed to connect UV chain to MainTexture.");
					return false;
				}
				UMaterialExpressionMultiply* MainTexMod =
					Multiply(EmissiveChain, TEXT(""), MainTextureSample, TEXT("RGB"), -120, 440);
				if (!MainTexMod)
				{
					Result.Error = TEXT("Failed to multiply emissive by MainTexture.");
					return false;
				}
				EmissiveChain = MainTexMod;
			}

			if (Has(TEXT("flow_maps")) && TexCoord && FlowSpeed && FlowMapSample)
			{
				UMaterialExpressionPanner* FlowPanner = AddExpression<UMaterialExpressionPanner>(-350, 780);
				if (!FlowPanner)
				{
					Result.Error = TEXT("Failed to create flow_maps panner node.");
					return false;
				}
				FlowPanner->SpeedX = 0.35f;
				FlowPanner->SpeedY = 0.35f;
				if (!Connect(TexCoord, TEXT(""), FlowPanner, TEXT("Coordinate")) ||
					!Connect(FlowSpeed, TEXT(""), FlowPanner, TEXT("Speed")) ||
					!Connect(FlowPanner, TEXT(""), FlowMapSample, TEXT("Coordinates")))
				{
					Result.Error = TEXT("Failed to wire flow_maps.");
					return false;
				}
				UMaterialExpressionMultiply* FlowMod =
					Multiply(EmissiveChain, TEXT(""), FlowMapSample, TEXT("RGB"), -120, 560);
				if (!FlowMod)
				{
					Result.Error = TEXT("Failed to multiply emissive by FlowMap.");
					return false;
				}
				EmissiveChain = FlowMod;
				Result.WiredFeatures.Add(TEXT("flow_maps"));
			}

			if (Has(TEXT("dynamic_intensity")))
			{
				UMaterialExpressionMultiply* IntensityMod =
					Multiply(EmissiveChain, TEXT(""), EmissiveScale, TEXT(""), 80, 120);
				if (!IntensityMod)
				{
					Result.Error = TEXT("Failed to wire dynamic_intensity.");
					return false;
				}
				EmissiveChain = IntensityMod;
				Result.WiredFeatures.Add(TEXT("dynamic_intensity"));
			}

			if (!UMaterialEditingLibrary::ConnectMaterialProperty(EmissiveChain, TEXT(""), MP_EmissiveColor))
			{
				Result.Error = TEXT("Failed to connect emissive output.");
				return false;
			}
			++Ops;

			UMaterialExpression* OpacityChain = MaskChain;
			if (!OpacityChain)
			{
				UMaterialExpressionConstant* FullOpacity = AddExpression<UMaterialExpressionConstant>(-350, 780);
				if (!FullOpacity)
				{
					Result.Error = TEXT("Failed to create base opacity.");
					return false;
				}
				FullOpacity->R = 1.0f;
				OpacityChain = FullOpacity;
			}

			if (Has(TEXT("erosion")) && DissolveAmount)
			{
				UMaterialExpressionOneMinus* OneMinus =
					AddExpression<UMaterialExpressionOneMinus>(-350, 900);
				if (!OneMinus)
				{
					Result.Error = TEXT("Failed to create erosion one-minus.");
					return false;
				}
				if (!Connect(DissolveAmount, TEXT(""), OneMinus, TEXT("Input")))
				{
					Result.Error = TEXT("Failed to wire DissolveAmount to erosion.");
					return false;
				}
				UMaterialExpressionMultiply* ErosionMod =
					Multiply(OpacityChain, TEXT(""), OneMinus, TEXT(""), -120, 780);
				if (!ErosionMod)
				{
					Result.Error = TEXT("Failed to wire erosion multiply.");
					return false;
				}
				OpacityChain = ErosionMod;
				Result.WiredFeatures.Add(TEXT("erosion"));
			}

			if (Has(TEXT("depth_fade")) && DepthFadeParam)
			{
				UMaterialExpressionDepthFade* DepthFadeExpr =
					AddExpression<UMaterialExpressionDepthFade>(-350, 1020);
				if (!DepthFadeExpr)
				{
					Result.Error = TEXT("Failed to create depth_fade node.");
					return false;
				}
				DepthFadeExpr->OpacityDefault = 1.0f;
				if (!Connect(OpacityChain, TEXT(""), DepthFadeExpr, TEXT("InOpacity")) ||
					!Connect(DepthFadeParam, TEXT(""), DepthFadeExpr, TEXT("FadeDistance")))
				{
					Result.Error = TEXT("Failed to wire depth_fade.");
					return false;
				}
				OpacityChain = DepthFadeExpr;
				Result.WiredFeatures.Add(TEXT("depth_fade"));
			}

			if (Has(TEXT("depth_fade")) || Has(TEXT("erosion")) || Has(TEXT("panning_textures")))
			{
				if (!UMaterialEditingLibrary::ConnectMaterialProperty(OpacityChain, TEXT(""), MP_Opacity))
				{
					Result.Error = TEXT("Failed to connect opacity output.");
					return false;
				}
				++Ops;
			}

			const TArray<FString> Unimplemented = UeremcpMaterialFeatures::FindUnimplementedFeatures(FeatureSet.Array());
			for (const FString& Skipped : Unimplemented)
			{
				Result.SkippedFeatures.Add(Skipped);
			}

			const TArray<FString> CompileErrors = UMaterialEditingLibrary::RecompileMaterial(Material);
			++Ops;
			if (CompileErrors.Num() > 0)
			{
				Result.Error = FString::Printf(
					TEXT("Feature graph recompile failed: %s"),
					*FString::Join(CompileErrors, TEXT("; ")));
				return false;
			}

			Material->MarkPackageDirty();
			Result.bSuccess = true;
			return true;
		}

	private:
		UMaterial* Material;
		int32& Ops;
		TSet<FString> FeatureSet;
	};
}

FUeremcpFeatureGraphBuildResult UeremcpMaterialFeatureGraph::BuildGraph(
	UMaterial* Material,
	const TArray<FString>& Features,
	bool bTrailPurpose)
{
	FUeremcpFeatureGraphBuildResult Result;
	(void)bTrailPurpose;
	if (!Material)
	{
		Result.Error = TEXT("Null material.");
		return Result;
	}

	const FUeremcpMaterialFunctionComposeResult ComposeProbe =
		UeremcpMaterialFunctionComposer::ProbeComposition(Material, Features);
	Result.CompositionStatus = ComposeProbe.Status;
	Result.InterpretationNotes.Append(ComposeProbe.InterpretationNotes);
	Result.CapabilityNotes.Append(ComposeProbe.CapabilityNotes);
	if (!ComposeProbe.Summary.IsEmpty() && ComposeProbe.DeferredFeatures.Num() > 0)
	{
		Result.InterpretationNotes.Add(ComposeProbe.Summary);
	}

	FFeatureGraphBuilder Builder(Material, Features, Result.InternalOperations);
	if (!Builder.Build(Result))
	{
		Result.bSuccess = false;
		return Result;
	}

	UeremcpMaterialFeatures::FFeatureGraphVerifyResult Verify;
	if (!UeremcpMaterialFeatures::VerifyFeatureGraph(Material, Features, Verify))
	{
		Result.Error = TEXT("Feature graph post-build verification failed.");
		Result.bSuccess = false;
		return Result;
	}

	Result.bSuccess = true;
	return Result;
}
