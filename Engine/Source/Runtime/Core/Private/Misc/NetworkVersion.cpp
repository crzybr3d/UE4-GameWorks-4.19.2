// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/NetworkVersion.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Misc/NetworkGuid.h"

DEFINE_LOG_CATEGORY( LogNetVersion );

FNetworkVersion::FGetLocalNetworkVersionOverride FNetworkVersion::GetLocalNetworkVersionOverride;
FNetworkVersion::FIsNetworkCompatibleOverride FNetworkVersion::IsNetworkCompatibleOverride;

FString FNetworkVersion::ProjectVersion;

enum EEngineNetworkVersionHistory
{
	HISTORY_INITIAL					= 1,
	HISTORY_REPLAY_BACKWARDS_COMPAT	= 2,		// Bump version to get rid of older replays before backwards compat was turned on officially
};

bool FNetworkVersion::bHasCachedNetworkChecksum			= false;
uint32 FNetworkVersion::CachedNetworkChecksum			= 0;

uint32 FNetworkVersion::EngineNetworkProtocolVersion	= HISTORY_REPLAY_BACKWARDS_COMPAT;
uint32 FNetworkVersion::GameNetworkProtocolVersion		= 0;

uint32 FNetworkVersion::EngineCompatibleNetworkProtocolVersion		= HISTORY_REPLAY_BACKWARDS_COMPAT;
uint32 FNetworkVersion::GameCompatibleNetworkProtocolVersion		= 0;

uint32 FNetworkVersion::GetNetworkCompatibleChangelist()
{
	//return FEngineVersion::Current().GetChangelist();
	return ENGINE_NET_VERSION;
}

uint32 FNetworkVersion::GetReplayCompatibleChangelist()
{
	return BUILT_FROM_CHANGELIST;
}

uint32 FNetworkVersion::GetEngineNetworkProtocolVersion()
{
	return EngineNetworkProtocolVersion;
}

uint32 FNetworkVersion::GetEngineCompatibleNetworkProtocolVersion()
{
	return EngineCompatibleNetworkProtocolVersion;
}

uint32 FNetworkVersion::GetGameNetworkProtocolVersion()
{
	return GameNetworkProtocolVersion;
}

uint32 FNetworkVersion::GetGameCompatibleNetworkProtocolVersion()
{
	return GameCompatibleNetworkProtocolVersion;
}

uint32 FNetworkVersion::GetLocalNetworkVersion( bool AllowOverrideDelegate /*=true*/ )
{
	if ( bHasCachedNetworkChecksum )
	{
		return CachedNetworkChecksum;
	}

	if ( AllowOverrideDelegate && GetLocalNetworkVersionOverride.IsBound() )
	{
		CachedNetworkChecksum = GetLocalNetworkVersionOverride.Execute();

		UE_LOG( LogNetVersion, Log, TEXT( "GetLocalNetworkVersionOverride: NetworkChecksum: %u" ), CachedNetworkChecksum );

		bHasCachedNetworkChecksum = true;

		return CachedNetworkChecksum;
	}

	// Get the project name (NOT case sensitive)
	const FString ProjectName( FString( FApp::GetGameName() ).ToLower() );

	// network compatible CL
	const uint32 NetworkCompatibleChangelist = GetNetworkCompatibleChangelist();

	// Start with project name+compatible changelist as seed
	CachedNetworkChecksum = FCrc::StrCrc32( *ProjectName, NetworkCompatibleChangelist);
	UE_LOG(LogNetVersion, Log, TEXT("Hash1: %u"), CachedNetworkChecksum);

	// Next, hash with project version
	CachedNetworkChecksum = FCrc::StrCrc32( *ProjectVersion, CachedNetworkChecksum );
	UE_LOG(LogNetVersion, Log, TEXT("Hash2: %u"), CachedNetworkChecksum);

	// Finally, hash with engine/game network version
	const uint32 EngineNetworkVersion	= FNetworkVersion::GetEngineNetworkProtocolVersion();
	const uint32 GameNetworkVersion		= FNetworkVersion::GetGameNetworkProtocolVersion();

	uint32 OldChecksum = CachedNetworkChecksum;
	CachedNetworkChecksum = FCrc::MemCrc32( &EngineNetworkVersion, sizeof( EngineNetworkVersion ), CachedNetworkChecksum );
	UE_LOG(LogNetVersion, Log, TEXT("EngineNetworkVersion: Hashed value %d at 0x%08X of %d bytes with %u into %u"), EngineNetworkVersion, &EngineNetworkVersion, sizeof(EngineNetworkVersion), OldChecksum, CachedNetworkChecksum);

	uint32 OldChecksum2 = CachedNetworkChecksum;
	CachedNetworkChecksum = FCrc::MemCrc32( &GameNetworkVersion, sizeof( GameNetworkVersion ), CachedNetworkChecksum );
	UE_LOG(LogNetVersion, Log, TEXT("GameNetworkVersion: Hashed value %d at 0x%08X of %d bytes with %u into %u"), GameNetworkVersion, &GameNetworkVersion, sizeof(GameNetworkVersion), OldChecksum2, CachedNetworkChecksum);

	UE_LOG( LogNetVersion, Log, TEXT( "GetNetworkCompatibleChangelist: %u, ProjectName: '%s', ProjectVersion: '%s', EngineNetworkVersion: %i, GameNetworkVersion: %i, NetworkChecksum: %u" ), 
		NetworkCompatibleChangelist, *ProjectName, *ProjectVersion, EngineNetworkVersion, GameNetworkVersion, CachedNetworkChecksum );


	for (int i = 0; i < 8; i++)
	{
		for (int j = 0; j < 16; j++)
		{
			FString Log;

			for (int k = 0; k < 16; k++)
			{
				uint32 Value = FCrc::CRCTablesSB8[i][j * 16 + k];

				if (Log.Len() > 0)
				{
					Log += TEXT(", ");
				}

				Log += FString::Printf(TEXT("0x%08X"), Value);				
			}

			UE_LOG(LogNetVersion, Log, TEXT("CRC[%d][%d]: %s"),  i, j, *Log);
		}
	}

	bHasCachedNetworkChecksum = true;

	return CachedNetworkChecksum;
}

bool FNetworkVersion::IsNetworkCompatible( const uint32 LocalNetworkVersion, const uint32 RemoteNetworkVersion )
{
	if ( IsNetworkCompatibleOverride.IsBound() )
	{
		return IsNetworkCompatibleOverride.Execute( LocalNetworkVersion, RemoteNetworkVersion );
	}

	return LocalNetworkVersion == RemoteNetworkVersion;
}

FNetworkReplayVersion FNetworkVersion::GetReplayVersion()
{
	const uint32 ReplayVersion = ( GameCompatibleNetworkProtocolVersion << 16 ) | EngineCompatibleNetworkProtocolVersion;

	return FNetworkReplayVersion( FApp::GetGameName(), ReplayVersion, GetReplayCompatibleChangelist() );
}
