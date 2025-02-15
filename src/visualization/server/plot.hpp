#ifndef __TC_PLOT_HPP_
#define __TC_PLOT_HPP_

#include <model_server/lib/toolkit_class_macros.hpp>
#include <model_server/lib/toolkit_function_macros.hpp>
#include <visualization/server/transformation.hpp>
#include <model_server/lib/extensions/model_base.hpp>
#include <string>

#ifdef __APPLE__
#ifndef TC_BUILD_IOS
extern "C" {
#include <CoreGraphics/CoreGraphics.h>
}
#endif // TC_BUILD_IOS
#endif // __APPLE__
       //
#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#ifndef NS_OPTIONS
#define NS_OPTIONS(_type, _name) enum _name : _type _name; enum _name : _type
#endif // NS_OPTIONS

#ifdef __cplusplus
// In C++, NS_OPTIONS doesn't seem to be valid syntax; skip it and just use a C++ enum.
#define MAKE_NS_OPTIONS(_type, _name) enum _name : _type
#else
#ifdef __OBJC__
// In ObjC, define MAKE_NS_OPTIONS in terms of NS_OPTIONS
#define MAKE_NS_OPTIONS(_type, _name) typedef NS_OPTIONS(_type, _name)
#else
#define MAKE_NS_OPTIONS(_type, _name) typedef enum _name##_enum _name; enum _name##_enum
#endif
#endif // __cplusplus



namespace turi {
  namespace visualization {

    const char * const tc_plot_title_default_label = "__TURI_DEFAULT_LABEL";
    enum tc_plot_variation: uint64_t {
        tc_plot_variation_default = 0x00,

        // Sizes (defaults to medium)
        tc_plot_size_small = 0x01,
        tc_plot_size_medium = 0x02,
        tc_plot_size_large = 0x03,

        // Color variations
        // default could be light/dark depending on OS settings
        tc_plot_color_light = 0x10,
        tc_plot_color_dark = 0x20,

    };

    class Plot: public model_base {
      private:
        mutable std::string m_id;
        std::string m_vega_spec;
        double m_size_array;
        std::shared_ptr<transformation_base> m_transformer;

      public:
        Plot();
        explicit Plot(const std::string& vega_spec);
        Plot(const std::string& vega_spec, std::shared_ptr<transformation_base> transformer, double size_array);
        void show(const std::string& path_to_client, tc_plot_variation variation = tc_plot_variation_default);
        void materialize() const;
        std::string get_url() const;
        const std::string& get_id() const;

        // vega specification
        std::string get_spec(tc_plot_variation variation=tc_plot_variation_default,
                             bool include_data=false) const;

        // streaming data aggregation
        double get_percent_complete() const; // out of 1.0
        bool finished_streaming() const;
        std::string get_next_data() const;

        // non-streaming data aggregation: causes full materialization
        std::string get_data() const;

#ifdef __APPLE__
#ifndef TC_BUILD_IOS
        // Streaming render (based on current computation -
        // call materialize first to get a final rendering up front).
        // Returns true if streaming finished, false otherwise.
        bool render(CGContextRef context, tc_plot_variation variation=tc_plot_variation_default);

        // Low-level rendering path - takes a Vega spec as input.
        static void render(const std::string& vega_spec, CGContextRef context);
#endif // TC_BUILD_IOS
#endif // __APPLE__

        BEGIN_CLASS_MEMBER_REGISTRATION("_Plot")
        REGISTER_CLASS_MEMBER_FUNCTION(Plot::show, "path_to_client", "variation")
        REGISTER_CLASS_MEMBER_FUNCTION(Plot::materialize)
        REGISTER_CLASS_MEMBER_FUNCTION(Plot::get_spec, "variation", "include_data")
        REGISTER_CLASS_MEMBER_FUNCTION(Plot::get_data)
        REGISTER_CLASS_MEMBER_FUNCTION(Plot::get_url)
        END_CLASS_MEMBER_REGISTRATION
    };

    std::shared_ptr<Plot> plot_from_vega_spec(const std::string& vega_spec);
  }
}

#endif
