//FEATURE_SDCARD_MEDIAEXN_SYSTEMCALL_ENCRYPTION

#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/sched.h>
#include <linux/unistd.h>
#include "LGSDEncManager.h"

int propertyMediaCheck;				// Media Ecryption 체크 여부
char savedfileExtList[MAX_MEDIA_EXT_LENGTH];

/*
*     System property의 Media Encryption 여부 저장.
*/
asmlinkage long sys_set_media_property(int value)
{
//	printk("%s :: SystemCall value: %d , propertymediacheck : %d\n", __func__,value,propertyMediaCheck);
	propertyMediaCheck = value;
	return 1;
}

int getMediaProperty(void){
	return propertyMediaCheck;
}

/*
* Media File 확장자 저장 System call 함수.
*/
asmlinkage long sys_set_media_ext(char *mediaExtList)
{
	memset(savedfileExtList, 0, sizeof(savedfileExtList));

	if(mediaExtList != NULL){
		strncpy(savedfileExtList, mediaExtList, strlen(mediaExtList));
	}

//	printk("%s :: savedfileExtList: %s\n", __func__,savedfileExtList);

	return 1;
}
//#endif //FEATURE_SDCARD_MEDIAEXN_SYSTEMCALL_ENCRYPTION

char *ecryptfs_Extfilename(const unsigned char *filename){
	char *pos = NULL;    
	int len , i;        

	if(filename == NULL) {    	
		return pos;    
	}

	// 확장자 추출 : ex> a.txt -> .txt    
	pos = strrchr(filename,'.');   
	if(pos == NULL){    	
		return pos;    
	}    	

	// 소문자 -> 대문자	
	len = strlen(pos);	
	for(i = 0 ; i < len ; i++){		
		if(*(pos+i) >= 'a' && *(pos+i) <= 'z'){			
			*(pos+i) = *(pos+i) -32;		
		}	
	}	    
	return pos+1;
}

int ecryptfs_mediaFileSearch(const unsigned char *filename){
	char *extP = NULL;

	// Filename에서 확장자 추출.
	extP = ecryptfs_Extfilename(filename);
	if(extP == NULL || strlen(extP) < 2){
		printk(KERN_DEBUG "%s :: Extfilename is NULL\n", __func__);
		return 0;
	}

	printk("%s :: savedfileExtList: %s\n", __func__,savedfileExtList);

	// MediaType에 존재 여부 확인	// 존재하면 status = 1으로 변환 : 미디어 파일이라는 의미	
	if(sizeof(savedfileExtList) != 0)
	{
		if(strstr(savedfileExtList,extP) == NULL){ 		  
			printk(KERN_DEBUG "%s :: NOT ECRYPTFS_MEDIA_EXCEPTION\n", __func__);
			return 0;
		}
	}
	else
	{
		printk(KERN_DEBUG "%s :: getMediaExtList() = NULL\n", __func__);
		return 0;
	}

	return 1;
}

