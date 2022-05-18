#pragma once
#include <ctime>
#include <Windows.h>
#include <winbase.h>
#include <iomanip>

enum color : int {
	normal = 7,
	failure = 4,
	success = 2
};

/**
*   GetCurrentSystemTime - Gets actual system time
*   @timeInfo: Reference to your own tm variable, gets modified.
*/
static void GetCurrentSystemTime( tm& timeInfo ) {
	const std::chrono::system_clock::time_point systemNow = std::chrono::system_clock::now( );
	std::time_t now_c = std::chrono::system_clock::to_time_t( systemNow );
	localtime_s( &timeInfo, &now_c ); // using localtime_s as std::localtime is not thread-safe.
};

namespace console {
	void heck( int color, std::string out ) {
		static HANDLE hConsole = GetStdHandle( STD_OUTPUT_HANDLE );
		SetConsoleTextAttribute( hConsole, color );
		std::cout << out << std::endl;
		SetConsoleTextAttribute( hConsole, normal );
	}

	void print( int color, std::string out, bool newline ) {
		static HANDLE hConsole = GetStdHandle( STD_OUTPUT_HANDLE );
		SetConsoleTextAttribute( hConsole, color );
		if ( color != normal ) {
			std::cout << out << ( newline ? "\n" : "" );
		} else {
			tm timeInfo{ };
			GetCurrentSystemTime( timeInfo );
			std::stringstream ss_time; // Temp stringstream to keep things clean
			ss_time << std::put_time(&timeInfo, "[%F %T] ");

			std::string print_text = ss_time.str( );
			print_text.append( out );
			std::cout << print_text << ( newline ? "\n" : "" );
		}

		SetConsoleTextAttribute( hConsole, normal );
	}

	void set_title_message(const char* msg, ...) {
		tm timeInfo{ };
		GetCurrentSystemTime(timeInfo);

		va_list va_alist;
		char logBuf[1024] = { 0 };

		std::stringstream ss_time; // Temp stringstream to keep things clean
		ss_time << std::put_time(&timeInfo, "[%T] ");

		va_start(va_alist, msg);
		_vsnprintf(logBuf + strlen(logBuf), sizeof(logBuf) - strlen(logBuf), msg, va_alist);
		va_end(va_alist);

		std::string print_text = ss_time.str();
		print_text.append(logBuf);

		SetConsoleTitle(print_text.c_str());
	}

	void print_success( bool newline, const char* str, ... ) {
		tm timeInfo{ };
		GetCurrentSystemTime( timeInfo );

		va_list va_alist;
		char logBuf[ 1024 ] = { 0 };

		std::stringstream ss_time; // Temp stringstream to keep things clean
		ss_time << std::put_time( &timeInfo, "[%F %T] " );

		va_start( va_alist, str );
		_vsnprintf( logBuf + strlen( logBuf ), sizeof( logBuf ) - strlen( logBuf ), str, va_alist );
		va_end( va_alist );

		std::string print_text = ss_time.str( );
		print_text.append( logBuf );

		print( success, print_text, newline );
	}

	void print_failure( bool newline, const char* str, ... ) {
		tm timeInfo{ };
		GetCurrentSystemTime( timeInfo );

		va_list va_alist;
		char logBuf[ 1024 ] = { 0 };

		std::stringstream ss_time; // Temp stringstream to keep things clean
		ss_time << std::put_time(&timeInfo, "[%F %T] ");

		va_start( va_alist, str );
		_vsnprintf( logBuf + strlen( logBuf ), sizeof( logBuf ) - strlen( logBuf ), str, va_alist );
		va_end( va_alist );

		std::string print_text = ss_time.str( );
		print_text.append( logBuf );

		print( failure, print_text, newline );
	}

}
