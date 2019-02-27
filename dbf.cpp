#include "dbf.h"

//
// CDBFTable
//
CDBFTable::CDBFTable()
{
	memset(m_szName,         0, sizeof(m_szName));     
	memset(m_szFileName,     0, sizeof(m_szFileName));
	memset(m_szPath,         0, sizeof(m_szFileName));
	memset(m_BackLink,       0, sizeof(m_BackLink));
	memset(m_szMemoFileName, 0, sizeof(m_szMemoFileName));
	
	m_hDBFFile           = INVALID_HANDLE_VALUE;
	m_hFPTFile           = INVALID_HANDLE_VALUE;

	memset(&m_DBFHeader, 0, sizeof(m_DBFHeader));
	memset(&m_FPTHeader, 0, sizeof(m_FPTHeader));

	m_FieldsCount        = 0;

	m_RecData            = NULL;

	m_MemoData           = NULL;
	m_MemoType           = 0;
	m_MemoSize           = 0;
	m_MemoRealBlockCount = 0;

	memset(m_NoMemoData, 0, sizeof(m_NoMemoData));
	
	m_RecNo              = 0;
         
	m_LockAttemptMax     = 10; 
	m_LockAttemptWait    = 300; 

	m_nErrorCode         = 0;
}

//
// ~CDBFTable
//
CDBFTable::~CDBFTable()
{
	Close();
}

// 
// Открытие таблицы
//
bool CDBFTable::Open( const char *szFileName, bool bWrite, bool bExclusive )
{
	DWORD dwError;

	// Закрываем предыдущую открытую таблицу
	Close(); 

	// Имя таблицы, имя файла таблицы, путь к таблице
	strcpy_s(m_szPath, MAX_PATH, szFileName);  
	for ( int i = (int)strlen(m_szPath) - 1; i >= 0; --i )
	{
		if ( m_szPath[i] == '\\' || m_szPath[i] == '/' )
		{
			strcpy_s(m_szFileName, MAX_PATH, m_szPath + i + 1); 

			strcpy_s(m_szName, MAX_PATH, m_szPath + i + 1);
			for ( int n = (int)strlen(m_szName) - 1; n >= 0; --n )
			{
				if ( m_szName[n] == '.' )
				{
					m_szName[n] = '\0';
					break;
				}
			}

			m_szPath[i] = '\0';
			break;
		}
	}
	
	// Открываем
	if ( (dwError = FileOpenA(szFileName, bWrite, bExclusive, &m_hDBFFile)) != ERROR_SUCCESS )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += szFileName;
		m_sErrorMessage += " ";
		m_sErrorMessage += ErrorMessage(dwError);
		goto failure;
	}

	if ( FileSize(m_hDBFFile) < (32+32+1+263) ) // если размер менее чем 328 (заголовок+описание хотяб одного поля+признак окончания полей+ссылка на контейнер), то не рассматриваем этот файла как DBF
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += szFileName;
		m_sErrorMessage += " файл не является DBF, так как его размер менее 328 байт";
		goto failure;
	}

	// Читаем заголовок
	if ( !ReadHeader() )
		goto failure;

	// Читаем описание полей
	if ( !ReadFields() )
		goto failure;

	// Если есть MEMO поля
	if ( m_DBFHeader.flags & DBF_HAS_MEMO )
	{
		char szMemoFileName[MAX_PATH];

		strcpy_s(szMemoFileName, MAX_PATH, szFileName);
		for ( int i = (int)strlen(szMemoFileName) - 1; i >= 0; --i )
		{
			if ( szMemoFileName[i] == '.' )
			{
				// Подберем нужное расширение
				if ( !_stricmp(szMemoFileName + i + 1, "PJX") )
					strcpy_s(szMemoFileName + i + 1, 4, "PJT");
				else if ( !_stricmp(szMemoFileName + i + 1, "DBF") )
					strcpy_s(szMemoFileName + i + 1, 4, "FPT");
				else if ( !_stricmp(szMemoFileName + i + 1, "DBC") )
					strcpy_s(szMemoFileName + i + 1, 4, "DCT");
				else if ( !_stricmp(szMemoFileName + i + 1, "SCX") )
					strcpy_s(szMemoFileName + i + 1, 4, "SCT");
				else if ( !_stricmp(szMemoFileName + i + 1, "VCX") )
					strcpy_s(szMemoFileName + i + 1, 4, "VCT");
				else
				{
					m_sErrorMessage = __FUNCTION__;
					m_sErrorMessage += ": не найден соответствующий MEMO файл: ";
					m_sErrorMessage += szMemoFileName;
					goto failure;
				}
				break;
			}
		}

		// Открываем MEMO
		strcpy_s(m_szMemoFileName, MAX_PATH, szMemoFileName);
		if ( (dwError = FileOpenA(szMemoFileName, bWrite, bExclusive, &m_hFPTFile)) != ERROR_SUCCESS )
		{
			m_sErrorMessage = __FUNCTION__;
			m_sErrorMessage += ": ";
			m_sErrorMessage += szMemoFileName;
			m_sErrorMessage += " ";
			m_sErrorMessage += ErrorMessage(dwError);
			goto failure;
		}

		// Читаем MEMO заголовок
		if ( !ReadMemoHeader() )
			goto failure;
	}

	// Если есть CDX
	if ( m_DBFHeader.flags & DBF_HAS_CDX )
	{
		char szCDXFileName[MAX_PATH];
		sprintf_s(szCDXFileName, MAX_PATH, "%s\\%s.%s", m_szPath, m_szName, (m_DBFHeader.flags & DBF_IS_DBC) ? "dcx" : "cdx");
		m_CDX.Close();
		if ( !m_CDX.Open(szCDXFileName, bExclusive) )
		{
			m_sErrorMessage = __FUNCTION__;
			m_sErrorMessage += ": ";
			m_sErrorMessage += m_CDX.GetLastErrorMessage();
			goto failure;
		}
	}

	// Выделим память под данные записи
	m_RecData = new unsigned char[m_DBFHeader.rec_size]; 

	m_sErrorMessage = "";

	return true;

