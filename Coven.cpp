/*
 * "WitchLord"
 * by Wipe/Rotators
 *
 * Global functions:
 *	void crash()		-- crashes application
 *	void callstack()	-- prints callstack of current context
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <angelscript.h>

using namespace std;

extern void PrintCallstack( bool crash );

void Crash()
{
	asIScriptEngine* crash = NULL;
	crash->SetUserData( NULL, asDWORD(NULL) );
}

void Callstack()
{
	printf( "Script callstack:\n" );
	asIScriptContext* ctx = asGetActiveContext();
	if( ctx )
	{
		int len = ctx->GetCallstackSize();
		for( int c=0; c<len; c++ )
		{
			const char* sectionName;
			int column;
			int line = ctx->GetLineNumber( c, &column, &sectionName );
			asIScriptFunction* function = ctx->GetFunction(c);
			if( function )
				printf( " [%s (%d,%d)] %s\n", sectionName, line, column,
					function->GetDeclaration( true, strlen( function->GetNamespace()) > 0 ? true : false )
				);
			else
				printf( " [???]\n" );
		}
	}
	else
		printf( " [???] Not available\n" );
}

#define ASSERT(x,y)  x = y; assert( x >= 0 )

void Coven( asIScriptEngine* engine )
{
	int r = 0;
    ASSERT( r, engine->RegisterGlobalFunction( "void crash()", asFUNCTION( Crash ), asCALL_CDECL ));
    ASSERT( r, engine->RegisterGlobalFunction( "void callstack()", asFUNCTION( Callstack ), asCALL_CDECL ));
}
