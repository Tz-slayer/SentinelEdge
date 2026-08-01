
/*
 * calution: this file was generated automaticlly donot change it.
*/

#ifndef ACLNN_UNFOLD_GRAD_H_
#define ACLNN_UNFOLD_GRAD_H_

#include "aclnn/acl_meta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* funtion: aclnnUnfoldGradGetWorkspaceSize
 * parameters :
 * gradOut : required
 * inputSizes : required
 * dim : required
 * size : required
 * step : required
 * out : required
 * workspaceSize : size of workspace(output).
 * executor : executor context(output).
 */
__attribute__((visibility("default")))
aclnnStatus aclnnUnfoldGradGetWorkspaceSize(
    const aclTensor *gradOut,
    const aclIntArray *inputSizes,
    int64_t dim,
    int64_t size,
    int64_t step,
    const aclTensor *out,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

__attribute__((visibility("default")))
aclnnStatus aclnnUnfoldGradTensorGetWorkspaceSize(
    const aclTensor *gradOut,
    const aclTensor *inputSizes,
    int64_t dim,
    int64_t size,
    int64_t step,
    const aclTensor *out,
    uint64_t *workspaceSize,
    aclOpExecutor **executor);

/* funtion: aclnnUnfoldGrad
 * parameters :
 * workspace : workspace memory addr(input).
 * workspaceSize : size of workspace(input).
 * executor : executor context(input).
 * stream : acl stream.
 */
__attribute__((visibility("default")))
aclnnStatus aclnnUnfoldGrad(
    void *workspace,
    uint64_t workspaceSize,
    aclOpExecutor *executor,
    aclrtStream stream);

#ifdef __cplusplus
}
#endif

#endif
