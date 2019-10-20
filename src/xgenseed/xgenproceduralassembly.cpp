
//
// This source file is part of appleseed.
// Visit https://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2017-2019 Esteban Tovagliari, The appleseedhq Organization
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

// Build options header.
#include "foundation/core/buildoptions.h"

// appleseed.renderer headers.
#include "renderer/api/camera.h"
#include "renderer/api/frame.h"
#include "renderer/api/object.h"
#include "renderer/api/project.h"
#include "renderer/api/scene.h"
#include "renderer/api/utility.h"

// appleseed.foundation headers.
#include "foundation/math/matrix.h"
#include "foundation/math/scalar.h"
#include "foundation/math/vector.h"
#include "foundation/utility/api/specializedapiarrays.h"
#include "foundation/utility/containers/dictionary.h"
#include "foundation/utility/string.h"

// random numbers
#include "foundation/math/rng/distribution.h"
#include "foundation/math/rng/mersennetwister.h"

// appleseed.main headers.
#include "main/dllvisibility.h"

// XGen headers.
#if MAYA_API_VERSION >= 201900
    #include <xgen/src/xgrenderer/XgRenderAPI.h>
    #include <xgen/src/xgrenderer/XgRenderAPIUtils.h>
#else
    #include <XGen/XgRenderAPI.h>
    #include <XGen/XgRenderAPIUtils.h>
#endif

// Standard headers.
#include <cstring>
#include <memory>
#include <sstream>
#include <iostream>

namespace asf = foundation;
namespace asr = renderer;
using namespace XGenRenderAPI;
using namespace XGenRenderAPI::Utils;

namespace
{
    const char* Model = "xgen_patch_assembly";

