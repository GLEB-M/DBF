#ifndef _DB_UTILS_
#define _DB_UTILS_

#include <windows.h>
#include <algorithm> 
#include <string>
#include <vector>
#include <set>

using namespace std;

namespace DBUtils
{
	// Дата
	struct date_t
	{
		unsigned char  day;
		unsigned char  month;
		unsigned short year;
	};

	// Дата/Время
	struct date_time_t
	{
		unsigned char  day;
		unsigned char  month;
		unsigned short year;

		unsigned char  hours;
		unsigned char  minutes;
		unsigned char  seconds;
	};
	
	DWORD FileOpenA( const char *szFileName, bool bWriteEnable, bool bExclusive, HANDLE *hFile );
	void  FileClose( HANDLE hFile );
	DWORD FileRead( HANDLE hFile, BYTE *pBuffer, DWORD dwSize, DWORD *lpRead );
	DWORD FileWrite( HANDLE hFile, BYTE *pBuffer, DWORD dwSize, DWORD *lpWritten );
	DWORD FileSeek( HANDLE hFile, LONG lPosition, DWORD dwOrigin );
	DWORD FileSize( HANDLE hFile );
	BOOL  FileLock( HANDLE hFile, DWORD dwOffset, DWORD dwSize );
	BOOL  FileUnlock( HANDLE hFile, DWORD dwOffset, DWORD dwSize );
	BOOL  FileExistsA( const char *szFileName );
	DWORD WriteBuffer( HANDLE hFile, unsigned int nOffset, BYTE *pBuffer, unsigned int nSize );

	string         IntToStr( int number );
	date_t         ConvertToDate( unsigned char *data );
	date_time_t    ConvertToDateTime( unsigned char *data );
	unsigned short SwapBytes16( unsigned short data );
	unsigned int   SwapBytes32( unsigned int data );

	string ErrorMessage( DWORD dwErrorCode );
}

#endif
