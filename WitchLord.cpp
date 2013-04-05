/*
 * "WitchLord"
 * by Wipe/Rotators
 *
 * Utility for checking issues with AngelScript
 * Similiar to ASCompiler, but aiming to be "clean" environment - without preprocessor
 * or any FOnline-related stuff.
 *
 * Features:
 * - loading multiple modules from source and/or bytecode
 * - if module is loaded from source, its bytecode can be saved for further tests
 * - executing specific functions after compilation
 *
 * Run without arguments to see possible options.
 *
 * PS: Don't forget to report bugs found in AngelScript! :)
 *     http://www.gamedev.net/forum/49-angelcode/
 */

#include <fstream>
#include <string>
#include <vector>

#include <stdio.h>
#include <string.h>

#include <angelscript.h>

#include "Inquisition.h"

using namespace std;

extern void Callstack();
extern void Coven( asIScriptEngine* engine );

void Message( string message )
{
	printf( message.c_str() );
}
void ErrorMessage( string message )
{
	Message( "ERROR: "+ message + "\n" );
}

int Error( string message )
{
	ErrorMessage( message );
	return( -1 );
}

class CBytecodeStream: public asIBinaryStream // Source/Script.h
{
private:
    int                   readPos;
    int                   writePos;
    vector< asBYTE > binBuf;

public:
    CBytecodeStream()
    {
        writePos = 0;
        readPos = 0;
    }
    void Write( const void* ptr, asUINT size )
    {
        if( !ptr || !size ) return;
        binBuf.resize( binBuf.size() + size );
        memcpy( &binBuf[ writePos ], ptr, size );
        writePos += size;
    }
    void Read( void* ptr, asUINT size )
    {
        if( !ptr || !size ) return;
        memcpy( ptr, &binBuf[ readPos ], size );
        readPos += size;
    }
    vector< asBYTE >& GetBuf() { return binBuf; }
};

class WitchLordStackWalker : public StackWalker
{
public:
	WitchLordStackWalker() : StackWalker()
	{
	}

	// NOTE: all original On* events (end of Inquisition.cpp) are commented!

	void OnCallstackEntry(CallstackEntryType eType, CallstackEntry &entry)
	{
		if( strcmp( entry.moduleName, "WitchLord" ))
			return;

		string path = string(entry.lineFileName);
		string file = path.substr( (path.find_last_of( "\\" ))+1 );

		if( file == "crt0.c" )
			return;

		printf( " [%s + %d, %s (%d)]\n",
			entry.name, entry.offsetFromLine, file.c_str(), entry.lineNumber );

		return;
	}
};

void ASCallback( const asSMessageInfo* msg, void* param ) // Source/ASCompiler.cpp
{
	(void)(param); // :P

    const char* type = "ERROR";
    if( msg->type == asMSGTYPE_WARNING )
        type = "WARNING";
    else if( msg->type == asMSGTYPE_INFORMATION )
        type = "INFO";

    if( msg->type != asMSGTYPE_INFORMATION )
    {
        if( msg->row )
        {
			printf( "%s(%d) : %s : %s.\n", msg->section, msg->row, type, msg->message );
        }
        else
        {
            printf( "%s : %s.\n", type, msg->message );
        }
    }
    else
    {
        printf( "%s(%d) : %s : %s.\n", msg->section, msg->row, type, msg->message );
    }
}

class InputFile
{
public:
	string source;
	string bytecode;
	asIScriptModule* module;

	InputFile()
	{
		source = bytecode = "";
		module = NULL;
	}

	bool Init( string _source, string _bytecode )
	{
		if( !_source.empty() )
		{
			source = _source;
			if( !_bytecode.empty() )
				bytecode = _bytecode;

			return( true );
		}
		else if( !_bytecode.empty() )
		{
			bytecode = _bytecode;

			return( true );
		}
		else
			Error( "Unknown input file type." );

		return( false );
	}

	bool ModuleExists( string& name, asIScriptEngine* engine )
	{
		asIScriptModule* tmp = engine->GetModule( name.c_str(), asGM_ONLY_IF_EXISTS );
		if( tmp )
		{
			ErrorMessage( "Module already created: " + name );
			return( true );
		}

		return( false );
	}

