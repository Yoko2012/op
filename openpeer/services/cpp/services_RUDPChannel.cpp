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

#include <openpeer/services/internal/services_RUDPChannel.h>
#include <openpeer/services/internal/services_ICESocket.h>
#include <openpeer/services/internal/services_IRUDPChannelStream.h>
#include <openpeer/services/internal/services_Helper.h>

#include <openpeer/services/RUDPPacket.h>

#include <zsLib/Exception.h>
#include <zsLib/helpers.h>
#include <zsLib/Stringize.h>

#include <algorithm>

#define OPENPEER_SERVICES_RUDPCHANNEL_DEFAULT_LIFETIME_IN_SECONDS (10*60)

namespace openpeer { namespace services { ZS_DECLARE_SUBSYSTEM(openpeer_services) } }


namespace openpeer
{
  namespace services
  {
    namespace internal
    {
      typedef zsLib::ITimerDelegateProxy ITimerDelegateProxy;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      static String sequenceToString(QWORD value)
      {
        return string(value) + " (" + string(value & 0xFFFFFF) + ")";
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IRUDPChannelForRUDPICESocketSession
      #pragma mark

      //-----------------------------------------------------------------------
      RUDPChannelPtr IRUDPChannelForRUDPICESocketSession::createForRUDPICESocketSessionIncoming(
                                                                                                IMessageQueuePtr queue,
                                                                                                IRUDPChannelDelegateForSessionAndListenerPtr master,
                                                                                                const IPAddress &remoteIP,
                                                                                                WORD incomingChannelNumber,
                                                                                                const char *localUsernameFrag,
                                                                                                const char *localPassword,
                                                                                                const char *remoteUsernameFrag,
                                                                                                const char *remotePassword,
                                                                                                STUNPacketPtr channelOpenPacket,
                                                                                                STUNPacketPtr &outResponse
                                                                                                )
      {
        return IRUDPChannelFactory::singleton().createForRUDPICESocketSessionIncoming(queue, master, remoteIP, incomingChannelNumber, localUsernameFrag, localPassword, remoteUsernameFrag, remotePassword, channelOpenPacket, outResponse);
      }

      //-----------------------------------------------------------------------
      RUDPChannelPtr IRUDPChannelForRUDPICESocketSession::createForRUDPICESocketSessionOutgoing(
                                                                                                IMessageQueuePtr queue,
                                                                                                IRUDPChannelDelegateForSessionAndListenerPtr master,
                                                                                                IRUDPChannelDelegatePtr delegate,
                                                                                                const IPAddress &remoteIP,
                                                                                                WORD incomingChannelNumber,
                                                                                                const char *localUsernameFrag,
                                                                                                const char *localPassword,
                                                                                                const char *remoteUsernameFrag,
                                                                                                const char *remotePassword,
                                                                                                const char *connectionInfo,
                                                                                                ITransportStreamPtr receiveStream,
                                                                                                ITransportStreamPtr sendStream
                                                                                                )
      {
        return IRUDPChannelFactory::singleton().createForRUDPICESocketSessionOutgoing(queue, master, delegate, remoteIP, incomingChannelNumber, localUsernameFrag, localPassword, remoteUsernameFrag, remotePassword, connectionInfo, receiveStream, sendStream);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IRUDPChannelForRUDPListener
      #pragma mark

      RUDPChannelPtr IRUDPChannelForRUDPListener::createForListener(
                                                                    IMessageQueuePtr queue,
                                                                    IRUDPChannelDelegateForSessionAndListenerPtr master,
                                                                    const IPAddress &remoteIP,
                                                                    WORD incomingChannelNumber,
                                                                    STUNPacketPtr channelOpenPacket,
                                                                    STUNPacketPtr &outResponse
                                                                    )
      {
        return IRUDPChannelFactory::singleton().createForListener(queue, master, remoteIP, incomingChannelNumber, channelOpenPacket, outResponse);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark RUDPChannel
      #pragma mark

      //-----------------------------------------------------------------------
      RUDPChannel::RUDPChannel(
                               IMessageQueuePtr queue,
                               IRUDPChannelDelegateForSessionAndListenerPtr master,
                               const IPAddress &remoteIP,
                               const char *localUsernameFrag,
                               const char *localPassword,
                               const char *remoteUsernameFrag,
                               const char *remotePassword,
                               DWORD minimumRTT,
                               DWORD lifetime,
                               WORD incomingChannelNumber,
                               QWORD localSequenceNumber,
                               const char *localChannelInfo,
                               WORD outgoingChannelNumber,
                               QWORD remoteSequenceNumber,
                               const char *remoteChannelInfo
                               ) :
        MessageQueueAssociator(queue),
        mCurrentState(RUDPChannelState_Connecting),
        mMasterDelegate(IRUDPChannelDelegateForSessionAndListenerProxy::createWeak(queue, master)),
        mRemoteIP(remoteIP),
        mShutdownDirection(IRUDPChannel::Shutdown_None),
        mLocalUsernameFrag(localUsernameFrag),
        mLocalPassword(localPassword),
        mRemoteUsernameFrag(remoteUsernameFrag),
        mRemotePassword(remotePassword),
        mIncomingChannelNumber(incomingChannelNumber),
        mOutgoingChannelNumber(outgoingChannelNumber),
        mLocalSequenceNumber(localSequenceNumber),
        mRemoteSequenceNumber(remoteSequenceNumber),
        mMinimumRTT(minimumRTT),
        mLifetime(lifetime),
        mLocalChannelInfo(localChannelInfo ? localChannelInfo : ""),
        mRemoteChannelInfo(remoteChannelInfo ? remoteChannelInfo : ""),
        mLastSentData(zsLib::now()),
        mLastReceivedData(zsLib::now())
      {
        ZS_LOG_BASIC(log("created"))
      }

      //-----------------------------------------------------------------------
      void RUDPChannel::init()
      {
        AutoRecursiveLock lock(mLock);
        step();
      }

      //-----------------------------------------------------------------------
      RUDPChannel::~RUDPChannel()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel(false);
      }

      //-----------------------------------------------------------------------
      RUDPChannelPtr RUDPChannel::convert(IRUDPChannelPtr channel)
      {
        return boost::dynamic_pointer_cast<RUDPChannel>(channel);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark RUDPChannel => IRUDPChannel
      #pragma mark

      //-----------------------------------------------------------------------
      String RUDPChannel::toDebugString(IRUDPChannelPtr channel, bool includeCommaPrefix)
      {
        if (!channel) return String(includeCommaPrefix ? ", rudp channel=(null)" : "rudp channel=(null)");

        RUDPChannelPtr pThis = RUDPChannel::convert(channel);
        return pThis->getDebugValueString(includeCommaPrefix);
      }

      //-----------------------------------------------------------------------
      IRUDPChannel::RUDPChannelStates RUDPChannel::getState(
                                                            WORD *outLastErrorCode,
                                                            String *outLastErrorReason
                                                            ) const
      {
        AutoRecursiveLock lock(mLock);
        if (outLastErrorCode) *outLastErrorCode = mLastError;
        if (outLastErrorReason) *outLastErrorReason = mLastErrorReason;
        return mCurrentState;
      }

      //-----------------------------------------------------------------------
      void RUDPChannel::shutdown()
      {
        ZS_LOG_DETAIL(log("shutdown called"))
        AutoRecursiveLock lock(mLock);
        cancel(true);
      }

      //-----------------------------------------------------------------------
      void RUDPChannel::shutdownDirection(Shutdown state)
      {
        AutoRecursiveLock lock(mLock);
        ZS_LOG_DETAIL(log("shutdown direction called") + ", state=" + IRUDPChannel::toString(state) + ", current shutdown=" + IRUDPChannel::toString(mShutdownDirection))
        mShutdownDirection = static_cast<IRUDPChannelStream::Shutdown>(mShutdownDirection | state);
        if (!mStream) return;
        mStream->shutdownDirection(mShutdownDirection);
      }

      //-----------------------------------------------------------------------
      IPAddress RUDPChannel::getConnectedRemoteIP()
      {
        AutoRecursiveLock lock(mLock);
        ZS_LOG_DEBUG(log("get connected remote IP called") + ", ip=" + mRemoteIP.string())
        return mRemoteIP;
      }

      //-----------------------------------------------------------------------
      String RUDPChannel::getRemoteConnectionInfo()
      {
        AutoRecursiveLock lock(mLock);
        ZS_LOG_DEBUG(log("get connection info called") + ", info=" + mRemoteChannelInfo)
        return mRemoteChannelInfo;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark RUDPChannel => IRUDPChannelForRUDPICESocketSession
      #pragma mark

      //-----------------------------------------------------------------------
      RUDPChannelPtr RUDPChannel::createForRUDPICESocketSessionIncoming(
                                                                        IMessageQueuePtr queue,
                                                                        IRUDPChannelDelegateForSessionAndListenerPtr master,
                                                                        const IPAddress &remoteIP,
                                                                        WORD incomingChannelNumber,
                                                                        const char *localUsernameFrag,
                                                                        const char *localPassword,
                                                                        const char *remoteUsernameFrag,
                                                                        const char *remotePassword,
                                                                        STUNPacketPtr stun,
                                                                        STUNPacketPtr &outResponse
                                                                        )
      {
        QWORD sequenceNumber = 0;
        DWORD minimumRTT = 0;
        IRUDPChannelStream::CongestionAlgorithmList localAlgorithms;
        IRUDPChannelStream::CongestionAlgorithmList remoteAlgorithms;
        IRUDPChannelStream::getRecommendedStartValues(sequenceNumber, minimumRTT, localAlgorithms, remoteAlgorithms);

        DWORD lifetime = OPENPEER_SERVICES_RUDPCHANNEL_DEFAULT_LIFETIME_IN_SECONDS;
        if (stun->hasAttribute(STUNPacket::Attribute_Lifetime)) {
          lifetime = stun->mLifetime;
        }
        // do not ever negotiate higher
        if (lifetime > OPENPEER_SERVICES_RUDPCHANNEL_DEFAULT_LIFETIME_IN_SECONDS)
          lifetime = OPENPEER_SERVICES_RUDPCHANNEL_DEFAULT_LIFETIME_IN_SECONDS;

        if (stun->hasAttribute(STUNPacket::Attribute_MinimumRTT)) {
          minimumRTT = (minimumRTT > stun->mMinimumRTT ? minimumRTT : stun->mMinimumRTT);
        }

        RUDPChannelPtr pThis(new RUDPChannel(
                                             queue,
                                             master,
                                             remoteIP,
                                             localUsernameFrag,
                                             localPassword,
                                             remoteUsernameFrag,
                                             remotePassword,
                                             minimumRTT,
                                             lifetime,
                                             incomingChannelNumber,
                                             sequenceNumber,
                                             NULL,
                                             stun->mChannelNumber,
                                             stun->mNextSequenceNumber,
                                             stun->mConnectionInfo
                                             ));

        pThis->mThisWeak = pThis;
        get(pThis->mIncoming) = true;
        pThis->init();
        // do not allow sending to the remote party until we receive an ACK or data
        pThis->mStream = IRUDPChannelStream::create(queue, pThis, pThis->mLocalSequenceNumber, pThis->mRemoteSequenceNumber, pThis->mOutgoingChannelNumber, pThis->mIncomingChannelNumber, pThis->mMinimumRTT);
        pThis->mStream->holdSendingUntilReceiveSequenceNumber(stun->mNextSequenceNumber);
        pThis->handleSTUN(stun, outResponse, localUsernameFrag, remoteUsernameFrag);
        if (!outResponse) {
          ZS_LOG_WARNING(Detail, pThis->log("failed to create a STUN response for the incoming channel so channel must be closed"))
          pThis->setError(RUDPChannelShutdownReason_OpenFailure, "open channel failure");
          pThis->cancel(false);
        }
        if (outResponse) {
          if (STUNPacket::Class_ErrorResponse == outResponse->mClass) {
            ZS_LOG_WARNING(Detail, pThis->log("failed to create an incoming channel as STUN response was a failure"))
            pThis->setError(RUDPChannelShutdownReason_OpenFailure, "open channel failure");
            pThis->cancel(false);
          }
        }
        ZS_LOG_DETAIL(pThis->log("created for socket session incoming") + ", localUsernameFrag=" + localUsernameFrag + ", remoteUsernameFrag=" + remoteUsernameFrag + ", local password=" + localPassword + ", remote password=" + remotePassword + ", incoming channel=" + string(incomingChannelNumber))
        return pThis;
      }

      //-----------------------------------------------------------------------
      RUDPChannelPtr RUDPChannel::createForRUDPICESocketSessionOutgoing(
                                                                        IMessageQueuePtr queue,
                                                                        IRUDPChannelDelegateForSessionAndListenerPtr master,
                                                                        IRUDPChannelDelegatePtr delegate,
                                                                        const IPAddress &remoteIP,
                                                                        WORD incomingChannelNumber,
                                                                        const char *localUsernameFrag,
                                                                        const char *localPassword,
                                                                        const char *remoteUsernameFrag,
                                                                        const char *remotePassword,
                                                                        const char *connectionInfo,
                                                                        ITransportStreamPtr receiveStream,
                                                                        ITransportStreamPtr sendStream
                                                                        )
      {
        QWORD sequenceNumber = 0;
        DWORD minimumRTT = 0;
        IRUDPChannelStream::CongestionAlgorithmList localAlgorithms;
        IRUDPChannelStream::CongestionAlgorithmList remoteAlgorithms;
        IRUDPChannelStream::getRecommendedStartValues(sequenceNumber, minimumRTT, localAlgorithms, remoteAlgorithms);

        RUDPChannelPtr pThis(new RUDPChannel(
                                             queue,
                                             master,
                                             remoteIP,
                                             localUsernameFrag,
                                             localPassword,
                                             remoteUsernameFrag,
                                             remotePassword,
                                             minimumRTT,
                                             OPENPEER_SERVICES_RUDPCHANNEL_DEFAULT_LIFETIME_IN_SECONDS,
                                             incomingChannelNumber,
                                             sequenceNumber,
                                             connectionInfo
                                             ));

        pThis->mThisWeak = pThis;
        pThis->mDelegate = IRUDPChannelDelegateProxy::createWeak(queue, delegate);
        pThis->mReceiveStream = receiveStream;
        pThis->mSendStream = sendStream;
        pThis->init();
        // do not allow sending to the remote party until we receive an ACK or data
        ZS_LOG_DETAIL(pThis->log("created for socket session outgoing") + ", localUserFrag=" + localUsernameFrag + ", remoteUsernameFrag=" + remoteUsernameFrag + ", local password=" + localPassword + ", remote password=" + remotePassword + ", incoming channel=" + string(incomingChannelNumber))
        return pThis;
      }
      
      //-----------------------------------------------------------------------
      void RUDPChannel::setDelegate(IRUDPChannelDelegatePtr delegate)
      {
        ZS_LOG_DEBUG(log("set delegate called"))

        AutoRecursiveLock lock(mLock);

        mDelegate = IRUDPChannelDelegateProxy::createWeak(getAssociatedMessageQueue(), delegate);

        try
        {
          if (RUDPChannelState_Connecting != mCurrentState) {
            ZS_LOG_DEBUG(log("delegate notified of channel state change"))
            mDelegate->onRDUPChannelStateChanged(mThisWeak.lock(), mCurrentState);
          }
        } catch(IRUDPChannelDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_ERROR(Basic, log("delegate destroyed during the set"))
          setError(RUDPChannelShutdownReason_DelegateGone, "delegate gone");
          cancel(false);
          return;
        }
      }

      //-----------------------------------------------------------------------
      void RUDPChannel::setStreams(
                                   ITransportStreamPtr receiveStream,
                                   ITransportStreamPtr sendStream
                                   )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!receiveStream)
        ZS_THROW_INVALID_ARGUMENT_IF(!sendStream)

        ZS_LOG_DEBUG(log("set streams called"))
        AutoRecursiveLock lock(mLock);

        mReceiveStream = receiveStream;
        mSendStream = sendStream;

        if (mStream) {
          mStream->setStreams(receiveStream, sendStream);
        }
      }

      //-----------------------------------------------------------------------
      bool RUDPChannel::handleSTUN(
                                   STUNPacketPtr stun,
                                   STUNPacketPtr &outResponse,
                                   const String &localUsernameFrag,
                                   const String &remoteUsernameFrag
                                   )
      {
        AutoRecursiveLock lock(mLock);
        if (!mMasterDelegate) {
          ZS_LOG_TRACE(log("no master delegate, thus ignoring"))
          return false;
        }

        if ((localUsernameFrag != mLocalUsernameFrag) &&
            (remoteUsernameFrag != mRemoteUsernameFrag)) {
          ZS_LOG_TRACE(log("the request local/remote username frag does not match, thus ignoring") + ", local:remote username frag=" + localUsernameFrag + ":" + remoteUsernameFrag + ", expected local:remote username frag=" + mLocalUsernameFrag + ":" + mRemoteUsernameFrag)
          return false;
        }

        if (remoteUsernameFrag.size() < 1) {
          ZS_LOG_TRACE(log("missing remote username frag, thus ignoring") + ", local:remote username frag=" + localUsernameFrag + ":" + remoteUsernameFrag + ", expected local:remote username frag=" + mLocalUsernameFrag + ":" + mRemoteUsernameFrag)
          return false;
        }

        // do not allow a change in credentials!
        if ((stun->mRealm != mRealm) ||
            (stun->mNonce != mNonce)) {

          ZS_LOG_ERROR(Detail, log("STUN message credentials mismatch") + ", expecting username=" + (mLocalUsernameFrag + ":" + mRemoteUsernameFrag) + ", received username=" + stun->mUsername + ", expecting realm=" + mRealm + ", received realm=" + stun->mRealm + ", expecting nonce=" + mNonce + ", received nonce=" + stun->mNonce)
          if (STUNPacket::Class_Request == stun->mClass) {
            stun->mErrorCode = STUNPacket::ErrorCode_Unauthorized;  // the username doesn't match
            outResponse = STUNPacket::createErrorResponse(stun);
            fix(outResponse);
          }
          return true;
        }

        // make sure the message integrity is correct!
        if (!isValidIntegrity(stun)) {
          ZS_LOG_ERROR(Detail, log("STUN message integrity failed"))
          if (STUNPacket::Class_Request == stun->mClass) {
            stun->mErrorCode = STUNPacket::ErrorCode_Unauthorized;  // the password doesn't match
            outResponse = STUNPacket::createErrorResponse(stun);
            fix(outResponse);
          }
          return true;
        }

        if (STUNPacket::Method_ReliableChannelOpen == stun->mMethod) {
          if (STUNPacket::Class_Request != stun->mClass) {
            ZS_LOG_ERROR(Debug, log("received illegal class on STUN request") + ", class=" + string((int)stun->mClass))
            return false;  // illegal unless it is a request, responses will come through a different method
          }

          DWORD lifetime = OPENPEER_SERVICES_RUDPCHANNEL_DEFAULT_LIFETIME_IN_SECONDS;
          if (stun->hasAttribute(STUNPacket::Attribute_Lifetime)) {
            lifetime = stun->mLifetime;
          }

          if (0 == lifetime) {
            ZS_LOG_DETAIL(log("received open channel with lifetime of 0 thus closing channel"))

            if (!mStream) {
              // we were never connected, so return an error code...
              ZS_LOG_WARNING(Detail, log("received open channel with lifetime of 0 but the channel is not open"))
              stun->mErrorCode = STUNPacket::ErrorCode_Unauthorized;  // the password doesn't match
              outResponse = STUNPacket::createErrorResponse(stun);
              return true;
            }

            // a lifetime of zero means they are closing the channel
            outResponse = STUNPacket::createResponse(stun);
            fix(stun);
            outResponse->mLifetimeIncluded = true;
            outResponse->mLifetime = 0;
            outResponse->mUsername = mLocalUsernameFrag + ":" + mRemoteUsernameFrag;
            outResponse->mPassword = mLocalPassword;
            outResponse->mRealm = stun->mRealm;
            if (!mRealm.isEmpty()) {
              outResponse->mCredentialMechanism = STUNPacket::CredentialMechanisms_LongTerm;
            } else {
              outResponse->mCredentialMechanism = STUNPacket::CredentialMechanisms_ShortTerm;
            }

            if (mOpenRequest) {
              mOpenRequest->cancel();
              mOpenRequest.reset();
            }

            setError(RUDPChannelShutdownReason_OpenFailure, "open channel failure");
            cancel(false);
            return true;
          }

          if (lifetime > OPENPEER_SERVICES_RUDPCHANNEL_DEFAULT_LIFETIME_IN_SECONDS)
            lifetime = OPENPEER_SERVICES_RUDPCHANNEL_DEFAULT_LIFETIME_IN_SECONDS;

          // if the lifetime is too low we can't keep up with keep alives so reject it
          if (lifetime < 20) {
            ZS_LOG_ERROR(Detail, log("received open channel with lifetime too low"))
            stun->mErrorCode = STUNPacket::ErrorCode_BadRequest;
            outResponse = STUNPacket::createErrorResponse(stun);
            fix(outResponse);
            return true;
          }

          QWORD sequenceNumber = 0;
          DWORD minimumRTT = 0;
          IRUDPChannelStream::CongestionAlgorithmList localAlgorithms;
          IRUDPChannelStream::CongestionAlgorithmList remoteAlgorithms;
          IRUDPChannelStream::getRecommendedStartValues(sequenceNumber, minimumRTT, localAlgorithms, remoteAlgorithms);

          if (stun->hasAttribute(STUNPacket::Attribute_MinimumRTT)) {
            minimumRTT = (minimumRTT > stun->mMinimumRTT ? minimumRTT : stun->mMinimumRTT);
          }

          // verify all attributes match or we will flag as unauthorized as we don't support renegotiation at this time
          if ((stun->mChannelNumber != mOutgoingChannelNumber) ||
              (minimumRTT != mMinimumRTT) ||
              (stun->mConnectionInfo != mRemoteChannelInfo) ||
              (lifetime != mLifetime) ||
              (stun->mLocalCongestionControl.size() < 1) ||
              (stun->mRemoteCongestionControl.size() < 1)) {
            ZS_LOG_WARNING(Detail, log("received open channel with non supported renegociation"))
            stun->mErrorCode = STUNPacket::ErrorCode_AllocationMismatch;  // sorry, not going to support renegotiation at this time...
            outResponse = STUNPacket::createErrorResponse(stun);
            fix(outResponse);
            return true;
          }

          // do not try to sneak in new congestion controls... we do not support anything but the default right now...
          if ((stun->mLocalCongestionControl.end() == find(stun->mLocalCongestionControl.begin(), stun->mLocalCongestionControl.end(), IRUDPChannel::CongestionAlgorithm_TCPLikeWindowWithSlowCreepUp)) ||
              (stun->mRemoteCongestionControl.end() == find(stun->mRemoteCongestionControl.begin(), stun->mRemoteCongestionControl.end(), IRUDPChannel::CongestionAlgorithm_TCPLikeWindowWithSlowCreepUp))) {
            ZS_LOG_ERROR(Detail, log("received open channel with unsupported congrestion controls"))
            stun->mErrorCode = STUNPacket::ErrorCode_UnsupportedTransportProtocol;
            outResponse = STUNPacket::createErrorResponse(stun);
            fix(outResponse);
            return true;
          }

          mLastReceivedData = zsLib::now();
          outResponse = STUNPacket::createResponse(stun);
          fix(outResponse);
          outResponse->mUsername = mLocalUsernameFrag + ":" + mRemoteUsernameFrag;
          outResponse->mPassword = mLocalPassword;
          outResponse->mRealm = stun->mRealm;
          if (!mRealm.isEmpty()) {
            outResponse->mCredentialMechanism = STUNPacket::CredentialMechanisms_LongTerm;
          } else {
            outResponse->mCredentialMechanism = STUNPacket::CredentialMechanisms_ShortTerm;
          }

          outResponse->mNextSequenceNumber = mLocalSequenceNumber;
          outResponse->mMinimumRTTIncluded = true;
          outResponse->mMinimumRTT = minimumRTT;
          outResponse->mLifetime = lifetime;
          outResponse->mChannelNumber = mIncomingChannelNumber;

          IRUDPChannelStream::getResponseToOfferedAlgorithms(
                                                             stun->mRemoteCongestionControl,        // the remote applies to us
                                                             stun->mLocalCongestionControl,         // the local applies to them
                                                             outResponse->mLocalCongestionControl,  // in reply, local applies to us
                                                             outResponse->mRemoteCongestionControl  // in reply, remote applies to them
                                                             );

          ZS_LOG_DETAIL(log("received open channel succeeded"))
          (IWakeDelegateProxy::create(mThisWeak.lock()))->onWake();
          return true;
        }

        // this has to be a reliable channel ACK or it is not legal
        if (STUNPacket::Method_ReliableChannelACK != stun->mMethod) {
          ZS_LOG_ERROR(Debug, log("received illegal method on STUN request") + ", method=" + string((int)stun->mMethod))
          return false;
        }

        // only legal to be a request or an indication
        if ((STUNPacket::Class_Request != stun->mClass) &&
            (STUNPacket::Class_Indication != stun->mClass)) {
          ZS_LOG_ERROR(Debug, log("was expecting a request or an indication only for the class on the reliable ACK") + ", class=" + string((int)stun->mClass))
          return false;
        }

        if (!mStream) return false; // not possible to handle a stream if there is no stream attached

        if ((!stun->hasAttribute(STUNPacket::Attribute_NextSequenceNumber)) ||
            (!stun->hasAttribute(STUNPacket::Attribute_GSNR)) ||
            (!stun->hasAttribute(STUNPacket::Attribute_GSNFR)) ||
            (!stun->hasAttribute(STUNPacket::Attribute_RUDPFlags))) {

          ZS_LOG_ERROR(Detail, log("received external ACK with missing attributes"))
          // all of these attributes are manditory or it is not legal
          if (STUNPacket::Class_Request == stun->mClass) {
            stun->mErrorCode = STUNPacket::ErrorCode_BadRequest;
            outResponse = STUNPacket::createErrorResponse(stun);
            fix(outResponse);
          }
          return true;
        }

        ZS_LOG_DETAIL(log("received external ACK request or indication") + ", sequence number=" + sequenceToString(stun->mNextSequenceNumber) + ", GSNR=" + sequenceToString(stun->mGSNR) + ", GSNFR=" + sequenceToString(stun->mGSNFR))

        mLastReceivedData = zsLib::now();
        mStream->handleExternalAck(
                                   0,
                                   stun->mNextSequenceNumber,
                                   stun->mGSNR,
                                   stun->mGSNFR,
                                   stun->hasAttribute(STUNPacket::Attribute_ACKVector) ? stun->mACKVector.get() : NULL,
                                   stun->hasAttribute(STUNPacket::Attribute_ACKVector) ? stun->mACKVectorLength : 0,
                                   (0 != (stun->mReliabilityFlags & RUDPPacket::Flag_VP_VectorParity)),
                                   (0 != (stun->mReliabilityFlags & RUDPPacket::Flag_PG_ParityGSNR)),
                                   (0 != (stun->mReliabilityFlags & RUDPPacket::Flag_XP_XORedParityToGSNFR)),
                                   (0 != (stun->mReliabilityFlags & RUDPPacket::Flag_DP_DuplicatePacket)),
                                   (0 != (stun->mReliabilityFlags & RUDPPacket::Flag_EC_ECNPacket))
                                   );

        if (STUNPacket::Class_Request != stun->mClass) return true; // handled now

        outResponse = STUNPacket::createResponse(stun);
        fix(outResponse);
        fillACK(outResponse);

        return true;
      }

      //-----------------------------------------------------------------------
      void RUDPChannel::handleRUDP(
                                   RUDPPacketPtr rudp,
                                   const BYTE *buffer,
                                   ULONG bufferLengthInBytes
                                   )
      {
        IRUDPChannelStreamPtr stream;
        boost::shared_array<BYTE> newBuffer;

        // scope: do the work in the context of a lock but call the stream outside the lock
        {
          AutoRecursiveLock lock(mLock);
          if (!mStream) {
            ZS_LOG_WARNING(Trace, log("no attached stream to channel thus ignoring RUDP packet"))
            return;
          }

          stream = mStream;

          ZS_LOG_DEBUG(log("received RUDP packet") + ", stream ID=" + string(stream->getID()) + ", length=" + string(bufferLengthInBytes))

          newBuffer = boost::shared_array<BYTE>(new BYTE[bufferLengthInBytes]);
          memcpy(newBuffer.get(), buffer, bufferLengthInBytes);

          // fix the pointer to point to the newly constructed buffer
          if (NULL != rudp->mData)
            rudp->mData = (newBuffer.get() + (rudp->mData - buffer));

          mLastReceivedData = zsLib::now();
        }

        stream->handlePacket(rudp, newBuffer, bufferLengthInBytes, false);
      }

      //-----------------------------------------------------------------------
      void RUDPChannel::notifyWriteReady()
      {
        IRUDPChannelStreamPtr stream;

        // scope: do the work in the context of a lock but call the stream outside the lock
        {
          AutoRecursiveLock lock(mLock);
          if (!mStream) {
            ZS_LOG_WARNING(Trace, log("received notify write ready but no stream is attached"))
            return;
          }

          stream = mStream;

          ZS_LOG_DEBUG(log("received notify write ready") + ", stream ID=" + string(stream->getID()))
        }
        stream->notifySocketWriteReady();
      }

      //-----------------------------------------------------------------------
      WORD RUDPChannel::getIncomingChannelNumber() const
      {
        AutoRecursiveLock lock(mLock);
        return mIncomingChannelNumber;
      }

      //-----------------------------------------------------------------------
      WORD RUDPChannel::getOutgoingChannelNumber() const
      {
        AutoRecursiveLock lock(mLock);
        return mOutgoingChannelNumber;
      }

      //-----------------------------------------------------------------------
      void RUDPChannel::issueConnectIfNotIssued()
      {
        AutoRecursiveLock lock(mLock);

        if ((isShuttingDown()) ||
            (isShutdown())) return;

        if (mOpenRequest) return;

        IRUDPChannelStream::CongestionAlgorithmList local;
        IRUDPChannelStream::CongestionAlgorithmList remote;

        QWORD ignore1 = 0;
        DWORD ignore2 = 0;
        IRUDPChannelStream::getRecommendedStartValues(ignore1, ignore2, local, remote);

        STUNPacketPtr stun = STUNPacket::createRequest(STUNPacket::Method_ReliableChannelOpen);
        fix(stun);
        fillCredentials(stun);
        stun->mLifetimeIncluded = true;
        stun->mLifetime = OPENPEER_SERVICES_RUDPCHANNEL_DEFAULT_LIFETIME_IN_SECONDS;
        stun->mNextSequenceNumber = mLocalSequenceNumber;
        stun->mChannelNumber = mIncomingChannelNumber;
        stun->mMinimumRTTIncluded = true;
        stun->mMinimumRTT = mMinimumRTT;
        stun->mConnectionInfo = mLocalChannelInfo;
        stun->mLocalCongestionControl = local;
        stun->mRemoteCongestionControl = remote;
        if (mRemotePassword.isEmpty()) {
          // we are attempting a server connect, we won't actually put the username on the request but we will generate a username for later
          if (mRemoteUsernameFrag.isEmpty()) {
            mRemoteUsernameFrag = IHelper::randomString(20);
          }
          mRemotePassword = mRemoteUsernameFrag;
          mLocalPassword = mLocalUsernameFrag;
          stun->mUsername.clear();
          stun->mPassword.clear();
          stun->mCredentialMechanism = STUNPacket::CredentialMechanisms_None;
        } else {
          stun->mUsername = (mRemoteUsernameFrag + ":" + mLocalUsernameFrag);
          stun->mPassword = mRemotePassword;
          stun->mCredentialMechanism = STUNPacket::CredentialMechanisms_ShortTerm;
        }
        ZS_LOG_DETAIL(log("issuing channel open request"))
        mOpenRequest = ISTUNRequester::create(getAssociatedMessageQueue(), mThisWeak.lock(), mRemoteIP, stun, STUNPacket::RFC_draft_RUDP);
      }

      //-----------------------------------------------------------------------
      void RUDPChannel::shutdownFromTimeout()
      {
        ZS_LOG_DEBUG(log("shutdown from timeout called"))
        AutoRecursiveLock lock(mLock);
        get(mSTUNRequestPreviouslyTimedOut) = true;
        setError(RUDPChannelShutdownReason_Timeout, "Timeout failure");
        cancel(false);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark RUDPChannel => IRUDPChannelForRUDPListener
      #pragma mark

      //-----------------------------------------------------------------------
      RUDPChannelPtr RUDPChannel::createForListener(
                                                    IMessageQueuePtr queue,
                                                    IRUDPChannelDelegateForSessionAndListenerPtr master,
                                                    const IPAddress &remoteIP,
                                                    WORD incomingChannelNumber,
                                                    STUNPacketPtr stun,
                                                    STUNPacketPtr &outResponse
                                                    )
      {
        String localUsernameFrag;
        String remoteUsernameFrag;
        size_t pos = stun->mUsername.find(":");
        if (String::npos == pos) {
          localUsernameFrag = stun->mUsername;
          remoteUsernameFrag = stun->mUsername;
        } else {
          // split the string at the fragments
          localUsernameFrag = stun->mUsername.substr(0, pos); // this would be our local username
          remoteUsernameFrag = stun->mUsername.substr(pos+1);  // this would be the remote username
        }

        QWORD sequenceNumber = 0;
        DWORD minimumRTT = 0;
        IRUDPChannelStream::CongestionAlgorithmList localAlgorithms;
        IRUDPChannelStream::CongestionAlgorithmList remoteAlgorithms;
        IRUDPChannelStream::getRecommendedStartValues(sequenceNumber, minimumRTT, localAlgorithms, remoteAlgorithms);

        DWORD lifetime = OPENPEER_SERVICES_RUDPCHANNEL_DEFAULT_LIFETIME_IN_SECONDS;
        if (stun->hasAttribute(STUNPacket::Attribute_Lifetime)) {
          lifetime = stun->mLifetime;
        }
        // do not ever negotiate higher
        if (lifetime > OPENPEER_SERVICES_RUDPCHANNEL_DEFAULT_LIFETIME_IN_SECONDS)
          lifetime = OPENPEER_SERVICES_RUDPCHANNEL_DEFAULT_LIFETIME_IN_SECONDS;

        if (stun->hasAttribute(STUNPacket::Attribute_MinimumRTT)) {
          minimumRTT = (minimumRTT > stun->mMinimumRTT ? minimumRTT : stun->mMinimumRTT);
        }

        RUDPChannelPtr pThis(new RUDPChannel(
                                             queue,
                                             master,
                                             remoteIP,
                                             localUsernameFrag,
                                             localUsernameFrag,
                                             remoteUsernameFrag,
                                             remoteUsernameFrag,
                                             minimumRTT,
                                             lifetime,
                                             incomingChannelNumber,
                                             sequenceNumber,
                                             NULL,
                                             stun->mChannelNumber,
                                             stun->mNextSequenceNumber,
                                             stun->mConnectionInfo
                                             ));

        pThis->mThisWeak = pThis;
        get(pThis->mIncoming) = true;
        pThis->mRealm = stun->mRealm;
        pThis->mNonce = stun->mNonce;
        pThis->init();
        // do not allow sending to the remote party until we receive an ACK or data
        pThis->mStream = IRUDPChannelStream::create(queue, pThis, pThis->mLocalSequenceNumber, pThis->mRemoteSequenceNumber, pThis->mOutgoingChannelNumber, pThis->mIncomingChannelNumber, pThis->mMinimumRTT);
        pThis->mStream->holdSendingUntilReceiveSequenceNumber(stun->mNextSequenceNumber);
        pThis->handleSTUN(stun, outResponse, localUsernameFrag, remoteUsernameFrag);
        if (!outResponse) {
          ZS_LOG_WARNING(Detail, pThis->log("failed to provide a STUN response so channel must be closed"))
          pThis->setError(RUDPChannelShutdownReason_OpenFailure, "channel open failure");
          pThis->cancel(false);
        }
        if (outResponse) {
          if (STUNPacket::Class_ErrorResponse == outResponse->mClass) {
            ZS_LOG_WARNING(Detail, pThis->log("channel could not be opened as response was a failure"))
            pThis->setError(RUDPChannelShutdownReason_OpenFailure, "channel open failure");
            pThis->cancel(false);
          }
        }
        ZS_LOG_BASIC(pThis->log("created for listener") + ", localUserFrag=" + localUsernameFrag + ", remoteUserFrag=" + remoteUsernameFrag + ", incoming channel=" + string(incomingChannelNumber))
        return pThis;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark RUDPChannel => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void RUDPChannel::onWake()
      {
        AutoRecursiveLock lock(mLock);
        ZS_LOG_DETAIL(log("on wake"))

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark RUDPChannel => IRUDPChannelStreamDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void RUDPChannel::onRUDPChannelStreamStateChanged(
                                                        IRUDPChannelStreamPtr stream,
                                                        RUDPChannelStreamStates state
                                                        )
      {
        AutoRecursiveLock lock(mLock);
        ZS_LOG_DETAIL(log("notify channel stream shutdown") + ", stream ID=" + string(stream->getID()))

        if (stream != mStream) return;

        if (IRUDPChannelStream::RUDPChannelStreamState_Shutdown != state) return;

        WORD errorCode = 0;
        String reason;
        mStream->getState(&errorCode, &reason);

        if (0 != errorCode) {
          setError(errorCode, reason);
        }

        // the channel stream is shutdown, stop the channel
        cancel(false);
      }

      //-----------------------------------------------------------------------
      bool RUDPChannel::notifyRUDPChannelStreamSendPacket(
                                                          IRUDPChannelStreamPtr stream,
                                                          const BYTE *packet,
                                                          ULONG packetLengthInBytes
                                                          )
      {
        ZS_LOG_DEBUG(log("notify channel stream send packet") + ", stream ID=" + string(stream->getID()) + ", length=" + string(packetLengthInBytes))
        IRUDPChannelDelegateForSessionAndListenerPtr master;
        IPAddress remoteIP;

        {
          AutoRecursiveLock lock(mLock);
          if (!mMasterDelegate) return false;
          master = mMasterDelegate;
          remoteIP = mRemoteIP;
          mLastSentData = zsLib::now();
        }

        try {
          return master->notifyRUDPChannelSendPacket(mThisWeak.lock(), remoteIP, packet, packetLengthInBytes);
        } catch(IRUDPChannelDelegateForSessionAndListenerProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("master delegate gone for sent packet"))
          setError(RUDPChannelShutdownReason_DelegateGone, "delegate gone");
          cancel(false);
        }
        return false;
      }

      //-----------------------------------------------------------------------
      void RUDPChannel::onRUDPChannelStreamSendExternalACKNow(
                                                              IRUDPChannelStreamPtr stream,
                                                              bool guarenteeDelivery,
                                                              PUID guarenteeDeliveryRequestID
                                                              )
      {
        ZS_LOG_DETAIL(log("notify channel stream send external ACK now") + ", stream ID=" + string(stream->getID()) + ", gaurantee=" + (guarenteeDelivery ? "true" : "false"))
        IRUDPChannelDelegateForSessionAndListenerPtr master;
        STUNPacketPtr stun;

        IPAddress remoteIP;
        boost::shared_array<BYTE> packet;
        ULONG packetLengthInBytes = 0;

        {
          AutoRecursiveLock lock(mLock);
          if (!mMasterDelegate) return;
          if (!mStream) return;

          master = mMasterDelegate;
          remoteIP = mRemoteIP;

          stun = (guarenteeDelivery ? STUNPacket::createRequest(STUNPacket::Method_ReliableChannelACK) : STUNPacket::createIndication(STUNPacket::Method_ReliableChannelACK));
          fix(stun);
          fillACK(stun);

          if (guarenteeDelivery) {
            ZS_LOG_DETAIL(log("STUN ACK sent via STUN requester") + ", method=request")
            ISTUNRequesterPtr request = ISTUNRequester::create(getAssociatedMessageQueue(), mThisWeak.lock(), mRemoteIP, stun, STUNPacket::RFC_draft_RUDP);
            if (0 == guarenteeDeliveryRequestID) {
              guarenteeDeliveryRequestID = zsLib::createPUID();
            }

            mOutstandingACKs[guarenteeDeliveryRequestID] = request;
            return;
          }

          stun->packetize(packet, packetLengthInBytes, STUNPacket::RFC_draft_RUDP);
          ZS_LOG_DETAIL(log("STUN ACK sent") + ", method=indication, stun packet size=" + string(packetLengthInBytes))
          mLastSentData = zsLib::now();
        }

        try {
          master->notifyRUDPChannelSendPacket(mThisWeak.lock(), remoteIP, packet.get(), packetLengthInBytes);
        } catch(IRUDPChannelDelegateForSessionAndListenerProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("master delegate gone for send external ack now"))
          setError(RUDPChannelShutdownReason_DelegateGone, "delegate gone");
          cancel(false);
          return;
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark RUDPChannel => ISTUNRequesterDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void RUDPChannel::onSTUNRequesterSendPacket(
                                                  ISTUNRequesterPtr requester,
                                                  IPAddress destination,
                                                  boost::shared_array<BYTE> packet,
                                                  ULONG packetLengthInBytes
                                                  )
      {
        ZS_LOG_DEBUG(log("notify requester send packet") + ", ip=" + destination.string() + ", length=" + string(packetLengthInBytes))

        IRUDPChannelDelegateForSessionAndListenerPtr master;
        bool isShuttdingDown = false;

        {
          AutoRecursiveLock lock(mLock);
          if (!mMasterDelegate) {
            ZS_LOG_WARNING(Detail, log("cannot send STUN packet as master delegate gone"))
            return;
          }
          master = mMasterDelegate;
          mLastSentData = zsLib::now();
          isShuttdingDown = isShuttingDown();
        }

        try {
          bool result = master->notifyRUDPChannelSendPacket(mThisWeak.lock(), destination, packet.get(), packetLengthInBytes);
          if ((!result) &&
              (isShuttdingDown)) {
            goto do_shutdown;
          }
        } catch(IRUDPChannelDelegateForSessionAndListenerProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("cannot send STUN packet as master delegate is now gone"))
          goto do_shutdown;
        }

        return;

      do_shutdown:
        {
          AutoRecursiveLock lock(mLock);
          if (isShuttdingDown) {
            if (mStream) {
              mStream->shutdown(false);
            }
            if (mShutdownRequest) {
              mShutdownRequest->cancel();
            }
          }
          setError(RUDPChannelShutdownReason_DelegateGone, "delegate gone");
          cancel(false);
        }
      }

      //-----------------------------------------------------------------------
      bool RUDPChannel::handleSTUNRequesterResponse(
                                                    ISTUNRequesterPtr requester,
                                                    IPAddress fromIPAddress,
                                                    STUNPacketPtr response
                                                    )
      {
        ZS_LOG_DEBUG(log("notify requester received reply") + ", ip=" + fromIPAddress.string())
        AutoRecursiveLock lock(mLock);
        if (!mMasterDelegate) return false;

        if (STUNPacket::Class_Response == response->mClass) {
          if (!isValidIntegrity(response)) {
            ZS_LOG_ERROR(Detail, log("requester response failed integrity"))
            return false;  // nope, not legal reply
          }
          mLastReceivedData = zsLib::now();
        }

        if (requester == mShutdownRequest) {
          if (handleStaleNonce(mShutdownRequest, response)) return true;

          ZS_LOG_DETAIL(log("shutdown requester completed"))

          if (mStream) {
            mStream->shutdown(false);
          }

          (IWakeDelegateProxy::create(mThisWeak.lock()))->onWake();
          return true;
        }

        if (requester == mOpenRequest) {
          if (handleStaleNonce(mOpenRequest, response)) return true;

          if (STUNPacket::Class_ErrorResponse == response->mClass) {
            if (STUNPacket::ErrorCode_Unauthorized == response->mErrorCode) {
              // this request is unauthorized but perhaps because we have to reissue the request now that server has given us a nonce?
              STUNPacketPtr originalRequest = requester->getRequest();
              if (originalRequest->mUsername.isEmpty()) {
                ZS_LOG_DETAIL(log("open unauthorized-challenge") + ", realm=" + response->mRealm + ", nonce=" + response->mNonce)

                // correct, we have to re-issue the request now
                STUNPacketPtr newRequest = originalRequest->clone(true);
                mNonce = response->mNonce;
                mRealm = response->mRealm;
                fillCredentials(newRequest);
                // reissue the request
                mOpenRequest = ISTUNRequester::create(getAssociatedMessageQueue(), mThisWeak.lock(), mRemoteIP, newRequest, STUNPacket::RFC_draft_RUDP);
                return true;
              }
              // nope, that's not the issue, we have failed...
            }

            if (mOpenRequest) {
              mOpenRequest->cancel();
              mOpenRequest.reset();
            }

            ZS_LOG_WARNING(Detail, log("failed to open as error response was received"))
            setError(RUDPChannelShutdownReason_OpenFailure, "open failure");
            cancel(false);  // failed to open, we are done...
            return true;
          }

          // these attributes must be present in the response of it is illegal
          if ((!response->hasAttribute(STUNPacket::Attribute_ChannelNumber)) ||
              (!response->hasAttribute(STUNPacket::Attribute_NextSequenceNumber))) {
            ZS_LOG_ERROR(Detail, log("open missing attributes") + ", channel=" + string(response->mChannelNumber) + ", sequence number=" + sequenceToString(response->mNextSequenceNumber) + ", congestion local=" + string(response->mLocalCongestionControl.size()) + ", congestion remote=" + string(response->mRemoteCongestionControl.size()))
            return false;
          }

          if (response->hasAttribute(STUNPacket::Attribute_MinimumRTT)) {
            if (response->mMinimumRTT < mMinimumRTT) {
              ZS_LOG_ERROR(Detail, log("remote party attempting lower min RTT") + ", min=" + string(mMinimumRTT) + ", reply min=" + string(response->mMinimumRTT))
              return false;  // not legal to negotiate a lower minimum RRT
            }
            mMinimumRTT = response->mMinimumRTT;
          }

          if (response->hasAttribute(STUNPacket::Attribute_Lifetime)) {
            if (response->mLifetime > mLifetime) {
              ZS_LOG_ERROR(Detail, log("remote party attempting raise lifetime") + ", lifetime=" + string(mLifetime) + ", reply min=" + string(response->mLifetime))
              return false;  // not legal to negotiate a longer lifetime
            }
            mLifetime = response->mLifetime;
          }

          if (response->mLocalCongestionControl.size() > 0) {
            if (response->mLocalCongestionControl.begin() != find(response->mLocalCongestionControl.begin(), response->mLocalCongestionControl.end(), IRUDPChannel::CongestionAlgorithm_TCPLikeWindowWithSlowCreepUp)) {
              ZS_LOG_ERROR(Detail, log("remote party did not offer valid local congestion control"))
              return false;
            }
          }

          if (response->mRemoteCongestionControl.size() > 0) {
            if (response->mRemoteCongestionControl.begin() != find(response->mRemoteCongestionControl.begin(), response->mRemoteCongestionControl.end(), IRUDPChannel::CongestionAlgorithm_TCPLikeWindowWithSlowCreepUp)) {
              ZS_LOG_ERROR(Detail, log("remote party did not offer valid remote congestion control"))
              return false;
            }
          }

          // this is not legal if they don't support our encoders/decoders as the first preference since that is what we asked for
          if ((response->mLocalCongestionControl.begin() != find(response->mLocalCongestionControl.begin(), response->mLocalCongestionControl.end(), IRUDPChannel::CongestionAlgorithm_TCPLikeWindowWithSlowCreepUp)) ||
              (response->mRemoteCongestionControl.begin() != find(response->mRemoteCongestionControl.begin(), response->mRemoteCongestionControl.end(), IRUDPChannel::CongestionAlgorithm_TCPLikeWindowWithSlowCreepUp))) {
            ZS_LOG_ERROR(Detail, log("remote party did not select our congestion control algorithm"))
            return false;
          }

          mRemoteSequenceNumber = response->mNextSequenceNumber;
          mOutgoingChannelNumber = response->mChannelNumber;

          ZS_LOG_DETAIL(log("open") + ", local sequence number=" + sequenceToString(mLocalSequenceNumber) + ", local sequence number=" + sequenceToString(mRemoteSequenceNumber) + ", outgoing channel number=" + string(mOutgoingChannelNumber) + ", incoming channel number=" + string(mIncomingChannelNumber))

          // we should have enough to open our stream now!
          mStream = IRUDPChannelStream::create(
                                               getAssociatedMessageQueue(),
                                               mThisWeak.lock(),
                                               mLocalSequenceNumber,
                                               mRemoteSequenceNumber,
                                               mOutgoingChannelNumber,
                                               mIncomingChannelNumber,
                                               mMinimumRTT
                                               );

          if ((mReceiveStream) &&
              (mSendStream)) {
            mStream->setStreams(mReceiveStream, mSendStream);
          }

          if (IRUDPChannel::Shutdown_None != mShutdownDirection)
            mStream->shutdownDirection(mShutdownDirection);

          step();
          return true;
        }

        for (ACKRequestMap::iterator iter = mOutstandingACKs.begin(); iter != mOutstandingACKs.end(); ++iter) {
          if ((*iter).second == requester) {
            if (handleStaleNonce((*iter).second, response)) return true;

            if (STUNPacket::Class_ErrorResponse == response->mClass) {
              ZS_LOG_ERROR(Detail, log("shutting down channel as ACK failed") + ", error code=" + string(response->mErrorCode) + " (" + STUNPacket::toString((STUNPacket::ErrorCodes)response->mErrorCode) + ")")
              mOutstandingACKs.erase(iter);
              if (mStream) {
                mStream->shutdown(false);
              }
              setError(RUDPChannelShutdownReason_IllegalStreamState, "illegal stream state");
              cancel(false);
              return true;
            }

            if ((!response->hasAttribute(STUNPacket::Attribute_NextSequenceNumber)) ||
                (!response->hasAttribute(STUNPacket::Attribute_GSNR)) ||
                (!response->hasAttribute(STUNPacket::Attribute_GSNFR)) ||
                (!response->hasAttribute(STUNPacket::Attribute_RUDPFlags))) {
              ZS_LOG_ERROR(Detail, log("ACK reply missing attributes (thus being ignored)") + ", sequence number=" + sequenceToString(response->mNextSequenceNumber) + ", GSNR=" + sequenceToString(response->mGSNR) + ", GSNFR=" + sequenceToString(response->mGSNFR) + ", flags=" + string(((WORD)response->mReliabilityFlags)))
              // all of these attributes are manditory or it is not legal - so ignore the response
              return false;
            }

            ZS_LOG_DETAIL(log("handle external ACK"))
            mStream->handleExternalAck(
                                       (*iter).first, // the ACK ID
                                       response->mNextSequenceNumber,
                                       response->mGSNR,
                                       response->mGSNFR,
                                       response->hasAttribute(STUNPacket::Attribute_ACKVector) ? response->mACKVector.get() : NULL,
                                       response->hasAttribute(STUNPacket::Attribute_ACKVector) ? response->mACKVectorLength : 0,
                                       (0 != (response->mReliabilityFlags & RUDPPacket::Flag_VP_VectorParity)),
                                       (0 != (response->mReliabilityFlags & RUDPPacket::Flag_PG_ParityGSNR)),
                                       (0 != (response->mReliabilityFlags & RUDPPacket::Flag_XP_XORedParityToGSNFR)),
                                       (0 != (response->mReliabilityFlags & RUDPPacket::Flag_DP_DuplicatePacket)),
                                       (0 != (response->mReliabilityFlags & RUDPPacket::Flag_EC_ECNPacket))
                                       );
            mOutstandingACKs.erase(iter);
            // the response has been processed
            return true;
          }
        }

        return false;
      }

      //-----------------------------------------------------------------------
      void RUDPChannel::onSTUNRequesterTimedOut(ISTUNRequesterPtr requester)
      {
        AutoRecursiveLock lock(mLock);
        ZS_LOG_ERROR(Detail, log("STUN requester timeout"))

        // any timeout is considered fatal
        if (mStream) {
          mStream->shutdown(false);
        }

        get(mSTUNRequestPreviouslyTimedOut) = true;
        setError(RUDPChannelShutdownReason_Timeout, "channel timeout");
        cancel(false);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark RUDPChannel => ITimerDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void RUDPChannel::onTimer(TimerPtr timer)
      {
        AutoRecursiveLock lock(mLock);
        if (!mStream) return;

        if (mOutstandingACKs.size() > 0) return;  // don't add to the queue

        Time current = zsLib::now();

        DWORD actualLifeTime = (mLifetime > 120 ? mLifetime - 60 : mLifetime);

        if (mLastSentData + Seconds(actualLifeTime) < current) {
          ZS_LOG_DEBUG(log("timer haven't sent data in a while fired"))
          onRUDPChannelStreamSendExternalACKNow(mStream, true, 0);
          mLastSentData = current;
        }

        if (mOutstandingACKs.size() > 0) return;  // don't add to the queue

        actualLifeTime = (mLifetime > 90 ? mLifetime - 60 : mLifetime);
        if (mLastReceivedData + Seconds(actualLifeTime) < current) {
          ZS_LOG_DEBUG(log("timer lifetime refresh required"))
          onRUDPChannelStreamSendExternalACKNow(mStream, true, 0);
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark RUDPChannel => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      String RUDPChannel::log(const char *message) const
      {
        return String("RUDPChannel [") + string(mID) + "] " + message;
      }

      //-----------------------------------------------------------------------
      void RUDPChannel::fix(STUNPacketPtr stun) const
      {
        stun->mLogObject = "RUDPChannel";
        stun->mLogObjectID = mID;
      }

      //-----------------------------------------------------------------------
      String RUDPChannel::getDebugValueString(bool includeCommaPrefix) const
      {
        AutoRecursiveLock lock(mLock);
        bool firstTime = !includeCommaPrefix;
        return

        Helper::getDebugValue("rudp channel ID", string(mID), firstTime) +
        Helper::getDebugValue("graceful shutdown", mGracefulShutdownReference ? String("true") : String(), firstTime) +

        Helper::getDebugValue("incoming", mIncoming ? String("true") : String(), firstTime) +

        Helper::getDebugValue("state", IRUDPChannel::toString(mCurrentState), firstTime) +
        Helper::getDebugValue("last error", 0 != mLastError ? string(mLastError) : String(), firstTime) +
        Helper::getDebugValue("last reason", mLastErrorReason, firstTime) +

        Helper::getDebugValue("delegate", mDelegate ? String("true") : String(), firstTime) +
        Helper::getDebugValue("master delegate", mMasterDelegate ? String("true") : String(), firstTime) +

        Helper::getDebugValue("stream", mStream ? String("true") : String(), firstTime) +
        Helper::getDebugValue("open request", mOpenRequest ? String("true") : String(), firstTime) +
        Helper::getDebugValue("shutdown request", mShutdownRequest ? String("true") : String(), firstTime) +
        Helper::getDebugValue("stun request previously timed out", mSTUNRequestPreviouslyTimedOut ? String("true") : String(), firstTime) +

        Helper::getDebugValue("timer", mTimer ? String("true") : String(), firstTime) +

        Helper::getDebugValue("shutdown direction", IRUDPChannel::toString(mShutdownDirection), firstTime) +

        Helper::getDebugValue("remote ip", !mRemoteIP.isEmpty() ? string(mRemoteIP) : String(), firstTime) +

        Helper::getDebugValue("local username frag", mLocalUsernameFrag, firstTime) +
        Helper::getDebugValue("local password", mLocalPassword, firstTime) +
        Helper::getDebugValue("remote username frag", mRemoteUsernameFrag, firstTime) +
        Helper::getDebugValue("remote password", mRemotePassword, firstTime) +

        Helper::getDebugValue("realm", mRealm, firstTime) +
        Helper::getDebugValue("nonce", mNonce, firstTime) +

        Helper::getDebugValue("incoming channel number", 0 != mIncomingChannelNumber ? string(mIncomingChannelNumber) : String(), firstTime) +
        Helper::getDebugValue("outgoing channel number", 0 != mOutgoingChannelNumber ? string(mOutgoingChannelNumber) : String(), firstTime) +

        Helper::getDebugValue("local sequence number", 0 != mLocalSequenceNumber ? string(mLocalSequenceNumber) : String(), firstTime) +
        Helper::getDebugValue("remote sequence number", 0 != mRemoteSequenceNumber ? string(mLocalSequenceNumber) : String(), firstTime) +

        Helper::getDebugValue("minimum RTT", 0 != mMinimumRTT ? string(mMinimumRTT) : String(), firstTime) +
        Helper::getDebugValue("lifetime", 0 != mLifetime ? string(mLifetime) : String(), firstTime) +

        Helper::getDebugValue("local channel info", mLocalChannelInfo, firstTime) +
        Helper::getDebugValue("remote channel info", mRemoteChannelInfo, firstTime) +

        Helper::getDebugValue("last sent data", Time() != mLastSentData ? IHelper::timeToString(mLastSentData) : String(), firstTime) +
        Helper::getDebugValue("last received data", Time() != mLastReceivedData ? IHelper::timeToString(mLastReceivedData) : String(), firstTime) +

        Helper::getDebugValue("outstanding acks", mOutstandingACKs.size() > 0 ? string(mOutstandingACKs.size()) : String(), firstTime);
      }

      //-----------------------------------------------------------------------
      void RUDPChannel::cancel(bool waitForDataToBeSent)
      {
        AutoRecursiveLock lock(mLock); // just in case...

        if (isShutdown()) {
          ZS_LOG_DEBUG(log("already shutdown"))
          return;
        }

        ZS_LOG_DEBUG(log("cancel called"))

        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        // we don't need any external ACKs going on to clutter the traffic
        for (ACKRequestMap::iterator iter = mOutstandingACKs.begin(); iter != mOutstandingACKs.end(); ++iter)
        {
          ((*iter).second)->cancel();
        }
        mOutstandingACKs.clear();

        if (mTimer) {
          mTimer->cancel();
          mTimer.reset();
        }

        setState(RUDPChannelState_ShuttingDown);

        if (mStream) {
          mStream->shutdown(waitForDataToBeSent);
        }

        if (mOpenRequest) {
          if (!mOpenRequest->isComplete()) {
            mOpenRequest->cancel();
            mOpenRequest.reset();
          }
        }

        if (mGracefulShutdownReference) {

          if (mStream) {
            if (IRUDPChannelStream::RUDPChannelStreamState_Shutdown != mStream->getState()) {
              ZS_LOG_DEBUG(log("waiting for RUDP channel stream to shutdown"))
              return;
            }

            if (((mOpenRequest) ||
                 (mIncoming)) &&
                (!mSTUNRequestPreviouslyTimedOut)) {

              // if we had a successful open request then we must shutdown
              if (!mShutdownRequest) {
                // create the shutdown request...
                STUNPacketPtr stun = STUNPacket::createRequest(STUNPacket::Method_ReliableChannelOpen);
                fix(stun);
                fillCredentials(stun);
                stun->mLifetimeIncluded = true;
                stun->mLifetime = 0;
                stun->mChannelNumber = mIncomingChannelNumber;
                mShutdownRequest = ISTUNRequester::create(getAssociatedMessageQueue(), mThisWeak.lock(), mRemoteIP, stun, STUNPacket::RFC_draft_RUDP);
              }
            }

            if (mShutdownRequest) {
              // see if the master is still around (if not there is no possibility to send the packet so cancel the object immediately)
              if (!(IRUDPChannelDelegateForSessionAndListenerProxy::create(mMasterDelegate))) {
                mShutdownRequest->cancel();
              }

              if (!mShutdownRequest->isComplete()) {
                ZS_LOG_DEBUG(log("waiting for RUDP channel shutdown request to complete"))
                return;
              }
            }
          }
        }

        setState(RUDPChannelState_Shutdown);

        // stop sending any more open requests
        if (mOpenRequest) {
          mOpenRequest->cancel();
          mOpenRequest.reset();
        }

        mGracefulShutdownReference.reset();
        mMasterDelegate.reset();
        mDelegate.reset();

        if (mShutdownRequest) {
          mShutdownRequest->cancel();
          mShutdownRequest.reset();
        }

        if (mStream) {
          mStream->shutdown(false);
          mStream.reset();
        }

        ZS_LOG_DEBUG(log("cancel complete"))
      }

      //-----------------------------------------------------------------------
      void RUDPChannel::step()
      {
        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_DEBUG(log("step passing to cancel due to shutting down/shutdown state"))
          cancel(true);
          return;
        }

        ZS_LOG_DEBUG(log("step") + getDebugValueString())

        if (mStream) {
          // we need to create a timer now we have a stream
          if (!mTimer) {
            mTimer = Timer::create(mThisWeak.lock(), Seconds(20));  // fire ever 20 seconds
            (ITimerDelegateProxy::create(mThisWeak.lock()))->onTimer(mTimer);
          }

          setState(RUDPChannelState_Connected);
        }
      }

      //-----------------------------------------------------------------------
      void RUDPChannel::setState(RUDPChannelStates state)
      {
        if (mCurrentState == state) return;
        ZS_LOG_BASIC(log("state changed") + ", old state=" + toString(mCurrentState) + ", new state=" + toString(state))

        mCurrentState = state;

        RUDPChannelPtr pThis = mThisWeak.lock();

        if (pThis) {
          if (mMasterDelegate) {
            try {
              mMasterDelegate->onRUDPChannelStateChanged(pThis, mCurrentState);
            } catch(IRUDPChannelDelegateForSessionAndListenerProxy::Exceptions::DelegateGone &) {
            }
          }

          if (mDelegate) {
            try {
              mDelegate->onRDUPChannelStateChanged(pThis, mCurrentState);
            } catch(IRUDPChannelDelegateProxy::Exceptions::DelegateGone &) {
            }
          }
        }
      }

      //-----------------------------------------------------------------------
      void RUDPChannel::setError(WORD errorCode, const char *inReason)
      {
        String reason(inReason ? String(inReason) : String());
        if (reason.isEmpty()) {
          reason = IHTTP::toString(IHTTP::toStatusCode(errorCode));
        }

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, log("already shutting down thus ignoring new error") + ", new error=" + string(errorCode) + ", new reason=" + reason + getDebugValueString())
          return;
        }

        if (0 != mLastError) {
          ZS_LOG_WARNING(Detail, log("error already set thus ignoring new error") + ", new error=" + string(errorCode) + ", new reason=" + reason + getDebugValueString())
          return;
        }

        get(mLastError) = errorCode;
        mLastErrorReason = reason;

        ZS_LOG_WARNING(Detail, log("error set") + ", code=" + string(mLastError) + ", reason=" + mLastErrorReason + getDebugValueString())
      }

      //-----------------------------------------------------------------------
      bool RUDPChannel::isValidIntegrity(STUNPacketPtr stun)
      {
        if ((STUNPacket::Class_Request == stun->mClass) ||
            (STUNPacket::Class_Indication == stun->mClass)) {

          // this is an incoming request
          if (!mRealm.isEmpty())
            return stun->isValidMessageIntegrity(mLocalPassword, (mLocalUsernameFrag + ":" + mRemoteUsernameFrag).c_str(), mRealm);

          return stun->isValidMessageIntegrity(mLocalPassword);
        }

        // this is a reply to an outgoing request
        if (!mRealm.isEmpty())
          return stun->isValidMessageIntegrity(mRemotePassword, (mRemoteUsernameFrag + ":" + mLocalUsernameFrag).c_str(), mRealm);

        return stun->isValidMessageIntegrity(mRemotePassword);
      }

      //-----------------------------------------------------------------------
      void RUDPChannel::fillCredentials(STUNPacketPtr &outSTUN)
      {
        if ((STUNPacket::Class_Request == outSTUN->mClass) ||
            (STUNPacket::Class_Indication == outSTUN->mClass)) {
          outSTUN->mUsername = mRemoteUsernameFrag + ":" + mLocalUsernameFrag;
          outSTUN->mPassword = mRemotePassword;
        } else {
          outSTUN->mUsername = mLocalUsernameFrag + ":" + mRemoteUsernameFrag;
          outSTUN->mPassword = mLocalPassword;
        }
        outSTUN->mRealm = mRealm;
        outSTUN->mNonce = mNonce;
        if (!mRealm.isEmpty()) {
          outSTUN->mCredentialMechanism = STUNPacket::CredentialMechanisms_LongTerm;
        } else {
          outSTUN->mCredentialMechanism = STUNPacket::CredentialMechanisms_ShortTerm;
        }
      }

      //-----------------------------------------------------------------------
      void RUDPChannel::fillACK(STUNPacketPtr &outSTUN)
      {
        ZS_THROW_INVALID_ASSUMPTION_IF(!mStream)

        fillCredentials(outSTUN);

        outSTUN->mSoftware.clear(); // do not include the software attribute

        // we need to pre-fill with bogus data so we can calculate the room available for a vector
        outSTUN->mNextSequenceNumber = 1;
        outSTUN->mGSNR = 1;
        outSTUN->mGSNFR = 1;
        outSTUN->mChannelNumber = mIncomingChannelNumber;
        outSTUN->mReliabilityFlagsIncluded = true;
        outSTUN->mReliabilityFlags = 0;

        ULONG available = outSTUN->getTotalRoomAvailableForData(OPENPEER_SERVICES_RUDP_MAX_PACKET_SIZE_WHEN_PMTU_IS_NOT_KNOWN, STUNPacket::RFC_draft_RUDP);

        boost::shared_array<BYTE> vector;
        if (available > 0) {
          vector = boost::shared_array<BYTE>(new BYTE[available]);
        }

        QWORD nextSequenceNumber = 0;
        QWORD gsnr = 0;
        QWORD gsnfr = 0;
        ULONG vectorOutputSize = 0;
        bool vpFlag = false;
        bool pgFlag = false;
        bool xpFlag = false;
        bool dpFlag = false;
        bool ecFlag = false;
        mStream->getState(
                          nextSequenceNumber,
                          gsnr,
                          gsnfr,
                          (0 != available ? vector.get() : NULL),
                          vectorOutputSize,
                          (0 != available ? available : 0),
                          vpFlag,
                          pgFlag,
                          xpFlag,
                          dpFlag,
                          ecFlag
                          );

        mStream->notifyExternalACKSent(gsnr);

        outSTUN->mNextSequenceNumber = nextSequenceNumber;
        outSTUN->mGSNR = gsnr;
        outSTUN->mGSNFR = gsnfr;
        outSTUN->mACKVector = (0 != vectorOutputSize ? vector : boost::shared_array<BYTE>());
        outSTUN->mACKVectorLength = vectorOutputSize;
        outSTUN->mReliabilityFlagsIncluded = true;
        outSTUN->mReliabilityFlags = ((vpFlag ? RUDPPacket::Flag_VP_VectorParity : 0) |
                                      (pgFlag ? RUDPPacket::Flag_PG_ParityGSNR : 0) |
                                      (xpFlag ? RUDPPacket::Flag_XP_XORedParityToGSNFR : 0) |
                                      (dpFlag ? RUDPPacket::Flag_DP_DuplicatePacket : 0) |
                                      (ecFlag ? RUDPPacket::Flag_EC_ECNPacket : 0));
      }

      //-----------------------------------------------------------------------
      bool RUDPChannel::handleStaleNonce(
                                         ISTUNRequesterPtr &originalRequestVariable,
                                         STUNPacketPtr response
                                         )
      {
        if (STUNPacket::Class_ErrorResponse != response->mClass) return false;
        if (STUNPacket::ErrorCode_StaleNonce != response->mErrorCode) return false;

        if (!response->mNonce) return false;

        STUNPacketPtr originalSTUN = originalRequestVariable->getRequest();
        if (originalSTUN->mTotalRetries > 0) return false;

        STUNPacketPtr stun = originalSTUN->clone(true);
        mNonce = response->mNonce;
        if (!response->mRealm.isEmpty())
          mRealm = response->mRealm;

        stun->mTotalRetries = originalSTUN->mTotalRetries;
        ++(stun->mTotalRetries);
        stun->mNonce = mNonce;
        stun->mRealm = mRealm;

        originalRequestVariable = ISTUNRequester::create(getAssociatedMessageQueue(), mThisWeak.lock(), mRemoteIP, stun, STUNPacket::RFC_draft_RUDP);
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IRUDPChannel
    #pragma mark

    //-------------------------------------------------------------------------
    const char *IRUDPChannel::toString(RUDPChannelStates state)
    {
      switch (state)
      {
        case RUDPChannelState_Connecting:   return "Connecting";
        case RUDPChannelState_Connected:    return "Connected";
        case RUDPChannelState_ShuttingDown: return "Shutting down";
        case RUDPChannelState_Shutdown:     return "Shutdown";
        default: break;
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    const char *IRUDPChannel::toString(RUDPChannelShutdownReasons reason)
    {
      switch (reason)
      {
        case RUDPChannelShutdownReason_None:                return "None";
        case RUDPChannelShutdownReason_OpenFailure:         return "Open failure";
        case RUDPChannelShutdownReason_DelegateGone:        return "Delegate gone";
        case RUDPChannelShutdownReason_Timeout:             return "Timeout";
        case RUDPChannelShutdownReason_IllegalStreamState:  return "Illegal stream state";
      }
      return IHTTP::toString(IHTTP::toStatusCode(reason));
    }

    //-------------------------------------------------------------------------
    const char *IRUDPChannel::toString(Shutdown value)
    {
      switch (value)
      {
        case Shutdown_None:     return "None";
        case Shutdown_Send:     return "Send";
        case Shutdown_Receive:  return "Receive";
        case Shutdown_Both:     return "Both";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    const char *IRUDPChannel::toString(CongestionAlgorithms value)
    {
      switch (value)
      {
        case CongestionAlgorithm_None:                          return "None";
        case CongestionAlgorithm_TCPLikeWindowWithSlowCreepUp:  return "TCP-like Window with slow creep up";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    String IRUDPChannel::toDebugString(IRUDPChannelPtr channel, bool includeCommaPrefix)
    {
      return internal::RUDPChannel::toDebugString(channel, includeCommaPrefix);
    }

  }
}
