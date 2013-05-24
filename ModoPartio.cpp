/*
 * ModoPartio.CPP	Plug-in Particle Generator Item Type
 *
 *	Copyright (c) 2008-2012 Luxology LLC
 *	
 *	Permission is hereby granted, free of charge, to any person obtaining a
 *	copy of this software and associated documentation files (the "Software"),
 *	to deal in the Software without restriction, including without limitation
 *	the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *	and/or sell copies of the Software, and to permit persons to whom the
 *	Software is furnished to do so, subject to the following conditions:
 *	
 *	The above copyright notice and this permission notice shall be included in
 *	all copies or substantial portions of the Software.   Except as contained
 *	in this notice, the name(s) of the above copyright holders shall not be
 *	used in advertising or otherwise to promote the sale, use or other dealings
 *	in this Software without prior written authorization.
 *	
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *	DEALINGS IN THE SOFTWARE.
 */
#include <lx_item.hpp>
#include <lx_package.hpp>
#include <lx_particle.hpp>
#include <lx_tableau.hpp>
#include <lx_action.hpp>
#include <lx_plugin.hpp>
#include <lx_value.hpp>
#include <lxu_attributes.hpp>
#include <lxu_math.hpp>
#include <lxu_log.hpp>
#include <lxvmath.h>
#include <lxidef.h>
#include <lx_vertex.hpp>	//	new in 701
#include <lx_channelui.hpp>
#include <lx_select.hpp>

#include <algorithm>

#include <Partio.h>

#include <boost/bimap.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/algorithm/string.hpp>    





static const char * paddingStringList[] = {
	"#", "##", "###", "####", "#####", NULL
};


typedef boost::bimap<std::string, std::string> ConversionBimap;
typedef ConversionBimap::value_type ConversionBimapValue;


class Conversion
{
public:
	ConversionBimap bimap;

	Conversion()
	{
		SetConstants();
	}
	Conversion(std::string format)
	{
		SetFormat(format);
	}

	void SetConstants()
	{
		bimap.insert(ConversionBimapValue("importedID", "id"));	//	avoid collision with id's for other formats/applications
		bimap.insert(ConversionBimapValue(LXsTBLX_PARTICLE_ID, "ModoID"));	

		bimap.insert(ConversionBimapValue(LXsTBLX_PARTICLE_POS, "position"));	//	dedicated Partio term 
	}

	void SetFormat(std::string format)
	{
		bimap.clear();
		SetConstants();

		boost::algorithm::to_lower(format);
		if (format == ".icecache")
		{
			bimap.insert(ConversionBimapValue(LXsTBLX_PARTICLE_XFRM, "Orientation"));
			bimap.insert(ConversionBimapValue(LXsTBLX_PARTICLE_SIZE, "Size"));
			bimap.insert(ConversionBimapValue(LXsTBLX_PARTICLE_VEL, "PointVelocity"));
			bimap.insert(ConversionBimapValue(LXsTBLX_PARTICLE_MASS, "Mass"));
			bimap.insert(ConversionBimapValue(LXsTBLX_PARTICLE_FORCE, "Force"));
			bimap.insert(ConversionBimapValue(LXsTBLX_PARTICLE_AGE, "Age"));
			bimap.insert(ConversionBimapValue(LXsTBLX_PARTICLE_ANGVEL, "AngularVelocity"));
			bimap.insert(ConversionBimapValue(LXsTBLX_PARTICLE_RGB, "Color"));
		}
		else if (format == ".bin")
		{
			bimap.insert(ConversionBimapValue(LXsTBLX_PARTICLE_VEL, "velocity"));
		}
	}
};


struct ParticleFeature
{
	std::string name;
	unsigned offset;
	unsigned size;		//	number of floats
	Partio::ParticleAttribute attr;
	Partio::ParticleAccessor * pacc;

	ParticleFeature()
	{
		name = "";
		offset = -1;
		size = -1;
		pacc = NULL;
	}
	ParticleFeature(std::string in_name, unsigned in_offset, unsigned in_size)
	{
		name = in_name;
		offset = in_offset;
		size = in_size;
		pacc = NULL;
	}
	ParticleFeature(std::string in_name, unsigned in_offset, unsigned in_size, Partio::ParticleAttribute in_attr, Partio::ParticleAccessor * in_pacc)
	{
		name = in_name;
		offset = in_offset;
		size = in_size;
		attr = in_attr;
		pacc = in_pacc;
	}

	~ParticleFeature()
	{
		if (pacc)
		{
			delete pacc;
		}
	}
};

struct Compare : std::binary_function<ParticleFeature,ParticleFeature,bool> {
//	Compare(int i) : _i(i) { }

	bool operator()(const ParticleFeature& v1, const ParticleFeature& v2) const {
		return (v1.offset < v2.offset)/* && (_i != 1)*/;
	}

//	int _i;
};


#define SRVNAME_PACKAGE		"ModoPartio"
#define SPNNAME_INSTANCE	"ModoPartio.inst"
#define SPNNAME_GENERATOR	"ModoPartio.gen"


/*
 * Class Declarations
 *
 * These have to come before their implementations because they reference each
 * other. Descriptions are attached to the implementations.
 */
class CModoPartioPackage;

class CModoPartioGenerator :
                public CLxImpl_TableauSurface
//                public CLxDynamicAttributes
{
    public:
                static void
        initialize ()
        {
                CLxGenericPolymorph	*srv;

                srv = new CLxPolymorph<CModoPartioGenerator>;
                srv->AddInterface (new CLxIfc_TableauSurface<CModoPartioGenerator>);
 //               srv->AddInterface (new CLxIfc_Attributes    <CModoPartioGenerator>);
                lx::AddSpawner (SPNNAME_GENERATOR, srv);
        }


        CLxUser_Matrix		 w_matrix;
		int		frame;

		std::string		s_path, fileType, cacheFileName;
		CLxUser_Item pins_item;
        CLxUser_TriangleSoup	 tri_soup;
        CLxPseudoRandom		 rand_seq;
        int			 vrt_size;
        float			*vrt_vec;

		boost::ptr_vector<ParticleFeature> particleFeatures;

		Partio::ParticlesData * data;
		std::vector<std::string> particleAttributeNames;

		Conversion conversion;


        CModoPartioGenerator ();

        unsigned int	 tsrf_FeatureCount (LXtID4 type) LXx_OVERRIDE;
        LxResult	 tsrf_FeatureByIndex (LXtID4 type, unsigned int index, const char **name) LXx_OVERRIDE;
        LxResult	 tsrf_SetVertex (ILxUnknownID vdesc) LXx_OVERRIDE;
        LxResult	 tsrf_Sample (const LXtTableauBox bbox, float scale, ILxUnknownID trisoup) LXx_OVERRIDE;

	private:
		void		ReadModoPartio();
};

class CModoPartioInstance :
        public CLxImpl_PackageInstance,
        public CLxImpl_ParticleItem,
		public CLxImpl_TableauSource,
		public CLxImpl_PointCacheItem
{
	friend class CLxTriSoup;

    public:
                static void
        initialize ()
        {
                CLxGenericPolymorph	*srv;

                srv = new CLxPolymorph<CModoPartioInstance>;
                srv->AddInterface (new CLxIfc_PackageInstance<CModoPartioInstance>);
                srv->AddInterface (new CLxIfc_ParticleItem   <CModoPartioInstance>);
                srv->AddInterface (new CLxIfc_TableauSource  <CModoPartioInstance>);
				srv->AddInterface(new CLxIfc_PointCacheItem <CModoPartioInstance>);
                lx::AddSpawner (SPNNAME_INSTANCE, srv);
        }

        CLxSpawner<CModoPartioGenerator>	 gen_spawn;
        CLxUser_Item			 m_item;

		Partio::ParticlesDataMutable * pData;
		std::string fileName;
		std::string fileType;
		Partio::ParticleIndex particleIndex;
		unsigned int padding;
		std::string paddingString;

		Conversion conversion;

        CModoPartioInstance ()
                : gen_spawn (SPNNAME_GENERATOR), pData(NULL), paddingString("0000")
        {}

        /*
         * PackageInstance interface.
         */
        LxResult	 pins_Initialize (ILxUnknownID item, ILxUnknownID super) LXx_OVERRIDE;
        void		 pins_Cleanup (void) LXx_OVERRIDE;

        /*
         * ParticleItem interface.
         */
        LxResult	 prti_Prepare  (ILxUnknownID eval, unsigned *index) LXx_OVERRIDE;
        LxResult	 prti_Evaluate (ILxUnknownID attr, unsigned index, void **ppvObj) LXx_OVERRIDE;

        /*
         * TableauSource interface.
         */
        LxResult	 tsrc_PreviewUpdate (int chanIndex, int *update) LXx_OVERRIDE;

		/*
         * PointCacheItem interface.
         */
		boost::ptr_vector<ParticleFeature>particleFeatures;
//		std::vector<ParticleFeature> particleFeatures;

		LxResult pcache_Prepare(ILxUnknownID eval, unsigned *index) LXx_OVERRIDE;
		LxResult pcache_Initialize(ILxUnknownID vdesc, ILxUnknownID attr, unsigned index, double time, double sample) LXx_OVERRIDE;
		LxResult pcache_SaveFrame(ILxUnknownID pobj, double time) LXx_OVERRIDE;
		LxResult pcache_Cleanup() LXx_OVERRIDE;

	private:
		void AddVertex(const float *vertex,	unsigned int *index);

		static inline void CalculateRotation( float * q, const float * xfrm );
};

class CModoPartioPackage :
	public CLxImpl_Package,
	public CLxImpl_ChannelUI
				
{
    public:
        static LXtTagInfoDesc		 descInfo[];

                static void
        initialize ()
        {
                CLxGenericPolymorph	*srv;

                srv = new CLxPolymorph<CModoPartioPackage>;
                srv->AddInterface (new CLxIfc_Package   <CModoPartioPackage>);
                srv->AddInterface (new CLxIfc_StaticDesc<CModoPartioPackage>);
				srv->AddInterface(new CLxIfc_ChannelUI<CModoPartioPackage>);
                lx::AddServer (SRVNAME_PACKAGE, srv);
        }

        CLxSpawner<CModoPartioInstance>	 inst_spawn;

        CModoPartioPackage ()
                : inst_spawn (SPNNAME_INSTANCE)
        {}


        LxResult		pkg_SetupChannels (ILxUnknownID addChan) LXx_OVERRIDE;
        LxResult		pkg_TestInterface (const LXtGUID *guid) LXx_OVERRIDE;
        LxResult		pkg_Attach (void **ppvObj) LXx_OVERRIDE;

		LxResult		cui_UIHints(const char *channelName, ILxUnknownID hints) LXx_OVERRIDE; 
};



/*
 * ----------------------------------------------------------------
 * Package Class
 *
 * Packages implement item types, or simple item extensions. They are
 * like the metatype object for the item type.
 *
 * The ModoPartio package is a locator.
 */
LXtTagInfoDesc	 CModoPartioPackage::descInfo[] = {
        { LXsPKG_SUPERTYPE,	LXsITYPE_LOCATOR	},
        { 0 }
};


/*
 * The package has a set of standard channels with default values. These
 * are setup at the start using the AddChannel interface.
 */


        LxResult
CModoPartioPackage::pkg_SetupChannels (
        ILxUnknownID		 addChan)
{
        CLxUser_AddChannel	 ac (addChan);


		// Create a new LXtObjectID. This will be used for storing the default value.
		LXtObjectID	obj;

		// CLxUser_Value SetString() method will be used to set the value of the string channel.
		CLxUser_Value	val;

		// Add the new channel. Define it as a string channel.
		ac.NewChannel("cacheFileName", LXsTYPE_STRING);

		// Next, set the storage type as a string. This is the type of value
		// that will be stored in the action layer.
		ac.SetStorage(LXsTYPE_STRING);

		// Finally we set the default object to be the LXtObjectID (obj).
		// This returns an object which we can use to store the value.
		ac.SetDefaultObj(&obj);

		// Localize the LXtObjectID.
		val.take(obj);

		// Finally, the SetString method is called to set the default

		val.SetString("*.*");

		ac.NewChannel("padding", LXsTYPE_INTEGER);
		ac.SetDefault(0.0, 0);

		ac.NewChannel("frame", LXsTYPE_INTEGER);


        return LXe_OK;
}


/*
 * TestInterface() is required so that nexus knows what interfaces instance
 * of this package support. Necessary to prevent query loops.
 */
        LxResult
CModoPartioPackage::pkg_TestInterface (
        const LXtGUID		*guid)
{
        return inst_spawn.TestInterfaceRC (guid);
}


/*
 * Attach is called to create a new instance of this item. The returned
 * object implements a specific item of this type in the scene.
 */
        LxResult
CModoPartioPackage::pkg_Attach (
        void		       **ppvObj)
{
        inst_spawn.Alloc (ppvObj);
        return LXe_OK;
}


		LxResult
CModoPartioPackage::cui_UIHints(const char *channelName, ILxUnknownID hints)
		{
			std::string nameString(channelName);

			CLxUser_UIHints phints(hints);

			if (nameString.compare("padding") == 0)
			{
				phints.Class("iPopChoice");
				phints.Label("Output Padding");
				phints.StringList(paddingStringList);
			}


			return LXe_OK;
		}

/*
 * ----------------------------------------------------------------
 * ModoPartio Item Instance
 *
 * The instance is the implementation of the item, and there will be one
 * allocated for each item in the scene. It can respond to a set of
 * events. Initialization typically stores the item it's attached to.
 */
        LxResult
CModoPartioInstance::pins_Initialize (
        ILxUnknownID		 item,
        ILxUnknownID		 super)
{
        m_item.set (item);

        return LXe_OK;
}

        void
CModoPartioInstance::pins_Cleanup (void)
{
        m_item.clear ();
}


/*
 * The particle interface initializes and returns a particle object, which is then
 * used to access particle data. This first method is passed an evaluation object
 * which selects the channels it wants and returns a key index.
 */
        LxResult
CModoPartioInstance::prti_Prepare (
        ILxUnknownID		 evalObj,
        unsigned		*index)
{
        CLxUser_Evaluation	 eval (evalObj);

        index[0] =
            eval.AddChan (m_item, "cacheFileName");
//			eval.AddChan (m_item, "padding");
            eval.AddChan (m_item, LXsICHAN_XFRMCORE_WORLDMATRIX);
			eval.AddChan(m_item, "frame");

        return LXe_OK;
}

/*
 * Once prepared, the second method is called to actually create the object and
 * initialize its state from the channel values.
 */
        LxResult
