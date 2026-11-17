// Copyright © 2025 CCP ehf.

#pragma once

#include <cctype>
#include <filesystem>
#include <optional>
#include <vector>

// Resolve a destination that stays inside permittedRoot.
// An exact existing name wins. A single case-only match uses the on-disk
// name. An ambiguous match, a parent that escapes the root, or a symlink
// whose target leaves the root is refused.
inline std::optional<std::filesystem::path> PreferExistingPathCase( const std::filesystem::path& requested, const std::filesystem::path& permittedRoot )
{
	namespace fs = std::filesystem;
	std::error_code ec;
	if( requested.empty() || permittedRoot.empty() )
	{
		return std::nullopt;
	}

	const fs::path root = fs::weakly_canonical( permittedRoot, ec );
	if( ec )
	{
		return std::nullopt;
	}
	const fs::path canonicalRequest = fs::weakly_canonical( requested, ec );
	if( ec )
	{
		return std::nullopt;
	}

	fs::path relative = canonicalRequest.lexically_relative( root );
	if( relative.empty() )
	{
		return std::nullopt;
	}
	for( const auto& part : relative )
	{
		if( part == ".." )
		{
			return std::nullopt;
		}
	}

	auto insideRoot = [&root]( const fs::path& candidate ) {
		std::error_code local;
		const fs::path canonical = fs::weakly_canonical( candidate, local );
		if( local )
		{
			return false;
		}
		const fs::path rel = canonical.lexically_relative( root );
		if( rel.empty() )
		{
			return false;
		}
		for( const auto& part : rel )
		{
			if( part == ".." )
			{
				return false;
			}
		}
		return true;
	};

	fs::path resolved = root;
	for( const auto& part : relative )
	{
		if( part == "." || part.empty() )
		{
			continue;
		}
		const fs::path next = resolved / part;
		if( fs::is_symlink( next ) )
		{
			if( !insideRoot( next ) )
			{
				return std::nullopt;
			}
			resolved = fs::weakly_canonical( next, ec );
			if( ec )
			{
				return std::nullopt;
			}
			continue;
		}
		if( fs::exists( next ) )
		{
			if( !insideRoot( next ) )
			{
				return std::nullopt;
			}
			resolved = next;
			continue;
		}
		if( !fs::is_directory( resolved ) )
		{
			resolved = next;
			continue;
		}

		std::vector<fs::path> matches;
		for( const auto& entry : fs::directory_iterator( resolved, ec ) )
		{
			if( ec )
			{
				return std::nullopt;
			}
			const auto have = entry.path().filename().string();
			const auto want = part.string();
			const bool sameCase = have.size() == want.size() && std::equal( have.begin(), have.end(), want.begin(), []( unsigned char a, unsigned char b ) { return std::tolower( a ) == std::tolower( b ); } );
			if( sameCase )
			{
				matches.push_back( entry.path() );
			}
		}
		if( ec )
		{
			return std::nullopt;
		}
		if( matches.empty() )
		{
			resolved = next;
			continue;
		}
		if( matches.size() != 1 || !insideRoot( matches.front() ) )
		{
			return std::nullopt;
		}
		if( fs::is_symlink( matches.front() ) )
		{
			resolved = fs::weakly_canonical( matches.front(), ec );
			if( ec )
			{
				return std::nullopt;
			}
			continue;
		}
		resolved = matches.front();
	}

	if( !insideRoot( resolved ) )
	{
		return std::nullopt;
	}
	return resolved;
}
