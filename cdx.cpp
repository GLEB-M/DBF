#include "cdx.h"

//
// CCompoundIndex
//
CCompoundIndex::CCompoundIndex( )
{
	m_hFile = INVALID_HANDLE_VALUE;
	m_FileSize = 0;
	m_TagsCount = 0;

	memset(m_szFileName, 0, sizeof(m_szFileName));
	memset(&m_Header, 0, sizeof(m_Header));

	m_nErrorCode = 0;
}

//
// ~CCompoundIndex
//
CCompoundIndex::~CCompoundIndex( )
{
	Close();
}

//
// Открытие индексного файла
//
bool CCompoundIndex::Open( const char *szFileName, bool bExclusive )
{
	DWORD dwError;

	// Закрываем предыдущий открытый индексный файл
	Close();
	
	if ( (dwError = FileOpenA(szFileName, false, bExclusive, &m_hFile)) != ERROR_SUCCESS )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += szFileName;
		m_sErrorMessage += " ";
		m_sErrorMessage += ErrorMessage(dwError);
		return false;
	}

	strcpy_s(m_szFileName, MAX_PATH, szFileName);

	m_FileSize = FileSize(m_hFile);

	if ( !ReadHeader(0, &m_Header) ) // Читаем заголовок корневого узла
		return false;

	if ( !ReadTags() ) // Читаем тэги
		return false;

	m_sErrorMessage = "";
	return true;
}

//
// Закрытие индексного файла
//
void CCompoundIndex::Close( )
{
	ClearTags(); 
	
	if ( m_hFile != INVALID_HANDLE_VALUE )
	{
		FileClose(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;
	}

	m_FileSize = 0;
}

//
// Чтение заголовка узла
//
bool CCompoundIndex::ReadHeader( unsigned int nAddress, cdx_header_t *Header )
{
	DWORD dwError, dwRead;

	FileSeek(m_hFile, nAddress, FILE_BEGIN);
	dwError = FileRead(m_hFile, (BYTE *)Header, sizeof(cdx_header_t), &dwRead);
	if ( dwError != ERROR_SUCCESS )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szFileName;
		m_sErrorMessage += " ";
		m_sErrorMessage += ErrorMessage(dwError);
		return false;
	}

	if ( dwRead != sizeof(cdx_header_t) )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szFileName;
		m_sErrorMessage += " неожиданный конец файла";
		return false;
	}

	if ( !(Header->options & COMPACT_INDEX) )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szFileName;
		m_sErrorMessage += " файл не является компактным индексом";
		return false;
	}

	if ( !(Header->options & COMPOUND_INDEX) )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szFileName;
		m_sErrorMessage += " файл не является составным индексом";
		return false;
	}

	if ( Header->key_len < 1 || Header->key_len > 240 )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szFileName;
		m_sErrorMessage += " длина ключа должна быть в пределах 1..240";
		return false;
	}
	
	m_sErrorMessage = "";
	return true;
}

//
// Процедура чтение тэгов
//
void TagEnumProc( cdx_key_t *Key, void *Param )
{
	CCompoundIndex *cdx = (CCompoundIndex *)Param;

	cdx_key_t *Tag = new cdx_key_t;

	memcpy(Tag, Key, sizeof(cdx_key_t));

	cdx->m_Tags.push_back(Tag);
}

//
// Чтение тэгов
//
bool CCompoundIndex::ReadTags( )
{
	ClearTags(); // освобождаем память 

	if ( !ReadKeys(m_Header.root_node, m_Header.key_len, TagEnumProc) ) // Читаем тэги 
	{
		ClearTags();
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szFileName;
		m_sErrorMessage += " ошибка при чтении ключей";
		return false;
	}

	sort(m_Tags.begin(), m_Tags.end(), SortTagsProc); // Сортируем по адресу заголовков тэгов

	m_TagsCount = (unsigned int)m_Tags.size();

	m_sErrorMessage = "";
	return true;
}

//
// Освобождение памяти списка тэгов
//
void CCompoundIndex::ClearTags( )
{
	for ( unsigned int i = 0; i < m_Tags.size(); ++i )
		delete m_Tags[i];

	m_Tags.clear();
	m_TagsCount = 0;
}

//
// Листья
//
bool CCompoundIndex::ReadExteriorKeys( unsigned char *pNode, unsigned int nKeyLen, vector <cdx_key_t *> &Keys )
{
	cdx_exterior_node_t *ExtNode = (cdx_exterior_node_t *)pNode;
	unsigned long long Entry; // 8-ми байтовая переменная

	unsigned int nOffset = 488;

	unsigned char aKey[240];
	unsigned char aKeyPrev[240];

	if ( nKeyLen > 240 )
		return false;

	if ( ExtNode->rec_num_dup_trail_size > 6 )
		return false;

	unsigned char *pEntryData = ExtNode->keys_info;
	for ( int i = 0; i < ExtNode->key_num; ++i )
	{
		memcpy(&Entry, pEntryData, ExtNode->rec_num_dup_trail_size);

		unsigned int  nRecNum     = Entry & ExtNode->rec_num_mask;
		unsigned char nDupCount   = (Entry >> ExtNode->rec_num_bits) & ExtNode->dup_mask;
		unsigned char nTrailCount = nKeyLen - ((unsigned char)(Entry >> (ExtNode->rec_num_bits + ExtNode->dup_bits)) & ExtNode->trail_mask);

		int nLen = nTrailCount - nDupCount;

		nOffset -= nLen;

		memset(aKey, 0, sizeof(aKey));
		if ( nDupCount )
			memcpy(aKey, aKeyPrev, nDupCount); // повторяющаяся часть (берем с предыдущего ключа)
		memcpy(aKey + nDupCount, ExtNode->keys_info + nOffset, nLen); // неповторяющаяся часть

		// добавляем ключ
		cdx_key_t *Key = new cdx_key_t;
		memcpy(Key->value, aKey, sizeof(aKey));
		Key->rec_num = nRecNum;
		Key->intra_node = 0;
		Keys.push_back(Key);

		memcpy(aKeyPrev, aKey, sizeof(aKey)); // запоминаем предыдущий ключ

		pEntryData += ExtNode->rec_num_dup_trail_size;

		if ( pEntryData - ExtNode->keys_info > 488 ) // Проверим превышение размера в случае поврежденного файла
			return false;
	}

	return true;
}

