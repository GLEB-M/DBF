#ifndef _CDX_H_
#define _CDX_H_

#include "db_utils.h"

using namespace DBUtils;

#define COMPACT_INDEX   0x20                //  омпактный индекс
#define COMPOUND_INDEX  0x40                // —оставной индекс

#pragma pack(push, 1)                       // ¬ыравнивание структур по границе 1 байта

struct cdx_header_t
{
	unsigned int   root_node;               // адрес корневого узла
	unsigned int   free_node;
	unsigned char  reserved1[4];
	unsigned short key_len;
	unsigned char  options;
	unsigned char  signature;
	unsigned char  reserved2[486];
	unsigned short asc_desc;
	unsigned char  reserved3[2];
	unsigned short for_len;
	unsigned char  reserved4[2];
	unsigned short key_exp_len;
	unsigned char  key_exp[512];
};

struct cdx_interior_node_t
{
	unsigned short attributes;               // атрибуты (0, 1, 2)
	unsigned short key_num;                  // количество ключей
	unsigned int   left_node;                // узел слева (-1 если нет)
	unsigned int   right_node;               // узел справа (-1 если нет)
	unsigned char  keys_info[500];           // информаци€ о ключах
};

struct cdx_exterior_node_t
{
	unsigned short attributes;               // атрибуты (0, 1, 2)
	unsigned short key_num;                  // количество ключей
	unsigned int   left_node;                // узел слева (-1 если нет)
	unsigned int   right_node;               // узел справа (-1 если нет)
	unsigned short free_space;               // свободное место в keys_info
	unsigned int   rec_num_mask;             // маска дл€ номера записи
	unsigned char  dup_mask;                 // маска дл€ дубирующихс€ частей
	unsigned char  trail_mask;               // маска дл€ недубирующихс€ частей
	unsigned char  rec_num_bits;             // количество бит отвечающих за номер записи
	unsigned char  dup_bits;                 // количество бит отвечающих за дубли
	unsigned char  trail_bits;               // количество бит отвечающих за недубли
	unsigned char  rec_num_dup_trail_size;   // количество хран€щих номер записи + дубли + недубли 
	unsigned char  keys_info[488];           // информаци€ о ключах
};

struct cdx_key_t
{
	char         value[240];
	unsigned int rec_num;
	unsigned int intra_node;
};

#pragma pack(pop)

/* 

ѕример

rec_num_mask           = 0x0000FFFF   номер записи                                  - 2 байта
dup_mask               = 0x0F         дубли                                         - полбайта
trail_mask             = 0x0F         недубли                                       - полбайта
rec_num_bits           = 0x10         размер номера записи в битах                  - 16 бит (2 байта)
dup_bits               = 0x4          размер количества дублей                      - 4 бита
trail_bits             = 0x4          размер количества недублей                    - 4 бита
rec_num_dup_trail_size = 0x3          размер номера записи,дублей,недублей в байтах - 3 байта


1) читаем из блок keys_info размером rec_num_dup_trail_size
2) примен€ем к значению rec_num_mask - это будет номер записи
3) сдвигаем значение вправо на rec_num_bits
4) примен€ем к значению dup_mask - это будет количество дублей
5) сдвигаем значение вправо на dup_bits
6) примен€ем к значению trail_mask - 10 минус полученное значение будет количество недублей
7) повтор€ем все key_num раз

*/

class CCompoundIndex
{
public:
	CCompoundIndex( );
	~CCompoundIndex( );

	bool Open( const char *szFileName, bool bExclusive );
	void Close( );

	int  GetTagsCount( );
	const char *GetTagName( int nTagNum );

	string GetLastErrorMessage( );

	cdx_header_t         m_Header;

protected:
	char                 m_szFileName[MAX_PATH];  // »м€ индексного файла
	HANDLE               m_hFile;                 // ќписатель файла
	unsigned int         m_FileSize;
	vector <cdx_key_t *> m_Tags;                  // “эги
	unsigned int         m_TagsCount;

	int                  m_nErrorCode;            // код ошибки
	string               m_sErrorMessage;         // сообщение ошибки
 
	bool ReadHeader( unsigned int nAddress, cdx_header_t *Header );
	bool ReadTags( );
	void ClearTags( );

	bool ReadExteriorKeys( unsigned char *pNode, unsigned int nKeyLen, vector <cdx_key_t *> &Keys );
	bool ReadInteriorKeys( unsigned char *pNode, unsigned int nKeyLen, vector <cdx_key_t *> &Keys );
	bool ReadKeys( unsigned int nAddress, unsigned int nKeyLen, void proc(cdx_key_t *key, void *param) );

	bool CheckTagNum( int nTagNum );

	static bool SortTagsProc( cdx_key_t *i, cdx_key_t *j );

	friend void TagEnumProc( cdx_key_t *Key, void *Param );
};

#endif