failure:
	Close(true);
	return false;
}

//
// Закрытие таблицы
//
void CDBFTable::Close( bool bNoClearLastError )
{
	*m_szName = '\0';
	*m_szFileName = '\0';
	*m_szPath = '\0';
	memset(&m_DBFHeader, 0, sizeof(m_DBFHeader));     
	memset(&m_FPTHeader, 0, sizeof(m_FPTHeader));      
	m_Fields.clear();     
	m_FieldsCount = 0;    
	*m_BackLink = '\0';
	m_RecNo = 0;
	m_MemoType = 0;     
	m_MemoSize = 0;    
	m_MemoRealBlockCount = 0; 
	memset(m_NoMemoData, 0, sizeof(m_NoMemoData));

	if ( m_hDBFFile != INVALID_HANDLE_VALUE )
	{
		FileClose(m_hDBFFile);
		m_hDBFFile = INVALID_HANDLE_VALUE;
	}

	if ( m_RecData )
	{
		delete [] m_RecData;
		m_RecData = NULL;
	}

	if ( m_hFPTFile != INVALID_HANDLE_VALUE )
	{
		FileClose(m_hFPTFile);
		m_hFPTFile = INVALID_HANDLE_VALUE;
	}

	if ( m_MemoData )
	{
		delete [] m_MemoData;
		m_MemoData = NULL;
	}

	m_CDX.Close();

	if ( !bNoClearLastError )
	{
		m_nErrorCode = 0;        
		m_sErrorMessage.clear(); 
	}
}

//
// Таблица имеет составной индекс
//
bool CDBFTable::HasCDX( )
{
	return (m_DBFHeader.flags & DBF_HAS_CDX);
}

// 
// Блокировка файла таблицы
//
bool CDBFTable::Lock( )
{
	if ( m_hDBFFile == INVALID_HANDLE_VALUE )
		return false; // таблица не открыта

	int attempts = m_LockAttemptMax;

	while ( true )
	{
		//
		//  Выяснил, что для атомарного доступа к таблице, Visual FoxPro блокирует 1 байт по смещению 2Гб-1, т.е. 0x7FFFFFFE.
		//  Пока заблокирован этот байт, другой экземпяр Visual FoxPro, обращающийся к таблице, ожидает блокировки.
		//  Эти блокировки делаются при любом обращении к таблице, когда она открыта в shared mode
		//  Если открыта монопольно, то этот механизм не используется.
		//
		if ( FileLock(m_hDBFFile, 0x7FFFFFFE, 1) ) 
			return true;

		if ( !attempts-- )
			break;

		Sleep(m_LockAttemptWait);
	}

	return false;
}

// 
// Снятие блокировки
//
bool CDBFTable::Unlock( )
{
	return FileUnlock(m_hDBFFile, 0x7FFFFFFE, 1) ? true : false;
}

// 
// Если указатель за пределами первой записи
//
bool CDBFTable::BeginOfFile( )
{
	return (m_RecNo < 0);
}

// 
// Если указатель за пределами последней записи
//
bool CDBFTable::EndOfFile( )
{
	return (m_RecNo >= (int)m_DBFHeader.rec_count);
}

// 
// Установка указателя на первую запись
//
bool CDBFTable::GoTop( )
{
	return ReadHeader() && Set(0);
}

// 
// Установка указателя на последнюю запись
//
bool CDBFTable::GoBottom( )
{
	return ReadHeader() && Set(m_DBFHeader.rec_count - 1);
}

//
// Установка указателя на указанную запись
//
bool CDBFTable::GoTo( int nPos )
{
	return ReadHeader() && Set(nPos);	
}

// 
// Пропуск указанного количества записей
//
bool CDBFTable::Skip( int nCount )
{
	return GoTo(m_RecNo + nCount);
}

// 
// Пропуск указанного количества записей без перечитывания заголовка
//
bool CDBFTable::Skip2( int nCount ) 
{
	return Set(m_RecNo + nCount);
}

// 
// Количество полей таблицы
//
int CDBFTable::GetFieldsCount( )
{
	int nCount = m_FieldsCount;

	if ( nCount > 0 && (m_Fields[nCount-1].flags & COLUMN_SYSTEM) ) // Если последнее поле системное, игнорируем его
		nCount -= 1;

	return nCount;
}

//
// Тип поля
//
char CDBFTable::GetFieldType( int nFieldNum )
{
	if ( !CheckFieldNum(nFieldNum) )
		return 0;
	
	return m_Fields[nFieldNum].type;
}

// 
// Количество записей таблицы (из заголовка)
// 
int CDBFTable::RecCount( )
{
	ReadHeader();
	return m_DBFHeader.rec_count;
}

// 
// Чтение записи (в m_RecData)
//
bool CDBFTable::Read( )
{
	DWORD dwRead, dwError;
	DWORD nPos;

	if ( m_hDBFFile == INVALID_HANDLE_VALUE )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": таблица не открыта";
		return false; // 
	}

	// Позиционируем
	if ( m_RecNo < 0 || m_RecNo >= (int)m_DBFHeader.rec_count )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": указатель записи находится за пределами допустимого диапазона";
		return false;
	}

	nPos = m_DBFHeader.rec_offset + (m_DBFHeader.rec_size * m_RecNo);

	// Становимся на нужную запись
	if ( FileSeek(m_hDBFFile, nPos, FILE_BEGIN) != nPos )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": FileSeek failed!";
		return false; // что-то пошло не так
	}

	// Читаем данные записи
	dwError = FileRead(m_hDBFFile, m_RecData, m_DBFHeader.rec_size, &dwRead);	

	// Что-то пошло не так
	if ( dwError != ERROR_SUCCESS || dwRead != m_DBFHeader.rec_size )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += ErrorMessage(dwError);
		return false;
	}

	m_sErrorMessage = "";

	return true; // Успешно
}

// 
// Состояние записи (удалена/неудалена)
//
unsigned char CDBFTable::GetRecordStatus( )
{
	return m_RecData[0];
}

// 
// Ссылка на контейнер (DBC)
//
const char *CDBFTable::GetBackLink( )
{
	return m_BackLink;
}

// 
// Имя файла таблицы
//
const char *CDBFTable::GetTableFileName( )
{
	return m_szFileName;
}

// 
// Проверка сущаствования поля с номером nFieldNum
//
bool CDBFTable::CheckFieldNum( int nFieldNum )
{
	return (nFieldNum >= 0 && nFieldNum < (int)m_FieldsCount);
}

// 
// Номер поля по имени поля (-1 если не найдено)
//
int CDBFTable::GetFieldByName( const char *szFieldName )
{
	for ( unsigned int i = 0; i < m_FieldsCount; ++i )
		if ( !_stricmp(m_Fields[i].name, szFieldName) )
			return i;

	return -1;
}

// 
// Имя поля по номеру (NULL при неверном номере)
// 
const char *CDBFTable::GetFieldName( int nFieldNum )
{
	if ( !CheckFieldNum(nFieldNum) )
		return NULL;

	return m_Fields[nFieldNum].name;
}

//
// Размер поля и количество десятичных знаков
//
bool CDBFTable::GetFieldSize( int nFieldNum, int *nSize, int *nDecimal )
{
	if ( !nSize || !nDecimal )
		return false;
	
	if ( !CheckFieldNum(nFieldNum) )
		return false;

	*nSize    = m_Fields[nFieldNum].size;
	*nDecimal = m_Fields[nFieldNum].decimal;

	return true;
}

// 
// CHAR
//
const char *CDBFTable::GetChar( int nFieldNum )
{
	if ( !CheckFieldNum(nFieldNum) )
		return "";
	
	dbf_field_t *Field =  &m_Fields[nFieldNum];

	// TODO: проверить тип 'C' и размер ?

	memcpy(m_NoMemoData, m_RecData + Field->offset, Field->size);
	m_NoMemoData[Field->size] = '\0';

	// RTrim TODO: может сделать параметром?
	for ( int i = (int)strlen((char *)m_NoMemoData) - 1; i >= 0; --i )
	{
		if ( m_NoMemoData[i] == ' ' )
			m_NoMemoData[i] = '\0';
		else
			break;
	}

	return (const char *)m_NoMemoData;
}

const char *CDBFTable::GetChar( const char *szFieldName )
{
	int nField;

	if ( (nField = GetFieldByName(szFieldName)) == -1 )
		return "";

	return GetChar(nField);
}

// 
// LOGICAL
//
bool CDBFTable::GetLogical( int nFieldNum )
{
	if ( !CheckFieldNum(nFieldNum) )
		return false;

	// TODO: проверить тип 'L' и размер 1 ?
	
	dbf_field_t *Field =  &m_Fields[nFieldNum];

	char c = *(char *)(m_RecData + Field->offset);

	return (c == 'T');
}

bool CDBFTable::GetLogical( const char *szFieldName )
{
	int nField;

	if ( (nField = GetFieldByName(szFieldName)) == -1 )
		return false;

	return GetLogical(nField);
}

// 
// NUMERIC
//
double CDBFTable::GetNumeric( int nFieldNum )
{
	if ( !CheckFieldNum(nFieldNum) )
		return 0.0;
	
	dbf_field_t *Field = &m_Fields[nFieldNum];

	// TODO: проверить тип 'N' и размер на 20 ?

	memcpy(m_NoMemoData, m_RecData + Field->offset, Field->size);
	m_NoMemoData[Field->size] = '\0';

	return atof((const char *)m_NoMemoData);
}


double CDBFTable::GetNumeric( const char *szFieldName )
{
	int nField;

	if ( (nField = GetFieldByName(szFieldName)) == -1 )
		return 0.0;

	return GetNumeric(nField);
}