//
// Каталоги
//
bool CCompoundIndex::ReadInteriorKeys( unsigned char *pNode, unsigned int nKeyLen, vector <cdx_key_t *> &Keys )
{
	cdx_interior_node_t *IntNode = (cdx_interior_node_t *)pNode;

	unsigned int  nRecNum;
	unsigned int  nIntraNode;

	unsigned char aKey[240];

	if ( nKeyLen > 240 )
		return false;

	unsigned char *pEntryData = IntNode->keys_info;
	for ( int i = 0; i < IntNode->key_num; ++i )
	{
		memset(aKey, 0, sizeof(aKey));
		
		memcpy(aKey, pEntryData, nKeyLen);
		memcpy(&nRecNum, pEntryData + nKeyLen, 4);
		memcpy(&nIntraNode, pEntryData + nKeyLen + 4, 4);
		
		// добавляем ключ
		cdx_key_t *Key = new cdx_key_t;
		memcpy(Key->value, aKey, sizeof(aKey));
		Key->rec_num = SwapBytes32(nRecNum);
		Key->intra_node = SwapBytes32(nIntraNode);
		Keys.push_back(Key);
		
		pEntryData += (nKeyLen + 4 + 4);

		if ( pEntryData - IntNode->keys_info > 500 ) // Проверим превышение размера в случае поврежденного файла
			return false;
	}

	return true;
}

//
// Чтение ключей
//
bool CCompoundIndex::ReadKeys( unsigned int nAddress, unsigned int nKeyLen, void proc(cdx_key_t *key, void *param) )
{
	DWORD dwError, dwRead;
	unsigned char pNode[512];

	if ( nAddress >= m_FileSize )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szFileName;
		m_sErrorMessage += " адрес узла превышает или равен размеру файла";
		return false;
	}

	FileSeek(m_hFile, nAddress, FILE_BEGIN);
	dwError = FileRead(m_hFile, (BYTE *)pNode, 512, &dwRead);
	if ( dwError != ERROR_SUCCESS )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szFileName;
		m_sErrorMessage += " ";
		m_sErrorMessage += ErrorMessage(dwError);
		return false;
	}

	if ( dwRead != 512 )
	{
		m_sErrorMessage = __FUNCTION__;
		m_sErrorMessage += ": ";
		m_sErrorMessage += m_szFileName;
		m_sErrorMessage += " неожиданный конец файла";
		return false;
	}

	//bool bRoot = (pNode[0] & 0x01);
	bool bLeaf = (pNode[0] & 0x02) ? true : false;
	//bool bKeys = (pNode[0] & 0x04); // ?????

	if ( /*bRoot &&*/ bLeaf ) // root, leaf
	{
		//Keys.clear();

		vector <cdx_key_t *> Keys2;
		ReadExteriorKeys(pNode, nKeyLen, Keys2);
		for ( size_t n = 0; n < Keys2.size(); ++n )
		{
			proc(Keys2[n], this);
			delete Keys2[n];
		}
	}
	else //if ( /*bRoot &&*/ !bLeaf ) // root, not leaf
	{
		vector <cdx_key_t *> iTags;
		ReadInteriorKeys(pNode, nKeyLen, iTags);

		for ( unsigned int i = 0; i < iTags.size(); ++i )
		{
			ReadKeys(iTags[i]->intra_node, nKeyLen, /*Keys*/proc);
			delete iTags[i];
		}
	}
	
	m_sErrorMessage = "";
	return true;
}

//
// Проверка номера (тэга) индекса
//
bool CCompoundIndex::CheckTagNum( int nTagNum )
{
	return (nTagNum >= 0 && nTagNum < (int)m_TagsCount);
}

// 
// Количество тэгов
//
int CCompoundIndex::GetTagsCount( )
{
	return m_TagsCount;
}

// 
// Имя тэга по номеру
//
const char *CCompoundIndex::GetTagName( int nTagNum )
{
	if ( !CheckTagNum(nTagNum) )
		return NULL;

	return m_Tags[nTagNum]->value;
}

//
// Процедура сортировки тэгов 
//
bool CCompoundIndex::SortTagsProc( cdx_key_t *i, cdx_key_t *j ) 
{ 
	return ( i->rec_num < j->rec_num ); 
}


// 
//Сообщение о последней ошибке
//
string CCompoundIndex::GetLastErrorMessage( )
{
	return m_sErrorMessage;
}