#include "UeremcpTransportProbe.h"

#include "IModelContextProtocolModule.h"
#include "ModelContextProtocol.h"
#include "ModelContextProtocolServer.h"
#include "ModelContextProtocolSettings.h"

#include "Modules/ModuleManager.h"

FUeremcpTransportProbeResult UeremcpTransport::ProbeEpicTransport()
{
	FUeremcpTransportProbeResult Result;
	Result.Capabilities = GetStaticCapabilityFlags();

	if (!FModuleManager::Get().IsModuleLoaded(TEXT("ModelContextProtocol")))
	{
		Result.LimitationNotes.Add(
			TEXT("ModelContextProtocol module not loaded — enable plugin and restart editor."));
		return Result;
	}

	IModelContextProtocolModule* McpModule = IModelContextProtocolModule::Get();
	if (!McpModule)
	{
		Result.LimitationNotes.Add(TEXT("IModelContextProtocolModule::Get() returned nullptr."));
		return Result;
	}

	Result.bMcpModuleLoaded = true;
	Result.NegotiatedProtocolVersion = UE::ModelContextProtocol::ProtocolVersion;
	Result.ServerUrlPath = UE::ModelContextProtocol::GetServerUrlPath();
	Result.ServerPort = UE::ModelContextProtocol::GetServerPortNumber();
	Result.bAutoStartConfigured = UE::ModelContextProtocol::ShouldAutoStartServer();
	Result.ClientEndpointUrl = FString::Printf(
		TEXT("http://127.0.0.1:%u%s"), Result.ServerPort, *Result.ServerUrlPath);

	if (FModelContextProtocolServer* Server = McpModule->GetServer())
	{
		Result.bServerRunning = Server->IsServerRunning();
		if (Result.bServerRunning)
		{
			Result.ServerPort = Server->GetServerPort();
			Result.ClientEndpointUrl = FString::Printf(
				TEXT("http://127.0.0.1:%u%s"), Result.ServerPort, *Result.ServerUrlPath);
		}
	}
	else
	{
		Result.LimitationNotes.Add(
			TEXT("MCP server object not created — call ModelContextProtocol.StartServer or set bAutoStartServer."));
	}

	if (!Result.bServerRunning)
	{
		Result.LimitationNotes.Add(
			TEXT("Server not running at probe time; clients must start it via editor settings, -ModelContextProtocolStartServer, or ModelContextProtocol.StartServer."));
	}

	return Result;
}
