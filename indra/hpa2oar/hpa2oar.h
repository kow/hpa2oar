 /* Copyright (C) 2010, Robin Cornelius <robin.cornelius@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef LL_HPA_2_OAR_H
#define LL_HPA_2_OAR_H

#include <llcommon.h>
#include "llthread.h"
#include "llxmltree.h"
#include "llassettype.h"
#include <libarchive/archive.h>
#include <libarchive/archive_entry.h>

class hpa_converter : public LLThread
{
	public:
	hpa_converter();
	~hpa_converter();
	virtual void run();
	void write_to_folder(std::string oar_folder);

	std::string path;
	std::string outputPath;

	std::string sep;

protected:
	void load_hpa(std::string hpa_path);
	LLSD parse_hpa_group(LLXmlTreeNode* group);
	LLSD parse_hpa_linkset(LLXmlTreeNode* group);
	LLSD parse_hpa_object(LLXmlTreeNode* group);
	std::string llsd_to_textureentry(LLSD te_faces);
	std::string pack_extra_params(LLSD extra_params);

	void create_directory_structure();
	void copy_all_assets();
	void copy_assets_from(std::string asset_path, std::string mask);
	void save_oar_objects();

	LLSD mOARFileContents;

	U32 assets_moved;
	U32 objects_processed;
};

class LLAssetTools
{
public:
	static LLAssetType::EType typefromExt(std::string src_filename);
	static std::string HPAtoOARName(std::string src_filename);
};

//enums from llpanelobject

enum {
	MI_BOX,
	MI_CYLINDER,
	MI_PRISM,
	MI_SPHERE,
	MI_TORUS,
	MI_TUBE,
	MI_RING,
	MI_SCULPT,
	MI_NONE,
	MI_VOLUME_COUNT
};

enum {
	MI_HOLE_SAME,
	MI_HOLE_CIRCLE,
	MI_HOLE_SQUARE,
	MI_HOLE_TRIANGLE,
	MI_HOLE_COUNT
};

void printinfo(std::string message);
void pack_directory_to_tgz(std::string basedir, std::string outpath);
void pack_directory(struct archive* tgz, std::string path, std::string basedir);

#endif
