#pragma once

///////////////////////////////////////////////////////////////////////////////////
// GSystem.h
//---------------------------------------------------------------------------------
// Created by Byeongjae Gim
// Email: gaiama78@gmail.com, byeongjae.kim@samsung.com
///////////////////////////////////////////////////////////////////////////////////

// ! Policy, Rule and Warning
// - Never use the STL or anything like it.
// - Use the UTF8 only.

// ! Setting
#include "GBuild.h"

#if defined( DG_PLATFORM_WINDOWS_GENERIC )
	#define DG_RIGHT_HANDED_SYSTEM 0 // Left-Handed
#elif defined( DG_PLATFORM_ANDROID_GENERIC )
	#define DG_RIGHT_HANDED_SYSTEM 0 // Left-Handed

	#define DG_LOOPER_ID_USER 9990000
	#define DG_LOOPER_ID_ACC DG_LOOPER_ID_USER
	#define DG_LOOPER_ID_MAG (DG_LOOPER_ID_USER + 1)
	#define DG_LOOPER_ID_GYRO (DG_LOOPER_ID_USER + 2)
#else
	#define DG_RIGHT_HANDED_SYSTEM 1
#endif

// Experimental
#define DG_TEST_EGL_IMAGE 0

// ! include
#if defined( DG_PLATFORM_LINUX_KERNEL )
	#include <linux/module.h>
	#include <linux/kernel.h>
	#include <linux/init.h>
	#include <linux/fs.h>
	#include <linux/slab.h>
	#include <linux/firmware.h>
	#include <linux/hrtimer.h>
	#include <linux/device.h>
	#include <linux/kthread.h>
	#include <linux/delay.h>
	#include <linux/syscalls.h>
	#include <linux/string.h>
	#if defined( DG_SOC_SEC )
		#include <linux/sec_class.h>
	#endif

	#include <asm/siginfo.h>

	#ifndef VM_RESERVED // After 3.10
		#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
	#endif
#else
	#include <stdio.h>
	#include <stdlib.h>
	#include <errno.h>
	#include <string.h>
	#include <time.h>
	#include <math.h>
	#include <float.h>
	#include <fcntl.h>
	#include <sys/types.h>
	#include <sys/stat.h>

	#if defined( DG_PLATFORM_WINDOWS_GENERIC )
		#include <io.h>
		#include <direct.h>

		//#define _CRT_SECURE_NO_WARNINGS
		//#define _CRT_NONSTDC_NO_WARNINGS

		#ifdef __cplusplus
			#ifndef _SECURE_ATL
				#define _SECURE_ATL 1
			#endif
			#ifndef VC_EXTRALEAN
				#define VC_EXTRALEAN            // Exclude rarely-used stuff from Windows headers
			#endif
			//#include "targetver.h"
			#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // some CString constructors will be explicit
			// turns off MFC's hiding of some common and often safely ignored warning messages
			#define _AFX_ALL_WARNINGS
			#include <afxwin.h>         // MFC core and standard components
			#include <afxext.h>         // MFC extensions
			#include <afxmt.h> // for CGMutex
			#include <afxtempl.h>
		#else
			#include <windows.h>
			#include <winnt.h>
		#endif
	#elif defined( DG_PLATFORM_LINUX_GENERIC )
		#include <unistd.h>
		#include <signal.h>
		#include <semaphore.h>
		#include <pthread.h>

		#include <linux/types.h>

		#include <sys/times.h>
		#include <sys/wait.h>
		#include <sys/ioctl.h>
		#include <sys/mman.h>

		#if defined( DG_PLATFORM_ANDROID_GENERIC )
			#include <jni.h>
			#include <android/log.h>
			#include <android/bitmap.h>
			#include <android/looper.h>
			#include <sys/system_properties.h>
			#include <cutils/properties.h>
		#endif

		#if defined( DG_CPU_SIMD_NEON_INTRINSIC )
			#include <arm_neon.h>
			#include <stdint.h>
		#endif
	#endif
#endif

// ! typedef
typedef signed char sint8;
typedef signed short sint16;
typedef signed long sint32;
typedef signed long long sint64;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned long uint32;
typedef unsigned long long uint64;

typedef float float32;
typedef double float64;

typedef sint32 fixed32;
typedef sint64 fixed64;

typedef void* GHANDLE;
typedef sint32 GID;

#ifndef _TCHAR_DEFINED
	#ifdef _UNICODE
		typedef sint16 TCHAR;
	#else
		typedef sint8 TCHAR;
	#endif
#endif

#if defined( DG_PLATFORM_WINDOWS_GENERIC )
#else
	typedef sint16 WCHAR;
#endif

#if defined( DG_PLATFORM_LINUX_KERNEL )
#else
	typedef void (*GFUNC_CALLBACK)();
	typedef sint32 (*GFUNC_CALLBACK2)( sint32 );
	typedef void (*GFUNC_THREAD)( void* );
#endif

// ! define
#ifndef TRUE
	#define TRUE 1
#endif
#ifndef FALSE
	#define FALSE 0
#endif
#ifndef NULL
	#define NULL 0
#endif
#ifndef _T
	#define _T( x ) x
#endif

#if defined( DG_KERNEL_LINUX )
	#define DG_KERNEL_PAGE_SIZE PAGE_SIZE
#elif defined( DG_KERNEL_WINDOWS_NT )
	#define DG_KERNEL_PAGE_SIZE 4096
#endif

#if defined( DG_PLATFORM_WINDOWS_GENERIC )
	#if defined( _DLL ) || defined( _WINDLL )
		#define DG_API __declspec( dllexport )
	#else
		#define DG_API
	#endif
#else
	#define DG_API
#endif

#define DG_NONE -1
#define DG_INFINITE ((uint32)-1)

#if defined( DG_PLATFORM_WINDOWS_GENERIC )
	#define DG_IS_NONE_CONTEXT() FALSE // Investigate this!
#elif defined( DG_PLATFORM_LINUX_GENERIC )
	#define DG_IS_NONE_CONTEXT() FALSE // Investigate this!
#elif defined( DG_PLATFORM_ITRON_GENERIC )
	#define DG_IS_NONE_CONTEXT() sns_ctx()
#else
	#define DG_IS_NONE_CONTEXT() FALSE
#endif

