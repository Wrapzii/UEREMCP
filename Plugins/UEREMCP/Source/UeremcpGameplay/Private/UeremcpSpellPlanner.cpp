#include "UeremcpSpellPlanner.h"

#include "Dom/JsonValue.h"
#include "Misc/PackageName.h"

namespace
{
bool RejectUnknownFields(
	const TSharedPtr<FJsonObject>& Object,
	const TSet<FString>& Allowed,
	const FString& Context,
	FString& OutError)
{
	if (!Object.IsValid())
	{
		return true;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
	{
		if (!Allowed.Contains(Pair.Key))
		{
			OutError = FString::Printf(TEXT("%s contains unknown field '%s'"), *Context, *Pair.Key);
			return false;
		}
	}
	return true;
}

bool ValidateFieldTypes(
	const TSharedPtr<FJsonObject>& Object,
	const TMap<FString, EJson>& ExpectedTypes,
	const FString& Context,
	FString& OutError)
{
	if (!Object.IsValid())
	{
		return true;
	}

	for (const TPair<FString, EJson>& Expected : ExpectedTypes)
	{
		const TSharedPtr<FJsonValue>* Value = Object->Values.Find(Expected.Key);
		if (Value && (!Value->IsValid() || (*Value)->Type != Expected.Value))
		{
			OutError = FString::Printf(
				TEXT("%s.%s has the wrong JSON type"),
				*Context,
				*Expected.Key);
			return false;
		}
	}
	return true;
}

bool RequiredString(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* Field,
	FString& OutValue,
	FString& OutError)
{
	if (!Object.IsValid() || !Object->TryGetStringField(Field, OutValue) || OutValue.IsEmpty())
	{
		OutError = FString::Printf(TEXT("specification.%s is required and must be a non-empty string"), Field);
		return false;
	}
	return true;
}

FString OptionalString(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* Field,
	const FString& DefaultValue)
{
	FString Value;
	return Object.IsValid() && Object->TryGetStringField(Field, Value) ? Value : DefaultValue;
}

double OptionalNumber(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* Field,
	double DefaultValue)
{
	double Value = DefaultValue;
	if (Object.IsValid())
	{
		Object->TryGetNumberField(Field, Value);
	}
	return Value;
}

TSharedPtr<FJsonObject> OptionalObject(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* Field)
{
	const TSharedPtr<FJsonObject>* Value = nullptr;
	if (Object.IsValid() && Object->TryGetObjectField(Field, Value) && Value)
	{
		return *Value;
	}
	return nullptr;
}

bool IsStableName(const FString& Value)
{
	if (Value.IsEmpty() || Value.Len() > 64 || !FChar::IsAlpha(Value[0]))
	{
		return false;
	}
	for (const TCHAR Character : Value)
	{
		if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
		{
			return false;
		}
	}
	return true;
}

FString StableLineIdFromElement(const FString& Element)
{
	FString Result;
	bool bPreviousUnderscore = false;
	for (const TCHAR Character : Element)
	{
		const bool bIsNameCharacter = FChar::IsAlnum(Character);
		const TCHAR Output = bIsNameCharacter ? FChar::ToLower(Character) : TEXT('_');
		if (Output != TEXT('_') || !bPreviousUnderscore)
		{
			Result.AppendChar(Output);
		}
		bPreviousUnderscore = Output == TEXT('_');
	}
	while (Result.RemoveFromEnd(TEXT("_")))
	{
	}
	if (Result.IsEmpty() || !FChar::IsAlpha(Result[0]))
	{
		Result = TEXT("element_") + Result;
	}
	return Result.Left(64);
}

void AddDependencyIfPresent(
	const TSharedPtr<FJsonObject>& Presentation,
	const TCHAR* Field,
	const TCHAR* RowField,
	const TSharedPtr<FJsonObject>& RowPayload,
	TArray<FString>& Dependencies)
{
	const FString Path = OptionalString(Presentation, Field, FString());
	RowPayload->SetStringField(RowField, Path);
	if (!Path.IsEmpty())
	{
		Dependencies.AddUnique(Path);
	}
}
}

bool FUeremcpSpellPlanner::BuildPlan(
	const TSharedPtr<FJsonObject>& Specification,
	FUeremcpSpellPlan& OutPlan,
	FString& OutError)
{
	OutPlan = FUeremcpSpellPlan();
	OutError.Reset();

	if (!Specification.IsValid())
	{
		OutError = TEXT("create_spell requires a specification object");
		return false;
	}

	static const TSet<FString> TopLevelFields = {
		TEXT("name"), TEXT("row_name"), TEXT("element"), TEXT("line_id"),
		TEXT("element_color"), TEXT("tier"), TEXT("wheel"), TEXT("circle_tier"),
		TEXT("timing"), TEXT("delivery"), TEXT("impact"), TEXT("effect_tag"),
		TEXT("presentation"), TEXT("progression"), TEXT("networking"),
	};
	static const TMap<FString, EJson> TopLevelTypes = {
		{TEXT("name"), EJson::String},
		{TEXT("row_name"), EJson::String},
		{TEXT("element"), EJson::String},
		{TEXT("line_id"), EJson::String},
		{TEXT("element_color"), EJson::Array},
		{TEXT("tier"), EJson::String},
		{TEXT("wheel"), EJson::String},
		{TEXT("circle_tier"), EJson::String},
		{TEXT("timing"), EJson::Object},
		{TEXT("delivery"), EJson::Object},
		{TEXT("impact"), EJson::Object},
		{TEXT("effect_tag"), EJson::String},
		{TEXT("presentation"), EJson::Object},
		{TEXT("progression"), EJson::Object},
		{TEXT("networking"), EJson::Object},
	};
	if (!RejectUnknownFields(Specification, TopLevelFields, TEXT("specification"), OutError)
		|| !ValidateFieldTypes(Specification, TopLevelTypes, TEXT("specification"), OutError))
	{
		return false;
	}

	FString RowName;
	FString Element;
	if (!RequiredString(Specification, TEXT("row_name"), RowName, OutError)
		|| !RequiredString(Specification, TEXT("element"), Element, OutError))
	{
		return false;
	}
	if (!IsStableName(RowName))
	{
		OutError = TEXT("specification.row_name must match ^[A-Za-z][A-Za-z0-9_]{0,63}$");
		return false;
	}
	if (Element.Len() > 64)
	{
		OutError = TEXT("specification.element must not exceed 64 characters");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ColorValues = nullptr;
	if (!Specification->TryGetArrayField(TEXT("element_color"), ColorValues)
		|| !ColorValues || ColorValues->Num() != 4)
	{
		OutError = TEXT("specification.element_color must contain four numeric components");
		return false;
	}
	TArray<TSharedPtr<FJsonValue>> NormalizedColor;
	for (const TSharedPtr<FJsonValue>& Value : *ColorValues)
	{
		if (!Value.IsValid() || Value->Type != EJson::Number)
		{
			OutError = TEXT("specification.element_color must contain four numeric components");
			return false;
		}
		NormalizedColor.Add(MakeShared<FJsonValueNumber>(Value->AsNumber()));
	}

	const TSharedPtr<FJsonObject> Delivery = OptionalObject(Specification, TEXT("delivery"));
	const TSharedPtr<FJsonObject> Networking = OptionalObject(Specification, TEXT("networking"));
	if (!Delivery.IsValid() || !Networking.IsValid())
	{
		OutError = TEXT("specification.delivery and specification.networking are required objects");
		return false;
	}

	static const TSet<FString> DeliveryFields = {
		TEXT("type"), TEXT("speed"), TEXT("range"), TEXT("projectile_radius"),
		TEXT("gravity_scale"), TEXT("homing"), TEXT("spawn_entity"),
		TEXT("entity_length_cm"), TEXT("entity_thickness_cm"), TEXT("entity_height_cm"),
	};
	static const TSet<FString> NetworkingFields = {
		TEXT("pattern"), TEXT("authority"), TEXT("cast_path"),
	};
	static const TMap<FString, EJson> DeliveryTypes = {
		{TEXT("type"), EJson::String},
		{TEXT("speed"), EJson::Number},
		{TEXT("range"), EJson::Number},
		{TEXT("projectile_radius"), EJson::Number},
		{TEXT("gravity_scale"), EJson::Number},
		{TEXT("homing"), EJson::Number},
		{TEXT("spawn_entity"), EJson::String},
		{TEXT("entity_length_cm"), EJson::Number},
		{TEXT("entity_thickness_cm"), EJson::Number},
		{TEXT("entity_height_cm"), EJson::Number},
	};
	static const TMap<FString, EJson> NetworkingTypes = {
		{TEXT("pattern"), EJson::String},
		{TEXT("authority"), EJson::String},
		{TEXT("cast_path"), EJson::String},
	};
	if (!RejectUnknownFields(Delivery, DeliveryFields, TEXT("specification.delivery"), OutError)
		|| !RejectUnknownFields(Networking, NetworkingFields, TEXT("specification.networking"), OutError)
		|| !ValidateFieldTypes(Delivery, DeliveryTypes, TEXT("specification.delivery"), OutError)
		|| !ValidateFieldTypes(Networking, NetworkingTypes, TEXT("specification.networking"), OutError))
	{
		return false;
	}

	FString DeliveryType;
	if (!RequiredString(Delivery, TEXT("type"), DeliveryType, OutError))
	{
		return false;
	}
	static const TMap<FString, FString> CastTypes = {
		{TEXT("projectile"), TEXT("Projectile")},
		{TEXT("ground_target"), TEXT("GroundTarget")},
		{TEXT("self_cast"), TEXT("SelfCast")},
		{TEXT("channel_beam"), TEXT("ChannelBeam")},
	};
	const FString* CastType = CastTypes.Find(DeliveryType);
	if (!CastType)
	{
		OutError = FString::Printf(TEXT("unsupported delivery.type '%s'"), *DeliveryType);
		return false;
	}

	if (OptionalString(Networking, TEXT("pattern"), FString()) != TEXT("B")
		|| OptionalString(Networking, TEXT("authority"), FString()) != TEXT("server")
		|| OptionalString(Networking, TEXT("cast_path"), FString()) != TEXT("AuthorityCastAbility"))
	{
		OutError = TEXT("networking must declare RE Pattern B: pattern=B, authority=server, cast_path=AuthorityCastAbility");
		return false;
	}

	const double Speed = OptionalNumber(Delivery, TEXT("speed"), DeliveryType == TEXT("projectile") ? 3200.0 : 0.0);
	const double Range = OptionalNumber(Delivery, TEXT("range"), 3000.0);
	const double ProjectileRadius = OptionalNumber(Delivery, TEXT("projectile_radius"), 18.0);
	const double Homing = OptionalNumber(Delivery, TEXT("homing"), 0.0);
	if ((DeliveryType == TEXT("projectile") && Speed <= 0.0)
		|| Range < 0.0 || ProjectileRadius < 0.0 || Homing < 0.0 || Homing > 1.0)
	{
		OutError = TEXT("delivery values are invalid: projectiles require speed > 0; range/radius >= 0; homing in [0,1]");
		return false;
	}

	const FString SpawnEntity = OptionalString(Delivery, TEXT("spawn_entity"), FString());
	if (!SpawnEntity.IsEmpty() && SpawnEntity != TEXT("spell_wall") && SpawnEntity != TEXT("spell_field"))
	{
		OutError = TEXT("delivery.spawn_entity must be empty, spell_wall, or spell_field");
		return false;
	}

	const TSharedPtr<FJsonObject> Timing = OptionalObject(Specification, TEXT("timing"));
	const TSharedPtr<FJsonObject> Impact = OptionalObject(Specification, TEXT("impact"));
	const TSharedPtr<FJsonObject> Presentation = OptionalObject(Specification, TEXT("presentation"));
	const TSharedPtr<FJsonObject> Progression = OptionalObject(Specification, TEXT("progression"));

	static const TSet<FString> TimingFields = {
		TEXT("cast_time_sec"), TEXT("cooldown_sec"), TEXT("stamina_cost"), TEXT("duration_sec"),
	};
	static const TSet<FString> ImpactFields = {
		TEXT("damage"), TEXT("aoe_radius"), TEXT("status"), TEXT("status_duration"), TEXT("escalate_to"),
	};
	static const TSet<FString> PresentationFields = {
		TEXT("cast_effect"), TEXT("projectile_effect"), TEXT("impact_effect"),
		TEXT("circle_material"), TEXT("vfx_definition"), TEXT("circle_diameter_cm"),
		TEXT("audio_cast"), TEXT("audio_travel"), TEXT("audio_impact"), TEXT("audio_fail"),
	};
	static const TSet<FString> ProgressionFields = {
		TEXT("unlock_skill_node"), TEXT("min_classification"),
	};
	static const TMap<FString, EJson> TimingTypes = {
		{TEXT("cast_time_sec"), EJson::Number},
		{TEXT("cooldown_sec"), EJson::Number},
		{TEXT("stamina_cost"), EJson::Number},
		{TEXT("duration_sec"), EJson::Number},
	};
	static const TMap<FString, EJson> ImpactTypes = {
		{TEXT("damage"), EJson::Number},
		{TEXT("aoe_radius"), EJson::Number},
		{TEXT("status"), EJson::String},
		{TEXT("status_duration"), EJson::Number},
		{TEXT("escalate_to"), EJson::String},
	};
	static const TMap<FString, EJson> PresentationTypes = {
		{TEXT("cast_effect"), EJson::String},
		{TEXT("projectile_effect"), EJson::String},
		{TEXT("impact_effect"), EJson::String},
		{TEXT("circle_material"), EJson::String},
		{TEXT("vfx_definition"), EJson::String},
		{TEXT("circle_diameter_cm"), EJson::Number},
		{TEXT("audio_cast"), EJson::String},
		{TEXT("audio_travel"), EJson::String},
		{TEXT("audio_impact"), EJson::String},
		{TEXT("audio_fail"), EJson::String},
	};
	static const TMap<FString, EJson> ProgressionTypes = {
		{TEXT("unlock_skill_node"), EJson::String},
		{TEXT("min_classification"), EJson::String},
	};
	if (!RejectUnknownFields(Timing, TimingFields, TEXT("specification.timing"), OutError)
		|| !RejectUnknownFields(Impact, ImpactFields, TEXT("specification.impact"), OutError)
		|| !RejectUnknownFields(Presentation, PresentationFields, TEXT("specification.presentation"), OutError)
		|| !RejectUnknownFields(Progression, ProgressionFields, TEXT("specification.progression"), OutError)
		|| !ValidateFieldTypes(Timing, TimingTypes, TEXT("specification.timing"), OutError)
		|| !ValidateFieldTypes(Impact, ImpactTypes, TEXT("specification.impact"), OutError)
		|| !ValidateFieldTypes(Presentation, PresentationTypes, TEXT("specification.presentation"), OutError)
		|| !ValidateFieldTypes(Progression, ProgressionTypes, TEXT("specification.progression"), OutError))
	{
		return false;
	}

	const FString Status = OptionalString(Impact, TEXT("status"), TEXT("None"));
	static const TSet<FString> StatusValues = {
		TEXT("None"), TEXT("Freeze"), TEXT("ChillSlow"), TEXT("Burn"),
		TEXT("Stagger"), TEXT("Shock"), TEXT("Root"), TEXT("Bleed"),
	};
	const double StatusDuration = OptionalNumber(Impact, TEXT("status_duration"), 0.0);
	if (!StatusValues.Contains(Status))
	{
		OutError = FString::Printf(TEXT("unsupported impact.status '%s'"), *Status);
		return false;
	}
	if (Status != TEXT("None") && StatusDuration <= 0.0)
	{
		OutError = TEXT("impact.status_duration must be > 0 when impact.status is not None");
		return false;
	}

	const double CastTimeSec = OptionalNumber(Timing, TEXT("cast_time_sec"), 0.0);
	const double CooldownSec = OptionalNumber(Timing, TEXT("cooldown_sec"), 0.5);
	const double StaminaCost = OptionalNumber(Timing, TEXT("stamina_cost"), 10.0);
	const double DurationSec = OptionalNumber(Timing, TEXT("duration_sec"), 0.0);
	const double ImpactDamage = OptionalNumber(Impact, TEXT("damage"), 12.0);
	const double AoeRadius = OptionalNumber(Impact, TEXT("aoe_radius"), 0.0);
	const double EntityLengthCm = OptionalNumber(Delivery, TEXT("entity_length_cm"), 800.0);
	const double EntityThicknessCm = OptionalNumber(Delivery, TEXT("entity_thickness_cm"), 60.0);
	const double EntityHeightCm = OptionalNumber(Delivery, TEXT("entity_height_cm"), 300.0);
	const double CircleDiameterCm = OptionalNumber(Presentation, TEXT("circle_diameter_cm"), 60.0);
	if (CastTimeSec < 0.0 || CooldownSec < 0.0 || StaminaCost < 0.0 || DurationSec < 0.0
		|| ImpactDamage < 0.0 || AoeRadius < 0.0
		|| EntityLengthCm < 50.0 || EntityThicknessCm < 20.0 || EntityHeightCm < 50.0
		|| CircleDiameterCm <= 0.0)
	{
		OutError = TEXT("timing, impact, entity dimensions, or circle diameter violates create_spell numeric bounds");
		return false;
	}

	const FString EscalateTo = OptionalString(Impact, TEXT("escalate_to"), FString());
	const FString UnlockSkillNode =
		OptionalString(Progression, TEXT("unlock_skill_node"), FString());
	if ((!EscalateTo.IsEmpty() && !IsStableName(EscalateTo))
		|| (!UnlockSkillNode.IsEmpty() && !IsStableName(UnlockSkillNode)))
	{
		OutError = TEXT("impact.escalate_to and progression.unlock_skill_node must be stable RE identifiers");
		return false;
	}
	if (OptionalString(Specification, TEXT("effect_tag"), FString()).Len() > 64)
	{
		OutError = TEXT("specification.effect_tag must not exceed 64 characters");
		return false;
	}

	const FString Tier = OptionalString(Specification, TEXT("tier"), TEXT("S"));
	const FString Wheel = OptionalString(Specification, TEXT("wheel"), TEXT("Q"));
	const FString CircleTier = OptionalString(Specification, TEXT("circle_tier"), TEXT("circle_hand"));
	static const TSet<FString> Tiers = {TEXT("S"), TEXT("M"), TEXT("L"), TEXT("XL")};
	static const TSet<FString> Wheels = {TEXT("Q"), TEXT("E"), FString()};
	static const TSet<FString> CircleTiers = {
		TEXT("circle_hand"), TEXT("circle_personal"), TEXT("circle_body"),
		TEXT("circle_ground"), TEXT("circle_arena"),
	};
	if (!Tiers.Contains(Tier) || !Wheels.Contains(Wheel) || !CircleTiers.Contains(CircleTier))
	{
		OutError = TEXT("tier, wheel, or circle_tier is outside the RE contract");
		return false;
	}

	const FString LineId =
		OptionalString(Specification, TEXT("line_id"), StableLineIdFromElement(Element));
	if (!IsStableName(LineId))
	{
		OutError = TEXT("specification.line_id must match ^[A-Za-z][A-Za-z0-9_]{0,63}$");
		return false;
	}

	const TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
	Row->SetStringField(TEXT("AbilityId"), RowName);
	Row->SetStringField(TEXT("DisplayName"), OptionalString(Specification, TEXT("name"), RowName));
	Row->SetStringField(TEXT("LineId"), LineId);
	Row->SetStringField(TEXT("Element"), Element);
	Row->SetStringField(TEXT("Tier"), Tier);
	Row->SetStringField(TEXT("Wheel"), Wheel);
	Row->SetStringField(TEXT("CastType"), *CastType);
	Row->SetStringField(TEXT("CircleTier"), CircleTier);
	Row->SetArrayField(TEXT("ElementColor"), NormalizedColor);
	Row->SetNumberField(TEXT("CastTimeSec"), CastTimeSec);
	Row->SetNumberField(TEXT("CooldownSec"), CooldownSec);
	Row->SetNumberField(TEXT("StaminaCost"), StaminaCost);
	Row->SetNumberField(TEXT("DurationSec"), DurationSec);
	Row->SetStringField(TEXT("EffectTag"), OptionalString(Specification, TEXT("effect_tag"), FString()));
	Row->SetNumberField(TEXT("Speed"), Speed);
	Row->SetNumberField(TEXT("Range"), Range);
	Row->SetNumberField(TEXT("ProjRadius"), ProjectileRadius);
	Row->SetNumberField(TEXT("GravityScale"), OptionalNumber(Delivery, TEXT("gravity_scale"), 0.0));
	Row->SetNumberField(TEXT("Homing"), Homing);
	Row->SetNumberField(TEXT("ImpactDamage"), ImpactDamage);
	Row->SetStringField(TEXT("ImpactStatus"), Status);
	Row->SetNumberField(TEXT("StatusDuration"), StatusDuration);
	Row->SetNumberField(TEXT("AoeRadius"), AoeRadius);
	Row->SetStringField(TEXT("EscalateTo"), EscalateTo);
	Row->SetStringField(TEXT("SpawnEntity"), SpawnEntity);
	Row->SetNumberField(TEXT("EntityLengthCm"), EntityLengthCm);
	Row->SetNumberField(TEXT("EntityThicknessCm"), EntityThicknessCm);
	Row->SetNumberField(TEXT("EntityHeightCm"), EntityHeightCm);
	Row->SetNumberField(TEXT("CircleDiameterCm"), CircleDiameterCm);
	Row->SetStringField(TEXT("UnlockSkillNode"), UnlockSkillNode);
	Row->SetStringField(TEXT("MinClassification"), OptionalString(Progression, TEXT("min_classification"), TEXT("Learner")));

	AddDependencyIfPresent(Presentation, TEXT("cast_effect"), TEXT("CastNS"), Row, OutPlan.DependencyAssetPaths);
	AddDependencyIfPresent(Presentation, TEXT("projectile_effect"), TEXT("ProjectileNS"), Row, OutPlan.DependencyAssetPaths);
	AddDependencyIfPresent(Presentation, TEXT("impact_effect"), TEXT("ImpactNS"), Row, OutPlan.DependencyAssetPaths);
	AddDependencyIfPresent(Presentation, TEXT("circle_material"), TEXT("CircleMaterial"), Row, OutPlan.DependencyAssetPaths);
	AddDependencyIfPresent(Presentation, TEXT("vfx_definition"), TEXT("VFXDefinition"), Row, OutPlan.DependencyAssetPaths);
	AddDependencyIfPresent(Presentation, TEXT("audio_cast"), TEXT("AudioCueCast"), Row, OutPlan.DependencyAssetPaths);
	AddDependencyIfPresent(Presentation, TEXT("audio_travel"), TEXT("AudioCueTravel"), Row, OutPlan.DependencyAssetPaths);
	AddDependencyIfPresent(Presentation, TEXT("audio_impact"), TEXT("AudioCueImpact"), Row, OutPlan.DependencyAssetPaths);
	AddDependencyIfPresent(Presentation, TEXT("audio_fail"), TEXT("AudioCueFail"), Row, OutPlan.DependencyAssetPaths);

	OutPlan.RowName = RowName;
	OutPlan.RowPayload = Row;
	OutPlan.StaticChecks = {
		TEXT("row_identity_matches_ability_id"),
		TEXT("pattern_b_declared"),
		TEXT("authority_cast_path_declared"),
		TEXT("delivery_parameters_valid"),
		TEXT("impact_status_duration_consistent"),
		TEXT("gameplay_tag_ini_untouched"),
	};
	return true;
}

bool FUeremcpSpellPlanner::BuildTableWritePlan(
	const FString& TargetPackagePath,
	const FString& Mode,
	bool bDryRun,
	const FUeremcpSpellPlan& SpellPlan,
	FUeremcpAbilityTableWritePlan& OutWritePlan,
	FString& OutError)
{
	OutWritePlan = FUeremcpAbilityTableWritePlan();
	OutError.Reset();

	if (!FPackageName::IsValidLongPackageName(TargetPackagePath))
	{
		OutError = TEXT("target.asset_path must be a valid long package name");
		return false;
	}
	if (!TargetPackagePath.StartsWith(TEXT("/Game/__UeremcpTests/")))
	{
		OutError = TEXT("ability DataTable writes are restricted to /Game/__UeremcpTests/");
		return false;
	}
	if (Mode != TEXT("create") && Mode != TEXT("create_or_update"))
	{
		OutError = TEXT("ability DataTable write plan supports create or create_or_update only");
		return false;
	}
	if (SpellPlan.RowName.IsEmpty() || !SpellPlan.RowPayload.IsValid())
	{
		OutError = TEXT("ability DataTable write plan requires a valid spell row plan");
		return false;
	}

	const FString AssetName = FPackageName::GetLongPackageAssetName(TargetPackagePath);
	if (AssetName.IsEmpty())
	{
		OutError = TEXT("target.asset_path must end in a DataTable asset name");
		return false;
	}

	OutWritePlan.TablePackagePath = TargetPackagePath;
	OutWritePlan.TableObjectPath =
		FString::Printf(TEXT("%s.%s"), *TargetPackagePath, *AssetName);
	OutWritePlan.RowStructPath = TEXT("/Script/RE.REAbilityDef");
	OutWritePlan.RowName = SpellPlan.RowName;
	OutWritePlan.Mode = Mode;
	OutWritePlan.bDryRun = bDryRun;
	OutWritePlan.OrderedSteps = {
		TEXT("acquire_shared_mutator"),
		TEXT("enter_content_sandbox"),
		TEXT("load_or_create_freabilitydef_table"),
		TEXT("check_mode_and_expected_revision"),
		TEXT("compare_existing_row"),
		TEXT("upsert_single_row_if_changed"),
		TEXT("save_table"),
		TEXT("reread_and_compare_normalized_row"),
		bDryRun ? TEXT("discard_dry_run") : TEXT("persist_sandbox"),
		TEXT("release_shared_mutator"),
	};
	return true;
}
