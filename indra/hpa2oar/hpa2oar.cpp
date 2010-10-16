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

//Just used Robin's J2K decoder thing to get this done faster -Hazim

#include "linden_common.h"
#include "llcommon.h"
#include "llthread.h"
#include "lldir.h"
#include "llpointer.h"
#include "llxmlparser.h"
#include "llmath.h"
#include "llprimitive.h"
#include "time.h"
#include "llerrorcontrol.h"
#include "llxmlnode.h"

#include "llsdserialize.h"
#include "llsdserialize_xml.h"
#include "llsdutil_math.h"
#include "v3dmath.h"

#include "lldir.h"
#include "llapr.h"

#include "llinventorytype.h"

#include "llrand.h"

#include <iostream>
#include <fstream>

#include "hpa2oar.h"

using namespace LLError;

int main(int argv,char * argc[])
{
	ll_init_apr();

	LLError::initForApplication(".");
	gDirUtilp->initAppDirs("hpa2oar",""); // path search is broken, if the pp_read_only_data_dir is not set to something searches fail

	// ensure converter dies before ll_cleanup_apr() is called
	{
		hpa_converter converter;

		converter.sep = gDirUtilp->getDirDelimiter();
		if(argv>2)
		{
			converter.path=std::string(argc[1]);
			converter.outputPath=std::string(argc[2]);
		}
		else if(argv == 2)
		{
			llerrs << "No output folder provided"<<llendl;
		}
		else
		{
			llerrs << "No HPA or output folder name provided"<<llendl;
		}

		if(!gDirUtilp->fileExists(converter.path))
			llerrs << "\"" << converter.path << "\" doesn't exist"<<llendl;

		//if(gDirUtilp->fileExists(converter.outputPath))
		//	llerrs << "Output path exists!" << llendl;

		converter.run();

		char dummy;
		printf("Press enter to continue\n");
		scanf("%c",&dummy);
	}

	ll_cleanup_apr();
}

hpa_converter::hpa_converter()
: LLThread("Meh why do we need a LLThread")
{
	// so we init the sMutex and image fails do not explode in a big stinking pile

	if(!mLocalAPRFilePoolp)
	{
		mLocalAPRFilePoolp = new LLVolatileAPRPool() ;
	}
}

hpa_converter::~hpa_converter()
{

}

void hpa_converter::run()
{
	//let's set up our directories and get everything in order before trying to parse the hpa
	printinfo("Creating OAR directory structure");
	create_directory_structure();

	printinfo("Copying assets");
	//skip this for now
	copy_all_assets();
	load_hpa(path);
	printinfo(llformat("Loaded %u linksets.",mOARFileContents.size()));

	//DEBUG CODE
	llofstream export_file(path + ".llsd",std::ios_base::app | std::ios_base::out);
	LLSDSerialize::toPrettyXML(mOARFileContents, export_file);
	export_file.close();

	printinfo("Saving linksets in OAR format (go grab a coffee)");
	save_oar_objects();
	printinfo(
"\n	     _                  \n"
"	  __| | ___  _ __   ___ \n"
"	 / _` |/ _ \\| '_ \\ / _ \\\n"
"	| (_| | (_) | | | |  __/\n"
"	 \\__,_|\\___/|_| |_|\\___|\n"

			  );
}

void hpa_converter::create_directory_structure()
{
	LLFile::mkdir(outputPath, 0755);

	//We don't actually need to do OAR 0.2 right now, just create an OAR 0.1 dir structure
	LLFile::mkdir(outputPath + sep + "assets", 0755);
	LLFile::mkdir(outputPath + sep + "objects", 0755);
	LLFile::mkdir(outputPath + sep + "terrains", 0755);
}

void hpa_converter::copy_all_assets()
{
	std::string hpa_basedir = gDirUtilp->getDirName(path) + sep;
	copy_assets_from(hpa_basedir + "textures", "*.j2c");
	copy_assets_from(hpa_basedir + "sculptmaps", "*.j2c");
	copy_assets_from(hpa_basedir + "inventory", "*");
}



void hpa_converter::copy_assets_from(std::string source_path, std::string mask)
{
	BOOL found = TRUE;
	std::string curr_name;

	source_path += sep;

	printinfo("copying from " + source_path);

	std::string oar_asset_path = outputPath + sep + "assets" + sep;

	while(found)// for every directory
	{
		if((found = gDirUtilp->getNextFileInDir(source_path,mask, curr_name, FALSE)))
		{
			std::string full_path=source_path + curr_name;
			if(LLFile::isfile(full_path))
			{
				std::string new_fname = LLAssetTools::HPAtoOARName(curr_name);

				//check if we know about this asset type yet

				if(!new_fname.empty())
				{
					std::string new_path = oar_asset_path + new_fname;
					//copy the file to output dir
					std::ifstream f1 (full_path.c_str(), std::fstream::binary);
					std::ofstream f2 (new_path.c_str(), std::fstream::trunc|std::fstream::binary);

					if(f1 && f2)
					{
						f2 << f1.rdbuf();

						f2.close();
						f1.close();
					}
					else
					{
						//make sure both are closed if they're open
						if(f1) f1.close();
						if(f2) f2.close();

						llwarns << "Failed to copy " << full_path << " to " << new_path << llendl;
					}
				}
				else
				{
					printinfo("Requested to copy unsupported asset");
				}

				std::cout << "=" << std::flush;
			}
			else
			{
				printinfo("This... doesn't exist?");
			}
		}
	}

	std::cout << std::endl;
}

//rewritten convenience function used in the code from imprudence's importer
void printinfo(std::string message)
{
	llinfos << message << llendl;
}

void hpa_converter::load_hpa(std::string hpa_path)
{
	LLXmlTree xml_tree;

	if (!xml_tree.parseFile(hpa_path))
	{
		llwarns << "Problem reading HPA file: " << hpa_path << llendl;
		return;
	}

	LLXmlTreeNode* root = xml_tree.getRoot();

	for (LLXmlTreeNode* child = root->getFirstChild(); child; child = root->getNextChild())
	{
		if (child->hasName("group"))
		{
			mOARFileContents = parse_hpa_group(child);

			//if (temp)
			//{
				//debug code?
				//std::stringstream temp2;
				//LLSDSerialize::toPrettyXML(temp, temp2);
				//printinfo(temp2.str());
				//linksets = temp;
			//}
		}
	}
}

void hpa_converter::save_oar_objects()
{
	//LLSD to HPA converter from exporttracker.cpp
	//now an LLSD to OAR converter

	//I don't really think this particularly matters, but it should probably be the same for every object.
	U32 region_handle = ll_rand(U32_MAX - 1);

	for(LLSD::array_iterator iter = mOARFileContents.beginArray();
		iter != mOARFileContents.endArray();
		++iter)
	{	// for each object


		LLXMLNode *linkset_xml = new LLXMLNode("SceneObjectGroup", FALSE);

		LLXMLNode *child_container = new LLXMLNode("OtherParts", FALSE);

		std::string pretty_position = "";
		std::string linkset_name = "";
		std::string linkset_id = "";

		LLVector3d root_position(0.0, 0.0, 0.0);
		LLQuaternion root_rotation(0.0, 0.0, 0.0, 0.0);

		LLSD plsd=(*iter)["Object"];

		bool is_root_prim = true;
		U32 link_num = 1;

		//for every prim in the linkset
		for(LLSD::array_iterator link_iter = plsd.beginArray();
			link_iter != plsd.endArray();
			++link_iter)
		{

			LLSD prim = (*link_iter);

			std::string selected_item	= "box";
			F32 scale_x=1.f, scale_y=1.f;

			LLVolumeParams volume_params;
			volume_params.fromLLSD(prim["volume"]);
			// Volume type
			U8 path = volume_params.getPathParams().getCurveType();
			U8 profile_and_hole = volume_params.getProfileParams().getCurveType();
			U8 profile	= profile_and_hole & LL_PCODE_PROFILE_MASK;
			U8 hole		= profile_and_hole & LL_PCODE_HOLE_MASK;

			//UUID to use for the folder and object
			LLUUID object_uuid;
			object_uuid.generate();

			//set the linkset identifier if this is the root prim
			if(is_root_prim)
				linkset_id = object_uuid.asString();


			// Scale goes first so we can differentiate between a sphere and a torus,
			// which have the same profile and path types.
			// Scale
			scale_x = volume_params.getRatioX();
			scale_y = volume_params.getRatioY();
			BOOL linear_path = (path == LL_PCODE_PATH_LINE) || (path == LL_PCODE_PATH_FLEXIBLE);
			if ( linear_path && profile == LL_PCODE_PROFILE_CIRCLE )
			{
				selected_item = "cylinder";
			}
			else if ( linear_path && profile == LL_PCODE_PROFILE_SQUARE )
			{
				selected_item = "box";
			}
			else if ( linear_path && profile == LL_PCODE_PROFILE_ISOTRI )
			{
				selected_item = "prism";
			}
			else if ( linear_path && profile == LL_PCODE_PROFILE_EQUALTRI )
			{
				selected_item = "prism";
			}
			else if ( linear_path && profile == LL_PCODE_PROFILE_RIGHTTRI )
			{
				selected_item = "prism";
			}
			else if (path == LL_PCODE_PATH_FLEXIBLE) // shouldn't happen
			{
				selected_item = "cylinder"; // reasonable default
			}
			else if ( path == LL_PCODE_PATH_CIRCLE && profile == LL_PCODE_PROFILE_CIRCLE && scale_y > 0.75f)
			{
				selected_item = "sphere";
			}
			else if ( path == LL_PCODE_PATH_CIRCLE && profile == LL_PCODE_PROFILE_CIRCLE && scale_y <= 0.75f)
			{
				selected_item = "torus";
			}
			else if ( path == LL_PCODE_PATH_CIRCLE && profile == LL_PCODE_PROFILE_CIRCLE_HALF)
			{
				selected_item = "sphere";
			}
			else if ( path == LL_PCODE_PATH_CIRCLE2 && profile == LL_PCODE_PROFILE_CIRCLE )
			{
				// Spirals aren't supported.  Make it into a sphere.  JC
				selected_item = "sphere";
			}
			else if ( path == LL_PCODE_PATH_CIRCLE && profile == LL_PCODE_PROFILE_EQUALTRI )
			{
				selected_item = "ring";
			}
			else if ( path == LL_PCODE_PATH_CIRCLE && profile == LL_PCODE_PROFILE_SQUARE && scale_y <= 0.75f)
			{
				selected_item = "tube";
			}
			else
			{
				llinfos << "Unknown path " << (S32) path << " profile " << (S32) profile << " in getState" << llendl;
				selected_item = "box";
			}
			// Create an LLSD object that represents this prim. It will be injected in to the overall LLSD
			// tree structure
			LLXMLNode *prim_xml;

			int pcode = 9; //prim["pcode"].asInteger();
			// Sculpt

			/* OAR FIXME! we need to translate trees + grass to a PCODE!
			if (prim.has("sculpt"))
				prim_xml = new LLXMLNode("sculpt", FALSE);
			else if (pcode == LL_PCODE_LEGACY_GRASS)
			{
				prim_xml = new LLXMLNode("grass", FALSE);
				LLXMLNodePtr shadow_xml = prim_xml->createChild("type", FALSE);
				shadow_xml->createChild("val", TRUE)->setValue(prim["state"]);
			}
			else if (pcode == LL_PCODE_LEGACY_TREE)
			{
				prim_xml = new LLXMLNode("tree", FALSE);
				LLXMLNodePtr shadow_xml = prim_xml->createChild("type", FALSE);
				shadow_xml->createChild("val", TRUE)->setValue(prim["state"]);
			} */

			prim_xml = new LLXMLNode("SceneObjectPart", FALSE);

			//hooray for standards!
			prim_xml->createChild("xmlns:xsi", TRUE)->setValue("http://www.w3.org/2001/XMLSchema-instance");
			prim_xml->createChild("xmlns:xsd", TRUE)->setValue("http://www.w3.org/2001/XMLSchema");

			//<Shape>
			LLXMLNodePtr shape_xml = prim_xml->createChild("Shape", FALSE);
			shape_xml->createChild("PCode", FALSE)->setIntValue(pcode);

			//<ProfileCurve>1</ProfileCurve>
			shape_xml->createChild("ProfileCurve", FALSE)->setIntValue(profile);

			//<PathCurve>16</PathCurve>
			shape_xml->createChild("PathCurve", FALSE)->setIntValue(path);

			//<PathScaleX>115</PathScaleX>
			F32 taper_x = (1.f - volume_params.getRatioX()) * 100.f + 100.f;
			shape_xml->createChild("PathScaleX", FALSE)->setFloatValue(taper_x);
			//<PathScaleY>75</PathScaleY>
			F32 taper_y = (1.f - volume_params.getRatioY()) * 100.f + 100.f;
			shape_xml->createChild("PathScaleY", FALSE)->setFloatValue(taper_y);
		
			const char	*selected_hole	= "1";
			switch (hole)
			{
			case LL_PCODE_HOLE_CIRCLE:
				selected_hole = "Circle";
				break;
			case LL_PCODE_HOLE_SQUARE:
				selected_hole = "Square";
				break;
			case LL_PCODE_HOLE_TRIANGLE:
				selected_hole = "Triangle";
				break;
			case LL_PCODE_HOLE_SAME:
			default:
				selected_hole = "Same";
				break;
			}

			//<ProfileHollow>2499</ProfileHollow>
			shape_xml->createChild("ProfileHollow", FALSE)->setIntValue(volume_params.getHollow() * 50000);

			//<HollowShape>Triangle</HollowShape>
			shape_xml->createChild("HollowShape", FALSE)->setStringValue(selected_hole);

			//<PathShearX>206</PathShearX>
			F32 shear_x = volume_params.getShearX();
			shear_x = shear_x / 0.01;
			if (shear_x < 0)
				shear_x = shear_x + 256;
			shape_xml->createChild("PathShearX", FALSE)->setIntValue(shear_x);

			//<PathShearY>30</PathShearY>
			F32 shear_y = volume_params.getShearY();
			shear_y = shear_y / 0.01;
			if (shear_y < 0)
				shear_y = shear_y + 256;
			shape_xml->createChild("PathShearY", FALSE)->setIntValue(shear_y);

			//<ProfileBegin>11250</ProfileBegin>
			F32 cut_begin = volume_params.getBeginS();
			if (cut_begin != 1)
				cut_begin = cut_begin * 50000;
			else
				cut_begin = 0;
			shape_xml->createChild("ProfileBegin", FALSE)->setIntValue(cut_begin);
			//<ProfileEnd>0</ProfileEnd>
			F32 cut_end = volume_params.getEndS();
			if (cut_end != 1)
				cut_end = (1 - cut_end) * 50000;
			else
				cut_end = 0;
			shape_xml->createChild("ProfileEnd", FALSE)->setIntValue(cut_end);


			//<PathBegin>0</PathBegin>
			F32 adv_cut_begin = volume_params.getBeginT();
			if (adv_cut_begin != 1)
				adv_cut_begin = adv_cut_begin * 50000;
			else
				adv_cut_begin = 0;
			shape_xml->createChild("PathBegin", FALSE)->setIntValue(adv_cut_begin);
			//<PathEnd>0</PathEnd>
			F32 adv_cut_end = volume_params.getEndT();
			if (adv_cut_end != 1)
				adv_cut_end = (1 - adv_cut_end) * 50000;
			else
				adv_cut_end = 0;
			shape_xml->createChild("PathEnd", FALSE)->setIntValue(adv_cut_end);



			//////////////
			//Properties//
			//////////////

			if(prim.has("name"))
				prim_xml->createChild("Name", FALSE)->setStringValue(prim["name"].asString());
			else
				prim_xml->createChild("Name", FALSE)->setStringValue(object_uuid.asString());

			if(prim.has("description"))
				prim_xml->createChild("Description", FALSE)->setStringValue(prim["description"].asString());
			else
				prim_xml->createChild("Description", FALSE)->setStringValue(std::string(""));

			prim_xml->createChild("UUID",FALSE)->createChild("Guid", FALSE)->setValue(object_uuid.asString());

			//if this is the root prim, set the linkset name
			if(is_root_prim)
			{
				if(prim.has("name"))
					linkset_name = prim["name"].asString();
				else
					linkset_name = object_uuid.asString();
			}

			if(prim.has("material"))
				prim_xml->createChild("Material", FALSE)->setStringValue(prim["material"].asString());
			else
				prim_xml->createChild("Material", FALSE)->setStringValue(std::string("3"));

			prim_xml->createChild("AllowedDrop", FALSE)->setStringValue(std::string("false"));
			prim_xml->createChild("CreatorID",FALSE)->createChild("Guid", FALSE)->setValue(LLUUID::null.asString());
			//make a normal-looking localid
			prim_xml->createChild("LocalId",FALSE)->setValue(llformat("%u", ll_rand(U32_MAX - 10000) + 9999));

			prim_xml->createChild("LinkNum",FALSE)->setValue(llformat("%u", link_num));

			prim_xml->createChild("RegionHandle",FALSE)->setValue(llformat("%u", region_handle));


			//////////////
			//Transforms//
			//////////////

			LLXMLNodePtr group_position_xml = prim_xml->createChild("GroupPosition", FALSE);

			if(is_root_prim)
			{
				root_position = ll_vector3d_from_sd(prim["position"]);
				//used in determining the filename
				pretty_position = llformat("%0.f_%0.f_%0.f", root_position.mdV[VX], root_position.mdV[VY], root_position.mdV[VZ]);
			}

			//This is probably wrong! This may be absolute rotations where it's wanting relative from the root's rot!
			LLXMLNodePtr rotation_xml = prim_xml->createChild("RotationOffset", FALSE);
			LLQuaternion rotation = ll_quaternion_from_sd(prim["rotation"]);

			if(is_root_prim)
				root_rotation = rotation;
			else
				rotation = rotation * ~root_rotation;

			//group_position is always the position of the root prim
			group_position_xml->createChild("X", FALSE)->setValue(llformat("%.5f", root_position.mdV[VX]));
			group_position_xml->createChild("Y", FALSE)->setValue(llformat("%.5f", root_position.mdV[VY]));
			group_position_xml->createChild("Z", FALSE)->setValue(llformat("%.5f", root_position.mdV[VZ]));

			LLXMLNodePtr offset_position_xml = prim_xml->createChild("OffsetPosition", FALSE);
			LLVector3d position = (ll_vector3d_from_sd(prim["position"]) - root_position) * ~root_rotation;
			offset_position_xml->createChild("X", FALSE)->setValue(llformat("%.5f", position.mdV[VX]));
			offset_position_xml->createChild("Y", FALSE)->setValue(llformat("%.5f", position.mdV[VY]));
			offset_position_xml->createChild("Z", FALSE)->setValue(llformat("%.5f", position.mdV[VZ]));

			LLXMLNodePtr scale_xml = prim_xml->createChild("Scale", FALSE);
			LLVector3d scale = ll_vector3d_from_sd(prim["scale"]);
			scale_xml->createChild("X", FALSE)->setValue(llformat("%.5f", scale.mdV[VX]));
			scale_xml->createChild("Y", FALSE)->setValue(llformat("%.5f", scale.mdV[VY]));
			scale_xml->createChild("Z", FALSE)->setValue(llformat("%.5f", scale.mdV[VZ]));

			//apparently shape wants a copy of this too, whatever.
			LLXMLNodePtr shape_scale_xml = shape_xml->createChild("Scale", FALSE);
			shape_scale_xml->createChild("X", FALSE)->setValue(llformat("%.5f", scale.mdV[VX]));
			shape_scale_xml->createChild("Y", FALSE)->setValue(llformat("%.5f", scale.mdV[VY]));
			shape_scale_xml->createChild("Z", FALSE)->setValue(llformat("%.5f", scale.mdV[VZ]));



			rotation_xml->createChild("X", FALSE)->setValue(llformat("%.5f", rotation.mQ[VX]));
			rotation_xml->createChild("Y", FALSE)->setValue(llformat("%.5f", rotation.mQ[VY]));
			rotation_xml->createChild("Z", FALSE)->setValue(llformat("%.5f", rotation.mQ[VZ]));
			rotation_xml->createChild("W", FALSE)->setValue(llformat("%.5f", rotation.mQ[VW]));

			// Twist
			F32 twist_begin = volume_params.getTwistBegin() * 100;
			F32 twist		= volume_params.getTwist() * 100;
			//I fucked up the HPA exporter, this is a temporary fix!
			if (path == LL_PCODE_PATH_LINE || path == LL_PCODE_PATH_FLEXIBLE)
			{
			}
			else
			{
				twist		*= 2;
				twist_begin	*= 2;
			}
			shape_xml->createChild("PathTwistBegin", FALSE)->setIntValue((U32)twist_begin);
			shape_xml->createChild("PathTwist", FALSE)->setIntValue((U32)twist);


			// Revolutions
			F32 revolutions = (volume_params.getRevolutions() - 1) / 0.015f;
			shape_xml->createChild("PathRevolutions", FALSE)->setValue(llformat("%u", (U32)revolutions));

			// Flags

			//I really hate libomv
			LLXMLNodePtr object_flags_xml = prim_xml->createChild("ObjectFlags", FALSE);

			U32 object_flags = 0;

			if(prim["phantom"].asBoolean())
				object_flags |= 0x00000400;
			if(prim["physical"].asBoolean())
				object_flags |= 0x00000001;

			object_flags_xml->setValue(llformat("%u", object_flags));

			// Grab S path
			F32 begin_s	= volume_params.getBeginS();
			F32 end_s	= volume_params.getEndS();
			// Compute cut and advanced cut from S and T
			F32 begin_t = volume_params.getBeginT();
			F32 end_t	= volume_params.getEndT();

			// Cut interpretation varies based on base object type
			if ( selected_item == "sphere" || selected_item == "torus" ||
				 selected_item == "tube"   || selected_item == "ring" )
			{
				cut_begin		= begin_t;
				cut_end			= end_t;
				adv_cut_begin	= begin_s;
				adv_cut_end		= end_s;
			}
			else
			{
				cut_begin       = begin_s;
				cut_end         = end_s;
				adv_cut_begin   = begin_t;
				adv_cut_end     = end_t;
			}
			if (selected_item != "sphere")
			{
				// Shear
				//<top_shear x="0" y="0" />
				F32 shear_x = volume_params.getShearX();
				F32 shear_y = volume_params.getShearY();
				LLXMLNodePtr shear_xml = prim_xml->createChild("top_shear", FALSE);
				shear_xml->createChild("x", TRUE)->setValue(llformat("%.5f", shear_x));
				shear_xml->createChild("y", TRUE)->setValue(llformat("%.5f", shear_y));
			}
			else
			{
				// Dimple
				//<dimple begin="0.0" end="1.0" />
				LLXMLNodePtr shear_xml = prim_xml->createChild("dimple", FALSE);
				shear_xml->createChild("begin", TRUE)->setValue(llformat("%.5f", adv_cut_begin));
				shear_xml->createChild("end", TRUE)->setValue(llformat("%.5f", adv_cut_end));
			}

			if (selected_item == "box" || selected_item == "cylinder" || selected_item == "prism")
			{
				// Taper
				//<taper x="0" y="0" />
				F32 taper_x = 1.f - volume_params.getRatioX();
				F32 taper_y = 1.f - volume_params.getRatioY();
				LLXMLNodePtr taper_xml = prim_xml->createChild("taper", FALSE);
				taper_xml->createChild("x", TRUE)->setValue(llformat("%.5f", taper_x));
				taper_xml->createChild("y", TRUE)->setValue(llformat("%.5f", taper_y));
			}
			else if (selected_item == "torus" || selected_item == "tube" || selected_item == "ring")
			{
				// Taper
				//<taper x="0" y="0" />
				F32 taper_x	= volume_params.getTaperX();
				F32 taper_y = volume_params.getTaperY();
				LLXMLNodePtr taper_xml = prim_xml->createChild("taper", FALSE);
				taper_xml->createChild("x", TRUE)->setValue(llformat("%.5f", taper_x));
				taper_xml->createChild("y", TRUE)->setValue(llformat("%.5f", taper_y));
				//Hole Size
				//<hole_size x="0.2" y="0.35" />
				F32 hole_size_x = volume_params.getRatioX();
				F32 hole_size_y = volume_params.getRatioY();
				LLXMLNodePtr hole_size_xml = prim_xml->createChild("hole_size", FALSE);
				hole_size_xml->createChild("x", TRUE)->setValue(llformat("%.5f", hole_size_x));
				hole_size_xml->createChild("y", TRUE)->setValue(llformat("%.5f", hole_size_y));
				//Advanced cut
				//<profile_cut begin="0" end="1" />
				LLXMLNodePtr profile_cut_xml = prim_xml->createChild("profile_cut", FALSE);
				profile_cut_xml->createChild("begin", TRUE)->setValue(llformat("%.5f", adv_cut_begin));
				profile_cut_xml->createChild("end", TRUE)->setValue(llformat("%.5f", adv_cut_end));
				//Skew
				//<skew val="0.0" />
				F32 skew = volume_params.getSkew();
				LLXMLNodePtr skew_xml = prim_xml->createChild("skew", FALSE);
				skew_xml->createChild("val", TRUE)->setValue(llformat("%.5f", skew));
				//Radius offset
				//<radius_offset val="0.0" />
				F32 radius_offset = volume_params.getRadiusOffset();
				LLXMLNodePtr radius_offset_xml = prim_xml->createChild("radius_offset", FALSE);
				radius_offset_xml->createChild("val", TRUE)->setValue(llformat("%.5f", radius_offset));
			}
			////<path_cut begin="0" end="1" />
			//LLXMLNodePtr path_cut_xml = prim_xml->createChild("path_cut", FALSE);
			//path_cut_xml->createChild("begin", TRUE)->setValue(llformat("%.5f", cut_begin));
			//path_cut_xml->createChild("end", TRUE)->setValue(llformat("%.5f", cut_end));
			////<twist begin="0" end="0" />
			//LLXMLNodePtr twist_xml = prim_xml->createChild("twist", FALSE);
			//twist_xml->createChild("begin", TRUE)->setValue(llformat("%.5f", twist_begin));
			//twist_xml->createChild("end", TRUE)->setValue(llformat("%.5f", twist));

			// Extra params
			// Flexible
			//if(prim.has("flexible"))
			//{
			//	LLFlexibleObjectData attributes;
			//	attributes.fromLLSD(prim["flexible"]);
			//	//<flexible>
			//	LLXMLNodePtr flex_xml = prim_xml->createChild("flexible", FALSE);
			//	//<softness val="2.0">
			//	LLXMLNodePtr softness_xml = flex_xml->createChild("softness", FALSE);
			//	softness_xml->createChild("val", TRUE)->setValue(llformat("%.5f", (F32)attributes.getSimulateLOD()));
			//	//<gravity val="0.3">
			//	LLXMLNodePtr gravity_xml = flex_xml->createChild("gravity", FALSE);
			//	gravity_xml->createChild("val", TRUE)->setValue(llformat("%.5f", attributes.getGravity()));
			//	//<drag val="2.0">
			//	LLXMLNodePtr drag_xml = flex_xml->createChild("drag", FALSE);
			//	drag_xml->createChild("val", TRUE)->setValue(llformat("%.5f", attributes.getAirFriction()));
			//	//<wind val="0.0">
			//	LLXMLNodePtr wind_xml = flex_xml->createChild("wind", FALSE);
			//	wind_xml->createChild("val", TRUE)->setValue(llformat("%.5f", attributes.getWindSensitivity()));
			//	//<tension val="1.0">
			//	LLXMLNodePtr tension_xml = flex_xml->createChild("tension", FALSE);
			//	tension_xml->createChild("val", TRUE)->setValue(llformat("%.5f", attributes.getTension()));
			//	//<force x="0.0" y="0.0" z="0.0" />
			//	LLXMLNodePtr force_xml = flex_xml->createChild("force", FALSE);
			//	force_xml->createChild("x", TRUE)->setValue(llformat("%.5f", attributes.getUserForce().mV[VX]));
			//	force_xml->createChild("y", TRUE)->setValue(llformat("%.5f", attributes.getUserForce().mV[VY]));
			//	force_xml->createChild("z", TRUE)->setValue(llformat("%.5f", attributes.getUserForce().mV[VZ]));
			//}

			//// Light
			//if (prim.has("light"))
			//{
			//	LLLightParams light;
			//	light.fromLLSD(prim["light"]);
			//	//<light>
			//	LLXMLNodePtr light_xml = prim_xml->createChild("light", FALSE);
			//	//<color r="255" g="255" b="255" />
			//	LLXMLNodePtr color_xml = light_xml->createChild("color", FALSE);
			//	LLColor4 color = light.getColor();
			//	color_xml->createChild("r", TRUE)->setValue(llformat("%u", (U32)(color.mV[VRED] * 255)));
			//	color_xml->createChild("g", TRUE)->setValue(llformat("%u", (U32)(color.mV[VGREEN] * 255)));
			//	color_xml->createChild("b", TRUE)->setValue(llformat("%u", (U32)(color.mV[VBLUE] * 255)));
			//	//<intensity val="1.0" />
			//	LLXMLNodePtr intensity_xml = light_xml->createChild("intensity", FALSE);
			//	intensity_xml->createChild("val", TRUE)->setValue(llformat("%.5f", color.mV[VALPHA]));
			//	//<radius val="10.0" />
			//	LLXMLNodePtr radius_xml = light_xml->createChild("radius", FALSE);
			//	radius_xml->createChild("val", TRUE)->setValue(llformat("%.5f", light.getRadius()));
			//	//<falloff val="0.75" />
			//	LLXMLNodePtr falloff_xml = light_xml->createChild("falloff", FALSE);
			//	falloff_xml->createChild("val", TRUE)->setValue(llformat("%.5f", light.getFalloff()));
			//}
			// Sculpt
			if (prim.has("sculpt"))
			{
				LLSculptParams sculpt;
				sculpt.fromLLSD(prim["sculpt"]);

				//<topology val="4" />
				LLXMLNodePtr topology_xml = prim_xml->createChild("topology", FALSE);
				topology_xml->createChild("val", TRUE)->setValue(llformat("%u", sculpt.getSculptType()));

				//<sculptmap_uuid>1e366544-c287-4fff-ba3e-5fafdba10272</sculptmap_uuid>
				//<sculptmap_file>apple_map.tga</sculptmap_file>
				//FIXME png/tga/j2c selection itt.
				std::string sculpttexture;
				sculpt.getSculptTexture().toString(sculpttexture);
				prim_xml->createChild("sculptmap_uuid", FALSE)->setValue(sculpttexture);
			}

			//TextureEntry
			//shape_xml->createChild("TextureEntry", FALSE)->setValue(llsd_to_textureentry(prim["textures"]));

			//<inventory>
			prim_xml->createChild("FolderID", FALSE)->createChild("Guid", FALSE)->setValue(object_uuid.asString());
			LLXMLNodePtr inventory_xml = prim_xml->createChild("TaskInventory", FALSE);

			U32 num_of_items = 0;

			if(prim.has("inventory"))
			{
				LLSD inventory = prim["inventory"];
				//for each inventory item
				for (LLSD::array_iterator inv = inventory.beginArray(); inv != inventory.endArray(); ++inv)
				{
					LLSD item = (*inv);

					//skip by undefined items
					if(item.isUndefined()) continue;

					++num_of_items;

					//<TaskInventoryItem>
					LLXMLNodePtr field_xml = inventory_xml->createChild("TaskInventoryItem", FALSE);
					   //<Description>2008-01-29 05:01:19 note card</Description>
					field_xml->createChild("Description", FALSE)->setValue(item["desc"].asString());
					if(item.has("asset_id"))
					{
					   //<ItemID><Guid>673b00e8-990f-3078-9156-c7f7b4a5f86c</Guid></ItemID>
						field_xml->createChild("ItemID", FALSE)->createChild("Guid", FALSE)->setValue(item["item_id"].asString());
					   //<AssetID><Guid>673b00e8-990f-3078-9156-c7f7b4a5f86c</Guid></AssetID>
						field_xml->createChild("AssetID", FALSE)->createChild("Guid", FALSE)->setValue(item["asset_id"].asString());
					}
					else
					{
						//<ItemID><Guid>673b00e8-990f-3078-9156-c7f7b4a5f86c</Guid></ItemID>
						//welp, we don't have an asset id, assume that the backup's screwed and has been using the itemid as the assetid
						LLUUID rand_itemid;
						rand_itemid.generate();
						field_xml->createChild("ItemID", FALSE)->createChild("Guid", FALSE)->setValue(rand_itemid.asString());
						//<AssetID><Guid>673b00e8-990f-3078-9156-c7f7b4a5f86c</Guid></AssetID>
						field_xml->createChild("AssetID", FALSE)->createChild("Guid", FALSE)->setValue(item["item_id"].asString());
					}
					   //<name>blah blah</name>
					field_xml->createChild("Name", FALSE)->setValue(item["name"].asString());
					   //<type>10</type>

					LLAssetType::EType asset_type = LLAssetType::lookup(item["type"].asString());

					field_xml->createChild("Type", FALSE)->setValue(llformat("%d", asset_type));
					field_xml->createChild("InvType", FALSE)->setValue(llformat("%d", LLInventoryType::defaultForAssetType(asset_type)));
				} // end for each inventory item
				//add this prim to the linkset.

			}

			//I don't even know if this is right, as far as I can tell InventorySerial isn't even used for anything
			prim_xml->createChild("InventorySerial", FALSE)->setValue(llformat("%d", num_of_items));

			prim_xml->addChild(inventory_xml);

			//check if this is the first prim in the linkset
			if(is_root_prim)
			{
				linkset_xml->addChild(prim_xml);
				//we want this after
				linkset_xml->addChild(child_container);
			}
			else
				child_container->addChild(prim_xml);

			//obviously, none of the next prims are going to be root prims
			is_root_prim = false;
			++link_num;
		}
		// Create a file stream and write to it
		std::string linkset_file_path = outputPath + sep + "objects" + sep +
				linkset_name + "_" + pretty_position + "__" + linkset_id + ".xml";
		llofstream out(linkset_file_path,std::ios_base::out | std::ios_base::trunc);
		if (!out.good())
		{
			llwarns << "\nUnable to open \"" + linkset_file_path + "\" for output." << llendl;
		}
		else
		{
			linkset_xml->writeToOstream(out, std::string(), FALSE);
			out.close();
		}

		std::cout << "=" << std::flush;
	}

	// Create the archive.xml
	LLXMLNode* archive_info_xml = new LLXMLNode("archive", FALSE);

	//We use the 0.1 format for now.
	archive_info_xml->createChild("major_version", TRUE)->setValue("0");
	archive_info_xml->createChild("minor_version", TRUE)->setValue("1");

	LLXMLNodePtr creation_info_xml = archive_info_xml->createChild("creation_info", FALSE);

	//doesn't particularly matter when it was exported
	creation_info_xml->createChild("datetime", FALSE)->setValue("1");

	LLUUID random_archive_id;
	random_archive_id.generate();

	creation_info_xml->createChild("id", FALSE)->setValue(random_archive_id.asString());

	std::string archive_info_path = outputPath + sep + "archive.xml";
	llofstream out(archive_info_path,std::ios_base::out | std::ios_base::trunc);

	if(!out.good())
	{
		llwarns << "\nUnable to open \"" + archive_info_path + "\" for output." << llendl;
	}
	else
	{
		out << "<?xml version=\"1.0\" encoding=\"utf-16\"?>" << std::endl;
		archive_info_xml->writeToOstream(out, std::string(), FALSE);
		out.close();
	}

	std::cout << std::endl;
}

//This function accepts the HPA <group> object and returns all nested objects and linksets as LLSD.
LLSD hpa_converter::parse_hpa_group(LLXmlTreeNode* group)
{
	LLSD group_llsd;

	for (LLXmlTreeNode* child = group->getFirstChild(); child; child = group->getNextChild())
	{
		if (child->hasName("center"))
		{
		}
		else if (child->hasName("max"))
		{
		}
		else if (child->hasName("min"))
		{
		}
		else if (child->hasName("group"))
		{
			//printinfo("parsing group");
			//group nested in a group
			//This is the heavy lifter right here -Haz
			LLSD temp = parse_hpa_group(child);
			group_llsd.append(temp);
		}
		else if (child->hasName("linkset"))
		{
			//printinfo("parsing linkset");
			LLSD temp = parse_hpa_linkset(child);
			if (temp)
			{
				LLSD object;
				object["Object"] = temp;
				group_llsd.append(object);
			}
			else
				printinfo("ERROR, INVALID LINKSET");
		}
		else
		{
			//printinfo("parsing object");
			LLSD temp = parse_hpa_object(child);
			if (temp)
			{
				//our code assumes everything is a linkset so insert this lone object into an array
				LLSD array_llsd;
				array_llsd[array_llsd.size()] = temp;

				//then add it to a linkset
				LLSD linkset;
				linkset["Object"] = array_llsd;
				group_llsd.append(linkset);
			}
			else
				printinfo("ERROR, INVALID OBJECT");
		}

		//llinfos << "total linksets = "<<group_llsd.size()<<llendl;

	}
	return group_llsd;
}

//This function accepts a <linkset> XML object and returns the LLSD of the linkset.
LLSD hpa_converter::parse_hpa_linkset(LLXmlTreeNode* linkset)
{
	LLSD linkset_llsd;

	for (LLXmlTreeNode* child = linkset->getFirstChild(); child; child = linkset->getNextChild())
	{
		//printinfo("parsing object");
		LLSD temp = parse_hpa_object(child);
		if (temp)
		{
			//std::stringstream temp2;
			//LLSDSerialize::toPrettyXML(temp, temp2);
			//printinfo(temp2.str());
			linkset_llsd[linkset_llsd.size()] = temp;
		}

	}
	return linkset_llsd;
}

//This function accepts a <box>,<cylinder>,<etc> XML object and returns the object LLSD.
LLSD hpa_converter::parse_hpa_object(LLXmlTreeNode* prim)
{
	LLSD prim_llsd;
	LLVolumeParams volume_params;
	std::string name, description;
	LLSD prim_scale, prim_pos, prim_rot;
	F32 shearx = 0.f, sheary = 0.f;
	F32 taperx = 0.f, tapery = 0.f;
	S32 selected_type = MI_BOX;
	S32 selected_hole = 1;
	F32 cut_begin = 0.f;
	F32 cut_end = 1.f;
	F32 skew = 0.f;
	F32 radius_offset = 0.f;
	F32 revolutions = 1.f;
	F32 adv_cut_begin = 0.f;
	F32 adv_cut_end = 1.f;
	F32 hollow = 0.f;
	F32 twist_begin = 0.f;
	F32 twist = 0.f;
	F32 scale_x=1.f, scale_y=1.f;
	LLUUID sculpttexture;
	U8 topology = 0;
	U8 type = 0;
	LLPCode pcode = 0;
	BOOL is_object = true;
	BOOL is_phantom = false;

	if (prim->hasName("box"))
		selected_type = MI_BOX;
	else if (prim->hasName("cylinder"))
		selected_type = MI_CYLINDER;
	else if (prim->hasName("prism"))
		selected_type = MI_PRISM;
	else if (prim->hasName("sphere"))
		selected_type = MI_SPHERE;
	else if (prim->hasName("torus"))
		selected_type = MI_TORUS;
	else if (prim->hasName("tube"))
		selected_type = MI_TUBE;
	else if (prim->hasName("ring"))
		selected_type = MI_RING;
	else if (prim->hasName("sculpt"))
		selected_type = MI_SCULPT;
	else if (prim->hasName("tree"))
		pcode = LL_PCODE_LEGACY_TREE;
	else if (prim->hasName("grass"))
		pcode = LL_PCODE_LEGACY_GRASS;
	else {
		printinfo("ERROR INVALID OBJECT, skipping.");
		return false;
	}

	if (is_object)
	{
		//COPY PASTE FROM LLPANELOBJECT
		// Figure out what type of volume to make
		U8 profile;
		U8 path;
		switch ( selected_type )
		{
			case MI_CYLINDER:
				profile = LL_PCODE_PROFILE_CIRCLE;
				path = LL_PCODE_PATH_LINE;
				break;

			case MI_BOX:
				profile = LL_PCODE_PROFILE_SQUARE;
				path = LL_PCODE_PATH_LINE;
				break;

			case MI_PRISM:
				profile = LL_PCODE_PROFILE_EQUALTRI;
				path = LL_PCODE_PATH_LINE;
				break;

			case MI_SPHERE:
				profile = LL_PCODE_PROFILE_CIRCLE_HALF;
				path = LL_PCODE_PATH_CIRCLE;
				break;

			case MI_TORUS:
				profile = LL_PCODE_PROFILE_CIRCLE;
				path = LL_PCODE_PATH_CIRCLE;
				break;

			case MI_TUBE:
				profile = LL_PCODE_PROFILE_SQUARE;
				path = LL_PCODE_PATH_CIRCLE;
				break;

			case MI_RING:
				profile = LL_PCODE_PROFILE_EQUALTRI;
				path = LL_PCODE_PATH_CIRCLE;
				break;

			case MI_SCULPT:
				profile = LL_PCODE_PROFILE_CIRCLE;
				path = LL_PCODE_PATH_CIRCLE;
				break;

			default:
				llwarns << "Unknown base type " << selected_type
					<< " in getVolumeParams()" << llendl;
				// assume a box
				selected_type = MI_BOX;
				profile = LL_PCODE_PROFILE_SQUARE;
				path = LL_PCODE_PATH_LINE;
				break;
		}

		for (LLXmlTreeNode* param = prim->getFirstChild(); param; param = prim->getNextChild())
		{
			//<name><![CDATA[Object]]></name>
			if (param->hasName("name"))
				name = param->getTextContents();
			//<description><![CDATA[]]></description>
			else if (param->hasName("description"))
				description = param->getTextContents();
			//<position x="115.80774" y="30.13144" z="41.09710" />
			else if (param->hasName("position"))
			{
				LLVector3 vec;
				param->getAttributeF32("x", vec.mV[VX]);
				param->getAttributeF32("y", vec.mV[VY]);
				param->getAttributeF32("z", vec.mV[VZ]);
				prim_pos.append((F64)vec.mV[VX]);
				prim_pos.append((F64)vec.mV[VY]);
				prim_pos.append((F64)vec.mV[VZ]);

				//printinfo("pos: " + llformat("%.1f, %.1f, %.1f ", vec.mV[VX], vec.mV[VY], vec.mV[VZ]));
			}
			//<size x="0.50000" y="0.50000" z="0.50000" />
			else if (param->hasName("size"))
			{
				LLVector3 vec;
				param->getAttributeF32("x", vec.mV[VX]);
				param->getAttributeF32("y", vec.mV[VY]);
				param->getAttributeF32("z", vec.mV[VZ]);
				prim_scale.append((F64)vec.mV[VX]);
				prim_scale.append((F64)vec.mV[VY]);
				prim_scale.append((F64)vec.mV[VZ]);

				//printinfo("size: " + llformat("%.1f, %.1f, %.1f ", vec.mV[VX], vec.mV[VY], vec.mV[VZ]));
			}
			//<rotation w="1.00000" x="0.00000" y="0.00000" z="0.00000" />
			else if (param->hasName("rotation"))
			{
				LLQuaternion quat;
				param->getAttributeF32("w", quat.mQ[VW]);
				param->getAttributeF32("x", quat.mQ[VX]);
				param->getAttributeF32("y", quat.mQ[VY]);
				param->getAttributeF32("z", quat.mQ[VZ]);
				prim_rot.append((F64)quat.mQ[VX]);
				prim_rot.append((F64)quat.mQ[VY]);
				prim_rot.append((F64)quat.mQ[VZ]);
				prim_rot.append((F64)quat.mQ[VW]);
			}


			//<phantom val="true" />
			else if (param->hasName("phantom"))
			{
				std::string value;
				param->getAttributeString("val", value);
				if (value == "true")
					is_phantom = true;
			}

			//<top_shear x="0.00000" y="0.00000" />
			else if (param->hasName("top_shear"))
			{
				param->getAttributeF32("x", shearx);
				param->getAttributeF32("y", sheary);
			}
			//<taper x="0.00000" y="0.00000" />
			else if (param->hasName("taper"))
			{
				// Check if we need to change top size/hole size params.
				switch (selected_type)
				{
					case MI_SPHERE:
					case MI_TORUS:
					case MI_TUBE:
					case MI_RING:
						param->getAttributeF32("x", taperx);
						param->getAttributeF32("y", tapery);
						break;
					default:
						param->getAttributeF32("x", scale_x);
						param->getAttributeF32("y", scale_y);
						scale_x = 1.f - scale_x;
						scale_y = 1.f - scale_y;
						break;
				}
			}
			//<hole_size x="1.00000" y="0.05000" />
			else if (param->hasName("hole_size"))
			{
				param->getAttributeF32("x", scale_x);
				param->getAttributeF32("y", scale_y);
			}
			//<profile_cut begin="0.22495" end="0.77499" />
			else if (param->hasName("profile_cut"))
			{
				param->getAttributeF32("begin", adv_cut_begin);
				param->getAttributeF32("end", adv_cut_end);
			}
			//<path_cut begin="0.00000" end="1.00000" />
			else if (param->hasName("path_cut"))
			{
				param->getAttributeF32("begin", cut_begin);
				param->getAttributeF32("end", cut_end);
			}
			//<skew val="0.0" />
			else if (param->hasName("skew"))
			{
				param->getAttributeF32("val", skew);
			}
			//<radius_offset val="0.0" />
			else if (param->hasName("radius_offset"))
			{
				param->getAttributeF32("val", radius_offset);
			}
			//<revolutions val="1.0" />
			else if (param->hasName("revolutions"))
			{
				param->getAttributeF32("val", revolutions);
			}
			//<twist begin="0.00000" end="0.00000" />
			else if (param->hasName("twist"))
			{
				param->getAttributeF32("begin", twist_begin);
				param->getAttributeF32("end", twist);
			}
			//<hollow amount="40.99900" shape="4" />
			else if (param->hasName("hollow"))
			{
				param->getAttributeF32("amount", hollow);
				param->getAttributeS32("shape", selected_hole);
			}
			//<dimple begin="0.00000" end="0.00000" />
			else if (param->hasName("dimple"))
			{
				param->getAttributeF32("begin", adv_cut_begin);
				param->getAttributeF32("end", adv_cut_end);
			}
			//<topology val="1" />
			else if (param->hasName("topology"))
				param->getAttributeU8("val", topology);
			//<sculptmap_uuid>be293869-d0d9-0a69-5989-ad27f1946fd4</sculptmap_uuid>
			else if (param->hasName("sculptmap_uuid"))
			{
				sculpttexture = LLUUID(param->getTextContents());
			}

			//<type val="3" />
			else if (param->hasName("type"))
			{
				param->getAttributeU8("val", type);
			}

			//<light>
			else if (param->hasName("light"))
			{
				F32 lightradius = 0,  lightfalloff = 0;
				LLColor4 lightcolor;

				for (LLXmlTreeNode* lightparam = param->getFirstChild(); lightparam; lightparam = param->getNextChild())
				{
					//<color b="0" g="0" r="0" />
					if (lightparam->hasName("color"))
					{
						lightparam->getAttributeF32("r", lightcolor.mV[VRED]);
						lightparam->getAttributeF32("g", lightcolor.mV[VGREEN]);
						lightparam->getAttributeF32("b", lightcolor.mV[VBLUE]);
						lightcolor.mV[VRED]/=256;
						lightcolor.mV[VGREEN]/=256;
						lightcolor.mV[VBLUE]/=256;
					}
					//<intensity val="0.80392" />
					else if (lightparam->hasName("intensity"))
					{
							lightparam->getAttributeF32("val", lightcolor.mV[VALPHA]);
					}
					//<radius val="0.80392" />
					else if (lightparam->hasName("radius"))
					{
							lightparam->getAttributeF32("val", lightradius);
					}
					//<falloff val="0.80392" />
					else if (lightparam->hasName("falloff"))
					{
							lightparam->getAttributeF32("val", lightfalloff);
					}
				}

				LLLightParams light;
				light.setColor(lightcolor);
				light.setRadius(lightradius);
				light.setFalloff(lightfalloff);
				//light.setCutoff(lightintensity);

				prim_llsd["light"] = light.asLLSD();
			}

			//<flexible>
			else if (param->hasName("flexible"))
			{
				F32 softness=0,  gravity=0, drag=0, wind=0, tension=0;
				LLVector3 force;

				for (LLXmlTreeNode* flexiparam = param->getFirstChild(); flexiparam; flexiparam = param->getNextChild())
				{
					//<force x="0.05000" y="0.00000" z="0.03000" />
					if (flexiparam->hasName("force"))
					{
						flexiparam->getAttributeF32("x", force.mV[VX]);
						flexiparam->getAttributeF32("y", force.mV[VY]);
						flexiparam->getAttributeF32("z", force.mV[VZ]);
					}

					//<softness val="2.00000" />
					else if (flexiparam->hasName("softness"))
					{
						flexiparam->getAttributeF32("val", softness);
					}
					//<gravity val="0.30000" />
					else if (flexiparam->hasName("gravity"))
					{
						flexiparam->getAttributeF32("val", gravity);
					}
					//<drag val="2.00000" />
					else if (flexiparam->hasName("drag"))
					{
						flexiparam->getAttributeF32("val", drag);
					}
					//<wind val="0.00000" />
					else if (flexiparam->hasName("wind"))
					{
						flexiparam->getAttributeF32("val", wind);
					}
					//<tension val="1.00000" />
					else if (flexiparam->hasName("tension"))
					{
						flexiparam->getAttributeF32("val", tension);
					}
				}
				LLFlexibleObjectData new_attributes;
				new_attributes.setSimulateLOD(softness);
				new_attributes.setGravity(gravity);
				new_attributes.setTension(tension);
				new_attributes.setAirFriction(drag);
				new_attributes.setWindSensitivity(wind);
				new_attributes.setUserForce(force);

				prim_llsd["flexible"] = new_attributes.asLLSD();
			}

			//<texture>
			else if (param->hasName("texture"))
			{
				LLSD textures;
				S32 texture_count = 0;

				//printinfo("texture found");
				for (LLXmlTreeNode* face = param->getFirstChild(); face; face = param->getNextChild())
				{
					LLTextureEntry thisface;
					std::string imagefile;
					LLUUID imageuuid;

					//<face id="0">
					for (LLXmlTreeNode* param = face->getFirstChild(); param; param = face->getNextChild())
					{
						//<tile u="1.00000" v="-0.90000" />
						if (param->hasName("tile"))
						{
							F32 u,v;
							param->getAttributeF32("u", u);
							param->getAttributeF32("v", v);
							thisface.setScale(u,v);
						}
						//<offset u="0.00000" v="0.00000" />
						else if (param->hasName("offset"))
						{
							F32 u,v;
							param->getAttributeF32("u", u);
							param->getAttributeF32("v", v);
							thisface.setOffset(u,v);
						}
						//<rotation w="0.00000" />
						else if (param->hasName("rotation"))
						{
							F32 temp;
							param->getAttributeF32("w", temp);
							thisface.setRotation(temp * DEG_TO_RAD);
						}
						//<image_file><![CDATA[87008270-fe87-bf2a-57ea-20dc6ecc4e6a.tga]]></image_file>
						else if (param->hasName("image_file"))
						{
							imagefile = param->getTextContents();
						}
						//<image_uuid>87008270-fe87-bf2a-57ea-20dc6ecc4e6a</image_uuid>
						else if (param->hasName("image_uuid"))
						{
							imageuuid = LLUUID(param->getTextContents());
						}
						//<color b="1.00000" g="1.00000" r="1.00000" />
						else if (param->hasName("color"))
						{
							LLColor3 color;
							param->getAttributeF32("r", color.mV[VRED]);
							param->getAttributeF32("g", color.mV[VGREEN]);
							param->getAttributeF32("b", color.mV[VBLUE]);
							thisface.setColor(LLColor3(color.mV[VRED]/255.f,color.mV[VGREEN]/255.f,color.mV[VBLUE]/255.f));
						}
						//<transparency val="1.00000" />
						else if (param->hasName("transparency"))
						{
							F32 temp;
							param->getAttributeF32("val", temp);
							thisface.setAlpha((100.f - temp) / 100.f);
						}
						//<glow val="0.00000" />
						else if (param->hasName("glow"))
						{
							F32 temp;
							param->getAttributeF32("val", temp);
							thisface.setGlow(temp);
						}
						//<fullbright val="true" />
						else if (param->hasName("fullbright"))
						{
							int temp = 0;
							std::string value;
							param->getAttributeString("val", value);
							if (value == "true")
								temp = 1;
							thisface.setFullbright(temp);
						}
						//<shiny val="true" />
						else if (param->hasName("shine"))
						{
							U8 shiny;
							param->getAttributeU8("val", shiny);
							thisface.setShiny(shiny);
						}
					}

					if (imageuuid.notNull())
					{
						//an image UUID was specified, lets use it
						thisface.setID(imageuuid);
					}
					else if (imagefile != "")
					{
						//an image file was specified
						printinfo("imagefile = " + imagefile);
						//generate a temporary UUID that will be replaced once this texture is uploaded
						LLUUID temp;
						temp.generate();
						thisface.setID(temp);
					}
					else
					{
						printinfo("ERROR: no valid texture found for current face");
						//make this an ERROR texture or something
						//temp = error!
					}

					textures[texture_count] = thisface.asLLSD();
					texture_count++;
				}
				prim_llsd["textures"] = textures;

			}

			//<inventory>
			else if (param->hasName("inventory"))
			{
				LLSD inventory;
				S32 inventory_count = 0;

				for (LLXmlTreeNode* item = param->getFirstChild(); item; item = param->getNextChild())
				{
					LLSD sd;

					//<item>
					for (LLXmlTreeNode* param = item->getFirstChild(); param; param = item->getNextChild())
					{
						//<description>2008-01-29 05:01:19 note card</description>
						if (param->hasName("description"))
							sd["desc"] = param->getTextContents();
						//<item_id>673b00e8-990f-3078-9156-c7f7b4a5f86c</item_id>
						else if (param->hasName("item_id"))
							sd["item_id"] = param->getTextContents();
						//<asset_id>imagine a uuid here</asset_id>
						else if (param->hasName("asset_id"))
							sd["asset_id"] = param->getTextContents();
						//<name>blah blah</name>
						else if (param->hasName("name"))
							sd["name"] = param->getTextContents();
						//<type>notecard</type>
						else if (param->hasName("type"))
							sd["type"] = param->getTextContents();
					}
					inventory[inventory_count] = sd;
					inventory_count++;
				}
				prim_llsd["inventory"] = inventory;

			}
		}

		U8 hole;
		switch (selected_hole)
		{
		case 3:
			hole = LL_PCODE_HOLE_CIRCLE;
			break;
		case 2:
			hole = LL_PCODE_HOLE_SQUARE;
			break;
		case 4:
			hole = LL_PCODE_HOLE_TRIANGLE;
			break;
		case 1:
		default:
			hole = LL_PCODE_HOLE_SAME;
			break;
		}

		volume_params.setType(profile | hole, path);
		//mSelectedType = selected_type;

		// Compute cut start/end

		// Make sure at least OBJECT_CUT_INC of the object survives
		if (cut_begin > cut_end - OBJECT_MIN_CUT_INC)
		{
			cut_begin = cut_end - OBJECT_MIN_CUT_INC;
		}

		// Make sure at least OBJECT_CUT_INC of the object survives
		if (adv_cut_begin > adv_cut_end - OBJECT_MIN_CUT_INC)
		{
			adv_cut_begin = adv_cut_end - OBJECT_MIN_CUT_INC;
		}

		F32 begin_s, end_s;
		F32 begin_t, end_t;

		if (selected_type == MI_SPHERE || selected_type == MI_TORUS ||
			selected_type == MI_TUBE   || selected_type == MI_RING)
		{
			begin_s = adv_cut_begin;
			end_s	= adv_cut_end;

			begin_t = cut_begin;
			end_t	= cut_end;
		}
		else
		{
			begin_s = cut_begin;
			end_s	= cut_end;

			begin_t = adv_cut_begin;
			end_t	= adv_cut_end;
		}

		volume_params.setBeginAndEndS(begin_s, end_s);
		volume_params.setBeginAndEndT(begin_t, end_t);

		// Hollowness
		hollow = hollow/100.f;
		if (  selected_hole == MI_HOLE_SQUARE &&
			( selected_type == MI_CYLINDER || selected_type == MI_TORUS ||
			  selected_type == MI_PRISM    || selected_type == MI_RING  ||
			  selected_type == MI_SPHERE ) )
		{
			if (hollow > 0.7f) hollow = 0.7f;
		}

		volume_params.setHollow( hollow );

		// Twist Begin,End
		// Check the path type for twist conversion.
		if (path == LL_PCODE_PATH_LINE || path == LL_PCODE_PATH_FLEXIBLE)
		{
			twist_begin	/= OBJECT_TWIST_LINEAR_MAX;
			twist		/= OBJECT_TWIST_LINEAR_MAX;
		}
		else
		{
			twist_begin	/= OBJECT_TWIST_MAX;
			twist		/= OBJECT_TWIST_MAX;
		}

		volume_params.setTwistBegin(twist_begin);
		volume_params.setTwist(twist);

		volume_params.setRatio( scale_x, scale_y );
		volume_params.setSkew(skew);
		volume_params.setTaper( taperx, tapery );
		volume_params.setRadiusOffset(radius_offset);
		volume_params.setRevolutions(revolutions);

		// Shear X,Y
		volume_params.setShear( shearx, sheary );

		if (selected_type == MI_SCULPT)
		{
			LLSculptParams sculpt;

			sculpt.setSculptTexture(sculpttexture);

			/* maybe we want the mirror/invert/etc data at some point?
			U8 sculpt_type = 0;

			if (mCtrlSculptType)
				sculpt_type |= mCtrlSculptType->getCurrentIndex();

			if ((mCtrlSculptMirror) && (mCtrlSculptMirror->get()))
				sculpt_type |= LL_SCULPT_FLAG_MIRROR;

			if ((mCtrlSculptInvert) && (mCtrlSculptInvert->get()))
				sculpt_type |= LL_SCULPT_FLAG_INVERT; */

			sculpt.setSculptType(topology);

			prim_llsd["sculpt"] = sculpt.asLLSD();
		}

		//we should have all our params by now, pack the LLSD.
		prim_llsd["name"] = name;
		prim_llsd["description"] = description;
		prim_llsd["position"] = prim_pos;
		prim_llsd["rotation"] = prim_rot;

		prim_llsd["scale"] = prim_scale;
		// Flags
		//prim_llsd["shadows"] = object->flagCastShadows();
		if (is_phantom)
			prim_llsd["phantom"] = is_phantom;
		//prim_llsd["physical"] = (BOOL)(object->mFlags & FLAGS_USE_PHYSICS);

		if (pcode == LL_PCODE_LEGACY_GRASS || pcode == LL_PCODE_LEGACY_TREE)
		{
			prim_llsd["pcode"] = pcode;
			prim_llsd["state"] = type;
		}
		else
			// Volume params
			prim_llsd["volume"] = volume_params.asLLSD();
	}
	return prim_llsd;
}

