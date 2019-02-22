#include "db_utils.h"

DWORD DBUtils::FileOpenA( const char *szFileName, bool bWriteEnable, bool bExclusive, HANDLE *hFile )
{
	DWORD dwAccess    = GENERIC_READ | (bWriteEnable ? GENERIC_WRITE : 0);
	DWORD dwShareMode = bExclusive ? 0 : (FILE_SHARE_READ | FILE_SHARE_WRITE);

	if ( (*hFile = CreateFileA(szFileName, dwAccess, dwShareMode, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) != INVALID_HANDLE_VALUE )
		return ERROR_SUCCESS;

	return GetLastError();
}

void DBUtils::FileClose( HANDLE hFile )
{
	CloseHandle(hFile);
}

DWORD DBUtils::FileRead( HANDLE hFile, BYTE *pBuffer, DWORD dwSize, DWORD *lpRead )
{
	if ( ReadFile(hFile, pBuffer, dwSize, lpRead, NULL) )
		return ERROR_SUCCESS;

	return GetLastError();
}

DWORD DBUtils::FileWrite( HANDLE hFile, BYTE *pBuffer, DWORD dwSize, DWORD *lpWritten )
{
	if ( WriteFile(hFile, pBuffer, dwSize, lpWritten, NULL) )
		return ERROR_SUCCESS;

	return GetLastError();
}

DWORD DBUtils::FileSeek( HANDLE hFile, LONG lPosition, DWORD dwOrigin )
{
	return SetFilePointer(hFile, lPosition, NULL, dwOrigin);
}

DWORD DBUtils::FileSize( HANDLE hFile )
{
	return GetFileSize(hFile, NULL);
}

BOOL DBUtils::FileLock( HANDLE hFile, DWORD dwOffset, DWORD dwSize )
{
	return LockFile(hFile, dwOffset, 0, dwSize, 0);
}

BOOL DBUtils::FileUnlock( HANDLE hFile, DWORD dwOffset, DWORD dwSize )
{
	return UnlockFile(hFile, dwOffset, 0, dwSize, 0);
}

BOOL DBUtils::FileExistsA( const char *szFileName )
{
	HANDLE hFile = CreateFileA(szFileName, 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if ( hFile != INVALID_HANDLE_VALUE )
	{
		CloseHandle(hFile);
		return TRUE;
	}

	return FALSE;
}

DWORD DBUtils::WriteBuffer( HANDLE hFile, unsigned int nOffset, unsigned char *pBuffer, unsigned int nSize )
{
	DWORD nCurOffset, dwError, dwWritten;

	nCurOffset = FileSeek(hFile, 0, FILE_CURRENT); // текущая позиция

	if ( FileSeek(hFile, nOffset, FILE_BEGIN) != nOffset ) // новая позиция
		return GetLastError();

	dwError = FileWrite(hFile, pBuffer, nSize, &dwWritten); // пишем
	if ( dwError != ERROR_SUCCESS || dwWritten != nSize )
	{
		dwError = GetLastError();
		FileSeek(hFile, nCurOffset, FILE_BEGIN); // восстанавливаем позицию
		return dwError;
	}

	if ( FileSeek(hFile, nCurOffset, FILE_BEGIN) != nCurOffset ) // восстанавливаем позицию
		return GetLastError();

	return ERROR_SUCCESS;
}

string DBUtils::IntToStr( int number )
{
	char buffer[10];
	sprintf_s(buffer, 10, "%d", number);
	return buffer;
}

DBUtils::date_t DBUtils::ConvertToDate( unsigned char *data )
{
	char   buffer[5];
	date_t d;
	
	buffer[0] = data[0];
	buffer[1] = data[1];
	buffer[2] = data[2];
	buffer[3] = data[3];
	buffer[4] = '\0';
	d.year = atoi(buffer);

	buffer[0] = data[4];
	buffer[1] = data[5];
	buffer[2] = '\0';
	d.month = atoi(buffer);

	buffer[0] = data[6];
	buffer[1] = data[7];
	buffer[2] = '\0';
	d.day = atoi(buffer);

	return d;
}

DBUtils::date_time_t DBUtils::ConvertToDateTime( unsigned char *data )
{
	date_time_t dt = { 0 };
	
	unsigned int date = *(unsigned int *)data;
	unsigned int time = *(unsigned int *)(data + 4);

	if ( date )
	{
		int a, b, c, d, e, m;
		
		// Преобразование из юлианской даты
		a = date + 32044;
		b = (4 * a + 3) / 146097;
		c = a - ((146097 * b) / 4);
		d = (4 * c + 3) / 1461;
		e = c - ((1461 * d) / 4);
		m = (5 * e + 2) / 153;

		dt.day   = e - ((153 * m + 2) / 5) + 1;
		dt.month = m + 3 - 12 * (m / 10);
		dt.year  = 100 * b + d - 4800 + (m / 10);
	}

	if ( time )
	{
		// Преобразование из миллисекунд
		dt.hours = time / 3600000;
		dt.minutes = (time - (dt.hours * 3600000)) / 60000;
		dt.seconds = (time - (dt.hours * 3600000) - (dt.minutes * 60000)) / 1000;
	}

	return dt;
}

unsigned short DBUtils::SwapBytes16( unsigned short data )
{
	unsigned short res;

	unsigned char *src = (unsigned char *)&data;
	unsigned char *dst = (unsigned char *)&res;

	dst[0] = src[1];
	dst[1] = src[0];

	return res;
}

unsigned int DBUtils::SwapBytes32( unsigned int data )
{
	unsigned int res;

	unsigned char *src = (unsigned char *)&data;
	unsigned char *dst = (unsigned char *)&res;

	dst[0] = src[3];
	dst[1] = src[2];
	dst[2] = src[1];
	dst[3] = src[0];

	return res;
}

string DBUtils::ErrorMessage( DWORD dwErrorCode )
{
	string sMsg;
	void *pBuffer;
	
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
		          NULL, dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&pBuffer, 0, NULL);

	sMsg = (LPCSTR)pBuffer;
	LocalFree(pBuffer);

	// убираем 13+10 в конце строки
	size_t pos = sMsg.find_last_not_of("\r\n");
	if ( pos != std::wstring::npos )
		sMsg.resize(pos+1);

	return sMsg;
}