///////////////////////////////////////////////////////////////////////////////////
// GMvSystem.c
//---------------------------------------------------------------------------------
// Created by Byeongjae Gim
// Email: gaiama78@gmail.com, byeongjae.kim@samsung.com
///////////////////////////////////////////////////////////////////////////////////

#include "GMvSystem.h"

//
sint32 GMvPacketMakeHeader( uint8 *pu8Dst, EG_MV_PKT_ID ePktId, uint16 u16Arg, uint16 u16PldSize )
{
	struct SGMvPktHdr *psPktHdr;
#if defined DG_MV_PKT_PARITY_RS_10_6
	struct SGMvPktHdr sPktHdr;
#endif

#if defined( DG_MV_PKT_PARITY_CRC16 ) || defined( DG_MV_PKT_PARITY_CRC32 )
	psPktHdr = (struct SGMvPktHdr*)pu8Dst;
#elif defined DG_MV_PKT_PARITY_RS_10_6
	psPktHdr = &sPktHdr;
#else
	psPktHdr = (struct SGMvPktHdr*)pu8Dst;
#endif
	psPktHdr->u8SyncByte = DG_MV_PKT_SYNC_BYTE;
	psPktHdr->u8PktId = ePktId;
	psPktHdr->u16Arg = u16Arg;
	psPktHdr->u16PldSize = u16PldSize;
#if defined( DG_MV_PKT_PARITY_CRC16 )
	*(uint16*)&pu8Dst[sizeof( struct SGMvPktHdr )] = GMvCrc16Encode( pu8Dst, sizeof( struct SGMvPktHdr ) );
#elif defined( DG_MV_PKT_PARITY_CRC32 )
	*(uint32*)&pu8Dst[sizeof( struct SGMvPktHdr )] = GMvCrc32Encode( pu8Dst, sizeof( struct SGMvPktHdr ) );
#elif defined DG_MV_PKT_PARITY_RS_10_6
	GMvRsEncode( pu8Dst, &sPktHdr, sizeof( sPktHdr ) );
#endif

	return sizeof( struct SGMvPktHdr ) + DG_MV_PKT_PARITY_SIZE_HEADER;
}

sint32 GMvPacketMakePayload( uint8 *pu8Dst, sint32 s32DstSize, uint8 *pu8Src, sint32 s32SrcSize )
{
#if defined( DG_MV_PKT_PARITY_CRC16 )
	memcpy( pu8Dst, pu8Src, s32SrcSize );
	*(uint16*)&pu8Dst[s32SrcSize] = GMvCrc16Encode( pu8Src, s32SrcSize );
#elif defined( DG_MV_PKT_PARITY_CRC32 )
	memcpy( pu8Dst, pu8Src, s32SrcSize );
	*(uint32*)&pu8Dst[s32SrcSize] = GMvCrc32Encode( pu8Src, s32SrcSize );
#elif defined DG_MV_PKT_PARITY_RS_10_6
	return GMvRsEncode( pu8Dst, pu8Src, s32SrcSize );
#else
	memcpy( pu8Dst, pu8Src, s32SrcSize );
#endif

	return s32SrcSize + DG_MV_PKT_PARITY_SIZE_HEADER;
}

struct SGMvPktHdr* GMvPacketCheckHeader( uint8 *pu8Pkt, sint32 s32HdrSize )
{
#if defined( DG_MV_PKT_PARITY_CRC16 )
	if( *(uint16*)&pu8Pkt[sizeof( struct SGMvPktHdr )] != GMvCrc16Encode( pu8Pkt, sizeof( struct SGMvPktHdr ) ) )
#elif defined( DG_MV_PKT_PARITY_CRC32 )
	if( *(uint32*)&pu8Pkt[sizeof( struct SGMvPktHdr )] != GMvCrc32Encode( pu8Pkt, sizeof( struct SGMvPktHdr ) ) )
#elif defined DG_MV_PKT_PARITY_RS_10_6
	if( EG_RESULT_SUCCESS != GMvRsDecode( NULL, pu8Pkt, s32HdrSize ) )
#else
	if( 0 )
#endif
	{
		DG_DBG_PRINT_ERROR( _T("Packet Header is Corrupted!") );
		return NULL;
	}
	DG_SAFE_IS_NOT_EQUAL( *pu8Pkt, DG_MV_PKT_SYNC_BYTE, DG_DBG_PRINT_ERROR( _T("*pu8Pkt != DG_MV_PKT_SYNC_BYTE") ); return NULL );

	return (struct SGMvPktHdr*)pu8Pkt;
}

// CRC16
#if defined( DG_MV_PKT_PARITY_CRC16 )
struct CGMvCrc16
{
	uint16 m_u16Polynomial, m_pu16Lut[256];
};

static struct CGMvCrc16 sg_cMvCrc16;

static uint16 SGMvCrc16BitReflect( uint16 u16ReflectMask, sint32 s32BitNum )
{
	sint32 s32BitPos;
	uint16 u16Reflected;

	u16Reflected = 0;
	for( s32BitPos=1; s32BitPos<=s32BitNum; s32BitPos++ )
	{
		if( u16ReflectMask & 1 )
			u16Reflected |= 1 << (s32BitNum - s32BitPos);
		u16ReflectMask >>= 1;
	}

	return u16Reflected;
}

void GMvCrc16Open( void )
{
	uint32 i, j;

	memset( &sg_cMvCrc16, 0, sizeof( struct CGMvCrc16 ) );
	sg_cMvCrc16.m_u16Polynomial = 0x8005;
	for( i=0; i<256; i++ )
	{
		sg_cMvCrc16.m_pu16Lut[i] = SGMvCrc16BitReflect( i, 8 ) << 8;
		for( j=0; j<8; j++ )
			sg_cMvCrc16.m_pu16Lut[i] = (sg_cMvCrc16.m_pu16Lut[i] << 1) ^ ((sg_cMvCrc16.m_pu16Lut[i] & (1 << 15)) ? sg_cMvCrc16.m_u16Polynomial : 0);
		sg_cMvCrc16.m_pu16Lut[i] = SGMvCrc16BitReflect( sg_cMvCrc16.m_pu16Lut[i], 16 );
	}
}

uint16 GMvCrc16Encode( uint8 *pu8Data, sint32 s32Size )
{
	uint16 u16Crc16;

	u16Crc16 = 0;
	while( s32Size-- )
		u16Crc16 = (u16Crc16 >> 8) ^ sg_cMvCrc16.m_pu16Lut[(u16Crc16 & 0xFF) ^ *pu8Data++];

	return u16Crc16;
}
#endif
