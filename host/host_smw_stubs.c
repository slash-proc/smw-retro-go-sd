/*
 * Symbols provided by upstream smw/src/main.c (desktop) but not linked in the
 * G&W / host builds. StateRecorder_StopReplay's body is #if'd/commented out in
 * common_rtl.c while RtlStopReplay still references it.
 */
#include <stddef.h>

int g_got_mismatch_count;

struct StateRecorder;
void StateRecorder_StopReplay(struct StateRecorder *sr)
{
    (void)sr;
}
