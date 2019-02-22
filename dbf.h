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

#define FIELDS_TYPES            "CYNFDTBILMGWVQP" // ���� �����

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

#pragma pack(push, 1)                  // ������������ �������� �� ������� 1 �����

// ��������� DBF
struct dbf_header_t                    //  32 �����
{
	unsigned char  type;               //  0       ��� �������
	unsigned char  date[3];            //  1-3     ���� ����������� YYMMDD
	unsigned int   rec_count;          //  4-7     ���-�� �������
	unsigned short rec_offset;         //  8-9     �������� ������
	unsigned short rec_size;           //  10-11   ������ ������ (������� ������� ��������� ������)
	unsigned char  reserved1[16];      //  12-27   ���������������
	unsigned char  flags;              //  28      ����� (0x1 - ���� CDX, 0x2 - ���� MEMO ����, 0x4 - ������� �������� DBC)
	unsigned char  code_page;          //  29      ������� ��������
	unsigned char  reserved2[2];       //  30-31   ���������������
};

// ����
struct dbf_field_t                     //  32 �����
{
	char           name[11];           //  0-10    ��� ���� + '\0' (���� ����� 10 ��������, ���������� ���� ������ ���� ��������� ������)
	char           type;               //  11      ��� ����
	unsigned int   offset;             //  12-15   �������� ������ ���� � ������
	unsigned char  size;               //  16      ������ ����
	unsigned char  decimal;            //  17      ���������� ���������� ������
	unsigned char  flags;              //  18      ����� (0x01, 0x02, 0x04, 0x0C)
	unsigned int   auto_inc_next;      //  19-22   �������� ��������������
	unsigned char  auto_inc_step;      //  23      ��� ��������������
	unsigned char  reserved[8];        //  24-31   ���������������
};

// ��������� FPT
struct fpt_header_t                    //  512 ����
{
	unsigned int   next_block_num;     //  0-3     ����� ���������� ���������� �����
	unsigned char  reserved1[2];       //  4-5     ���������������
	unsigned short size;               //  6-7     ������ �����
	unsigned char  reserved2[504];     //  8-511   ���������������
};

// ���� FPT
struct fpt_block_t                     //  8 ����
{
	unsigned int   type;               //  0-3     ��� ������ (0 � picture (picture field type), 1 � text (memo field type))
	unsigned int   size;               //  4-7     ������ ������, ������� ���������� ����� ����� ����
	// ����� ���� ������ �����
};

// DBC property
struct dbc_property_t                  //  6 ����
{
	unsigned int   size;               //  0-3     ������ ����� ����� (��������� � ������)
	unsigned short unknown;            //  4-5     ������ 0x0001
	unsigned char  code;               //  6       ��� property
	// ����� ���� ������ property
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

	// ��������� ������
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
	char                 m_szName[MAX_PATH];          // ��� �������
	char                 m_szFileName[MAX_PATH];      // ��� ����� �������
	char                 m_szPath[MAX_PATH];          // ���� � �������
	char                 m_szMemoFileName[MAX_PATH];  // ��� MEMO ����� �������
	HANDLE               m_hDBFFile;                  // ��������� ����� DBF
	HANDLE               m_hFPTFile;                  // ��������� ����� FPT
	dbf_header_t         m_DBFHeader;                 // ��������� DBF
	fpt_header_t         m_FPTHeader;                 // ��������� FPT
	vector <dbf_field_t> m_Fields;                    // ���� �������
	unsigned int         m_FieldsCount;               // ���������� ����� 
	char                 m_BackLink[263];             // ���� � ���������� (���� ������� �������� � ���� ������, ����� 0x00)
	int                  m_RecNo;                     // ������� ������
	unsigned char        *m_RecData;                  // ����� ��� ����������� ������
	unsigned char        *m_MemoData;                 // ����� ��� ������ MEMO ����
	unsigned int         m_MemoType;                  // ��� ������ MEMO ���� (0, 1)
	unsigned int         m_MemoSize;                  // ������ m_MemoData
	unsigned int         m_MemoRealBlockCount;        // ���������� �������� (�� �� ��������� ������ � MEMO �����)
	unsigned char        m_NoMemoData[255];           // ����� ��� ������� ����� �������� �� 254 ���� (�� ����). 255-� ��� ���� �����������

	unsigned int         m_LockAttemptMax;            // ������������ ���������� ������� ���������� 
	unsigned int         m_LockAttemptWait;           // ����� ����� ��������� ����������

	int                  m_nErrorCode;                // ��� ������
	string               m_sErrorMessage;             // ��������� ������

	bool ReadHeader( );                               // ������ ��������� �������
	bool ReadFields( );                               // ������ ��������� �����

	bool ReadMemoHeader( );                           // ������ ��������� memo
	bool ReadMemo( unsigned int nBlockNum );          // ������ ���� �����

	bool Set( int nRecNo );                           // ��������� �������
};

#endif
