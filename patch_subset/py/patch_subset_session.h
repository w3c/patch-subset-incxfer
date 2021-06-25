#ifndef PATCH_SUBSET_PATCH_SUBSET_SESSION_H_
#define PATCH_SUBSET_PATCH_SUBSET_SESSION_H_

#include <memory>

class PatchSubsetSession;

extern "C" {

PatchSubsetSession* PatchSubsetSession_new(
    const char* font_directory, const char* font_id,
    bool with_codepoint_remapping, int32_t max_predicted_codepoints,
    float prediction_frequency_threshold);

bool PatchSubsetSession_extend(PatchSubsetSession* session,
                               uint32_t* codepoints, uint32_t codepoints_count);

void PatchSubsetSession_delete(PatchSubsetSession* session);

const char* PatchSubsetSession_get_font(PatchSubsetSession* session,
                                        uint32_t* size);
}

#endif  // PATCH_SUBSET_PATCH_SUBSET_SESSION_H_
