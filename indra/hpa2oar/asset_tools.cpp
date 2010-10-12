#include "asset_tools.h"

#include "lldir.h"
#include "llapr.h"

//this uses the extensions as used by SLPE and may not be correct for the HPA exporter.
LLAssetType::EType LLAssetTools::typefromExt(std::string src_filename)
{
	std::string exten = gDirUtilp->getExtension(src_filename);
	if (exten.empty())
		return LLAssetType::AT_NONE;

	if(exten == "jp2" || exten == "j2k" || exten == "j2c" || exten == "texture")
	{
		return LLAssetType::AT_TEXTURE;
	}
	else if(exten == "notecard")
	{
		return LLAssetType::AT_NOTECARD;
	}
	else if(exten == "lsl" || exten == "lsltext")
	{
		return LLAssetType::AT_LSL_TEXT;
	}
	/* BELOW ARE UNTESTED */
	else if(exten == "wav")
	{
		return LLAssetType::AT_SOUND;
	}
	else if(exten == "ogg")
	{
		return LLAssetType::AT_SOUND;
	}
	else if (exten == "animatn")
	{
		return LLAssetType::AT_ANIMATION;
	}
	else if(exten == "gesture")
	{
		return LLAssetType::AT_GESTURE;
	}
	else
	{
		llwarns << "Unhandled extension" << llendl;
		return LLAssetType::AT_NONE;
	}
	return asset_type;
}

std::string LLAssetTools::HPAtoOARName(std::string src_filename)
{
	LLAssetType file_type = typeFromExt(src_filename);
	std::string base_filename = gDirUtilp->getBaseFileName(src_filename, true);

	if(file_type == LLAssetType::AT_NONE) return std::string("");

	switch(file_type)
	{
	case LLAssetType::AT_TEXTURE:
		return base_filename + "_texture.jp2";
	case LLAssetType::AT_NOTECARD:
		return base_filename + "_notecard.txt";
	case LLAssetType::AT_LSL_TEXT:
		return base_filename + "_script.lsl";
	default:
		llwarns << "For " << src_filename << ": This asset type isn't supported yet." << llendl;
		return std::string("");
		break;
	}
}

BOOL LLAssetTools::copyFile(std::string src_filename, std::string dst_filename)
{
	S32 src_size;
	LLAPRFile src_fp;
	src_fp.open(src_filename, LL_APR_RB, LLAPRFile::global, &src_size);
	if(!src_fp.getFileHandle()) return FALSE;
	LLAPRFile dst_fp;
	dst_fp.open(dst_filename, LL_APR_WB, LLAPRFile::global);
	if(!dst_fp.getFileHandle()) return FALSE;
	char* buffer = new char[src_size + 1];
	src_fp.read(buffer, src_size);
	dst_fp.write(buffer, src_size);
	src_fp.close();
	dst_fp.close();
	delete[] buffer;
	return TRUE;
}