	bool Load( asIScriptEngine* engine, bool import )
	{
		if( !source.empty() )
		{
			if( ModuleExists( source, engine ))
				return( false );

			Message( "Loading source: "+source+"...\n" );
			ifstream fsource( source );
			if( !fsource )
			{
				ErrorMessage( "Cannot open source: "+source );
				return( false );
			}
			string content( (istreambuf_iterator<char>(fsource)),
							(istreambuf_iterator<char>()) );
			fsource.close();

			module = engine->GetModule( source.c_str(), asGM_CREATE_IF_NOT_EXISTS );
			if( !module )
			{
				ErrorMessage( "Cannot create module: " + source );
				return( false );
			}

			if( module->AddScriptSection( source.c_str(), content.c_str(), content.length(), 0 ) < 0 )
			{
				ErrorMessage( "Cannot add script section: " + source );
				module = NULL;
				return( false );
			}

			if( module->Build() < 0 )
			{
				ErrorMessage( "Cannot build module: " + source );
				module = NULL;
				return( false );
			}

			if( import && module->BindAllImportedFunctions() < 0 )
			{
				ErrorMessage( "Cannot bind imported functions: " + source );
				module = NULL;
				return( false );
			}

			return( true );
		}
		else if( !bytecode.empty() )
		{
			if( ModuleExists( bytecode, engine ))
				return( false );

			Message( "Loading bytecode: "+bytecode+" ...\n" );

			ifstream fbytecode( bytecode, std::ios_base::out | std::ios_base::binary );
			if( !fbytecode )
			{
				ErrorMessage( "Cannot open bytecode: "+bytecode );
				return( false );
			}

			fbytecode.seekg( 0, ios::end );
			unsigned int size = (unsigned int)fbytecode.tellg(); // yea
			char *bytes = new char[size];
			fbytecode.seekg(0, ios::beg); 
			fbytecode.read( bytes, size );
			fbytecode.close();

			CBytecodeStream binary;
			binary.Write( bytes, size );

			module = engine->GetModule( bytecode.c_str(), asGM_CREATE_IF_NOT_EXISTS );
			if( !module )
			{
				ErrorMessage( "Cannot create module: "+bytecode );
				return( false );
			}


			int error = module->LoadByteCode( &binary, false );
			if( error < 0 )
			{
				ErrorMessage( "Error loading bytecode: " + bytecode );
				printf( "ERROR %d\n", error );
				module = NULL;
				return( false );
			}

			if( import && module->BindAllImportedFunctions() < 0 )
			{
				ErrorMessage( "Cannot bind imported functions: " + bytecode );
				module = NULL;
				return( false );
			}

			return( true );
		}

		return( false );
	}

	bool SaveBytecode()
	{
		if( module && !source.empty() && !bytecode.empty() )
		{
			CBytecodeStream binary;
			if( module->SaveByteCode( &binary, false ) >= 0 )
			{
				Message( "Saving bytecode : "+source+" -> "+bytecode+"\n" );
				vector< asBYTE >& data = binary.GetBuf();

				ofstream bytes( bytecode, std::ios_base::out | std::ios_base::binary );
				for( vector<asBYTE>::const_iterator i = data.begin(); i != data.end(); ++i )
					bytes << *i;
				bytes.flush();
				bytes.close();
			}
			else
			{
				ErrorMessage( "Cannot save bytecode:" + bytecode );
				return( false );
			}
		}

		return( true );
	}
	

	bool Run( string funcDecl )
	{
		if( !module )
			return( false );

		asIScriptFunction* function = module->GetFunctionByDecl( funcDecl.c_str() );
		if( function )
		{
			Message( "Executing: "+string(module->GetName())+" @ "+
								   string(function->GetDeclaration( true ))+"\n");
			asIScriptContext* ctx = module->GetEngine()->CreateContext();
			ctx->Prepare( function );
			ctx->Execute();
			ctx->Release();

			return( true );
		}
		else
			ErrorMessage( "Function not found: "+string(module->GetName()) +" @ "+funcDecl );

		return( false );
	}
};

LONG WINAPI ExceptionDump(EXCEPTION_POINTERS* pExp ) // Source/Exception.cpp (minor)
{
	if( pExp )
	{
		printf( "---------\n" );
		printf( "%-10s ", "Exception:" );
		switch( pExp->ExceptionRecord->ExceptionCode )
		{
			#define CASE_EXCEPTION(e) case e: printf( # e ); break
			CASE_EXCEPTION( EXCEPTION_ACCESS_VIOLATION );
			CASE_EXCEPTION( EXCEPTION_DATATYPE_MISALIGNMENT );
			CASE_EXCEPTION( EXCEPTION_BREAKPOINT );
			CASE_EXCEPTION( EXCEPTION_SINGLE_STEP );
			CASE_EXCEPTION( EXCEPTION_ARRAY_BOUNDS_EXCEEDED );
			CASE_EXCEPTION( EXCEPTION_FLT_DENORMAL_OPERAND );
			CASE_EXCEPTION( EXCEPTION_FLT_DIVIDE_BY_ZERO );
			CASE_EXCEPTION( EXCEPTION_FLT_INEXACT_RESULT );
			CASE_EXCEPTION( EXCEPTION_FLT_INVALID_OPERATION );
			CASE_EXCEPTION( EXCEPTION_FLT_OVERFLOW );
			CASE_EXCEPTION( EXCEPTION_FLT_STACK_CHECK );
			CASE_EXCEPTION( EXCEPTION_FLT_UNDERFLOW );
			CASE_EXCEPTION( EXCEPTION_INT_DIVIDE_BY_ZERO );
			CASE_EXCEPTION( EXCEPTION_INT_OVERFLOW );
			CASE_EXCEPTION( EXCEPTION_PRIV_INSTRUCTION );
			CASE_EXCEPTION( EXCEPTION_IN_PAGE_ERROR );
			CASE_EXCEPTION( EXCEPTION_ILLEGAL_INSTRUCTION );
			CASE_EXCEPTION( EXCEPTION_NONCONTINUABLE_EXCEPTION );
			CASE_EXCEPTION( EXCEPTION_STACK_OVERFLOW );
			CASE_EXCEPTION( EXCEPTION_INVALID_DISPOSITION );
			CASE_EXCEPTION( EXCEPTION_GUARD_PAGE );
			CASE_EXCEPTION( EXCEPTION_INVALID_HANDLE );
			default:
				printf( "0x%0X", pExp->ExceptionRecord->ExceptionCode );
				break;
		}
		printf( "\n" );
		printf( "%-10s 0x%p\n", "Address:", pExp->ExceptionRecord->ExceptionAddress );
		printf( "%-10s 0x%0X\n", "Flags:", pExp->ExceptionRecord->ExceptionFlags );
		printf( "\n" );
	}

	Callstack();

	printf( "Program callstack:\n" );

	if( pExp )
	{
		WitchLordStackWalker callstack;
		callstack.ShowCallstack( GetCurrentThread(), pExp->ContextRecord );
	}
	else
		printf( " [???] Not available\n" );

	printf( "---------\n" );
	printf( "Crashed.\n" );

	return EXCEPTION_EXECUTE_HANDLER;
}

