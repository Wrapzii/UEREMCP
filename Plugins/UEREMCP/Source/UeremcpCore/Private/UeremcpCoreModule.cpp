// UEREMCP — core module.
//
// SCAFFOLD — NOT YET COMPILED. See Plugins/UEREMCP/README.md.

#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogUeremcp, Log, All);

/**
 * UEREMCP core editor module.
 *
 * OPEN QUESTION (RB-03 q4) — the most important unknown in this file:
 * does ToolsetRegistry discover UToolsetDefinition subclasses automatically via UHT
 * reflection, or must something explicitly call FToolsetRegistry::RegisterToolset?
 *
 * Evidence either way:
 *  - The ToolsetDefinition.h comment says AICallable metadata "is used both by UHT and
 *    the runtime UToolRegistry", which hints at automatic discovery
 *    [VERIFIED: $TR/.../Public/ToolsetRegistry/ToolsetDefinition.h].
 *  - But the Python path is explicit: Registration(toolset_classes).register()
 *    [VERIFIED: $TR/Content/Python/toolset_registry/registration.py].
 *  - And FToolsetRegistry exposes RegisterToolset/UnregisterToolset publicly
 *    [VERIFIED: $TR/.../Public/ToolsetRegistry/ToolsetRegistry.h].
 *
 * Resolve empirically before writing registration code. If discovery is automatic,
 * StartupModule stays empty and this comment becomes the answer for everyone else.
 */
class FUeremcpCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogUeremcp, Log, TEXT("UEREMCP core module started."));

		// TODO(WS-03, RB-03 q4): explicit registration here IF discovery is not
		// automatic. Access the registry via UToolsetRegistrySubsystem::Get(...),
		// which returns TValueOrError — handle the error case; the subsystem may be
		// absent if ToolsetRegistry failed to load.
		//
		// TODO(WS-03, RB-03 q14): once domain toolsets exist, apply
		// FToolset::SetNameFilters block patterns to hide internal primitives from
		// agents while keeping them callable internally (ADR-0002 rule 5).
	}

	virtual void ShutdownModule() override
	{
		// TODO(WS-03): unregister anything registered in StartupModule. Leaking a
		// registration across a hot reload will produce duplicate tools.
		UE_LOG(LogUeremcp, Log, TEXT("UEREMCP core module shut down."));
	}
};

IMPLEMENT_MODULE(FUeremcpCoreModule, UeremcpCore)