CModoPartioInstance::prti_Evaluate (
        ILxUnknownID		 attr,
        unsigned		 index,
        void		       **ppvObj)
{
        CLxUser_Attributes	 ai (attr);
        CModoPartioGenerator	*gen = gen_spawn.Alloc (ppvObj);
		gen->pins_item = m_item;

		bool stringRead = ai.String(index + 0, gen->s_path);

        ai.ObjectRO            (index + 1, gen->w_matrix);		//	world matrix of locator

		gen->frame = ai.Int(index + 2);

        return LXe_OK;
}

        LxResult
CModoPartioInstance::tsrc_PreviewUpdate (
        int			 chanIndex,
        int			*update)
{
        *update = LXfTBLX_PREVIEW_UPDATE_GEOMETRY;

        return LXe_OK;
}




class CLxTriSoup :		//	Singleton COM object to pass to tableauSurface Sample method to collect particle data
	public CLxImpl_TriangleSoup,
	public CLxSingletonPolymorph
{
public:
	CModoPartioInstance * partioInstance;

	LXxSINGLETON_METHOD;

	CLxTriSoup ()
	{
		AddInterface (new CLxIfc_TriangleSoup<CLxTriSoup>);
	}

	unsigned int	 soup_TestBox (const LXtTableauBox bbox);
	LxResult	 soup_Segment (unsigned int segID, unsigned int type);
	LxResult	 soup_Vertex  (const float *vertex, unsigned int *index);
	LxResult	 soup_Polygon (unsigned int v0, unsigned int v1, unsigned int v2);
	void		 soup_Connect (unsigned int type);
};

unsigned int
	CLxTriSoup::soup_TestBox (
	const LXtTableauBox	 bbox)
{
	return 1;
}

LxResult
	CLxTriSoup::soup_Segment (
	unsigned int		 segID,
	unsigned int		 type)
{
	return LXe_TRUE;
}

LxResult
	CLxTriSoup::soup_Vertex  (
	const float		*vertex,
	unsigned int		*index)
{
	partioInstance->AddVertex(vertex, index);
	return LXe_OK;
}

LxResult
	CLxTriSoup::soup_Polygon (
	unsigned int		 v0,
	unsigned int		 v1,
	unsigned int		 v2)
{
	return LXe_OK;
}

void
	CLxTriSoup::soup_Connect (unsigned int type)
{
}


LxResult CModoPartioInstance::pcache_Prepare(ILxUnknownID evalObj, unsigned *index)
{
	CLxUser_Evaluation	 eval (evalObj);
	index[0] = eval.AddChan (m_item, "cacheFileName");
	eval.AddChan (m_item, "padding");

	return LXe_OK;
}

LxResult CModoPartioInstance::pcache_Initialize(ILxUnknownID vdesc, ILxUnknownID attr, unsigned index, double time, double sample)
{
	CLxUser_TableauVertex	 vrx (vdesc);	//	vdesc has particle features to be saved
	
	CLxUser_Attributes ai(attr);

	ai.String(index + 0, fileName);

	size_t last_period = fileName.find_last_of('.');
	if (last_period != fileName.npos)
	{
		fileType = fileName.substr(last_period);
		boost::algorithm::to_lower(fileType);
		fileName = fileName.substr(0, last_period);
	} 
	else
	{
		fileType = "";
	}

	size_t numbers = fileName.find_last_not_of("#1234567890");
	fileName = fileName.substr(0, numbers + 1);

	padding = ai.Int(index + 1) + 1;

	unsigned size = vrx.Size ();
	unsigned count = vrx.Count();

	LXtID4 type;
	const char * name = NULL;
	unsigned offset;
	unsigned next_offset = 0;

	particleFeatures.clear();

	for (unsigned int i = 0; i < count; ++i)
	{
		if (i == count - 1)
		{
			next_offset = size;
		} 
		else
		{
			vrx.ByIndex(i + 1, &type, &name, &next_offset);
		}
		vrx.ByIndex(i, &type, &name, &offset);
		particleFeatures.push_back(new ParticleFeature(name, offset, next_offset - offset));
	}

	conversion.SetFormat(fileType);

	return LXe_OK;
}

