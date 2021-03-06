/*
 Copyright (c) 2014, Richard Eakin - All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are permitted provided
 that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this list of conditions and
 the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
 the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#include "ShaderPreprocessor.h"
#include "cinder/app/App.h"
#include "cinder/Utilities.h"
#include "cinder/Log.h"

#include <regex>

#define ENABLE_PROFILING 1
#define ENABLE_CACHING 0

#if ENABLE_PROFILING
#include "cinder/Timer.h"
#endif

using namespace ci;
using namespace std;

namespace {
	const regex sIncludeRegex = regex( "^[ ]*#[ ]*include[ ]+[\"<](.*)[\">].*" );
} // anonymous namespace

ShaderPreprocessor::ShaderPreprocessor()
{
	init();
}

void ShaderPreprocessor::init()
{
	mSearchPaths.push_back( app::getAssetPath( "" ) );
}

string ShaderPreprocessor::parse( const fs::path &path )
{
	set<fs::path> includeTree;

#if ENABLE_PROFILING
	Timer timer( true );
	string result = parseRecursive( path, fs::path(), includeTree );
	CI_LOG_I( "parse " << path << " complete, ms: " << timer.getSeconds() * 1000 );
	return result;
#else
	return parseRecursive( path, fs::path(), includeTree );
#endif
}

string ShaderPreprocessor::parseRecursive( const fs::path &path, const ci::fs::path &parentPath, set<fs::path> &includeTree )
{
	if( includeTree.count( path ) )
		throw ShaderPreprocessorExc( "circular include found, path: " + path.string() );

	includeTree.insert( path );

	const fs::path fullPath = findFullPath( path, parentPath );

#if ENABLE_CACHING
	const time_t timeLastWrite = fs::last_write_time( fullPath );

	auto cachedIt = mCachedSources.find( path );
	if( cachedIt != mCachedSources.end() ) {
		if( cachedIt->second.mTimeLastWrite >= timeLastWrite ) {
			return cachedIt->second.mString;
		}
	}
#endif

	stringstream output;

	ifstream input( fullPath.c_str() );
	if( ! input.is_open() )
		throw ShaderPreprocessorExc( "Failed to open file at path: " + fullPath.string() );

	// go through each line and process includes

	string line;
	smatch matches;

	size_t lineNumber = 1;

	while( getline( input, line ) ) {
		if( regex_search( line, matches, sIncludeRegex ) ) {
			output << parseRecursive( matches[1].str(), fullPath.parent_path(), includeTree );
			output << "#line " << lineNumber << endl;
		}
		else
			output << line;

		output << endl;
		lineNumber++;
	}

	input.close();

#if ENABLE_CACHING
	Source &source = mCachedSources[path];
	source.mTimeLastWrite = timeLastWrite;
	source.mString = output.str();
	
	return source.mString;
#else
	return output.str();
#endif
}

fs::path ShaderPreprocessor::findFullPath( const fs::path &path, const ci::fs::path &parentPath )
{
	auto fullPath = parentPath / path;
	if( fs::exists( fullPath ) )
		return fullPath;

	for( const auto &searchPath : mSearchPaths ) {
		fullPath = searchPath / path;
		if( fs::exists( fullPath ) )
			return fullPath;
	}

	throw ShaderPreprocessorExc( "could not find shader with include path: " + path.string() );
}
