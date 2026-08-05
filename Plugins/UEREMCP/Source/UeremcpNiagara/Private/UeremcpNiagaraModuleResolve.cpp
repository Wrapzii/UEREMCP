// UEREMCP — module / renderer path resolution for custom emitter stacks (WS-07).

#include "UeremcpNiagaraModuleResolve.h"

#include "Misc/PackageName.h"
#include "NiagaraLightRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraScript.h"
#include "NiagaraSpriteRendererProperties.h"
#include "UObject/SoftObjectPath.h"

namespace UeremcpNiagaraModuleResolve
{
	namespace
	{
		UObject* LoadSoftPath(const FString& SoftPath)
		{
			if (SoftPath.IsEmpty())
			{
				return nullptr;
			}
			UObject* Loaded = FSoftObjectPath(SoftPath).TryLoad();
			if (Loaded)
			{
				return Loaded;
			}
			const FString AssetName = FPackageName::GetLongPackageAssetName(SoftPath);
			if (!AssetName.IsEmpty())
			{
				return FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *SoftPath, *AssetName)).TryLoad();
			}
			return nullptr;
		}

		FString CanonicalModuleKey(const FString& In)
		{
			FString Key = In;
			Key.ReplaceInline(TEXT(" "), TEXT(""));
			Key.ReplaceInline(TEXT("_"), TEXT(""));
			Key.ReplaceInline(TEXT("-"), TEXT(""));
			return Key.ToLower();
		}
	}

	bool ResolvePrimitiveId(
		const FString& PrimitiveId,
		FString& OutPath,
		FString& OutDisplayName,
		FString& OutError)
	{
		OutDisplayName = PrimitiveId;
		return ResolveModuleAssetPath(PrimitiveId, FString(), OutPath, OutError);
	}

	bool ResolveModuleAssetPath(
		const FString& NameOrAlias,
		const FString& ExplicitAssetPath,
		FString& OutPath,
		FString& OutError)
	{
		OutPath.Reset();
		OutError.Reset();

		if (!ExplicitAssetPath.IsEmpty())
		{
			OutPath = ExplicitAssetPath;
			return true;
		}

		if (NameOrAlias.IsEmpty())
		{
			OutError = TEXT(
				"module requires primitive_id or name and/or asset_path (module_script).");
			return false;
		}

		// Already a soft path?
		if (NameOrAlias.StartsWith(TEXT("/")))
		{
			OutPath = NameOrAlias;
			return true;
		}

		// Common engine modules used when building stacks on Minimal substrate.
		// Paths verified under Engine/Plugins/FX/Niagara/Content/Modules/
		// [VERIFIED: NiagaraEmitterFactoryNew.cpp AddModuleFromAssetPath examples]
		// Package soft paths — LoadSoftPath appends .AssetName when needed.
		// [VERIFIED: Engine/Plugins/FX/Niagara/Content/Modules/**/*.uasset]
		static const TMap<FString, FString> Aliases = {
			{ TEXT("emitterstate"), TEXT("/Niagara/Modules/Emitter/EmitterState") },
			{ TEXT("spawnrate"), TEXT("/Niagara/Modules/Emitter/SpawnRate") },
			{ TEXT("spawnburstinstantaneous"), TEXT("/Niagara/Modules/Emitter/SpawnBurst_Instantaneous") },
			{ TEXT("spawnburst"), TEXT("/Niagara/Modules/Emitter/SpawnBurst_Instantaneous") },
			{ TEXT("initializeparticle"), TEXT("/Niagara/Modules/Spawn/Initialization/InitializeParticle") },
			{ TEXT("systemlocation"), TEXT("/Niagara/Modules/Spawn/Location/SystemLocation") },
			{ TEXT("addvelocity"), TEXT("/Niagara/Modules/Spawn/Velocity/AddVelocity") },
			{ TEXT("updateage"), TEXT("/Niagara/Modules/Update/Life/UpdateAge") },
			{ TEXT("color"), TEXT("/Niagara/Modules/Update/Color/Color") },
			{ TEXT("particlestate"), TEXT("/Niagara/Modules/Update/Life/ParticleState") },
			{ TEXT("solveforcesandvelocity"), TEXT("/Niagara/Modules/Solvers/SolveForcesAndVelocity") },
			{ TEXT("gravityforce"), TEXT("/Niagara/Modules/Update/Forces/GravityForce") },
			{ TEXT("drag"), TEXT("/Niagara/Modules/Update/Forces/Drag") },
			{ TEXT("scalecolor"), TEXT("/Niagara/Modules/Update/Color/ScaleColor") },
			{ TEXT("scalesprite"), TEXT("/Niagara/Modules/Update/Size/ScaleSpriteSize") },
			{ TEXT("scalespritesize"), TEXT("/Niagara/Modules/Update/Size/ScaleSpriteSize") },
		};

		const FString Key = CanonicalModuleKey(NameOrAlias);
		if (const FString* Found = Aliases.Find(Key))
		{
			OutPath = *Found;
			return true;
		}

		OutError = FString::Printf(
			TEXT("Unknown module '%s' — pass asset_path to a /Niagara/Modules/… UNiagaraScript "
				 "(short-name catalog covers SpawnRate, InitializeParticle, EmitterState, "
				 "UpdateAge, Color, ParticleState, AddVelocity, SolveForcesAndVelocity, …). "
				 "Custom HLSL / NiagaraScriptGraph authorship is NOT supported."),
			*NameOrAlias);
		return false;
	}

	UNiagaraScript* LoadModuleScript(const FString& SoftPath, FString& OutError)
	{
		OutError.Reset();
		UNiagaraScript* Script = Cast<UNiagaraScript>(LoadSoftPath(SoftPath));
		if (!Script)
		{
			OutError = FString::Printf(TEXT("Could not load UNiagaraScript at '%s'."), *SoftPath);
			return nullptr;
		}
		return Script;
	}

	FString DefaultScriptUsageForModule(const FString& ModuleNameOrPath)
	{
		const FString Key = CanonicalModuleKey(
			FPackageName::GetLongPackageAssetName(ModuleNameOrPath).IsEmpty()
				? ModuleNameOrPath
				: FPackageName::GetLongPackageAssetName(ModuleNameOrPath));

		if (Key.Contains(TEXT("spawnrate"))
			|| Key.Contains(TEXT("spawnburst"))
			|| Key.Contains(TEXT("emitterstate"))
			|| Key.Contains(TEXT("emitterlifecycle")))
		{
			return TEXT("EmitterUpdateScript");
		}
		if (Key.Contains(TEXT("initialize"))
			|| Key.Contains(TEXT("systemlocation"))
			|| Key.Contains(TEXT("addvelocity"))
			|| Key.Contains(TEXT("spawnbeam"))
			|| Key.StartsWith(TEXT("initial")))
		{
			return TEXT("ParticleSpawnScript");
		}
		// Default particle update for forces / age / color / solvers.
		return TEXT("ParticleUpdateScript");
	}

	FString NormalizeScriptUsage(const FString& InUsage)
	{
		if (InUsage.IsEmpty())
		{
			return FString();
		}
		const FString Key = CanonicalModuleKey(InUsage);
		if (Key == TEXT("emitterspawn") || Key == TEXT("emitterspawnscript"))
		{
			return TEXT("EmitterSpawnScript");
		}
		if (Key == TEXT("emitterupdate") || Key == TEXT("emitterupdatescript"))
		{
			return TEXT("EmitterUpdateScript");
		}
		if (Key == TEXT("particlespawn") || Key == TEXT("particlespawnscript"))
		{
			return TEXT("ParticleSpawnScript");
		}
		if (Key == TEXT("particleupdate") || Key == TEXT("particleupdatescript"))
		{
			return TEXT("ParticleUpdateScript");
		}
		// Already canonical?
		if (InUsage.EndsWith(TEXT("Script")))
		{
			return InUsage;
		}
		return InUsage;
	}

	TSubclassOf<UNiagaraRendererProperties> ResolveRendererClass(
		const FString& RendererTypeHint,
		FString& OutError)
	{
		OutError.Reset();
		if (RendererTypeHint.IsEmpty())
		{
			return nullptr;
		}
		const FString Key = CanonicalModuleKey(RendererTypeHint);
		if (Key == TEXT("sprite") || Key == TEXT("spriterenderer"))
		{
			return UNiagaraSpriteRendererProperties::StaticClass();
		}
		if (Key == TEXT("mesh") || Key == TEXT("meshrenderer"))
		{
			return UNiagaraMeshRendererProperties::StaticClass();
		}
		if (Key == TEXT("ribbon") || Key == TEXT("ribbonrenderer"))
		{
			return UNiagaraRibbonRendererProperties::StaticClass();
		}
		if (Key == TEXT("light") || Key == TEXT("lightrenderer"))
		{
			return UNiagaraLightRendererProperties::StaticClass();
		}
		OutError = FString::Printf(
			TEXT("Unknown renderer type '%s' (supported: sprite, mesh, ribbon, light)."),
			*RendererTypeHint);
		return nullptr;
	}
}
