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

#pragma once

#include <openpeer/stack/message/MessageRequest.h>
#include <openpeer/stack/message/peer-common/MessageFactoryPeerCommon.h>

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      namespace peer_common
      {
        class PeerPublishRequest : public MessageRequest
        {
        public:
          friend class PeerPublishResult;

          enum AttributeTypes
          {
            AttributeType_Publication,
          };

        public:

          static PeerPublishRequestPtr convert(MessagePtr message);

          static PeerPublishRequestPtr create();
          static PeerPublishRequestPtr create(
                                              ElementPtr root,
                                              IMessageSourcePtr messageSource
                                              );

          virtual DocumentPtr encode();

          virtual Methods method() const                  {return (Message::Methods)MessageFactoryPeerCommon::Method_PeerPublish;}

          virtual IMessageFactoryPtr factory() const      {return MessageFactoryPeerCommon::singleton();}

          bool hasAttribute(AttributeTypes type) const;

          IPublicationPtr publication() const             {return mPublication;}
          void publication(IPublicationPtr publication)   {mPublication = publication;}

          ULONG publishedFromVersion() const              {return mPublishedFromVersion;}
          void publishedFromVersion(ULONG value)          {mPublishedFromVersion = value;}

          ULONG publishedToVersion() const                {return mPublishedToVersion;}
          void publishedToVersion(ULONG value)            {mPublishedToVersion = value;}

        protected:
          PeerPublishRequest();

          IPublicationPtr mPublication;

          ULONG mPublishedFromVersion;
          ULONG mPublishedToVersion;
        };
      }
    }
  }
}