    class XGenCallbacks
      : public ProceduralCallbacks
    {
      public:
        XGenCallbacks(
            const asr::Project& project,
            asr::Assembly&      assembly,
            asr::CurveObject&   curve_object)
          : m_assembly(assembly)
          , m_params(assembly.get_parameters())
          , m_curve_index(0)
          , m_curve_object(curve_object)
        {
            add_xgen_params(project);
            compute_transform_sequence(assembly);
        }

        void flush(const char* in_geom, PrimitiveCache* in_cache) override
        {
            // std::cerr << "FLUSH!!\n";
            if (in_cache->get(PrimitiveCache::PrimIsSpline))
            {
                flush_splines(in_geom, in_cache);
            }
            else
            {
                const char* primitive_type = in_cache->get(PrimitiveCache::PrimitiveType);

                if (strcmp(primitive_type, "CardPrimitive") == 0)
                    flush_cards(in_geom, in_cache);
                else if (strcmp(primitive_type, "SpherePrimitive") == 0)
                    flush_spheres(in_geom, in_cache);
                else if (strcmp(primitive_type, "ArchivePrimitive") ==0)
                    flush_archives(in_geom, in_cache);
                else
                {
                    RENDERER_LOG_ERROR(
                        "XGenCallbacks: unknown primitive type %s found", primitive_type);
                }
            }
        }

        void flush_splines(const char* in_geom, PrimitiveCache* in_cache)
        {
            // std::cerr << "flush_splines!!\n";
            RENDERER_LOG_DEBUG("XGenCallbacks: flush_splines called");

            unsigned int numSamples = in_cache->get(PrimitiveCache::NumMotionSamples);
            unsigned int shutterSize = in_cache->getSize(PrimitiveCache::Shutter);

            unsigned int cacheCount = in_cache->get(PrimitiveCache::CacheCount);

            // std::cerr << "numSamples: " << numSamples << "\n";
            // std::cerr << "shutterSize: " << shutterSize << "\n";
            // std::cerr << "cacheCount: " << cacheCount << "\n";

            const auto motion_segment_count = numSamples - 1;

            const size_t ControlPointCount = 4;

            unsigned int widthsSize = in_cache->getSize(PrimitiveCache::Widths);
            float constantWidth = in_cache->get(PrimitiveCache::ConstantWidth);

            unsigned int NumVertices_side = in_cache->getSize(PrimitiveCache::NumVertices);

            unsigned int numPointsTotal = in_cache->getSize2( PrimitiveCache::NumVertices, 0 );

            //
            // std::cerr <<  "widthsSize: "  << widthsSize << "\n";
            // std::cerr <<  "NumVertices_size: " <<  NumVertices_side << "\n";
            // std::cerr <<  "constantWidth: "  << widthsSize << "\n";
            // std::cerr <<  "numPointsTotal: " << numPointsTotal << "\n";
            if (constantWidth <= 0)
                constantWidth = .01;

            asf::MersenneTwister rng;



            for ( int i=0; i < numSamples; i++ ) {

                // auto_release_ptr<CurveObject> curve_object
                float *pos =(float *) in_cache->get(PrimitiveCache::Points, i);
                const int *NumVertices = in_cache->get(PrimitiveCache::NumVertices, i);
                float *cache_widths = (float*)in_cache->get(PrimitiveCache::Widths);
                // for (int j =0; j < numVerts-2; j += ControlPointCount) {

                for (int k = 0; k < cacheCount; k++) {
                    int vert_count = *NumVertices;
                    // std::cerr << "    Curve: " << k  <<  " vert_count: " <<  vert_count << "\n";
                    // float * pointPtr = pos;

                    float rand_value = rand1(rng, 0.0f, 1.0f);


                    for (int j = 0; j < vert_count-3; j++) {

                        float *pointPtr = pos + (j * 3);
                        float *w = cache_widths + j;

                        asr::GVector3 points[ControlPointCount];
                        asr::GScalar widths[ControlPointCount];
                        asr::GScalar opacities[ControlPointCount];
                        asf::Color3f colors[ControlPointCount];

                        for (int p = 0; p < ControlPointCount; p++) {


                            points[p] = asr::GVector3(pointPtr[0], pointPtr[1], pointPtr[2]);

                            opacities[p] = 1;
                            colors[p] = asf::Color3f(rand_value, rand_value, rand_value);

                            //Add phantom points at the end
                            if (j+p+1 < vert_count)
                                pointPtr += 3;

                            if (widthsSize > 0) {
                                float width = *w;
                                // if (width < 0) {
                                //     width = 0;
                                // }
                                widths[p] = width;

                                if (j + p > 0 && j+p+1 > vert_count - 1)
                                    w++;

                            } else {
                                widths[p] = constantWidth;
                            }
                        }
                        const asr::Curve3Type curve(&points[0], &widths[0], &opacities[0], &colors[0]);
                        m_curve_object.push_curve3(curve);
                    }

                    NumVertices++;
                    cache_widths += (widthsSize / cacheCount);
                    pos+= (3*vert_count);
                }
                break;
            }
        }

        void flush_cards(const char* in_geom, PrimitiveCache* in_cache)
        {
            RENDERER_LOG_DEBUG("XGenCallbacks: flush_cards called");
        }

        void flush_spheres(const char* in_geom, PrimitiveCache* in_cache)
        {
            RENDERER_LOG_DEBUG("XGenCallbacks: flush_spheres called");
        }

        void flush_archives(const char* in_geom, PrimitiveCache* in_cache)
        {
            RENDERER_LOG_DEBUG("XGenCallbacks: flush_archives called");
        }

        void log(const char* in_str)  override
        {
            RENDERER_LOG_INFO("XGen procedural assembly: %s", in_str);
        }

        bool get(EBoolAttribute attr) const override
        {
            switch (attr)
            {
                case ClearDescriptionCache:
                case DontUsePaletteRefCounting:
                    return false;
            }

            return false;
        }

        float get(EFloatAttribute attr) const override
        {
            switch (attr)
            {
                case ShadowMotionBlur:
                    return get_param("ShadowMotionBlur", 0.0f);

                case ShutterOffset:
                    return get_param("ShutterOffset", 0.0f);
            }

            return 0.0f;
        }

        const char* get(EStringAttribute attr) const override
        {
            switch (attr)
            {
                case BypassFXModulesAfterBGM:
                    return get_string("BypassFXModulesAfterBGM");

                case CacheDir:
                    return get_string("CacheDir", "xgenCache/");

                case Generator:
                    return get_string("Generator", "undefined");

                case Off:
                {
                    const asr::ParamArray& params = get_parameters();

                    if (params.strings().exist("Off"))
                    {
                        const char* str = params.get_path("Off");
                        if (stob(str))
                            return "xgen_OFF";
                    }

                    return "";
                }

                case Phase:
                    return get_string("Phase", "color");

                case RenderCam:
                    return get_string("irRenderCam");

                case RenderCamFOV:
                    return get_string("irRenderCamFOV");

                case RenderCamRatio:
                    return get_string("irRenderCamRatio");

                case RenderCamXform:
                    return get_string("irRenderCamXform");

                case RenderMethod:
                    return get_string("RenderMethod");
            }

            return "";
        }

        const float* get(EFloatArrayAttribute attr) const override
        {
            switch (attr)
            {
                case DensityFalloff:
                case LodHi:
                case LodLow:
                case LodMed:
                case Shutter:
                    return nullptr;
            }

            return nullptr;
        }

        unsigned int getSize(EFloatArrayAttribute attr) const override
        {
            switch (attr)
            {
                case DensityFalloff:
                case LodHi:
                case LodLow:
                case LodMed:
                case Shutter:
                    return 0;
            }

            return 0;
        }

        const char* getOverride(const char* name) const override
        {
            return get_string(name);
        }

        void getTransform(float in_time, mat44& out_mat) const override
        {
            RENDERER_LOG_DEBUG("XGenCallbacks: getTransform called");

            asf::Transformd scratch;
            const asf::Transformd& transform = m_transform_sequence.evaluate(in_time, scratch);
            const asf::Matrix4f matrix = transform.get_parent_to_local();

	        out_mat._00 = matrix(0, 0); out_mat._10 = matrix(0, 1); out_mat._20 = matrix(0, 2); out_mat._30 = matrix(0, 3);
	        out_mat._01 = matrix(1, 0); out_mat._11 = matrix(1, 1); out_mat._21 = matrix(1, 2); out_mat._31 = matrix(1, 3);
	        out_mat._02 = matrix(2, 0); out_mat._12 = matrix(2, 1); out_mat._22 = matrix(2, 2); out_mat._32 = matrix(2, 3);
	        out_mat._03 = matrix(3, 0); out_mat._13 = matrix(3, 1); out_mat._23 = matrix(3, 2); out_mat._33 = matrix(3, 3);
        }

        bool getArchiveBoundingBox(const char* in_filename, bbox& out_bbox) const override
        {
            RENDERER_LOG_DEBUG("XGenCallbacks: getArchiveBoundingBox called");
            return false;
        }

      private:
        asr::Assembly&          m_assembly;
        asr::ParamArray         m_params;
        asr::TransformSequence  m_transform_sequence;
        size_t                  m_curve_index;
        asr::CurveObject&       m_curve_object;

        const asr::ParamArray& get_parameters() const
        {
            return m_params;
        }

        const char* get_string(const char* key, const char* default_value = "") const
        {
            if (m_params.strings().exist(key))
                return m_params.get(key);

            return default_value;
        }

        template <typename T>
        T get_param(const char* key, const T& default_value) const
        {
            return m_params.get_optional(key, default_value);
        }

        void add_xgen_params(const asr::Project& project)
        {
            // Fetch the camera.
            const asr::Frame* frame = project.get_frame();
            const asr::Camera *camera = project.get_scene()->cameras().get_by_name(
                frame->get_active_camera_name());

            bool camera_is_persp = false;
            if (strcmp(camera->get_model(), "pinhole_camera") == 0)
                camera_is_persp = true;
            else if (strcmp(camera->get_model(), "thinlens_camera") == 0)
                camera_is_persp = true;

            // Get the camera transform.
            const asf::Transformd& transform =
                camera->transform_sequence().get_earliest_transform();

            if (!m_params.strings().exist("irRenderCam"))
            {
                asf::Vector3d camera_pos_or_dir;

                if (camera_is_persp)
                    camera_pos_or_dir = transform.get_local_to_parent().extract_translation();
                else
                    camera_pos_or_dir = transform.vector_to_parent(asf::Vector3d(0.0, 0.0, 1.0));

                m_params.insert_path(
                    "irRenderCam",
                    asf::format(
                        "{0}, {1}, {2}, {3}",
                        camera_is_persp ? "false" : "true",
                        camera_pos_or_dir.x,
                        camera_pos_or_dir.y,
                        camera_pos_or_dir.z));
            }

            if (!m_params.strings().exist("irRenderCamFOV"))
            {
                if (camera_is_persp)
                {
                    m_params.insert_path("irRenderCamFOV", "54.0");
                }
                else
                    m_params.insert_path("irRenderCamFOV", "90.0");
            }

            if (!m_params.strings().exist("irRenderCamRatio"))
            {
                m_params.insert_path("irRenderCamRatio", "1.0");
            }

            if (!m_params.strings().exist("irRenderCamXform"))
            {
                const asf::Matrix4d& m = transform.get_parent_to_local();
                std::stringstream ss;
                ss << m(0, 0) << "," << m(0, 1) << "," << m(0, 2) << "," << m(0, 3) << ",";
                ss << m(1, 0) << "," << m(1, 1) << "," << m(1, 2) << "," << m(1, 3) << ",";
                ss << m(2, 0) << "," << m(2, 1) << "," << m(2, 2) << "," << m(2, 3) << ",";
                ss << m(3, 0) << "," << m(3, 1) << "," << m(3, 2) << "," << m(3, 3);
                m_params.insert_path("irRenderCamXform", ss.str().c_str());
            }
        }

        void compute_transform_sequence(const asr::Assembly& assembly)
        {
            const asr::Assembly* parent_assembly =
                static_cast<const asr::Assembly*>(assembly.get_parent());

            const asr::AssemblyInstance* assembly_instance = nullptr;
            for (const auto& i : parent_assembly->assembly_instances())
            {
                if (strcmp(assembly.get_name(), i.get_assembly_name()) == 0)
                {
                    assembly_instance = &i;
                    break;
                }
            }

            assert(assembly_instance != nullptr);

            // Compose all the transformation sequences.
            int i = 0;
            while (assembly_instance)
            {
                auto t = assembly_instance->transform_sequence();

                m_transform_sequence = t * m_transform_sequence;

                assembly_instance = static_cast<const asr::AssemblyInstance*>(
                    assembly_instance->get_parent());

                // not sure why the second one segfaults;
                break;
            }
        }

    };