int main( int argc, char* argv[] )
{
	vector<InputFile> files;
	vector<string> run;
	bool import = false;
 
	if( argc < 3 )
	{
		Message( "Usage: WitchLord.exe [actions]\n\n"
				 " -s [source]               load [source]\n"
				 " -b [bytecode]             load [bytecode]\n"
				 " -sb [source] [bytecode]   load [source], save to [bytecode] before exiting\n"
				 "\n"
				 " -i                        bind imported functions after loading module\n"
				 " -run module@function      execute function after compilation -- void f()\n"
				 "                           modules names are same as filenames given\n"
				 "\n"
				 " all option can be used multiple times.\n\n"
		);
		return 0;
	}

	for( int i=1; i<argc; i++ )
	{
		InputFile file;

		if( !_stricmp( argv[i], "-s" ) && i+1 < argc )
		{
			if( file.Init( string(argv[++i]), "" ))
				files.push_back( file );
		}
		else if( !_stricmp( argv[i], "-b" ) && i+1 < argc )
		{
			if( file.Init( "", string(argv[++i]) ))
				files.push_back( file );
		}
		else if( !_stricmp( argv[i], "-sb" ) && i+2 < argc )
		{
			char* src = argv[++i];
			char* bc  = argv[++i];
			if( file.Init( string(src), string(bc) ))
				files.push_back( file );
		}
		else if( !_stricmp( argv[i], "-run" ) && i+1 < argc )
		{
			run.push_back(string(argv[++i]));
		}
		else if( !_stricmp( argv[i], "-i" ))
		{
			import = true;
		}
		else
			return( Error( "Invalid arguments." ));
	}

	if( files.size() == 0 )
		return( Error( "No input files." ));

	asIScriptEngine* engine = asCreateScriptEngine( ANGELSCRIPT_VERSION );
	if( !engine )
		return( Error( "Cannot create AngelScript engine." ));
	Message( "Using AngelScript v" ANGELSCRIPT_VERSION_STRING "\n" );

	engine->SetMessageCallback( asFUNCTION(ASCallback), NULL, asCALL_CDECL );

	Coven( engine );

	for( int f=0, fLen=files.size(); f<fLen; f++ )
	{
		if( !files[f].Load( engine, import ))
			return( -1 );
	}

	for( int f=0, fLen=files.size(); f<fLen; f++ )
	{
		if( !files[f].SaveBytecode())
			return( -1 );
	}

	SetUnhandledExceptionFilter( ExceptionDump ); 

	if( run.size() > 0 )
	{
		for( int r=0, rLen=run.size(); r<rLen; r++ )
		{
			if( run[r].empty() )
				continue;

			int pos = run[r].find_first_of( "@" );

			if( pos < 0 )
				continue;

			string moduleName = run[r].substr( 0, pos );
			string functionDecl = "void " + run[r].substr( pos + 1 ) + "()";

			if( moduleName.empty() || functionDecl.length() <= 7 )
				continue;

			bool found = false;
			for( int f=0, fLen=files.size(); f<fLen; f++ )
			{
				if( files[f].module && !strcmp( moduleName.c_str(),
					files[f].module->GetName() ))
				{
					found = true;
					files[f].Run( functionDecl );
					break;
				}
			}

			if( !found )
				ErrorMessage( "Module not found: "+moduleName );
		}
	}

	engine->Release();
	_flushall();

	printf( "Finished.\n\n" );

	return( 0 );
}
