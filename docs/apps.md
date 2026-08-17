Apps {#apps}
====

Unlike the tutorials, an app (`apps/`) is a live client/server program -- it keeps
running until stopped, so there's no "captured output" to embed into a static page the way
a demo's is. Each app page below documents what it does, why it's useful, and how to build
and run it (see `docs/generate_app_doc.py`) -- not its full source, which is a click away in
the file it names.

- \subpage live_video_stream_app -- ring_buffer.h/viewport.h/net/websocket_server.h/net/json.h: streaming a local video file (or, eventually, any real-time sensor) into a browser over a hand-rolled WebSocket, where the CLIENT's current view (crop region, resolution, window/level) drives what the server actually renders -- the same "client's view is a request" model Google Earth/Neuroglancer use, applied to a live feed.
- \subpage bouncing_donut_app -- ring_buffer.h/viewport.h/net/websocket_server.h/net/json.h: the same viewport-driven live-streaming model as live_video_stream, but with no source file at all -- a torus signed-distance field evaluated fresh into a RingBufferImage every tick, streamed through renderVolume() (viewer/viewport.h)'s CPU ray-marcher, the server-side counterpart to the static viewer's own WebGL volume shaders.
