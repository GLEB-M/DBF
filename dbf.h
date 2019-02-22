#ifndef _DBF_H_
#define _DBF_H_

#include "db_utils.h"
#include "cdx.h"

using namespace DBUtils;

#define VISUAL_FOXPRO_TABLE     0x30

#define CHAR_FIELD              'C'
#define CURRENCY_FIELD          'Y'
#define NUMERIC_FIELD           'N'
#define FLOAT_FIELD             'F'
#define DATE_FIELD              'D'
#define DATETIME_FIELD          'T'
#define DOUBLE_FIELD            'B'
#define INTEGER_FIELD           'I' 
#define LOGICAL_FIELD           'L'
#define MEMO_FIELD              'M'
#define GENERAL_FIELD           'G'
#define BLOB_FIELD              'W'
#define VARCHAR_FIELD           'V'
#define VARBINARY_FIELD         'Q'
#define PICTURE_FIELD           'P' 

#define FIELDS_TYPES            "CYNFDTBILMGWVQP" // типы полей

#define MAX_FIELDS              255

#define HEAD_RECORD_TERMINATOR  0x0D

#define DBF_EOF                 0x1A

#define NOT_DELETED             0x20
#define DELETED                 0x2A

#define DBF_HAS_CDX             0x01
#define DBF_HAS_MEMO            0x02
#define DBF_IS_DBC              0x04

#define COLUMN_SYSTEM           0x01   // System Column (not visible to user)
#define COLUMN_CAN_STORE_NULL   0x02   // Column can store null values
#define COLUMN BINARY           0x04   // Binary column (for CHAR and MEMO only)
#define COLUMN_IS_AUTOINC       0x0C   // Column is autoincrementing

#define DBC_PROPERTY_PATH       0x01   // Property Path
#define DBC_PROPERTY_PRIMARYKEY 0x14   // Property Primary key

#pragma pack(push, 1)                  // Выравнивание структур по границе 1 байта

// Заголовок DBF
struct dbf_header_t                    //  32 байта
{
	unsigned char  type;               //  0       тип таблицы
	unsigned char  date[3];            //  1-3     дата модификации YYMMDD
	unsigned int   rec_count;          //  4-7     кол-во записей
	unsigned short rec_offset;         //  8-9     смещение данных
	unsigned short rec_size;           //  10-11   размер записи (включая признак удаленной записи)
	unsigned char  reserved1[16];      //  12-27   зарезервировано
	unsigned char  flags;              //  28      флаги (0x1 - есть CDX, 0x2 - есть MEMO поля, 0x4 - таблица является DBC)
	unsigned char  code_page;          //  29      кодовая страница
	unsigned char  reserved2[2];       //  30-31   зарезервировано
};

// Поля
struct dbf_field_t                     //  32 байта
{
	char           name[11];           //  0-10    имя поля + '\0' (если менее 10 символов, оставшаяся чать должна быть заполнена нулями)
	char           type;               //  11      тип поля
	unsigned int   offset;             //  12-15   смещение данных поля в записи
	unsigned char  size;               //  16      размер поля
	unsigned char  decimal;            //  17      количество десятичных знаков
	unsigned char  flags;              //  18      флаги (0x01, 0x02, 0x04, 0x0C)
	unsigned int   auto_inc_next;      //  19-22   значение автоинкремента
	unsigned char  auto_inc_step;      //  23      шаг автоинкремента
	unsigned char  reserved[8];        //  24-31   зарезервировано
};

// Заголовок FPT
struct fpt_header_t                    //  512 байт
{
	unsigned int   next_block_num;     //  0-3     номер следующего свободного блока
	unsigned char  reserved1[2];       //  4-5     зарезервировано
	unsigned short size;               //  6-7     размер блока
	unsigned char  reserved2[504];     //  8-511   зарезервировано
};

// Блок FPT
struct fpt_block_t                     //  8 байт
{
	unsigned int   type;               //  0-3     тип данных (0 – picture (picture field type), 1 – text (memo field type))
	unsigned int   size;               //  4-7     размер данных, которые начинаются после этого поля
	// далее идут данные блока
};

// DBC property
struct dbc_property_t                  //  6 байт
{
	unsigned int   size;               //  0-3     размер этого блока (заголовок и данные)
	unsigned short unknown;            //  4-5     всегда 0x0001
	unsigned char  code;               //  6       код property
	// далее идут данные property
};

#pragma pack(pop)


class CDBFTable
{
public:
	CDBFTable();
	~CDBFTable();

	bool Open( const char *szFileName, bool bWrite, bool bExclusive );
	void Close( bool bNoClearLastError = false );
	bool HasCDX( );

	bool Lock( );
	bool Unlock( );

	bool BeginOfFile( );
	bool EndOfFile( );

	bool GoTop( );
	bool GoBottom( );
	bool GoTo( int nPos );

	bool Skip( int nCount = 1 );
	bool Skip2( int nCount = 1 ); 

	int  GetFieldsCount( );
	char GetFieldType( int nFieldNum );

	int  RecCount( );

	bool Read( );

	unsigned char GetRecordStatus( );
	const char    *GetBackLink( );
	const char    *GetTableFileName( );

	bool          CheckFieldNum( int nFieldNum );

	int           GetFieldByName( const char *szFieldName );
	const char    *GetFieldName( int nFieldNum );
	bool          GetFieldSize( int nFieldNum, int *nSize, int *nDecimal );

	// Получение данных
	// CHAR
	const char    *GetChar( int nFieldNum );
	const char    *GetChar( const char *szFieldName );

	// LOGICAL
	bool          GetLogical( int nFieldNum );
	bool          GetLogical( const char *szFieldName );

	// NUMERIC
	double        GetNumeric( int nFieldNum );
	double        GetNumeric( const char *szFieldName );

	// INTEGER
	int           GetInteger( int nFieldNum );
	int           GetInteger( const char *szFieldName );

	// DOUBLE
	double        GetDouble( int nFieldNum );
	double        GetDouble( const char *szFieldName );

	// DATE
	date_t        GetDate( int nFieldNum );
	date_t        GetDate( const char *szFieldName );

	// DATETIME
	date_time_t   GetDateTime( int nFieldNum );
	date_time_t   GetDateTime( const char *szFieldName );

	// MEMO
	const void    *GetMemo( int nFieldNum, unsigned int *nDataSize );
	const void    *GetMemo( const char *szFieldName, unsigned int *nDataSize );

	bool          WriteRecordData( unsigned int nRecNumber, unsigned int nFieldNumber, unsigned char *pData, unsigned char nDataSize );

	bool          WriteRecordMemoBlock( unsigned int nRecNumber, unsigned int nFieldNumber, unsigned int nBlockNumber );

	string        GetLastErrorMessage( );

	CCompoundIndex m_CDX;

protected:
	char                 m_szName[MAX_PATH];          // Имя таблицы
	char                 m_szFileName[MAX_PATH];      // Имя файла таблицы
	char                 m_szPath[MAX_PATH];          // Путь к таблице
	char                 m_szMemoFileName[MAX_PATH];  // Имя MEMO файла таблицы
	HANDLE               m_hDBFFile;                  // Описатель файла DBF
	HANDLE               m_hFPTFile;                  // Описатель файла FPT
	dbf_header_t         m_DBFHeader;                 // Заголовок DBF
	fpt_header_t         m_FPTHeader;                 // Заголовок FPT
	vector <dbf_field_t> m_Fields;                    // Поля таблицы
	unsigned int         m_FieldsCount;               // Количество полей 
	char                 m_BackLink[263];             // Путь к контейнеру (если таблица включена в базу данных, иначе 0x00)
	int                  m_RecNo;                     // Текущая запись
	unsigned char        *m_RecData;                  // Буфер для прочитанной записи
	unsigned char        *m_MemoData;                 // Буфер для данных MEMO поля
	unsigned int         m_MemoType;                  // Тип данных MEMO поля (0, 1)
	unsigned int         m_MemoSize;                  // Размер m_MemoData
	unsigned int         m_MemoRealBlockCount;        // Количество реальных (не из заголовка блоков в MEMO файле)
	unsigned char        m_NoMemoData[255];           // Буфер для обычных полей размером до 254 байт (не мемо). 255-й для нуль терминатора

	unsigned int         m_LockAttemptMax;            // Максимальное количество попыток блокировки 
	unsigned int         m_LockAttemptWait;           // Пауза между попытками блокировки

	int                  m_nErrorCode;                // код ошибки
	string               m_sErrorMessage;             // сообщение ошибки

	bool ReadHeader( );                               // Чтение заголовка таблицы
	bool ReadFields( );                               // Чтение структуры полей

	bool ReadMemoHeader( );                           // Чтение заголовка memo
	bool ReadMemo( unsigned int nBlockNum );          // Чтение мемо блока

	bool Set( int nRecNo );                           // Установка позиции
};

#endif
