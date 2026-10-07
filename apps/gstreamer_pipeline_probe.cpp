#include <gst/gst.h>

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    gst_init(&argc, &argv);
    const std::string pipeline_description = argc > 1
        ? argv[1]
        : "videotestsrc num-buffers=3 ! video/x-raw,framerate=30/1 ! fakesink sync=false";

    GError* error = nullptr;
    GstElement* pipeline = gst_parse_launch(pipeline_description.c_str(), &error);
    if (pipeline == nullptr) {
        std::cerr << "GStreamer pipeline parse failed: " << (error ? error->message : "unknown error") << '\n';
        if (error) g_error_free(error);
        return EXIT_FAILURE;
    }
    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "GStreamer pipeline could not enter PLAYING state\n";
        gst_object_unref(pipeline);
        return EXIT_FAILURE;
    }

    GstBus* bus = gst_element_get_bus(pipeline);
    GstMessage* message = gst_bus_timed_pop_filtered(
        bus, 5 * GST_SECOND, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
    bool success = message != nullptr && GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS;
    if (!success) std::cerr << "GStreamer pipeline did not reach EOS successfully\n";

    if (message) gst_message_unref(message);
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    if (error) g_error_free(error);
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
