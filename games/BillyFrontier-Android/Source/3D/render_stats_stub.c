// render_stats_stub.c
// Stub definitions for OttoMatic-derived render statistics counters.
// modern_gl.c and vertex_array_compat.c increment these; BillyFrontier
// doesn't use them.
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
int gDrawCallsThisFrame = 0;
int gVerticesThisFrame = 0;
int gBufferUploadsThisFrame = 0;
int gBufferUploadBytesThisFrame = 0;
int gCacheLookupsThisFrame = 0;
int gCacheHitsThisFrame = 0;
int gCacheMissesThisFrame = 0;
int gCacheEvictionsThisFrame = 0;
int gCacheInvalidationsThisFrame = 0;
int gIndexScansThisFrame = 0;
int gIndicesScannedThisFrame = 0;
#endif
