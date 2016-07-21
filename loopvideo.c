#include <gst/gst.h>

/*!
 * Based on
 * http://tristanswork.blogspot.ch/2010/10/looping-playback-with-gstreamer.html
 * and on
 * https://gstreamer.freedesktop.org/data/doc/gstreamer/head/manual/html/chapter-helloworld.html
 */

gboolean bus_callback(GstBus *bus, GstMessage *msg, gpointer data)
{
    GstElement *play = GST_ELEMENT(data);
    switch (GST_MESSAGE_TYPE(msg))
    {
    case GST_MESSAGE_EOS:
        /* restart playback if at end */
        if (!gst_element_seek(play,
                              1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
                              GST_SEEK_TYPE_SET, 0,
                              GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
            g_print("Seek failed!\n");
        }
        break;
    default:
        break;
    }
    return TRUE;
}

static void
on_pad_added (GstElement *element, GstPad *pad, gpointer data)
{
    GstPad *sinkpad;
    GstElement *decoder = (GstElement *) data;

    g_print ("Dynamic pad created, linking demuxer/decoder\n");

    sinkpad = gst_element_get_static_pad (decoder, "sink");

    gst_pad_link (pad, sinkpad);

    gst_object_unref (sinkpad);
}

gint
main (gint   argc,
      gchar *argv[])
{
    GMainLoop *loop;
    GstBus *bus;

    /* init GStreamer */
    gst_init (&argc, &argv);
    loop = g_main_loop_new (NULL, FALSE);

    /* make sure we have a URI */
    if (argc != 2) {
        g_print ("Usage: %s <URI>\n", argv[0]);
        return -1;
    }

    /* build */
    GstElement *pipeline;
    pipeline = gst_pipeline_new ("my-pipeline");

    /* gst-launch-0.10 filesrc location=data/videoCircle.mp4 ! qtdemux
    ! ffdec_h264 ! ffmpegcolorspace ! deinterlace ! v4l2sink device=/dev/video1 */

    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    guint bus_watch_id;
    bus_watch_id = gst_bus_add_watch (bus, bus_callback, pipeline);
    gst_object_unref (bus);

    GstElement *source;
    source = gst_element_factory_make ("filesrc", "src");
    if (source == NULL)
        g_error ("Could not create 'filesrc' element");
    g_object_set (source, "location", argv[1], NULL);

    GstElement *demuxer;
    demuxer = gst_element_factory_make ("qtdemux", "dm");
    if (demuxer == NULL)
        g_error ("Could not create 'qtdemux' element");

    GstElement *decoder;
    decoder = gst_element_factory_make ("ffdec_h264", "dec");
    if (decoder == NULL)
        g_error ("Could not create 'ffdec_h264' element");

    GstElement *clr;
    clr = gst_element_factory_make ("ffmpegcolorspace", "clr");
    if (clr == NULL)
        g_error ("Could not create 'ffmpegcolorspace' element");

    GstElement *dil;
    dil = gst_element_factory_make ("deinterlace", "dil");
    if (dil == NULL)
        g_error ("Could not create 'deinterlace' element");

    GstElement *sink;
    sink = gst_element_factory_make ("v4l2sink", "sink");
    if (sink == NULL)
        g_error ("Could not create 'v4l2sink' element");
    g_object_set (sink, "device", "/dev/video1", NULL);

    gst_bin_add_many (GST_BIN (pipeline), source, demuxer, decoder, clr, dil, sink, NULL);

    /* we link the elements together */
    gst_element_link (source, demuxer);
    gst_element_link_many (decoder, clr, dil, sink, NULL);
    g_signal_connect (demuxer, "pad-added", G_CALLBACK (on_pad_added), decoder);
    /* note that the demuxer will be linked to the decoder dynamically.The reason is that Ogg may contain various streams (for example
       audio and video). The source pad(s) will be created at run time, by the demuxer when it detects the amount and nature of streams.
       Therefore we connect a callback function which will be executed when the "pad-added" is emitted.*/

    /* Set the pipeline to "playing" state*/
    g_print ("Now playing: %s\n", argv[1]);
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    /* now run */
    g_print ("Running...\n");
    g_main_loop_run (loop);

    /* Out of the main loop, clean up nicely */
    g_print ("Returned, stopping playback\n");
    gst_element_set_state (pipeline, GST_STATE_NULL);

    g_print ("Deleting pipeline\n");
    gst_object_unref (GST_OBJECT (pipeline));
    g_source_remove (bus_watch_id);
    g_main_loop_unref (loop);

    return 0;
}