// 
// INTEGER
//
int CDBFTable::GetInteger( int nFieldNum )
{
	if ( !CheckFieldNum(nFieldNum) )
		return 0;
	
	dbf_field_t *Field = &m_Fields[nFieldNum];

	return *(int *)(m_RecData + Field->offset);
}

int CDBFTable::GetInteger( const char *szFieldName )
{
	int nField;

	if ( (nField = GetFieldByName(szFieldName)) == -1 )
		return 0;

	return GetInteger(nField);
}

//
// DOUBLE
//
double CDBFTable::GetDouble( int nFieldNum )
{
	if ( !CheckFieldNum(nFieldNum) )
		return 0.0;
	
	dbf_field_t *Field = &m_Fields[nFieldNum];

	return *(double *)(m_RecData + Field->offset);
}

double CDBFTable::GetDouble( const char *szFieldName )
{
	int nField;

	if ( (nField = GetFieldByName(szFieldName)) == -1 )
		return 0.0;

	return GetDouble(nField);
}

// 
// DATE
//
date_t CDBFTable::GetDate( int nFieldNum )
{
	date_t empty = { 0 };
	
	if ( !CheckFieldNum(nFieldNum) )
		return empty;
	
	dbf_field_t *Field = &m_Fields[nFieldNum];

	// TODO: проверить тип 'D' и размер 8

	return ConvertToDate(m_RecData + Field->offset);
}

date_t CDBFTable::GetDate( const char *szFieldName )
{
	date_t empty = { 0 };
	int nField;

	if ( (nField = GetFieldByName(szFieldName)) == -1 )
		return empty;

	return GetDate(nField);
}

// 
// DATETIME
//
date_time_t CDBFTable::GetDateTime( int nFieldNum )
{
	date_time_t empty = { 0 };

	if ( !CheckFieldNum(nFieldNum) )
		return empty;
	
	dbf_field_t *Field = &m_Fields[nFieldNum];

	return ConvertToDateTime(m_RecData + Field->offset);
}

date_time_t CDBFTable::GetDateTime( const char *szFieldName )
{
	date_time_t empty = { 0 };

	int nField;

	if ( (nField = GetFieldByName(szFieldName)) == -1 )
		return empty;
	
	return GetDateTime(nField);
}

// 
// MEMO
//
const void *CDBFTable::GetMemo( int nFieldNum, unsigned int *nDataSize )
{
	unsigned int nBlockNum;
	
	if ( !CheckFieldNum(nFieldNum) )
		return NULL;

	dbf_field_t *Field = &m_Fields[nFieldNum];

	if ( Field->type != MEMO_FIELD )
		return NULL;

	// Номер блока
	nBlockNum = *(unsigned int *)(m_RecData + Field->offset);

	// Нет данных
	if ( nBlockNum == 0 )
		return NULL; 

	// Читаем содержимое MEMO поля
	if ( !ReadMemo(nBlockNum) )
		return NULL;

	// Размер прочитанного
	if ( nDataSize )
		*nDataSize = m_MemoSize;

	return m_MemoData;
}

const void *CDBFTable::GetMemo( const char *szFieldName, unsigned int *nDataSize )
{
	int nField;

	if ( (nField = GetFieldByName(szFieldName)) == -1 )
		return NULL;
	
	return GetMemo(nField, nDataSize);
}



//
// Запись данных в поле
//
// nRecNumber   - номер записи
// nFieldNumber - номер поля
// pData        - данные произвольного типа
// nDataSize    - размер данных
//
bool CDBFTable::WriteRecordData( unsigned int nRecNumber, unsigned int nFieldNumber, unsigned char *pData, unsigned char nDataSize )
{
	DWORD nCurrentPos, nNewPos, dwError, dwWritten;

	if ( nRecNumber >= m_DBFHeader.rec_count )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": некорректный номер записи";
		return false;
	}

	if ( nFieldNumber >= m_FieldsCount )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": некорректный номер поля";
		return false;
	}

	if ( nDataSize > m_Fields[nFieldNumber].size )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": некорректный размер данных";
		return false;
	}

	nCurrentPos = FileSeek(m_hDBFFile, 0, FILE_CURRENT);

	nNewPos = m_DBFHeader.rec_offset + (m_DBFHeader.rec_size * nRecNumber) + m_Fields[nFieldNumber].offset;

	if ( FileSeek(m_hDBFFile, nNewPos, FILE_BEGIN) != nNewPos )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": FileSeek failed!";
		return false;
	}

	dwError = FileWrite(m_hDBFFile, pData, nDataSize, &dwWritten);
	if ( dwError != ERROR_SUCCESS || dwWritten != nDataSize )
	{
		FileSeek(m_hDBFFile, nCurrentPos, FILE_BEGIN); // Восстанавливаем позицию
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += ErrorMessage(dwError);
		return false;
	}

	FileSeek(m_hDBFFile, nCurrentPos, FILE_BEGIN); // Восстанавливаем позицию

	m_sErrorMessage = ""; // OK

	return true;
}

//
// Запись номера блока 
// nRecNumber   - номер записи
// nFieldNumber - номер поля
// nBlockNumber - номер блока начиная с единицы (0 - пусто)
//
bool CDBFTable::WriteRecordMemoBlock( unsigned int nRecNumber, unsigned int nFieldNumber, unsigned int nBlockNumber )
{
	// TODO: использовать WriteRecordData
	
	DWORD nCurrentPos, nNewPos, dwError, dwWritten;
	unsigned int nNewBlockNum;
	bool bRet = true;

	nCurrentPos = FileSeek(m_hDBFFile, 0, FILE_CURRENT);

	nNewPos = m_DBFHeader.rec_offset + (m_DBFHeader.rec_size * nRecNumber) + m_Fields[nFieldNumber].offset;

	nNewBlockNum = SwapBytes32(nBlockNumber);  // TODO: !!!!!!!!! тут дествительно нужен Swap ????? похоже я чтото попутал

	if ( FileSeek(m_hDBFFile, nNewPos, FILE_BEGIN) != nNewPos )
	{
		//VALIDATE_LOG(0, 0, LOG_ERROR, "FileSeek failed!");
		return false;
	}

	dwError = FileWrite(m_hDBFFile, (BYTE *)&nNewBlockNum, 4, &dwWritten);
	if ( dwError != ERROR_SUCCESS || dwWritten != 4 )
	{
		FileSeek(m_hDBFFile, nCurrentPos, FILE_BEGIN);
		//VALIDATE_LOG(0, 0, LOG_ERROR, ErrorMessage(dwError).c_str());
		return false;
	}

	FileSeek(m_hDBFFile, nCurrentPos, FILE_BEGIN);

	return bRet;
}

// 
// Сообщение о последней ошибке
//
string CDBFTable::GetLastErrorMessage( )
{
	return m_sErrorMessage;
}

// Private

// 
// Чтение заголовка DBF
//
bool CDBFTable::ReadHeader( )
{
	DWORD dwRead, dwError;
	
	if ( m_hDBFFile == INVALID_HANDLE_VALUE )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": таблица не открыта";
		return false;
	}

	FileSeek(m_hDBFFile, 0, FILE_BEGIN);

	dwError = FileRead(m_hDBFFile, (BYTE *)&m_DBFHeader, sizeof(m_DBFHeader), &dwRead);
	if ( dwError != ERROR_SUCCESS )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += ErrorMessage(dwError);
		return false;
	}

	if ( dwRead != sizeof(m_DBFHeader) )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": размер прочитанных данных (";
		m_sErrorMessage += IntToStr(dwRead);
		m_sErrorMessage += ") не соответствует размеру заголовка (";
		m_sErrorMessage += IntToStr(sizeof(m_DBFHeader));
		m_sErrorMessage += ")";
		return false;
	}

	m_sErrorMessage = "";

	return true; // успешно
}

// 
// Чтение структуры полей
//
bool CDBFTable::ReadFields( )
{
	DWORD dwRead, dwError;
	dbf_field_t Field;
	unsigned char cHeadTerminator = 0x00;
	
	if ( m_hDBFFile == INVALID_HANDLE_VALUE )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szFileName;
		m_sErrorMessage += ": таблица не открыта";
		return false;
	}
	
	if ( FileSeek(m_hDBFFile, sizeof(dbf_header_t), FILE_BEGIN) != sizeof(dbf_header_t) )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szFileName;
		m_sErrorMessage += ": FileSeek failed!";
		return false;
	}

	// Вычислим количество полей
	m_FieldsCount = (m_DBFHeader.rec_offset - 296) / 32;
	if ( m_FieldsCount > MAX_FIELDS )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szFileName;
		m_sErrorMessage += ": количество полей таблицы превышает допустимое";
		return false;
	}

	for ( unsigned int i = 0; i < m_FieldsCount; ++i )
	{
		dwError = FileRead(m_hDBFFile, (BYTE *)&Field, sizeof(Field), &dwRead);
		if ( dwError != ERROR_SUCCESS )
		{
			m_sErrorMessage = __FUNCTION__;
			m_sErrorMessage += ": ";
			m_sErrorMessage += m_szFileName;
			m_sErrorMessage += ": ";
			m_sErrorMessage += ErrorMessage(dwError);
			return false;
		}

		if ( dwRead == 0 )
		{
			m_sErrorMessage = __FUNCTION__;
			m_sErrorMessage += ": ";
			m_sErrorMessage += m_szFileName;
			m_sErrorMessage += ": неожиданный конец файла";
			return false;
		}

		if ( dwRead != sizeof(Field) )
		{
			m_sErrorMessage = __FUNCTION__;
			m_sErrorMessage += ": ";
			m_sErrorMessage += m_szFileName;
			//m_sErrorMessage += ": размер прочитанных данных меньше чем размер поля";
			m_sErrorMessage += ": размер прочитанных данных (";
			m_sErrorMessage += IntToStr(dwRead);
			m_sErrorMessage += ") не равен размеру поля (";
			m_sErrorMessage += IntToStr(sizeof(Field));
			m_sErrorMessage += ")";
			return false;
		}

		m_Fields.push_back(Field); // Добавляем поле в список
	}

	// Head Terminator
	dwError = FileRead(m_hDBFFile, (BYTE *)&cHeadTerminator, 1, &dwRead);
	if ( dwError != ERROR_SUCCESS )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szFileName;
		m_sErrorMessage += ": ";
		m_sErrorMessage += ErrorMessage(dwError);
		return false;
	}

	if ( dwRead != 1 )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szFileName;
		m_sErrorMessage += ": не удалось найти Head Terminator";
		return false;
	}

	if ( cHeadTerminator != HEAD_RECORD_TERMINATOR )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szFileName;
		m_sErrorMessage += ": Head Terminator не равен 0x0D";
		return false;
	}

	// Backlink
	dwError = FileRead(m_hDBFFile, (BYTE *)&m_BackLink, 263, &dwRead);
	if ( dwError != ERROR_SUCCESS )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szFileName;
		m_sErrorMessage += ": ";
		m_sErrorMessage += ErrorMessage(dwError);
		return false;
	}

	if ( dwRead != 263 )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szFileName;
		m_sErrorMessage += ": размер данных Backlink не равен 263";
		return false;
	}

	m_sErrorMessage = "";

	return true; // Успешно
}