LxResult CModoPartioInstance::pcache_SaveFrame(ILxUnknownID pobj, double time)
{
	CLxUser_SceneService ssvc;

	CLxUser_Scene scn;
	scn.from(m_item);

	CLxUser_Item sceneItem;
	LXtObjectID obj;
	scn.ItemByIndex(ssvc.ItemType(LXsITYPE_SCENE), 0, &obj);
	sceneItem.set(obj);
	unsigned index;
	sceneItem.ChannelLookup(LXsICHAN_SCENE_FPS, &index);

	CLxUser_ChannelRead	 rchan;
	scn.GetChannels(rchan, time);

	double fps;
	rchan.Double(sceneItem, index, &fps);
	
	unsigned int frame = (unsigned int)floor(time * fps + 0.5);


	CLxUser_TableauSurface tsrf(pobj);

	CLxUser_TableauVertex tvrt;
	CLxUser_TableauService tsrv;
	tsrv.NewVertex(tvrt);

	unsigned offset;

	pData = Partio::create();
	
	boost::ptr_vector<ParticleFeature>::iterator particleFeature_Iter = particleFeatures.begin();

	Partio::ParticleAttributeType attrType;
	std::string attrName;
	unsigned attrSize;

	for (; particleFeature_Iter != particleFeatures.end(); ++particleFeature_Iter)
	{									
		tvrt.AddFeature(LXiTBLX_PARTICLES, particleFeature_Iter->name.c_str(), &offset);	//	first set up vertex description to ask for data to be sent to triangle soup
	}																						//	apparently order matters, but not offset. Just returns index to address of offset					

	particleFeature_Iter = particleFeatures.begin();
	for (; particleFeature_Iter != particleFeatures.end(); ++particleFeature_Iter)		//	next add corresponding attributes to Partio data object
	{
		attrSize = 0;

		ConversionBimap::left_const_iterator conversionIter = conversion.bimap.left.find(particleFeature_Iter->name);
		if (conversionIter != conversion.bimap.left.end())
		{
			attrName = conversionIter->second;
		} 
		else
		{
			attrName = particleFeature_Iter->name;
		}

		if (fileType.compare(".icecache") == 0)
		{
			if (attrName == "Orientation" || attrName == "AngularVelocity")
			{
				attrSize = 4;	//	quaternion rotation in icecache
			}
			else if (attrName == "Color")	//	RGBA in Softimage
			{
				attrSize = 4;
			}
		}

		if(attrSize == 0)
		{
			attrSize = particleFeature_Iter->size;
		}

		if (attrSize == 1)
		{
			attrType = Partio::FLOAT;
		}
		else
		{
			attrType = Partio::VECTOR;
		}

		particleFeature_Iter->attr = Partio::ParticleAttribute(pData->addAttribute(attrName.c_str(), attrType, attrSize));	//	in Modo all of the particle features are floats
	}
	LxResult rc = tsrf.SetVertex(tvrt);


	particleIndex = 0;
	
	CLxTriSoup trisoup;
	trisoup.partioInstance = this;
	LXtTableauBox bbox;
	bbox[0] = bbox[1] = bbox[2] = -1.0e30f;
	bbox[3] = bbox[4] = bbox[5] = 1.0e30f;
	tsrf.Sample(bbox, -1.0f, trisoup);

	std::string writeName = fileName;
	std::string frameString = std::to_string((_ULonglong)frame);
	if (frameString.size() < padding)
	{
		writeName += paddingString.substr(0, padding - frameString.size());
	}
	writeName = writeName + frameString + fileType;
	Partio::write(writeName.c_str(), *pData, true);

	pData->release();

	return LXe_OK;
}

LxResult CModoPartioInstance::pcache_Cleanup()
{
	return LXe_OK;
}

void CModoPartioInstance::CalculateRotation( float * q, const float * xfrm )
{
	float trace = xfrm[0] + xfrm[4] + xfrm[8]; 
	if( trace > 0 ) {
		float s = 0.5f / sqrtf(trace+ 1.0f);
		q[0] = 0.25f / s;
		q[1] = ( xfrm[5] - xfrm[7] ) * s;
		q[2] = ( xfrm[6] - xfrm[2] ) * s;
		q[3] = ( xfrm[1] - xfrm[3] ) * s;
	} 
	else {
		if ( xfrm[0] > xfrm[4] && xfrm[0] > xfrm[8] ) {
			float s = 2.0f * sqrtf( 1.0f + xfrm[0] - xfrm[4] - xfrm[8]);
			q[0] = (xfrm[5] - xfrm[7] ) / s;
			q[1] = 0.25f * s;
			q[2] = (xfrm[3] + xfrm[1] ) / s;
			q[3] = (xfrm[6] + xfrm[2] ) / s;
		} else if (xfrm[4] > xfrm[8]) {
			float s = 2.0f * sqrtf( 1.0f + xfrm[4] - xfrm[0] - xfrm[8]);
			q[0] = (xfrm[6] - xfrm[2] ) / s;
			q[1] = (xfrm[3] + xfrm[1] ) / s;
			q[2] = 0.25f * s;
			q[3] = (xfrm[7] + xfrm[5] ) / s;
		} else {
			float s = 2.0f * sqrtf( 1.0f + xfrm[8] - xfrm[0] - xfrm[4] );
			q[0] = (xfrm[1] - xfrm[3] ) / s;
			q[1] = (xfrm[6] + xfrm[2] ) / s;
			q[2] = (xfrm[7] + xfrm[5] ) / s;
			q[3] = 0.25f * s;
		}
	}
}

