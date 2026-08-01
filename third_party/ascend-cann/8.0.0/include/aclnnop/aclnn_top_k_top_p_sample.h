
/*
 * calution: this file was generated automaticlly donot change it.
*/

#ifndef ACLNN_TOP_KTOP_PSAMPLE_H_
#define ACLNN_TOP_KTOP_PSAMPLE_H_

#include "aclnn/acl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* funtion: aclnnTopKTopPSampleGetWorkspaceSize
 * parameters :
 * logits : required
 * topK : required
 * topP : required
 * qOptional : optional
 * eps : optional
 * isNeedLogits : optional
 * topKGuess : optional
 * logitsSelectIdxOut : required
 * logitsTopKpSelectOut : required
 * workspaceSize : size of workspace(output).
 * executor : executor context(output).
 */
__attribute__((visibility("default")))
aclnnStatus aclnnTopKTopPSampleGetWorkspaceSize(
    const aclTensor *logits,
    const aclTensor *topK,
    const aclTensor *topP,
    const aclTensor *qOptional,
    double eps,
    bool isNeedLogits,
    int64_t topKGuess,
    const aclTensor *logitsSelectIdxOut,
    const aclTensor *logitsTopKpSelectOut,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

/* funtion: aclnnTopKTopPSample
 * parameters :
 * workspace : workspace memory addr(input).
 * workspaceSize : size of workspace(input).
 * executor : executor context(input).
 * stream : acl stream.
 */
__attribute__((visibility("default")))
aclnnStatus aclnnTopKTopPSample(
    void *workspace,
    uint64_t workspaceSize,
    aclOpExecutor *executor,
    aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif
