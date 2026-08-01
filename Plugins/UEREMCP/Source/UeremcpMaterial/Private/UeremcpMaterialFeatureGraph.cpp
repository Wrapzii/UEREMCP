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
#include "Materials/MaterialExpressionParticleColor.h"
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

		bool Build(FUeremcpFeatureGraphBuildResult& Result, bool bTranslucentBlend)
		{
			Material->BlendMode = bTranslucentBlend ? BLEND_Translucent : BLEND_Additive;
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

			UMaterialExpressionParticleColor* NiagaraParticleColor =
				AddExpression<UMaterialExpressionParticleColor>(-800, -120);
			if (!NiagaraParticleColor)
			{
				Result.Error = TEXT("Failed to create Niagara Particle Color expression.");
				return false;
			}

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

			UMaterialExpression* ElementTintChain = ParticleColor;
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
				ElementTintChain = Lerp;
				Result.WiredFeatures.Add(TEXT("dynamic_color"));
			}

			// UMaterialExpressionParticleColor compiles the renderer-provided ParticleColor value
			// [VERIFIED: MaterialExpressions.cpp:10069-10088; HLSLMaterialTranslator.cpp:6027-6031].
			// Keep the MI ParticleColor parameter as an element tint while consuming Particles.Color.
			UMaterialExpressionMultiply* RendererTint =
				Multiply(ElementTintChain, TEXT(""), NiagaraParticleColor, TEXT("RGB"), -300, 60);
			if (!RendererTint)
			{
				Result.Error = TEXT("Failed to multiply element tint by Niagara Particle Color.");
				return false;
			}
			UMaterialExpression* ColorChain = RendererTint;

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
				NoiseExpr->OutputMin = 0.0f;
				NoiseExpr->OutputMax = 1.0f;
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
				UMaterialExpressionConstant* NoiseFloor =
					AddExpression<UMaterialExpressionConstant>(-250, 200);
				UMaterialExpressionConstant* NoiseCeiling =
					AddExpression<UMaterialExpressionConstant>(-250, 240);
				UMaterialExpressionLinearInterpolate* NonZeroNoise =
					AddExpression<UMaterialExpressionLinearInterpolate>(-120, 240);
				if (!NoiseFloor || !NoiseCeiling || !NonZeroNoise)
				{
					Result.Error = TEXT("Failed to create non-zero animated_noise modulation.");
					return false;
				}
				NoiseFloor->R = 0.35f;
				NoiseCeiling->R = 1.0f;
				if (!Connect(NoiseFloor, TEXT(""), NonZeroNoise, TEXT("A")) ||
					!Connect(NoiseCeiling, TEXT(""), NonZeroNoise, TEXT("B")) ||
					!Connect(NoiseExpr, TEXT(""), NonZeroNoise, TEXT("Alpha")))
				{
					Result.Error = TEXT("Failed to wire non-zero animated_noise modulation.");
					return false;
				}
				UMaterialExpressionMultiply* NoiseMod =
					Multiply(EmissiveChain, TEXT(""), NonZeroNoise, TEXT(""), 40, 240);
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
				UMaterialExpressionConstant* FresnelFloor =
					AddExpression<UMaterialExpressionConstant>(-250, 320);
				UMaterialExpressionConstant* FresnelCeiling =
					AddExpression<UMaterialExpressionConstant>(-250, 360);
				UMaterialExpressionLinearInterpolate* NonZeroFresnel =
					AddExpression<UMaterialExpressionLinearInterpolate>(-120, 360);
				if (!FresnelFloor || !FresnelCeiling || !NonZeroFresnel)
				{
					Result.Error = TEXT("Failed to create non-zero fresnel modulation.");
					return false;
				}
				FresnelFloor->R = 0.35f;
				FresnelCeiling->R = 1.0f;
				if (!Connect(FresnelFloor, TEXT(""), NonZeroFresnel, TEXT("A")) ||
					!Connect(FresnelCeiling, TEXT(""), NonZeroFresnel, TEXT("B")) ||
					!Connect(FresnelExpr, TEXT(""), NonZeroFresnel, TEXT("Alpha")))
				{
					Result.Error = TEXT("Failed to wire non-zero fresnel modulation.");
					return false;
				}
				UMaterialExpressionMultiply* FresnelMod =
					Multiply(EmissiveChain, TEXT(""), NonZeroFresnel, TEXT(""), 40, 360);
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
				UMaterialExpressionConstant* PannerSpeedY =
					AddExpression<UMaterialExpressionConstant>(-500, 720);
				UMaterialExpressionAppendVector* PannerSpeed =
					AddExpression<UMaterialExpressionAppendVector>(-420, 690);
				if (!PannerExpr || !PannerSpeedY || !PannerSpeed)
				{
					Result.Error = TEXT("Failed to create panner node.");
					return false;
				}
				PannerExpr->SpeedX = 0.5f;
				PannerExpr->SpeedY = 0.0f;
				PannerSpeedY->R = 0.0f;
				// Panner Speed is float2; FlowSpeed is scalar [VERIFIED: MaterialExpressionPanner.h:23-24].
				if (!Connect(UvChain, TEXT(""), PannerExpr, TEXT("Coordinate")) ||
					!ConnectToInputWithFallback(FlowSpeed, TEXT(""), PannerSpeed, {TEXT("A"), TEXT("Input1"), TEXT("")}) ||
					!ConnectToInputWithFallback(PannerSpeedY, TEXT(""), PannerSpeed, {TEXT("B"), TEXT("Input2"), TEXT("")}) ||
					!ConnectToInputWithFallback(PannerSpeed, TEXT(""), PannerExpr, {TEXT("Speed"), TEXT("")}))
				{
					Result.Error = TEXT("Failed to wire panning_textures.");
					return false;
				}
				UvChain = PannerExpr;
				Result.WiredFeatures.Add(TEXT("panning_textures"));
			}

			if (bUsesMainTextureChain && MainTextureSample && UvChain)
			{
				// Texture sample UV pin is shortened to "UVs" in MaterialEditingLibrary
				// [VERIFIED: MaterialGraphNode.cpp:597-605].
				if (!ConnectToInputWithFallback(
						UvChain,
						TEXT(""),
						MainTextureSample,
						{TEXT("UVs"), TEXT("Coordinates"), TEXT("")}))
				{
					Result.Error = TEXT("Failed to connect UV chain to MainTexture.");
					return false;
				}
				UMaterialExpressionConstant* TextureFloor =
					AddExpression<UMaterialExpressionConstant>(-250, 440);
				UMaterialExpressionConstant* TextureCeiling =
					AddExpression<UMaterialExpressionConstant>(-250, 480);
				UMaterialExpressionLinearInterpolate* NonZeroTexture =
					AddExpression<UMaterialExpressionLinearInterpolate>(-120, 480);
				if (!TextureFloor || !TextureCeiling || !NonZeroTexture)
				{
					Result.Error = TEXT("Failed to create non-zero MainTexture modulation.");
					return false;
				}
				TextureFloor->R = 0.35f;
				TextureCeiling->R = 1.0f;
				if (!Connect(TextureFloor, TEXT(""), NonZeroTexture, TEXT("A")) ||
					!Connect(TextureCeiling, TEXT(""), NonZeroTexture, TEXT("B")) ||
					!Connect(MainTextureSample, TEXT("RGB"), NonZeroTexture, TEXT("Alpha")))
				{
					Result.Error = TEXT("Failed to wire non-zero MainTexture modulation.");
					return false;
				}
				UMaterialExpressionMultiply* MainTexMod =
					Multiply(EmissiveChain, TEXT(""), NonZeroTexture, TEXT(""), 40, 480);
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
				UMaterialExpressionConstant* FlowPannerSpeedY =
					AddExpression<UMaterialExpressionConstant>(-500, 840);
				UMaterialExpressionAppendVector* FlowPannerSpeed =
					AddExpression<UMaterialExpressionAppendVector>(-420, 810);
				if (!FlowPanner || !FlowPannerSpeedY || !FlowPannerSpeed)
				{
					Result.Error = TEXT("Failed to create flow_maps panner node.");
					return false;
				}
				FlowPanner->SpeedX = 0.35f;
				FlowPanner->SpeedY = 0.35f;
				FlowPannerSpeedY->R = 0.35f;
				if (!Connect(TexCoord, TEXT(""), FlowPanner, TEXT("Coordinate")) ||
					!ConnectToInputWithFallback(FlowSpeed, TEXT(""), FlowPannerSpeed, {TEXT("A"), TEXT("Input1"), TEXT("")}) ||
					!ConnectToInputWithFallback(FlowPannerSpeedY, TEXT(""), FlowPannerSpeed, {TEXT("B"), TEXT("Input2"), TEXT("")}) ||
					!ConnectToInputWithFallback(FlowPannerSpeed, TEXT(""), FlowPanner, {TEXT("Speed"), TEXT("")}) ||
					!ConnectToInputWithFallback(
						FlowPanner,
						TEXT(""),
						FlowMapSample,
						{TEXT("UVs"), TEXT("Coordinates"), TEXT("")}))
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

			UMaterialExpressionMultiply* ParticleAlphaMod =
				Multiply(OpacityChain, TEXT(""), NiagaraParticleColor, TEXT("A"), -120, 720);
			if (!ParticleAlphaMod)
			{
				Result.Error = TEXT("Failed to multiply opacity by Niagara Particle Color alpha.");
				return false;
			}
			OpacityChain = ParticleAlphaMod;

			if (Has(TEXT("erosion")) && DissolveAmount)
			{
				UMaterialExpressionOneMinus* OneMinus =
					AddExpression<UMaterialExpressionOneMinus>(-350, 900);
				if (!OneMinus)
				{
					Result.Error = TEXT("Failed to create erosion one-minus.");
					return false;
				}
				if (!ConnectToInputWithFallback(DissolveAmount, TEXT(""), OneMinus, {TEXT("Input"), TEXT("")}))
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
				// DepthFade input pin is "Opacity" via GetInputName(0), not the UPROPERTY name InOpacity
				// [VERIFIED: MaterialExpressionDepthFade.h:39-44; MaterialEditingLibrary.cpp:68-72].
				if (!ConnectToInputWithFallback(
						OpacityChain,
						TEXT(""),
						DepthFadeExpr,
						{TEXT("Opacity"), TEXT("InOpacity"), TEXT("")})
					|| !ConnectToInputWithFallback(
						DepthFadeParam,
						TEXT(""),
						DepthFadeExpr,
						{TEXT("FadeDistance"), TEXT("")}))
				{
					Result.Error = TEXT("Failed to wire depth_fade.");
					return false;
				}
				OpacityChain = DepthFadeExpr;
				Result.WiredFeatures.Add(TEXT("depth_fade"));
			}

			// Additive VFX masters consume renderer alpha through MP_Opacity for every purpose.
			// Particle Color exposes a named A output [VERIFIED: MaterialExpressions.cpp:10073-10079].
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
	bool bTrailPurpose,
	bool bTranslucentBlend)
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
	if (!Builder.Build(Result, bTranslucentBlend))
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
