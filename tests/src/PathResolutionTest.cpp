// Copyright © 2025 CCP ehf.

#include "ResourcesTestFixture.h"

#include "ExistingPath.h"

#include <fstream>

namespace
{
std::filesystem::path MakeTree()
{
	const auto root = std::filesystem::temp_directory_path() / "fe5-resources-path";
	std::filesystem::remove_all( root );
	std::filesystem::create_directories( root / "sub" );
	return root;
}

void WriteFile( const std::filesystem::path& path )
{
	std::ofstream output( path );
	output << "payload";
}

std::string Document( const std::string& first, const std::string& second, const std::string& firstMode, const std::string& secondMode )
{
	return "Group: test\nResources:\n  - Path: " + first + "\n    BinaryOperation: " + firstMode + "\n  - Path: " + second + "\n    BinaryOperation: " + secondMode + "\n";
}

void WriteText( const std::filesystem::path& path, const std::string& text )
{
	std::ofstream output( path );
	output << text;
}
}

TEST( PathResolution, ExactNameWinsOverAnotherCase )
{
	const auto root = MakeTree();
	WriteFile( root / "File.txt" );
	std::ofstream second( root / "file.txt" );
	if( !second )
	{
		GTEST_SKIP() << "This filesystem cannot hold two case-distinct names.";
	}
	second << "other";
	second.close();

	const auto exact = PreferExistingPathCase( root / "File.txt", root );
	ASSERT_TRUE( exact );
	EXPECT_EQ( "File.txt", exact->filename().string() );

	EXPECT_FALSE( PreferExistingPathCase( root / "FILE.TXT", root ) );
	std::filesystem::remove_all( root );
}

TEST( PathResolution, SingleCaseMatchUsesTheExistingName )
{
	const auto root = MakeTree();
	WriteFile( root / "File.txt" );
	const auto resolved = PreferExistingPathCase( root / "file.txt", root );
	ASSERT_TRUE( resolved );
#if defined( _WIN32 )
	EXPECT_EQ( "file.txt", resolved->filename().string() );
#else
	EXPECT_EQ( "File.txt", resolved->filename().string() );
#endif
	std::filesystem::remove_all( root );
}

TEST( PathResolution, RefusesParentAndSymlinkEscape )
{
	const auto root = MakeTree();
	WriteFile( root / "sub" / "inside.txt" );
	const auto outside = std::filesystem::temp_directory_path() / "fe5-resources-outside.txt";
	WriteFile( outside );
	std::error_code ec;
	std::filesystem::create_symlink( outside, root / "link", ec );
	if( ec )
	{
		GTEST_SKIP() << "Symlink creation is unavailable.";
	}

	EXPECT_FALSE( PreferExistingPathCase( root / ".." / "fe5-resources-outside.txt", root ) );
	EXPECT_FALSE( PreferExistingPathCase( root / "link", root ) );
	const auto inside = PreferExistingPathCase( root / "sub" / ".." / "sub" / "inside.txt", root );
	ASSERT_TRUE( inside );
	EXPECT_EQ( "inside.txt", inside->filename().string() );
	std::filesystem::remove_all( root );
	std::filesystem::remove( outside );
}

TEST_F( ResourcesTestFixture, ResourceComparisonChecksProtectedFields )
{
	const auto dir = std::filesystem::temp_directory_path() / "fe5-resources-compare";
	std::filesystem::remove_all( dir );
	std::filesystem::create_directories( dir );
	const auto gold = dir / "gold.yaml";
	const auto swapped = dir / "swapped.yaml";
	const auto mode = dir / "mode.yaml";
	const auto changed = dir / "changed.yaml";
	const auto missing = dir / "missing.yaml";
	WriteText( gold, Document( "a.bin", "b.bin", "33206", "33206" ) );
	WriteText( swapped, Document( "b.bin", "a.bin", "33188", "33279" ) );
	WriteText( mode, Document( "a.bin", "b.bin", "0", "33206" ) );
	WriteText( changed, Document( "a.bin", "c.bin", "33206", "33206" ) );
	WriteText( missing, "Group: test\nResources:\n  - Path: a.bin\n  - Path: b.bin\n    BinaryOperation: 33206\n" );

	EXPECT_TRUE( ResourceGroupContentMatches( swapped, gold ) );
	EXPECT_FALSE( ResourceGroupContentMatches( mode, gold ) );
	EXPECT_FALSE( ResourceGroupContentMatches( changed, gold ) );
	EXPECT_FALSE( ResourceGroupContentMatches( missing, gold ) );
	std::filesystem::remove_all( dir );
}
