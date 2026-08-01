/* Copyright (c) 2024 Huawei Technologies Co., Ltd.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 1.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 * ===================================================================================================================*/

#ifndef INC_EXTERNAL_REGISTER_REGISTER_PASS_H_
#define INC_EXTERNAL_REGISTER_REGISTER_PASS_H_

#include <functional>
#include <memory>
#include <string>

#include "graph/graph.h"
#include "external/ge_common/ge_api_error_codes.h"
#include "register/register_types.h"

namespace ge {
class PassRegistrationDataImpl;
class CustomPassContext;
class CustomPassContextImpl;
using CustomPassFunc = std::function<Status(ge::GraphPtr &, CustomPassContext &)>;

class FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY PassRegistrationData {
 public:
  PassRegistrationData() = default;
  ~PassRegistrationData() = default;

  PassRegistrationData(std::string pass_name);

  PassRegistrationData &CustomPassFn(const CustomPassFunc &custom_pass_fn);

  std::string GetPassName() const;

  CustomPassFunc GetCustomPassFn() const;

 private:
  std::shared_ptr<PassRegistrationDataImpl> impl_;
};

class FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY PassReceiver {
 public:
  PassReceiver(PassRegistrationData &reg_data);
  ~PassReceiver() = default;
};

class CustomPassContext {
 public:
  CustomPassContext();
  virtual ~CustomPassContext() = default;

  void SetErrorMessage(const AscendString &error_message);
  AscendString GetErrorMessage() const;

 private:
  std::unique_ptr<CustomPassContextImpl> impl_;
};
}  // namespace ge

#define REGISTER_CUSTOM_PASS(name) REGISTER_CUSTOM_PASS_UNIQ_HELPER(__COUNTER__, (name))
#define REGISTER_CUSTOM_PASS_UNIQ_HELPER(ctr, name) REGISTER_CUSTOM_PASS_UNIQ(ctr, (name))
#define REGISTER_CUSTOM_PASS_UNIQ(ctr, name)        \
  static ::ge::PassReceiver register_pass##ctr      \
      __attribute__((unused)) =                     \
          ::ge::PassRegistrationData((name))

#endif  // INC_EXTERNAL_REGISTER_REGISTER_PASS_H_
