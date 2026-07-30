// UEREMCP — Security module umbrella (ADR-0010).
//
// Domains should prefer FUeremcpMutatingDispatch (UeremcpCore / WS-03) for mutators.
// Include this header only when calling Security primitives directly (tests, Core).
//
// UeremcpSecurity is NOT a UToolsetDefinition and does NOT call
// UToolsetRegistry::RegisterToolsetClass — it is an application-layer policy library
// [VERIFIED: Plugins/UEREMCP/Source/UeremcpSecurity/Private/UeremcpSecurityModule.cpp].

#pragma once

#include "UeremcpAuditLog.h"
#include "UeremcpMutatorQueue.h"
#include "UeremcpPathPolicy.h"
#include "UeremcpPermissionPolicy.h"
#include "UeremcpPermissionTier.h"
#include "UeremcpSecurityDomainAdoption.h"
#include "UeremcpSecuritySettings.h"
#include "UeremcpSecurityTypes.h"
