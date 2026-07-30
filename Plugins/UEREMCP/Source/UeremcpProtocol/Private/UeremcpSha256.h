// Minimal SHA-256 (FIPS 180-4). Owned implementation so UeremcpProtocol does not
// depend on an unverified engine crypto API, and so Python tests can match byte-for-byte.
// Owner: WS-05.

#pragma once

#include "CoreMinimal.h"

namespace UeremcpSha256
{
	/** Digest length in bytes. */
	constexpr int32 DigestBytes = 32;

	/** Hash Data into OutDigest (32 bytes). */
	UEREMCPPROTOCOL_API void Hash(const uint8* Data, int64 Length, uint8 OutDigest[DigestBytes]);

	/** Lowercase hex encoding of a digest. */
	UEREMCPPROTOCOL_API FString ToHex(const uint8 Digest[DigestBytes]);
}
