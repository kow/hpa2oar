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

#include "llsdserialize.h"
#include "llsdserialize_xml.h"

#include "asset_tools.h"

#include "hpa2oar.h"

#ifndef LL_WINDOWS
#	define Sleep sleep
#	include "unistd.h"
#endif

using namespace LLError;

int main(int argv,char * argc[])
{
	ll_init_apr();

	LLError::initForApplication(".");
	gDirUtilp->initAppDirs("hpa2oar",""); // path search is broken, if the pp_read_only_data_dir is not set to something searches fail

	sep = gDirUtilp->getDirDelimiter();

	// ensure converter dies before ll_cleanup_apr() is called
	{
		hpa_converter converter;

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
			llerrs << "HPA path provided doesn't exist"<<llendl;

		if(gDirUtilp->fileExists(converter.outputPath))
			llerrs << "Output path exists!" << llendl;

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
	copy_assets();
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

void hpa_converter::copy_assets_from(std::string asset_path, std::string mask)
{
	BOOL found = TRUE;
	std::string fname;

	std::string asset_dir = outputPath + sep + "assets" + sep;

	while(found)// for every directory
	{
		if((found = gDirUtilp->getNextFileInDir(asset_path,mask, fname, FALSE)))
		{
			std::string fullpath=asset_path+fname;
			if(LLFile::isfile(fullpath))
			{
				std::string new_fname = LLAssetTools::HPAtoOARName(fname);

				//check if we know about this asset type yet
				if(!new_fname.empty())
				{
					std::string new_path = asset_dir + new_fname;
					//copy the file to output dir
					if(!LLAssetTools::copyFile(fullpath, new_path))
						llwarns << "Failed to copy " << fpath << " to " << new_path << llendl;
				}
			}
		}
	}
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
		//if (child->hasName("schema"))
			//printinfo("Version: "+child->getTextContents());

		if (child->hasName("group"))
		{
			mOARFileContents = parse_hpa_group(child);

			//total_linksets = temp.size();
			//llinfos << "imported "<<temp.size()<<" linksets"<<llendl;


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
		return LLSD();
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