void CModoPartioInstance::AddVertex(const float *vertex, unsigned int *index)
{
	particleIndex = pData->addParticle();

	boost::ptr_vector<ParticleFeature>::iterator particleFeature_Iter = particleFeatures.begin();
	for (; particleFeature_Iter != particleFeatures.end(); ++particleFeature_Iter)
	{
		float * pFloatData = pData->dataWrite<float>(particleFeature_Iter->attr, particleIndex);
		if (fileType.compare(".icecache") == 0)
		{
			if (particleFeature_Iter->attr.name.compare("Orientation") == 0)	//	convert matrix to quaternion
			{
				const float * xfrm = vertex + particleFeature_Iter->offset;
				float quat[4];
				CalculateRotation(quat, xfrm);
				for (unsigned i=0; i < 4; ++i)
				{
					pFloatData[i] = quat[i];
				}
			} 
			else if (particleFeature_Iter->attr.name.compare("Color") == 0)
			{
				for (unsigned i=0; i < 3; ++i)
				{
					pFloatData[i] = vertex[particleFeature_Iter->offset + i];
				}
				pFloatData[3] = 1.0f;	//	Add alpha value
			}
			else if (particleFeature_Iter->attr.name.compare("AngularVelocity") == 0)
			{
				for (unsigned i=0; i < 3; ++i)
				{
					pFloatData[i] = vertex[particleFeature_Iter->offset + i];
				}
				float vect_length = sqrtf(pFloatData[0] * pFloatData[0] + pFloatData[1] * pFloatData[1] + pFloatData[2] * pFloatData[2]);
				pFloatData[3] = vect_length;			//		probably not the correct conversion
			}
			else
			{
				for (unsigned i=0; i < particleFeature_Iter->size; ++i)
				{
					pFloatData[i] = vertex[particleFeature_Iter->offset + i];
				}
			}
		}
		else
		{
			for (unsigned i=0; i < particleFeature_Iter->size; ++i)
			{
				pFloatData[i] = vertex[particleFeature_Iter->offset + i];
			}
		}
	}
	*index  = (unsigned int)particleIndex;	//	not sure this is needed
	
	return;
}


/*
 * ----------------------------------------------------------------
 * Particle Object
 *
 * The particle object can be enumerated by a client to get the data for
 * all particles. It also can preset an attributes interface to allow the
 * client to set hints, in this case the random seed.
 */
CModoPartioGenerator::CModoPartioGenerator ()
{
	data = NULL;
        //dyna_Add (LXsPARTICLEATTR_SEED, "integer");
        //attr_SetInt (0, 137);
}

/*
 * Like tableau surfaces, particle sources have features. These are the
 * properties of each particle as a vector of floats. We provide the standard
 * 3 particle features: position, transform, and ID.
 */
        unsigned int
CModoPartioGenerator::tsrf_FeatureCount (
        LXtID4			 type)
{
	boost::filesystem::path filePath(s_path);
	if (!filePath.has_parent_path())
	{
		return 0;
	}
	boost::filesystem::path fileParentPath = filePath.parent_path();

	fileType = filePath.extension().string();
	std::string fileStem = filePath.stem().string();
	size_t numbers = fileStem.find_last_not_of("#1234567890");

	boost::filesystem::path cacheFilePath;

	std::string filterString = fileStem.substr(0, numbers + 1) + "0*" + std::to_string((_ULONGLONG)frame) + fileType;
	boost::regex cacheFileFilter(filterString.c_str());
	boost::filesystem::directory_iterator end_iter; // Default ctor yields past-the-end
	boost::smatch m;
	for (boost::filesystem::directory_iterator iter(fileParentPath); iter != end_iter; ++iter)
	{
		if (!boost::filesystem::is_regular_file(iter->status()))
		{
			continue;
		}
		if (boost::regex_match(iter->path().filename().string(), m, cacheFileFilter))
		{
			cacheFilePath = iter->path();
			break;
		}
	}
	if (cacheFilePath.empty())
	{
		return 0;
	}

	boost::algorithm::to_lower(fileType);	//	Partio readers expect lower case
	cacheFilePath.replace_extension(boost::filesystem::path(fileType));

	conversion.SetFormat(fileType);

	data = Partio::readCached(cacheFilePath.string().c_str(), false);
	if (!data)
	{
		return 0;
	}

	int attribCount = data->numAttributes();
	particleAttributeNames.clear();
	Partio::ParticleAttribute attr;

	if (!data->attributeInfo("position",attr) || attr.type != Partio::VECTOR || attr.count != 3) 
	{
		return 0;							//	always need particle position data
	}
	particleAttributeNames.push_back("position");
	for (int i=0; i < attribCount; ++i)
	{
		data->attributeInfo(i, attr);
		if (attr.name != "position")
		{
			particleAttributeNames.push_back(attr.name);
		}
	}

	if (!data->attributeInfo(LXsTBLX_PARTICLE_ID, attr))
	{
		particleAttributeNames.push_back(LXsTBLX_PARTICLE_ID);					//	always set particle id
	}
	if (!data->attributeInfo(LXsTBLX_PARTICLE_XFRM, attr) && !(fileType == ".icecache" && data->attributeInfo("Orientation", attr)))
	{
		particleAttributeNames.push_back(LXsTBLX_PARTICLE_XFRM);							//	always set particle xfrm
	}

    return (type == LXiTBLX_PARTICLES ? (unsigned int)particleAttributeNames.size() : 0);
}

        LxResult