    class XGenPatchAssembly
      : public asr::ProceduralAssembly
    {
      public:
        XGenPatchAssembly(const char* name, const asr::ParamArray params)
          : asr::ProceduralAssembly(name, params)
        {
        }

        void release() override
        {
            delete this;
        }

        const char* get_model() const override
        {
            return Model;
        }
        bool do_expand_contents(
            const asr::Project&     project,
            const asr::Assembly*    parent,
            asf::IAbortSwitch*      abort_switch = nullptr) override
        {
            std::string xgen_args;

            try
            {
                xgen_args = get_parameters().get("xgen_args");
                std::cerr<< "xgen_args: "  << xgen_args << "\n";
            }
            catch (const asf::ExceptionDictionaryKeyNotFound&)
            {
                RENDERER_LOG_ERROR("XGen procedural error: missing xgen_args parameter");
                return false;
            }

            std::string obj_name =  asf::format("curve_{0}", get_name());

            auto curve_object = asf::auto_release_ptr<asr::CurveObject>(
                asr::CurveObjectFactory().create(
                    obj_name.c_str(),
                    asr::ParamArray()));

            curve_object->push_basis(asf::CurveBasis::BSpline);

            XGenCallbacks xgen_callbacks(project, *this, curve_object.ref());


            std::unique_ptr<PatchRenderer> patch_renderer(
                PatchRenderer::init(&xgen_callbacks, xgen_args.c_str()));

            if (!patch_renderer)
            {
                RENDERER_LOG_ERROR("Error creating XGen patch renderer");
                return false;
            }

            bbox face_bbox;
            unsigned int face_id;
            bool success = true;

            while (patch_renderer->nextFace(face_bbox, face_id))
            {
                if (isEmpty(face_bbox))
                    continue;

                std::unique_ptr<FaceRenderer> face_renderer(FaceRenderer::init(
                    patch_renderer.get(),
                    face_id,
                    &xgen_callbacks));

                if (face_renderer)
                {
                    success = success && face_renderer->render();
                }
                else
                {
                    RENDERER_LOG_ERROR("Error creating XGen face renderer");
                    success = false;
                }
            }

            objects().insert(
                asf::auto_release_ptr<asr::Object>(curve_object));



            asf::StringDictionary materials;
            materials.insert("default",   get_parameters()
                .get_optional<std::string>("material", "initialShadingGroup_material"));

            object_instances().insert(
                asr::ObjectInstanceFactory::create(
                    (obj_name+"_inst").c_str(), // Instance name.
                    asr::ParamArray(),
                    obj_name.c_str(), // Object name.
                    asf::Transformd::identity(),
                    materials));


            return success;
        }
    };

    class XGenPatchAssemblyFactory
      : public asr::IAssemblyFactory
    {
      public:
        void release() override
        {
            delete this;
        }

        const char* get_model() const override
        {
            return Model;
        }

        asf::Dictionary get_model_metadata() const override
        {
            return
                asf::Dictionary()
                    .insert("name", Model)
                    .insert("label", "XGen Patch Assembly");
        }

        asf::DictionaryArray get_input_metadata() const override
        {
            asf::DictionaryArray metadata;

            return metadata;
        }

        asf::auto_release_ptr<asr::Assembly> create(
            const char*             name,
            const asr::ParamArray&  params = asr::ParamArray()) const override
        {
            return asf::auto_release_ptr<asr::Assembly>(
                new XGenPatchAssembly(name, params));
        }
    };
}


//
// Plugin entry point.
//

extern "C"
{
    APPLESEED_DLL_EXPORT asr::IAssemblyFactory* appleseed_create_assembly_factory()
    {
        return new XGenPatchAssemblyFactory();
    }
}