// 
// Чтение заголовка MEMO файла
//
bool CDBFTable::ReadMemoHeader( )
{
	DWORD dwRead, dwError;
	
	if ( m_hFPTFile == INVALID_HANDLE_VALUE )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szMemoFileName;
		m_sErrorMessage += ": FPT файл не открыт";
		return false;
	}

	FileSeek(m_hFPTFile, 0, FILE_BEGIN);

	dwError = FileRead(m_hFPTFile, (BYTE *)&m_FPTHeader, sizeof(m_FPTHeader), &dwRead);
	if ( dwError != ERROR_SUCCESS )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szMemoFileName;
		m_sErrorMessage += ": ";
		m_sErrorMessage += ErrorMessage(dwError);
		return false;
	}

	if ( dwRead != sizeof(m_FPTHeader) )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szMemoFileName;
		m_sErrorMessage += ": размер прочитанных данных (";
		m_sErrorMessage += IntToStr(dwRead);
		m_sErrorMessage += ") не соответствует размеру заголовка FPT файла (";
		m_sErrorMessage += IntToStr(sizeof(m_DBFHeader));
		m_sErrorMessage += ")";
		return false;
	}

	// меняем порядок байт
	m_FPTHeader.next_block_num = SwapBytes32(m_FPTHeader.next_block_num);
	m_FPTHeader.size = SwapBytes16(m_FPTHeader.size);

	m_sErrorMessage = "";

	return true; // успешно
}

// 
// Чтение MEMO блока
//
bool CDBFTable::ReadMemo( unsigned int nBlockNum )
{
	fpt_block_t  Block;
	unsigned int nOffset;
	DWORD dwRead, dwError;
	
	// Становимся на нужную позицию
	nOffset = m_FPTHeader.size * nBlockNum;
	if ( FileSeek(m_hFPTFile, nOffset, FILE_BEGIN) != nOffset )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szMemoFileName;
		m_sErrorMessage += ": FileSeek failed!";
		return false;
	}

	// Читаем заголовок блока
	dwError = FileRead(m_hFPTFile, (BYTE *)&Block, sizeof(Block), &dwRead);

	if ( dwError != ERROR_SUCCESS )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szMemoFileName;
		m_sErrorMessage += ": ";
		m_sErrorMessage += ErrorMessage(dwError);
		return false;
	}

	if ( dwRead != sizeof(Block) )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szMemoFileName;
		//m_sErrorMessage += ": неверный размер прочитанных данных заголовка MEMO блока";
		m_sErrorMessage += ": размер прочитанных данных (";
		m_sErrorMessage += IntToStr(dwRead);
		m_sErrorMessage += ") не соответствует размеру заголовка MEMO блока (";
		m_sErrorMessage += IntToStr(sizeof(Block));
		m_sErrorMessage += ")";
		return false;
	}

	// меняем порядок байт
	Block.type = SwapBytes32(Block.type);
	Block.size = SwapBytes32(Block.size);

	// Освобождаем память предыдущих данных
	if ( m_MemoData )
		delete [] m_MemoData;

	if ( Block.size > 1024*1024*100 ) // похоже что херня какая-то TODO: уточнить сколько может быть, пока поставим 100МБ
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szMemoFileName;
		m_sErrorMessage += ": размер данных в MEMO блоке слишком большой!";
		m_MemoData = NULL;
		return false;
	}

	// Выделяем память под данные блока
	m_MemoData = new unsigned char[Block.size];

	// Читаем данные блока
	dwError = FileRead(m_hFPTFile, m_MemoData, Block.size, &dwRead);
	if ( dwError != ERROR_SUCCESS )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szMemoFileName;
		m_sErrorMessage += ": ";
		m_sErrorMessage += ErrorMessage(dwError);
		return false;
	}

	if ( dwRead != Block.size )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szMemoFileName;
		m_sErrorMessage += ": размер прочитанных данных не соответствует размеру указанному в заголовке MEMO блока";
		// TODO: указать размеры
		return false;
	}

	m_MemoType = Block.type;
	m_MemoSize = Block.size;

	m_sErrorMessage = "";

	return true;
}

// 
// Установка позиции
// nRecNo - номер записи
//
bool CDBFTable::Set( int nRecNo )
{
	if ( nRecNo < -1 )
		nRecNo = -1;

	if ( nRecNo > (int)m_DBFHeader.rec_count )
		nRecNo = m_DBFHeader.rec_count;
	
	m_RecNo = nRecNo;

	return true;
}

