// Copyright © 2025 CCP ehf.

#pragma once

#include <cctype>
#include <filesystem>

// Windows file APIs resolve an existing file regardless of path case.
// On Linux, use the on-disk name when one already matches.
inline std::filesystem::path PreferExistingPathCase( const std::filesystem::path& requested )
{
	namespace fs = std::filesystem;
	if( requested.empty() || fs::exists( requested ) )
	{
		return requested;
	}

	fs::path resolved;
	for( const auto& part : requested )
	{
		const fs::path next = resolved.empty() ? fs::path( part ) : resolved / part;
		if( fs::exists( next ) )
		{
			resolved = next;
			continue;
		}
		if( resolved.empty() || !fs::is_directory( resolved ) )
		{
			return requested;
		}
		bool found = false;
		for( const auto& entry : fs::directory_iterator( resolved ) )
		{
			const auto have = entry.path().filename().string();
			const auto want = part.string();
			if( have.size() == want.size() && std::equal( have.begin(), have.end(), want.begin(), []( unsigned char a, unsigned char b ) { return std::tolower( a ) == std::tolower( b ); } ) )
			{
				resolved = entry.path();
				found = true;
				break;
			}
		}
		if( !found )
		{
			return requested;
		}
	}
	return resolved;
}