CModoPartioGenerator::tsrf_FeatureByIndex (
        LXtID4			 type,
        unsigned int		 index,
        const char	       **name)
{
        if (type != LXiTBLX_PARTICLES || index > (unsigned int)(particleAttributeNames.size()))
                return LXe_OUTOFBOUNDS;

		if (particleAttributeNames[index] == "position")
		{
			name[0] = LXsTBLX_PARTICLE_POS;
		}
		else if (fileType == ".icecache")
		{
			ConversionBimap::right_const_iterator conversionIter = conversion.bimap.right.find(particleAttributeNames[index]);
			if (conversionIter != conversion.bimap.right.end())
			{
				name[0] = conversionIter->second.c_str();	//	conversion available
			} 
			else
			{
				name[0] = particleAttributeNames[index].c_str();
			}
		} 
		else
		{
			name[0] = particleAttributeNames[index].c_str();
		}


        return LXe_OK;
}


/*
 * Given a tableau vertex allocated by the client, we need to determine the
 * features they want and their offsets into the vertex vector.
 */
        LxResult
CModoPartioGenerator::tsrf_SetVertex (
        ILxUnknownID		 vdesc)
{
        CLxUser_TableauVertex	 vrx (vdesc);


        vrt_size = vrx.Size ();
        if (vrt_size == 0)
                return LXe_OK;

        unsigned		 offset;
		const char * featureName;		//	name of modo particle feature
		std::string attrName;			//	Partio attribute name
		Partio::ParticleAttribute partioAttr;
		LXtID4 type;
		unsigned int featureCount = vrx.Count();
		particleFeatures.clear();
		for (unsigned int i=0; i < featureCount; ++i)
		{
			vrx.ByIndex(i, &type, &featureName, &offset);

			attrName = featureName;
			if (attrName == LXsTBLX_PARTICLE_POS)
			{
				attrName = "position";
			}
			else if (fileType == ".icecache")
			{
				ConversionBimap::left_const_iterator conversionIter = conversion.bimap.left.find(std::string(featureName));
				if (conversionIter != conversion.bimap.left.end())
				{
					attrName = conversionIter->second;
				} 
				else
				{
					attrName = featureName;
				}
			}


			if(data->attributeInfo(attrName.c_str(), partioAttr))
			{
				particleFeatures.push_back(new ParticleFeature(featureName, offset, 0, partioAttr, new Partio::ParticleAccessor(partioAttr)));
			}
			else
			{
				particleFeatures.push_back(new ParticleFeature(featureName, offset, 0));
			}
		}

		Compare cmp;
		particleFeatures.sort(cmp);	//	sorting probably not necessary since features already seem to be in this order

		unsigned int prev_offset = vrt_size;
		boost::ptr_vector<ParticleFeature>::reverse_iterator particleFeatures_Iter = particleFeatures.rbegin();	//	calculate size of features (# floats)
		for (; particleFeatures_Iter != particleFeatures.rend(); ++particleFeatures_Iter)
		{
			particleFeatures_Iter->size = std::min((int)(prev_offset - particleFeatures_Iter->offset), particleFeatures_Iter->attr.count);	//	make sure not trying to read more data than present in Partio
			prev_offset = particleFeatures_Iter->offset;
		}

        return LXe_OK;
}


/*
 * Sampling walks the particles.
 */
        LxResult
