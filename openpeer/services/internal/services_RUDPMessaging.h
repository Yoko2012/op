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

#include <openpeer/services/internal/types.h>

#include <openpeer/services/IRUDPMessaging.h>
#include <openpeer/services/IRUDPChannel.h>
#include <openpeer/services/ITransportStream.h>

#include <boost/shared_array.hpp>

namespace openpeer
{
  namespace services
  {
    namespace internal
    {
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark RUDPMessaging
      #pragma mark

      class RUDPMessaging : public Noop,
                            public MessageQueueAssociator,
                            public IRUDPMessaging,
                            public IRUDPChannelDelegate,
                            public ITransportStreamWriterDelegate,
                            public ITransportStreamReaderDelegate
      {
      public:
        friend interaction IRUDPMessagingFactory;
        friend interaction IRUDPMessaging;
        
      protected:
        RUDPMessaging(
                     IMessageQueuePtr queue,
                     IRUDPMessagingDelegatePtr delegate,
                      ITransportStreamPtr receiveStream,
                      ITransportStreamPtr sendStream,
                      ULONG maxMessageSizeInBytes
                     );
        
        RUDPMessaging(Noop) : Noop(true), MessageQueueAssociator(IMessageQueuePtr()) {};

        void init();

      public:
        ~RUDPMessaging();

        static RUDPMessagingPtr convert(IRUDPMessagingPtr socket);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark RUDPMessaging => IRUDPMessaging
        #pragma mark

        static String toDebugString(IRUDPMessagingPtr messaging, bool includeCommaPrefix = true);

        static RUDPMessagingPtr acceptChannel(
                                              IMessageQueuePtr queue,
                                              IRUDPListenerPtr listener,
                                              IRUDPMessagingDelegatePtr delegate,
                                              ITransportStreamPtr receiveStream,
                                              ITransportStreamPtr sendStream,
                                              ULONG maxMessageSizeInBytes
                                              );

        static RUDPMessagingPtr acceptChannel(
                                              IMessageQueuePtr queue,
                                              IRUDPICESocketSessionPtr session,
                                              IRUDPMessagingDelegatePtr delegate,
                                              ITransportStreamPtr receiveStream,
                                              ITransportStreamPtr sendStream,
                                              ULONG maxMessageSizeInBytes
                                              );

        static RUDPMessagingPtr openChannel(
                                            IMessageQueuePtr queue,
                                            IRUDPICESocketSessionPtr session,
                                            IRUDPMessagingDelegatePtr delegate,
                                            const char *connectionInfo,
                                            ITransportStreamPtr receiveStream,
                                            ITransportStreamPtr sendStream,
                                            ULONG maxMessageSizeInBytes
                                            );

        virtual PUID getID() const {return mID;}

        virtual RUDPMessagingStates getState(
                                             WORD *outLastErrorCode = NULL,
                                             String *outLastErrorReason = NULL
                                             ) const;

        virtual void shutdown();

        virtual void shutdownDirection(Shutdown state);

        virtual void setMaxMessageSizeInBytes(ULONG maxMessageSizeInBytes);

        virtual IPAddress getConnectedRemoteIP();

        virtual String getRemoteConnectionInfo();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark RUDPMessaging => IRUDPChannelDelegate
        #pragma mark

        virtual void onRDUPChannelStateChanged(
                                               IRUDPChannelPtr session,
                                               RUDPChannelStates state
                                               );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark RUDPMessaging => ITransportStreamWriterDelegate
        #pragma mark

        virtual void onTransportStreamWriterReady(ITransportStreamWriterPtr reader);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark RUDPMessaging => ITransportStreamReaderDelegate
        #pragma mark

        virtual void onTransportStreamReaderReady(ITransportStreamReaderPtr reader);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark RUDPMessaging => (internal)
        #pragma mark

        bool isShuttingDown() const {return RUDPMessagingState_ShuttingDown == mCurrentState;}
        bool isShutdown() const {return RUDPMessagingState_Shutdown == mCurrentState;}

        String log(const char *message) const;

        virtual String getDebugValueString(bool includeCommaPrefix = true) const;

        void step();
        bool stepSendData();
        bool stepReceiveData();

        void cancel();
        void setState(RUDPMessagingStates state);
        void setError(WORD errorCode, const char *inReason = NULL);

        IRUDPChannelPtr getChannel() const;

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark RUDPMessaging => (data)
        #pragma mark

        AutoPUID mID;
        mutable RecursiveLock mLock;
        RUDPMessagingWeakPtr mThisWeak;

        RUDPMessagingStates mCurrentState;
        AutoWORD mLastError;
        String mLastErrorReason;

        IRUDPMessagingDelegatePtr mDelegate;

        ITransportStreamWriterPtr mOuterReceiveStream;
        ITransportStreamReaderPtr mOuterSendStream;

        ITransportStreamReaderPtr mWireReceiveStream;
        ITransportStreamWriterPtr mWireSendStream;

        ITransportStreamWriterSubscriptionPtr mOuterReceiveStreamSubscription;
        ITransportStreamReaderSubscriptionPtr mOuterSendStreamSubscription;

        ITransportStreamReaderSubscriptionPtr mWireReceiveStreamSubscription;
        ITransportStreamWriterSubscriptionPtr mWireSendStreamSubscription;

        AutoBool mInformedOuterReceiveReady;
        AutoBool mInformedWireSendReady;

        RUDPMessagingPtr mGracefulShutdownReference;

        IRUDPChannelPtr mChannel;

        AutoDWORD mNextMessageSizeInBytes;

        ULONG mMaxMessageSizeInBytes;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IRUDPMessagingFactory
      #pragma mark

      interaction IRUDPMessagingFactory
      {
        static IRUDPMessagingFactory &singleton();

        virtual RUDPMessagingPtr acceptChannel(
                                               IMessageQueuePtr queue,
                                               IRUDPListenerPtr listener,
                                               IRUDPMessagingDelegatePtr delegate,
                                               ITransportStreamPtr receiveStream,
                                               ITransportStreamPtr sendStream,
                                               ULONG maxMessageSizeInBytes
                                               );

        virtual RUDPMessagingPtr acceptChannel(
                                               IMessageQueuePtr queue,
                                               IRUDPICESocketSessionPtr session,
                                               IRUDPMessagingDelegatePtr delegate,
                                               ITransportStreamPtr receiveStream,
                                               ITransportStreamPtr sendStream,
                                               ULONG maxMessageSizeInBytes
                                               );

        virtual RUDPMessagingPtr openChannel(
                                             IMessageQueuePtr queue,
                                             IRUDPICESocketSessionPtr session,
                                             IRUDPMessagingDelegatePtr delegate,
                                             const char *connectionInfo,
                                             ITransportStreamPtr receiveStream,
                                             ITransportStreamPtr sendStream,
                                             ULONG maxMessageSizeInBytes
                                             );
      };
      
    }
  }
}
