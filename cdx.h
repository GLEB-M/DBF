#ifndef _CDX_H_
#define _CDX_H_

#include "db_utils.h"

using namespace DBUtils;

#define COMPACT_INDEX   0x20                // ���������� ������
#define COMPOUND_INDEX  0x40                // ��������� ������

#pragma pack(push, 1)                       // ������������ �������� �� ������� 1 �����

struct cdx_header_t
{
	unsigned int   root_node;               // ����� ��������� ����
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
	unsigned short attributes;               // �������� (0, 1, 2)
	unsigned short key_num;                  // ���������� ������
	unsigned int   left_node;                // ���� ����� (-1 ���� ���)
	unsigned int   right_node;               // ���� ������ (-1 ���� ���)
	unsigned char  keys_info[500];           // ���������� � ������
};

struct cdx_exterior_node_t
{
	unsigned short attributes;               // �������� (0, 1, 2)
	unsigned short key_num;                  // ���������� ������
	unsigned int   left_node;                // ���� ����� (-1 ���� ���)
	unsigned int   right_node;               // ���� ������ (-1 ���� ���)
	unsigned short free_space;               // ��������� ����� � keys_info
	unsigned int   rec_num_mask;             // ����� ��� ������ ������
	unsigned char  dup_mask;                 // ����� ��� ������������ ������
	unsigned char  trail_mask;               // ����� ��� �������������� ������
	unsigned char  rec_num_bits;             // ���������� ��� ���������� �� ����� ������
	unsigned char  dup_bits;                 // ���������� ��� ���������� �� �����
	unsigned char  trail_bits;               // ���������� ��� ���������� �� �������
	unsigned char  rec_num_dup_trail_size;   // ���������� �������� ����� ������ + ����� + ������� 
	unsigned char  keys_info[488];           // ���������� � ������
};

struct cdx_key_t
{
	char         value[240];
	unsigned int rec_num;
	unsigned int intra_node;
};

#pragma pack(pop)

/* 

������

rec_num_mask           = 0x0000FFFF   ����� ������                                  - 2 �����
dup_mask               = 0x0F         �����                                         - ��������
trail_mask             = 0x0F         �������                                       - ��������
rec_num_bits           = 0x10         ������ ������ ������ � �����                  - 16 ��� (2 �����)
dup_bits               = 0x4          ������ ���������� ������                      - 4 ����
trail_bits             = 0x4          ������ ���������� ��������                    - 4 ����
rec_num_dup_trail_size = 0x3          ������ ������ ������,������,�������� � ������ - 3 �����


1) ������ �� ���� keys_info �������� rec_num_dup_trail_size
2) ��������� � �������� rec_num_mask - ��� ����� ����� ������
3) �������� �������� ������ �� rec_num_bits
4) ��������� � �������� dup_mask - ��� ����� ���������� ������
5) �������� �������� ������ �� dup_bits
6) ��������� � �������� trail_mask - 10 ����� ���������� �������� ����� ���������� ��������
7) ��������� ��� key_num ���

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
	char                 m_szFileName[MAX_PATH];  // ��� ���������� �����
	HANDLE               m_hFile;                 // ��������� �����
	unsigned int         m_FileSize;
	vector <cdx_key_t *> m_Tags;                  // ����
	unsigned int         m_TagsCount;

	int                  m_nErrorCode;            // ��� ������
	string               m_sErrorMessage;         // ��������� ������
 
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
