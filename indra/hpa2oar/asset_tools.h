#ifndef LL_HPA_2_OAR_H
#define LL_HPA_2_OAR_H

#include "llcommon.h"
#include "llassettype.h"

class LLAssetTools
{
public:
	static LLAssetType::EType typeFromExt(std::string src_filename);
	static std::string HPAtoOARName(std::string src_filename);
	static BOOL copyFile(std::string src_filename, std::string dest_filename);
};

#endif
