/*

 Copyright (c) 2013, SMB Phone Inc.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 The views and conclusions contained in the software and documentation are those
 of the authors and should not be interpreted as representing official policies,
 either expressed or implied, of the FreeBSD Project.

 */

#include <openpeer/stack/message/peer/PeerServicesGetRequest.h>
#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>
#include <openpeer/stack/IPeerFiles.h>
#include <openpeer/stack/IPeerFilePrivate.h>
#include <openpeer/stack/IPeerFilePublic.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/XML.h>
#include <zsLib/helpers.h>

#define OPENPEER_STACK_MESSAGE_PEER_SERVICES_GET_REQUEST_EXPIRES_TIME_IN_SECONDS ((60*60)*24)

namespace openpeer { namespace stack { namespace message { ZS_DECLARE_SUBSYSTEM(openpeer_stack_message) } } }

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      using services::IHelper;

      namespace peer
      {
        using zsLib::Seconds;
        using internal::MessageHelper;

        //---------------------------------------------------------------------
        PeerServicesGetRequestPtr PeerServicesGetRequest::convert(MessagePtr message)
        {
          return boost::dynamic_pointer_cast<PeerServicesGetRequest>(message);
        }

        //---------------------------------------------------------------------
        PeerServicesGetRequest::PeerServicesGetRequest()
        {
        }

        //---------------------------------------------------------------------
        PeerServicesGetRequestPtr PeerServicesGetRequest::create()
        {
          PeerServicesGetRequestPtr ret(new PeerServicesGetRequest);
          return ret;
        }

        //---------------------------------------------------------------------
        bool PeerServicesGetRequest::hasAttribute(AttributeTypes type) const
        {
          switch (type)
          {
            case AttributeType_LockboxInfo:   return mLockboxInfo.hasData();
            default:                          break;
          }
          return false;
        }

        //---------------------------------------------------------------------
        DocumentPtr PeerServicesGetRequest::encode()
        {
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(*this);
          ElementPtr root = ret->getFirstChildElement();

          String clientNonce = IHelper::randomString(32);

          LockboxInfo lockboxInfo;

          lockboxInfo.mAccessToken = mLockboxInfo.mAccessToken;
          if (mLockboxInfo.mAccessSecret.hasData()) {
            lockboxInfo.mAccessSecretProofExpires = zsLib::now() + Seconds(OPENPEER_STACK_MESSAGE_PEER_SERVICES_GET_REQUEST_EXPIRES_TIME_IN_SECONDS);
            lockboxInfo.mAccessSecretProof = IHelper::convertToHex(*IHelper::hmac(*IHelper::hmacKeyFromPassphrase(mLockboxInfo.mAccessSecret), "lockbox-access-validate:" + clientNonce + ":" + IHelper::timeToString(lockboxInfo.mAccessSecretProofExpires) + ":" + lockboxInfo.mAccessToken + ":peer-services-get"));
          }

          root->adoptAsLastChild(IMessageHelper::createElementWithText("nonce", clientNonce));
          if (lockboxInfo.hasData()) {
            root->adoptAsLastChild(MessageHelper::createElement(lockboxInfo));
          }
          
          return ret;
        }
      }
    }
  }
}
