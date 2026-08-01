
/*
 * calution: this file was generated automaticlly donot change it.
*/

#ifndef ACLNN_REPEAT_INTERLEAVE_GRAD_H_
#define ACLNN_REPEAT_INTERLEAVE_GRAD_H_

#include "aclnn/acl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* funtion: aclnnRepeatInterleaveGradGetWorkspaceSize
 * parameters :
 * yGrad : required
 * repeats : required
 * axis : optional
 * out : required
 * workspaceSize : size of workspace(output).
 * executor : executor context(output).
 */
__attribute__((visibility("default")))
aclnnStatus aclnnRepeatInterleaveGradGetWorkspaceSize(
    const aclTensor *yGrad,
    const aclTensor *repeats,
    int64_t axis,
    const aclTensor *out,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

/* funtion: aclnnRepeatInterleaveGrad
 * parameters :
 * workspace : workspace memory addr(input).
 * workspaceSize : size of workspace(input).
 * executor : executor context(input).
 * stream : acl stream.
 */
__attribute__((visibility("default")))
aclnnStatus aclnnRepeatInterleaveGrad(
    void *workspace,
    uint64_t workspaceSize,
    aclOpExecutor *executor,
    aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif
