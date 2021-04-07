#ifndef __UH_H__
#define __UH_H__

#ifndef __ASSEMBLY__

/* For uH Command */
#define	APP_INIT	0
#define	APP_SAMPLE	1
#define APP_RKP		2

#define UH_PREFIX  UL(0xc300c000)
#define UH_APPID(APP_ID)  ((UL(APP_ID) & UL(0xFF)) | UH_PREFIX)

enum __UH_APP_ID {
	UH_APP_INIT 	= UH_APPID(APP_INIT),
	UH_APP_SAMPLE 	= UH_APPID(APP_SAMPLE),
	UH_APP_RKP 	= UH_APPID(APP_RKP),
};

struct test_case_struct {
	int (* fn)(void); //test case func
	char * describe;
};

#define UH_LOG_START	(0x87100000)
#define UH_LOG_SIZE		(0x40000)

unsigned long uh_call(u64 app_id, u64 command, u64 arg0, u64 arg1, u64 arg2, u64 arg3);

#endif //__ASSEMBLY__
#endif //__UH_H__