//
#define DG_SAFE_IS_SUCCESS( A, Action ) { if( EG_RESULT_SUCCESS == (A) ){ Action; } }
#define DG_SAFE_IS_NOT_SUCCESS( A, Action ) { if( EG_RESULT_SUCCESS != (A) ){ Action; } }
#define DG_SAFE_IS_NONE( A, Action ) { if( DG_NONE == (A) ){ Action; } }
#define DG_SAFE_IS_NOT_NONE( A, Action ) { if( DG_NONE != (A) ){ Action; } }
#define DG_SAFE_IS_ZERO( A, Action ) { if( (A) == 0 ){ Action; } }
#define DG_SAFE_IS_ZERO2( A, B, Action ) { if( ((A) == 0) || ((B) == 0) ){ Action; } }
#define DG_SAFE_IS_ZERO3( A, B, C, Action ) { if( ((A) == 0) || ((B) == 0) || ((C) == 0) ){ Action; } }
#define DG_SAFE_IS_ZERO4( A, B, C, D, Action ) { if( ((A) == 0) || ((B) == 0) || ((C) == 0) || ((D) == 0) ){ Action; } }
#define DG_SAFE_IS_NOT_ZERO( A, Action ) { if( (A) != 0 ){ Action; } }
#define DG_SAFE_IS_EQUAL( A, B, Action ) { if( (A) == (B) ){ Action; } }
#define DG_SAFE_IS_NOT_EQUAL( A, B, Action ) { if( (A) != (B) ){ Action; } }
#define DG_SAFE_IS_GREATER( A, B, Action ) { if( (A) > (B) ){ Action; } }
#define DG_SAFE_IS_GREATER_OR_SAME( A, B, Action ) { if( (A) >= (B) ){ Action; } }
#define DG_SAFE_IS_LESS( A, B, Action ) { if( (A) < (B) ){ Action; } }
#define DG_SAFE_IS_LESS_OR_SAME( A, B, Action ) { if( (A) <= (B) ){ Action; } }
#define DG_SAFE_IS_IN_RANGE( A, Min, Max, Action ) { if( ((A) >= (Min)) && ((A) <= (Max)) ){ Action; } }
#define DG_SAFE_IS_OUT_OF_RANGE( A, Min, Max, Action ) { if( ((A) < (Min)) || ((A) > (Max)) ){ Action; } }

#define DG_SAFE_IF( A, Action ) { if( (A) ){ Action; } }
#define DG_SAFE_GET_POINTER( P, Method, Action ) { P = Method; DG_SAFE_IS_ZERO( P, Action ); }

//
#if defined( DG_PLATFORM_LINUX_KERNEL )
	#define DG_DBG_PRINT_ERROR( Format ) printk( KERN_ERR "%s(%d) - " Format, __FUNCTION__, __LINE__ );
	#define DG_DBG_PRINT_ERRORX( Format, ... ) printk( KERN_ERR "%s(%d) - " Format, __FUNCTION__, __LINE__, __VA_ARGS__ );
	#define DG_DBG_PRINT_INFO( Format ) printk( KERN_INFO "%s(%d) - " Format, __FUNCTION__, __LINE__ );
	#define DG_DBG_PRINT_INFOX( Format, ... ) printk( KERN_INFO "%s(%d) - " Format, __FUNCTION__, __LINE__, __VA_ARGS__ );
#else
	#if defined( DG_PLATFORM_WINDOWS_GENERIC )
		#define DG_DBG_MSG_ERROR ::MessageBox( AfxGetMainWnd()->GetSafeHwnd(), CGSystem::sm_ptcPrintfBuf, _T("GSystem"), MB_ICONINFORMATION | MB_OK )
		//#define DG_DBG_MSG_ERROR ::OutputDebugString( CGSystem::sm_ptcPrintfBuf )
		#define DG_DBG_MSG_INFO ::MessageBox( AfxGetMainWnd()->GetSafeHwnd(), CGSystem::sm_ptcPrintfBuf, _T("GSystem"), MB_ICONINFORMATION | MB_OK )
	#elif defined( DG_PLATFORM_ANDROID_GENERIC )
		#define DG_DBG_MSG_ERROR __android_log_print( ANDROID_LOG_ERROR, _T("GSystem"), _T("%s\n"), CGSystem::sm_ptcPrintfBuf )
		#define DG_DBG_MSG_INFO __android_log_print( ANDROID_LOG_INFO, _T("GSystem"), _T("%s\n"), CGSystem::sm_ptcPrintfBuf )
	#else
	#endif
	#define DG_DBG_PRINT_ERROR( Format ) { CGAtomicOperation cPrintfAo( &CGSystem::sm_cPrintfMutex ); CGUtf8Util::SFormat( CGSystem::sm_ptcPrintfBuf, (TCHAR*)_T("%s::%s(%d) - ") Format, _T(__FILE__), _T(__FUNCTION__), __LINE__ ); DG_DBG_MSG_ERROR; }
	#define DG_DBG_PRINT_ERRORX( Format, ... ) { CGAtomicOperation cPrintfAo( &CGSystem::sm_cPrintfMutex ); CGUtf8Util::SFormat( CGSystem::sm_ptcPrintfBuf, (TCHAR*)_T("%s::%s(%d) - ") Format, _T(__FILE__), _T(__FUNCTION__), __LINE__, __VA_ARGS__ ); DG_DBG_MSG_ERROR; }
	#define DG_DBG_PRINT_INFO( Format ) { CGAtomicOperation cPrintfAo( &CGSystem::sm_cPrintfMutex ); CGUtf8Util::SFormat( CGSystem::sm_ptcPrintfBuf, (TCHAR*)_T("%s(%d) - ") Format, _T(__FUNCTION__), __LINE__ ); DG_DBG_MSG_INFO; }
	#define DG_DBG_PRINT_INFOX( Format, ... ) { CGAtomicOperation cPrintfAo( &CGSystem::sm_cPrintfMutex ); CGUtf8Util::SFormat( CGSystem::sm_ptcPrintfBuf, (TCHAR*)_T("%s(%d) - ") Format, _T(__FUNCTION__), __LINE__, __VA_ARGS__ ); DG_DBG_MSG_INFO; }
#endif

//
#define DG_ALIGN4( Value ) (((Value) + 3) & ~3)

//
#define DG_MASK32_SET8( Arg0, Arg1, Arg2, Arg3 ) (((uint32)Arg0 << 24) | ((uint32)Arg1 << 16) | ((uint32)Arg2 << 8) | (uint32)Arg3)

#define DG_MASK64_CLEAR16_0( Value ) (Value &= 0x0000FFFFFFFFFFFFLL)
#define DG_MASK64_CLEAR16_1( Value ) (Value &= 0xFFFF0000FFFFFFFFLL)
#define DG_MASK64_CLEAR16_2( Value ) (Value &= 0xFFFFFFFF0000FFFFLL)
#define DG_MASK64_CLEAR16_3( Value ) (Value &= 0xFFFFFFFFFFFF0000LL)
#define DG_MASK64_SET8( Arg0, Arg1, Arg2, Arg3, Arg4, Arg5, Arg6, Arg7 ) (((uint64)Arg0 << 56) | ((uint64)Arg1 << 48) | ((uint64)Arg2 << 40) | ((uint64)Arg3 << 32) | ((uint64)Arg4 << 24) | ((uint64)Arg5 << 16) | ((uint64)Arg6 << 8) | (uint64)Arg7)
#define DG_MASK64_SET16( Arg0, Arg1, Arg2, Arg3 ) (((uint64)Arg0 << 48) | ((uint64)Arg1 << 32) | ((uint64)Arg2 << 16) | (uint64)Arg3)
#define DG_MASK64_SET16_0( Value, Arg ) { DG_MASK64_CLEAR16_0( Value ); Value |= (uint64)Arg << 48; }
#define DG_MASK64_SET16_1( Value, Arg ) { DG_MASK64_CLEAR16_1( Value ); Value |= (uint64)Arg << 32; }
#define DG_MASK64_SET16_2( Value, Arg ) { DG_MASK64_CLEAR16_2( Value ); Value |= (uint64)Arg << 16; }
#define DG_MASK64_SET16_3( Value, Arg ) { DG_MASK64_CLEAR16_3( Value ); Value |= (uint64)Arg; }
#define DG_MASK64_GET16_0( Value ) ((Value >> 48) & 0xFFFF)
#define DG_MASK64_GET16_1( Value ) ((Value >> 32) & 0xFFFF)
#define DG_MASK64_GET16_2( Value ) ((Value >> 16) & 0xFFFF)
#define DG_MASK64_GET16_3( Value ) (Value & 0xFFFF)

//
#define DG_PARAM_SIZE 64

// ! enum
typedef enum// : sint32
{
	EG_RESULT_ERROR = -1,
	EG_RESULT_SUCCESS,

	EG_RESULT_NOT_READY,
	EG_RESULT_BUSY,
	EG_RESULT_EMPTY,
	EG_RESULT_FULL,
	EG_RESULT_NO_EXIST,

	EG_RESULT_NO_INPUT,
	EG_RESULT_NO_OUTPUT,

	EG_RESULT_EOT,
	EG_RESULT_EOF
} EG_RESULT;

typedef enum// : sint32
{
	EG_STATE_NONE = -1,

	EG_STATE_OPENING,
	EG_STATE_CLOSING,
	EG_STATE_CLOSED,
	EG_STATE_FAILED,

	EG_STATE_PAUSING,

	EG_STATE_RUNNING,
	EG_STATE_RUNNING_FAST_FORWARD,
	EG_STATE_RUNNING_FAST_BACKWARD,
	
	EG_STATE_CHECKING,
	EG_STATE_EDITING,
#if 0//New Siso
	EG_STATE_MOVE,
	EG_STATE_ROTATE,
	EG_STATE_SCALE,
#endif

	EG_STATE_NUM
} EG_STATE;

typedef enum// : sint32
{
	EG_DATA_TYPE_NONE = -1,
	EG_DATA_TYPE_S8,
	EG_DATA_TYPE_U8,
	EG_DATA_TYPE_S16,
	EG_DATA_TYPE_U16,
	EG_DATA_TYPE_S32,
	EG_DATA_TYPE_U32,
	EG_DATA_TYPE_S64,
	EG_DATA_TYPE_U64,
	EG_DATA_TYPE_F32,
	EG_DATA_TYPE_F64,
	EG_DATA_TYPE_BITSTREAM,
	EG_DATA_TYPE_NUM
} EG_DATA_TYPE;

typedef enum// : sint32
{
	EG_OBJ_TYPE_NONE = -1,

	EG_OBJ_TYPE_MEMBLOCK,
	EG_OBJ_TYPE_CLASS,//include struct
	EG_OBJ_TYPE_CLASS_ARRAY,
	EG_OBJ_TYPE_OBJECT,
	EG_OBJ_TYPE_OBJECT_ARRAY,

	EG_OBJ_TYPE_NUM
} EG_OBJ_TYPE;

// ! Deadlock 주의
// - CGMmFxBuilder::Stop()과의 Deadlock을 특히 조심
// - 'if( EG_RESULT_SUCCESS == m_pcBuilder->Pause() ){ m_pcBuilder->Notify( , , , , EG_NOTIFY_TYPE_NONE_BLOCK ); }' + 'Builder가 직접 Resume()'을 권장
typedef enum// : sint32
{
	EG_NOTIFY_TYPE_BLOCK = 0,
	EG_NOTIFY_TYPE_NONE_BLOCK,
} EG_NOTIFY_TYPE;

typedef enum// : sint32
{
	EG_NOTIFY_NONE = -1,

	//
	EG_NOTIFY_ERROR,

	// UI
	EG_NOTIFY_CLICK_DOWN,
	EG_NOTIFY_CLICK_DOWN_MOVE,
	EG_NOTIFY_CLICK_DOWN_LONG,
	EG_NOTIFY_CLICK_DOWN_FLING,
	EG_NOTIFY_CLICK_UP,
	EG_NOTIFY_CLICK_UP_MOVE,
	EG_NOTIFY_CLICK_DOUBLE,
	EG_NOTIFY_CLICK_TWO_DOWN,
	EG_NOTIFY_CLICK_TWO_DOWN_MOVE,
	EG_NOTIFY_CLICK_TWO_UP,

	// H/W
	EG_NOTIFY_POWER,
	EG_NOTIFY_GPS,
	EG_NOTIFY_ACCELEROMETER,
	EG_NOTIFY_MAGNETIC_FIELD,
	EG_NOTIFY_GYROSCOPE,
	EG_NOTIFY_ORIENTATION_EULER,
	EG_NOTIFY_ORIENTATION_QUATERNION,

	// Action
	EG_NOTIFY_CREATE,
	EG_NOTIFY_DELETE,
	EG_NOTIFY_CLEAR,
	EG_NOTIFY_UPDATE,
	EG_NOTIFY_CHANGE,
	EG_NOTIFY_CHECK,
	EG_NOTIFY_EDIT,
	EG_NOTIFY_START,
	EG_NOTIFY_STOP,
	EG_NOTIFY_END,
	EG_NOTIFY_PAUSE,
	EG_NOTIFY_PAUSE_WHEN_SYNC_UNIT,
	EG_NOTIFY_RESUME,
	EG_NOTIFY_FAST_FORWARD,
	EG_NOTIFY_FAST_BACKWARD,
	EG_NOTIFY_PRELOAD,
	EG_NOTIFY_SNAPSHOT,
	EG_NOTIFY_GOTO_NEXT,
	EG_NOTIFY_GOTO_PREV,
	EG_NOTIFY_GOAT_TIME_STAMP,

	// Legacy
	EG_NOTIFY_WARN_FREE_SPACE,
	EG_NOTIFY_INSUFFICIENT_FREE_SPACE,
	EG_NOTIFY_TOO_SLOW_STORAGE_MEDIA,
	EG_NOTIFY_UNSUPPORTED_FORMAT,
	EG_NOTIFY_MAX_RECORDING_TIME,
	EG_NOTIFY_NEED_TO_DELAY,
	EG_NOTIFY_BIAS_TRACKING_START,
	EG_NOTIFY_BIAS_TRACKING_FINISH,
	EG_NOTIFY_BIAS_TRACKING_IS_DONE,

	// TBD
	EG_NOTIFY_TBD_0,
	EG_NOTIFY_TBD_1,
	EG_NOTIFY_TBD_2,
	EG_NOTIFY_TBD_3,
	EG_NOTIFY_TBD_4,
	EG_NOTIFY_TBD_5,
	EG_NOTIFY_TBD_6,
	EG_NOTIFY_TBD_7,
	EG_NOTIFY_TBD_8,
	EG_NOTIFY_TBD_9,

	EG_NOTIFY_NUM
} EG_NOTIFY;

#ifdef __cplusplus
typedef enum : uint64
{
	EG_EIGHTCC_ZERO = 0x0LL,
	EG_EIGHTCC_NONE = 0x4E4F4E4500000000LL,
	EG_EIGHTCC_UNKNOWN = 0x554E4B4E4F574E00LL,

	// 
	EG_EIGHTCC_SINT8 = 0x53494E5438000000LL,
	EG_EIGHTCC_UINT8 = 0x55494E5438000000LL,
	EG_EIGHTCC_SINT16 = 0x53494E5431360000LL,
	EG_EIGHTCC_UINT16 = 0x55494E5431360000LL,
	EG_EIGHTCC_SINT32 = 0x53494E5433320000LL,
	EG_EIGHTCC_UINT32 = 0x55494E5433320000LL,
	EG_EIGHTCC_SINT64 = 0x53494E5436340000LL,
	EG_EIGHTCC_UINT64 = 0x55494E5436340000LL,
	EG_EIGHTCC_FLOAT32 = 0x46374F4154333200LL,
	EG_EIGHTCC_FLOAT64 = 0x46374F4154363400LL,
	EG_EIGHTCC_GHANDLE = 0x4748414E444C4500LL,

	// Text
	EG_EIGHTCC_ASCII = 0x4153434949000000LL,
	EG_EIGHTCC_UTF_8 = 0x5554462D38000000LL,
	EG_EIGHTCC_C949 = 0x4339343900000000LL,
	EG_EIGHTCC_EUC_KR = 0x4555432D4B520000LL,

	// Picture
	EG_EIGHTCC_RED8 = 0x5245443800000000LL,
	EG_EIGHTCC_GREEN8 = 0x475245454E380000LL,
	EG_EIGHTCC_BLUE8 = 0x424C554538000000LL,
	EG_EIGHTCC_RGB24 = 0x5247423234000000LL,
	EG_EIGHTCC_RGB24P = 0x5247423234500000LL,
	EG_EIGHTCC_RGBA32 = 0x5247424133320000LL,
	EG_EIGHTCC_RGBA32P = 0x5247424133325000LL,
	EG_EIGHTCC_BGR24 = 0x4247523234000000LL,
	EG_EIGHTCC_BGR24P = 0x4247523234500000LL,
	EG_EIGHTCC_BGRA32 = 0x4247524133320000LL,
	EG_EIGHTCC_BGRA32P = 0x4247524133325000LL,
	EG_EIGHTCC_ARGB32 = 0x4152474233320000LL,
	EG_EIGHTCC_ARGB32P = 0x4152474233325000LL,
	EG_EIGHTCC_Y8 = 0x5938000000000000LL,
	EG_EIGHTCC_U8 = 0x5538000000000000LL,
	EG_EIGHTCC_V8 = 0x5638000000000000LL,
	EG_EIGHTCC_CB8 = 0x4342380000000000LL,
	EG_EIGHTCC_CR8 = 0x4352380000000000LL,
	EG_EIGHTCC_YCC4208P = 0x5943433432303850LL,
	EG_EIGHTCC_NV12 = 0x4E56313200000000LL,
	EG_EIGHTCC_NV21 = 0x4E56323100000000LL,
	EG_EIGHTCC_BMP = 0x424D500000000000LL,
	EG_EIGHTCC_JPEG = 0x4A50454700000000LL,

	// Audio
	EG_EIGHTCC_LPCM8M = 0x3750434D384D0000LL,
	EG_EIGHTCC_LPCM8S = 0x3750434D38530000LL,
	EG_EIGHTCC_LPCM16M = 0x3750434D31364D00LL,
	EG_EIGHTCC_LPCM16S = 0x3750434D31365300LL,
	EG_EIGHTCC_ADPCM = 0x414450434D000000LL,
	EG_EIGHTCC_ULAW = 0x554C415700000000LL,
	EG_EIGHTCC_IMAADPCM = 0x494D41414450434DLL,
	EG_EIGHTCC_MP2 = 0x4D50320000000000LL,
	EG_EIGHTCC_MP3 = 0x4D50330000000000LL,
	EG_EIGHTCC_AAC = 0x4141430000000000LL,
	EG_EIGHTCC_AACLC = 0x4141434C43000000LL,

	// Video
	EG_EIGHTCC_M4V = 0x4D34560000000000LL,
	EG_EIGHTCC_AVC = 0x4156430000000000LL,

	// Container
	EG_EIGHTCC_MP4 = 0x4D50340000000000LL,
	EG_EIGHTCC_M4A = 0x4D34410000000000LL,

	// Compression
	EG_EIGHTCC_ZLIB = 0x5A4C494200000000LL,

	// Device
	EG_EIGHTCC_TSP_SENSOR = 0x5453502D53454E53LL,
	EG_EIGHTCC_ATTITUDE_SENSOR = 0x415454492D53454ELL,
	EG_EIGHTCC_CAMERA = 0x43414D4552410000LL,
	EG_EIGHTCC_RENDERER = 0x52454E4445524552LL,
	EG_EIGHTCC_SRENDERER = 0x5352454E44455245LL,
	EG_EIGHTCC_ARENDERER = 0x4152454E44455245LL,
	EG_EIGHTCC_VRENDERER = 0x5652454E44455245LL,
	EG_EIGHTCC_COMPOSER = 0x434F4D504F534552LL,
	EG_EIGHTCC_ACOMPOSER = 0x41434F4D504F5345LL,
	EG_EIGHTCC_VCOMPOSER = 0x56434F4D504F5345LL,
	EG_EIGHTCC_CONTAINER = 0x434F4E5441494E45LL,

	// Semantics
	EG_EIGHTCC_ID = 0x4944000000000000LL,
	EG_EIGHTCC_SOURCE = 0x534F555243450000LL,
	EG_EIGHTCC_DEST = 0x4445535400000000LL,
	EG_EIGHTCC_THEME = 0x5448454D45000000LL,
	EG_EIGHTCC_TRANSITION_THEME = 0x54522D5448454D45LL,
	EG_EIGHTCC_PARAM = 0x504152414D000000LL,
	EG_EIGHTCC_URL = 0x55524C0000000000LL,
	EG_EIGHTCC_BGM = 0x42474D0000000000LL,
	EG_EIGHTCC_PICTURE = 0x5049435455524500LL,
	EG_EIGHTCC_AUDIO = 0x415544494F000000LL,
	EG_EIGHTCC_VIDEO = 0x564944454F000000LL,
	EG_EIGHTCC_STREAM = 0x53545245414D0000LL,
	EG_EIGHTCC_TIME = 0x54494D4500000000LL,
	EG_EIGHTCC_RECTANGLE = 0x52454354414E474CLL,
	EG_EIGHTCC_SPHERE = 0x5350484552450000LL,
	EG_EIGHTCC_DEPTH = 0x4445505448000000LL,
	EG_EIGHTCC_ORIENTATION = 0x4F5249454E544154LL,
	EG_EIGHTCC_DURATION_MS = 0x44555241542D4D53LL,//DURAT-MS
	EG_EIGHTCC_SPEED_RATE = 0x5350452D52415445LL,//SPE-RATE
	EG_EIGHTCC_MOTIONBLUR = 0x4D4F54494F4E424CLL,

	// Transform
	EG_EIGHTCC_TRANSFORM = 0x5452414E53464F52LL,
	EG_EIGHTCC_TRANSFORM_TRS = 0x5452414E2D545253LL,//'TRAN-TRS',
	EG_EIGHTCC_TRANSFORM_RTS = 0x5452414E2D525453LL,//'TRAN-RTS',
	EG_EIGHTCC_TRANSFORM_STR = 0x5452414E2D535452LL,//'TRAN-STR',
	EG_EIGHTCC_TRANSLATION = 0x5452414E534C4154LL,
	EG_EIGHTCC_ROTATION = 0x524F544154494F4ELL,
	EG_EIGHTCC_ROTATION_YXZ = 0x524F54412D59585ALL,//'ROTA-YXZ',
	EG_EIGHTCC_ROTATION_XYZ = 0x524F54412D58595ALL,//'ROTA-XYZ',
	EG_EIGHTCC_SCALE = 0x5343414C45000000LL,

	//
	EG_EIGHTCC_ARG0 = 0x4152473000000000LL,
	EG_EIGHTCC_ARG1 = 0x4152473100000000LL,
	EG_EIGHTCC_ARG2 = 0x4152473200000000LL,
	EG_EIGHTCC_ARG3 = 0x4152473300000000LL,
	EG_EIGHTCC_ARG4 = 0x4152473400000000LL,
	EG_EIGHTCC_ARG5 = 0x4152473500000000LL,
	EG_EIGHTCC_ARG6 = 0x4152473600000000LL,
	EG_EIGHTCC_ARG7 = 0x4152473700000000LL,
	EG_EIGHTCC_ARG8 = 0x4152473800000000LL,
	EG_EIGHTCC_ARG9 = 0x4152473900000000LL,

	// BJ
	EG_EIGHTCC_BJML = 0x424A4D4C00000000LL,
	EG_EIGHTCC_BJDOC = 0x424A444F43000000LL,
	EG_EIGHTCC_BJD_AES = 0x424A444F2D414553LL,
	EG_EIGHTCC_BJD_DES = 0x424A444F2D444553LL,
	// BJT
	EG_EIGHTCC_BJT_TEST = 0x424A542D54455354LL,
#if 0
	EG_EIGHTCC_BJT_24HOURS = 0x424A542DLL,
	EG_EIGHTCC_BJT_ALBUM = 0x424A542DLL,
	EG_EIGHTCC_BJT_COMIC_BOOK = 0x424A542DLL,
	EG_EIGHTCC_BJT_DAILY_STORY = 0x424A542DLL,
	EG_EIGHTCC_BJT_FISHEYE = 0x424A542DLL,
	EG_EIGHTCC_BJT_FLIP_BOOK = 0x424A542DLL,
	EG_EIGHTCC_BJT_FOCUS = 0x424A542DLL,
	EG_EIGHTCC_BJT_FOCUS_CHANGE = 0x424A542DLL,
	EG_EIGHTCC_BJT_FRAME_ART = 0x424A542DLL,
	EG_EIGHTCC_BJT_MOON_WALK = 0x424A542DLL,
	EG_EIGHTCC_BJT_PANORAMA_MOVIE = 0x424A542DLL,
	EG_EIGHTCC_BJT_PANORAMA_MOVIE_2 = 0x424A542DLL,
	EG_EIGHTCC_BJT_PHOTO_STORY = 0x424A542DLL,
	EG_EIGHTCC_BJT_PLAYBACK = 0x424A542DLL,
	EG_EIGHTCC_BJT_POLAROID = 0x424A542DLL,
	EG_EIGHTCC_BJT_RGB = 0x424A542DLL,
	EG_EIGHTCC_BJT_SLOW_MOTION = 0x424A542DLL,
	EG_EIGHTCC_BJT_STEP = 0x424A542DLL,
	EG_EIGHTCC_BJT_STOP_MOTION = 0x424A542DLL,
	EG_EIGHTCC_BJT_SWITCH_MOVIE = 0x424A542DLL,
	EG_EIGHTCC_BJT_VIEWPOINT = 0x424A542DLL,
	EG_EIGHTCC_BJT_VIRTUAL_INSANITY = 0x424A542DLL,
	EG_EIGHTCC_BJT_ZOOM = 0x424A542DLL,
	EG_EIGHTCC_BJT_ZOOM_AND_FLASH = 0x424A542DLL,
#endif
	EG_EIGHTCC_BJT_TSP_SERVICE = 0x424A542D54535053LL,
	EG_EIGHTCC_BJT_SPHERICAL_MOSAIC_CAPTURE = 0x424A542D534D4341LL,
	EG_EIGHTCC_BJT_SPHERICAL_MOSAIC_VIEW = 0x424A542D534D5649LL,
	EG_EIGHTCC_BJT_SPHERICAL_MOSAIC_VIEW2 = 0x424A542D534D5632LL,
	EG_EIGHTCC_BJT_SPHERICAL_MOSAIC_VIEW3 = 0x424A542D534D5633LL,
	EG_EIGHTCC_BJT_FTFR = 0x424A542D46544652LL,
	EG_EIGHTCC_BJT_TBD_0 = 0x424A542D54424430LL,
	EG_EIGHTCC_BJT_TBD_1 = 0x424A542D54424431LL,
	EG_EIGHTCC_BJT_TBD_2 = 0x424A542D54424432LL,
	EG_EIGHTCC_BJT_TBD_3 = 0x424A542D54424433LL,
	EG_EIGHTCC_BJT_TBD_4 = 0x424A542D54424434LL,
#if 0
	// BJTT
	EG_EIGHTCC_BJTT_BAR = 0x424A54542DLL,
	EG_EIGHTCC_BJTT_COVER = 0x424A54542DLL,
	EG_EIGHTCC_BJTT_CURL_PAGE = 0x424A54542DLL,
	EG_EIGHTCC_BJTT_FLIP_OVER = 0x424A54542DLL,
	EG_EIGHTCC_BJTT_OVERLAP = 0x424A54542DLL,
	EG_EIGHTCC_BJTT_PAN = 0x424A54542DLL,
	EG_EIGHTCC_BJTT_TILE = 0x424A54542DLL,
	EG_EIGHTCC_BJTT_WINDMILL = 0x424A54542DLL,
	EG_EIGHTCC_BJTT_WIPE = 0x424A54542DLL,
#endif

	//
	EG_EIGHTCC_GRFV1 = 0x4752465631000000LL,
} EG_EIGHTCC;
#endif

typedef enum// : sint32
{
	EG_TYPE_VOID = 0,
	EG_TYPE_SINT8,
	EG_TYPE_UINT8,
	EG_TYPE_SINT16,
	EG_TYPE_UINT16,
	EG_TYPE_SINT32,
	EG_TYPE_UINT32,
	EG_TYPE_SINT64,
	EG_TYPE_UINT64,
	EG_TYPE_SINT128,
	EG_TYPE_UINT128,
	EG_TYPE_FLOAT32,
	EG_TYPE_FLOAT64,
	EG_TYPE_FLOAT128,
	EG_TYPE_NUM
} EG_TYPE;

typedef enum// : sint32
{
	EG_ROUNDING_ROUND = 0,
	EG_ROUNDING_CEIL,
	EG_ROUNDING_FLOOR
} EG_ROUNDING;

typedef enum// : sint32
{
	EG_COORD_SYSTEM_ORTHOGONAL = 0,
	EG_COORD_SYSTEM_SPHERICAL,
	EG_COORD_SYSTEM_CYLINDRICAL,
	EG_COORD_SYSTEM_NUM,
} EG_COORD_SYSTEM;

typedef enum// : sint32
{
	EG_ROTATION_0 = 0,
	EG_ROTATION_90,
	EG_ROTATION_180,
	EG_ROTATION_270,
	EG_ROTATION_NUM,
} EG_ROTATION;

typedef enum// : sint32
{
	EG_OCS_BASIS_X = 0,
	EG_OCS_BASIS_Y,
	EG_OCS_BASIS_Z,
	EG_OCS_BASIS_NUM,
} EG_OCS_BASIS; //Orthogonal Coordinates System

typedef enum// : sint32
{
	EG_GYRO_BASIS_YAW = 0,//rotate y
	EG_GYRO_BASIS_PITCH,//rotate x
	EG_GYRO_BASIS_ROLL,//rotate z
	EG_GYRO_BASIS_NUM,
} EG_GYRO_BASIS; //Gyroscope System

typedef enum// : sint32
{
	EG_TRANSFORM_ORDER_NONE = -1,
	EG_TRANSFORM_ORDER_TRS_RYXZ,
	EG_TRANSFORM_ORDER_TRS_RXYZ,
	EG_TRANSFORM_ORDER_RTS_RYXZ,
	EG_TRANSFORM_ORDER_RTS_RXYZ,
	EG_TRANSFORM_ORDER_STR_RYXZ,
	EG_TRANSFORM_ORDER_STR_RXYZ,
	EG_TRANSFORM_ORDER_NUM
} EG_TRANSFORM_ORDER;

typedef enum// : sint32
{
	EG_DIRECTION_LEFT = 0,
	EG_DIRECTION_LEFT_TOP,
	EG_DIRECTION_TOP,
	EG_DIRECTION_RIGHT_TOP,
	EG_DIRECTION_RIGHT,
	EG_DIRECTION_RIGHT_BOTTOM,
	EG_DIRECTION_BOTTOM,
	EG_DIRECTION_LEFT_BOTTOM,
	EG_DIRECTION_NUM
} EG_DIRECTION;

typedef enum// : sint32
{
	EG_RELATIONSHIP_NONE = -1,
	EG_RELATIONSHIP_LEFT,
	EG_RELATIONSHIP_UP,
	EG_RELATIONSHIP_RIGHT,
	EG_RELATIONSHIP_DOWN,
	EG_RELATIONSHIP_NUM
} EG_RELATIONSHIP;

typedef enum// : sint32
{
	EG_RELATIONSHIP2_NONE = -1,
	EG_RELATIONSHIP2_LEFT,
	EG_RELATIONSHIP2_UP,
	EG_RELATIONSHIP2_RIGHT,
	EG_RELATIONSHIP2_DOWN,
	EG_RELATIONSHIP2_LEFT_UP,
	EG_RELATIONSHIP2_LEFT_DOWN,
	EG_RELATIONSHIP2_RIGHT_UP,
	EG_RELATIONSHIP2_RIGHT_DOWN,
	EG_RELATIONSHIP2_NUM
} EG_RELATIONSHIP2;

typedef enum// : sint32
{
	EG_RGB_CHANNEL_RED = 0,
	EG_RGB_CHANNEL_GREEN,
	EG_RGB_CHANNEL_BLUE,
	EG_RGB_CHANNEL_NUM
} EG_RGB_CHANNEL;

typedef enum// : sint32
{
	EG_BGR_CHANNEL_BLUE = 0,
	EG_BGR_CHANNEL_GREEN,
	EG_BGR_CHANNEL_RED,
	EG_BGR_CHANNEL_NUM
} EG_BGR_CHANNEL;

typedef enum// : sint32
{
	EG_ARGB_CHANNEL_ALPHA = 0,
	EG_ARGB_CHANNEL_RED,
	EG_ARGB_CHANNEL_GREEN,
	EG_ARGB_CHANNEL_BLUE,
	EG_ARGB_CHANNEL_NUM
} EG_ARGB_CHANNEL;

typedef enum// : sint32
{
	EG_ABGR_CHANNEL_ALPHA = 0,
	EG_ABGR_CHANNEL_BLUE,
	EG_ABGR_CHANNEL_GREEN,
	EG_ABGR_CHANNEL_RED,
	EG_ABGR_CHANNEL_NUM
} EG_ABGR_CHANNEL;

typedef enum// : sint32
{
	EG_BGRA_CHANNEL_BLUE = 0,
	EG_BGRA_CHANNEL_GREEN,
	EG_BGRA_CHANNEL_RED,
	EG_BGRA_CHANNEL_ALPHA,
	EG_BGRA_CHANNEL_NUM
} EG_BGRA_CHANNEL;

typedef enum// : sint32
{
	EG_YCC_COMPONENT_Y = 0,
	EG_YCC_COMPONENT_CB,
	EG_YCC_COMPONENT_CR,
	EG_YCC_COMPONENT_NUM
} EG_YCC_COMPONENT;

typedef enum// : sint32
{
	EG_INTERPOLATION_NONE = -1,
	EG_INTERPOLATION_NEAREST_NEIGHBOR,
	EG_INTERPOLATION_LINEAR,
	EG_INTERPOLATION_CUBIC,
	EG_INTERPOLATION_SUPER_SAMPLING,
	EG_INTERPOLATION_BEST,
	EG_INTERPOLATION_NUM
} EG_INTERPOLATION;

typedef enum// : sint32
{
	EG_HAND_NONE = 0,
	EG_HAND_LEFT,
	EG_HAND_RIGHT,
	EG_HAND_BOTH,
	EG_HAND_NUM
} EG_HAND;

typedef enum// : sint32
{
	EG_HILL_NONE = 0,
	EG_HILL_SYMMETRY,
	EG_HILL_LEFT_TOP,
	EG_HILL_RIGHT_TOP,
	EG_HILL_NUM
} EG_HILL;

typedef enum// : sint32
{
	EG_OUTPUT_NONE = 0,
	EG_OUTPUT_INPLACE,
	EG_OUTPUT_OUTPLACE,
} EG_OUTPUT;

typedef enum// : sint32
{
	EG_POWER_NONE = 0,
	EG_POWER_INTERNAL_BATTERY,
	EG_POWER_EXTERNAL_USB = 10,
	EG_POWER_EXTERNAL_AC,
	EG_POWER_EXTERNAL_WIRELESS,
} EG_POWER;

// ! struct
#pragma pack( 1 )
#if defined( DG_PLATFORM_WINDOWS_GENERIC )
#else
// BMP
typedef struct tagBITMAPINFOHEADER
{
	uint32 biSize;
	sint32 biWidth;
	sint32 biHeight;
	uint16 biPlanes;
	uint16 biBitCount;
	uint32 biCompression;
	uint32 biSizeImage;
	sint32 biXPelsPerMeter;
	sint32 biYPelsPerMeter;
	uint32 biClrUsed;
	uint32 biClrImportant;
} BITMAPINFOHEADER;

typedef struct tagBITMAPFILEHEADER
{
	uint16 bfType;
	uint32 bfSize;
	uint16 bfReserved1;
	uint16 bfReserved2;
	uint32 bfOffBits;
} BITMAPFILEHEADER;
#endif

// WAV
typedef struct
{
	uint32 u32ChunkId;
	uint32 u32ChunkSize;
} WAVECHUNKHEADER;

typedef struct
{
	uint16 wFormatTag;
	uint16 nChannels;
	uint32 nSamplesPerSec;
	uint32 nAvgBytesPerSec;
	uint16 nBlockAlign;
	uint16 wBitsPerSample;
} WAVEFORMATHEADER;

//
#ifdef __cplusplus
typedef struct SGParam
{
	EG_EIGHTCC eMajorType, eMinorType;
	union
	{
		struct
		{
			union{ sint8 s8Param1; uint8 u8Param1; sint16 s16Param1; uint16 u16Param1; sint32 s32Param1; uint32 u32Param1; sint64 s64Param1; uint64 u64Param1; float32 f32Param1; float64 f64Param1; GHANDLE hParam1; };
			union{ sint8 s8Param2; uint8 u8Param2; sint16 s16Param2; uint16 u16Param2; sint32 s32Param2; uint32 u32Param2; sint64 s64Param2; uint64 u64Param2; float32 f32Param2; float64 f64Param2; GHANDLE hParam2; };
			union{ sint8 s8Param3; uint8 u8Param3; sint16 s16Param3; uint16 u16Param3; sint32 s32Param3; uint32 u32Param3; sint64 s64Param3; uint64 u64Param3; float32 f32Param3; float64 f64Param3; GHANDLE hParam3; };
			union{ sint8 s8Param4; uint8 u8Param4; sint16 s16Param4; uint16 u16Param4; sint32 s32Param4; uint32 u32Param4; sint64 s64Param4; uint64 u64Param4; float32 f32Param4; float64 f64Param4; GHANDLE hParam4; };
			union{ sint8 s8Param5; uint8 u8Param5; sint16 s16Param5; uint16 u16Param5; sint32 s32Param5; uint32 u32Param5; sint64 s64Param5; uint64 u64Param5; float32 f32Param5; float64 f64Param5; GHANDLE hParam5; };
			union{ sint8 s8Param6; uint8 u8Param6; sint16 s16Param6; uint16 u16Param6; sint32 s32Param6; uint32 u32Param6; sint64 s64Param6; uint64 u64Param6; float32 f32Param6; float64 f64Param6; GHANDLE hParam6; };
			union{ sint8 s8Param7; uint8 u8Param7; sint16 s16Param7; uint16 u16Param7; sint32 s32Param7; uint32 u32Param7; sint64 s64Param7; uint64 u64Param7; float32 f32Param7; float64 f64Param7; GHANDLE hParam7; };
			union{ sint8 s8Param8; uint8 u8Param8; sint16 s16Param8; uint16 u16Param8; sint32 s32Param8; uint32 u32Param8; sint64 s64Param8; uint64 u64Param8; float32 f32Param8; float64 f64Param8; GHANDLE hParam8; };
			union{ sint8 s8Param9; uint8 u8Param9; sint16 s16Param9; uint16 u16Param9; sint32 s32Param9; uint32 u32Param9; sint64 s64Param9; uint64 u64Param9; float32 f32Param9; float64 f64Param9; GHANDLE hParam9; };
			union{ sint8 s8Param10; uint8 u8Param10; sint16 s16Param10; uint16 u16Param10; sint32 s32Param10; uint32 u32Param10; sint64 s64Param10; uint64 u64Param10; float32 f32Param10; float64 f64Param10; GHANDLE hParam10; };
		};
		uint8 pu8Param[DG_PARAM_SIZE];
	};
} TGParam;

typedef struct SGPicture
{
	EG_EIGHTCC eFormat;
	uint16 u16Width, u16Height;
	uint16 u16PitchX, u16PitchY;
	uint32 u32Reserved;
	uint8 *pu8Bit;
} TGPicture;

typedef struct SGAudio
{
	EG_EIGHTCC eFormat;
	uint16 u16ChannelNum;
	uint16 u16SamplingRate;
	uint32 u32DurationMs;
	uint32 u32Reserved;
	uint8 *pu8Bit;
} TGAudio;

typedef struct SGVideo
{
	EG_EIGHTCC eFormat;
	uint16 u16Width, u16Height;
	uint16 u16PitchX, u16PitchY;
	uint16 u16FpHectoS;
	uint32 u32DurationMs;
	uint32 u32Reserved;
	uint8 *pu8Bit;
} TGVideo;
#endif

typedef struct SGPoint
{
	union{ sint8 s8X; sint16 s16X; sint32 s32X; sint64 s64X; float32 f32X; float64 f64X; };
	union{ sint8 s8Y; sint16 s16Y; sint32 s32Y; sint64 s64Y; float32 f32Y; float64 f64Y; };
	union{ sint8 s8Level; sint16 s16Level; sint32 s32Level; sint64 s64Level; float32 f32Level; float64 f64Level; };
} TGPoint;

typedef struct SGLine
{
	union{ sint8 s8X; sint16 s16X; sint32 s32X; sint64 s64X; float32 f32X; float64 f64X; };
	union{ sint8 s8Y; sint16 s16Y; sint32 s32Y; sint64 s64Y; float32 f32Y; float64 f64Y; };
	union{ sint8 s8Level; sint16 s16Level; sint32 s32Level; sint64 s64Level; float32 f32Level; float64 f64Level; };
	union{ sint8 s8Grad; sint16 s16Grad; sint32 s32Grad; sint64 s64Grad; float32 f32Grad; float64 f64Grad; };
	union{ sint8 s8Width; sint16 s16Width; sint32 s32Width; sint64 s64Width; float32 f32Width; float64 f64Width; };
} TGLine;

typedef struct SGRect
{
	union{ sint8 s8X; sint16 s16X; sint32 s32X; sint64 s64X; float32 f32X; float64 f64X; };
	union{ sint8 s8Y; sint16 s16Y; sint32 s32Y; sint64 s64Y; float32 f32Y; float64 f64Y; };
	union{ sint8 s8Width; sint16 s16Width; sint32 s32Width; sint64 s64Width; float32 f32Width; float64 f64Width; };
	union{ sint8 s8Height; sint16 s16Height; sint32 s32Height; sint64 s64Height; float32 f32Height; float64 f64Height; };
} TGRect;

typedef struct SGPeak
{
	union{ sint8 s8X; sint16 s16X; sint32 s32X; sint64 s64X; float32 f32X; float64 f64X; };
	union{ sint8 s8Y; sint16 s16Y; sint32 s32Y; sint64 s64Y; float32 f32Y; float64 f64Y; };
	union{ sint8 s8Level; sint16 s16Level; sint32 s32Level; sint64 s64Level; float32 f32Level; float64 f64Level; };
	union{ sint8 s8Width; sint16 s16Width; sint32 s32Width; sint64 s64Width; float32 f32Width; float64 f64Width; };
	union{ sint8 s8Height; sint16 s16Height; sint32 s32Height; sint64 s64Height; float32 f32Height; float64 f64Height; };
	union{ sint8 s8Area; sint16 s16Area; sint32 s32Area; sint64 s64Area; float32 f32Area; float64 f64Area; };

	union{ sint8 s8GradLeft; sint16 s16GradLeft; sint32 s32GradLeft; sint64 s64GradLeft; float32 f32GradLeft; float64 f64GradLeft; };
	union{ sint8 s8GradRight; sint16 s16GradRight; sint32 s32GradRight; sint64 s64GradRight; float32 f32GradRight; float64 f64GradRight; };
	union{ sint8 s8GradTop; sint16 s16GradTop; sint32 s32GradTop; sint64 s64GradTop; float32 f32GradTop; float64 f64GradTop; };
	union{ sint8 s8GradBottom; sint16 s16GradBottom; sint32 s32GradBottom; sint64 s64GradBottom; float32 f32GradBottom; float64 f64GradBottom; };
	union{ sint8 s8GradLeftTop; sint16 s16GradLeftTop; sint32 s32GradLeftTop; sint64 s64GradLeftTop; float32 f32GradLeftTop; float64 f64GradLeftTop; };
	union{ sint8 s8GradRightTop; sint16 s16GradRightTop; sint32 s32GradRightTop; sint64 s64GradRightTop; float32 f32GradRightTop; float64 f64GradRightTop; };
	union{ sint8 s8GradRightBottom; sint16 s16GradRightBottom; sint32 s32GradRightBottom; sint64 s64GradRightBottom; float32 f32GradRightBottom; float64 f64GradRightBottom; };
	union{ sint8 s8GradLeftBottom; sint16 s16GradLeftBottom; sint32 s32GradLeftBottom; sint64 s64GradLeftBottom; float32 f32GradLeftBottom; float64 f64GradLeftBottom; };
	union{ sint8 s8GradLeft2; sint16 s16GradLeft2; sint32 s32GradLeft2; sint64 s64GradLeft2; float32 f32GradLeft2; float64 f64GradLeft2; };
	union{ sint8 s8GradRight2; sint16 s16GradRight2; sint32 s32GradRight2; sint64 s64GradRight2; float32 f32GradRight2; float64 f64GradRight2; };
	union{ sint8 s8GradTop2; sint16 s16GradTop2; sint32 s32GradTop2; sint64 s64GradTop2; float32 f32GradTop2; float64 f64GradTop2; };
	union{ sint8 s8GradBottom2; sint16 s16GradBottom2; sint32 s32GradBottom2; sint64 s64GradBottom2; float32 f32GradBottom2; float64 f64GradBottom2; };
	union{ sint8 s8GradLeftTop2; sint16 s16GradLeftTop2; sint32 s32GradLeftTop2; sint64 s64GradLeftTop2; float32 f32GradLeftTop2; float64 f64GradLeftTop2; };
	union{ sint8 s8GradRightTop2; sint16 s16GradRightTop2; sint32 s32GradRightTop2; sint64 s64GradRightTop2; float32 f32GradRightTop2; float64 f64GradRightTop2; };
	union{ sint8 s8GradRightBottom2; sint16 s16GradRightBottom2; sint32 s32GradRightBottom2; sint64 s64GradRightBottom2; float32 f32GradRightBottom2; float64 f64GradRightBottom2; };
	union{ sint8 s8GradLeftBottom2; sint16 s16GradLeftBottom2; sint32 s32GradLeftBottom2; sint64 s64GradLeftBottom2; float32 f32GradLeftBottom2; float64 f64GradLeftBottom2; };
	
	union{ sint8 s8RadiusLeft; sint16 s16RadiusLeft; sint32 s32RadiusLeft; sint64 s64RadiusLeft; float32 f32RadiusLeft; float64 f64RadiusLeft; };
	union{ sint8 s8RadiusRight; sint16 s16RadiusRight; sint32 s32RadiusRight; sint64 s64RadiusRight; float32 f32RadiusRight; float64 f64RadiusRight; };

	EG_HILL eHillType;
	
	union{ sint8 s8Id; sint16 s16Id; sint32 s32Id; sint64 s64Id; };
	union{ uint32 u32TimeStamp; uint64 u64TimeStamp; };
	union{ sint32 s32Age; sint64 s64Age; };
	sint32 s32Moving;
} TGPeak;
#pragma pack()

// ! API
#ifdef __cplusplus
#include "GOal/GMutex.h"

class DG_API CGAtomicOperation
{
public:
	CGAtomicOperation( CGMutex* );
	virtual ~CGAtomicOperation();

protected:
	CGMutex *m_pcMutex;
};

class DG_API CGObject : public CGMutex
{
	friend class CGAtomicOperation;

public:
	CGObject( CGObject* );
	// ! destructor의 시작에, CGAtomicOperation cAo( this ); 기입!
	// ! destructor에서, 절대, virtual 멤버를 접근하지 말 것!
	virtual ~CGObject();

	// 1. Close()를 implement한 경우, 반드시, 자체 destructor에서 CG자체::Close()를 호출 할 것!
	// 2. 다른 context가, 본 class 계통의 virtual을 참조 하고, Close()로 관계를 정리하는 경우
	// - 1번을 따르지 말 것!
	// - 반드시, destructor 진입 전에, Close()를 불러서, 관계를 미리 정리 할 것!
	// 3. 특별한 상황이 아니면, 마지막에 Close()를 명시적으로 부르는 것이 원칙!
	virtual EG_RESULT Close() = 0;

protected:
	CGObject *m_pcParent;

public:
	CGObject* GetParent();
};

// GGraphics
#include "GGraphics/GRasterGraphics/GRasterGraphics.h"
#include "GGraphics/GVectorGraphics/GOgl/GOgl.h"

// GMath
#include "GMath/GMath.h"

// GOal
#include "GOal/GStream.h"
#include "GOal/GThread.h"
#include "GOal/GTime.h"
#include "GOal/GTimer.h"

// GSound
#include "GSound/GSound.h"

// GUtil
#include "GUtil/GBit.h"
#include "GUtil/GCodec.h"
#include "GUtil/GDump.h"
#include "GUtil/GJava.h"
#include "GUtil/GLinkedList.h"
#include "GUtil/GLog.h"
#include "GUtil/GMemBlock.h"
#include "GUtil/GObjManager.h"
#include "GUtil/GResourceFormat.h"
#include "GUtil/GStringUtf8.h"

// GSystem
#include "GProcess.h"

//
class DG_API CGSystem
{
public:
	static CGMutex sm_cPrintfMutex;
	static TCHAR sm_ptcPrintfBuf[DG_KERNEL_PAGE_SIZE];

	static CGProcess sm_cProcess;

public:
	static void SDbgPrint( const char*, uint32, const char*, void*, EG_TYPE, sint32, sint32 = 0 );

	static void SDelay( uint32 );

	static uint16 SH2N16( uint16 );
	static uint32 SH2N32( uint32 );
	static uint64 SH2N64( uint64 );

	static EG_EIGHTCC SS2Ecc( TCHAR* );
	static void SEcc2S( TCHAR*, EG_EIGHTCC );

	static sint32 SMakeStorageId( CGStringUtf8* );
};
#endif