CModoPartioGenerator::tsrf_Sample (
        const LXtTableauBox	 bbox,
        float			 scale,
        ILxUnknownID		 trisoup)
{
        int			 i;
        LxResult		 result;

        /*
         * Allocate the vertex vector and init to zeros. We only need one
         * since points are sampled sequentially.
         */
        vrt_vec = new float[vrt_size];
        if (!vrt_vec)
                return LXe_OUTOFMEMORY;

        for (i = 0; i < vrt_size; i++)
                vrt_vec[i] = 0.0f;


        result = LXe_OK;
        try 
		{
            /*
             * Init the triangle soup.
             */
            tri_soup.set (trisoup);
            tri_soup.Segment (1, LXiTBLX_SEG_POINT);

			Partio::ParticlesData::const_iterator data_iter = data->begin();

			boost::ptr_vector<ParticleFeature>::const_iterator particleFeature_Iter = particleFeatures.cbegin();
			for (; particleFeature_Iter != particleFeatures.cend(); ++particleFeature_Iter)
			{	
				if (particleFeature_Iter->pacc)
				{
					data_iter.addAccessor(*particleFeature_Iter->pacc);
				}
			}

			for (; data_iter != data->end(); ++data_iter)
			{
				particleFeature_Iter = particleFeatures.cbegin();
				for (; particleFeature_Iter != particleFeatures.cend(); ++particleFeature_Iter)
				{
					if (particleFeature_Iter->offset >= 0)
					{
						if( particleFeature_Iter->pacc != NULL)
						{
							if (particleFeature_Iter->attr.type == Partio::FLOAT || particleFeature_Iter->attr.type == Partio::VECTOR)
							{
								float * featureData = particleFeature_Iter->pacc->raw<float, Partio::ParticlesData::const_iterator>(data_iter);
								if (fileType == ".icecache" && particleFeature_Iter->name == LXsTBLX_PARTICLE_XFRM)
								{
									float magnitude = sqrtf(featureData[0] * featureData[0] + featureData[1] * featureData[1] + featureData[2] * featureData[2] + featureData[3] * featureData[3]);
									for (int i=0; i < 4; ++i)
									{
										featureData[i] = featureData[i] / magnitude;
									}
									vrt_vec[particleFeature_Iter->offset + 0] = 1.0f - 2.0f * featureData[2] * featureData[2] - 2.0f * featureData[3] * featureData[3];	//	convert from icecache quaternion to rotation matrix
									vrt_vec[particleFeature_Iter->offset + 1] = 2.0f * featureData[1] * featureData[2] + 2.0f * featureData[3] * featureData[0];
									vrt_vec[particleFeature_Iter->offset + 2] = 2.0f * featureData[1] * featureData[3] - 2.0f * featureData[2] * featureData[0];
									vrt_vec[particleFeature_Iter->offset + 3] = 2.0f * featureData[1] * featureData[2] - 2.0f * featureData[3] * featureData[0];
									vrt_vec[particleFeature_Iter->offset + 4] = 1.0f - 2.0f * featureData[1] * featureData[1] - 2.0f * featureData[3] * featureData[3];
									vrt_vec[particleFeature_Iter->offset + 5] = 2.0f * featureData[2] * featureData[3] + 2.0f * featureData[1] * featureData[0];
									vrt_vec[particleFeature_Iter->offset + 6] = 2.0f * featureData[1] * featureData[3] + 2.0f * featureData[2] * featureData[0];
									vrt_vec[particleFeature_Iter->offset + 7] = 2.0f * featureData[2] * featureData[3] - 2.0f * featureData[1] * featureData[0];
									vrt_vec[particleFeature_Iter->offset + 8] = 1.0f - 2.0f * featureData[1] * featureData[1] - 2.0f * featureData[2] * featureData[2];
								}
								else
								{
									if (particleFeature_Iter->name == LXsTBLX_PARTICLE_XFRM && particleFeature_Iter->size != 9)
									{
										{
											vrt_vec[particleFeature_Iter->offset + 0] = 1.0f;		//	set identity rotation if we don't have the right number of matrix elements
											vrt_vec[particleFeature_Iter->offset + 4] = 1.0f;
											vrt_vec[particleFeature_Iter->offset + 8] = 1.0f;
										}
									}
									else
									{
										for (unsigned int i=0; i < particleFeature_Iter->size; ++i)
										{
											vrt_vec[particleFeature_Iter->offset + i] = featureData[i];
										}
									}
								}
							}
							else if (particleFeature_Iter->attr.type == Partio::INT)
							{
								int * featureData = particleFeature_Iter->pacc->raw<int, Partio::ParticlesData::const_iterator>(data_iter);
								for (unsigned int i=0; i < particleFeature_Iter->size; ++i)
								{
									vrt_vec[particleFeature_Iter->offset + i] = (float)featureData[i];
								}
							}
						}
						else		//	pacc == NULL so no imported data
						{
							if (particleFeature_Iter->name == LXsTBLX_PARTICLE_ID)		
							{
								vrt_vec[particleFeature_Iter->offset] = rand_seq.uniform ();	//	use random values for particle id
							}
							else if (particleFeature_Iter->name == LXsTBLX_PARTICLE_XFRM)
							{
								vrt_vec[particleFeature_Iter->offset + 0] = 1.0f;		//	set identity rotation
								vrt_vec[particleFeature_Iter->offset + 4] = 1.0f;
								vrt_vec[particleFeature_Iter->offset + 8] = 1.0f;
							}
						}
					}
				}

				LxResult rc;
				unsigned index;
				rc = tri_soup.Vertex  (vrt_vec, &index);
				if (LXx_FAIL (rc))
					throw (rc);

				rc = tri_soup.Polygon (index, 0, 0);
				if (LXx_FAIL (rc))
					throw (rc);
			}

			data->release();


        } catch (LxResult rc) 
		{
                result = rc;
        }

        delete [] vrt_vec;

        return result;
}



/*
 * Export package server to define a new item type.
 */
        void
initialize ()
{
        CModoPartioPackage   :: initialize ();
        CModoPartioGenerator :: initialize ();
        CModoPartioInstance  :: initialize ();
}


