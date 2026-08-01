
/*
 * calution: this file was generated automaticlly donot change it.
*/

#ifndef ACLNN_ROTARY_POSITION_EMBEDDING_GRAD_H_
#define ACLNN_ROTARY_POSITION_EMBEDDING_GRAD_H_

#include "aclnn/acl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* funtion: aclnnRotaryPositionEmbeddingGradGetWorkspaceSize
 * parameters :
 * dy : required
 * cos : required
 * sin : required
 * xOptional : optional
 * mode : optional
 * dxOut : required
 * dcosOut : required
 * dsinOut : required
 * workspaceSize : size of workspace(output).
 * executor : executor context(output).
 */
__attribute__((visibility("default")))
aclnnStatus aclnnRotaryPositionEmbeddingGradGetWorkspaceSize(
    const aclTensor *dy,
    const aclTensor *cos,
    const aclTensor *sin,
    const aclTensor *xOptional,
    int64_t mode,
    const aclTensor *dxOut,
    const aclTensor *dcosOut,
    const aclTensor *dsinOut,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

/* funtion: aclnnRotaryPositionEmbeddingGrad
 * parameters :
 * workspace : workspace memory addr(input).
 * workspaceSize : size of workspace(input).
 * executor : executor context(input).
 * stream : acl stream.
 */
__attribute__((visibility("default")))
aclnnStatus aclnnRotaryPositionEmbeddingGrad(
    void *workspace,
    uint64_t workspaceSize,
    aclOpExecutor *executor,
    aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif
