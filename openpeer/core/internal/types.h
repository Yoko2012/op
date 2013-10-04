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

#include <openpeer/core/internal/types.h>
#include <openpeer/core/types.h>
#include <openpeer/stack/types.h>
#include <openpeer/stack/message/types.h>
#include <openpeer/services/types.h>

namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      using zsLib::string;
      using zsLib::Noop;
      using zsLib::BYTE;
      using zsLib::CSTR;
      using zsLib::INT;
      using zsLib::UINT;
      using zsLib::DWORD;
      using zsLib::AutoLock;
      using zsLib::AutoRecursiveLock;
      using zsLib::Lock;
      using zsLib::RecursiveLock;
      using zsLib::Log;
      using zsLib::MessageQueue;
      using zsLib::IMessageQueuePtr;
      using zsLib::MessageQueuePtr;
      using zsLib::MessageQueueAssociator;
      using zsLib::IMessageQueueNotify;
      using zsLib::IMessageQueueMessagePtr;
      using zsLib::IMessageQueueThread;
      using zsLib::MessageQueueThread;
      using zsLib::IMessageQueueThreadPtr;
      using zsLib::MessageQueueThreadPtr;
      using zsLib::Timer;
      using zsLib::TimerPtr;
      using zsLib::ITimerDelegate;
      using zsLib::Seconds;
      using zsLib::Socket;

      using zsLib::XML::AttributePtr;
      using zsLib::XML::Document;
      using zsLib::XML::DocumentPtr;
      using zsLib::XML::Generator;
      using zsLib::XML::GeneratorPtr;

      using stack::Candidate;
      using stack::CandidateList;
      using stack::AutoRecursiveLockPtr;
      using stack::IBootstrappedNetwork;
      using stack::IBootstrappedNetworkPtr;
      using stack::IBootstrappedNetworkDelegate;
      using stack::ILocation;
      using stack::ILocationPtr;
      using stack::LocationList;
      using stack::LocationListPtr;
      using stack::IMessageIncomingPtr;
      using stack::IMessageMonitor;
      using stack::IMessageMonitorPtr;
      using stack::IPeer;
      using stack::IPeerPtr;
      using stack::IPeerFiles;
      using stack::IPeerFilesPtr;
      using stack::IPeerFilePrivatePtr;
      using stack::IPeerFilePublic;
      using stack::IPeerFilePublicPtr;
      using stack::IPeerSubscription;
      using stack::IPeerSubscriptionPtr;
      using stack::IPeerSubscriptionDelegate;
      using stack::ILocation;
      using stack::IPublication;
      using stack::IPublicationPtr;
      using stack::IPublicationMetaData;
      using stack::IPublicationMetaDataPtr;
      using stack::IPublicationFetcherPtr;
      using stack::IPublicationPublisherPtr;
      using stack::IPublicationPublisherDelegateProxy;
      using stack::IPublicationRepository;
      using stack::IPublicationRepositoryPtr;
      using stack::IPublicationSubscription;
      using stack::IPublicationSubscriptionPtr;
      using stack::IPublicationSubscriptionDelegate;
      using stack::IServiceIdentity;
      using stack::IServiceIdentityPtr;
      using stack::IServiceIdentitySession;
      using stack::IServiceIdentitySessionPtr;
      using stack::IServiceIdentitySessionDelegate;
      using stack::ServiceIdentitySessionList;
      using stack::ServiceIdentitySessionListPtr;
      using stack::IServiceLockbox;
      using stack::IServiceLockboxPtr;
      using stack::IServiceLockboxSession;
      using stack::IServiceLockboxSessionPtr;
      using stack::IServiceLockboxSessionDelegate;
      using stack::IServiceNamespaceGrantSession;
      using stack::IServiceNamespaceGrantSessionPtr;
      using stack::IServiceNamespaceGrantSessionDelegate;

      using stack::message::IdentityInfo;

      using services::IICESocket;
      using services::IICESocketPtr;
      using services::IICESocketDelegate;
      using services::IICESocketSubscriptionPtr;
      using services::IICESocketSession;
      using services::IICESocketSessionPtr;
      using services::IHTTP;
      using services::IWakeDelegate;
      using services::IWakeDelegatePtr;
      using services::IWakeDelegateWeakPtr;
      using services::IWakeDelegateProxy;

      class Account;
      typedef boost::shared_ptr<Account> AccountPtr;
      typedef boost::weak_ptr<Account> AccountWeakPtr;

      class Cache;
      typedef boost::shared_ptr<Cache> CachePtr;
      typedef boost::weak_ptr<Cache> CacheWeakPtr;

      class Call;
      typedef boost::shared_ptr<Call> CallPtr;
      typedef boost::weak_ptr<Call> CallWeakPtr;

      interaction ICallAsync;
      typedef boost::shared_ptr<ICallAsync> ICallAsyncPtr;
      typedef boost::weak_ptr<ICallAsync> ICallAsyncWeakPtr;
      typedef zsLib::Proxy<ICallAsync> ICallAsyncProxy;

      class CallTransport;
      typedef boost::shared_ptr<CallTransport> CallTransportPtr;
      typedef boost::weak_ptr<CallTransport> CallTransportWeakPtr;

      interaction ICallTransport;
      typedef boost::shared_ptr<ICallTransport> ICallTransportPtr;
      typedef boost::weak_ptr<ICallTransport> ICallTransportWeakPtr;

      interaction ICallTransportDelegate;
      typedef boost::shared_ptr<ICallTransportDelegate> ICallTransportDelegatePtr;
      typedef boost::weak_ptr<ICallTransportDelegate> ICallTransportDelegateWeakPtr;
      typedef zsLib::Proxy<ICallTransportDelegate> ICallTransportDelegateProxy;

      interaction ICallTransportAsync;
      typedef boost::shared_ptr<ICallTransportAsync> ICallTransportAsyncPtr;
      typedef boost::weak_ptr<ICallTransportAsync> ICallTransportAsyncWeakPtr;
      typedef zsLib::Proxy<ICallTransportAsync> ICallTransportAsyncProxy;

      class Contact;
      typedef boost::shared_ptr<Contact> ContactPtr;
      typedef boost::weak_ptr<Contact> ContactWeakPtr;

      class ContactPeerFilePublicLookup;
      typedef boost::shared_ptr<ContactPeerFilePublicLookup> ContactPeerFilePublicLookupPtr;
      typedef boost::weak_ptr<ContactPeerFilePublicLookup> ContactPeerFilePublicLookupWeakPtr;

      class ConversationThread;
      typedef boost::shared_ptr<ConversationThread> ConversationThreadPtr;
      typedef boost::weak_ptr<ConversationThread> ConversationThreadWeakPtr;

      interaction IConversationThreadHostSlaveBase;
      typedef boost::shared_ptr<IConversationThreadHostSlaveBase> IConversationThreadHostSlaveBasePtr;
      typedef boost::weak_ptr<IConversationThreadHostSlaveBase> IConversationThreadHostSlaveBaseWeakPtr;

      class ConversationThreadHost;
      typedef boost::shared_ptr<ConversationThreadHost> ConversationThreadHostPtr;
      typedef boost::weak_ptr<ConversationThreadHost> ConversationThreadHostWeakPtr;

      class ConversationThreadSlave;
      typedef boost::shared_ptr<ConversationThreadSlave> ConversationThreadSlavePtr;
      typedef boost::weak_ptr<ConversationThreadSlave> ConversationThreadSlaveWeakPtr;

      interaction IConversationThreadDocumentFetcher;
      typedef boost::shared_ptr<IConversationThreadDocumentFetcher> IConversationThreadDocumentFetcherPtr;
      typedef boost::weak_ptr<IConversationThreadDocumentFetcher> IConversationThreadDocumentFetcherWeakPtr;

      interaction IConversationThreadDocumentFetcherDelegate;
      typedef boost::shared_ptr<IConversationThreadDocumentFetcherDelegate> IConversationThreadDocumentFetcherDelegatePtr;
      typedef boost::weak_ptr<IConversationThreadDocumentFetcherDelegate> IConversationThreadDocumentFetcherDelegateWeakPtr;
      typedef zsLib::Proxy<IConversationThreadDocumentFetcherDelegate> IConversationThreadDocumentFetcherDelegateProxy;

      interaction ConversationThreadDocumentFetcher;
      typedef boost::shared_ptr<ConversationThreadDocumentFetcher> ConversationThreadDocumentFetcherPtr;
      typedef boost::weak_ptr<ConversationThreadDocumentFetcher> ConversationThreadDocumentFetcherWeakPtr;

      interaction Factory;
      typedef boost::shared_ptr<Factory> FactoryPtr;
      typedef boost::weak_ptr<Factory> FactoryWeakPtr;

      class Identity;
      typedef boost::shared_ptr<Identity> IdentityPtr;
      typedef boost::weak_ptr<Identity> IdentityWeakPtr;

      class IdentityLookup;
      typedef boost::shared_ptr<IdentityLookup> IdentityLookupPtr;
      typedef boost::weak_ptr<IdentityLookup> IdentityLookupWeakPtr;

      class MediaEngineObsolete;
      typedef boost::shared_ptr<MediaEngineObsolete> MediaEngineObsoletePtr;
      typedef boost::weak_ptr<MediaEngineObsolete> MediaEngineObsoleteWeakPtr;
      
      class MediaManager;
      typedef boost::shared_ptr<MediaManager> MediaManagerPtr;
      typedef boost::weak_ptr<MediaManager> MediaManagerWeakPtr;
      
      interaction IMediaSession;
      typedef boost::shared_ptr<IMediaSession> IMediaSessionPtr;
      typedef boost::weak_ptr<IMediaSession> IMediaSessionWeakPtr;
      
      interaction IMediaSessionDelegate;
      typedef boost::shared_ptr<IMediaSessionDelegate> IMediaSessionDelegatePtr;
      typedef boost::weak_ptr<IMediaSessionDelegate> IMediaSessionDelegateWeakPtr;
      
      typedef std::list<IMediaSessionPtr> MediaSessionList;
      typedef boost::shared_ptr<MediaSessionList> MediaSessionListPtr;
      typedef boost::weak_ptr<MediaSessionList> MediaSessionListWeakPtr;
      
      class MediaSession;
      typedef boost::shared_ptr<MediaSession> MediaSessionPtr;
      typedef boost::weak_ptr<MediaSession> MediaSessionWeakPtr;

      interaction IMediaTransport;
      typedef boost::shared_ptr<IMediaTransport> IMediaTransportPtr;
      typedef boost::weak_ptr<IMediaTransport> IMediaTransportWeakPtr;

      class MediaTransport;
      typedef boost::shared_ptr<MediaTransport> MediaTransportPtr;
      typedef boost::weak_ptr<MediaTransport> MediaTransportWeakPtr;
      
      class SendMediaTransport;
      typedef boost::shared_ptr<SendMediaTransport> SendMediaTransportPtr;
      typedef boost::weak_ptr<SendMediaTransport> SendMediaTransportWeakPtr;
      
      class ReceiveMediaTransport;
      typedef boost::shared_ptr<ReceiveMediaTransport> ReceiveMediaTransportPtr;
      typedef boost::weak_ptr<ReceiveMediaTransport> ReceiveMediaTransportWeakPtr;
      
      interaction IMediaStream;
      typedef boost::shared_ptr<IMediaStream> IMediaStreamPtr;
      typedef boost::weak_ptr<IMediaStream> IMediaStreamWeakPtr;
      
      interaction IMediaStreamDelegate;
      typedef boost::shared_ptr<IMediaStreamDelegate> IMediaStreamDelegatePtr;
      typedef boost::weak_ptr<IMediaStreamDelegate> IMediaStreamDelegateWeakPtr;
      
      typedef std::list<IMediaStreamPtr> MediaStreamList;
      typedef boost::shared_ptr<MediaStreamList> MediaStreamListPtr;
      typedef boost::weak_ptr<MediaStreamList> MediaStreamListWeakPtr;

      class MediaStream;
      typedef boost::shared_ptr<MediaStream> MediaStreamPtr;
      typedef boost::weak_ptr<MediaStream> MediaStreamWeakPtr;
      
      class AudioStream;
      typedef boost::shared_ptr<AudioStream> AudioStreamPtr;
      typedef boost::weak_ptr<AudioStream> AudioStreamWeakPtr;
      
      class LocalSendAudioStream;
      typedef boost::shared_ptr<LocalSendAudioStream> LocalSendAudioStreamPtr;
      typedef boost::weak_ptr<LocalSendAudioStream> LocalSendAudioStreamWeakPtr;
      
      class RemoteReceiveAudioStream;
      typedef boost::shared_ptr<RemoteReceiveAudioStream> RemoteReceiveAudioStreamPtr;
      typedef boost::weak_ptr<RemoteReceiveAudioStream> RemoteReceiveAudioStreamWeakPtr;
      
      class RemoteSendAudioStream;
      typedef boost::shared_ptr<RemoteSendAudioStream> RemoteSendAudioStreamPtr;
      typedef boost::weak_ptr<RemoteSendAudioStream> RemoteSendAudioStreamWeakPtr;
      
      class LocalSendVideoStream;
      typedef boost::shared_ptr<LocalSendVideoStream> LocalSendVideoStreamPtr;
      typedef boost::weak_ptr<LocalSendVideoStream> LocalSendVideoStreamWeakPtr;
      
      class RemoteReceiveVideoStream;
      typedef boost::shared_ptr<RemoteReceiveVideoStream> RemoteReceiveVideoStreamPtr;
      typedef boost::weak_ptr<RemoteReceiveVideoStream> RemoteReceiveVideoStreamWeakPtr;
      
      class RemoteSendVideoStream;
      typedef boost::shared_ptr<RemoteSendVideoStream> RemoteSendVideoStreamPtr;
      typedef boost::weak_ptr<RemoteSendVideoStream> RemoteSendVideoStreamWeakPtr;
      
      interaction IMediaEngine;
      typedef boost::shared_ptr<IMediaEngine> IMediaEnginePtr;
      typedef boost::weak_ptr<IMediaEngine> IMediaEngineWeakPtr;
      
      class MediaEngine;
      typedef boost::shared_ptr<MediaEngine> MediaEnginePtr;
      typedef boost::weak_ptr<MediaEngine> MediaEngineWeakPtr;
      
      interaction IMediaEngineDelegate;
      typedef boost::shared_ptr<IMediaEngineDelegate> IMediaEngineDelegatePtr;
      typedef boost::weak_ptr<IMediaEngineDelegate> IMediaEngineDelegateWeakPtr;
      typedef zsLib::Proxy<IMediaEngineDelegate> IMediaEngineDelegateProxy;

      interaction IShutdownCheckAgainDelegate;
      typedef boost::shared_ptr<IShutdownCheckAgainDelegate> IShutdownCheckAgainDelegatePtr;
      typedef boost::weak_ptr<IShutdownCheckAgainDelegate> IShutdownCheckAgainDelegateWeakPtr;
      typedef zsLib::Proxy<IShutdownCheckAgainDelegate> IShutdownCheckAgainDelegateProxy;

      class Stack;
      typedef boost::shared_ptr<Stack> StackPtr;
      typedef boost::weak_ptr<Stack> StackWeakPtr;

      class VideoViewPort;
      typedef boost::shared_ptr<VideoViewPort> VideoViewPortPtr;
      typedef boost::weak_ptr<VideoViewPort> VideoViewPortWeakPtr;
    }
  }
}
