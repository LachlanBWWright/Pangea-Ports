// render_stats_stub.c
// Billy's GL compatibility layers increment the same render-stat counters that
// Otto/CroMag expose, and the debug overlay now reads them too.
int gDrawCallsThisFrame = 0;
int gVerticesThisFrame = 0;
int gBufferUploadsThisFrame = 0;
