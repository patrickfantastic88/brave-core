/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/extensions/api/brave_shields_api.h"

#include "brave/common/extensions/api/brave_shields.h"
#include "brave/content/common/frame_messages.h"
#include "content/public/browser/web_contents.h"

namespace extensions {
namespace api {

BraveShieldsAllowScriptsOnceFunction::~BraveShieldsAllowScriptsOnceFunction() {
}

ExtensionFunction::ResponseAction BraveShieldsAllowScriptsOnceFunction::Run() {
  std::unique_ptr<brave_shields::AllowScriptsOnce::Params> params(
      brave_shields::AllowScriptsOnce::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Send an IPC msg to update the list in this tab's content settings observer
  GetSenderWebContents()->SendToAllFrames(new BraveFrameMsg_AllowScriptsOnce(
        MSG_ROUTING_NONE, params->origins));

  return RespondNow(NoArguments());
}

}  // namespace api
}  // namespace extensions