std::string hpa_converter::llsd_to_textureentry(LLSD te_faces)
{
	/*
	< stupid workaround for TEs >
	 ---------------------------
			\   ^__^
			 \  (oo)\_______
				(__)\       )\/\
					||----w |
					||     ||


	Imagine this: This is packed to LLSD/XML, from LLSD.
	Then it is unpacked into OSD. Then it is repacked into a bitfield.
	That bitfield is then packed into a binary blob, and then the binary blob
	is packed into a base64-encoded string. Nice. I don't even know if it's right
	*/


	LLSD osd_tes; //textureentities to be parsed by the OSD parser
	LLSD default_face; //the default face (which isn't even really used)

	for(int color_num = 0; color_num<4; ++color_num)
		default_face["colors"][color_num] = 0.;

	default_face["scales"] = 1.;
	default_face["scalet"] = 1.;
	default_face["offsets"] = 0.;
	default_face["offsett"] = 0.;
	default_face["imagerot"] = 0.;
	default_face["bump"] = 0;
	default_face["shiny"] = 0;
	default_face["fullbright"] = false;
	default_face["media_flags"] = 0;
	default_face["mapping"] = 0;
	default_face["glow"] = 0.;
	default_face["imageid"] = LLUUID("89556747-24cb-43ed-920b-47caed15465f"); //plywood

	osd_tes.append(default_face);

	for (int i = 0; i < te_faces.size(); i++)
	{
		te_faces[i]["face_number"] = i; //I hope these are in order!
		te_faces[i]["mapping"] = 0; //hmm, looks like the mapping type isn't in the HPA file format
		te_faces[i]["fullbright"] = te_faces[i]["fullbright"].asBoolean();

		osd_tes.append(te_faces[i]);
	}

	std::string encoded_te = "";

	std::string temp_te_path = "temp_te.xml";
	llofstream out(temp_te_path,std::ios_base::out | std::ios_base::trunc);

	if (!out.good())
	{
		llwarns << "\nUnable to open \"" + temp_te_path + "\" for output." << llendl;
	}
	else
	{
		LLSDSerialize::toXML(osd_tes, out);
		out.close();

		std::string exe_path = gDirUtilp->getExecutableDir();
		exe_path += gDirUtilp->getDirDelimiter();
		exe_path += "TEUnscrewer.exe";

		if(!gDirUtilp->fileExists(exe_path)) llerrs << "Then who was TEUnscrewer.exe?" << llendl;

//DUMB DUMB DUMB
#if !LL_WINDOWS
		system("mono TEUnscrewer.exe");
#else
		system(exe_path.c_str());
#endif

		//the TE unscrewer will have written a file for us, hoorah

		std::string encoded_te_path = "encoded_te.txt";
		llifstream in(encoded_te_path,std::ios_base::in);

		if (!in.is_open())
		{
			llwarns << "\nUnable to open \"" + encoded_te_path + "\" for input." << llendl;
		}
		else
		{
			std::getline(in, encoded_te);

			LLFile::remove(encoded_te_path);
		}

		LLFile::remove(temp_te_path);
	}

	return encoded_te;
}

///////////////////////
//asset handling code//
///////////////////////

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
}

std::string LLAssetTools::HPAtoOARName(std::string src_filename)
{
	LLAssetType::EType file_type = typefromExt(src_filename);
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
		break;
	}

	llwarns << "For " << src_filename << ": This asset type isn't supported yet." << llendl;
	return std::string("");
}
