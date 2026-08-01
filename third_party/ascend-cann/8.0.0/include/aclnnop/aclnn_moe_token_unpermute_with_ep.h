
/*
 * calution: this file was generated automaticlly donot change it.
*/

#ifndef ACLNN_MOE_TOKEN_UNPERMUTE_WITH_EP_H_
#define ACLNN_MOE_TOKEN_UNPERMUTE_WITH_EP_H_

#include "aclnn/acl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* funtion: aclnnMoeTokenUnpermuteWithEpGetWorkspaceSize
 * parameters :
 * permutedTokens : required
 * sortedIndices : required
 * probsOptional : optional
 * numTopk : optional
 * rangeOptional : optional
 * paddedMode : optional
 * restoreShapeOptional : optional
 * out : required
 * workspaceSize : size of workspace(output).
 * executor : executor context(output).
 */
__attribute__((visibility("default")))
aclnnStatus aclnnMoeTokenUnpermuteWithEpGetWorkspaceSize(
    const aclTensor *permutedTokens,
    const aclTensor *sortedIndices,
    const aclTensor *probsOptional,
    int64_t numTopk,
    const aclIntArray *rangeOptional,
    bool paddedMode,
    const aclIntArray *restoreShapeOptional,
    const aclTensor *out,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

/* funtion: aclnnMoeTokenUnpermuteWithEp
 * parameters :
 * workspace : workspace memory addr(input).
 * workspaceSize : size of workspace(input).
 * executor : executor context(input).
 * stream : acl stream.
 */
__attribute__((visibility("default")))
aclnnStatus aclnnMoeTokenUnpermuteWithEp(
    void *workspace,
    uint64_t workspaceSize,
    aclOpExecutor *executor,
    aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif
